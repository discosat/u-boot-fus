/*
 * (C) Copyright 2000-2010
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2008
 * Stuart Wood, Lab X Technologies <stuart.wood@labxtechnologies.com>
 *
 * (C) Copyright 2004
 * Jian Zhang, Texas Instruments, jzhang@ti.com.
 *
 * (C) Copyright 2001 Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Andreas Heppel <aheppel@sysgo.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <environment.h>
#include <linux/stddef.h>
#include <malloc.h>
#include <memalign.h>
#include <nand.h>
#include <search.h>
#include <errno.h>

#if defined(CONFIG_CMD_SAVEENV) && defined(CONFIG_CMD_NAND) && \
		!defined(CONFIG_SPL_BUILD)
#define CMD_SAVEENV
#elif defined(CONFIG_ENV_OFFSET_REDUND)
#error CONFIG_ENV_OFFSET_REDUND must have CONFIG_CMD_SAVEENV & CONFIG_CMD_NAND
#endif

#if defined(CONFIG_ENV_SIZE_REDUND) &&	\
	(CONFIG_ENV_SIZE_REDUND != CONFIG_ENV_SIZE)
#error CONFIG_ENV_SIZE_REDUND should be the same as CONFIG_ENV_SIZE
#endif

#ifdef CONFIG_NAND_ENV_DST
static env_t *get_env_ptr(void)
{
	return  (env_t *)CONFIG_NAND_ENV_DST;
}

static env_t *get_redundand_env_ptr(void)
{
	return (env_t *)(CONFIG_NAND_ENV_DST + get_env_size());
}
#endif

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_ENV_SIZE
inline size_t get_env_range(void)
{
#ifdef CONFIG_ENV_RANGE
	return CONFIG_ENV_RANGE;
#else
	return CONFIG_ENV_SIZE;
#endif
}

static inline size_t get_env_size(void)
{
	return CONFIG_ENV_SIZE;
}
#else
extern size_t get_env_range(void);
extern size_t get_env_size(void);
#endif

#ifdef CONFIG_ENV_OFFSET
inline size_t get_env_offset(void)
{
	return CONFIG_ENV_OFFSET;
}

#ifdef CONFIG_ENV_OFFSET_REDUND
inline size_t __get_env_offset_redund(void)
{
	return CONFIG_ENV_OFFSET_REDUND;
}
#endif
#else
extern size_t get_env_offset(void);
#ifdef CONFIG_ENV_OFFSET_REDUND
extern size_t get_env_offset_redund(void);
#endif
#endif

uchar env_get_char_spec(int index)
{
	return *((uchar *)(gd->env_addr + index));
}

/*
 * This is called before nand_init() so we can't read NAND to
 * validate env data.
 *
 * Mark it OK for now. env_relocate() in env_common.c will call our
 * relocate function which does the real validation.
 *
 * When using a NAND boot image (like sequoia_nand), the environment
 * can be embedded or attached to the U-Boot image in NAND flash.
 * This way the SPL loads not only the U-Boot image from NAND but
 * also the environment.
 */
static int env_nand_init(void)
{
#if defined(ENV_IS_EMBEDDED) || defined(CONFIG_NAND_ENV_DST)
	size_t env_size = get_env_size();
	env_t *env1 = get_env_ptr();
	int crc1_ok;

	gd->env_size = env_size;
	env_size -= ENV_HEADER_SIZE;
	crc1_ok = (crc32(0, (u_char *)(env1 + 1), env_size) == env1->crc);

#ifdef CONFIG_ENV_OFFSET_REDUND
	{
		env_t *env2 = get_redundand_env_ptr();
		int crc2_ok =
		       (crc32(0, (u_char *)(env2 + 1), env_size) == env2->crc);

		if (crc1_ok && crc2_ok) {
			/* both ok - check serial */
			if ((env1->flags < env2->flags)
			    || (env1->flags == 255 && env2->flags == 0)) {
				/* env2 is newer */
				gd->env_valid = ENV_REDUND;
				gd->env_addr = env2;
			} else {
				/* env1 is newer (or same age) */
				gd->env_valid = ENV_VALID;
				gd->env_addr = env1;
			}
			return 0;
		} else if (crc2_ok) {
			gd->env_valid = ENV_REDUND;
			gd->env_addr = env2;
			return 0;
		}
	}
#endif
	if (crc1_ok) {
		gd->env_valid = ENV_VALID;
		gd->env_addr = env1;
	} else {
		gd->env_valid = ENV_INVALID;
		gd->env_addr = NULL;
	}

#else /* ENV_IS_EMBEDDED || CONFIG_NAND_ENV_DST */
	gd->env_addr	= (ulong)&default_environment[0];
	gd->env_valid	= ENV_VALID;
	gd->env_size	= get_default_env_size();
#endif /* ENV_IS_EMBEDDED || CONFIG_NAND_ENV_DST */

	return 0;
}

