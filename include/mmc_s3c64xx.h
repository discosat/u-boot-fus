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
/*** File:     s1sdhc.h                                                    ***/
/*** Author:   Hartmut Keller                                              ***/
/*** Created:  06.06.2011                                                  ***/
/*** Modified: 06.06.2011 15:11:57 (HK)                                    ***/
/***                                                                       ***/
/*** Description:                                                          ***/
/*** SD-card driver for S3C64xx in U-Boot. Based on the SD-card driver for ***/
/*** PicoMOD6 in the F&S NBoot loader.                                     ***/
/***                                                                       ***/
/*** Modification History:                                                 ***/
/*** 06.06.2011 HK: Converted from NBoot to U-Boot.                        ***/
/*****************************************************************************/
#ifndef __MMC_S3C64XX_H__
#define __MMC_S3C64XX_H__

typedef unsigned int U32;
typedef unsigned short U16;
typedef unsigned char U8;
typedef unsigned int bool;
#define TRUE 1
#define FALSE 0

/* ------------ Exported Definitions --------------------------------------- */

/* Normal Interrupt Signal Flags */
#define SDHC_SD_ADDRESS_INT3_EN           (1<<14)
#define SDHC_SD_ADDRESS_INT2_EN           (1<<13)
#define SDHC_SD_ADDRESS_INT1_EN           (1<<12)
#define SDHC_SD_ADDRESS_INT0_EN           (1<<11)
#define SDHC_READWAIT_SIG_INT_EN          (1<<10)
#define SDHC_CCS_INTERRUPT_STATUS_EN      (1<<9)
#define SDHC_CARD_SIG_INT_EN              (1<<8)
#define SDHC_CARD_REMOVAL_SIG_INT_EN      (1<<7)
#define SDHC_CARD_INSERT_SIG_INT_EN       (1<<6)
#define SDHC_BUFFER_READREADY_SIG_INT_EN  (1<<5)
#define SDHC_BUFFER_WRITEREADY_SIG_INT_EN (1<<4)
#define SDHC_DMA_SIG_INT_EN               (1<<3)
#define SDHC_BLOCKGAP_EVENT_SIG_INT_EN    (1<<2)
#define SDHC_TRANSFERCOMPLETE_SIG_INT_EN  (1<<1)
#define SDHC_COMMANDCOMPLETE_SIG_INT_EN   (1<<0)


/* ------------ Exported Types And Enumerations ---------------------------- */

/* SDHC Channel Number */
typedef enum
{
    SDHC_CHANNEL_0,
    SDHC_CHANNEL_1,
    SDHC_CHANNEL_2,
} SDHC_Channel;

/* SDHC Card Type */
typedef enum
{
    SDHC_SD_CARD,
    SDHC_MMC_CARD,
} SDHC_CardType;


/* Clock Source */
typedef enum
{
    SDHC_HCLK=1,
    SDHC_EXTCLK,
    SDHC_EPLL,
    SDHC_MPLL,
    SDHC_FIN,
    SDHC_27M
} SDHC_ClockSource;

/* SD/MMC Speed Mode */
typedef enum _SDHC_SpeedMode
{
    SDHC_NORMAL_SPEED,
    SDHC_HIGH_SPEED
} SDHC_SpeedMode;

/* Card transfer modes */
typedef enum
{
    SDHC_BYTE_MODE=0,
    SDHC_BLOCK_MODE
} SDHC_TransferMode;

/* Bus width */
typedef enum
{
    SDHC_BUSWIDTH1,
    SDHC_BUSWIDTH4,
    SDHC_BUSWIDTH8
} SDHC_Buswidth;

/* Driver strength */
typedef enum
{
    SDHC_STRENGTH_2MA,
    SDHC_STRENGTH_4MA,
    SDHC_STRENGTH_7MA,
    SDHC_STRENGTH_9MA
} SDHC_DriverStrength;    

