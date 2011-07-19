/*****************************************************************************/
/***      _____ _           __  __  ____  ____     //____ ____  __  __     ***/
/***     |  __ (_)         |  \/  |/ __ \|  _ \   // ____/ __ \|  \/  |    ***/
/***     | |__) |  ___ ___ | \  / | |  | | | \ \ // |   | |  | | \  / |    ***/
/***     |  ___/ |/ __/ _ \| |\/| | |  | | | | |//| |   | |  | | |\/| |    ***/
/***     | |   | | (__ (_) | |  | | |__| | |_/ // | |____ |__| | |  | |    ***/
/***     |_|   |_|\___\___/|_|  |_|\____/|____//   \_____\____/|_|  |_|    ***/
/***                                         //                            ***/
/*****************************************************************************/
/***                                                                       ***/
/***           S 3 C 6 4 x x  -  M M C   Dr i v e r   U - B o o t          ***/
/***                                                                       ***/
/*****************************************************************************/
/*** File:     mmc_s3c64xx.c                                               ***/
/*** Author:   Hartmut Keller                                              ***/
/*** Created:  06.06.2011                                                  ***/
/*** Modified: 07.06.2011 14:51:16 (HK)                                    ***/
/***                                                                       ***/
/*** Description:                                                          ***/
/*** SD-card driver for S3C64xx in U-Boot. Based on the SD-card driver for ***/
/*** PicoMOD6 in the F&S NBoot loader.                                     ***/
/***                                                                       ***/
/*** Modification History:                                                 ***/
/*** 06.06.2011 HK: Converted from NBoot to U-Boot.                        ***/
/*****************************************************************************/

#include <common.h>
#include <regs.h>
#include <mmc.h>
#include <mmc_s3c64xx.h>                  /* Own interface */

/* ##### 24.08.2010 HK: TODO
   - SDHC_OpenMedia() ist gerade eine große lange Funktion. Evtl. wieder in
     ein paar kleinere Funktionen aufsplitten, damit es überschaubarer wird.
 */

/* We support external SD card slot (SDHC_CHANNEL_0) and on-board SD card
   slot (SDHC_CHANNEL_1) */
#define MAX_UNITS 2

#define __USE_ISSUE_AND_READ__

/* ------------ Local Definitions ------------------------------------------ */

/* SDDBG should only be activated if stage 2 (printf) is available */
#if 0
#include "lib/s2clib.h"
#define SDDBG printf
#else
#define SDDBG(...)
#endif

/* Timeout when waiting for block read/write/etc: 1s */
#define SDHC_TIMEOUT CFG_HZ

/* HighSpeed mode separator */
#define SDHC_MMC_HIGH_SPEED_CLOCK 26000000
#define SDHC_SD_HIGH_SPEED_CLOCK 25000000

/* Flag if CMD or ACMD should be used for in SDHC_IssueCommandReadBlock() */
#define SDHC_ACMD 0x8000


/* ------------ Local Variables -------------------------------------------- */

/* Info of possible mmc devices */
SDHC mmc_unit[MAX_UNITS];

/* Mapping between mmc device number and SDHC channel */
SDHC_Channel mmc_channel[MAX_UNITS] = {
	SDHC_CHANNEL_0,			  /* mmc 0 is on channel 0 */
	SDHC_CHANNEL_1			  /* mmc 1 is on channel 1 */
};

/* Possible card types */
const char * const pCardTypes[] =
{
    "SD Card (Byte Mode)",                /* SDHC_SD_CARD, SDHC_BYTE_MODE */
    "SDHC Card (Block Mode)",             /* SDHC_SD_CARD, SDHC_BLOCK_MODE */
    "MMC Card (Byte Mode)",               /* SDHC_MMC_CARD, SDHC_BYTE_MODE */
    "MMC Card (Block Mode)"               /* SDHC_MMC_CARD, SDHC_BLOCK_MODE */
};

/* Response settings depending on response type:
    [7:6] Command Type
    [5]   Data Present Select
    [4]   Command Index Check Enable
    [3]   CRC Check Enable
    [1:0] Response Type Select */
unsigned char const SDHC_cmd_sfr_data[] =
{
    (unsigned char)((0<<4)|(0<<3)|(0<<0)),  /* RES_NO_TYPE */
    (unsigned char)((1<<4)|(1<<3)|(2<<0)),  /* RES_R1_TYPE */
    (unsigned char)((1<<4)|(1<<3)|(3<<0)),  /* RES_R1B_TYPE */
    (unsigned char)((0<<4)|(1<<3)|(1<<0)),  /* RES_R2_TYPE */
    (unsigned char)((0<<4)|(0<<3)|(2<<0)),  /* RES_R3_TYPE */
    (unsigned char)((0<<4)|(0<<3)|(2<<0)),  /* RES_R4_TYPE */
    (unsigned char)((1<<4)|(1<<3)|(2<<0)),  /* RES_R5_TYPE */
    (unsigned char)((1<<4)|(1<<3)|(2<<0)),  /* RES_R6_TYPE */
    (unsigned char)((1<<4)|(1<<3)|(2<<0)),  /* RES_R7_TYPE */
};

/* Register base addresses for the three channels */
SDHC_REGS *const ChannelBaseAddr[] =
{
	(SDHC_REGS *)ELFIN_HSMMC_0_BASE,
	(SDHC_REGS *)ELFIN_HSMMC_1_BASE,
	(SDHC_REGS *)ELFIN_HSMMC_2_BASE,
};

/* Table with command settings for buswidth on MMC cards */
U32 const MmcBuswidth[] =
{
    0<<8,                                 /* for 1 bit buswidth */
    1<<8,                                 /* for 4 bit buswidth */
    2<<8                                  /* for 8 bit buswidth */
};

/* Table with buswidth settings for the SDHC_HOST_CTRL register */
U8 const HostBuswidth[] =
{
    0,                                    /* 1 bit buswidth */
    1<<1,                                 /* 4 bit buswidth */
    1<<5                                  /* 8 bit buswidth */
};


/* ------------ Prototypes Of Imported Functions --------------------------- */

/* Board specific switching of MMC slot power */
extern void mmc_s3c64xx_board_power(unsigned int channel);


/* ------------ Prototypes Of Local Functions ------------------------------ */

/* Set SDCLK as close as possible to the given frequency, return finally used
   frequency */
static U32 SDHC_SetHostSDCLK(SDHC_REGS *pRegs, U32 uTargetFreq, U32 uSrcFreq,
                             SDHC_CardType eCardType);

/* Set voltage range and get OCR for SD memory cards */
static bool SDHC_SetSDOCR(SDHC *sCh);

#ifndef PROGRAMSD
/* Set voltage range and get OCR for MMC cards */
static bool SDHC_SetMmcOcr(SDHC *sCh);
#endif

/* Wait until data is received, read data block and store in pBuffer */
static bool SDHC_ReadOneBlock(SDHC_REGS *pRegs, U32 *pBuffer);

#ifdef __USE_ISSUE_AND_READ__
/* Issue a command and read a data block */
static bool SDHC_IssueCommandReadBlock(SDHC_REGS *pRegs, U16 uCmd, U32 uArg,
                                       U32 uBlockSize, U32 *pBuffer);
#endif

/* ------------ Prototypes Of Library Internal Functions ------------------- */

/* These functions are declared globally because they are also used from
   s2sdhc.c, but they should not be called from outside the sdhc module. */

/* Wait until given interrupt is signaled and then clear it (or timeout) */
bool SDHC_IntWaitAndClear(SDHC_REGS *pRegs, U16 uIntMask);

