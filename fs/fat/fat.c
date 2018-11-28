/*
 * fat.c
 *
 * R/O (V)FAT 12/16/32 filesystem implementation by Marcus Sundberg
 *
 * 2002-07-28 - rjones@nexus-tech.net - ported to ppcboot v1.1.6
 * 2003-03-10 - kharris@nexus-tech.net - ported to uboot
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <config.h>
#include <fat.h>			/* struct fsdata, ATTR*, ... */
#include <asm/byteorder.h>
#include <part.h>
#include <malloc.h>
#include <linux/compiler.h>		/* __aligned() */


/* Directory preload size, must be a multiple of the sector size 512 (0x200) */
#ifndef CONFIG_SYS_FAT_PRELOAD_DIR
#define CONFIG_SYS_FAT_PRELOAD_DIR 4096
#endif

/* FAT preload size, must be a multiple of 3 and the sector size 512 (0x200) */
#ifndef CONFIG_SYS_FAT_PRELOAD_FAT
#define CONFIG_SYS_FAT_PRELOAD_FAT 3072
#endif

#define DOS_BOOT_MAGIC_OFFSET	0x1fe

#define TO_FAT_DIRINFO(wdi)	((struct fat_dirinfo *)wdi)
#define TO_WC_DIRINFO(fdi)	((struct wc_dirinfo *)fdi)
#define IS_FAT_CHAR(c)		((c >= 32) && (c <= 255) && (c != 127))

/* Directory entry; the generic wc_dirinfo must be part of it; these entries
   are allocated with fat_alloc_dir() and released with fat_free_dir() */
struct fat_dirinfo {
	/* Info seen from outside */
	struct wc_dirinfo wdi;

	/* Internal info/state to load directory in chunks and access data */
	__u32 dir_cluster;		/* Current dir cluster, 0: root dir */
	__u32 cluster_offset;		/* Offset within cluster or fix dir */
	__u32 preload_offset;		/* Dir entry offset in preload data */
	__u32 preload_count;		/* Number of preloaded sectors */
};

static block_dev_desc_t *cur_dev;
static unsigned int cur_part_nr;
static disk_partition_t cur_part_info;

/* Device specific data; if we allow parallel access to more than one FAT
   device in the future, we must allocate this dynamically. The following
   dir_buffer and fat_buffer are also part of the device data */
static struct fsdata myfsdata;

/* Buffer to cache directory entries */
__u8 dir_buffer[CONFIG_SYS_FAT_PRELOAD_DIR] __aligned(ARCH_DMA_MINALIGN);

/* Buffer to cache FAT table entries */
__u8 fat_buffer[CONFIG_SYS_FAT_PRELOAD_FAT] __aligned(ARCH_DMA_MINALIGN);


#ifdef CONFIG_SUPPORT_VFAT
/* Indexes into a VFAT longname slot where there are UTF16 characters; this
   array is also required by FAT writing functions */
const int slot_index_table[13] = {
	1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30
};

/*
 * Calculate checksum of a short.
 *
 * Parameter:
 *   dirname: Pointer to name from directory entry (8+3 = 11 characters)
 *
 * Return:
 *   Checksum
 */
__u8 fat_checksum(const char *dirname)
{
	__u8 cs = 0;
	int i;

	for (i = 0; i < 11; i++)
		cs = (((cs & 1) << 7) | (cs >> 1)) + dirname[i];

	return cs;
}
#endif

int fat_register_device(block_dev_desc_t *dev_desc, int part_no)
{
	disk_partition_t info;

	/* First close any currently found FAT filesystem */
	cur_dev = NULL;

	/* Read the partition table, if present */
	if (get_partition_info(dev_desc, part_no, &info)) {
		if (part_no != 0) {
			printf("** Partition %d not valid on device %d **\n",
					part_no, dev_desc->dev);
			return -1;
		}

		info.start = 0;
		info.size = dev_desc->lba;
		info.blksz = dev_desc->blksz;
		info.name[0] = 0;
		info.type[0] = 0;
		info.bootable = 0;
#ifdef CONFIG_PARTITION_UUIDS
		info.uuid[0] = 0;
#endif
	}

	return fat_set_blk_dev(dev_desc, &info);
}

