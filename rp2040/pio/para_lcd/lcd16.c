#include "rp2040.h"
#include <stdint.h>

// lcd16.pio.h は pico-sdk 用のヘッダだが、PICO_NO_HARDWARE=1 を定義すれば
// 命令配列と wrap 定数だけが使える (Makefileの -DPICO_NO_HARDWARE=1)。
#define PICO_NO_HARDWARE    1
#include "lcd16.pio.h"

#define PIO_INSTR_JMP_O     0x0000u  // PC を 0 に戻すために設定

void PUT32 (uint32_t, uint32_t);
uint32_t GET32 (uint32_t);


static void reset_subsys(void)
{
    // リセットビット設定
    unsigned int rb;
    // IO_BANK0:5, PADS_BANK0:8, PIO0:10, TIMER:21
    rb = (1 << 5) | (1 << 8) | (1 << 10) | (1 << 21);
    // リセット状態を解除する（=使える状態にする）
    PUT32(RESETS_RESET_CLR, rb);
    // リセットが完了していればループを抜ける
    while (!(GET32(RESETS_RESET_DONE_RW) & rb));
}

static void clock_init(void)
{
    // 12MHzに設定する
    PUT32(CLK_SYS_RESUS_CTRL_RW, 0x00);  // RESUS無効化
    PUT32(XOSC_STARTUP_RW, 47);  // 256 * 47 = 12032cycles(=約1ms) に設定
    PUT32(XOSC_CTRL_RW, 0xAA0 | (0xFAB << 12));  // 水晶発振器を1-15MHzレンジに設定、水晶発振器enable
    while (!(GET32(XOSC_STATUS_RW) & 0x80000000));  // 発振器の動作確認

    PUT32(CLK_REF_CTRL_RW, 0x2);  // SRC-> XOSC_CLKSRC
    while((GET32(CLK_REF_SELECTED_RW) != (1 << 2)));  // 切り替え完了待ち

    PUT32(CLK_SYS_CTRL_RW, 0x0);  // SRC-> clk_ref
    while (GET32(CLK_SYS_SELECTED_RW) != (1 << 0));

    // clk_peri を有効化(AUXSRC-> CLK_SYS, ENABLE-> 1) UART使用の場合に設定
    PUT32(CLK_PERI_CTRL_RW, (1 << 11) | (0x0 << 5));
    // clk_periの有効待ち
    while (!(GET32(CLK_PERI_CTRL_RW) & (1 << 11)));

    PUT32(WATCHDOG_TICK_RW, 12 | (1 << 9));  // 12cycles(12MHz) = 1us, enable
}

static void delay_us(uint32_t microsec)
{
    uint32_t start = GET32(TIMER_TIMERAWL_RW);
    while ((GET32(TIMER_TIMERAWL_RW) - start) < microsec);
}

static void delay_ms(uint32_t millisec)
{
    uint32_t start = GET32(TIMER_TIMERAWL_RW);
    while ((GET32(TIMER_TIMERAWL_RW) - start) < (millisec * 1000));
}