/* Issue the given command with the given type, argument and response */
bool SDHC_IssueCommand(SDHC_REGS *pRegs, U16 uCmd, U32 uArg,
                       SDHC_CommandType cType, SDHC_ResponseType rType);

/* Wait until card is (again) in transfer state (SDHC_CS_TRAN) */
bool SDHC_WaitForCard2TransferState(SDHC_REGS *pRegs, U32 uRca);


/* ------------ Implementation Of Local Functions -------------------------- */

/*****************************************************************************
*** Function:    U32 SDHC_SetHostSDCLK(SDHC_REGS *pRegs, U32 uTargetFreq,  ***
***                                  U32 uSrcFreq, SDHC_CardType           ***
***                                  eCardType)                            ***
***                                                                        ***
*** Parameters:  pRegs:       Pointer to host controller registers         ***
***              uTargetFreq: Target frequency to set                      ***
***              uSrcFreq:    Input frequency into SD/MMC controller       ***
***              eCardType:   SD or MMC card                               ***
***                                                                        ***
*** Return:      Best possible frequency that is finally used              ***
***                                                                        ***
*** Description                                                            ***
*** -----------                                                            ***
*** Compute the divisor to divide the source frequency uSrcFreq to get the ***
*** fastest frequency not higher than the given target frequency           ***
*** uTargetFreq. Depending on the card type eCardType, do timing settings. ***
*** Then activate the clock, wait until internal clock is stable and       ***
*** activate external clock SDCLK.                                         ***
*****************************************************************************/
static U32 SDHC_SetHostSDCLK(SDHC_REGS *pRegs, U32 uTargetFreq, U32 uSrcFreq,
                             SDHC_CardType eCardType)
{
    U32 uClockDivision = 1;
    U32 control2, control3;
    U32 uHostCtl;

    /* Calculate minimum divisor (a power of 2 between 1 and 256) for source
       clock that results in a frequency of at most uTargetFreq. */
    while ((uSrcFreq > uTargetFreq) && (uClockDivision < 256))
    {
        uSrcFreq >>= 1;
        uClockDivision <<= 1;
    }

    /* Stop SDCLK, set internal clock speed and enable internal clock.
       uClockDivision contains the real divisor, we have to shift it right
       by one bit to follow this scheme: 0x00: /1, 0x01: /2, ... 0x80: /256 */
    pRegs->CLK_CTRL = ((uClockDivision >> 1) << 8) | (1<<0);

    /* The internal clock needs some time to stabilize; do the control setting
       in the meantime */
    if (uSrcFreq <= SDHC_SD_HIGH_SPEED_CLOCK)
    {
        /* Falling edge output (=normal speed, up to 25MHz) */
        uHostCtl = 0;
        control2 = (0<<30) | (0<<15) | (0<<14) | (1<<8);
        /* Set Rx/Tx Feedback Clock delay control to Delay3 (inverter delay) */
        control3 = (0<<31) | (0<<23) | (0<<15) | (0<<7);
    }
    else
    {
        /* Rising edge output (=high speed, up to 50MHz) */
        uHostCtl = (1<<2);
        if (eCardType == SDHC_SD_CARD)
        {
            /* SD: Setup time 6ns, hold time 2ns (Feedback clock disable on Rx
               and Tx clock, enable SDCLK Hold Enable, enable Command Conflict
               Mask) */
            control2 = (1<<30) | (0<<15) | (0<<14) | (1<<8);
            /* Set Rx and Tx Feedback Clock delay control to Delay3 (inverter
               delay) */
            control3 = (0<<31) | (0<<23) | (0<<15) | (0<<7);
        }
        else                              /* SDHC_MMC_CARD */
        {
            /* MMC: Setup time 5ns, hold time 5ns (Feedback clock disable on
               Tx clock, feedback clock enable on Rx clock, enable SDCLK Hold
               Enable, enable Command Conflict Mask) */
            control2 = (1<<30) | (0<<15) | (1<<14) | (1<<8);
            /* Set Tx Feedback Clock delay control to Delay3 (inverter
               delay), Rx Feedback Clock delay control to Delay2 (basic delay
               + 2ns) (Settings are for Samsung movinand card) */
            control3 = (0<<31) | (0<<23) | (1<<15) | (1<<7);
        }
    }

    pRegs->HOST_CTRL = (U8)uHostCtl;
    /* Keep clock source in CONTROL2 */
    pRegs->CONTROL2 = (pRegs->CONTROL2 & (3<<4)) | control2;
    pRegs->CONTROL3 = control3;

    /* Wait until internal clock is stable */
    while (!(pRegs->CLK_CTRL & (1<<1)))
	    ;

    /* Enable SDCLK */
    pRegs->CLK_CTRL |= (1<<2);

    /* Return the finally used frequency */
    return uSrcFreq;
}


/*****************************************************************************
*** Function:    bool SDHC_SetSDOCR(SDHC *sCh)                             ***
***                                                                        ***
*** Parameters:  sCh: Pointer to SD context                                ***
***                                                                        ***
*** Return:      TRUE: Success, FALSE: Failure                             ***
***                                                                        ***
*** Description                                                            ***
*** -----------                                                            ***
*** Set the voltage range and read the Operation Conditions Register (OCR) ***
*** of an SD card.                                                         ***
*****************************************************************************/
static bool SDHC_SetSDOCR(SDHC *sCh)
{
    U32 i, OCR;
    SDHC_REGS *pRegs = sCh->m_pRegs;
    bool bRet = FALSE;

    /* CMD0 (GO_IDLE_STATE): Reset the card to idle state. */
    if (!SDHC_IssueCommand(pRegs, 0, 0, SDHC_CMD_BC_TYPE, SDHC_RES_NO_TYPE))
        return bRet;

    /* CMD8 (SEND_IF_COND): Try to set 2.7-3.6V voltage range; this command
       is mandatory for Physical Spec V2.0 (SDHC); card remains in idle state */
    if (!SDHC_IssueCommand(pRegs, 8, (0x1<<8)|(0xaa), SDHC_CMD_BCR_TYPE,
                           SDHC_RES_R7_TYPE))
    {
        /* Legacy SD cards (before Spec V2.0) cards don't recognize CMD8 and
           return "illegal command"; this is OK, clear error state */
        while (pRegs->NORMAL_INT_STAT & (1<<15))
            pRegs->ERROR_INT_STAT = pRegs->ERROR_INT_STAT;
    }

    /* Set and receive OCR, wait until power-up routine is finished; try at
       most 500 times */
    for (i=0; i<500; i++)
    {
        /* CMD55 (APP_CMD): The next command should be interpreted as
           application specific command. RCA is (and must be) zero here! An
           MMC card does not respond to CMD55 with RCA 0. */
        if (!SDHC_IssueCommand(pRegs, 55, /*sCh->m_uRca*/0, SDHC_CMD_AC_TYPE,
                               SDHC_RES_R1_TYPE))
            break;

        /* ACMD41 (SD_SEND_OP_COND) Set voltage operation range
           (Ocr:2.7V~3.6V) and try to activate HCS (high capacity status) of
           SDHC cards; then receive card OCR */
        if (!SDHC_IssueCommand(pRegs, 41, 0x40ff8000, SDHC_CMD_BCR_TYPE,
                               SDHC_RES_R3_TYPE))
            break;

        /* If bit 31 is set, the card has finished its power up routine;
           otherwise we have to wait */
        OCR = sCh->m_pRegs->RSP0;
        if (OCR & (1U<<31))
        {
            if (OCR & (1<<30))
                sCh->m_eTransMode = SDHC_BLOCK_MODE;
            else
                sCh->m_eTransMode = SDHC_BYTE_MODE;
            bRet = TRUE;

            /* Legacy SD has Command Response Error, needs to be cleared */
            break;
        }
    }
    /* The current card is MMC card, then there's time out error,
       needs to be cleared. */
    while (pRegs->NORMAL_INT_STAT & (1<<15))
        pRegs->ERROR_INT_STAT = pRegs->ERROR_INT_STAT;
    return bRet;
}