/* Command types */
typedef enum _SDHC_CommandType
{
    SDHC_CMD_BC_TYPE,                     /* Broadcast commands (bc),
                                             no response */
    SDHC_CMD_BCR_TYPE,                    /* Broadcast commands with response
                                             (bcr) */
    SDHC_CMD_AC_TYPE,                     /* Addressed (point-to-point)
                                             commands (ac), no data transfer
                                             on DAT lines */
    SDHC_CMD_ADTC_TYPE,                   /* Addressed (point-to-point) data
                                             transfer commands (adtc), data
                                             transfer on DAT lines */
} SDHC_CommandType;

/* Command response types (as defined in the standard) */
typedef enum _SDHC_ResponseType
{
    SDHC_RES_NO_TYPE,
    SDHC_RES_R1_TYPE,                     /* Standard response, not busy */
    SDHC_RES_R1B_TYPE,                    /* Standard response, maybe busy */
    SDHC_RES_R2_TYPE,                     /* CID (CMD2/10), CSD (CMD9) */
    SDHC_RES_R3_TYPE,                     /* OCR (ACMD41) */
    SDHC_RES_R4_TYPE,                     /* I/O-OCR (CMD5, only SDIO) */
    SDHC_RES_R5_TYPE,                     /* R/W-Data (CMD52, only SDIO) */
    SDHC_RES_R6_TYPE,                     /* RCA (CMD3) */
    SDHC_RES_R7_TYPE,                     /* Card interface condition (CMD8) */
} SDHC_ResponseType;

/* Possible card states (point-of-view of the card) */
typedef enum _SDHC_CurrentState
{
    /* Card identification mode: only with preliminary (slow) SDCLK */
    SDHC_CS_IDLE,                         /* 0: Idle (after Reset/CMD0) */
    SDHC_CS_READY,                        /* 1: Ready (after ACMD41) */
    SDHC_CS_IDENT,                        /* 2: Identification (after CMD2) */

    /* Data transfer mode: can use final (full speed) SDCLK */
    SDHC_CS_STDBY,                        /* 3: Standby, waiting for commands
                                                (after CMD3) */
    SDHC_CS_TRAN,                         /* 4: Transfer (after CMD7) */
    SDHC_CS_DATA,                         /* 5: Sending data (Card->Host) */
    SDHC_CS_RCV,                          /* 6: Receiving data (Host->Card) */
    SDHC_CS_PRG,                          /* 7: Programming (Storing data) */
    SDHC_CS_DIS                           /* 8: Disconnected (CMD7 while
                                                programming) */
} SDHC_CurrentState;


/* ------------ Exported Structures ---------------------------------------- */

/* Structure describing the SD/MMC host controller registers */
typedef volatile struct _SDHC_REGS
{
    U32 SYS_ADDR;                         /* 0x00 */
    U16 BLK_SIZE;                         /* 0x04 */
    U16 BLK_COUNT;                        /* 0x06 */
    U32 ARG;                              /* 0x08 */
    U16 TRANS_MODE;                       /* 0x0C */
    U16 COMMAND;                          /* 0x0E */
    U32 RSP0;                             /* 0x10 */
    U32 RSP1;                             /* 0x14 */
    U32 RSP2;                             /* 0x18 */
    U32 RSP3;                             /* 0x1C */
    U32 BUF_DAT_PORT;                     /* 0x20 */
    U32 PRESENT_STAT;                     /* 0x24 */
    U8  HOST_CTRL;                        /* 0x28 */
    U8  PWR_CTRL;                         /* 0x29 */
    U8  BLOCKGAP_CTRL;                    /* 0x2A */
    U8  WAKEUP_CTRL;                      /* 0x2B */
    U16 CLK_CTRL;                         /* 0x2C */
    U8  TIMEOUT_CTRL;                     /* 0x2E */
    U8  SOFTWARE_RESET;                   /* 0x2F */
    U16 NORMAL_INT_STAT;                  /* 0x30 */
    U16 ERROR_INT_STAT;                   /* 0x32 */
    U16 NORMAL_INT_STAT_ENABLE;           /* 0x34 */
    U16 ERROR_INT_STAT_ENABLE;            /* 0x36 */
    U16 NORMAL_INT_SIGNAL_ENABLE;         /* 0x38 */
    U16 ERROR_INT_SIGNAL_ENABLE;          /* 0x3A */
    U16 AUTO_CMD12_ERR_STAT;              /* 0x3C */
    U16 _Reserved1;                       /* 0x3E */
    U32 CAPA;                             /* 0x40 */
    U32 _Reserved2;                       /* 0x44 */
    U32 MAX_CURRENT_CAPA;                 /* 0x48 */
    U32 MONITOR;                          /* 0x4C (not available on S3C6410) */
    U32 _Reserved3[12];                   /* 0x50 */
    U32 CONTROL2;                         /* 0x80 */
    /* [15] Feedback clock used for Tx Data/Command logic.
       [14] Feedback clock used for Rx Data/Command logic.
       [13] Select card detection signal. 0=nSDCD, 1=DAT[3].
       [11] CE-ATA I/F mode. 1=Enable, 0=Disable.
       [8]  SDCLK Hold enable. */
    U32 CONTROL3;                         /* 0x84 */
    U32 _Reserved4;                       /* 0x88 */
    U32 CONTROL4;                         /* 0x8C */
    U32 _Reserved5[27];                   /* 0x90 */
    U16 SLOT_INT_STAT;                    /* 0xFC (not available on S3C6410) */
    U16 HOST_CONTROLLER_VERSION;          /* 0xFE */
} SDHC_REGS;