/*
 * Load consecutive blocks from block device.
 *
 * Parameters:
 *   sector: Sector where to start loading
 *   count:  Number of sectors to load
 *   buffer: Buffer where to store the loaded data
 *
 * Return:
 *  Number of read sectors; if smaller than count, this indicates an error
 */
static __u32 disk_read(__u32 sector, __u32 count, void *buffer)
{
	debug("Reading %u sectors from sector 0x%x\n", count, sector);
	if (!cur_dev || !cur_dev->block_read)
		return 0;

	sector += cur_part_info.start;

	return cur_dev->block_read(cur_dev->dev, sector, count, buffer);
}

static __u32 clust2sect(struct fsdata *mydata, __u32 cluster)
{
	return mydata->data_sect + (cluster - 2) * mydata->clust_size;
}

/*
 * Get a short filename from a 8+3 directory entry.
 *
 * A FAT short name may not contain leading blanks, as any blank in the first
 * positon of the basename or extension is interpreted as an empty basename or
 * extension respectively. Trailing blanks in the basename and/or the
 * extension are padding and are removed. Other blanks are allowed!
 *
 * The basename is always stored in upper case, but it can be either fully
 * lower case or fully upper case, depending on a bit in the lcase entry. The
 * same is true for the extension. So abc.def, ABC.def, abc.DEF and ABC.DEF
 * are valid combinations for short names, but Abc.def must be saved as a long
 * name.
 *
 * Parameters:
 *   name:    Pointer to buffer where to store name; the size of the buffer is
 *            WC_NAME_MAX characters which is always sufficient for short names
 *   dirname: Pointer to name from directory entry (8+3 = 11 characters)
 *   lcase:   Flags whether basename and extension are upper or lower case
 */
static void get_shortname(char *name, const char *dirname, __u8 lcase)
{
	int src = 0;
	int dst = 0;
	char c = dirname[src];

	if (c != ' ') {
		/* Copy basename */
		if (c == aRING)
			c = DELETED_FLAG;
		do {
			if (lcase & (1 << 3))
				TOLOWER(c);
			name[dst++] = c;
			c = dirname[++src];
		} while (src < 8);

		/* Remove trailing blanks (padding); we don't need to check
		   dst for zero as we know that the basename contains at least
		   one non-blank character */
		while (name[dst-1] == ' ')
			dst--;
	} else {
		/* Skip to extension */
		src = 8;
		c = dirname[src];
	}

	if (c != ' ') {
		/* Insert '.' and copy extension */
		name[dst++] = '.';
		do {
			if (lcase & (1 << 4))
				TOLOWER(c);
			name[dst++] = c;
			if (src >= 10)
				break;
			c = dirname[++src];
		} while (c && (c != ' '));

		/* Remove trailing blanks (padding); we don't need to check
		   dst for zero as the extension contains at least one
		   non-blank character (and of course '.' is also present) */
		while (name[dst-1] == ' ')
			dst--;
	}
	name[dst] = '\0';
}

/*
 * Get the FAT table entry at given index, i.e. return the next cluster in the
 * file; the FAT table may have 12, 16 or 32 bit entries.
 *
 * Parameters:
 *   mydata:  Pointer to device specific information
 *   cluster: Index into FAT table from where to get next value
 *
 * Return:
 *   Next cluster, or 0 on failure
 */