/*****************************************************************************
*** Function:    bool SDHC_SetMmcOcr(SDHC *sCh)                            ***
***                                                                        ***
*** Parameters:  sCh: Pointer to SD context                                ***
***                                                                        ***
*** Return:      TRUE: Success, FALSE: Failure                             ***
***                                                                        ***
*** Description                                                            ***
*** -----------                                                            ***
*** Set the voltage range and read the Operation Conditions Register (OCR) ***
*** of an MMC card.                                                        ***
*****************************************************************************/
static bool SDHC_SetMmcOcr(SDHC *sCh)
{
    U32 i, OCR;
    SDHC_REGS *pRegs = sCh->m_pRegs;;
    bool bRet = FALSE;

    /* CMD0 (GO_IDLE_STATE): Reset card to idle state. */
    if (!SDHC_IssueCommand(pRegs, 0, 0, SDHC_CMD_BC_TYPE, SDHC_RES_NO_TYPE))
        return FALSE;

    /* Set and receive OCR, wait until power-up routine is finished; try at
       most 500 times */
    for (i=0; i<500; i++)
    {
#if 1   /* for New Movinand 2007.3.29 */
        /* CMD1 (Send_OP_COND): Initialize MMC card, set voltage range
           (Ocr:2.7V~3.6V), get back operation condition register (OCR) */
        SDHC_IssueCommand(pRegs, 1, 0x40FF8000, SDHC_CMD_BCR_TYPE,
                          SDHC_RES_R3_TYPE);
        OCR = pRegs->RSP0;
#else
        /* Query OCR (voltages) */
        SDHC_IssueCommand(pRegs, 1, 0, SDHC_CMD_BCR_TYPE, SDHC_RES_R3_TYPE);
        OCR = pRegs->RSP0 | (1<<30);

        /* Set given voltages and get OCR again */
        SDHC_IssueCommand(pRegs, 1, OCR, SDHC_CMD_BCR_TYPE, SDHC_RES_R3_TYPE);
        OCR = pRegs->RSP0;
#endif
        if (OCR & (1U<<31))
        {
            if (OCR & (1<<30))
            {
                sCh->m_ucSpecVer = 4;     /* default spec version */
                sCh->m_eTransMode = SDHC_BLOCK_MODE;
            }
            else
            {
                sCh->m_ucSpecVer = 1;     /* default spec version */
                sCh->m_eTransMode=SDHC_BYTE_MODE;
            }
            bRet = TRUE;
            break;
        }
    }

    /* If the current card is SD card, then there's time out error, needs to
       be cleared. */
    while (pRegs->NORMAL_INT_STAT & (1<<15))
        pRegs->ERROR_INT_STAT = pRegs->ERROR_INT_STAT;
    return FALSE;
}


/*****************************************************************************
*** Function:    bool SDHC_ReadOneBlock(SDHC_REGS *pRegs, U32 *pBuffer)    ***
***                                                                        ***
*** Parameters:  pRegs:   Pointer to host controller registers             ***
***              pBuffer: Pointer to buffer where to store data            ***
***                                                                        ***
*** Return:      TRUE: Success, FALSE: Failure                             ***
***                                                                        ***
*** Description                                                            ***
*** -----------                                                            ***
*** Wait until data transfer is done, read the required number of bytes    ***
*** and store them in the given buffer.                                    ***
*****************************************************************************/
static bool SDHC_ReadOneBlock(SDHC_REGS *pRegs, U32 *pBuffer)
{
    unsigned int uCount;

    if (!SDHC_IntWaitAndClear(pRegs, SDHC_BUFFER_READREADY_SIG_INT_EN))
        return FALSE;                     /* Timeout */

    uCount = (pRegs->BLK_SIZE << 20) >> 22;

    while (uCount)
    {
        *pBuffer++ = pRegs->BUF_DAT_PORT;
        uCount--;
    }

    return TRUE;
}


#ifdef __USE_ISSUE_AND_READ__
/*****************************************************************************
*** Function:    bool SDHC_IssueCommandReadBlock(SDHC_REGS *pRegs, U16     ***
***                                  uCmd, U32 uArg, U32 uBlockSize, U32   ***
***                                  *pBuffer)                             ***
***                                                                        ***
*** Parameters:  pRegs:      Pointer to the host controller registers      ***
***              uCmd:       Command to issue; if SDHC_ACMD is set, issue  ***
***                          CMD55 (APPL_CMD) before.                      ***
***              uArg:       Argument for command; if SDHC_ACMD is set,    ***
***                          this must be the RCA to use in CMD55          ***
***              uBlockSize: Size of the response data block               ***
***              pBuffer:    Pointer where to store read data              ***
***                                                                        ***
*** Return:      TRUE: Success, FALSE: Failure                             ***
***                                                                        ***
*** Description                                                            ***
*** -----------                                                            ***
*** Set the BLOCKLEN for the following transfer on the card and in the     ***
*** host controller, then issue the given command (maybe prefixed by CMD55 ***
*** if SDHC_CMD is set). Then wait until command is successfully           ***
*** processed.                                                             ***
*****************************************************************************/
static bool SDHC_IssueCommandReadBlock(SDHC_REGS *pRegs, U16 uCmd, U32 uArg,
                                       U32 uBlockSize, U32 *pBuffer)
{
    /* CMD16 (SET_BLOCKLEN): set block len to given block size */
    if (!SDHC_IssueCommand(pRegs, 16, 8, SDHC_CMD_AC_TYPE,
                           SDHC_RES_R1_TYPE))
        return FALSE;

    /* Set maximum DMA buffer size (512K) and given block size */
    pRegs->BLK_SIZE = (7<<12) | uBlockSize;

    /* Single block select, read transfer, disable Auto CMD12, disable
       block count, disable DMA */
    pRegs->TRANS_MODE = (0<<5) | (1<<4) | (0<<2) | (0<<1) | (0<<0);

    /* Do we have ACMD instead of CMD? */
    if (uCmd & SDHC_ACMD)
    {
        /* Issue CMD 55 (APP_CMD): next command is application command; uArg
           is the relative card address (RCA) */
        if (!SDHC_IssueCommand(pRegs, 55, uArg, SDHC_CMD_AC_TYPE,
                               SDHC_RES_R1_TYPE))
            return FALSE;
        uCmd &= ~SDHC_ACMD;
        uArg = 0;                         /* Main command has no argument */
    }

    /* Issue main command */
    if (!SDHC_IssueCommand(pRegs, uCmd, uArg, SDHC_CMD_ADTC_TYPE,
                           SDHC_RES_R1_TYPE))
        return FALSE;

    /* Read the SCR data */
    if (!SDHC_ReadOneBlock(pRegs, pBuffer))
        return FALSE;                     /* Timeout */

    if (!SDHC_IntWaitAndClear(pRegs, SDHC_TRANSFERCOMPLETE_SIG_INT_EN))
        return FALSE;                     /* Timeout */

    return TRUE;
}
#endif /*__USE_ISSUE_AND_READ__*/


/* ------------ Implementation Of Library Internal Functions --------------- */

