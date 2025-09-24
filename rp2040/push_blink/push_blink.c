void PUT32 (unsigned int, unsigned int);
unsigned int GET32 (unsigned int);
// void DELAY (unsigned int);

#define RESETS_BASE                 0x4000C000

#define RESETS_RESET_RW             (RESETS_BASE+0x0+0x0000)
#define RESETS_RESET_XOR            (RESETS_BASE+0x0+0x1000)
#define RESETS_RESET_SET            (RESETS_BASE+0x0+0x2000)
#define RESETS_RESET_CLR            (RESETS_BASE+0x0+0x3000)

#define RESETS_WDSEL_RW             (RESETS_BASE+0x4+0x0000)
#define RESETS_WDSEL_XOR            (RESETS_BASE+0x4+0x1000)
#define RESETS_WDSEL_SET            (RESETS_BASE+0x4+0x2000)
#define RESETS_WDSEL_CLR            (RESETS_BASE+0x4+0x3000)

#define RESETS_RESET_DONE_RW        (RESETS_BASE+0x8+0x0000)
#define RESETS_RESET_DONE_XOR       (RESETS_BASE+0x8+0x1000)
#define RESETS_RESET_DONE_SET       (RESETS_BASE+0x8+0x2000)
#define RESETS_RESET_DONE_CLR       (RESETS_BASE+0x8+0x3000)

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

#define IO_BANK0_GPIO14_STATUS_RW   (IO_BANK0_BASE+0x070+0x0000)
#define IO_BANK0_GPIO14_STATUS_XOR  (IO_BANK0_BASE+0x070+0x1000)
#define IO_BANK0_GPIO14_STATUS_SET  (IO_BANK0_BASE+0x070+0x2000)
#define IO_BANK0_GPIO14_STATUS_CLR  (IO_BANK0_BASE+0x070+0x3000)

#define IO_BANK0_GPIO14_CTRL_RW     (IO_BANK0_BASE+0x074+0x0000)
#define IO_BANK0_GPIO14_CTRL_XOR    (IO_BANK0_BASE+0x074+0x1000)
#define IO_BANK0_GPIO14_CTRL_SET    (IO_BANK0_BASE+0x074+0x2000)
#define IO_BANK0_GPIO14_CTRL_CLR    (IO_BANK0_BASE+0x074+0x3000)

#define IO_BANK0_GPIO15_STATUS_RW   (IO_BANK0_BASE+0x078+0x0000)
#define IO_BANK0_GPIO15_STATUS_XOR  (IO_BANK0_BASE+0x078+0x1000)
#define IO_BANK0_GPIO15_STATUS_SET  (IO_BANK0_BASE+0x078+0x2000)
#define IO_BANK0_GPIO15_STATUS_CLR  (IO_BANK0_BASE+0x078+0x3000)

#define IO_BANK0_GPIO15_CTRL_RW     (IO_BANK0_BASE+0x07C+0x0000)
#define IO_BANK0_GPIO15_CTRL_XOR    (IO_BANK0_BASE+0x07C+0x1000)
#define IO_BANK0_GPIO15_CTRL_SET    (IO_BANK0_BASE+0x07C+0x2000)
#define IO_BANK0_GPIO15_CTRL_CLR    (IO_BANK0_BASE+0x07C+0x3000)

// #define IO_BANK0_GPIO25_STATUS_RW   (IO_BANK0_BASE+0x0C8+0x0000)
// #define IO_BANK0_GPIO25_STATUS_XOR  (IO_BANK0_BASE+0x0C8+0x1000)
// #define IO_BANK0_GPIO25_STATUS_SET  (IO_BANK0_BASE+0x0C8+0x2000)
// #define IO_BANK0_GPIO25_STATUS_CLR  (IO_BANK0_BASE+0x0C8+0x3000)

// #define IO_BANK0_GPIO25_CTRL_RW     (IO_BANK0_BASE+0x0CC+0x0000)
// #define IO_BANK0_GPIO25_CTRL_XOR    (IO_BANK0_BASE+0x0CC+0x1000)
// #define IO_BANK0_GPIO25_CTRL_SET    (IO_BANK0_BASE+0x0CC+0x2000)
// #define IO_BANK0_GPIO25_CTRL_CLR    (IO_BANK0_BASE+0x0CC+0x3000)


int push_blink(void)
{
    // IO_BANK0のリセット状態を解除する（=使える状態にする）
    PUT32(RESETS_RESET_CLR, 1 << 5);  // ビット５がIO_BANK0に相当（rp2040datasheet p176より）
    // リセットが完了するまでの待機処理
    while (1)
    {
        // リセットが完了したレジスタのチェック：ビット５が１かどうかをチェック（=リセット完了なら1になる）
        if ((GET32(RESETS_RESET_DONE_RW) & (1 << 5)) != 0)
            break; // IO_BANK0のリセットが完了していればループを抜ける
    }

    // GPIO 15 の出力を無効化する（初期化のための安全動作）
    PUT32(SIO_GPIO_OE_CLR, 1 << 15);

    // GPIO 15 の値をクリアする
    PUT32(SIO_GPIO_OUT_CLR, 1 << 15);

    // GPIO 14,15 をSIO機能（ソフトウェア制御モード）に設定する
    PUT32(IO_BANK0_GPIO14_CTRL_RW, 5);
    PUT32(IO_BANK0_GPIO15_CTRL_RW, 5);

    // GPIO 15 の出力を有効化する
    PUT32(SIO_GPIO_OE_SET, 1 << 15);

    while (1)
    {
        if (((GET32(SIO_GPIO_IN) >> 14) & 1) == 1)
        {
            // turn on the led
            PUT32(SIO_GPIO_OUT_SET, 1 << 15);
        }
        else
        {
        // turn on the led
            PUT32(SIO_GPIO_OUT_CLR, 1 << 15);
        }
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
