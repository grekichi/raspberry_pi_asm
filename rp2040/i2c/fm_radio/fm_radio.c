#include "rp2040.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "rda5807m.h"

void PUT32 (uint32_t, uint32_t);
uint32_t GET32 (uint32_t);
void WFI(void);

// GPIO12(I2C0-SDA) --> RDA5807MのSDAに接続
// GPIO13(I2C0-SDL) --> RDA5807MのSDLに接続

static void clock_init(void)
{
    // 125MHzに設定する
    PUT32(CLK_SYS_RESUS_CTRL_RW, 0x00);  // RESUS無効化、time out を0サイクルに設定
    PUT32(XOSC_CTRL_RW, 0xAA0 | (0xFAB << 12));  // 水晶発振器を1-15MHzレンジに設定、水晶発振器enable
    PUT32(XOSC_STARTUP_RW, 0xC4);  // 256 * 196 = 50176 cycles に設定
    while (!(GET32(XOSC_STATUS_RW) & 0x80000000));  // 発振器の動作確認
    PUT32(CLK_REF_CTRL_RW, 0x2);  // SRC -> XOSC_CLKSRC
    
    // PLL_SYSのリセット状態を解除する（=使える状態にする）
    PUT32(RESETS_RESET_CLR, 1 << 12);  // ビット12がPLL_SYSに相当
    while (!(GET32(RESETS_RESET_DONE_RW) & (1 << 12)));

    // PLL_SYSを125MHzに設定する
    PUT32(PLL_SYS_PWR_RW, (1 << 5) | (1 << 3) | (1 << 0));  // PWR 電源off
    PUT32(PLL_SYS_CS_RW, 0);  // REFDIV=1
    PUT32(PLL_SYS_FBDIV_INT_RW, 125);  // VCO=1500MHz *63に設定して、PD2=1にすると同等設定で消費電力を抑えられる
    PUT32(PLL_SYS_PWR_RW, 1 << 3);  // POSTDIVPD=1で保持
    while (!(GET32(PLL_SYS_CS_RW) & (1 << 31)));  // PLLのロック状況を確認
    PUT32(PLL_SYS_PRIM_RW, (6 << 16) | (2 << 12));  // PD1->6, PD2->2
    PUT32(PLL_SYS_PWR_RW, 0);  // Power -> on, PD=0, VCOPD=0, POSTDIVPD=0
    
    PUT32(CLK_SYS_CTRL_CLR, 1 << 0);  // SRCを一旦CLK_REF(=XOSC)に退避する 
    PUT32(CLK_SYS_CTRL_CLR, 0x7 << 5);  // AUXSRC 7:5bits クリア -> PLL_SYS
    PUT32(CLK_SYS_CTRL_RW, (0x0 << 5) | (0x1 << 0));  // AUXSRC -> PLL_SYS, CLKSRC_CLK_SYS_AUX

    PUT32(CLK_PERI_CTRL_CLR, 0x7 << 5);  // AUXSRC 7:5bits クリア -> CLK_SYS
    PUT32(CLK_PERI_CTRL_RW, (1 << 11) | (0x1 << 5));  // enable, CLKSRC_PLL_SYS
}

