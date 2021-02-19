/*
 * wildcard.c
 *
 * Generic commands to read and write a file and to list directories. These
 * commands support wildcards in file/path names.
 *
 * (C) Copyright 2012-2018 Hartmut Keller (keller@fs-net.de)
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <config.h>
#include <wildcard.h>
#include <fat.h>
#include <errno.h>

/* Pointer to file system calls for data access */
static const struct wc_fsops *fs_ops;

/**
 * skip_dir_delim() - Remove all leading directory delimiters
 * @pattern: String to check
 *
 * Remove all leading directory delimiters '/' from the pattern.
 *
 * Return:
 * Pointer to string behind any leading slashes.
 */
static const char *skip_dir_delim(const char *pattern)
{
	while (*pattern == '/')
		pattern++;

	return pattern;
}

/**
 * wildcard_match() - Check if a filename matches the pattern
 * @filename: Filename to check
 * @pattern:  Pattern to check for
 *
 * Check if the filename matches the pattern. The pattern may contain '?'
 * which matches any single character and '*' which matches any sequence of
 * zero or more characters. The function is called recursively to resolve
 * the '*' wildcards.
 *
 * Return:
 * Pointer to pattern behind the matching part. This either points to the
 * end of the pattern ('\0') or to the next directory part ('/'). Return
 * NULL if no match.
 */
static const char *wildcard_match(const char *filename, const char *pattern)
{
	char p;
	char f;

	do {
		p = *pattern++;
		if (p == '/')
			p = 0;
		f = *filename;
		if (f != p) {
			switch (p) {
			case '*':	  /* matches 0 or more characters */
				do {
					const char *end;

					end = wildcard_match(filename, pattern);
					if (end)
						return end;
				} while (*filename++);
				return NULL;
			case '?':	  /* matches any character but 0 */
				if (f)
					break;
				/* Fall through to default */
			default:	  /* mismatch */
				return NULL;
			}
		}
		filename++;
	} while (f);

	return pattern - 1;
}

/**
 * wildcard_find_file() - Return next file match for a pattern
 * @wdi:    Directory entry; wdi->dir_pattern is the pattern to search for
 * @wfi:    Structure where to store file information if a file is found
 *
 * Return the next match for a file.
 *
 * Return:
 *  1 - Found a file match;
 *  0 - No more matches;
 * -1 - Error, e.g. while reading data from the device
 */
static int wildcard_find_file(struct wc_dirinfo *wdi, struct wc_fileinfo *wfi)
{
	int ret;
	const char *pattern;

	while ((ret = fs_ops->get_fileinfo(wdi, wfi)) > 0) {
		/* Dummy directories . and .. should never match */
		if (!strcmp(wfi->file_name, ".")
		    || !strcmp(wfi->file_name, ".."))
			continue;

		/* Try the pattern */
		pattern = wildcard_match(wfi->file_name, wdi->dir_pattern);
		if (pattern) {
			wfi->pattern = pattern;
			break;
		}
	}

	return ret;
}

/**
 * wildcard_find_dir() - Return next directory match for a pattern
 * @wdi:    Directory entry; wdi->dir_pattern is the pattern to search for
 * @wfi:    Structure where to store file information if a directory is found
 *
 * Return the next match for a directory. This is similar to
 * wildcard_find_file() but only returns directory matches.
 *
 * Return:
 *  1 - Found a directory match;
 *  0 - No more matches;
 * -1 - Error, e.g. while reading data from the device
 */
static int wildcard_find_dir(struct wc_dirinfo *wdi, struct wc_fileinfo *wfi)
{
	int ret;

	/* FIXME: Symlinks may also point to directories */
	do {
		ret = wildcard_find_file(wdi, wfi);
	} while ((ret > 0) && (wfi->file_type != WC_TYPE_DIRECTORY));

	return ret;
}

/**
 * wildcard_is_unique_file() - Check if file pattern has only one match
 * @wdi:    Directory entry; wdi->dir_pattern is the pattern to search for
 * @wfi:    Structure to store temporary information; ignore after return
 *
 * Check the first or the first two file matches of the given directory
 * to decide if the match is unique or not.
 *
 * Return:
 *  2 - Not unique, at least two files match;
 *  1 - A file exists and is the only match;
 *  0 - There is no such file;
 * <0 - Error
 */
static int wildcard_is_unique_file(struct wc_dirinfo *wdi,
				  struct wc_fileinfo *wfi)
{
	int ret = wildcard_find_file(wdi, wfi);

