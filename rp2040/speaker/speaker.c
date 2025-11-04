#include "rp2040.h"

void PUT32(unsigned int, unsigned int);
unsigned int GET32(unsigned int);
void DELAY(unsigned int);

#define TIME_1SEC       (12000000)  // 12MHz
#define TIME_100MSEC    (TIME_1SEC / 10)
#define TIME_1MSEC      (TIME_1SEC / 1000)

#define C4_262  (45801)
#define D4_294  (40815)
#define E4_330  (36362)
#define F4_349  (34383)
#define FS4_370 (32431)
#define G4_392  (30611)
#define GS4_415 (28915)
#define A4_440  (27271)
#define B4_494  (24290)
#define C5_523  (22943)
#define CS5_554 (21660)
#define D5_587  (20442)
#define E5_659  (18208)
#define F5_698  (17191)
#define FS5_740 (16215)
#define G5_784  (15305)
#define A5_880  (13635)
#define B5_988  (12144)
#define C6_1047 (11460)
#define D6_1175 (10212)
#define E6_1319 (9097)
#define F6_1397 (8589)
#define G6_1568 (7652)
#define A6_1760 (6817)
#define B6_1976 (6072)
#define C7_2093 (5732)
#define D7_2349 (5108)
#define E7_2637 (4550)
#define F7_2794 (4294)
#define G7_3136 (3826)
#define A7_3520 (3408)
#define B7_3951 (3036)
#define C8_4186 (2866)

// 音階表
// C           C#          D            D#          E           F             F#            G            G#           A           A#          B
//                                                                                                                   ラ0:  28Hz, ラ#0:  29Hz, シ0:  31Hz
// ド1:  33Hz, ド#1:  35Hz, レ1:  37Hz, レ#1:  39Hz, ミ1:  41Hz, ファ1:  44Hz, ファ#1:  46Hz, ソ1:  49Hz, , ソ#1:  52Hz, ラ1:  55Hz, ラ#1:  58Hz, シ1:  62Hz
// ド2:  65Hz, ド#2:  69Hz, レ2:  73Hz, レ#2:  78Hz, ミ2:  82Hz, ファ2:  87Hz, ファ#2:  92Hz, ソ2:  98Hz, , ソ#2: 104Hz, ラ2: 110Hz, ラ#2: 117Hz, シ2: 123Hz
// ド3: 131Hz, ド#3: 139Hz, レ3: 147Hz, レ#3: 156Hz, ミ3: 165Hz, ファ3: 175Hz, ファ#3: 185Hz, ソ3: 196Hz, , ソ#3: 208Hz, ラ3: 220Hz, ラ#3: 233Hz, シ3: 247Hz
// ド4: 262Hz, ド#4: 277Hz, レ4: 294Hz, レ#4: 311Hz, ミ4: 330Hz, ファ4: 349Hz, ファ#4: 370Hz, ソ4: 392Hz, , ソ#4: 415Hz, ラ4: 440Hz, ラ#4: 446Hz, シ4: 494Hz
// ド5: 523Hz, ド#5: 554Hz, レ5: 587Hz, レ#5: 622Hz, ミ5: 659Hz, ファ5: 698Hz, ファ#5: 740Hz, ソ5: 784Hz, , ソ#5: 831Hz, ラ5: 880Hz, ラ#5: 932Hz, シ5: 988Hz
// ド6:1047Hz, ド#6:1109Hz, レ6:1175Hz, レ#6:1245Hz, ミ6:1319Hz, ファ6:1397Hz, ファ#6:1480Hz, ソ6:1568Hz, , ソ#6:1661Hz, ラ6:1760Hz, ラ#6:1865Hz, シ6:1976Hz
// ド7:2093Hz, ド#7:2217Hz, レ7:2349Hz, レ#7:2489Hz, ミ7:2637Hz, ファ7:2794Hz, ファ#7:2960Hz, ソ7:3136Hz, , ソ#7:3322Hz, ラ7:3520Hz, ラ#7:3729Hz, シ7:3951Hz
// ド8:4186Hz


