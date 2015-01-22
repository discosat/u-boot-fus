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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <environment.h>
#include <linux/stddef.h>
#include <malloc.h>
#include <nand.h>
#include <search.h>
#include <errno.h>

#if defined(CONFIG_CMD_SAVEENV) && defined(CONFIG_CMD_NAND)
#define CMD_SAVEENV
#elif defined(CONFIG_ENV_OFFSET_REDUND)
#error CONFIG_ENV_OFFSET_REDUND must have CONFIG_CMD_SAVEENV & CONFIG_CMD_NAND
#endif

#if defined(CONFIG_ENV_SIZE_REDUND) &&	\
	(CONFIG_ENV_SIZE_REDUND != CONFIG_ENV_SIZE)
#error CONFIG_ENV_SIZE_REDUND should be the same as CONFIG_ENV_SIZE
#endif

char *env_name_spec = "NAND";

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

#if defined(CONFIG_ENV_RANGE) || defined(CONFIG_ENV_SIZE)
inline size_t __get_env_range(void)
{
#ifdef CONFIG_ENV_RANGE
	return CONFIG_ENV_RANGE;
#else
	return CONIG_ENV_SIZE;
#endif
}
/* We may define our own get_env_range() or use the default one here */
size_t get_env_range(void) __attribute__((weak, alias("__get_env_range")));
#else
/* We must have get_env_range() */
size_t get_env_range(void);
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
int env_init(void)
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
				gd->env_valid = 2;
				gd->env_addr = env2;
			} else {
				/* env1 is newer (or same age) */
				gd->env_valid = 1;
				gd->env_addr = env1;
			}
			return 0;
		} else if (crc2_ok) {
			gd->env_valid = 2;
			gd->env_addr = env2;
			return 0;
		}
	}
#endif
	if (crc1_ok) {
		gd->env_valid = 1;
		gd->env_addr = env1;
	} else {
		gd->env_valid = 0;
		gd->env_addr = NULL;
	}

#else /* ENV_IS_EMBEDDED || CONFIG_NAND_ENV_DST */
	gd->env_addr	= (ulong)&default_environment[0];
	gd->env_valid	= 1;
	gd->env_size	= get_default_env_size();
#endif /* ENV_IS_EMBEDDED || CONFIG_NAND_ENV_DST */

	return 0;
}

#ifdef CMD_SAVEENV
/*
 * The legacy NAND code saved the environment in the first NAND device i.e.,
 * nand_dev_desc + 0. This is also the behaviour using the new NAND code.
 */
