/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * R/O (V)FAT 12/16/32 filesystem implementation by Marcus Sundberg
 *
 * 2002-07-28 - rjones@nexus-tech.net - ported to ppcboot v1.1.6
 * 2003-03-10 - kharris@nexus-tech.net - ported to u-boot
 */

#ifndef _FAT_H_
#define _FAT_H_

#include <asm/byteorder.h>
#include <fs.h>

/* Maximum Long File Name length supported here is 128 UTF-16 code units */
#define VFAT_MAXLEN_BYTES	256 /* Maximum LFN buffer in bytes */
#define VFAT_MAXSEQ		9   /* Up to 9 of 13 2-byte UTF-16 entries */
#define PREFETCH_BLOCKS		2

#define MAX_CLUSTSIZE	CONFIG_FS_FAT_MAX_CLUSTSIZE

#define DIRENTSPERBLOCK	(mydata->sect_size / sizeof(dir_entry))
#define DIRENTSPERCLUST	((mydata->clust_size * mydata->sect_size) / \
			 sizeof(dir_entry))

#define FATBUFBLOCKS	6
#define FATBUFSIZE	(mydata->sect_size * FATBUFBLOCKS)
#define FAT12BUFSIZE	((FATBUFSIZE*2)/3)
#define FAT16BUFSIZE	(FATBUFSIZE/2)
#define FAT32BUFSIZE	(FATBUFSIZE/4)

/* Maximum number of entry for long file name according to spec */
#define MAX_LFN_SLOT	20

/* Filesystem identifiers */
#define FAT12_SIGN	"FAT12   "
#define FAT16_SIGN	"FAT16   "
#define FAT32_SIGN	"FAT32   "
#define SIGNLEN		8

/* File attributes */
#define ATTR_RO		1
#define ATTR_HIDDEN	2
#define ATTR_SYS	4
#define ATTR_VOLUME	8
#define ATTR_DIR	16
#define ATTR_ARCH	32

#define ATTR_VFAT	(ATTR_RO | ATTR_HIDDEN | ATTR_SYS | ATTR_VOLUME)

#define DELETED_FLAG	((char)0xe5) /* Marks deleted files when in name[0] */
#define aRING		0x05	     /* Used as special character in name[0] */

/*
 * Indicates that the entry is the last long entry in a set of long
 * dir entries
 */
#define LAST_LONG_ENTRY_MASK	0x40
#define DELETED_LONG_ENTRY      0x80

#define ISDIRDELIM(c)	((c) == '/' || (c) == '\\')

#define FSTYPE_NONE	(-1)

#if defined(__linux__) && defined(__KERNEL__)
#define FAT2CPU16	le16_to_cpu
#define FAT2CPU32	le32_to_cpu
#else
#if __LITTLE_ENDIAN
#define FAT2CPU16(x)	(x)
#define FAT2CPU32(x)	(x)
#else
#define FAT2CPU16(x)	((((x) & 0x00ff) << 8) | (((x) & 0xff00) >> 8))
#define FAT2CPU32(x)	((((x) & 0x000000ff) << 24)  |	\
			 (((x) & 0x0000ff00) << 8)  |	\
			 (((x) & 0x00ff0000) >> 8)  |	\
			 (((x) & 0xff000000) >> 24))
#endif
#endif

#define START(dent)	(FAT2CPU16((dent)->start) \
			+ (mydata->fatsize != 32 ? 0 : \
			  (FAT2CPU16((dent)->starthi) << 16)))
#define IS_LAST_CLUST(x, fatsize) ((x) >= ((fatsize) != 32 ? \
					((fatsize) != 16 ? 0xff8 : 0xfff8) : \
					0xffffff8))
#define CHECK_CLUST(x, fatsize) ((x) <= 1 || \
				(x) >= ((fatsize) != 32 ? \
					((fatsize) != 16 ? 0xff0 : 0xfff0) : \
					0xffffff0))

typedef struct boot_sector {	/* Offset */
	__u8	ignored[3];	/* 0x000: Bootstrap code */
	char	system_id[8];	/* 0x003: Name of fs */
	__u8	sector_size[2];	/* 0x00B: Bytes/sector */
	__u8	cluster_size;	/* 0x00D: Sectors/cluster */
	__u16	reserved;	/* 0x00E: Number of reserved sectors */
	__u8	fats;		/* 0x010: Number of FATs */
	__u8	dir_entries[2];	/* 0x011: Number of root directory entries */
	__u8	sectors[2];	/* 0x013: Number of sectors */
	__u8	media;		/* 0x015: Media code */
	__u16	fat_length;	/* 0x016: Sectors/FAT */
	__u16	secs_track;	/* 0x018: Sectors/track */
	__u16	heads;		/* 0x01A: Number of heads */
	__u32	hidden;		/* 0x01C: Number of hidden sectors */
	__u32	total_sect;	/* 0x020: Number of sectors (if sectors == 0) */

	/* FAT32 Extended BIOS Parameter Block; valid if fat_length == 0,
	   i.e. in rare cases it may also be present on FAT12/FAT16 */
	__u32	fat32_length;	/* 0x024: Sectors/FAT */
	__u16	flags;		/* 0x028: Bit 7: FAT mirroring, then
				          Bit 3..0 is active FAT */
	__u8	version[2];	/* 0x02A: Filesystem version */
	__u32	root_cluster;	/* 0x02C: First cluster of root directory (if
				          dir_entries == 0) */
	__u16	info_sector;	/* 0x030: Filesystem information sector */
	__u16	backup_boot;	/* 0x032: Sector for boot sector backup */
	__u16	unused[6];	/* 0x034: Unused */
} boot_sector;

