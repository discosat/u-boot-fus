/*
 * drivers/mtd/nand/nand_refresh.c
 *
 * Copyright (C) 2014 F&S Elektronik Systeme GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 *
 * Block Refresh Algorithm for NAND pages with high bitflip counts
 * ---------------------------------------------------------------
 *
 * Bitflips may be caused by read disturbs, i.e. after many many reads. Then
 * rewriting the block will help and usually reduces the bit errors back to
 * zero.
 *
 * To refresh the block, we could load it to RAM, erase the block in NAND and
 * then write back the data from RAM to NAND. However if we are interrupted
 * exactly while doing this, for example by a power failure, the data is
 * irretrievably lost. So we'll write the data to a reserved backup block
 * first before we erase the original block. If anything fails, we can resume
 * the refresh procedure at the next start by using the data from the backup
 * block.
 *
 * What's even more, there is a small chance that we do not only have soft
 * bitflips that can be recovered by this refreshing procedure. If we are
 * unlucky, there may also be hard bitflips that remain stuck when erasing or
 * writing back the data to the original block. So instead of a refreshed
 * block we might end up with a bad block after running this procedure. If we
 * keep the data only in RAM, we don't know where to store back the data in
 * this case. But if we use the backup block, we can go to a kind of emergency
 * mode and use the backup block instead of the original block as a
 * replacement. At least until the system is repaired or the original block is
 * "officially" marked bad, in which case we can end the redirection and free
 * the backup block again.
 *
 * So using this backup block provides more data safety, but of course is a
 * little bit more complex.
 *
 * The opportunity to resume an interrupted block refresh depends on the fact
 * that we can detect at boot time if the previous refresh was completely
 * finished or if it was interrupted at some point. This requires to store the
 * offset (or number) of the currently refreshed block in some non-volatile
 * memory as long as the refreshing procedure is in progress. If the page
 * layout of the OOB and ECC data is chosen accordingly, this can be stored in
 * the last page of the backup block itself. For example in the first few bytes
 * of the spare area, where also the bad block marker is stored in the first
 * one or two pages. That is what we'll assume here. But it has to be noted
 * that this approach has some unfortunate side effects. For example step 5 of
 * the following algorithm would not be necessary if the information could be
 * stored somewhere else.
 *
 * This is the basic algorithm:
 *
 *   1. Erase the backup block (just to be sure).
 *   2. Copy data from the original block to the backup block; as the last
 *      step store the refresh offset (e.g. as block number) to the last page.
 *   3. Erase the original block.
 *   4. Copy data from the backup block to the original block.
 *   5. To invalidate the refresh number as quick and atomic as possible,
 *      clear the last page of the backup block by writing 0 to all bytes of
 *      the main and spare area (with a raw write). This should trigger an
 *      uncorrectable ECC error when read before step 6 is finished.
 *   6. Erase the backup block.
 *
 * What happens if power fails during one of these steps?
 *
 *   1. No harm done, the original block is still there and we have no refresh
 *      offset set in the backup block. No resume is done at the next start.
 *   2. No harm done, the original block is still there and we most probably
 *      have no valid refresh offset set in the backup block because we write
 *      it as the last thing in step 2. So no resume is done at the next start.
 *   3. At the next start we'll see the refresh offset in the backup block and
 *      therefore resume the refresh algorithm from step 3.
 *   4. At the next start we'll see the refresh offset in the backup block and
 *      therefore resume the refresh algorithm from step 3. So we'll erase any
 *      partially written data again and write anew.
 *   5. Even if the last page with all zeroes is not completely written back
 *      to the NAND cells, the ECC should fail when reading this page, so the
 *      refresh number is immediately invalidated and we don't do a resume at
 *      the next start. This is important. Without this step it would be
 *      impossible to say if we were interrupted in steps 3 or 4 before the
 *      original block was written back, or in step 6 after the block was
 *      written back but before the backup block was completely erased. In the
 *      latter case it would be fatal to resume a refresh again from step 3,
 *      because we would replace the already correctly written original block
 *      with incomplete data from the partially erased backup block.
 *   6. Even if the backup block is not completely erased yet, we don't have a
 *      valid refresh number anymore due to step 5. So at the next start we'll
 *      ignore the backup block and go to normal mode. No resume is done.
 *
 * Possible error states:
 *
 * NAND_REFRESH_STATE_INSTABLE:
 *       Many bitflips are detected in a page but refresh is not possible.
 *       May happen if backup blocks are all worn down and unusable or
 *       original block is not fully readable in step 2. Data is still OK but
 *       failure is imminent.
 *
 * NAND_REFRESH_STATE_EMERGENCY:
 *       Original block went bad while doing refresh when erasing or restoring
 *       the original block. May happen when flash becomes old and blocks are
 *       worn down and have really stuck bits. The data is still available in
 *       the backup block, therefore reads are redirected to the backup block
 *       (=EMERGENCY MODE). Writes to the block and erasing the block will
 *       fail immediately. No more refreshs are possible because the backup
 *       block is still in use. System is *very* instable, failure is
 *       imminent.
 *       This mode can be cancelled by marking the original block "officially"
 *       as bad, which may happen automatically by the filesystem if any
 *       further modification of this block will fail. Instead of marking the
 *       block bad (which it is already), the backup block is freed again in
 *       this case.
 *
 * NAND_REFRESH_STATE_DATALOSS:
 *       Backup block went bad while doing refresh, before data of original
 *       block could be restored (somewhere between steps 2 and 4). Some data
 *       was actually lost. This is very very unlikely to happen.
 *
 * Remark:
 * The following code uses 0 for invalid offsets. This silently assumes that
 * block 0 (at offset 0) is always required for booting and can neither be
 * refreshed, nor be used as backup block.
 */