static __u32 get_fatent(struct fsdata *mydata, __u32 cluster)
{
	__u32 bufnum;
	__u32 offset;
	__u8 *fatentry;
	__u32 ret = 0;

	bufnum = cluster / mydata->fatbuf_entries;
	offset = cluster - bufnum * mydata->fatbuf_entries;

	/* Load according chunk of FAT to the cache (if not already loaded) */
	if (bufnum != mydata->fatbufnum) {
		__u32 count = mydata->fatbuf_sectors;
		__u32 sector = bufnum * count;
		if (sector + count > mydata->fat_length)
			count = mydata->fat_length - sector;
		sector += mydata->fat_sect;

		debug("Read FAT bufnum: %d, sector: %u, count: %u\n",
		      bufnum, sector, count);
		if (disk_read(sector, count, mydata->fatbuf) != count)
			return ret;

		mydata->fatbufnum = bufnum;
	}

	/* Get the actual entry from the table */
	fatentry = mydata->fatbuf;
	switch (mydata->fatsize) {
	case 32:
		fatentry += offset * 4;
		ret = (fatentry[3] << 24) | (fatentry[2] << 16)
			                  | (fatentry[1] << 8) | fatentry[0];
		break;
	case 16:
		fatentry += offset * 2;
		ret = (fatentry[1] << 8) | fatentry[0];
		break;
	case 12:
		fatentry += offset/2*3;
		ret = (fatentry[2] << 16) | (fatentry[1] << 8) | fatentry[0];
		if (offset & 1)
			ret >>= 12;
		else
			ret &= 0xfff;
		break;
	}
	debug("FAT%u: ret: %08x, offset: %04x\n",
	      mydata->fatsize, ret, offset);

	return ret;
}


/*
 * Load the next chunk of the directory into the directory preload buffer.
 *
 * Parameters:
 *   mydata: Pointer to device specific information
 *   fdi:    Pointer to directory path
 *
 * Return:
 *   1:  OK, next chunk is loaded
 *   0:  Done, end of directory, no more chunks
 *   -1: Error while reading data from device
 */
static int fat_dir_preload(struct fsdata *mydata, struct fat_dirinfo *fdi)
{
	__u32 count;
	__u32 sector;

	if (!fdi->dir_cluster && !mydata->rootdir_length)
		fdi->dir_cluster = mydata->rootdir_clust;

	if (fdi->dir_cluster) {
		/* Read from (root or sub-) directory located in data region */
		count = mydata->clust_size - fdi->cluster_offset;
		if (!count) {
			int cluster;

			/* Get next cluster */
			cluster = get_fatent(mydata, fdi->dir_cluster);
			if (cluster >= mydata->eof)
				return 0;
			if ((cluster < 2) || (cluster >= mydata->max_cluster))
				return -1;
			fdi->dir_cluster = cluster;
			fdi->cluster_offset = 0;
			count = mydata->clust_size;
		}
		sector = clust2sect(mydata, fdi->dir_cluster);
	} else {
		/* Read from fixed size root directory */
		count = mydata->rootdir_length - fdi->cluster_offset;
		if (!count)
			return 0;
		sector = mydata->rootdir_sect;
	}
	sector += fdi->cluster_offset;
	if (count * mydata->sect_size > CONFIG_SYS_FAT_PRELOAD_DIR)
		count = CONFIG_SYS_FAT_PRELOAD_DIR / mydata->sect_size;
	fdi->preload_count = count;
	if (disk_read(sector, count, mydata->dirbuf) != count)
		return -1;

	return 1;
}

/*
 * Allocate a directory entry; we allocate a fat_dirinfo, but we return a
 * pointer to the embedded wc_dirinfo. All functions that are called from the
 * wildcard module also pass this pointer, so we always have to convert it
 * back to the surrounding fat_dirinfo.
 *
 * Return:
 *   Pointer to the embedded wc_dirinfo, NULL on error
 */
static struct wc_dirinfo *fat_alloc_dir(void)
{
	struct fat_dirinfo *fdi;

	fdi = malloc(sizeof(struct fat_dirinfo));
	if (!fdi)
		return NULL;

	return TO_WC_DIRINFO(fdi);
}

/*
 * Free a directory entry.
 *
 * Parameter:
 *   wdi: Pointer to directory entry
 */
static void fat_free_dir(struct wc_dirinfo *wdi)
{
	free(TO_FAT_DIRINFO(wdi));
}

/*
 * Get next directory entry and return the type, size and name of the file.
 *
 * Parameters:
 *   wdi: Pointer to directory entry (data will be updated)
 *   wfi: Pointer to structure where to store file information
 *
 * Return:
 *   1:  OK, found another entry
 *   0:  Done, no more entries
 *   -1: Error while reading data from device
 */