#ifdef CMD_SAVEENV
struct nand_env_location {
	const char *name;
	size_t env_size;
	nand_erase_options_t erase_opts;
};

/*
 * The legacy NAND code saved the environment in the first NAND device i.e.,
 * nand_dev_desc + 0. This is also the behaviour using the new NAND code.
 */
static int writeenv(const struct nand_env_location *location, u_char *buf)
{
	size_t offset = location->erase_opts.offset;
	size_t end = offset + location->erase_opts.length;
	size_t env_size = location->env_size;
	size_t amount_saved = 0;
	size_t blocksize, len;
	struct mtd_info *mtd;

	mtd = get_nand_dev_by_index(0);
	if (!mtd)
		return 1;

	blocksize = mtd->erasesize;
	len = min(blocksize, env_size);

	while ((amount_saved < env_size) && (offset < end)) {
		if (nand_block_isbad(mtd, offset)) {
			offset += blocksize;
		} else {
			if (nand_write(mtd, offset, &len, buf))
				return 1;

			offset += blocksize;
			buf += len;
			amount_saved += len;
		}
	}
	if (amount_saved != env_size)
		return 1;

	return 0;
}

static int erase_and_write_env(const struct nand_env_location *location,
			       u_char *env_new)
{
	struct mtd_info *mtd;
	int ret;

	mtd = get_nand_dev_by_index(0);
	if (!mtd)
		return 1;

	printf("Erasing %s... ", location->name);
	ret = nand_erase_opts(mtd, &location->erase_opts);
	if (!ret) {
		printf("OK\n"
		       "Writing to %s... ", location->name);
		ret = writeenv(location, env_new);
	}
	puts(ret ? "FAILED!\n" : "OK\n");

	return ret;
}

static int env_nand_save(void)
{
	int	ret = 0;
	env_t *env_new;
	int env_idx = 0;
	size_t env_size = get_env_size();
	size_t env_range = get_env_range();
	static struct nand_env_location location[] = {
		{
			.name = "NAND",
		},
#ifdef CONFIG_ENV_OFFSET_REDUND
		{
			.name = "redundant NAND",
		},
#endif
	};

	if (env_range < env_size)
		return 1;

	env_new = (env_t *)malloc_cache_aligned(env_size);
	if (!env_new) {
		printf("Cannot export environment: malloc() failed\n");
		return -1;
	}

	ret = env_export(env_new);
	if (ret)
		return ret;

	location[0].env_size = env_size;
	location[0].erase_opts.length = env_range;
	location[0].erase_opts.offset = get_env_offset();

#ifdef CONFIG_ENV_OFFSET_REDUND
	location[1].env_size = env_size;
	location[1].erase_opts-length = env_range;
	location[1].erase_opts.offset = get_env_offset_redund();
	env_new->flags = ++env_flags; /* increase the serial */
	env_idx = (gd->env_valid == ENV_VALID);
#endif

	ret = erase_and_write_env(&location[env_idx], (u_char *)env_new);

#ifdef CONFIG_ENV_OFFSET_REDUND
	if (!ret) {
		/* preset other copy for next write */
		if (gd->env_valid == ENV_REDUND)
			gd->env_valid = ENV_VALID;
		else
			gd->env_valid = ENV_REDUND;
		goto DONE;
	}

	env_idx = (env_idx + 1) & 1;
	ret = erase_and_write_env(&location[env_idx], (u_char *)env_new);
	if (!ret)
		printf("Warning: primary env write failed,"
				" redundancy is lost!\n");
DONE:
#endif

	free(env_new);

	return ret;
}
#endif /* CMD_SAVEENV */

#if defined(CONFIG_SPL_BUILD)
static int readenv(size_t offset, u_char *buf, size_t env_size)
{
	return nand_spl_load_image(offset, env_size, buf);
}
#else
static int readenv(size_t offset, u_char *buf, size_t env_size)
{
	size_t end = offset + get_env_range();
	size_t amount_loaded = 0;
	size_t blocksize, len;
	struct mtd_info *mtd;
	u_char *char_ptr;

	mtd = get_nand_dev_by_index(0);
	if (!mtd)
		return 1;

	blocksize = mtd->erasesize;

	while (amount_loaded < env_size && offset < end) {
		len = min(blocksize, env_size - amount_loaded);
		if (!nand_block_isbad(mtd, offset)) {
			char_ptr = &buf[amount_loaded];
			if (nand_read_skip_bad(mtd, offset,
					       &len, NULL,
					       mtd->size, char_ptr))
				return 1;

			amount_loaded += len;
		}
		offset += blocksize;
	}

	if (amount_loaded != env_size)
		return 1;

	return 0;
}
#endif /* #if defined(CONFIG_SPL_BUILD) */