#include <common.h>
#include <asm/errno.h>
#include <linux/mtd/mtd.h>
#include <nand.h>

typedef struct erase_info erase_info_t;
typedef struct mtd_info	  mtd_info_t;

/* We need a buffer for one page and one OOB for doing the block refresh */
static u_char pagebuf[NAND_MAX_PAGESIZE];
static u_char oobbuf[NAND_MAX_OOBSIZE];


/* Erase block at given offset; mark as bad if it fails */
static int erase_block(struct mtd_info *mtd, loff_t offset)
{
	struct erase_info erase;
	int rval;

	memset(&erase, 0, sizeof(erase));
	erase.mtd = mtd;
	erase.len = mtd->erasesize;
	erase.addr = offset;

	rval = mtd_erase(mtd, &erase);
	if (rval == -EIO) {
		printf("%s: Erasing block at 0x%08llx failed, marking bad\n",
		       mtd->name, offset);
		mtd_block_markbad(mtd, offset);
	}

	return rval;
}

/*
 * Erase the destination block, then copy data from source block to target
 * block.
 * Return value: 0:        Successfully erased and copied
 *               -EBADMSG: Uncorrectable read errors when reading source block
 *               -EIO:     Destination block could not be erased or written
 *               -EROFS:   Locked or read-only chip
 */