static int fat_get_fileinfo(struct wc_dirinfo *wdi, struct wc_fileinfo *wfi)
{
	int ret;
	struct dir_entry *dentry;
	struct fat_dirinfo *fdi = TO_FAT_DIRINFO(wdi);
	struct fsdata *mydata = &myfsdata;
#ifdef CONFIG_SUPPORT_VFAT
	struct dir_slot *dslot;		/* Directory entry as VFAT name slot */
	__u8 *ds;			/* Directory entry as byte array */
	char *namepart;
	int i;
	unsigned int c;
	__u8 alias_checksum = 0;
	__u8 id;
#endif

	wfi->file_name[0] = '\0';
	if (fdi->wdi.flags & WC_FLAGS_REWIND) {
		fdi->wdi.flags &= ~WC_FLAGS_REWIND;
		fdi->preload_offset = 0;

		/* If we the beginning of the directory is not in the preload
		   buffer anymore, we have to reload it */
		if ((fdi->dir_cluster != fdi->wdi.reference)
		    || (fdi->cluster_offset != 0)) {
			fdi->dir_cluster = fdi->wdi.reference;
			fdi->cluster_offset = 0;
			fdi->preload_count = 0;
		}
	}
	if (fdi->wdi.flags & WC_FLAGS_RELOAD) {
		fdi->wdi.flags &= ~WC_FLAGS_RELOAD;
		fdi->preload_count = 0;
	}

	for (;;) {
		if (!fdi->preload_count) {
			ret = fat_dir_preload(mydata, fdi);
			if (ret <= 0)
				return ret; /* Error or done */
		}

		dentry = (struct dir_entry *)
			               (mydata->dirbuf + fdi->preload_offset);
		if (dentry->name[0] == 0)
			return 0;	/* First unused entry: done */

		fdi->preload_offset += sizeof(struct dir_entry);
		if (fdi->preload_offset
		    >= fdi->preload_count * mydata->sect_size) {
			fdi->preload_offset = 0;
			fdi->cluster_offset += fdi->preload_count;
			fdi->preload_count = 0;
		}

		if (dentry->name[0] == DELETED_FLAG)
			continue;

#ifdef CONFIG_SUPPORT_VFAT
		if ((dentry->attr & ATTR_VFAT) == ATTR_VFAT) {
			/* Get VFAT longname slot */
			dslot = (struct dir_slot *)dentry;
			id = dslot->id;
			if (id & DELETED_LONG_ENTRY) {
				printf("### DELETED_LONG_ENTRY\n");
				continue;
			}
			if (id & LAST_LONG_ENTRY_MASK) {
				/* New filename, clear old content */
				alias_checksum = dslot->alias_checksum;
				id &= ~LAST_LONG_ENTRY_MASK;
				memset(wfi->file_name, 0, WC_NAME_MAX);
			}
			if (--id > VFAT_SEQ_MAX) {
				printf("### Bad VFAT slot index\n");
				continue;
			}

			ds = (__u8 *)dslot;
			namepart = &wfi->file_name[id*13];

			i = 0;
			do {
				int idx = slot_index_table[i];

				c = ds[idx + 1] * 256 + ds[idx];
				if (c && !IS_FAT_CHAR(c))
					c = '_';
				namepart[i] = (char)c;
			} while (c && (++i < 13));
			continue;
		}

		/* Do we have a long name and does the checksum match? Then
		   this directory entry is the short name alias for the VFAT
		   long name entry, use the long name */
		if (wfi->file_name[0]
		    && (fat_checksum(dentry->name) == alias_checksum)) {
			break;
		}
#endif
		get_shortname(wfi->file_name, dentry->name, dentry->lcase);
		break;
	}

	/* Fill in fileinfo data */
	if (dentry->attr & ATTR_DIR)
		wfi->file_type = WC_TYPE_DIRECTORY;
	else if (dentry->attr & ATTR_VOLUME)
		wfi->file_type = WC_TYPE_VOLUME;
	else
		wfi->file_type = WC_TYPE_REGULAR;
	wfi->file_size = (loff_t)FAT2CPU32(dentry->size);
	wfi->reference = FAT2CPU16(dentry->start);
	if (mydata->fatsize == 32)
		wfi->reference += FAT2CPU16(dentry->starthi) << 16;

	return 1;
}