/*****************************************************************************
*** Function:    bool SDHC_IntWaitAndClear(SDHC_REGS *pRegs, U16 uIntMask) ***
***                                                                        ***
*** Parameters:  pRegs:    Pointer to host controller registers            ***
***              uIntMask: Mask of interrupts to check (usually only one   ***
***                        bit set)                                        ***
***                                                                        ***
*** Return:      TRUE: Success, FALSE: Failure (timeout)                   ***
***                                                                        ***
*** Description                                                            ***
*** -----------                                                            ***
*** If the given interrupt status is active, clear the status.             ***
*****************************************************************************/
bool SDHC_IntWaitAndClear(SDHC_REGS *pRegs, U16 uIntMask)
{
    U32 uStartCycleCount = get_timer(0);

    do
    {
        if (pRegs->NORMAL_INT_STAT & uIntMask)
        {
            do  //#### 17.08.2010 HK: Why the loop?
            {
                pRegs->NORMAL_INT_STAT = uIntMask;
            } while (pRegs->NORMAL_INT_STAT & uIntMask);
            return TRUE;
        }
    } while (get_timer(uStartCycleCount) < SDHC_TIMEOUT);

    return FALSE;
}


/*****************************************************************************
*** Function:    bool SDHC_IssueCommand(SDHC_REGS *pRegs, U16 uCmd, U32    ***
***                                  uArg, SDHC_CommandType cType,         ***
***                                  SDHC_ResponseType rType)              ***
***                                                                        ***
*** Parameters:  pRegs: Pointer to host controller registers               ***
***              uCmd:  Command number to issue                            ***
***              uArg:  Argument of command                                ***
***              cType: Command type                                       ***
***              rType: Resonse type                                       ***
***                                                                        ***
*** Return:      TRUE: Success, FALSE: Failure                             ***
***                                                                        ***
*** Description                                                            ***
*** -----------                                                            ***
*** Wait until the CMD line is free, then issue the given command. If the  ***
*** resonse may result in a busy state, wait until busy is over.           ***
*****************************************************************************/
bool SDHC_IssueCommand(SDHC_REGS *pRegs, U16 uCmd, U32 uArg,
                       SDHC_CommandType cType, SDHC_ResponseType rType)
{
    U16 sfrData;

    /* Check Command Inhibit (CMD): Wait until CMD line is free */
    while (pRegs->PRESENT_STAT & 0x1)
        ;

    /* Set the command number and the response type */
    sfrData = (uCmd<<8) | SDHC_cmd_sfr_data[rType];

    /* Set "Data Present" on commands with data */
    if (cType == SDHC_CMD_ADTC_TYPE)
        sfrData |= (1<<5);

    /* CMD12 (STOP_TRANSMISSION) has to set command type "Abort" */
    if (uCmd == 12)
        sfrData |= (3<<6);

    /* Set the argument */
    pRegs->ARG = uArg;

    /* Start the command */
    pRegs->COMMAND = sfrData;

    /* Wait until transfer and command complete */
    if (!SDHC_IntWaitAndClear(pRegs, SDHC_COMMANDCOMPLETE_SIG_INT_EN))
        return FALSE;                     /* Timeout */

    /* Error Status Check - reduce too much error message */
    if (pRegs->NORMAL_INT_STAT & (1<<15))
    {
#if 0
        if ((uCmd != 1) && (uCmd != 55) && (uCmd != 41))
            SDDBG("Command %d, ErrorStat 0x%x\n", uCmd, pRegs->ERROR_INT_STAT);
#endif
        return FALSE;
    }

    if (rType == SDHC_RES_R1B_TYPE)
    {
        /* In case of a R1b response, the card may get busy. This is indicated
           on the DAT line. Check Command Inhibit (DAT): Wait until DAT line
           is again free, i.e. card is not busy anymore. */
        while (pRegs->PRESENT_STAT & (1<<1))
            ;
    }

    return TRUE;
}


/*****************************************************************************
*** Function:    bool SDHC_WaitForCard2TransferState(SDHC_REGS *pRegs, U32 ***
***                                  uRca)                                 ***
***                                                                        ***
*** Parameters:  pRegs: Pointer to host controller registers               ***
***              uRca:  Relative card address (in upper 16 bits)           ***
***                                                                        ***
*** Return:      TRUE: Success, FALSE: Failure                             ***
***                                                                        ***
*** Description                                                            ***
*** -----------                                                            ***
*** Wait until the card is back in transfer state (SDHC_CS_TRAN).          ***
*** Especially wait until all write transfers and the programming is done. ***
*****************************************************************************/
bool SDHC_WaitForCard2TransferState(SDHC_REGS *pRegs, U32 uRca)
{
    SDHC_CurrentState eCardState;

    /* Wait until card has completely received and programmed the data */
    do
    {
        /* CMD13 (SEND_STATUS) */
        if (!SDHC_IssueCommand(pRegs, 13, uRca, SDHC_CMD_AC_TYPE,
                               SDHC_RES_R1B_TYPE))
            return FALSE;

        /* The CURRENT_STATE of the card is contained in bits[12:9] */
        eCardState = (pRegs->RSP0 << 19) >> 28;
    } while ((eCardState == SDHC_CS_PRG) || (eCardState == SDHC_CS_RCV));

    /* After programming, the card should return to transfer state */
    if (eCardState != SDHC_CS_TRAN)
        return FALSE;

    return TRUE;
}


/* ------------ Implementation Of Exported Functions ----------------------- */

