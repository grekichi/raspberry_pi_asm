void PUT32 (unsigned int, unsigned int);
unsigned int GET32 (unsigned int);
void DELAY (unsigned int);

#define RESETS_BASE                 0x4000C000

// #define RESETS_RESET_RW             (RESETS_BASE+0x0+0x0000)
// #define RESETS_RESET_XOR            (RESETS_BASE+0x0+0x1000)
// #define RESETS_RESET_SET            (RESETS_BASE+0x0+0x2000)
#define RESETS_RESET_CLR            (RESETS_BASE+0x0+0x3000)

// #define RESETS_WDSEL_RW             (RESETS_BASE+0x4+0x0000)
// #define RESETS_WDSEL_XOR            (RESETS_BASE+0x4+0x1000)
// #define RESETS_WDSEL_SET            (RESETS_BASE+0x4+0x2000)
// #define RESETS_WDSEL_CLR            (RESETS_BASE+0x4+0x3000)

#define RESETS_RESET_DONE_RW        (RESETS_BASE+0x8+0x0000)
// #define RESETS_RESET_DONE_XOR       (RESETS_BASE+0x8+0x1000)
// #define RESETS_RESET_DONE_SET       (RESETS_BASE+0x8+0x2000)
// #define RESETS_RESET_DONE_CLR       (RESETS_BASE+0x8+0x3000)

#define SIO_BASE                    0xD0000000

#define SIO_GPIO_IN                 (SIO_BASE+0x04)

#define SIO_GPIO_OUT_RW             (SIO_BASE+0x10)
#define SIO_GPIO_OUT_SET            (SIO_BASE+0x14)
#define SIO_GPIO_OUT_CLR            (SIO_BASE+0x18)
#define SIO_GPIO_OUT_XOR            (SIO_BASE+0x1C)

#define SIO_GPIO_OE_RW              (SIO_BASE+0x20)
#define SIO_GPIO_OE_SET             (SIO_BASE+0x24)
#define SIO_GPIO_OE_CLR             (SIO_BASE+0x28)
#define SIO_GPIO_OE_XOR             (SIO_BASE+0x2C)

#define IO_BANK0_BASE               0x40014000

// #define IO_BANK0_GPIO12_STATUS_RW   (IO_BANK0_BASE+0x060+0x0000)
// #define IO_BANK0_GPIO12_STATUS_XOR  (IO_BANK0_BASE+0x060+0x1000)
// #define IO_BANK0_GPIO12_STATUS_SET  (IO_BANK0_BASE+0x060+0x2000)
// #define IO_BANK0_GPIO12_STATUS_CLR  (IO_BANK0_BASE+0x060+0x3000)

#define IO_BANK0_GPIO12_CTRL_RW     (IO_BANK0_BASE+0x064+0x0000)
// #define IO_BANK0_GPIO12_CTRL_XOR    (IO_BANK0_BASE+0x064+0x1000)
// #define IO_BANK0_GPIO12_CTRL_SET    (IO_BANK0_BASE+0x064+0x2000)
// #define IO_BANK0_GPIO12_CTRL_CLR    (IO_BANK0_BASE+0x064+0x3000)

// #define IO_BANK0_GPIO13_STATUS_RW   (IO_BANK0_BASE+0x068+0x0000)
// #define IO_BANK0_GPIO13_STATUS_XOR  (IO_BANK0_BASE+0x068+0x1000)
// #define IO_BANK0_GPIO13_STATUS_SET  (IO_BANK0_BASE+0x068+0x2000)
// #define IO_BANK0_GPIO13_STATUS_CLR  (IO_BANK0_BASE+0x068+0x3000)

#define IO_BANK0_GPIO13_CTRL_RW     (IO_BANK0_BASE+0x06C+0x0000)
// #define IO_BANK0_GPIO13_CTRL_XOR    (IO_BANK0_BASE+0x06C+0x1000)
// #define IO_BANK0_GPIO13_CTRL_SET    (IO_BANK0_BASE+0x06C+0x2000)
// #define IO_BANK0_GPIO13_CTRL_CLR    (IO_BANK0_BASE+0x06C+0x3000)

// #define IO_BANK0_GPIO14_STATUS_RW   (IO_BANK0_BASE+0x070+0x0000)
// #define IO_BANK0_GPIO14_STATUS_XOR  (IO_BANK0_BASE+0x070+0x1000)
// #define IO_BANK0_GPIO14_STATUS_SET  (IO_BANK0_BASE+0x070+0x2000)
// #define IO_BANK0_GPIO14_STATUS_CLR  (IO_BANK0_BASE+0x070+0x3000)