/* Common SD/MMC Structure */
typedef struct _SDHC
{
    /* Part 1: data required for operation */
    SDHC_REGS *m_pRegs;                   /* Pointer to host controller */
    SDHC_TransferMode m_eTransMode;       /* Detected transfer mode */
    SDHC_SpeedMode m_eSpeedMode;          /* Detected transfer speed */
    U8   m_ucSpecVer;                     /* Specification version */
    U32  m_uRca;                          /* Relative card address (in
                                             [31:16], [15:0] always zero) */
    // -- Card Information
    U16  m_sReadBlockLen;                 /* Block size (in bits) */
    U16  m_sReadBlockPartial;             /* Flag if partial read is allowed */
    U16  m_sCSize;                        /* Card size */
    U16  m_sCSizeMult;                    /* Card size multiplicator */
    /* CardSize = (m_sCSize+1) * 2^(m_sCSizeMult+2) * 2^(m_sReadBlockLen) */

    /* Part 2: data for information purposes only */
    U32  m_uOperFreq;                     /* Operation frequency */
    U32  m_uPSN;                          /* Serial number */
    U16  m_uMDT;                          /* Manufacturing date */
    U8   m_chMID;                         /* Manufacturer ID */
    U8   m_chOID[3];                      /* OEM/Application ID */
    U8   m_chPNM[6];                      /* Product name */
    U8   m_chPRV[4];                      /* Product revision */
    SDHC_CardType m_eCardType;            /* Detected card type */
    SDHC_Buswidth m_eBuswidth;            /* Bus width (1, 4, 8 bit) */

    /* Part 3: extended CSD data on MMC cards (valid if m_ucSpecVer >= 4) */
    U32  m_uSectorCount;
    U8   m_chCsdStructVers;
    U8   m_chExtCsdRev;
    U8   m_chCardType;
    U8   m_chSupportedCmdSet;
    U8   m_chCommandSet;
    U8   m_chCommandSetRev;
    U8   m_chPowerClass;
    U8   m_chHighSpeedTiming;
    U8   m_chBusWidthTiming;
    U8   m_chErasedMemContent;
    U8   m_chMinWritePerf8_52;
    U8   m_chMinReadPerf8_52;
    U8   m_chMinWritePerf8_26;
    U8   m_chMinReadPerf8_26;
    U8   m_chMinWritePerf4_26;
    U8   m_chMinReadPerf4_26;

    block_dev_desc_t mmc_dev;
} SDHC;


#endif /*!__MMC_S3C64XX_H__*/