/*****************************************************************************
*** Function:    bool SDHC_OpenMedia(SDHC *sCh, SDHC_channel eChannel,     ***
***                                  SDHC_Buswidth eBuswidth,              ***
***                                  SDHC_clockSource eClockSource)      ***
***                                                                        ***
*** Parameters:  sCh:           Pointer to SD context                      ***
***              eChannel:      SD/MMC channel to use                      ***
***              eBuswidth:     Buswidth to use                            ***
***              eClockSource:  Clock source for SDCLK                     ***
***                                                                        ***
*** Return:      TRUE: Success, FALSE: Failure                             ***
***                                                                        ***
*** Description                                                            ***
*** -----------                                                            ***
*** Configure the SD/MMC host controller (GPIOs, signal strength, clocks,  ***
*** buswidth, etc.), check the type of the inserted card and read required ***
*** data. After return, the SDHC structure is filled and data can be read  ***
*** or written from/to the card.                                           ***
*****************************************************************************/
bool SDHC_OpenMedia(SDHC *sCh, SDHC_Channel eChannel, SDHC_Buswidth eBuswidth,
                    SDHC_ClockSource eClockSource)
{
    U32 uSrcFreq;
    int shift;
    U32 uRsp2, uRsp1;
    U32 pBuffer[512/4];
    SDHC_DriverStrength eStrength;
    SDHC_REGS *pRegs;
    SDHC_CardType eCardType;

    /* Store buswidth, channel and get appropriate base address */
    pRegs = ChannelBaseAddr[eChannel];
    sCh->m_pRegs = pRegs;

    /* ---- Set the SD clock source ---- */
    shift = eChannel*2 + 18;
    switch (eClockSource)
    {
      default:
      case SDHC_HCLK:
	  uSrcFreq = get_HCLK();
	  break;

      case SDHC_EXTCLK:
          uSrcFreq = 0;
          break;

      case SDHC_EPLL:
	  CLK_SRC_REG = (CLK_SRC_REG & ~(0x3 << shift)) | (0 << shift);
          uSrcFreq = get_UCLK();
	  break;

      case SDHC_MPLL:
	  CLK_SRC_REG = (CLK_SRC_REG & ~(0x3 << shift)) | (1 << shift);
          uSrcFreq = get_MCLK();
	  break;

      case SDHC_FIN:
	  CLK_SRC_REG = (CLK_SRC_REG & ~(0x3 << shift)) | (2 << shift);
	  uSrcFreq = 12000000;
	  break;

      case SDHC_27M:
	  CLK_SRC_REG = (CLK_SRC_REG & ~(0x3 << shift)) | (3 << shift);
	  uSrcFreq = 27000000;
	  break;
    }

    /* ---- Configure GPIOs for SD card and set signal drive strength ---- */
    eStrength = SDHC_STRENGTH_9MA;
    switch (eChannel)
    {
      case SDHC_CHANNEL_0:                /* External SD slot */
      default:
          /* Set GPG to SD card signals: GPG0: CLK0, GPG1: CMD0, GPG6: CD0,
             GPG5..GPG2: DATA0[3..0] */
          GPGCON_REG = 0x02222222;
          GPGPUD_REG = 0;

          /* Set line driver strength for data lines and clock line */
          SPCON_REG = (SPCON_REG & ~(0x3<<26)) | (eStrength<<26); /* Data */
          pRegs->CONTROL4 = (0x3<<16);                      /* Clock */
          break;

      case SDHC_CHANNEL_1:                /* Internal micro SD slot */
          /* Set GPH to SD card signals: GPH0: CLK1, GPH1: CMD1,
             GPH9..GPH2: DATA1[7..0]. This port is theoretically capable of 1,
             4, and 8 bit bus width, but our slot uses only 4 bits, the
             upper 4 bits are fixed to high. */
          GPHCON0_REG = 0x22222222;
          GPHCON1_REG = 0x22;
          GPHPUD_REG = 0;

          /* Set line driver strength for data lines and clock line */
          SPCON_REG = (SPCON_REG & ~(0x3<<26)) | (eStrength<<26); /* Data */
          pRegs->CONTROL4 = (0x3<<16);                      /* Clock */
          break;

      case SDHC_CHANNEL_2:                /* 1 + 4 bits width */
#if 0     /* Not used */
          /* GPC4: CMD2, GPC5: CLK2, GPH9..GPH6: DATA2[3..0]; this slot is
             only capable for 1 and 4 bit bus width */
          /* ### TODO ### */

          /* Set line driver strength for data lines and clock line. There is
             no SELCLKPADDS value in CONTROL4 for channel 2, but instead the
             setting for SPICLK1 in SPCON is used; therefore both data and
             clock strength are set in SPCON */
          rSPCON = (rSPCON & ~((0x3<<26)|(0x3<<18))) | (eStrength<<26)
                                                     | (eStrength<<18);
#endif
          break;
    }

    /* Enable the SD card power */
    mmc_s3c64xx_board_power(eChannel);

    /* ---- Reset controller and wait for reset to finish ---- */
    pRegs->SOFTWARE_RESET = 0x7;
    while (pRegs->SOFTWARE_RESET & 0x7)
        ;

    /* ---- Select and activate power ---- */
    pRegs->PWR_CTRL = (0x7<<1);
    pRegs->PWR_CTRL |= 0x1;

    /* ---- Set a preliminary SDCLOCK clock speed to identify card ---- */
    /* Set the clock source */
    pRegs->CONTROL2 = (eClockSource<<4);

    /* Set timeout to TMCLK * 2^27 */
    pRegs->TIMEOUT_CTRL = (pRegs->TIMEOUT_CTRL & ~0xF) | 0xe;

    /* During card identification mode, the frequency must be below 400kHz */
    SDHC_SetHostSDCLK(pRegs, 400000, uSrcFreq, SDHC_SD_CARD);

    /* ---- Setup interrupt status flags  ---- */
    /* Clear all pending error interrupt status flags, then enable all status
       flags, but disable all interrupts */
    while (pRegs->NORMAL_INT_STAT & (1<<15))
        pRegs->ERROR_INT_STAT = pRegs->ERROR_INT_STAT;
    pRegs->NORMAL_INT_STAT_ENABLE = 0xFF;
    pRegs->ERROR_INT_STAT_ENABLE = 0xFF;
    pRegs->NORMAL_INT_SIGNAL_ENABLE = 0;
    pRegs->ERROR_INT_SIGNAL_ENABLE = 0;

    /* ---- Identify Card ---- */
    /* Check card Operation Condition Register (OCR) */
    sCh->m_uRca = (0<<16);                /* Relative card address 0 */
    if (SDHC_SetSDOCR(sCh))
        eCardType = SDHC_SD_CARD;
#ifndef PROGRAMSD
    else if (SDHC_SetMmcOcr(sCh))
    {
        eCardType = SDHC_MMC_CARD;
        sCh->m_uRca = (1<<16);            /* Default Relative card address is
                                             1 for MMC cards */
    }
#endif
    else
        return FALSE;

#ifndef PROGRAMSD
    sCh->m_eCardType = eCardType;
#endif

    /* ---- Get Card Identification (CID) ---- */
    /* The card is now in SDHC_CS_READY state. Issue CMD2 (ALL_SEND_CID):
       this brings the card to SDHC_CS_IDENT state and returns the Card
       Identification register (CID). */
    SDHC_IssueCommand(pRegs, 2, 0, SDHC_CMD_BCR_TYPE, SDHC_RES_R2_TYPE);

#ifndef PROGRAMSD
    {
        U32 uRsp0, uRsp3;
        /* Read the CID data and store them in the SDHC structure (for later
           text output). The CID is 128bits, but the last 8 bits are CRC7 and
           stop bit, which are already removed by the controller. Therefore
           data is in RSP3[23:0] and in RSP2..RSP0. */
        uRsp3 = pRegs->RSP3;
        sCh->m_chMID = (U8)(uRsp3>>16);
        sCh->m_chOID[0] = (U8)(uRsp3>>8);
        sCh->m_chOID[1] = (U8)uRsp3;
        sCh->m_chOID[2] = 0;
        uRsp2 = pRegs->RSP2;
        sCh->m_chPNM[0] = (U8)(uRsp2>>24);
        sCh->m_chPNM[1] = (U8)(uRsp2>>16);
        sCh->m_chPNM[2] = (U8)(uRsp2>>16);
        sCh->m_chPNM[3] = (U8)(uRsp2>>16);
        uRsp1 = pRegs->RSP1;
        sCh->m_chPNM[4] = (U8)(uRsp2>>24);
        sCh->m_chPNM[5] = 0;
        sCh->m_chPRV[0] = ((uRsp1>>20) & 0xF) + '0';
        sCh->m_chPRV[1] = '.';
        sCh->m_chPRV[2] = ((uRsp1>>16) & 0xF) + '0';
        sCh->m_chPRV[3] = 0;
        uRsp0 = pRegs->RSP0;
        sCh->m_uPSN = (uRsp1 << 16) | (uRsp0 >> 16);
        sCh->m_uMDT = uRsp0 & 0xFFF;
    }
#endif

    /* Issue CMD3. This is SET_RELATIVE_ADDR on MMC cards and the sent RCA (in
       our case 1) is set for the card. This command has no response. But it
       is SEND_RELATIVE_ADDR on SD cards (with argument set to 0) and the
       command has a result with the RCA to use for this card. In both cases,
       the card is placed into SDHC_CS_STDBY state, basically switching from
       card-indentification mode to data-transfer mode. */
    SDHC_IssueCommand(pRegs, 3, sCh->m_uRca, SDHC_CMD_AC_TYPE,
                      SDHC_RES_R1_TYPE);
    if (eCardType == SDHC_SD_CARD)
        sCh->m_uRca = pRegs->RSP0 & 0xFFFF0000; /* Get RCA on SD cards */

    /* ---- Get the Card Specific Data (CSD) ---- */
    /* Issue CMD9 (SEND_CSD): get Card Specific Data (CSD); the result is a
       response with 128 bits; as bits 7..0 are CRC7 and stop bit and already
       removed by the controller, the data is in RSP3[23:0] and RSP2..RSP0. */
    SDHC_IssueCommand(pRegs, 9, sCh->m_uRca, SDHC_CMD_AC_TYPE,
                      SDHC_RES_R2_TYPE);

#ifndef PROGRAMSD
    if (eCardType == SDHC_MMC_CARD)
    {
        /* On MMC cards, the spec version is coded in bits [125:122]
           (RSP3[21:18]) */
        sCh->m_ucSpecVer = (U8)((pRegs->RSP3 << 10) >> 28);
    }
#endif

    /* max. read block length (READ_BL_LEN) is in bits [83:80] (RSP2[11:8]);
       Allowed values: 9: 512 bytes, 10: 1024 bytes, 11: 2048 bytes */
    uRsp2 = pRegs->RSP2;
    sCh->m_sReadBlockLen = (U16)((uRsp2 << 20) >> 28);

    /* Flag if partial reads are allowed (READ_BL_PARTIAL) is in bit [79]
       (RSP2[7]) */
    sCh->m_sReadBlockPartial = (U16)((uRsp2 << 24) >> 31);

    uRsp1 = pRegs->RSP1;
    /* C_SIZE is in bits [73:62] (RSP2[1:0] and RSP1[31..22] */
    sCh->m_sCSize = (U16)(((uRsp2 << 30) >> 20) | (uRsp1 >> 22));

    /* C_SIZE_MULT is in bits [49:47] (RSP1[9:7]) */
    sCh->m_sCSizeMult = (U16)((uRsp1 << 22) >> 29);


    /* OK, card is identified; CMD7 (SELECT_CARD); this puts the card in
       SDHC_CS_TRAN state where data transfer commands can be issued */
    if (!SDHC_IssueCommand(pRegs, 7, sCh->m_uRca, SDHC_CMD_AC_TYPE,
                           SDHC_RES_R1B_TYPE))
        return FALSE;

    /* ---- Set final card speed ---- */
    sCh->m_eSpeedMode = SDHC_NORMAL_SPEED;
#ifndef PROGRAMSD
    if (eCardType == SDHC_MMC_CARD)
    {
        if (sCh->m_ucSpecVer >= 4)
        {
            U8 *p;

            /* ---- Read Extended CSD for MMC card ---- */
#ifdef __USE_ISSUE_AND_READ__
            /* Issue CMD8 (SEND_EXT_CSD): get the extended CSD of the MMC
               card; this also sets the final block len to 512 bytes */
            if (!SDHC_IssueCommandReadBlock(pRegs, 8, 0, 512, pBuffer))
                return FALSE;
#else
            /* CMD16 (SET_BLOCKLEN): set back to standard 512 bytes. */
            if (!SDHC_IssueCommand(pRegs, 16, 512, SDHC_CMD_AC_TYPE,
                                   SDHC_RES_R1_TYPE))
                return FALSE;

            /* Set maximum DMA buffer size (512K), block size 512 bytes */
            pRegs->BLK_SIZE = (7<<12) | 512;

            /* Single block select, read transfer, disable Auto CMD12, disable
               block count, disable DMA */
            pRegs->TRANS_MODE = (0<<5) | (1<<4) | (0<<2) | (0<<1) | (0<<0);

            /* CMD8 (SEND_EXT_CSD): get the extended CSD of the MMC card */
            if (!SDHC_IssueCommand(pRegs, 8, 0, SDHC_CMD_ADTC_TYPE,
                                   SDHC_RES_R1_TYPE))
                return FALSE;

            /* Read answer (512 bytes of SD Memory Card interface condition) */
            if (!SDHC_ReadOneBlock(pRegs, pBuffer))
                return FALSE;             /* Timeout */

            if (!SDHC_IntWaitAndClear(pRegs, SDHC_TRANSFERCOMPLETE_SIG_INT_EN))
                return FALSE;             /* Timeout */
#endif /*__USE_ISSUE_AND_READ__*/

            /* Read some relevant data and store it in SDHC structure; this
               allows text output later */
            p = (U8 *)pBuffer;
            sCh->m_chErasedMemContent = p[181];
            sCh->m_chBusWidthTiming = p[183];
            sCh->m_chHighSpeedTiming = p[185];
            sCh->m_chPowerClass = p[187];
            sCh->m_chCommandSetRev = p[189];
            sCh->m_chCommandSet = p[191];
            sCh->m_chExtCsdRev = p[192];
            sCh->m_chCsdStructVers = p[194];
            sCh->m_chCardType = p[196];
            sCh->m_chMinReadPerf4_26 = p[205];
            sCh->m_chMinWritePerf4_26 = p[206];
            sCh->m_chMinReadPerf8_26 = p[207];
            sCh->m_chMinWritePerf8_26 = p[208];
            sCh->m_chMinReadPerf8_52 = p[209];
            sCh->m_chMinWritePerf8_52 = p[210];
            sCh->m_uSectorCount = *(U32*)&p[212];
            sCh->m_chSupportedCmdSet = p[504];

            /* A standard MMC card can operate up to 26MHz. The possible speed
               is indicated by the card type[1:0]: 0b01: 26MHz, 0b11: 52MHz;
               other values are currently not defined. */
            if (sCh->m_chCardType & (1<<1))
                sCh->m_eSpeedMode = SDHC_HIGH_SPEED;

            /* CMD6 (SWITCH): Set MMC speed (normal or high speed) by setting
               a new value (0= normal speed, 1= high speed) into the
               HS_TIMING entry (at index 185) of the EXT_CSD block. The
               argument of SWITCH is:
                [31:26]: set to 0b000000
                [25:24]: Access: 0b00: Command Set, 0b01: Set 1-bits of Value,
                         0b10: Clear 1-bits of Value, 0b11: Write Value
                [23:16]: Index in EXT_CSD (0..191)
                [15:8]:  Value (set on access == 0b11, combined with previous
                                content if access == 0b01 or 0b10)
                [7:3]:   set to 0b00000
                [2:0]:   Command set (ignored if access is != 0b00) */
            if (!SDHC_IssueCommand(pRegs, 6,
                                   (3<<24)|(185<<16)|(sCh->m_eSpeedMode<<8),
                                   SDHC_CMD_AC_TYPE, SDHC_RES_R1B_TYPE))
                return FALSE;
        }
    }
    else if (eCardType == SDHC_SD_CARD)
#endif /*!PROGRAMSD*/
    {
        U32 uArg;

        /* ---- Get SD Card Configuration Register (SCR) ---- */
#ifdef __USE_ISSUE_AND_READ__
        if (!SDHC_IssueCommandReadBlock(pRegs, SDHC_ACMD | 51, sCh->m_uRca, 8,
                                        pBuffer))
            return FALSE;
#else
        /* CMD16 (SET_BLOCKLEN) set block len to 8 bytes (64 bits response) */
        if (!SDHC_IssueCommand(pRegs, 16, 8, SDHC_CMD_AC_TYPE,
                               SDHC_RES_R1_TYPE))
            return FALSE;

        pRegs->BLK_SIZE = (7<<12) | 8;

        /* Single block select, read transfer, disable Auto CMD12, disable
           block count, disable DMA */
        pRegs->TRANS_MODE = (0<<5) | (1<<4) | (0<<2) | (0<<1) | (0<<0);

        /* CMD55 (APP_CMD): next command is application command */
        if (!SDHC_IssueCommand(pRegs, 55, sCh->m_uRca, SDHC_CMD_AC_TYPE,
                               SDHC_RES_R1_TYPE))
            return FALSE;

        /* ACMD51 (SEND_SCR) */
        if (!SDHC_IssueCommand(pRegs, 51, 0, SDHC_CMD_ADTC_TYPE,
                               SDHC_RES_R1_TYPE))
            return FALSE;

        /* Read the SCR data */
        if (!SDHC_ReadOneBlock(pRegs, pBuffer))
            return FALSE;                 /* Timeout */

        if (!SDHC_IntWaitAndClear(pRegs, SDHC_TRANSFERCOMPLETE_SIG_INT_EN))
            return FALSE;                 /* Timeout */
#endif

        /* The supported bus widths are set in bits [51:48] (pBuffer[0][11:8]):
           Bit 48: 1 bit (DAT0), Bit 49: reserver, Bit 50: 4 bit (DAT3..0),
           Bit 51: reserved */
        if ((pBuffer[0] & 0x400) && (eBuswidth > SDHC_BUSWIDTH1))
            eBuswidth = SDHC_BUSWIDTH4; /* 4 bit width supported */
        else
            eBuswidth = SDHC_BUSWIDTH1; /* only 1 bit width supported */

        /* The SD Mem Card Spec. Version is in bits [59:56] (pBuffer[0][3..0]):
           0: Version 1.0 ~ 1.01, 1: Version 1.10, 2: Verison 2.00 */
        sCh->m_ucSpecVer = pBuffer[0] & 0xf;
        switch (sCh->m_ucSpecVer)
        {
          default:
              sCh->m_ucSpecVer = 0;
              /* Fall through to case 0 */
          case 0:
              sCh->m_eSpeedMode = SDHC_NORMAL_SPEED;
              break;

          case 1:                         /* Vers. 1.1 */
          case 2:                         /* Vers. 2.0 */
              /* Cards with V1.1 or V2.0 can theoretically support high speed */
#ifdef __USE_ISSUE_AND_READ__
              /* Prepare argument for the following SWITCH_FUNCTION command:
                 Mode 1 (=SWITCH_FUNCTION), 0xF (=don't change) for function
                 groups 3-6, 0 (=default function) for function group 2
                 (command system) and 0 (=default) or 1 (=high speed) for
                 function group 1 (access mode) */
              if (uSrcFreq > SDHC_SD_HIGH_SPEED_CLOCK)
                  uArg = SDHC_HIGH_SPEED;
              else
                  uArg = SDHC_NORMAL_SPEED;
              uArg |= (1U<<31)|(0xffff<<8);
              if (!SDHC_IssueCommandReadBlock(pRegs, 6, uArg, 64, pBuffer))
                  return FALSE;
#else
              /*#### 17.08.2010 HK: PHYSICAL LAYER Simplified Specification
                sagt, dass das Setzen der Blocklänge für CMD6 nicht notwendig
                ist, da die Länge auf 512bit (64 Bytes) fest vorgegeben ist.
                Das könnte hier also evtl. weg. */
              /* CMD16 (SET_BLOCKLEN) set block len to 64 bytes */
              if (!SDHC_IssueCommand(pRegs, 16, 64, SDHC_CMD_AC_TYPE,
                                     SDHC_RES_R1_TYPE))
                  return FALSE;

              /* Set host controller to receive 64 bytes */
              pRegs->BLK_SIZE = (7<<12) | 64;

              /* Single block select, read transfer, disable Auto CMD12,
                 disable block count, disable DMA */
              pRegs->TRANS_MODE = (0<<5)|(1<<4)|(0<<2)|(0<<1)|(0<<0);

              /* Prepare argument for the following SWITCH_FUNCTION command:
                 Mode 1 (=SWITCH_FUNCTION), 0xF (=don't change) for function
                 groups 3-6, 0 (=default function) for function group 2
                 (command system) and 0 (=default) or 1 (=high speed) for
                 function group 1 (access mode) */
              if (uSrcFreq > SDHC_SD_HIGH_SPEED_CLOCK)
                  uArg = SDHC_HIGH_SPEED;
              else
                  uArg = SDHC_NORMAL_SPEED;
              uArg |= (1U<<31)|(0xffff<<8);

              /* Issue CMD6 (SWITCH_FUNCTION) to set and query transfer speed */
              if (!SDHC_IssueCommand(pRegs, 6, uArg, SDHC_CMD_ADTC_TYPE,
                                     SDHC_RES_R1_TYPE))
                  return FALSE;

              /* Read the result (512 bit status) */
              if (!SDHC_ReadOneBlock(pRegs, pBuffer))
                  return FALSE;           /* Timeout */
              if (!SDHC_IntWaitAndClear(pRegs, SDHC_TRANSFERCOMPLETE_SIG_INT_EN))
                  return FALSE;           /* Timeout */
#endif /*__USE_ISSUE_AND_READ__*/

              /* Check function group 1 (access mode); if bit 1 is set
                 (overall bit #401 where #511 is bit 0 in pBuffer[0]), high
                 speed is supported. */
              if (pBuffer[3] & (1<<9))
                  sCh->m_eSpeedMode = SDHC_HIGH_SPEED;
              break;
        } /* switch */
    } /* eCardType == SDHC_SD_CARD */

    /* ---- Switch also final host speed (SDCLK clock speed) ---- */

    /* Compute the final operating speed (the maximum clock that is not higher
       than 25MHz or 50MHz respectively) */
    /* Store final operating frequency */
    sCh->m_uOperFreq = SDHC_SetHostSDCLK(pRegs,
                                         (sCh->m_eSpeedMode == SDHC_HIGH_SPEED)
                                         ? (2*SDHC_SD_HIGH_SPEED_CLOCK)
                                         : SDHC_SD_HIGH_SPEED_CLOCK, uSrcFreq,
                                         eCardType);

    /* ---- Set the Bus Width ---- */
    pRegs->NORMAL_INT_STAT_ENABLE &= ~0x0100;

    /* Issue command to card to switch bus width */
    if (eCardType == SDHC_SD_CARD)
    {
        /* SD cards don't support 8 bits buswidth */
        if (eBuswidth == SDHC_BUSWIDTH8)
            eBuswidth = SDHC_BUSWIDTH4;

        /* SD card: CMD55 (APP_CMD) Next command is Application Command */
        if (!SDHC_IssueCommand(pRegs, 55, sCh->m_uRca, SDHC_CMD_AC_TYPE,
                               SDHC_RES_R1_TYPE))
            return FALSE;

        /* ACMD6 - Set Bus Width. 0->1bit, 2->4bit */
        if (!SDHC_IssueCommand(pRegs, 6,
                               (eBuswidth == SDHC_BUSWIDTH1) ? 0 : 2,
                               SDHC_CMD_AC_TYPE, SDHC_RES_R1_TYPE))
            return FALSE;
    }
    else                                  /* SDHC_MMC_CARD */
    {
        /* MMC card */
        if (sCh->m_ucSpecVer >= 4)
        {
            /* Newer MMC cards support also 4 and 8 bit buswidth */

            /* Refer to p.37~38, p.53~54 & p.83 of "MMC Card system Spec.
               ver4.0", refer to Lee's Spec and Add for 8-bit mode */
            if (!SDHC_IssueCommand(pRegs, 6,
                                   MmcBuswidth[eBuswidth] | (3<<24)|(183<<16),
                                   SDHC_CMD_AC_TYPE, SDHC_RES_R1B_TYPE))
                return FALSE;
        }
        else
        {
            /* Older MMC card, only 1 bit, no need to issue command */
            eBuswidth = SDHC_BUSWIDTH1;
        }

        /* The bus width is already set to 512 bytes from CMD8 (SEND_EXT_CSD)
           above; we don't need to set it here */
    }

    /* ---- Switch host to the same bus width ---- */
    pRegs->HOST_CTRL = (pRegs->HOST_CTRL & ~((1<<5)|(1<<1)))
                                                     | HostBuswidth[eBuswidth];
#ifndef PROGRAMSD
    /* Set finally used bus width */
    sCh->m_eBuswidth = eBuswidth;
#endif

    /* Enable normal card interrupt status */
    pRegs->NORMAL_INT_STAT_ENABLE |= 0x0100;

    /* Wait until card is back in SDHC_CS_TRAN state */
    if (!SDHC_WaitForCard2TransferState(pRegs, sCh->m_uRca))
        return FALSE;

    /* On MMC card, the block length is already set to 512 bytes from CMD8
       (SEND_EXT_CSD) above, but on SD card, the length is still set to 64
       bytes from CMD6 (SWITCH_FUNCTION) above. */
    if (eCardType == SDHC_SD_CARD)
    {
        /* CMD16 (SET_BLOCKLEN): set back to standard 512 bytes. */
        if (!SDHC_IssueCommand(pRegs, 16, 512, SDHC_CMD_AC_TYPE,
                               SDHC_RES_R1_TYPE))
            return FALSE;
    }

    /* Do debouncing of card slot */
    pRegs->CONTROL2 |= (1<<8) | (2<<9) | (1<<28);

    return TRUE;
}


