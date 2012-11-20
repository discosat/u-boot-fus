/*
 * wildcard.c
 *
 * Generic commands to read and write a file and to list directories. These
 * commands support wildcards in file/path names.
 *
 * (C) Copyright 2012 Hartmut Keller (keller@fs-net.de)
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <config.h>
#include <wildcard.h>
#include <fat.h>

/* Pointer to file system calls for data access */
static const struct wc_filesystem_ops *fs_ops;

/*
 * Remove all leading directory delimiters '/' from the pattern.
 *
 * Parameters:
 *   pattern: Pointer to string to check
 *
 * Return:
 *   Pointer to string behind any leading slashes
 */
static const char *skip_dir_delim(const char *pattern)
{
	while (*pattern == '/')
		pattern++;

	return pattern;
}

/* 
 * Check if the filename matches the pattern. The pattern may contain '?'
 * which matches any single character and '*' which matches any sequence of
 * zero or more characters. The function is called recursively to resolve
 * the '*' wildcards.
 *
 * Parameters:
 *   filename: Pointer to filename to check
 *   pattern:  Pointer to pattern to check for
 *
 * Return:
 *   Pointer to pattern behind the matching part. This either points to the
 *   end of the pattern ('\0') or to the next directory part ('/'). Return
 *   NULL if no match.
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

/*
 * Return the next match for a file.
 *
 * Parameters:
 *   wdi: Pointer to directory entry; especially wdi->dir_pattern is the
 *        pattern to search for
 *   wfi: Pointer to structure where to store file information if found
 *
 * Return:
 *   1:  Found a file match
 *   0:  No more matches
 *   -1: Error, e.g. while reading data from the device
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

/*
 * Return the next match for a directory. This is similar to
 * wildcard_find_file() but only returns directory matches.
 *
 * Parameters:
 *   wdi: Pointer to directory entry; especially wdi->dir_pattern is the
 *        pattern to search for
 *   wfi: Pointer to structure where to store file information if found
 *
 * Return:
 *   1:  Found a directory match
 *   0:  No more matches
 *   -1: Error, e.g. while reading data from the device
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

/*
 * Check the first or the first two file matches of the given directory
 * to decide if the match is unique or not.
 *
 * Parameters:
 *   wdi: Pointer to directory entry; especially wdi->dir_pattern is the
 *        pattern to search for
 *   wfi: Pointer to structure where to store intermediate file information;
 *        it does not contain meaningful data after return!
 *
 * Return:
 *   2:  Not unique, at least two files match
 *   1:  A file exists and is the only match
 *   0:  There is no such file
 *   <0: Error
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

/*
 * Check the first or the first two directory matches of the given directory
 * to decide if the match is unique or not. This is similar to
 * wildcard_is_unique_file(), but only checks for directories.
 *
 * Parameters:
 *   wdi: Pointer to directory entry; especially wdi->dir_pattern is the
 *        pattern to search for
 *   wfi: Pointer to structure where to store intermediate file information;
 *        it does not contain meaningful data after return!
 *
 * Return:
 *   2:  Not unique, at least two directories match
 *   1:  Directory exists and is the only match
 *   0:  There is no such directory or several directories match
 *   <0: Error
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

/*
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
 *
 * Parameter:
 *   wdi: Pointer to directory entry
 */
static void wildcard_print_path(struct wc_dirinfo *wdi)
{
	if (wdi->parent)
		wildcard_print_path(wdi->parent);
	puts(wdi->dir_name);
	putc('/');
}

/*
 * Recursively print the given directory path; the path always ends with '/'.
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
 *
 * Parameter:
 *   wdi: Pointer to directory entry
 *   wfi: Pointer to file information
 */
static void wildcard_print_pathfile(struct wc_dirinfo *wdi,
				    struct wc_fileinfo *wfi)
{
	wildcard_print_path(wdi);
	puts(wfi->file_name);
}

/*
 * Free the given directory entry and return a pointer to the parent.
 *
 * Parameter:
 *   wdi: Pointer to current directory
 *
 * Return:
 *   Pointer to parent directory, NULL if this was the root directory
 */
static struct wc_dirinfo *wildcard_free_dir(struct wc_dirinfo *wdi)
{
	struct wc_dirinfo *parent_wdi = wdi->parent;

	fs_ops->free_dir(wdi);

	return parent_wdi;
}

/*
 * Free the given directory path and return NULL.
 *
 * Parameter:
 *   wdi: Pointer to directory path
 *
 * Return:
 *   NULL
 */
static struct wc_dirinfo *wildcard_path_done(struct wc_dirinfo *wdi)
{
	while (wdi)
		wdi = wildcard_free_dir(wdi);

	return wdi;
}

