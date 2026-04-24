#include "rp2040.h"

void PUT32(unsigned int, unsigned int);
unsigned int GET32(unsigned int);
void WFI(void);
// void DELAY(unsigned int);


static void clock_init(void)
{
    PUT32(CLK_SYS_RESUS_CTRL_RW, 0x00);  // time out を0サイクルに設定
    PUT32(XOSC_CTRL_RW, 0xAA0);  // 水晶発振器を1-15MHzレンジに設定
    PUT32(XOSC_STARTUP_RW, 47);  // 256 * 47 = 12032cycles(=約1ms)を 起動遅延として設定
    PUT32(XOSC_CTRL_RW, 0xFAB << 12);  // 水晶発振器を使用可能(enable)にする 
    while (1)
    {
        if ((GET32(XOSC_STATUS_RW) & 0x80000000) != 0) break;  // 発振器の動作確認
    }
    PUT32(CLK_REF_CTRL_RW, 0x2);  // XOSC Clock source
    PUT32(CLK_SYS_CTRL_RW, 0x0);  // CLK_REF
    PUT32(CLK_PERI_CTRL_RW, (1 << 11) | (0x4 << 5));  // enable, XOSC_CLKSRC
}

static void uart_init(void)
{    
    PUT32(RESETS_RESET_CLR, (1 << 5));  // IO_BANK0のリセットを解除
    while (1)
    {
        if ((GET32(RESETS_RESET_DONE_RW) & (1 << 5)) != 0) break;
    }
    
    PUT32(RESETS_RESET_CLR, (1 << 8));  // PADS_BANK0のリセットを解除
    while (1)
    {
        if ((GET32(RESETS_RESET_DONE_RW) & (1 << 8)) != 0) break;
    }
    
    PUT32(RESETS_RESET_CLR, (1 << 22));  // UART0のリセットを解除
    while (1)
    {
        if ((GET32(RESETS_RESET_DONE_RW) & (1 << 22)) != 0) break;
    }
    
    // ボーレート分周値の計算と設定
    // 水晶発振器(XOSC)使用のため、clk_periの周波数は、12MHz = (12 * 10^6)
    // 通信速度(ボーレート)は、115200 bps に設定
    // 計算方法: clk_periの周波数 / (ボーレート * 16倍)
    // 割った結果の小数部分は、64倍した値を小数ボーレート分周値とする
    // 12000000 / (16 * 115200) = 6.5104 = 6 + 0.5104
    // m = 0.5104 * 64 + 0.5 = 33.1656
    PUT32(UART0_BASE_UARTIBRD_RW, 6);  // ボーレート分周値 整数部
    PUT32(UART0_BASE_UARTFBRD_RW, 33);  // ボーレート分周値 小数部
    
    // 0 11 1 0 0 0 0 = (0111 0000) が設定値
    PUT32(UART0_BASE_UARTLCR_H_RW, 0x70);  // １フレームのデータビット数 8bits, FIFO有効化, パリティビットなし
    PUT32(UART0_BASE_UARTCR_RW, (1 << 8) | (1 << 0));  // UART送信部有効 受信部使用時は(1<<9)を追加要

    PUT32(PADS_BANK0_GPIO0_CLR, 1 << 2);  // Pull down disable 
    PUT32(PADS_BANK0_GPIO0_SET, 1 << 3);  // Pull up enable

    PUT32(IO_BANK0_GPIO0_CTRL_RW, 2);  // UART0_TX機能を選択
    // PUT32(IO_BANK0_GPIO1_CTRL_RW, 2);  // UART0_RX機能を選択
}

static void uart_send(char x)
{
    while (1)
    {
        if ((GET32(UART0_BASE_UARTFR_RW) & (1 << 5)) == 0) break;
    }
    PUT32(UART0_BASE_UARTDR_RW, x);
}

static void uart_send_str(char *y)
{
    while (*y != '\0')
    {
        uart_send(*y);
        y++;
    }
    uart_send('\r');
    uart_send('\n');
    uart_send('\n');
}

static void led_init(void)
{
    unsigned int ra;

    PUT32(RESETS_RESET_CLR, 1 << 5);  // IO_BANK0のリセットを解除
    while (1)
    {
        if ((GET32(RESETS_RESET_DONE_RW) & (1 << 5)) != 0) break;  // 解除の終了確認
    }

    PUT32(RESETS_RESET_CLR, 1 << 8);  // PADS_BANK0のリセットを解除
    while (1)
    {
        if ((GET32(RESETS_RESET_DONE_RW) & (1 << 8)) != 0) break;
    }

    PUT32(SIO_GPIO_OE_CLR, 1 << 25);  // GPIO25出力を無効化（初期化のための安全動作）
    PUT32(SIO_GPIO_OUT_CLR, 1 << 25);  // GPIO25の値をクリア
    ra = GET32(PADS_BANK0_GPIO25_RW);
    ra ^= 0x40;  // if input disabled then enable
    ra &= 0xC0;  // if output disabled then enable
    PUT32(PADS_BANK0_GPIO25_XOR, ra);
    PUT32(IO_BANK0_GPIO25_CTRL_RW, 5);  // SIO
    PUT32(SIO_GPIO_OE_SET, 1 << 25);  // GPIO25出力を有効化
}

void systick_handler(void)
{
    PUT32(SIO_GPIO_OUT_XOR, 1 << 25);  // GPIO25出力を停止
    GET32(SYST_CSR);
}


int uart(void)
{
    char str[] = "Hello, world !";
    // char str[] = "What are you doing here ?";
    // char str[] = "Are you happy ?";

    clock_init();
    uart_init();
    led_init();

    PUT32(SYST_CSR, 0x00000004);  // プロセッサクロックの使用宣言
    PUT32(SYST_RVR, 12000000-1);  // 
    PUT32(SYST_CVR, 12000000-1);  // 
    PUT32(SYST_CSR, 0x00000007);  // プロセッサクロック使用、例外ステータス保留、カウンター有効

    while (1)
    {
        WFI();
        uart_send_str(str);
        // DELAY(6000000);
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
