#include "rp2040.h"
#include <stdint.h>

#define PICO_NO_HARDWARE    1
#include "blink.pio.h"

#define LED_PIN             25u
#define PIO_INSTR_JMP_O     0x0000u  // PC を ０に戻すために使用


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

static void start_timer(void)
{
    // TICK: ENABLE, 6u(ROSCのため、6MHzで約1us) CYCLES
    PUT32(WATCHDOG_TICK_RW, ((1u << 9) | 6u));
}

static void delay_ms(uint32_t millisec)
{
    uint32_t start = GET32(TIMER_TIMERAWL_RW);
    while ((GET32(TIMER_TIMERAWL_RW) - start) < (millisec * 1000u));
}

static void pio_init(void)
{
    // SM0を停止 （安全のため）
    PUT32(PIO0_CTRL_RW, GET32(PIO0_CTRL_RW) & ~(1u << 0));

    // 命令メモリへの書き込み処理
    const uint32_t len = sizeof(blink_program_instructions) /
                        sizeof(blink_program_instructions[0]);
    
    for (uint32_t i = 0; i < len; i++)
    {
        PUT32(PIO0_INSTR_MEM0_RW + i * 4u, blink_program_instructions[i]);
    }

    // クロック分周 -> 1.0
    // 周波数 = クロック周波数 / (CLKDIV_INT + CLKDIV_FRAC / 256)
    PUT32(PIO0_SM0_CLKDIV_RW, 1u << 16);

    // EXECCTRL 繰り返し命令箇所の指定
    PUT32(PIO0_SM0_EXECCTRL_RW,
        ((uint32_t)blink_wrap << 12) | ((uint32_t)blink_wrap_target << 7));
    
    // SHIFTCTRL 右シフト  *autopullは使わない設定
    PUT32(PIO0_SM0_SHIFTCTRL_RW, (1u << 19) | (1u << 18));

    // PINCTRL SET_COUNT: 1, OUT_COUNT: 1, SET_BASE: 25pin, OUT_BASE: 25pin
    PUT32(PIO0_SM0_PINCTRL_RW,
        (1u << 26) | (1u << 20) | (LED_PIN << 5) | (LED_PIN << 0));
    
    // ステートマシンの内部状態クリア
    PUT32(PIO0_CTRL_RW, GET32(PIO0_CTRL_RW) | (1u << 8) | (1u << 4));
    // PC を 0 に戻す
    PUT32(PIO0_SM0_INSTR_RW, PIO_INSTR_JMP_O);

    // GPIO25 を PIO0 に接続
    PUT32(IO_BANK0_GPIO25_CTRL_RW, 6u);

    // SM0 を有効化
    PUT32(PIO0_CTRL_RW, GET32(PIO0_CTRL_RW) | (1u << 0));
}


int blink_pio(void)
{
    reset_subsys();
    start_timer();
    pio_init();

    while (1)
    {
        PUT32(PIO0_TXF0_RW, 1);  // 点灯
        delay_ms(1000);  // 1000ms待ち
        PUT32(PIO0_TXF0_RW, 0);  // 消灯
        delay_ms(1000);  // 1000ms待ち
    }

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