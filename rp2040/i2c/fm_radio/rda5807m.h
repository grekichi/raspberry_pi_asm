#ifndef __RDA5807M_H__
#define __RDA5807M_H__

/* ------------------------------------------------------------------ */
/* RDA5807M ドライバ                                                   */
/* ------------------------------------------------------------------ */
#define RDA_SEQ_ADDR            0x10u   /* シーケンシャルアクセス */
#define RDA_RDM_ADDR            0x11u   /* ランダムアクセス */

/* reg 0x02 */
#define RDA_R02_DHIZ            (1u << 15)  /* 1: オーディオ出力ハイZ解除 */
#define RDA_R02_DMUTE           (1u << 14)  /* 1: ミュート解除 */
#define RDA_R02_MONO            (1u << 13)
#define RDA_R02_BASS            (1u << 12)
#define RDA_R02_SEEKUP          (1u << 9)
#define RDA_R02_SEEK            (1u << 8)
#define RDA_R02_SKMODE          (1u << 7)   /* 1: バンド端で停止 */
#define RDA_R02_NEW_METHOD      (1u << 2)
#define RDA_R02_SOFT_RESET      (1u << 1)
#define RDA_R02_ENABLE          (1u << 0)

/* reg 0x03 */
#define RDA_R03_CHAN_SHIFT      6
#define RDA_R03_CHAN_MASK       (0x3FFu << R03_CHAN_SHIFT)
#define RDA_R03_TUNE            (1u << 4)
#define RDA_R03_BAND_76_108     (2u << 2)   /* 76-108MHz (日本のFMを含む) */
#define RDA_R03_SPACE_100K      (0u << 0)

/* reg 0x04 */
#define RDA_R04_DEFAULT         0x0400u
#define RDA_R04_DE_50US         (1u << 11)  /* 日本は50us (米国は75us) */

/* reg 0x05 */
#define RDA_R05_BASE            0x8880u     /* INT_MODE=1, SEEKTH=8, LNAP */
#define RDA_R05_VOL_MASK        0x000Fu

/* reg 0x0A / 0x0B (読み出し) */
#define RDA_R0A_STC             (1u << 14)
#define RDA_R0A_SF              (1u << 13)
#define RDA_R0A_ST              (1u << 10)  /* ステレオ */
#define RDA_R0A_READCHAN_MASK   0x03FFu
#define RDA_R0B_RSSI_SHIFT      9
#define RDA_R0B_FM_TRUE         (1u << 8)

#endif