	if (ret > 0) {
		ret = wildcard_find_file(wdi, wfi);
		if (ret >= 0) {
			ret++;
		}
	}
	wdi->flags |= WC_FLAGS_REWIND;

	return ret;
}

/**
 * wildcard_is_unique_dir() - Check if directory pattern has only one match
 * @wdi:    Directory entry; wdi->dir_pattern is the pattern to search for
 * @wfi:    Structure to store temporary information; ignore after return
 *
 * Check the first or the first two directory matches of the given directory
 * to decide if the match is unique or not. This is similar to
 * wildcard_is_unique_file(), but only checks for directories.
 *
 * Return:
 *  2 - Not unique, at least two directories match;
 *  1 - Directory exists and is the only match;
 *  0 - There is no such directory or several directories match;
 * <0 - Error
 */
static int wildcard_is_unique_dir(struct wc_dirinfo *wdi,
				  struct wc_fileinfo *wfi)
{
	int ret = wildcard_find_dir(wdi, wfi);

	if (ret > 0) {
		ret = wildcard_find_dir(wdi, wfi);
		if (ret >= 0)
			ret++;
	}
	wdi->flags |= WC_FLAGS_REWIND;

	return ret;
}

/**
 * wildcard_print_path() - Print directory path
 * @wdi:    Pointer to directory entry
 *
 * Recursively print the given directory path; the path always ends with '/'.
 *
 * The parameter points to the deepest directory, all parent directories can
 * be reached by the parent link in the wc_dirinfo structure:
 *
 *        parent        parent       parent       parent
 *   NULL <----- [root] <----- [dir] <----- [dir] <----- [dir] <--- wdi
 *                '\0'     /   "abc"    /   "def"    /   "ghi"   /
 *
 * The result in this example would be:  /abc/def/ghi/
 */
static void wildcard_print_path(struct wc_dirinfo *wdi)
{
	if (wdi->parent)
		wildcard_print_path(wdi->parent);
	puts(wdi->dir_name);
	putc('/');
}

/**
 * wildcard_print_pathfile() - Print directory path and file name
 * @wdi:    Pointer to directory entry
 * @wfi:    Pointer to file information
 *
 * Recursively print the given directory path and append the file name.
 *
 * The parameter points to the deepest directory, all parent directories can
 * be reached by the parent link in the wc_dirinfo structure:
 *
 *        parent        parent       parent       parent
 *   NULL <----- [root] <----- [dir] <----- [dir] <----- [dir] <--- wdi
 *                '\0'     /   "abc"    /   "def"    /   "ghi"   /
 *
 *   "basename.ext" <--- wfi
 *
 * The result in this example would be:  /abc/def/ghi/basename.ext
 */
static void wildcard_print_pathfile(struct wc_dirinfo *wdi,
				    struct wc_fileinfo *wfi)
{
	wildcard_print_path(wdi);
	puts(wfi->file_name);
}

/**
 * wildcard_free_dir() - Free directory entry
 * @wdi:    Pointer to current directory
 *
 * Free the given directory entry and return a pointer to the parent.
 *
 * Return:
 * Pointer to parent directory, NULL if this was the root directory.
 */
static struct wc_dirinfo *wildcard_free_dir(struct wc_dirinfo *wdi)
{
	struct wc_dirinfo *parent_wdi = wdi->parent;

	fs_ops->free_dir(wdi);

	return parent_wdi;
}

/**
 * wildcard_path_done() - Free the given directory path
 * @wdi:    Pointer to directory path
 *
 * Free the given directory path and return NULL.
 *
 * Return:
 * NULL
 */
static struct wc_dirinfo *wildcard_path_done(struct wc_dirinfo *wdi)
{
	while (wdi)
		wdi = wildcard_free_dir(wdi);

	return wdi;
}

/**
 * wildcard_alloc_dir() - Allocate directory entry
 * @parent_wdi: Pointer to directory path where a new entry will be appended
 * @wfi:        File information for subdirectory
 *
 * Allocate a directory entry, link it to the given parent directory and fill
 * in the given file data. If the entry can not be allocated, show an
 * allocation error, free whole path and return NULL.
 *
 * Return:
 * Pointer to the new directory entry or NULL on error; in the latter case,
 * the whole path is already deallocated on return.
 */
static struct wc_dirinfo *wildcard_alloc_dir(struct wc_dirinfo *parent_wdi,
					     struct wc_fileinfo *wfi)
{
	struct wc_dirinfo *wdi;