/*
 * Read the file given in wfistarting at cluster wfi->reference.
 *
 * ATTENTION: We are re-using the directory preload buffer here. This should
 * be no problem as long as we don't require the directory content anymore.
 *
 * Parameters:
 *   wdi:     Pointer to current directory entry
 *   wfi:     Information of file to load; especially wfi->reference is the
 *            start cluster of the file
 *   skip:    Number of bytes to skip at beginning of file
 *   buffer:  Pointer to buffer where to store data
 *   maxsize: Maximum number of bytes to read (0: whole file)
 *
 * Return:
 *   Number of read bytes
 */
static unsigned long fat_read_at(struct wc_dirinfo *wdi,
				 struct wc_fileinfo *wfi, unsigned long skip,
				 void *buffer, unsigned long maxsize)
{
	unsigned long bytes_next_chunk;
	unsigned long total_bytes_loaded;
	unsigned long remaining;
	struct fsdata *mydata = &myfsdata;
	unsigned long bytes_per_cluster;
	unsigned long sect_size;
	int warning = 0;
	__u32 cluster;
	__u32 tmp;
	__u32 sector_count;
	__u32 sector;

	remaining = (unsigned long)wfi->file_size;
	if (!remaining || (skip >= remaining))
		return 0;		/* Empty file or seek past EOF */
	if (maxsize && (remaining > skip + maxsize))
		remaining = skip + maxsize;
	if (!buffer)
		return remaining;	/* Do not read, just return size */

	sect_size = mydata->sect_size;
	bytes_per_cluster = mydata->clust_size * sect_size;
	cluster = wfi->reference;
	total_bytes_loaded = 0;

	do {
		/* Combine sectors to a long chunk if they are in sequence */
		sector = clust2sect(mydata, cluster);
		bytes_next_chunk = 0;
		do {
			if (remaining <= bytes_per_cluster) {
				bytes_next_chunk += remaining;
				remaining = 0;
				break;
			}
			bytes_next_chunk += bytes_per_cluster;
			remaining -= bytes_per_cluster;

			tmp = cluster;
			cluster = get_fatent(mydata, cluster);

			/* Return on unexpected EOF or invalid cluster */
			if ((cluster < 2) || (cluster >= mydata->max_cluster))
				return total_bytes_loaded;
		} while (cluster == tmp + 1);

		debug("Next chunk: 0x%lx bytes at sector 0x%x\n",
		      bytes_next_chunk, sector);

		/* Number of full sectors in this chunk */
		sector_count = bytes_next_chunk / sect_size;

		/* Skip unwanted full sectors at beginning of chunk */
		if (skip) {
			__u32 sectors_to_skip;
			unsigned long bytes_to_skip;

			sectors_to_skip = skip / sect_size;
			if (sectors_to_skip > sector_count)
				sectors_to_skip = sector_count;
			sector_count -= sectors_to_skip;
			bytes_to_skip = sectors_to_skip * sect_size;
			bytes_next_chunk -= bytes_to_skip;
			skip -= bytes_to_skip;
			debug("Skip 0x%lx bytes (=%u full sectors) at sector "
			      "0x%x\n", bytes_to_skip, sectors_to_skip, sector);
			sector += sectors_to_skip;
		}
		
		/* Skip part of a sector and read part of a sector if the
		   beginning is not at a sector boundary. */
		if (skip && bytes_next_chunk) {
			unsigned long bytes_to_copy;
			bytes_to_copy = sect_size;
			if (bytes_to_copy > bytes_next_chunk)
				bytes_to_copy = bytes_next_chunk;
			bytes_next_chunk -= bytes_to_copy;
			bytes_to_copy -= skip;
			debug("Skip 0x%lx bytes and read 0x%lx bytes at"
			      " sector 0x%x\n", skip, bytes_to_copy, sector);
			if (disk_read(sector, 1, mydata->dirbuf) != 1)
				return total_bytes_loaded;
			memcpy(buffer, mydata->dirbuf + skip, bytes_to_copy);
			total_bytes_loaded += bytes_to_copy;
			buffer += bytes_to_copy;
			skip = 0;
			sector++;
			if (sector_count > 0)
				sector_count--;
		}

		/* Load full sectors */
		if (sector_count) {
			debug("Load 0x%lx bytes (=%d full sectors) at sector "
			      "0x%x\n", sector_count * sect_size, sector_count,
			      sector);
			if ((unsigned long)buffer & (ARCH_DMA_MINALIGN - 1)) {
				void *temp = mydata->dirbuf;
				__u32 i;

				/* Read sectors with single reads */
				if (!warning) {
					puts("Buffer unaligned, using slow"
					     " single-sector reads\n");
					warning = 1;
				}
				for (i = 0; i < sector_count; i++) {
					if (disk_read(sector, 1, temp) != 1)
						return total_bytes_loaded;
					sector++;
					memcpy(buffer, temp, sect_size);
					buffer += sect_size;
					total_bytes_loaded += sect_size;
					bytes_next_chunk -= sect_size;
				}
			} else {
				unsigned long bytes_loaded;
				__u32 read;

				/* Read sectors in one go */
				read = disk_read(sector, sector_count, buffer);
				sector += sector_count;
				bytes_loaded = read * sect_size;
				buffer += bytes_loaded;
				bytes_next_chunk -= bytes_loaded;
				total_bytes_loaded += bytes_loaded;
				if (read != sector_count)
					return total_bytes_loaded;
			}
		}

		/* Load final bytes from last sector. */
		if (bytes_next_chunk) {
			debug("Load 0x%lx bytes and ignore 0x%lx bytes"
			      " from sector 0x%x\n", bytes_next_chunk,
			      sect_size - bytes_next_chunk, sector);
			if (disk_read(sector, 1, mydata->dirbuf) != 1)
				return total_bytes_loaded;
			memcpy(buffer, mydata->dirbuf, bytes_next_chunk);
			total_bytes_loaded += bytes_next_chunk;
			buffer += bytes_next_chunk;
			bytes_next_chunk = 0;
		}
	} while (remaining);

	return total_bytes_loaded;
}