static int erase_copy_block(struct mtd_info *mtd, loff_t refreshoffs,
			    int to_backup)
{
	int rval;
	int final_rval = 0;
	loff_t from;
	loff_t to;
	u_int32_t offs;
	mtd_oob_ops_t ops;
	int i;

	if (to_backup) {
		from = refreshoffs;
		to = mtd->backupoffs;
	} else {
		from = mtd->backupoffs;
		to = refreshoffs;
	}

	/* Erase the destination block */
	rval = erase_block(mtd, to);
	if (rval) {
		if (rval != -EROFS)
			rval = -EIO;
		return rval;
	}

	nr_debug("%s block at 0x%08llx erased\n",
		 to_backup ? "Backup" : "Original", to);

	ops.len = mtd->writesize;
	ops.ooboffs = 0;
	ops.ooblen = mtd->oobavail;
	ops.mode = MTD_OPS_AUTO_OOB;

	/* Read page by page from source and write to destination block. */
	for (offs = 0; offs < mtd->erasesize; offs += mtd->writesize) {
		/* Read main and OOB area from source block. */
		ops.datbuf = pagebuf;
		ops.oobbuf = mtd->oobavail ? oobbuf : NULL;
		nr_debug("Reading page at 0x%08llx -> MAIN%s\n", from + offs,
			 ops.oobbuf ? " OOB" : "");
		rval = mtd_read_oob(mtd, from + offs, &ops);

		/* If we have read errors while copying to backup block, we can
		   return immediately as we can not read the original block
		   and therefore a refresh does not make sense. However if we
		   have read errors while copying to the original block, this
		   is fatal, as we can't read the backup block anymore. We must
		   continue and we must live with the bad data because we
		   don't have the original data anywhere else anymore. */
		if (rval && (rval != -EUCLEAN)) {
			if (to_backup)
				return -EBADMSG;

			final_rval = -EBADMSG;
			nr_debug("Reading page at 0x%08llx failed with %d\n",
				 from + offs, rval);
		}

		/* Only write data if it is not completely empty. Exception:
		   when writing the last page to the backup block it must be
		   written even if empty to allow setting the refresh offset
		   value. */
		if (!to_backup || (offs != mtd->erasesize - mtd->writesize)) {
			i = mtd->writesize;
			do {
				if (pagebuf[i - 1] != 0xFF)
					break;
			} while (--i);
			if (!i)
				ops.datbuf = NULL;
			i = mtd->oobavail;
			while (i) {
				if (oobbuf[i - 1] != 0xFF)
					break;
				i--;
			}
			if (!i)
				ops.oobbuf = NULL;
			if (!ops.datbuf && !ops.oobbuf)
			{
				nr_debug("Skipping empty page at 0x%08llx\n",
					 to + offs);
				continue;
			}
		}
		nr_debug("Writing page at 0x%08llx ->%s%s\n", to + offs,
			 ops.datbuf ? " MAIN" : "", ops.oobbuf ? " OOB" : "");
		if (to_backup && (offs == mtd->erasesize - mtd->writesize)) {
			mtd->extradata = from;
			mtd->extraflags = MTD_EXTRA_REFRESHOFFS;
		}
		rval = mtd_write_oob(mtd, to + offs, &ops);
		mtd->extraflags = 0;

		/* If we have write errors, we can stop immediately and mark
		   the block as bad. We return without marking the block as
		   bad if we have a read-only or locked chip. On all other
		   errors we try to continue. */
		if (rval == -EIO) {
			printf("%s: Writing page at 0x%08llx failed, marking "
			       "block at 0x%08llx bad\n",
			       mtd->name, to + offs, to);
			mtd_block_markbad(mtd, to);
			return rval;
		}
		if (rval == -EROFS)
			return rval;
		if (rval)
			printf("%s: Writing page at 0x%08llx failed with %d\n",
			       mtd->name, to + offs, rval);
	}

	nr_debug("Data %s block at 0x%08llx\n",
		 to_backup ? "written to backup" : "restored to original", to);

	return final_rval;
}

/*
 * Check if the current backup block is valid and can be used. If not, search
 * for the next valid block in the backup block region towards backupend. If
 * backupend is larger than backupoffs, the blocks are searched in increasing
 * order, otherwise in decreasing order.
 */
static int get_backupblock(struct mtd_info *mtd)
{
	loff_t backupoffs = mtd->backupoffs & ~(mtd->erasesize - 1);
	loff_t backupend = mtd->backupend & ~(mtd->erasesize - 1);

	while (backupoffs && mtd_block_isbad(mtd, backupoffs)) {
		if (backupoffs > backupend)
			backupoffs -= mtd->erasesize;
		else if (backupoffs < backupend)
			backupoffs += mtd->erasesize;
		else
			backupoffs = 0;
	}

	mtd->backupoffs = backupoffs;

	nr_debug("Using backup block at 0x%08llx\n", backupoffs);

	return backupoffs != 0;
}

/* Report NAND state to OS and return the final return value for this case */
static void __board_nand_state(struct mtd_info *mtd, unsigned int state)
{
}

void board_nand_state(struct mtd_info *mtd, unsigned int state)
	__attribute__((weak, alias("__board_nand_state")));

/* Go to INSTABLE MODE. This is rather harmless, just show message. */
static void enter_instable_mode(struct mtd_info *mtd, loff_t refreshoffs)
{
	printf("\n*** INSTABLE NAND SYSTEM WITH MANY BITFLIPS ***\n"
	       "%s: Can not refresh weak block at 0x%08llx!\n"
	       "Data of this block is considered unsafe!!!\n\n",
	       mtd->name, refreshoffs);

	board_nand_state(mtd, NAND_REFRESH_STATE_INSTABLE);
}

/*
 * Go to EMERGENCY MODE. From now on we will redirect all read accesses to the
 * original block to the backup block and hope that some service technician
 * will repair the system. Board specific code may pass some information to
 * the operating system to make the user aware of the risk of imminent failure
 * and probable loss of data.
 */