	wdi = fs_ops->alloc_dir();
	if (wdi) {
		wdi->parent = parent_wdi;
		wdi->reference = wfi->reference;
		wdi->flags = WC_FLAGS_REWIND | WC_FLAGS_RELOAD;
		wdi->dir_pattern = skip_dir_delim(wfi->pattern);
		strcpy(wdi->dir_name, wfi->file_name);
	} else {
		if (parent_wdi) {
			wildcard_print_path(parent_wdi);
			wildcard_path_done(parent_wdi);
			puts(": ");
		}
		printf("Allocation error on \"%s\"\n",
		       skip_dir_delim(wfi->file_name));
	}
	return wdi;
}

/**
 * wildcard_path_error() - Show error message and free directory path
 * @wdi:    Pointer to directory path
 * @reason: Error reason (NULL: use fix string "I/O error on")
 *
 * Show the path, the error reason and the remaining pattern. Then free the
 * given directory path and return NULL.
 *
 * Return:
 * NULL
 */
static struct wc_dirinfo *wildcard_path_error(struct wc_dirinfo *wdi,
					      const char *reason)
{
	if (!reason)
		reason = "I/O error on";
	wildcard_print_path(wdi);
	printf(": %s \"%s\"\n", reason, wdi->dir_pattern);

	return wildcard_path_done(wdi);
}

/**
 * wildcard_list_dir() - List the given directory in ls style
 * @wdi:    Directory path to list; wdi->dir_pattern is the pattern to show
 * @wfi:    Structure to store temporary information; ignore after return
 *
 * List the files of the given directory. If the wdi->dir_pattern is empty,
 * list all files (including "." and ".."), otherwise only list matching files.
 *
 * Remark: Files are not sorted in any way. They are shown in the sequence as
 * they appear in the directory.
 *
 * Return:
 * -1 on error, 0 for success.
 */
static int wildcard_list_dir(struct wc_dirinfo *wdi, struct wc_fileinfo *wfi)
{
	int ret;
	int (*get_fileinfo)(struct wc_dirinfo *wdi, struct wc_fileinfo *wfi);
	char symlink[WC_SYMLINK_MAX];

	wdi->flags |= WC_FLAGS_REWIND;
	wildcard_print_path(wdi);
	puts(":\n");

	if (wdi->dir_pattern[0])
		get_fileinfo = wildcard_find_file;
	else
		get_fileinfo = fs_ops->get_fileinfo;


	while ((ret = get_fileinfo(wdi, wfi)) > 0) {
		switch (wfi->file_type) {
		  case WC_TYPE_NONE:
			  /* Should not happen */
			  break;
		  case WC_TYPE_DIRECTORY:
			  printf(" <DIR>       %s/\n", wfi->file_name);
			  break;
		  case WC_TYPE_VOLUME:
			  printf(" <VOLUME>    %s\n", wfi->file_name);
			  break;
		  case WC_TYPE_SYMLINK:
			  if (fs_ops->get_symlink)
				  fs_ops->get_symlink(wdi, wfi, symlink);
			  else
				  strcpy(symlink, "???");
			  printf(" <SYMLINK>   %s -> %s\n", wfi->file_name,
				 symlink);
			  break;
		  case WC_TYPE_REGULAR:
			  printf("%11llu  %s\n", wfi->file_size,
				 wfi->file_name);
			  break;
		}
	}

	if (ret < 0) {
		wildcard_path_error(wdi, NULL);
		return -1;
	}

	putc('\n');

	return 0;
}

/**
 * wildcard_find_unique() - Check if path and file name exist and are unique
 * @wfi:    Info for root directory and pattern for file; will be updated
 *
 * Check if the path and file name exist and are unique. Return a pointer to
 * the directory path and the fileinfo in *wfi. If the file does not exist,
 * wfi->file_type is set to WC_TYPE_NONE, wfi->file_name is empty and
 * wfi->pattern points to the file name pattern to use. If it exists, all *wfi
 * entries show valid data and wfi->pattern should be empty. Please note that
 * the file may be of any type, even a directory.
 *
 * After you are done, you must call wildcard_path_done() with the pointer
 * that was returned to free the directory path.
 *
 * On errors (path or file is ambiguous, I/O errors, etc) the function prints
 * an appropriate error message (including path and remaining pattern) and
 * returns NULL. Then *wfi is not valid.
 *
 * Return:
 * Pointer to directory path where file is found, NULL if no match.
 */