static void reset_subsys(void)
{
    // IO_BANK0のリセット状態を解除する（=使える状態にする）
    PUT32(RESETS_RESET_CLR, 1 << 5);  // ビット５がIO_BANK0に相当
    // IO_BANK0のリセットが完了していればループを抜ける
    while (!(GET32(RESETS_RESET_DONE_RW) & (1 << 5)));

    // PADS_BANK0のリセット状態を解除する（=使える状態にする）
    PUT32(RESETS_RESET_CLR, 1 << 8);  // ビット8がPADS_BANK0に相当
    // PADS_BANK0のリセットが完了していればループを抜ける
    while (!(GET32(RESETS_RESET_DONE_RW) & (1 << 8)));

    // I2C0のリセット状態を解除する（=使える状態にする）
    PUT32(RESETS_RESET_CLR, 1 << 3);  // ビット3がI2C0に相当
    // I2C0のリセットが完了していればループを抜ける
    while (!(GET32(RESETS_RESET_DONE_RW) & (1 << 3)));
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

static void led_init(void)
{
    uint32_t ra;

    PUT32(SIO_GPIO_OE_CLR, 1 << 25);  // GPIO25出力を無効化（初期化のための安全動作）
    PUT32(SIO_GPIO_OUT_CLR, 1 << 25);  // GPIO25の値をクリア
    ra = GET32(PADS_BANK0_GPIO25_RW);
    ra ^= 0x40;  // if input disabled then enable
    ra &= 0xC0;  // if output disabled then enable
    PUT32(PADS_BANK0_GPIO25_XOR, ra);
    PUT32(IO_BANK0_GPIO25_CTRL_RW, 5);  // SIO
    PUT32(SIO_GPIO_OE_SET, 1 << 25);  // GPIO25出力を有効化
}

static void uart_init(void)
{
    // uint32_t ra;

    PUT32(RESETS_RESET_CLR, (1 << 22));  // UART0のリセットを解除
    while (!(GET32(RESETS_RESET_DONE_RW) & (1 << 22)));

    PUT32(UART0_CR_RW, 0x00000000);  // UART初期化
    // ボーレート分周器の設定
    // 水晶発振器(XOSC)使用のため、clk_periの周波数は、125MHz = (125 * 10^6)
    // 通信速度(ボーレート)は、115200 bps に設定
    // 計算方法: clk_periの周波数 / (ボーレート * 16倍)
    // 割った結果の小数部分は、64倍した値を小数ボーレート分周値とする
    // 125000000 / (16 * 115200) = 67.8168
    // 0.8168 * 64 = 52.2752 -> + 0.5 = 52.7752
    PUT32(UART0_IBRD_RW, 67);  // 整数ボーレート分周値
    PUT32(UART0_FBRD_RW, 53);  // 小数ボーレート分周値
    // ボーレート57600の場合： 13.0208, 0.0208*64=1.3312 

    // 0 11 1 0 0 0 0 = (0111 0000) が設定値
    PUT32(UART0_LCR_H_RW, 0x70);  // １フレームのデータビット数 8bits, FIFO有効化, パリティビットなし
    PUT32(UART0_CR_RW, (1 << 9) | (1 << 8) | (1 << 0));  // TXE:有効、RXE:有効

    // ra = GET32(PADS_BANK0_GPIO0_RW);  // UART_TX
    // ra ^= 0x40;  // 0100 0000 : if input disabled then enable
    // ra &= 0xC0;  // 1100 0000 : if output disabled then enable

    // GPIO0の設定
    PUT32(PADS_BANK0_GPIO0_CLR, (1 << 7) | (1 << 6));  // OD/IE -> disable
    PUT32(PADS_BANK0_GPIO0_CLR, 1 << 2);  // Pull down disable 
    PUT32(PADS_BANK0_GPIO0_SET, 1 << 3);  // Pull up enable
    PUT32(IO_BANK0_GPIO0_CTRL_RW, 2);  // UART0_TX

    // GPIO1の設定
    PUT32(PADS_BANK0_GPIO1_SET, 1 << 6);  // IE enable
    PUT32(PADS_BANK0_GPIO1_CLR, 1 << 2);  // Pull down disable 
    PUT32(PADS_BANK0_GPIO1_SET, 1 << 3);  // Pull up enable
    PUT32(IO_BANK0_GPIO1_CTRL_RW, 2);  // UART0_RX
}

static void uart_send(uint32_t x)
{
    while (GET32(UART0_FR_RW) & (1 << 5));
    PUT32(UART0_DR_RW, x);
}

static void uart_send_char(uint8_t x)
{
    while (GET32(UART0_FR_RW) & (1 << 5));
    PUT32(UART0_DR_RW, x);
}

static void uart_send_str(uint8_t *y)
{
    while (*y != '\0')
    {
        uart_send_char(*y);
        y++;
    }
    uart_send_char('\r');
    uart_send_char('\n');
    uart_send_char('\n');
}

static int uart_getc_nonblocking(void)
{
    // RXFE(bit 4)が "1" なら受信データ無し
    if (GET32(UART0_FR_RW) & (1 << 4))
    {
        return -1;
    }

    // 受信データレジスタから１バイト取得して返す
    return (int)(GET32(UART0_DR_RW) & (0xFF));
}


static void i2c_init(void)
{
    PUT32(I2C0_IC_ENABLE_RW, 0);  // I2C0初期化
    while (GET32(I2C0_IC_ENABLE_RW) & 1);

    /* ==========================================================
     * 1. I2C0 BUS RECOVERY
     *    GPIO12 = SDA, GPIO13 = SCL
     * ========================================================== */

    /* --- GPIO (SIO) 機能に切替 --- */
    PUT32(IO_BANK0_GPIO12_CTRL_RW, 5);   // FUNCSEL = SIO (GPIO)
    PUT32(IO_BANK0_GPIO13_CTRL_RW, 5);

    /* --- Pad 設定 --- */
    // 入力有効（SDA 状態確認用）
    PUT32(PADS_BANK0_GPIO12_SET, 1 << 6);
    PUT32(PADS_BANK0_GPIO13_SET, 1 << 6);

    // 内蔵 Pull-down は使わない
    PUT32(PADS_BANK0_GPIO12_CLR, 1 << 2);
    PUT32(PADS_BANK0_GPIO13_CLR, 1 << 2);
    // 内蔵 Pull-up を使用
    PUT32(PADS_BANK0_GPIO12_SET, 1 << 3);
    PUT32(PADS_BANK0_GPIO13_SET, 1 << 3);

    /* --- SDA: Hi-Z, SCL: 出力 --- */
    PUT32(SIO_GPIO_OE_CLR, 1 << 12);     // SDA = input (Hi-Z)
    PUT32(SIO_GPIO_OE_SET, 1 << 13);     // SCL = output

    /* SCL idle = High */
    PUT32(SIO_GPIO_OUT_SET, 1 << 13);
    delay_us(1);  // 1us

    /* ---  貼り付き対策のため、数回クロック出力 --- */
    for (int i = 0; i < 9; i++) {
        PUT32(SIO_GPIO_OUT_CLR, 1 << 13);  // SCL Low
        delay_us(1);
        PUT32(SIO_GPIO_OUT_SET, 1 << 13);  // SCL High
        delay_us(1);
    }

    /* --- STOP 条件生成 (SDA↑ while SCL↑) --- */
    PUT32(SIO_GPIO_OUT_SET, 1 << 13);     // SCL High
    delay_us(1);

    // ==========================================
    // FIFO trigger
    PUT32(I2C0_IC_TX_TL_RW, 0);
    PUT32(I2C0_IC_RX_TL_RW, 0);

    // DMA off
    PUT32(I2C0_IC_DMA_CR_RW, 0);

    // interrupt clear
    GET32(I2C0_IC_CLR_INTR_RW);

    // I2C0をマスターモード、 高速モード、7bitアドレス送信 に設定
    PUT32(
        I2C0_IC_CON_RW,
        1 << 0 |  // MASTER_MODE -> enabled
        1 << 1 |  // SPEED -> standard (*12MHz発振子ではstandardでしか動かせない)
        0 << 3 |  // IC_10BITADDR_SLAVE -> Slave 7bits addressing mode
        0 << 4 |  // IC_10BITADDR_MASTER -> Master 7bits addressing mode
        1 << 5 |  // IC_RESTART_EN -> enable
        1 << 6 |  // IC_SLAVE_DISABLE -> slave_disabled
        0 << 7 |  // STOP_DET_IFADDRESSED -> disbaled
        1 << 8 |  // TX_EMPTY_CTRL -> enabled
        0 << 9    // RX_FIFO_FULL_HLD_CTRL -> disabled
    );    
    // I2C0 標準モードのSCLクロックのHigh期間カウント
    PUT32(I2C0_IC_SS_SCL_HCNT_RW, 500);  // FSの場合、このレジスタ値は、(I2C0_IC_FS_SPKLEN_RW + 5)よりも大きくなければいけない 
    // I2C0 標準モードのSCLクロックのLow期間カウント
    PUT32(I2C0_IC_SS_SCL_LCNT_RW, 741);  // FSの場合、このレジスタ値は、(I2C0_IC_FS_SPKLEN_RW + 7)よりも大きくなければいけない
    // I2C0 スパイク抑制ロジックによってフィルタリングされる最長スパイクの持続時間
    PUT32(I2C0_IC_FS_SPKLEN_RW, 4);  // 3:32ns 4:40ns 5:48ns
    // I2C0 SDAホールド持続時間
    PUT32(I2C0_IC_SDA_HOLD_RW, 75); // 600ns 最低値は0x1

    // IO_BANK0 setting
    PUT32(IO_BANK0_GPIO12_CTRL_RW, 3);  // I2C0-SDA
    PUT32(IO_BANK0_GPIO13_CTRL_RW, 3);  // I2C0-SCL

    // Input有効化
    PUT32(PADS_BANK0_GPIO12_SET, 1 << 6);  // IE -> enable
    PUT32(PADS_BANK0_GPIO13_SET, 1 << 6);  // IE -> enable

    // I2C0を有効化
    PUT32(I2C0_IC_ENABLE_RW, 1);
}

static inline int write_i2c_bytes(const uint8_t address, const uint8_t *data, size_t len)
{
    // TX_FIFOが空くのを確認(TFE -> 0x1)
    while (!(GET32(I2C0_IC_STATUS_RW) & (1 << 2)));

    // ABORT/STOPクリア
    GET32(I2C0_IC_CLR_TX_ABRT_RW);
    GET32(I2C0_IC_CLR_STOP_DET_RW);
    GET32(I2C0_IC_CLR_INTR_RW);

    PUT32(I2C0_IC_ENABLE_RW, 0);
    while (GET32(I2C0_IC_ENABLE_RW) & 1);
    PUT32(I2C0_IC_TAR_RW, address & 0x7F); // RDA5807のシーケンシャルアドレス (0x10)
    PUT32(I2C0_IC_ENABLE_RW, 1);  
    delay_us(40);  // 40μs待ち

    for (size_t i = 0; i < len; i++)
    {
        uint32_t cmd = data[i];  
        
        // 最初のバイト
        if (i == 0)
        {
            cmd |= (0 << 10);
            cmd |= (0 << 9);
        }
        // 最後のバイト → STOP
        if (i == len - 1)
        {
            cmd |= (1 << 9);
        }

        PUT32(I2C0_IC_DATA_CMD_RW, cmd);

        // TX_EMPTY確認
        while(!(GET32(I2C0_IC_RAW_INTR_STAT_RW) & (1 << 4))); 

        // ABORT確認
        uint32_t abort_reason = GET32(I2C0_IC_TX_ABRT_SOURCE_RW);
        if (abort_reason != 0)
        {
            GET32(I2C0_IC_CLR_TX_ABRT_RW);  // ABORTデータクリア
            
            // STOP_DET割り込みがアクティブになるのを確認
            while (!(GET32(I2C0_IC_RAW_INTR_STAT_RW) & (1 << 9)));
            GET32(I2C0_IC_CLR_STOP_DET_RW);

            uart_send_str("Failed to write I2C command");
            return -1;  // 書込失敗
        }
    }

    // 実際のI2Cバス上でストップコンディションが完了するのを待つ
    while (!(GET32(I2C0_IC_RAW_INTR_STAT_RW) & (1 << 9)));
    GET32(I2C0_IC_CLR_STOP_DET_RW);

    uart_send_str("Succeeded to write I2C command");
    return 0;  // 成功
}

static void rrd102_init(uint16_t default_freq_tenths)
{
    // ステップ周波数は100kHzで計算するので、設定周波数値は最初から10倍しておく
    if (default_freq_tenths < 760 || default_freq_tenths > 1080)
    {
        default_freq_tenths = 800;  // 範囲外なら80.0MHz に設定
        uart_send_str("set default frequency: 80.0MHz");
    }

    uint16_t channel = default_freq_tenths - 760;
    uart_send_str("Initializing RRD102 ...");

    // 例: レジスタ 0x02 から 0x05 までの連続書き込み（合計8バイト）
    // 0x03設定補足 : TUNE -> 1(0x10), BAND -> 10(2 << 2)
    uint8_t data[8];
    data[0] = 0xC0; // Reg 0x02 High (ENABLE=1, DMUTE=1)
    data[1] = 0x01; // Reg 0x02 Low  (SKMODEなど)
    data[2] = (channel >> 2) & 0xFF; // Reg 0x03 High (CHAN[9:2])
    data[3] = ((channel & 0x03) << 6) | 0x10 | (2 << 2); // Reg 0x03 Low (CHAN[1:0] + TUNE)
    data[4] = 0x04; // Reg 0x04 High 
    data[5] = 0x00; // Reg 0x04 Low  (AUDIO / 簡易設定など)
    data[6] = 0x88; // Reg 0x05 High (DAC Enable / 音量関連など)
    data[7] = 0x8F; // Reg 0x05 Low  (Volume 最大値など)

    write_i2c_bytes(RDA_SEQ_ADDR, data, 8);
    uart_send_str("Tuned successfully");
}

static void set_freq(uint16_t freq_tenths)
{
    if (freq_tenths < 760 || freq_tenths > 1080)
    {
        uart_send_str("Error: Out of Range");
        return;
    }

    uint16_t channel = freq_tenths - 760;
    uart_send_str("Tuning frequency ...");

    // 周波数変更時は、すでに動作中なのでレジスタ 0x02 と 0x03 の4バイトだけでOK
    uint8_t freq_data[4];
    freq_data[0] = 0xC0; // Reg 0x02 High (ENABLE=1, DMUTE=1)
    freq_data[1] = 0x01; // Reg 0x02 Low  (SKMODEなど)
    freq_data[2] = (channel >> 2) & 0xFF; // Reg 0x03 High (CHAN[9:2])
    freq_data[3] = ((channel & 0x03) << 6) | 0x10 | (2 << 2); // Reg 0x03 Low (CHAN[1:0] + TUNE)

    write_i2c_bytes(RDA_SEQ_ADDR, freq_data, 4);
    uart_send_str("Tuned successfully !");
}

static void shutdown_radio(void)
{
    uint8_t data[4] = {0x00, 0x01, 0x00, 0x00};

    write_i2c_bytes(RDA_SEQ_ADDR, data, 4);

    // 100ミリ秒停止
    delay_ms(100);
}

// 文字列を整数に変換する自作関数 (例: "800" -> 800)
int my_atoi(const char *str)
{
    int res = 0;
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res;
}

volatile uint32_t systick_ms = 0;
void SysTick_Handler(void)
{
    systick_ms++;
    if (systick_ms >= 1000)
    {
        systick_ms = 0;
        PUT32(SIO_GPIO_OUT_XOR, 1 << 25);  // GPIO25出力を切替
    }
}


int fm_radio(void)
{
    clock_init();
    reset_subsys();
    led_init();
    // monitor_init();
    i2c_init();
    // clk_periの有効待ち
    while (!(GET32(CLK_PERI_CTRL_RW) & (1 << 11)));
    
    // UART初期化
    uart_init();
    
    PUT32(SYST_RVR, 125000-1);  // 125MHz設定のため、1msカウントにセット（1s設定できないため）
    PUT32(SYST_CVR, 0);  // 125MHz設定のため、1msカウントにセット（1s設定できないため）
    PUT32(SYST_CSR, 0x07);  // プロセッサクロック使用、例外ステータス保留、カウンター有効
    
    // ラジオを一旦シャットダウンする
    shutdown_radio();

    // RRD-102初期化設定
    rrd102_init(700);

    uart_send_str("\r\n=== RRD102 UART Radio Controller ===");
    uart_send_str("Enter frequency (e.g. 800 for 80.0MHz): \r\n> ");

    char rx_buf[8];
    uint8_t buf_idx = 0;

    while (1)
    {
        int c = uart_getc_nonblocking();

        if (c != -1)
        {
            if (c == '\r' || c == '\n')
            {
                uart_send_char('\r');
                uart_send_char('\n');

                if (buf_idx > 0)
                {
                    rx_buf[buf_idx] = '\0';
                    int freq = my_atoi(rx_buf);

                    if (freq >= 760 && freq <= 1080)
                    {
                        set_freq((uint16_t)freq);
                    }
                    else
                    {
                        uart_send_str("Error: Out of Range (760 - 1080)");
                    }
                    buf_idx = 0;
                }
                uart_send_char('>');
                uart_send_char(' ');
            }
            else if (c == '\b' || c == 127)  // バックスペース対応
            {
                if (buf_idx > 0)
                {
                    buf_idx--;
                    uart_send_char('\b');
                    uart_send_char(' ');
                    uart_send_char('\b');
                }
            }
            else if (buf_idx < sizeof(rx_buf) - 1)
            {
                rx_buf[buf_idx++] = (char)c;
                // エコーバック（入力された文字を画面に送り返す）
                uart_send_char(c);
            }
        }
        else
        {
            // 文字がないときだけ WFI でCPUをスリープさせて省電力化（文字があれば即座に処理）
            WFI();
        }
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
