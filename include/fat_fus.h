/*
 * fat_fus.h
 *
 * (C) 2012-2019 Hartmut Keller (keller@fs-net.de)
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _FAT_FUS_H_
#define _FAT_FUS_H_

/* Maximum Long File Name length supported here is 128 UTF-16 code units */
#define VFAT_SEQ_MAX	19	    /* Maximum slots, each 13 UTF-16 chars */
#define FAT_DEF_VOLUME	"NO NAME"   /* Default volume name if not found */

#define INVALID_CLUSTER	0xFFFFFFFF

#define TOLOWER(c)	if((c) >= 'A' && (c) <= 'Z'){(c)+=('a' - 'A');}
#define TOUPPER(c)	if ((c) >= 'a' && (c) <= 'z') \
				(c) -= ('a' - 'A');

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

typedef int (file_detectfs_func)(void);
typedef int (file_ls_func)(const char *dir);
typedef int (file_read_func)(const char *filename, void *buffer,
			 int maxsize);

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
#endif /* _FAT_FUS_H_ */