static struct wc_dirinfo *wildcard_find_unique(struct wc_fileinfo *wfi)
{
	struct wc_dirinfo *wdi = NULL;
	int ret;
	const char *reason;

	do {
		wdi = wildcard_alloc_dir(wdi, wfi);
		if (!wdi)
			return wdi;

		/* If the pattern is empty, then the file name is missing */
		if (wdi->dir_pattern[0] == '\0') {
			wildcard_print_path(wdi);
			puts(": Missing file name\n");
			return wildcard_path_done(wdi);
		}

		if (!strchr(wdi->dir_pattern, '/')) {
			/* No more directory delimiter in the path. So the
			   path exists and is unique, now check file name.
			   Here all file types are allowed to match. */
			ret = wildcard_is_unique_file(wdi, wfi);
			if (ret == 0) {
				/* Final file does not exist; return the path
				   and an empty fileinfo with the pattern of
				   the file name */
				wfi->file_type = WC_TYPE_NONE;
				wfi->file_name[0] = '\0';
				wfi->pattern = wdi->dir_pattern;
				wfi->reference = 0;
				return wdi;
			}
			if (ret == 1) {
				/* If we have a unique file, return path and
				   fileinfo (successful unique match) */
				ret = wildcard_find_file(wdi, wfi);
				if (ret == 1)
					return wdi;
			}
		} else {
			/* Pattern contains a directory delimiter '/', so next
			   part of the path must match a directory */
			ret = wildcard_is_unique_dir(wdi, wfi);
			if (ret == 1)
				ret = wildcard_find_dir(wdi, wfi);
		}
	} while (ret == 1);

	if (ret < 0)
		reason = NULL;
	else if (ret == 0)
		reason = "No match for";
	else
		reason = "Ambiguous matches for";

	return wildcard_path_error(wdi, reason);
}


/**
 * wildcard_ls() - List files
 * @wfi:      Info for root directory and pattern for files to list
 * @ops:      Pointer to the filesystem functions doing the data access
 *
 * Recursively list the files in the given root directory and all
 * subdirectories that match the pattern in wfi->pattern.
 *
 * Return:
 * -1 on errors, 0 for success.
 */
int wildcard_ls(struct wc_fileinfo *wfi, const struct wc_fsops *ops)
{
	struct wc_dirinfo *wdi = NULL;
	int ret;

	fs_ops = ops;

	for (;;) {
		/* Go down path into subdirectories */
		do {
			wdi = wildcard_alloc_dir(wdi, wfi);
			if (!wdi)
				return -1;

			/* If the remaining pattern is empty, just do a full
			   listing of this directory */
			if (wdi->dir_pattern[0] == '\0') {
				ret = 0;
				wildcard_list_dir(wdi, wfi);
				break;
			}

			if (!strchr(wdi->dir_pattern, '/')) {
				/* We are at the final part of the path, the
				   file name. If we match exactly one file and
				   if this file is a directory, open it as
				   subdirectory and do a full listing in the
				   next loop cycle. Otherwise list all
				   matching files. */
				ret = wildcard_is_unique_file(wdi, wfi);
				if (ret == 1) {
					ret = wildcard_find_dir(wdi, wfi);
				}
				if ((ret == 0) || (ret > 1)) {
					/* Zero matches or more than one
					   match or no directory */
					wildcard_list_dir(wdi, wfi);
					break;
				}
			} else {
				/* Pattern contains a directory delimiter '/',
				   so next part of the path must match a
				   directory */
				ret = wildcard_find_dir(wdi, wfi);
			}
		} while (ret > 0);

		/* No more matches or error */
		if (ret >= 0) {
			/* Go up path towards root directory */
			do {
				wdi = wildcard_free_dir(wdi);
				if (!wdi)
					return 0; /* Done */
				wdi->flags |= WC_FLAGS_RELOAD;
				ret = wildcard_find_dir(wdi, wfi);
			} while (!ret);
		}
	} while (ret >= 0);

	/* Show I/O error */
	wildcard_path_error(wdi, NULL);
	return -1;
}

/**
 * wildcard_read_at() - Read a file.
 * @wfi:      Info for root directory and pattern for files to search for
 * @ops:      Pointer to the filesystem functions doing the data access
 * @buffer:   Pointer to buffer with data to write
 * @offset:   Starting position in file
 * @len:      Number of bytes to write
 * @actread:  Pointer where to store the number of actually read bytes
 *
 * If there is exactly one file matchin the pattern, load the data from it by
 * calling the filesystem specific read_file_at() function. Special files,
 * e.g. the volume name or directories can not be read.
 *
 * Return:
 * Error code (<0) or 0 for success.
 */