static void pio_init(void)
{
    // GPIO0~9 (DB0~DB7 + RS + E) を PIO0 に接続
    for (uint32_t i = 0; i < 10; i++)
    {
        PUT32(IO_BANK0_GPIO0_CTRL_RW + (i << 3), 6u);
    }

    // 命令メモリへの書き込み処理
    const uint32_t len = sizeof(lcd16_program_instructions) /
    sizeof(lcd16_program_instructions[0]);
    
    for (uint32_t j = 0; j < len; j++)
    {
        PUT32(PIO0_INSTR_MEM0_RW + j * 4u, lcd16_program_instructions[j]);
    }
    
    // SM0を停止
    PUT32(PIO0_CTRL_RW, 0);
    // クロック分周 -> 12.0 (= 1MHz)
    // 周波数 = クロック周波数 / (CLKDIV_INT + CLKDIV_FRAC / 256)
    PUT32(PIO0_SM0_CLKDIV_RW, 12u << 16);

    // EXECCTRL 繰り返し命令箇所の指定
    PUT32(PIO0_SM0_EXECCTRL_RW,
        ((uint32_t)lcd16_wrap << 12) | ((uint32_t)lcd16_wrap_target << 7));
    
    // SHIFTCTRL OUT/IN共に右シフト, autopullは使わない設定
    PUT32(PIO0_SM0_SHIFTCTRL_RW, (1u << 19) | (1u << 18));

    // ピン設定: SET_COUNT-> 5u (PIN数), SET_BASE-> 0
    // 使用ピンは10個のため、同じ処理を2回連続で行う
    PUT32(PIO0_SM0_PINCTRL_RW, (5u << 26) | (0u << 5));
    PUT32(PIO0_SM0_INSTR_RW, 0xE09F);  // set pindirs, 0x1fの命令バイナリ(111 00000 100 11111)
    PUT32(PIO0_SM0_PINCTRL_RW, (5u << 26) | (5u << 5));
    PUT32(PIO0_SM0_INSTR_RW, 0xE09F);

    // ピン割り当て: out=> GPIO0~8(9本), side-set=>GPIO9(1本) 
    // SIDESET_COUNT-> 1, OUT_COUNT->9 , SIDESET_BASE->9, OUT_BASE-> 0
    PUT32(PIO0_SM0_PINCTRL_RW, (1u << 29) | (9u << 20) | (9u << 10) | (0u << 0));
    
    // ステートマシンの内部状態クリア
    PUT32(PIO0_CTRL_RW, (1u << 8) | (1u << 4));
    // PC を 0 に戻す
    PUT32(PIO0_SM0_INSTR_RW, PIO_INSTR_JMP_O);
    // SM0 を有効化
    PUT32(PIO0_CTRL_RW, (1u << 0));
}

static void lcd_send(uint32_t rs, uint32_t data, uint32_t wait_us)
{
    while (GET32(PIO0_FSTAT_RW) & (1u << 16));
    PUT32(PIO0_TXF0_RW, (data & 0xFFu) | (rs ? 0x100 : 0) | (wait_us << 9));
}

// 命令実行の待ち時間
// 通常命令: 37us@270kHz, 内部OSC最小190kHzで約53us → 100us
// Clear/Home: 1.52ms@270kHz → 3ms(=3000us)
static void lcd_cmd(uint32_t c)
{
    lcd_send(0, c, (c <= 0x03) ? 3000 : 100);
}

static void lcd_data(uint32_t d)
{
    lcd_send(1, d, 100);
}

static void lcd_init(void)
{
    delay_ms(50);   // 電源投入後待ち時間 > 40ms

    lcd_send(0, 0x30, 5000);    // Function set -> 8bit : 1回目, > 4.1ms
    lcd_send(0, 0x30, 200);     // 2回目, > 100us
    lcd_send(0, 0x30, 200);     // 3回目

    lcd_cmd(0x38);  // Function Set -> 8bit, 2行, 5x8 (0011 1000)
    lcd_cmd(0x08);  // Display ON/OFF -> 表示OFF (0000 1000)
    lcd_cmd(0x01);  // Clear Display (クリア) (0000 0001)
    lcd_cmd(0x06);  // Entry Mode Set -> インクリメント, No shift (0000 0110)
    lcd_cmd(0x0C);  // Display ON/OFF -> 表示ON, カーソル無し、ブリンク無し (0000 1100)
}

static void lcd_set_cursor(uint32_t col, uint32_t row)
{
    // Cursor or Display Shift の設定を使用
    lcd_cmd(0x80 | ((row ? 0x40 : 0x00) + col));
}

static void lcd_puts(const char *s)
{
    while (*s)
    {
        lcd_data((uint8_t)*s++);
    }
}


int lcd16(void)
{
    reset_subsys();
    clock_init();
    // ステートマシンの設定
    pio_init();
    lcd_init();

    lcd_set_cursor(0, 0);
    lcd_puts("Hi, Grekichi !");
    lcd_set_cursor(0, 1);
    lcd_puts("SC1602BSLB Run !");

    for (;;) { }

    return 0;
}

//-------------------------------------------------------------------------
//
// Copyright (c) 2026 grekichi
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//-------------------------------------------------------------------------
//