static void clock_init(void)  // 水晶発振器のセット 
{
    PUT32(CLK_SYS_RESUS_CTRL_RW, 0);  // time out を0サイクルに設定
    PUT32(XOSC_CTRL_RW, 0xAA0);  // 水晶発振器を1-15MHzレンジに設定
    PUT32(XOSC_STARTUP_RW, 47);  // 256 * 47 = 12032cycles(=1ms)を 起動遅延として設定
    PUT32(XOSC_CTRL_RW, 0xFAB << 12);  // 水晶発振器を使用可能(enable)にする 
    while (1)
    {
        if ((GET32(XOSC_STATUS_RW) & 0x80000000) != 0) break;  // 発振器の動作確認
    }
    PUT32(CLK_REF_CTRL_RW, 2);  // XOSC Clock source
    PUT32(CLK_SYS_CTRL_RW, 0);  // CLK_REF
    PUT32(CLK_PERI_CTRL_RW, (1 << 11) | (4 << 5));  // enable, XOSC_CLKSRC
}

static void pwm_init(void)
{
    PUT32(RESETS_RESET_CLR, (1 << 5));  // IO_BANK0のリセットを解除
    while (1)
    {
        if ((GET32(RESETS_RESET_DONE_RW) & (1 << 5)) != 0) break;
    }

    PUT32(RESETS_RESET_CLR, (1 << 14));  // PWMのリセットを解除
    while (1)
    {
        if ((GET32(RESETS_RESET_DONE_RW) & (1 << 14)) != 0) break;
    }

    PUT32(IO_BANK0_GPIO2_CTRL_RW, 4);   // PWM    // ftag=523.251Hz: TOP=22932, tag=0x5994
    PUT32(PWM_CH1_CSR_RW, 1);           // PWM enable
}


// main program
int speaker(void)
{
    clock_init();
    pwm_init();

    PUT32(SYST_CSR, 0x00000004);
    PUT32(SYST_RVR, 12000000-1);
    PUT32(SYST_CVR, 12000000-1);
    PUT32(SYST_CSR, 0x00000005);

    // メリーさんのひつじ
    static const unsigned int melody[25] = {
        // E4_330, D4_294, C4_262, D4_294,
        // E4_330, E4_330, E4_330,
        // D4_294, D4_294, D4_294,
        // E4_330, G4_392, G4_392,

        // E4_330, D4_294, C4_262, D4_294,
        // E4_330, E4_330, E4_330,
        // D4_294, D4_294, E4_330, D4_294,
        // C4_262

        // 音階１UPバージョン
        E5_659, D5_587, C5_523, D5_587,
        E5_659, E5_659, E5_659,
        D5_587, D5_587, D5_587,
        E5_659, G5_784, G5_784,

        E5_659, D5_587, C5_523, D5_587,
        E5_659, E5_659, E5_659,
        D5_587, D5_587, E5_659, D5_587,
        C5_523
    };

    static const unsigned int melody_durations[25] = {
        TIME_100MSEC*3, TIME_100MSEC, TIME_100MSEC*2, TIME_100MSEC*2,
        TIME_100MSEC*2, TIME_100MSEC*2, TIME_100MSEC*2,
        TIME_100MSEC*2, TIME_100MSEC*2, TIME_100MSEC*2, 
        TIME_100MSEC*2, TIME_100MSEC*2, TIME_100MSEC*2, 

        TIME_100MSEC*3, TIME_100MSEC, TIME_100MSEC*2, TIME_100MSEC*2,
        TIME_100MSEC*2, TIME_100MSEC*2, TIME_100MSEC*2,
        TIME_100MSEC*2, TIME_100MSEC*2, TIME_100MSEC*3, TIME_100MSEC,
        TIME_100MSEC*4
    };

    while (1)
    {
        // メインmelody
        for (int i = 0; i < 25; i++)
        {
            PUT32(PWM_CH1_TOP_RW, melody[i]);
            PUT32(PWM_CH1_DIV_RW, 0x10);    // DIV_INT=1, DIV_FRAC=0 : 固定値
            PUT32(PWM_CH1_CC_RW, (melody[i] >> 1)); // duty cycle 50%
            DELAY(melody_durations[i] >> 1); // (標準:BPM60) 右シフト1でBPM120
            if (i == 6 || i == 9 || i == 12 || i == 19)
            {
                // PUT32(PWM_CH1_CSR_RW, 0);
                // DELAY(TIME_100MSEC);
                // PUT32(PWM_CH1_CSR_RW, 1);
                PUT32(PWM_CH1_CC_RW, 0);
                DELAY(TIME_100MSEC);
                PUT32(PWM_CH1_CC_RW, (melody[i] >> 1));
            }
            PUT32(PWM_CH1_CC_RW, 0);  // 終了処理
            DELAY(TIME_1MSEC*20);  // 短い休符
        }
        DELAY(TIME_1SEC);  // インターバル
    }

    return 0;
}

//-------------------------------------------------------------------------
// 
// Copyright (c) 2025 Grekichi peacedode@info.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
