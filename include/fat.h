/*
 * R/O (V)FAT 12/16/32 filesystem implementation by Marcus Sundberg
 *
 * 2002-07-28 - rjones@nexus-tech.net - ported to ppcboot v1.1.6
 * 2003-03-10 - kharris@nexus-tech.net - ported to u-boot
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _FAT_H_
#define _FAT_H_

#include <asm/byteorder.h>
#include <wildcard.h>			/* struct wc_dirinfo, ... */

#define CONFIG_SUPPORT_VFAT
/* Maximum Long File Name length supported here is 128 UTF-16 code units */
#define VFAT_SEQ_MAX	19	    /* Maximum slots, each 13 UTF-16 chars */
#define FAT_DEF_VOLUME	"NO NAME"   /* Default volume name if not found */

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

#define INVALID_CLUSTER	0xFFFFFFFF

/*
 * Indicates that the entry is the last long entry in a set of long
 * dir entries
 */
#define LAST_LONG_ENTRY_MASK	0x40
#define DELETED_LONG_ENTRY      0x80

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

#define TOLOWER(c)	if((c) >= 'A' && (c) <= 'Z'){(c)+=('a' - 'A');}
#define TOUPPER(c)	if ((c) >= 'a' && (c) <= 'z') \
				(c) -= ('a' - 'A');

struct boot_sector {		/* Offset */
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
};

struct volume_info
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
};

/* Standard FAT directory entry with 8.3 name */
struct dir_entry {
	char	name[8+3];	/* 0x00: Name and extension */
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
};

/* VFAT directory slot for up to 13 file name characters; the name ends with
   a 0x0000 character and any unused characters should be set to 0xFFFF */
struct dir_slot {
	__u8	id;		/* 0x00: Sequence number for slot */
	__u8	name0_4[10];	/* 0x01: First 5 characters in name */
	__u8	attr;		/* 0x0B: Attribute bits (0x0F for VFAT) */
	__u8	reserved;	/* 0x0C: Unused (0x00, see lcase above) */
	__u8	alias_checksum; /* 0x0D: Checksum for 8.3 alias */
	__u8	name5_10[12];	/* 0x0E: 6 more characters in name */
	__u16	start;		/* 0x1A: Unused (0x0000) */
	__u8	name11_12[4];	/* 0x1C: Last 2 characters in name */
};

/*
 * Private filesystem parameters
 *
 * Note: FAT buffer has to be 32 bit aligned
 * (see FAT32 accesses)
 */
struct fsdata {
	__u8	*dirbuf;	/* Pointer to directory preload buffer */
	__u8	*fatbuf;	/* Pointer to FAT preload buffer */
	__u32	fatbufnum;	/* Currently loaded FAT chunk present in
				   fatbuf; used by get_fatent(), init to -1 */
	__u32	fatbuf_sectors;	/* Usable size of fatbuf (in sectors) */
	__u32	fatbuf_entries;	/* Number of FAT entries in fatbuf */
	__u32	fatsize;	/* FAT type (12, 16 or 32) */
	__u32   sectors;	/* Total sector count (size of device) */
	__u16	sect_size;	/* Size of a sector (in bytes, usually 512) */
	__u16	clust_size;	/* Size of a cluster (in sectors, power of 2) */
	__u32	fat_sect;	/* Starting sector of the FAT */
	__u32	fat_length;	/* Length of FAT (in sectors) */
	__u32	rootdir_sect;	/* Start sector of root directory */
	__u32   rootdir_length;	/* Length of root directory (in sectors) */
	__u32   rootdir_clust;	/* Start cluster if root dir in data region */
	__u32   data_sect;      /* Start sector of data region */
	__u32   data_length;	/* Length of data region (in sectors) */
	__u32   max_cluster;	/* First cluster outside of file system */
	__u32   eof;		/* First cluster number accepted as EOF */
	char    volume_name[13];/* Volume name read from boot sector */
};

typedef int	(file_detectfs_func)(void);
typedef int	(file_ls_func)(const char *dir);
typedef long	(file_read_func)(const char *filename, void *buffer,
				 unsigned long maxsize);

struct filesystem {
	file_detectfs_func	*detect;
	file_ls_func		*ls;
	file_read_func		*read;
	const char		name[12];
};

/* FAT tables */
file_detectfs_func	file_fat_detectfs;
file_ls_func		file_fat_ls;
file_read_func		file_fat_read;

/* Currently this doesn't check if the dir exists or is valid... */
int file_cd(const char *path);
int file_fat_detectfs(void);
int file_fat_ls(const char *pattern);
int fat_exists(const char *pattern);
long file_fat_read_at(const char *pattern, unsigned long pos,
		      void *buffer, unsigned long maxsize);
long file_fat_read(const char *pattern, void *buffer, unsigned long maxsize);
const char *file_getfsname(int idx);
int fat_set_blk_dev(block_dev_desc_t *rbdd, disk_partition_t *info);
int fat_register_device(block_dev_desc_t *dev_desc, int part_no);

int file_fat_write(const char *pattern, void *buffer, unsigned long maxsize);
int fat_read_file(const char *filename, void *buf, int offset, int len);
void fat_close(void);
#endif /* _FAT_H_ */