/* Function to write a file, see fat_write.c */
extern unsigned long fat_write(struct wc_dirinfo *wdi, struct wc_fileinfo *wfi,
			       void *buffer, unsigned long maxsize);

/* Callback functions for wildcard module to access directories and files */
const struct wc_filesystem_ops fat_ops = {
	"FAT",
	fat_alloc_dir,
	fat_free_dir,
	fat_get_fileinfo,
	fat_read_at,
#ifdef CONFIG_FAT_WRITE
	fat_write,
#else
	NULL,
#endif
	NULL
};

/*
 * List directory contents. This may actually be a sequence of lists as the
 * pattern may refer to several directories.
 *
 * Parameter:
 *   pattern: File or path name to list; may contain wildcards * and ?
 *
 * Return:
 *   0:  OK
 *   -1: Error, e.g. while reading data from device
 */
int file_fat_ls(const char *pattern)
{
	struct wc_fileinfo wfi;

	/* Prepare file info for root directory */
	wfi.reference = 0;
	wfi.file_size = 0;
	wfi.file_type = WC_TYPE_DIRECTORY;
	wfi.pattern = pattern;
	wfi.file_name[0] = '\0';

	return wildcard_ls(&wfi, &fat_ops);
}

/*
 * Read the given file with up to maxsize bytes.
 *
 * Parameters:
 *   pattern: file name; may contain wildcards, but must match uniquely
 *   buffer:  Pointer to buffer where to store data
 *   maxsize: Maximum number of bytes to read (0: whole file)
 *
 * Return:
 *   Number of read bytes or -1 (0xFFFFFFFF) on error (e.g. while loading data
 *   from device, non-existent file, etc.)
 */