#ifdef CONFIG_ENV_OFFSET_OOB
int get_nand_env_oob(struct mtd_info *mtd, unsigned long *result)
{
	struct mtd_oob_ops ops;
	uint32_t oob_buf[ENV_OFFSET_SIZE / sizeof(uint32_t)];
	int ret;

	ops.datbuf	= NULL;
	ops.mode	= MTD_OOB_AUTO;
	ops.ooboffs	= 0;
	ops.ooblen	= ENV_OFFSET_SIZE;
	ops.oobbuf	= (void *)oob_buf;

	ret = mtd->read_oob(mtd, ENV_OFFSET_SIZE, &ops);
	if (ret) {
		printf("error reading OOB block 0\n");
		return ret;
	}

	if (oob_buf[0] == ENV_OOB_MARKER) {
		*result = oob_buf[1] * mtd->erasesize;
	} else if (oob_buf[0] == ENV_OOB_MARKER_OLD) {
		*result = oob_buf[1];
	} else {
		printf("No dynamic environment marker in OOB block 0\n");
		return -ENOENT;
	}

	return 0;
}
#endif

#ifdef CONFIG_ENV_OFFSET_REDUND
static int env_nand_load(void)
{
#if defined(ENV_IS_EMBEDDED)
	return 0;
#else
	int read1_fail = 0, read2_fail = 0;
	env_t *env1, *env2;
	env_size = get_env_size();
	int ret = 0;

	env1 = (env_t *)malloc_cache_aligned(env_size);
	env2 = (env_t *)malloc_cache_aligned(env_size);
	if (env1 == NULL || env2 == NULL) {
		puts("Can't allocate buffers for environment\n");
		set_default_env("!malloc() failed");
		ret = -EIO;
		goto done;
	}

	read1_fail = readenv(get_env_offset(), (u_char *)env1, env_size);
	read2_fail = readenv(get_env_offset_redund(), (u_char *)env2, env_size);

	gd->env_size = env_size;
	env_size -= ENV_HEADER_SIZE;

	if (read1_fail && read2_fail)
		puts("*** Error - No Valid Environment Area found\n");
	else if (read1_fail || read2_fail)
		puts("*** Warning - some problems detected "
		     "reading environment; recovered successfully\n");

	if (read1_fail && read2_fail) {
		set_default_env("!bad env area");
		goto done;
	} else if (!read1_fail && read2_fail) {
		gd->env_valid = ENV_VALID;
		env_import((char *)tmp_env1, 1);
	} else if (read1_fail && !read2_fail) {
		gd->env_valid = ENV_REDUND;
		env_import((char *)tmp_env2, 1);
	} else {
		env_import_redund((char *)tmp_env1, (char *)tmp_env2);
	}

done:
	free(env1);
	free(env2);

	return ret;
#endif /* ! ENV_IS_EMBEDDED */
}
#else /* ! CONFIG_ENV_OFFSET_REDUND */
/*
 * The legacy NAND code saved the environment in the first NAND
 * device i.e., nand_dev_desc + 0. This is also the behaviour using
 * the new NAND code.
 */
static int env_nand_load(void)
{
#if !defined(ENV_IS_EMBEDDED)
	char *buf;
	size_t env_size;

#if defined(CONFIG_ENV_OFFSET_OOB)
	struct mtd_info *mtd  = get_nand_dev_by_index(0);
	/*
	 * If unable to read environment offset from NAND OOB then fall through
	 * to the normal environment reading code below
	 */
	if (mtd && !get_nand_env_oob(mtd, &nand_env_oob_offset)) {
		printf("Found Environment offset in OOB..\n");
	} else {
		set_default_env("!no env offset in OOB");
		return;
	}
#endif

	env_size = get_env_size();
	buf = malloc_cache_aligned(env_size);
	if (!buf || readenv(get_env_offset(), (u_char *)buf, env_size)) {
		set_default_env("!readenv() failed");
		free(buf);
		return -EIO;
	}

	env_import(buf, 1, env_size);
	free(buf);
#endif /* ! ENV_IS_EMBEDDED */

	return 0;
}
#endif /* CONFIG_ENV_OFFSET_REDUND */

U_BOOT_ENV_LOCATION(nand) = {
	.location	= ENVL_NAND,
	ENV_NAME("NAND")
	.load		= env_nand_load,
#if defined(CMD_SAVEENV)
	.save		= env_save_ptr(env_nand_save),
#endif
	.init		= env_nand_init,
};