#define IO_BANK0_GPIO14_CTRL_RW     (IO_BANK0_BASE+0x074+0x0000)
// #define IO_BANK0_GPIO14_CTRL_XOR    (IO_BANK0_BASE+0x074+0x1000)
// #define IO_BANK0_GPIO14_CTRL_SET    (IO_BANK0_BASE+0x074+0x2000)
// #define IO_BANK0_GPIO14_CTRL_CLR    (IO_BANK0_BASE+0x074+0x3000)

// #define IO_BANK0_GPIO15_STATUS_RW   (IO_BANK0_BASE+0x078+0x0000)
// #define IO_BANK0_GPIO15_STATUS_XOR  (IO_BANK0_BASE+0x078+0x1000)
// #define IO_BANK0_GPIO15_STATUS_SET  (IO_BANK0_BASE+0x078+0x2000)
// #define IO_BANK0_GPIO15_STATUS_CLR  (IO_BANK0_BASE+0x078+0x3000)

#define IO_BANK0_GPIO15_CTRL_RW     (IO_BANK0_BASE+0x07C+0x0000)
// #define IO_BANK0_GPIO15_CTRL_XOR    (IO_BANK0_BASE+0x07C+0x1000)
// #define IO_BANK0_GPIO15_CTRL_SET    (IO_BANK0_BASE+0x07C+0x2000)
// #define IO_BANK0_GPIO15_CTRL_CLR    (IO_BANK0_BASE+0x07C+0x3000)

// #define IO_BANK0_GPIO16_STATUS_RW   (IO_BANK0_BASE+0x080+0x0000)
// #define IO_BANK0_GPIO16_STATUS_XOR  (IO_BANK0_BASE+0x080+0x1000)
// #define IO_BANK0_GPIO16_STATUS_SET  (IO_BANK0_BASE+0x080+0x2000)
// #define IO_BANK0_GPIO16_STATUS_CLR  (IO_BANK0_BASE+0x080+0x3000)

#define IO_BANK0_GPIO16_CTRL_RW     (IO_BANK0_BASE+0x084+0x0000)
// #define IO_BANK0_GPIO16_CTRL_XOR    (IO_BANK0_BASE+0x084+0x1000)
// #define IO_BANK0_GPIO16_CTRL_SET    (IO_BANK0_BASE+0x084+0x2000)
// #define IO_BANK0_GPIO16_CTRL_CLR    (IO_BANK0_BASE+0x084+0x3000)

// #define IO_BANK0_GPIO17_STATUS_RW   (IO_BANK0_BASE+0x088+0x0000)
// #define IO_BANK0_GPIO17_STATUS_XOR  (IO_BANK0_BASE+0x088+0x1000)
// #define IO_BANK0_GPIO17_STATUS_SET  (IO_BANK0_BASE+0x088+0x2000)
// #define IO_BANK0_GPIO17_STATUS_CLR  (IO_BANK0_BASE+0x088+0x3000)

#define IO_BANK0_GPIO17_CTRL_RW     (IO_BANK0_BASE+0x08C+0x0000)
// #define IO_BANK0_GPIO17_CTRL_XOR    (IO_BANK0_BASE+0x08C+0x1000)
// #define IO_BANK0_GPIO17_CTRL_SET    (IO_BANK0_BASE+0x08C+0x2000)
// #define IO_BANK0_GPIO17_CTRL_CLR    (IO_BANK0_BASE+0x08C+0x3000)

// #define IO_BANK0_GPIO18_STATUS_RW   (IO_BANK0_BASE+0x090+0x0000)
// #define IO_BANK0_GPIO18_STATUS_XOR  (IO_BANK0_BASE+0x090+0x1000)
// #define IO_BANK0_GPIO18_STATUS_SET  (IO_BANK0_BASE+0x090+0x2000)
// #define IO_BANK0_GPIO18_STATUS_CLR  (IO_BANK0_BASE+0x090+0x3000)

#define IO_BANK0_GPIO18_CTRL_RW     (IO_BANK0_BASE+0x094+0x0000)
// #define IO_BANK0_GPIO18_CTRL_XOR    (IO_BANK0_BASE+0x094+0x1000)
// #define IO_BANK0_GPIO18_CTRL_SET    (IO_BANK0_BASE+0x094+0x2000)
// #define IO_BANK0_GPIO18_CTRL_CLR    (IO_BANK0_BASE+0x094+0x3000)