typedef struct volume_info
{				/* FAT12+16/32 offset */
	__u8 drive_number;	/* 0x024/0x040: BIOS drive number */
	__u8 unused;		/* 0x025/0x041: Unused */
	__u8 ext_boot_sign;	/* 0x026/0x042: 0x28: only Volume ID follows
				                0x29: Volume ID, volume label
				                      and FS type follow */
	__u8 volume_id[4];	/* 0x027/0x043: Volume ID number */
	char volume_label[11];	/* 0x02B/0x047: Volume label */
	char fs_type[8];	/* 0x036/0x052: FS type, padded with blanks,
				                typically one of "FAT12   ",
						"FAT16   ", or "FAT32   " */
	/* Boot code comes next, all but 2 bytes to fill up sector */
	/* Boot sign comes last, 2 bytes */
} volume_info;

/* see dir_entry::lcase: */
#define CASE_LOWER_BASE	8	/* base (name) is lower case */
#define CASE_LOWER_EXT	16	/* extension is lower case */

/* Standard FAT directory entry with 8.3 name */
typedef struct dir_entry {
#ifdef CONFIG_FAT_FUS
	char	name[8+3];	/* 0x00: Name and extension */
#else
	char	name[8],ext[3];	/* Name and extension */
#endif
	__u8	attr;		/* 0x0B: Attribute bits */
	__u8	lcase;		/* 0x0C: Case for basename and extension */
	__u8	ctime_ms;	/* 0x0D: Creation time, 10ms units 0..199 */
	__u16	ctime;		/* 0x0E: Creation time */
	__u16	cdate;		/* 0x10: Creation date */
	__u16	adate;		/* 0x12: Last access date */
	__u16	starthi;	/* 0x14: High 16 bits of cluster in FAT32 */
	__u16	time;		/* 0x16: Last modified time */
	__u16   date;		/* 0x18: Last modified date */
	__u16   start;		/* 0x1A: First cluster, low 16 bits in FAT32 */
	__u32	size;		/* 0x1C: File size in bytes */
} dir_entry;

/* VFAT directory slot for up to 13 file name characters; the name ends with
   a 0x0000 character and any unused characters should be set to 0xFFFF */
typedef struct dir_slot {
	__u8	id;		/* 0x00: Sequence number for slot */
	__u8	name0_4[10];	/* 0x01: First 5 characters in name */
	__u8	attr;		/* 0x0B: Attribute bits (0x0F for VFAT) */
	__u8	reserved;	/* 0x0C: Unused (0x00, see lcase above) */
	__u8	alias_checksum; /* 0x0D: Checksum for 8.3 alias */
	__u8	name5_10[12];	/* 0x0E: 6 more characters in name */
	__u16	start;		/* 0x1A: Unused (0x0000) */
	__u8	name11_12[4];	/* 0x1C: Last 2 characters in name */
} dir_slot;

/*
 * Private filesystem parameters
 *
 * Note: FAT buffer has to be 32 bit aligned
 * (see FAT32 accesses)
 */
#ifndef CONFIG_FAT_FUS
typedef struct fsdata {
	__u8	*fatbuf;	/* Current FAT buffer */
	int	fatsize;	/* Size of FAT in bits */
	__u32	fatlength;	/* Length of FAT in sectors */
	__u16	fat_sect;	/* Starting sector of the FAT */
	__u8	fat_dirty;      /* Set if fatbuf has been modified */
	__u32	rootdir_sect;	/* Start sector of root directory */
	__u16	sect_size;	/* Size of sectors in bytes */
	__u16	clust_size;	/* Size of clusters in sectors */
	int	data_begin;	/* The sector of the first cluster, can be negative */
	int	fatbufnum;	/* Used by get_fatent, init to -1 */
	int	rootdir_size;	/* Size of root dir for non-FAT32 */
	__u32	root_cluster;	/* First cluster of root dir for FAT32 */
} fsdata;

static inline u32 clust_to_sect(fsdata *fsdata, u32 clust)
{
	return fsdata->data_begin + clust * fsdata->clust_size;
}

static inline u32 sect_to_clust(fsdata *fsdata, int sect)
{
	return (sect - fsdata->data_begin) / fsdata->clust_size;
}
#endif /* !CONFIG_FAT_FUS */

int file_fat_ls(const char *pattern); /* F&S */
int file_fat_detectfs(void);
int fat_exists(const char *filename);
int fat_size(const char *filename, loff_t *size);
int file_fat_read_at(const char *filename, loff_t pos, void *buffer,
		     loff_t maxsize, loff_t *actread);
int file_fat_read(const char *filename, void *buffer, int maxsize);
int fat_set_blk_dev(struct blk_desc *rbdd, disk_partition_t *info);
int fat_register_device(struct blk_desc *dev_desc, int part_no);

int file_fat_write(const char *filename, void *buf, loff_t offset, loff_t len,
		   loff_t *actwrite);
int fat_read_file(const char *filename, void *buf, loff_t offset, loff_t len,
		  loff_t *actread);
int fat_opendir(const char *filename, struct fs_dir_stream **dirsp);
int fat_readdir(struct fs_dir_stream *dirs, struct fs_dirent **dentp);
void fat_closedir(struct fs_dir_stream *dirs);
void fat_close(void);
#endif /* _FAT_H_ */