long file_fat_read(const char *pattern, void *buffer, unsigned long maxsize)
{
	struct wc_fileinfo wfi;

	/* Prepare file info for root directory */
	wfi.reference = 0;
	wfi.file_size = 0;
	wfi.file_type = WC_TYPE_DIRECTORY;
	wfi.pattern = pattern;
	wfi.file_name[0] = '\0';

	return (long)wildcard_read_at(&wfi, &fat_ops, 0, buffer, maxsize);
}

/*
 * Read the given file with up to maxsize bytes.
 *
 * Parameters:
 *   pattern: file name; may contain wildcards, but must match uniquely
 *   pos:     Start reading at pos (i.e. skip pos bytes at beginning of file)
 *   buffer:  Pointer to buffer where to store data
 *   maxsize: Maximum number of bytes to read (0: whole file)
 *
 * Return:
 *   Number of read bytes or -1 (0xFFFFFFFF) on error (e.g. while loading data
 *   from device, non-existent file, etc.)
 */
long file_fat_read_at(const char *pattern, unsigned long pos, void *buffer,
		      unsigned long maxsize)
{
	struct wc_fileinfo wfi;

	/* Prepare file info for root directory */
	wfi.reference = 0;
	wfi.file_size = 0;
	wfi.file_type = WC_TYPE_DIRECTORY;
	wfi.pattern = pattern;
	wfi.file_name[0] = '\0';

	return (long)wildcard_read_at(&wfi, &fat_ops, pos, buffer, maxsize);
}

/*
 * Check device and initialize device specific data (FAT type, etc.)
 *
 * Parameters:
 *   dev_desc: Device description
 *   part_no:  Partition number
 *
 * Return:
 *   0:  OK, FAT filesystem initialized
 *   -1: Error, e.g. no FAT filesystem, error reading data from device, etc.
 */