// #define IO_BANK0_GPIO19_STATUS_RW   (IO_BANK0_BASE+0x098+0x0000)
// #define IO_BANK0_GPIO19_STATUS_XOR  (IO_BANK0_BASE+0x098+0x1000)
// #define IO_BANK0_GPIO19_STATUS_SET  (IO_BANK0_BASE+0x098+0x2000)
// #define IO_BANK0_GPIO19_STATUS_CLR  (IO_BANK0_BASE+0x098+0x3000)

#define IO_BANK0_GPIO19_CTRL_RW     (IO_BANK0_BASE+0x09C+0x0000)
// #define IO_BANK0_GPIO19_CTRL_XOR    (IO_BANK0_BASE+0x09C+0x1000)
// #define IO_BANK0_GPIO19_CTRL_SET    (IO_BANK0_BASE+0x09C+0x2000)
// #define IO_BANK0_GPIO19_CTRL_CLR    (IO_BANK0_BASE+0x09C+0x3000)

#define PADS_BANK0_BASE             0x4001C000

#define PADS_BANK0_GPIO12_SET       (PADS_BANK0_BASE+0x34+0x2000)
#define PADS_BANK0_GPIO12_CLR       (PADS_BANK0_BASE+0x34+0x3000)

#define PADS_BANK0_GPIO13_SET       (PADS_BANK0_BASE+0x38+0x2000)
#define PADS_BANK0_GPIO13_CLR       (PADS_BANK0_BASE+0x38+0x3000)

#define PADS_BANK0_GPIO14_SET       (PADS_BANK0_BASE+0x3C+0x2000)
#define PADS_BANK0_GPIO14_CLR       (PADS_BANK0_BASE+0x3C+0x3000)

#define PADS_BANK0_GPIO15_SET       (PADS_BANK0_BASE+0x40+0x2000)
#define PADS_BANK0_GPIO15_CLR       (PADS_BANK0_BASE+0x40+0x3000)

#define PADS_BANK0_GPIO16_SET       (PADS_BANK0_BASE+0x44+0x2000)
#define PADS_BANK0_GPIO16_CLR       (PADS_BANK0_BASE+0x44+0x3000)

#define PADS_BANK0_GPIO17_SET       (PADS_BANK0_BASE+0x48+0x2000)
#define PADS_BANK0_GPIO17_CLR       (PADS_BANK0_BASE+0x48+0x3000)

#define PADS_BANK0_GPIO18_SET       (PADS_BANK0_BASE+0x4C+0x2000)
#define PADS_BANK0_GPIO18_CLR       (PADS_BANK0_BASE+0x4C+0x3000)

#define PADS_BANK0_GPIO19_SET       (PADS_BANK0_BASE+0x50+0x2000)
#define PADS_BANK0_GPIO19_CLR       (PADS_BANK0_BASE+0x50+0x3000)


static void set_number(unsigned int x)
{
    // turn on leds
    PUT32(SIO_GPIO_OUT_XOR, x << 12);
    DELAY(0x200000);
    // turn off leds
    PUT32(SIO_GPIO_OUT_SET, 0xFF << 12);
    DELAY(0x200000);
}


