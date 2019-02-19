/*
 * wildcard.h
 *
 * Generic commands to read and write a file and to list directories. These
 * commands support wildcards in file/path names.
 *
 * (C) Copyright 2012-2018 Hartmut Keller (keller@fs-net.de)
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _WILDCARD_H_
#define _WILDCARD_H_

/* Maximum sizes */
#define WC_NAME_MAX	256		/* Maximum filename size in bytes */
//#define WC_PATH_MAX	1024		/* Maximum path length in bytes */
#define WC_SYMLINK_MAX	256		/* Maximum symlink length */

/* Flags for get_fileinfo(); these flags should autoclear, i.e. get_fileinfo
   must clear them after they have taken effect once. */
#define WC_FLAGS_REWIND 0x01		/* (Re)start at beginning of dir */
#define WC_FLAGS_RELOAD	0x02		/* Resume dir after handling subdir */

/* Filesystem doing the call; used as index into the wc_filesystem_ops array */
enum wc_filesystem {
#ifdef CONFIG_FS_FAT
	WC_FS_FAT,
#endif
	WC_FS_COUNT			/* Keep as last entry */
};

/* Possible file types */
enum wc_file_type {
	WC_TYPE_NONE,
	WC_TYPE_REGULAR,
	WC_TYPE_DIRECTORY,
	WC_TYPE_VOLUME,
	WC_TYPE_SYMLINK
};

/* Directory info; we never create an instance of this structure directly, it
   is only meant to be embedded by a file system into its own specific
   structure (like a derived object). A new instance of such an object is
   then allocated with alloc_dir() and deallocated with free_dir(). The parent
   entry must link directly to the wc_dirinfo part of the parent object. */
struct wc_dirinfo {
	struct wc_dirinfo *parent;	/* Parent directory */
	unsigned long reference;	/* Dir reference, e.g. FAT cluster */
	const char *dir_pattern;	/* Pattern to search this directory */
	unsigned int flags;		/* WC_FLAGS_* */
	char dir_name[WC_NAME_MAX];	/* Name of this directory */
};

/* File info. The reference is file system specific. It must refer the file
   or subdirectory in a unique way. */
struct wc_fileinfo {
	unsigned long reference;	/* File reference, e.g. FAT cluster */
	loff_t file_size;		/* File size */
	enum wc_file_type file_type;	/* File type */
	const char *pattern;		/* Remaining pattern after match */
	char file_name[WC_NAME_MAX];	/* File name */
};

/* Calls into the filesystem to do the necessary data accesses */
struct wc_fsops {
	const char *name;
	struct wc_dirinfo *(*alloc_dir)(void);
	void (*free_dir)(struct wc_dirinfo *wdi);
	int (*get_fileinfo)(struct wc_dirinfo *wdi, struct wc_fileinfo *wfi);
	int (*read_file_at)(struct wc_dirinfo *wdi, struct wc_fileinfo *wfi, 
			    void *buffer, loff_t offset, loff_t len,
			    loff_t *actread);
	int (*write_file)(struct wc_dirinfo *wdi, struct wc_fileinfo *wfi,
			  const void *buffer, loff_t offset, loff_t len,
			  loff_t *actwrite);
	int (*get_symlink)(struct wc_dirinfo *wdi, struct wc_fileinfo *wfi,
			   char *symlink);
};


/* ------ Exported functions ------ */

/* List directory contents */
int wildcard_ls(struct wc_fileinfo *wfi, const struct wc_fsops *ops);

/* Read a file */
int wildcard_read_at(struct wc_fileinfo *wfi, const struct wc_fsops *ops,
		     void *buffer, loff_t offset, loff_t len, loff_t *actread);

/* Write a file */
int wildcard_write(struct wc_fileinfo *wfi, const struct wc_fsops *ops,
		   const void *buffer, loff_t offset, loff_t len,
		   loff_t *actwrite);

/* Check for existence of a file */
int wildcard_exists(struct wc_fileinfo *wfi, const struct wc_fsops *ops);
#endif /*!_WILDCARD_H_*/