int fat_set_blk_dev(block_dev_desc_t *dev_desc, disk_partition_t *info)
{
	struct boot_sector *bs;
	__u32 clusters;
	struct volume_info *vistart;
	struct fsdata *mydata = &myfsdata;
	ALLOC_CACHE_ALIGN_BUFFER(unsigned char, buffer, dev_desc->blksz);

	cur_dev = dev_desc;
	cur_part_info = *info;

	/* Make sure it has a valid FAT header */
	if (disk_read(0, 1, buffer) != 1) {
		cur_dev = NULL;
		return -1;
	}

	/* Check if it's actually a DOS volume */
	if (memcmp(buffer + DOS_BOOT_MAGIC_OFFSET, "\x55\xAA", 2)) {
		cur_dev = NULL;
		return -1;
	}

	/* Extract the data that we need for accessing the file system */
	bs = (struct boot_sector *)buffer;

	/* Get size of device (in sectors) */
	mydata->sectors = (bs->sectors[1] << 8) + bs->sectors[0];
	if (mydata->sectors == 0)
		mydata->sectors = FAT2CPU32(bs->total_sect);

	/* Get size of a sector (in bytes) */
	mydata->sect_size = (bs->sector_size[1] << 8) + bs->sector_size[0];
	if (mydata->sect_size != cur_part_info.blksz) {
		printf("Error: FAT sector size mismatch (fs=%hu, dev=%lu)\n",
		       mydata->sect_size, cur_part_info.blksz);
		cur_dev = NULL;
		return -1;
	}

	/* Get size of a cluster (in sectors) */
	mydata->clust_size = bs->cluster_size;

	/* The first FAT starts behind the reserved sectors */
	mydata->fat_sect = FAT2CPU16(bs->reserved);

	/* Get the size of a FAT (in sectors) */
	mydata->fat_length = FAT2CPU16(bs->fat_length);
	if (mydata->fat_length == 0) {
		vistart = (struct volume_info *)
			                 (buffer + sizeof(struct boot_sector));
		mydata->fat_length = FAT2CPU32(bs->fat32_length);
	} else
		vistart = (struct volume_info *)&(bs->fat32_length);

	/* Get beginning of root directory */
	mydata->rootdir_sect = mydata->fat_length * bs->fats + mydata->fat_sect;

	/* Get the size of the root directory (in sectors); in case of FAT32,
	   bs->dir_entries (and therefore rootdir_length) will be zero as the
	   root directory is located in the data region and can grow
	   arbitrarily. */
	mydata->rootdir_length =
		((bs->dir_entries[1] << 8) + bs->dir_entries[0])
		* sizeof(struct dir_entry) / mydata->sect_size;

	/* Get the beginning of the data region */
	mydata->data_sect = mydata->rootdir_sect + mydata->rootdir_length;

	/* Get the size of the data region (in sectors) */
	mydata->data_length = mydata->sectors - mydata->data_sect;

	/* Get beginning of root directory if located in data region */
	mydata->rootdir_clust = mydata->rootdir_length ? 0
		                               : FAT2CPU32(bs->root_cluster);
	/* Get number of clusters */
	clusters = mydata->data_length / mydata->clust_size;

	/* Get FAT type. FAT drivers should look only at the number of
	   clusters to distinguish between FAT12, FAT16, and FAT32. A FAT file
	   system with up to 4084 clusters is FAT12, 4085 to 65525 clusters is
	   FAT16 and (despite the name) FAT32 only uses 28 bit FAT entries and
	   therefore 65526 to 268435445 clusters is a FAT32 file system. */
	mydata->fatbuf_sectors = CONFIG_SYS_FAT_PRELOAD_FAT/mydata->sect_size;
	mydata->fatbuf_entries = mydata->sect_size;
	if (clusters <= 4084) {
		mydata->fatsize = 12;
		mydata->eof = 0xFF8;
		mydata->fatbuf_sectors /= 3;
		mydata->fatbuf_entries *= mydata->fatbuf_sectors * 2;
		mydata->fatbuf_sectors *= 3;
	} else if (clusters <= 65525) {
		mydata->fatsize = 16;
		mydata->eof = 0xFFF8;
		mydata->fatbuf_entries *= mydata->fatbuf_sectors;
		mydata->fatbuf_entries /= 2;
	} else if (clusters <= 268435445) {
		mydata->fatsize = 32;
		mydata->eof = 0x0FFFFFF8;
		mydata->fatbuf_entries *= mydata->fatbuf_sectors;
		mydata->fatbuf_entries /= 4;
	} else {
		cur_dev = NULL;		  /* Unknown FAT type (exFAT?) */
		return -1;
	}
	mydata->fatbufnum = (__u32)-1;
	mydata->fatbuf = fat_buffer;
	mydata->dirbuf = dir_buffer;
	mydata->max_cluster = clusters + 2;

	/* Get volume name from volume information */
	mydata->volume_name[0] = '\0';
	if (vistart->ext_boot_sign == 0x29)
		get_shortname(mydata->volume_name, vistart->volume_label, 0);
	if (mydata->volume_name[0] == '\0')
		strcpy(mydata->volume_name, FAT_DEF_VOLUME);

	return 0;
}

/*
 * Show information on current device and FAT filesystem.
 *
 * Return:
 *   0: OK
 *   1: No current device
 */
int file_fat_detectfs(void)
{
	if (cur_dev == NULL) {
		puts("No current device\n");
		return 1;
	}

	printf("FAT Device %d:\n", cur_dev->dev);
	dev_print(cur_dev);

	printf("Partition %d:\n", cur_part_nr);
	printf("  Filesystem: FAT%u\n", myfsdata.fatsize);
	printf("  Volume name: %s\n", myfsdata.volume_name);

	/* We could additionally search the root directory for the volume
	   name, and show both if they differ, but is it worth it? */

	return 0;
}

int fat_read_file(const char *filename, void *buf, int offset, int len)
{
	return file_fat_read_at(filename, offset, buf, len);
}

int fat_exists(const char *pattern)
{
	struct wc_fileinfo wfi;

	/* Prepare file info for root directory */
	wfi.reference = 0;
	wfi.file_size = 0;
	wfi.file_type = WC_TYPE_DIRECTORY;
	wfi.pattern = pattern;
	wfi.file_name[0] = '\0';

	return wildcard_exists(&wfi, &fat_ops);
}

int fat_size(const char *filename)
{
	return file_fat_read(filename, NULL, 0);
}

void fat_close(void)
{
}