int wildcard_read_at(struct wc_fileinfo *wfi, const struct wc_fsops *ops,
		     void *buffer, loff_t offset, loff_t len, loff_t *actread)
{
	struct wc_dirinfo *wdi;
	int err;

	if (actread)
		*actread = 0;

	fs_ops = ops;

	/* Find path and file */
	wdi = wildcard_find_unique(wfi);
	if (!wdi)
		return -ENOENT;

	if (wfi->file_type == WC_TYPE_NONE) {
		wildcard_path_error(wdi, "No file matches");
		return -ENOENT;
	}

	if (wfi->file_type == WC_TYPE_REGULAR) {
		/* Load file, print info only if buffer is not NULL */
		if (buffer) {
			puts("Loading ");
			wildcard_print_pathfile(wdi, wfi);
			puts(" ... ");
		}

		err = ops->read_file_at(wdi, wfi, buffer, offset, len, actread);
		if (buffer) {
			if (err < 0)
				puts("failed!\n");
			else
				puts("done!\n");
		}
	} else {
		wildcard_print_pathfile(wdi, wfi);
		puts(" is no regular file\n");
		err = -EINVAL;
	}

	wildcard_path_done(wdi);

	return err;
}

/**
 * wildcard_write() - Write a file
 * @wfi:      Info for root directory and pattern for files to search for
 * @ops:      Pointer to the filesystem functions doing the data access
 * @buffer:   Pointer to buffer with data to write
 * @offset:   Starting position in file
 * @len:      Number of bytes to write
 * @actwrite: Pointer where to store number of actually written bytes
 *
 * If there are several files matching the pattern or if no file matches, but
 * the pattern contains wildcards (* or ?), return an error. Otherwise use the
 * resulting filename and write the data to it by calling the filesystem
 * specific write_file() function. Special files, e.g. the volume name or
 * directories can not be written to.
 *
 * Return:
 * Error code (<0) or 0 for success.
 */
int wildcard_write(struct wc_fileinfo *wfi, const struct wc_fsops *ops,
		   const void *buffer, loff_t offset, loff_t len,
		   loff_t *actwrite)
{
	struct wc_dirinfo *wdi;
	int err = 0;

	if (actwrite)
		*actwrite = 0;

	fs_ops = ops;

	if (!ops->write_file) {
		printf("No write support for %s\n", ops->name);
		return -EROFS;
	}

	/* Find path and file */
	wdi = wildcard_find_unique(wfi);
	if (!wdi)
		return -ENOENT;

	/* If filename contains wildcards, we must have a match */
	if ((wfi->file_type == WC_TYPE_NONE)
	    && (strchr(wfi->pattern, '*') || strchr(wfi->pattern, '?'))) {
		wildcard_path_error(wdi, "No file matches");
		return -ENOENT;
	}

	if ((wfi->file_type == WC_TYPE_REGULAR)
	    || (wfi->file_type == WC_TYPE_NONE)) {
		/* Save file */
		puts("Saving ");
		wildcard_print_pathfile(wdi, wfi);
		puts(" ... ");

		err = ops->write_file(wdi, wfi, buffer, offset, len, actwrite);
		if (err < 0)
			puts("failed!\n");
		else
			puts("done!\n");
	} else {
		wildcard_print_pathfile(wdi, wfi);
		puts(" is no regular file\n");
		err = -EINVAL;
	}

	wildcard_path_done(wdi);

	return err;
}

/**
 * wildcard_exists() - Check for existence of a file.
 * @wfi:      Pointer to fileinfo of root directory, searches for wfi->pattern
 * @ops:      Pointer to the filesystem functions doing the data access
 *
 * Check if a file exists that matches the path and file name pattern.
 *
 * Return:
 * 1 if file exists, 0 if it does not exist or in case of I/O errors.
 */
int wildcard_exists(struct wc_fileinfo *wfi, const struct wc_fsops *ops)
{
	struct wc_dirinfo *wdi;

	fs_ops = ops;

	/* Find path and file */
	wdi = wildcard_find_unique(wfi);
	if (wdi && (wfi->file_type != WC_TYPE_NONE))
		return 1;

	return 0;
}