/*****************************************************************************
*** Function:    void SDHC_CloseMedia(SDHC *sCh)                           ***
***                                                                        ***
*** Parameters:  sCh: Pointer to SD context                                ***
***                                                                        ***
*** Return:      -                                                         ***
***                                                                        ***
*** Description                                                            ***
*** -----------                                                            ***
*** Close the given SD/MMC card. This is currently done by stopping the    ***
*** SDCLK.                                                                 ***
*****************************************************************************/
void SDHC_CloseMedia(SDHC *sCh)
{
    /* Disable SDCLK and internal clock; we could also remove the SD power,
       but as we don't have the SD/MMC channel available here, this is too
       much effort. */
    sCh->m_pRegs->CLK_CTRL &= ~((1<<2) | (1<<0));
}


static unsigned long mmc_bread(int dev_num, unsigned long blknr,
			       lbaint_t blkcnt, void *dst)
{
	if (dev_num >= MAX_UNITS)
		blkcnt = 0;

	if (blkcnt > 0)
	{
		SDHC *sCh = &mmc_unit[dev_num];
		SDHC_REGS *pRegs = sCh->m_pRegs;
		unsigned int i;

		/* Wait until card is in transfer state */
		if (!SDHC_WaitForCard2TransferState(pRegs, sCh->m_uRca))
			return 0;

		if (sCh->m_eTransMode == SDHC_BYTE_MODE)
			blknr = blknr<<9;       /* blknr * 512 */;

		/* Set maximum DMA buffer size (512K), block size 512 bytes */
		pRegs->BLK_SIZE = (7<<12) | 512;
		pRegs->BLK_COUNT = blkcnt;

		if (blkcnt == 1)
		{
			/* Single block select, read transfer, disable Auto
			   CMD12, enable block count, disable DMA */
			pRegs->TRANS_MODE =
				(0<<5) | (1<<4) | (0<<2) | (1<<1) | (0<<0);

			/* Use CMD17 READ_SINGLE_BLOCK command */
			if (!SDHC_IssueCommand(pRegs, 17, blknr,
					       SDHC_CMD_ADTC_TYPE,
					       SDHC_RES_R1_TYPE))
				return 0; /* CMD17 failed */
		}
		else
		{
			/* Multi block select, read transfer, enable Auto
			   CMD12, enable block count, disable DMA */
			pRegs->TRANS_MODE =
				(1<<5) | (1<<4) | (1<<2) | (1<<1) | (0<<0);

			/* Use CMD18 (READ_MULTIPLE_BLOCK) */
			if (!SDHC_IssueCommand(pRegs, 18, blknr,
					       SDHC_CMD_ADTC_TYPE,
					       SDHC_RES_R1_TYPE))
				return 0;             /* CMD18 failed */
		}

		i = blkcnt;
		do
		{
			if (!SDHC_ReadOneBlock(pRegs, (U32 *)dst))
				return FALSE;             /* Timeout */
			dst += 512;
		} while (--i > 0);

		/* Wait for Transfer Complete */
		if (!SDHC_IntWaitAndClear(pRegs,
					  SDHC_TRANSFERCOMPLETE_SIG_INT_EN))
			return 0;                 /* Timeout */
	}

	return blkcnt;
}