/*
 * Allocate a directory entry, link it to the given parent directory and fill
 * in the given file data. If the entry can not be allocated, show an
 * allocation error, free whole path and return NULL.
 *
 * Parameters:
 *   parent_wdi: Pointer to directory path where a new entry will be appended
 *   wfi:        File information for subdirectory
 *
 * Return:
 *   Pointer to the new directory entry or NULL on error; in the latter case,
 *   the whole path is already deallocated on return
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

/*
 * Show the path, the error reason and the remaining pattern. Then free the
 * given directory path and return NULL.
 *
 * Parameters:
 *   wdi:    Pointer to directory path
 *   reason: Error reason (NULL: use fix "I/O error on")
 *
 * Return:
 *   NULL
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

/*
 * List the given directory in ls style. If the directory pattern is empty,
 * list all files (including "." and ".."), otherwise only list matching files.
 *
 * Remark: Files are not sorted in any way. They are shown in the sequence as
 * they appear in the directory.
 *
 * Parameters:
 *   wdi: Pointer to directory path to list; especially wdi->dir_pattern is
 *        the pattern to show; if empty, show all files of directory
 *   wfi: Pointer structure where to store file information; it does not
 *        contain meaningful data after return!
 *
 * Return:
 *   0:  No error
 *   -1: Error
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

/* 
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
 * Parameter:
 *   wfi: Info for root directory and pattern for file; this is updated and
 *        contains the info for the found file after return
 *
 * Return:
 *   Pointer to directory path where file is found, or NULL if no match
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


/*
 * Recursively list the files in the given root directory and all
 * subdirectories that match the pattern in wfi->pattern.
 *
 * Parameters:
 *   wfi: Pointer to fileinfo for root directory, wfi->pattern is the pattern
 *        to search for
 *   ops: Pointer to the filesystem functions doing the data access
 *
 * Return:
 *   0:  OK
 *   -1: Error, e.g. while reading data from the device
 */
int wildcard_ls(struct wc_fileinfo *wfi, const struct wc_filesystem_ops *ops)
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

/*
 * Read a file.
 *
 * Parameters:
 *   wfi:     Pointer to fileinfo for root directory, wfi->pattern is the
 *            pattern for the filename to search for
 *   ops:     Pointer to the filesystem functions doing the data access
 *   buffer:  Pointer to buffer where to store data
 *   maxsize: Maximum number of bytes to read (0: whole file)
 *
 * Return:
 *   Number of read bytes or -1 (0xFFFFFFFF) on error, e.g. if path or file is
 *   not found or while reading data from the device
 */
unsigned long wildcard_read(struct wc_fileinfo *wfi,
			    const struct wc_filesystem_ops *ops,
			    void *buffer, unsigned long maxsize)
{
	struct wc_dirinfo *wdi;
	unsigned long loaded_size = (unsigned long)-1;

	fs_ops = ops;

	/* Find path and file */
	wdi = wildcard_find_unique(wfi);
	if (!wdi)
		return loaded_size;

	if (wfi->file_type == WC_TYPE_NONE) {
		wildcard_path_error(wdi, "No file matches");
		return loaded_size;
	}

	if (wfi->file_type == WC_TYPE_REGULAR) {
		/* Load file */
		puts("Loading ");
		wildcard_print_pathfile(wdi, wfi);
		puts(" ... ");

		loaded_size = ops->read_file(wdi, wfi, buffer, maxsize);
		if (loaded_size == (unsigned long)-1)
			printf("failed!\n");
		else
			printf("done!\n");
	} else {
		wildcard_print_pathfile(wdi, wfi);
		puts(" is no regular file\n");
	}

	wildcard_path_done(wdi);

	return loaded_size;
}

/*
 * Write a file.
 *
 * Parameters:
 *   wfi:     Pointer to fileinfo for root directory, wfi->pattern is the
 *            pattern for the filename to search for
 *   ops:     Pointer to the filesystem functions doing the data access
 *   buffer:  Pointer to buffer with data to write
 *   maxsize: Number of bytes to write
 *
 * Return:
 *   Number of written bytes or -1 (0xFFFFFFFF) on error, e.g. if path not
 *   found or while reading data from or writing data to the device
 */
unsigned long wildcard_write(struct wc_fileinfo *wfi,
			     const struct wc_filesystem_ops *ops,
			     void *buffer, unsigned long maxsize)
{
	struct wc_dirinfo *wdi;
	unsigned long saved_size = (unsigned long)-1;

	fs_ops = ops;

	if (!ops->write_file) {
		printf("No write support for %s\n", ops->name);
		return saved_size;
	}

	/* Find path and file */
	wdi = wildcard_find_unique(wfi);
	if (!wdi)
		return saved_size;

	/* If filename contains wildcards, we must have a match */
	if ((wfi->file_type == WC_TYPE_NONE)
	    && (strchr(wfi->pattern, '*') || strchr(wfi->pattern, '?'))) {
		wildcard_path_error(wdi, "No file matches");
		return saved_size;
	}

	if ((wfi->file_type == WC_TYPE_REGULAR)
	    || (wfi->file_type == WC_TYPE_NONE)) {
		/* Save file */
		puts("Saving ");
		wildcard_print_pathfile(wdi, wfi);
		puts(" ... ");

		saved_size = ops->write_file(wdi, wfi, buffer, maxsize);
		if (saved_size == (unsigned long)-1)
			printf("failed!\n");
		else
			printf("done!\n");
	} else {
		wildcard_print_pathfile(wdi, wfi);
		puts(" is no regular file\n");
	}

	wildcard_path_done(wdi);

	return saved_size;
}