static void enter_emergency_mode(struct mtd_info *mtd, loff_t refreshoffs)
{
	mtd->replaceoffs = refreshoffs;

	printf("\n*** EMERGENCY MODE AFTER NAND BLOCK REFRESH ***\n"
	       "%s: block at 0x%08llx is heavily damaged and marked bad!\n"
	       "It is replaced by temporary backup block at 0x%08llx.\n"
	       "System failure imminent!\n\n",
	       mtd->name, refreshoffs, mtd->backupoffs);

	board_nand_state(mtd, NAND_REFRESH_STATE_EMERGENCY);
}

/* Go to DATALOSS MODE. We can not do much more than reporting it. */
static void enter_dataloss_mode(struct mtd_info *mtd, loff_t refreshoffs)
{
	printf("\n*** DATA LOSS IN NAND BLOCK REFRESH ***\n"
	       "%s: Reading from backup block at 0x%08llx failed!\n"
	       "Original block at 0x%08llx may contain bad data now!!!\n\n",
	       mtd->name, mtd->backupoffs, refreshoffs);

	board_nand_state(mtd, NAND_REFRESH_STATE_DATALOSS);
}

/* Release backup block after refresh procedure */
void nand_refresh_free_backup(struct mtd_info *mtd)
{
	mtd_oob_ops_t ops;

	/* We can leave EMERGENCY MODE now if it was active */
	if (mtd->replaceoffs) {
		printf("\n*** LEAVING EMERGENCY MODE ***\n"
		       "%s: Bad block at 0x%08llx now officially marked bad.\n"
		       "Redirection to backup block at 0x%08llx ended.\n\n",
		       mtd->name, mtd->replaceoffs, mtd->backupoffs);
		mtd->replaceoffs = 0;
	}

	/* Step 5: Clear last page of the backup block to invalidate the
	   refresh offset. If this fails it does not matter too much as we
	   erase the whole block immediately after that anyway. See
	   explanation at the top of this file why we need this step. */
	memset(pagebuf, 0, mtd->writesize);
	memset(oobbuf, 0, mtd->oobsize);
	ops.datbuf = pagebuf;
	ops.len = mtd->writesize;
	ops.oobbuf = oobbuf;
	ops.ooboffs = 0;
	ops.ooblen = mtd->oobsize;
	ops.mode = MTD_OPS_RAW;
	mtd_write_oob(mtd, mtd->backupoffs + mtd->erasesize - mtd->writesize,
		      &ops);

	nr_debug("Refresh offset in backup block invalidated\n");

	/* Step 6: Erase backup block */
	erase_block(mtd, mtd->backupoffs);

	nr_debug("Backup block erased\n");
}

/* Finish (normal or interrupted) refresh cycle, starting from step 3 */
static int finish_refresh(struct mtd_info *mtd, loff_t refreshoffs)
{
	int rval;

	/* Step 3 + 4: Erase original block and copy back data */
	rval = erase_copy_block(mtd, refreshoffs, 0);

	/* Erase or write error: enter EMERGENCY MODE */
	if (rval == -EIO) {
		enter_emergency_mode(mtd, refreshoffs);
		return rval;
	}

	/* Read error: This is very very unlikely because we just have written
	   the backup block back in step 2. But if it happens, it is fatal. We
	   already restored all data that we could in erase_copy_block(), but
	   some is damaged. */
	if (rval == -EBADMSG)
		enter_dataloss_mode(mtd, refreshoffs);

	/* The original block is rewritten in any case, so we also have to do
	   steps 5 + 6 in any case. */
	nand_refresh_free_backup(mtd);

	return rval;
}

