#include "rp2040.h"

void PUT32 (unsigned int, unsigned int);
unsigned int GET32 (unsigned int);
void DELAY (unsigned int);

// GPIO10 --> SER(Serial data input pin) に接続
// GPIO11 --> RCLK(Latch clock pin) に接続
// GPIO12 --> SRCLK(Clock input pin) に接続

// QA --> A(7) ＊（）内の数字は、7segLED端子の番号
// QB --> B(6)
// QC --> C(4)
// QD --> D(2)
// QE --> E(1)
// QF --> F(9)
// QG --> G(10)
// QH --> DP(5)

static void shift_init(void)
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

    // GPIO 10 ~ 12 の出力を無効化する（初期化のための安全動作）
    PUT32(SIO_GPIO_OE_CLR, 0x7 << 10);
    // GPIO 10 ~ 12 の値をクリアする
    PUT32(SIO_GPIO_OUT_CLR, 0x7 << 10);
    // GPIO 10 ~ 12 をSIO機能（ソフトウェア制御モード）に設定する
    PUT32(IO_BANK0_GPIO10_CTRL_RW, 5);
    PUT32(IO_BANK0_GPIO11_CTRL_RW, 5);
    PUT32(IO_BANK0_GPIO12_CTRL_RW, 5);
}

static void set_register(unsigned int val)
{
    unsigned int idx;
    unsigned int bv;

    for (idx = 0; idx < 8; idx++)
    {
        PUT32(SIO_GPIO_OUT_CLR, 1 << 10);
        
        PUT32(SIO_GPIO_OUT_CLR, 1 << 12);  // クロックをLowにして、待機

        bv = (val >> idx) & 0x01;
        if (bv == 1)
        {
            PUT32(SIO_GPIO_OUT_SET, 1 << 10);
        }

        PUT32(SIO_GPIO_OUT_SET, 1 << 12);  // クロックをHighにして、データを1ビット進める
    }
}


int sl_shift(void)
{
    shift_init();

    // GPIO 10 ~ 12 の出力を有効化する
    PUT32(SIO_GPIO_OE_SET, 0x7 << 10);

    unsigned int num[] = {0x03, 0x9F, 0x25, 0x0D, 0x99, 0x49, 0x41, 0x1B, 0x01, 0x09};

    // 7 seg leds shift (#0) ... 0x03
    // 7 seg leds shift (#1) ... 0x9F
    // 7 seg leds shift (#2) ... 0x25
    // 7 seg leds shift (#3) ... 0x0D
    // 7 seg leds shift (#4) ... 0x99
    // 7 seg leds shift (#5) ... 0x49
    // 7 seg leds shift (#6) ... 0x41
    // 7 seg leds shift (#7) ... 0x1B
    // 7 seg leds shift (#8) ... 0x01
    // 7 seg leds shift (#9) ... 0x09

    while (1)
    {
        for (int i = 0; i < (sizeof(num) / sizeof(num[0])); i++)
        {
            PUT32(SIO_GPIO_OUT_CLR, 1 << 11); // ラッチをLowに設定して待機
            set_register(num[i]);
            PUT32(SIO_GPIO_OUT_SET, 1 << 11); // ラッチをHighにしてストレージレジスタへデータ転送
            DELAY(0x200000);
        }
    }

    return 0;
}

//-------------------------------------------------------------------------
// 
// Copyright (c) 2025 grekichi
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//-------------------------------------------------------------------------