U32 SDCardGetMediaSize(SDHC *sCh)
{
    /* Get the number of blocks of size sCh->m_sReadBlockLen */
    return (sCh->m_sCSize+1) * (1U << (sCh->m_sCSizeMult+2));
}

int mmc_init(int verbose)
{
	SDHC_Channel i;
	SDHC *sCh;

	/* Activate 48MHz clock, this is not only required for USB OTG, but
	   also for MMC */
	OTHERS_REG |= 1<<16;		  /* Enable USB signal */
	__REG(S3C_OTG_PHYPWR) = 0x00;
	__REG(S3C_OTG_PHYCTRL) = 0x00;
	__REG(S3C_OTG_RSTCON) = 1;
	udelay(10);
	__REG(S3C_OTG_RSTCON) = 0;

	/* Check for all available MMC slots if there is a card inserted */
	for (i=0; i<MAX_UNITS; i++) {
		if (verbose)
			printf("mmc %d: ", i);
		sCh = &mmc_unit[i];
		if (SDHC_OpenMedia(sCh, mmc_channel[i], SDHC_BUSWIDTH4,
				   SDHC_EPLL)) {
			U32 uBlocks;

			uBlocks = SDCardGetMediaSize(sCh);
			if (verbose) {
				printf("%s, %uHz, %dMB\n",
				       pCardTypes[sCh->m_eCardType*2
						  + sCh->m_eTransMode],
				       sCh->m_uOperFreq,
				       uBlocks >> (20 - sCh->m_sReadBlockLen));
			}
		} else {
			if (verbose)
				puts("slot empty\n");
		}

		sCh->mmc_dev.if_type = IF_TYPE_MMC;
		sCh->mmc_dev.part_type = PART_TYPE_DOS;
		sCh->mmc_dev.dev = i;
		sCh->mmc_dev.blksz = 512;
		sCh->mmc_dev.block_read = mmc_bread;
	}

	return 0;
}

block_dev_desc_t* mmc_get_dev(int dev_num)
{
	if (dev_num < MAX_UNITS)
		return &mmc_unit[dev_num].mmc_dev;
	return NULL;
}

/* No support for copying data from/to MMC with memory command cp */
int mmc_read(ulong src, uchar *dst, int size)
{
	return -1;
}

/* No support for copying data from/to MMC with memory command cp */
int mmc_write(uchar *src, ulong dst, int size)
{
	return -1;
}

/* No support for copying data from/to MMC with memory command cp */
int mmc2info(ulong addr)
{
	return 0;
}