/* Refresh the block with given offset */
int nand_refresh(struct mtd_info *mtd, loff_t refreshoffs)
{
	int rval;

	/* If we are already in emergency mode, we can not do any refreshs
	   because the backup block is already in use */
	if (mtd->replaceoffs)
		return -EUCLEAN;

	/* Find start of block */
	refreshoffs &= ~(mtd->erasesize - 1);
	printf("%s: Refreshing block with many bitflips at 0x%08llx...\n",
	       mtd->name, refreshoffs);

	/* Steps 1 + 2: Erase the backup block and copy data to it */
	do {
		/* If we run out of backup blocks, we can't refresh the block,
		   but no harm is done yet. We have to live with the worn down
		   original block then. Just issue a warning. */
		if (!get_backupblock(mtd)) {
			printf("Refresh cancelled, no backup blocks\n");
			enter_instable_mode(mtd, refreshoffs);
			return -EUCLEAN;
		}
		rval = erase_copy_block(mtd, refreshoffs, 1);
	} while (rval == -EIO);

	if (rval == -EBADMSG) {
		/* We can't read the block (for example a later page than the
		   one that triggered refreshing), so we also have to live
		   with the worn down original block. */
		printf("Refresh cancelled, can not read original data\n");
		enter_instable_mode(mtd, refreshoffs);
		return -EUCLEAN;
	} else if (rval) {
		printf("Refresh cancelled, error %d\n", rval);
		enter_instable_mode(mtd, refreshoffs);
		return -EUCLEAN;
	}

	/* Steps 3 to 6: Copy back data and erase backup block */
	rval = finish_refresh(mtd, refreshoffs);
	if (!rval)
		printf("%s: Refresh successfully completed.\n", mtd->name);

	/* If we get -EIO, then the original block could not be written and we
	   are in EMERGENCY MODE. Even if this is bad we still have the data
	   in the backup block. This should be without bitflips and therefore
	   we can return 0. If we get -EBADMSG, then we could not read back
	   the data from the backup block and we are in DATALOSS MODE. However
	   the original read did succeed with bitflips, so we should return
	   -EUCLEAN. If the device is read-only, we could not write back the
	   block, so we still have the bitflips and should return -EUCLEAN.
	   finish_refresh() will not return any other error values. */
	if ((rval == -EBADMSG) || (rval == -EROFS))
		return -EUCLEAN;

	return 0;
}


/*
 * Returns a different value if in EMERGENCY MODE and the offset is redirected
 * to the backup block.
 */
loff_t nand_refresh_final_offset(struct mtd_info *mtd, loff_t offset)
{
	if (mtd->replaceoffs && (offset >= mtd->replaceoffs)
	    && (offset < mtd->replaceoffs + mtd->erasesize))
		offset = offset - mtd->replaceoffs + mtd->backupoffs;

	return offset;
}


/*
 * At system start, check for any interrupted block refresh and resume it if
 * possible.
 *
 * Return value: 0:    Resume successful or no resume necessary
 *               -EIO: Resume failed, we are in emergency mode now
 */
void nand_refresh_init(struct mtd_info *mtd)
{
	int rval;
	loff_t refreshoffs;
	mtd_oob_ops_t ops;

	/* Get the first possible backup block. If there is no backup block
	   available at all, it is not possible that a block refresh was in
	   progress the last time. Go to normal mode then. */
	if (!get_backupblock(mtd))
		return;

	/* Read the last page of the backup block. If we can't read this page,
	   we may have been interrupted at the end of step 2 or in steps 5 or
	   6. In both cases we can ignore the backup block as our original
	   block is OK. If we actually do have a corrupt backup block, we can
	   not resume anyway because we don't know the original block number.
	   So we must ignore it, too. */
	ops.datbuf = pagebuf;
	ops.len = mtd->writesize;
	ops.oobbuf = mtd->oobavail ? oobbuf : NULL;
	ops.ooboffs = 0;
	ops.ooblen = mtd->oobavail;
	ops.mode = MTD_OPS_AUTO_OOB;
	mtd->extraflags = MTD_EXTRA_REFRESHOFFS;
	rval = mtd_read_oob(mtd, mtd->backupoffs + mtd->erasesize
			    - mtd->writesize, &ops);
	mtd->extraflags = 0;
	if (rval && (rval != -EUCLEAN)) {
		/* If the last page of the backup block could not be read
		   (e.g. if we were interrupted in step 5 or 6), erase the
		   backup block to avoid any further error messages */
		if (rval == -EBADMSG)
			erase_block(mtd, mtd->backupoffs);
		return;
	}

	/* If the page shows no valid refresh offset (for example if the page
	   is empty), there was either no block refresh in progress at all or
	   we were interrupted in steps 1 or 2. In all cases all our original
	   data is OK and we can go to normal mode. */
	refreshoffs = mtd->extradata & ~(mtd->erasesize - 1);
	if (!refreshoffs)
		return;

	/* We have a valid refresh number, so there was a block refresh in
	   progress that was interrupted. But if the original block is bad
	   then we could neither write back the data the last time, nor can we
	   now. Enter EMERGENCY MODE. */
	if (mtd_block_isbad(mtd, refreshoffs)) {
		enter_emergency_mode(mtd, refreshoffs);
		return;
	}

	/* Resume the refresh cycle */
	printf("%s: Resuming interrupted refresh of block at 0x%08llx...\n",
	       mtd->name, refreshoffs);
	if (!finish_refresh(mtd, refreshoffs))
		printf("%s: Interrupted refresh completed\n", mtd->name);
}