int segled(void)
{
    // unsigned int ra;

    // IO_BANK0のリセット状態を解除する（=使える状態にする）
    PUT32(RESETS_RESET_CLR, 1 << 5);  // ビット５がIO_BANK0に相当（rp2040datasheet p176より）
    // リセットが完了するまでの待機処理
    while (1)
    {
        // リセットが完了したレジスタのチェック：ビット５が１かどうかをチェック（=リセット完了なら1になる）
        if ((GET32(RESETS_RESET_DONE_RW) & (1 << 5)) != 0)
            break; // IO_BANK0のリセットが完了していればループを抜ける
    }

    // PADS_BANK0のリセット状態を解除する（=使える状態にする）
    PUT32(RESETS_RESET_CLR, 1 << 8);  // ビット8がPADS_BANK0に相当（rp2040datasheet p176より）
    // リセットが完了するまでの待機処理
    while (1)
    {
        // リセットが完了したレジスタのチェック：ビット8が１かどうかをチェック（=リセット完了なら1になる）
        if ((GET32(RESETS_RESET_DONE_RW) & (1 << 8)) != 0)
            break; // PADS_BANK0のリセットが完了していればループを抜ける
    }

    // 各GPIOピンのデフォルトLOW(プルダウン)を無効にする
    PUT32(PADS_BANK0_GPIO12_CLR, 1 << 2);  // PDE disable
    PUT32(PADS_BANK0_GPIO13_CLR, 1 << 2);  // PDE disable
    PUT32(PADS_BANK0_GPIO14_CLR, 1 << 2);  // PDE disable
    PUT32(PADS_BANK0_GPIO15_CLR, 1 << 2);  // PDE disable
    PUT32(PADS_BANK0_GPIO16_CLR, 1 << 2);  // PDE disable
    PUT32(PADS_BANK0_GPIO17_CLR, 1 << 2);  // PDE disable
    PUT32(PADS_BANK0_GPIO18_CLR, 1 << 2);  // PDE disable
    PUT32(PADS_BANK0_GPIO19_CLR, 1 << 2);  // PDE disable

    // 各GPIOピンをデフォルトHIGH(プルアップ)に変更する
    PUT32(PADS_BANK0_GPIO12_SET, 1 << 3);  // PUE enable
    PUT32(PADS_BANK0_GPIO13_SET, 1 << 3);  // PUE enable
    PUT32(PADS_BANK0_GPIO14_SET, 1 << 3);  // PUE enable
    PUT32(PADS_BANK0_GPIO15_SET, 1 << 3);  // PUE enable
    PUT32(PADS_BANK0_GPIO16_SET, 1 << 3);  // PUE enable
    PUT32(PADS_BANK0_GPIO17_SET, 1 << 3);  // PUE enable
    PUT32(PADS_BANK0_GPIO18_SET, 1 << 3);  // PUE enable
    PUT32(PADS_BANK0_GPIO19_SET, 1 << 3);  // PUE enable

    // GPIO 12 ~ 19 の出力を無効化する（初期化のための安全動作）
    PUT32(SIO_GPIO_OE_CLR, 0xFF << 12);
    // GPIO 12 ~ 19 の値をクリアする
    PUT32(SIO_GPIO_OUT_CLR, 0xFF << 12);
    // GPIO 12 ~ 19 をSIO機能（ソフトウェア制御モード）に設定する
    PUT32(IO_BANK0_GPIO12_CTRL_RW, 5);
    PUT32(IO_BANK0_GPIO13_CTRL_RW, 5);
    PUT32(IO_BANK0_GPIO14_CTRL_RW, 5);
    PUT32(IO_BANK0_GPIO15_CTRL_RW, 5);
    PUT32(IO_BANK0_GPIO16_CTRL_RW, 5);
    PUT32(IO_BANK0_GPIO17_CTRL_RW, 5);
    PUT32(IO_BANK0_GPIO18_CTRL_RW, 5);
    PUT32(IO_BANK0_GPIO19_CTRL_RW, 5);

    // GPIO 12 ~ 19 の出力をHigh(=1)に設定する
    PUT32(SIO_GPIO_OUT_SET, 0xFF << 12);
    // GPIO 12 ~ 19 の出力を有効化する
    PUT32(SIO_GPIO_OE_SET, 0XFF << 12);


    while (1)
    {
        // 7 seg leds (#0) ... 0x77
        // 7 seg leds (#1) ... 0x14
        // 7 seg leds (#2) ... 0xB3
        // 7 seg leds (#3) ... 0xB6
        // 7 seg leds (#4) ... 0xD4
        // 7 seg leds (#5) ... 0xE6
        // 7 seg leds (#6) ... 0xE7
        // 7 seg leds (#7) ... 0x74
        // 7 seg leds (#8) ... 0xF7
        // 7 seg leds (#9) ... 0xF6

        set_number(0xB6);
        set_number(0xE7);
        set_number(0xF6);
        
        // // セットする値が少ない場合は、下記の通り、個別に指示したほうが、データ容量が抑えられる
        // ra = 0xF6;
        // // turn off leds
        // PUT32(SIO_GPIO_OUT_CLR, ra << 12);
        // DELAY(0x200000);
        // // turn on leds
        // PUT32(SIO_GPIO_OUT_SET, ra << 12);
        // DELAY(0x200000);

        // ra = 0xB3;
        // // turn off leds
        // PUT32(SIO_GPIO_OUT_CLR, ra << 12);
        // DELAY(0x200000);
        // // turn on leds
        // PUT32(SIO_GPIO_OUT_SET, ra << 12);
        // DELAY(0x200000);

    }
    return 0;
}

//-------------------------------------------------------------------------
//--- Original program ---
// 
// Copyright (c) 2021 David Welch dwelch@dwelch.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//-------------------------------------------------------------------------
//
// Modified program
// Copyright (c) 2025 grekichi
//