static int writeenv(size_t offset, u_char *buf, size_t env_size)
{
	size_t end = offset + get_env_range();
	size_t amount_saved = 0;
	size_t blocksize, len;

	blocksize = nand_info[0].erasesize;
	len = min(blocksize, env_size);

	while (amount_saved < env_size && offset < end) {
		if (nand_block_isbad(&nand_info[0], offset)) {
			offset += blocksize;
		} else {
			if (nand_write(&nand_info[0], offset, &len, buf))
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

#ifdef CONFIG_ENV_OFFSET_REDUND
static unsigned char env_flags;

int saveenv(void)
{
	nand_erase_options_t nand_erase_options;
	env_t *env_new;
	char *data;
	size_t env_offset;
	size_t env_range = get_env_range();
	size_t env_size = get_env_size();
	size_t env_offset;

	if (env_range < env_size)
		return 1;

	env_new = (env_t *)malloc(env_size);
	if (!env_new) {
		puts("malloc ");
		goto FAILED;
	}

	data = (char *)(env_new + 1);
	env_size -= ENV_HEADER_SIZE;
	if (hexport_r(&env_htab, '\0', 0, data, env_size, 0, NULL) < 0) {
		printf("Export environment: errno = %d ", errno);
		goto FAILED;
	}
	env_new.crc	= crc32(0, (u_char *)data, env_size);
	env_new.flags	= ++env_flags; /* increase the serial */

	if (gd->env_valid == 1) {
		env_offset = CONFIG_ENV_OFFSET_REDUND;;
		puts("Erasing redundant NAND...\n");
	} else {
		env_offset = get_env_offset();
		puts("Erasing NAND...\n");
	}
	memset(&nand_erase_options, 0, sizeof(nand_erase_options));
	nand_erase_options.length = env_range;
	nand_erase_options.offset = env_offset;
	if (!nand_erase_opts(&nand_info[0], &nand_erase_options)) {
		puts(gd->env_valid == 1 ?
		     "Writing to redundant NAND... " : "Writing to NAND... ");
		if (!writeenv(env_offset, (u_char *)env_new)) {
			gd->env_valid = (gd->env_valid == 2) ? 1 : 2;
			puts("done\n");
			free(env_new);

			return 0;
		}
	}

FAILED:
	free(env_new);
	puts("FAILED!\n");

	return 1;
}
#else /* ! CONFIG_ENV_OFFSET_REDUND */
int saveenv(void)
{
	env_t	*env_new;
	char *data;
	nand_erase_options_t nand_erase_options;
	size_t env_size = get_env_size();
	size_t env_offset;
	size_t env_range = get_env_range();

	if (env_range < env_size)
		return 1;

	env_new = (env_t *)malloc(env_size);
	if (!env_new) {
		puts("malloc ");
		goto FAILED;
	}

	data = (char *)(env_new + 1);
	if (hexport_r(&env_htab, '\0', 0, &data, env_size - ENV_HEADER_SIZE, 0,
		      NULL) < 0) {
		printf("Export environment: errno = %d ", errno);
		goto FAILED;
	}
	env_new->crc = crc32(0, (u_char *)data, env_size - ENV_HEADER_SIZE);

	puts("Erasing Nand...\n");
	env_offset = get_env_offset();
	memset(&nand_erase_options, 0, sizeof(nand_erase_options));
	nand_erase_options.length = env_range;
	nand_erase_options.offset = env_offset;
	if (!nand_erase_opts(&nand_info[0], &nand_erase_options)) {
		puts("Writing to Nand... ");
		if (!writeenv(env_offset, (u_char *)env_new, env_size)) {
			puts("done\n");
			free(env_new);
			return 0;
		}
	}

FAILED:
	free(env_new);
	puts("FAILED!\n");

	return 1;
}
#endif /* CONFIG_ENV_OFFSET_REDUND */
#endif /* CMD_SAVEENV */

static int readenv(size_t offset, u_char *buf, size_t env_size)
{
	size_t end = offset + get_env_range();
	size_t amount_loaded = 0;
	size_t blocksize, len;
	u_char *char_ptr;

	blocksize = nand_info[0].erasesize;
	if (!blocksize)
		return 1;

	while (amount_loaded < env_size && offset < end) {
		len = min(blocksize, env_size - amount_loaded);
		if (!nand_block_isbad(&nand_info[0], offset)) {
			char_ptr = &buf[amount_loaded];
			if (nand_read_skip_bad(&nand_info[0], offset,
					       &len, NULL,
					       nand_info[0].size, char_ptr))
				return 1;

			amount_loaded += len;
		}
		offset += blocksize;
	}

	if (amount_loaded != env_size)
		return 1;

	return 0;
}

#ifdef CONFIG_ENV_OFFSET_OOB
int get_nand_env_oob(nand_info_t *nand, unsigned long *result)
{
	struct mtd_oob_ops ops;
	uint32_t oob_buf[ENV_OFFSET_SIZE / sizeof(uint32_t)];
	int ret;

	ops.datbuf	= NULL;
	ops.mode	= MTD_OOB_AUTO;
	ops.ooboffs	= 0;
	ops.ooblen	= ENV_OFFSET_SIZE;
	ops.oobbuf	= (void *)oob_buf;

	ret = nand->read_oob(nand, ENV_OFFSET_SIZE, &ops);
	if (ret) {
		printf("error reading OOB block 0\n");
		return ret;
	}

	if (oob_buf[0] == ENV_OOB_MARKER) {
		*result = oob_buf[1] * nand->erasesize;
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
void env_relocate_spec(void)
{
#if !defined(ENV_IS_EMBEDDED)
	int crc1_ok = 0, crc2_ok = 0;
	env_t *ep, *env1, *env2;
	env_size = get_env_size();

	env1 = (env_t *)malloc(env_size);
	env2 = (env_t *)malloc(env_size);
	if (env1 == NULL || env2 == NULL) {
		puts("Can't allocate buffers for environment\n");
		set_default_env("!malloc() failed");
		goto done;
	}

	if (readenv(CONFIG_ENV_OFFSET, (u_char *)env1, env_size))
		puts("No Valid Environment Area found\n");

	if (readenv(CONFIG_ENV_OFFSET_REDUND, (u_char *)env2, env_size))
		puts("No Valid Redundant Environment Area found\n");

	gd->env_size = env_size;
	env_size -= ENV_HEADER_SIZE;
	crc1_ok = (crc32(0, (u_char *)(env1 + 1), env_size) == env1->crc);
	crc2_ok = (crc32(0, (u_char *)(env2 + 1), env_size) == env2->crc);

	if (crc1_ok && crc2_ok) {
		/* both ok - check serial */
		if ((env1->flags < env2->flags)
		    || (env1->flags == 255 && env2->flags == 0)) {
			/* env2 is newer */
			gd->env_valid = 2;
		} else {
			/* env1 is newer (or same age) */
			gd->env_valid = 1;
		}
	} else if (crc2_ok) {
		gd->env_valid = 2;
	} else if (crc2_ok) {
		gd->env_valid = 1;
	} else {
		set_default_env("!bad CRC");
		goto done;
	}

        if (gd->env_valid == 1)
                ep = env1;
        else
                ep = env2;

        env_flags = ep->flags;
        env_import((char *)ep, 0);

done:
	free(env1);
	free(env2);

#endif /* ! ENV_IS_EMBEDDED */
}
#else /* ! CONFIG_ENV_OFFSET_REDUND */
/*
 * The legacy NAND code saved the environment in the first NAND
 * device i.e., nand_dev_desc + 0. This is also the behaviour using
 * the new NAND code.
 */
void env_relocate_spec(void)
{
#if !defined(ENV_IS_EMBEDDED)
	char *buf;
	size_t env_size;

#if defined(CONFIG_ENV_OFFSET_OOB)
	/*
	 * If unable to read environment offset from NAND OOB then fall through
	 * to the normal environment reading code below
	 */
	if (!get_nand_env_oob(&nand_info[0], &nand_env_oob_offset)) {
		printf("Found Environment offset in OOB..\n");
	} else {
		set_default_env("!no env offset in OOB");
		return;
	}
#endif

	env_size = get_env_size();
	buf = malloc(env_size);
	if (buf && (readenv(get_env_offset(), (u_char *)buf, env_size) == 0))
		env_import(buf, 1, env_size);
	else
		set_default_env("!readenv() failed");
	free(buf);
#endif /* ! ENV_IS_EMBEDDED */
}
#endif /* CONFIG_ENV_OFFSET_REDUND */
