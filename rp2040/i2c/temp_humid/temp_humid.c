#include "rp2040.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

void PUT32 (uint32_t, uint32_t);
uint32_t GET32 (uint32_t);
void DELAY (uint32_t);
void WFI(void);

// GPIO4(I2C0-SDA) --> SHT31のSDAに接続
// GPIO5(I2C0-SDL) --> SHT31のSDLに接続

static void clock_init(void)
{
    // 125MHzに設定する
    PUT32(CLK_SYS_RESUS_CTRL_RW, 0);  // RESUS無効化、time out を0サイクルに設定
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
    PUT32(RESETS_RESET_CLR, (1 << 22));  // UART0のリセットを解除
    while (!(GET32(RESETS_RESET_DONE_RW) & (1 << 22)));

    PUT32(UART0_BASE_UARTCR_RW, 0x00000000);  // UART初期化
    // ボーレート分周器の設定
    // 水晶発振器(XOSC)使用のため、clk_periの周波数は、125MHz = (125 * 10^6)
    // 通信速度(ボーレート)は、115200 bps に設定
    // 計算方法: clk_periの周波数 / (ボーレート * 16倍)
    // 割った結果の小数部分は、64倍した値を小数ボーレート分周値とする
    // 125000000 / (16 * 115200) = 67.8168
    // 0.8168 * 64 = 52.2752 -> + 0.5 = 52.7752
    PUT32(UART0_BASE_UARTIBRD_RW, 67);  // 整数ボーレート分周値
    PUT32(UART0_BASE_UARTFBRD_RW, 53);  // 小数ボーレート分周値

    // 0 11 1 0 0 0 0 = (0111 0000) が設定値
    PUT32(UART0_BASE_UARTLCR_H_RW, 0x70);  // １フレームのデータビット数 8bits, FIFO有効化, パリティビットなし
    PUT32(UART0_BASE_UARTCR_RW, (1 << 9) | (1 << 8) | (1 << 0));  // TXE:有効、RXE:有効

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
    while (GET32(UART0_BASE_UARTFR_RW) & (1 << 5));
    PUT32(UART0_BASE_UARTDR_RW, x);
}

static void uart_send_char(uint8_t x)
{
    while (GET32(UART0_BASE_UARTFR_RW) & (1 << 5));
    PUT32(UART0_BASE_UARTDR_RW, x);
}

static void uart_send_str(uint8_t *y)
{
    while (*y != '\0')
    {
        uart_send_char(*y);
        y++;
    }
}

static inline void int2bin32(uint32_t val, uint8_t *buf)
{
    uint32_t u = (uint32_t)val;

    for (int i = 0; i < 32; i++)
    {
        buf[i] = ((u >> (31 - i)) & 1) ? '1' : '0';
    }
    buf[32] = '\0';
}

static inline void int2str(int value, uint8_t *buf)
{
    uint8_t i = 0;

    if (value < 0)
    {
        buf[i++] = '-';
        value = -value;  // -45 -> 45 に変換
    }

    // 桁毎に処理
    int tens[] = {100, 10, 1};
    uint8_t started = 0;

    for (int j = 0; j < 3; j++)
    {
        uint8_t digit = 0;
        int v = value;

        while (v >= tens[j])
        {
            v -= tens[j];
            digit++;
        }

        if (digit > 0 || started || j == 2)
        {
            buf[i++] = digit + '0';
            value -= digit * tens[j];  // 次の桁用に減算
            started = 1;
        }
    }

    buf[i] = '\0';
}

static void i2c_init(void)
{
    PUT32(I2C0_IC_ENABLE_RW, 0);  // I2C0初期化
    while (GET32(I2C0_IC_ENABLE_RW) & 1);

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
        1 << 1 |  // SPEED -> standard
        0 << 3 |  // IC_10BITADDR_SLAVE -> Slave 7bits addressing mode
        0 << 4 |  // IC_10BITADDR_MASTER -> Master 7bits addressing mode
        1 << 5 |  // IC_RESTART_EN -> enable
        1 << 6 |  // IC_SLAVE_DISABLE -> slave_disabled
        0 << 7 |  // STOP_DET_IFADDRESSED -> disbaled
        1 << 8 |  // TX_EMPTY_CTRL -> enabled
        0 << 9    // RX_FIFO_FULL_HLD_CTRL -> disabled
    );    
    // I2C0 標準モードのSCLクロックのHigh期間カウント
    PUT32(I2C0_IC_SS_SCL_HCNT_RW, 500);
    // I2C0 標準モードのSCLクロックのLow期間カウント
    PUT32(I2C0_IC_SS_SCL_LCNT_RW, 741);
    // I2C0 スパイク抑制ロジックによってフィルタリングされる最長スパイクの持続時間
    PUT32(I2C0_IC_FS_SPKLEN_RW, 4);
    // I2C0 SDAホールド持続時間
    PUT32(I2C0_IC_SDA_HOLD_RW, 75); // 最低値は0x1

    // IO_BANK0 setting
    PUT32(IO_BANK0_GPIO4_CTRL_RW, 3);  // I2C0-SDA
    PUT32(IO_BANK0_GPIO5_CTRL_RW, 3);  // I2C0-SCL

    // I2C0を有効化
    PUT32(I2C0_IC_ENABLE_RW, 1);
    DELAY(5000);  // 40μs待ち
}

static inline int read_i2c(const uint8_t address, uint8_t cmd_msb, uint8_t cmd_lsb, uint8_t *data)
{
    // TX_FIFOが空くのを確認(TFE -> 0x1)
    while (!(GET32(I2C0_IC_STATUS_RW) & (1 << 2)));

    // ABORT/STOPクリア
    GET32(I2C0_IC_CLR_TX_ABRT_RW);
    GET32(I2C0_IC_CLR_STOP_DET_RW);
    // 割り込み・ABORT完全クリア
    GET32(I2C0_IC_CLR_INTR_RW);

    PUT32(I2C0_IC_ENABLE_RW, 0);
    while (GET32(I2C0_IC_ENABLE_RW) & 1);
    PUT32(I2C0_IC_TAR_RW, address & 0x7F);
    PUT32(I2C0_IC_ENABLE_RW, 1);  // データ読み込み前に必ずこのレジスタをenableに設定すること
    DELAY(5000);  // 40μs待ち
    
    // コマンド読み込み
    uint8_t command[2] = {cmd_msb, cmd_lsb};

    uint32_t i;
    for (i = 0; i < 2; i++)
    {
        uint32_t cmd = command[i];  
        
        // 最初のバイト
        if (i == 0)
        {
            cmd |= (0 << 10);  // STARTを促す
            cmd |= (0 <<  9);
        }
        // 最後のバイト → STOP
        if (i == 1)
        {
            cmd |= (1 <<  9);
        }

        PUT32(I2C0_IC_DATA_CMD_RW, cmd);

        // TX_TLのしきい値以下確認（CON設定上、実質０確認となる）
        while(!(GET32(I2C0_IC_RAW_INTR_STAT_RW) & (1 << 4)));  // TX_EMPTY

        // ABORT確認
        uint8_t log[33];
        bool abort = false;
        uint32_t abort_reason = GET32(I2C0_IC_TX_ABRT_SOURCE_RW);
        if (abort_reason != 0)
        {
            int2bin32(abort_reason, log);
            uart_send(0x23);  // DEBUG #
            uart_send_str(log);  // ログ出力
            GET32(I2C0_IC_CLR_TX_ABRT_RW);  // ABORTデータクリア
            abort = true;
            // 発生ABORTチェック
            if (abort_reason & (1 << 0))
            {
                uart_send_str("Write Com_msb NoAck !");  // DEBUG
            }
            if (abort_reason & (1 << 3))
            {
                uart_send_str("Write Com_lsb NoAck !");  // DEBUG 
            }

            // STOP_DET割り込みがアクティブになるのを確認
            while (!(GET32(I2C0_IC_RAW_INTR_STAT_RW) & (1 << 9)));
            GET32(I2C0_IC_CLR_STOP_DET_RW);
            
            return -2;  // NOACK発生
        }
        if (abort) break;  // Abort発生したら書き込みストップ
    }

    // STOP_DET割り込みがアクティブになるのを確認
    while (!(GET32(I2C0_IC_RAW_INTR_STAT_RW) & (1 << 9)));
    GET32(I2C0_IC_CLR_STOP_DET_RW);
    
    uint32_t cnt;
    for (cnt = 0; cnt < 6; cnt++)
    {
        bool firstByte = (cnt == 0 ? true : false);
        bool lastByte = (cnt == 5 ? true : false);
 
        PUT32(
            I2C0_IC_DATA_CMD_RW,
            ((firstByte ? 1 : 0) << 10) |  // RESTART -> 最初のみenable, 後はdisabled
            ((lastByte  ? 1 : 0) <<  9) |  // STOP -> enable、最後のみdisable
            (1 << 8)                       // COMMAND -> read
        );

        // uart_send_str("before RXFLR");
        while (GET32(I2C0_IC_RXFLR_RW) == 0);  // RXFLRゼロ確認
        // データ書き出し
        data[cnt] = GET32(I2C0_IC_DATA_CMD_RW) & 0xFF; // DATのデータ(下8桁)のみ取得
        // uart_send_str("Get result data");
    }
    
    // STOP_DET割り込みがアクティブになるのを確認
    while (!(GET32(I2C0_IC_RAW_INTR_STAT_RW) & (1 << 9)));
    GET32(I2C0_IC_CLR_STOP_DET_RW);

    return 0;
}

// スレーブデバイスへの書き込み関数
static inline int write_i2c(const uint8_t address, uint8_t comMsb, uint8_t comLsb)
{
    // TX_FIFOが空くのを確認(TFE -> 0x1)
    while (!(GET32(I2C0_IC_STATUS_RW) & (1 << 2)));

    // ABORT/STOPクリア
    GET32(I2C0_IC_CLR_TX_ABRT_RW);
    GET32(I2C0_IC_CLR_STOP_DET_RW);
    // 割り込み・ABORT完全クリア
    GET32(I2C0_IC_CLR_INTR_RW);

    PUT32(I2C0_IC_ENABLE_RW, 0);
    while (GET32(I2C0_IC_ENABLE_RW) & 1);
    PUT32(I2C0_IC_TAR_RW, address & 0x7F);
    PUT32(I2C0_IC_ENABLE_RW, 1);  // データ読み込み前に必ずこのレジスタをenableに設定すること
    DELAY(5000);  // 40μs待ち

    // コマンド読み込み
    uint8_t data[2] = {comMsb, comLsb};

    uint32_t i;
    for (i = 0; i < 2; i++)
    {
        uint32_t cmd = data[i];  
        
        // 最初のバイト
        if (i == 0)
        {
            cmd |= (0 << 10);
            cmd |= (0 <<  9);
        }
        // 最後のバイト → STOP
        if (i == 1)
        {
            cmd |= (1 <<  9);
        }

        PUT32(I2C0_IC_DATA_CMD_RW, cmd);

        // TX_TLのしきい値以下確認（CON設定上、実質０確認となる）
        while(!(GET32(I2C0_IC_RAW_INTR_STAT_RW) & (1 << 4)));  // TX_EMPTY

        // ABORT確認
        uint8_t log[33];
        bool abort = false;
        uint32_t abort_reason = GET32(I2C0_IC_TX_ABRT_SOURCE_RW);
        if (abort_reason != 0)
        {
            int2bin32(abort_reason, log);
            uart_send(0x23);  // DEBUG #
            uart_send_str(log);  // ログ出力
            GET32(I2C0_IC_CLR_TX_ABRT_RW);  // ABORTデータクリア
            abort = true;
            // 発生ABORTチェック
            if (abort_reason & (1 << 0))
            {
                uart_send_str("Write ComMsb NoAck !");  // DEBUG 
            }
            if (abort_reason & (1 << 3))
            {
                uart_send_str("Write ComLsb NoAck !");  // DEBUG 
            }

            // STOP_DET割り込みがアクティブになるのを確認
            while (!(GET32(I2C0_IC_RAW_INTR_STAT_RW) & (1 << 9)));
            GET32(I2C0_IC_CLR_STOP_DET_RW);

            return -1;  // 書込失敗
        }
        if (abort) break;
    }

    return 0;  // 成功
}

volatile uint32_t systick_ms = 0;
void Systick_Handler(void)
{
    systick_ms++;
    if (systick_ms >= 1000)
    {
        systick_ms = 0;
        PUT32(SIO_GPIO_OUT_XOR, 1 << 25);  // GPIO25出力を切替
    }
}

int temp_humid(void)
{
    clock_init();
    reset_subsys();
    led_init();
    i2c_init();
    // clk_periの有効待ち
    while (!(GET32(CLK_PERI_CTRL_RW) & (1 << 11)));
    uart_init();

    PUT32(SYST_RVR, 125000-1);  // 125MHz設定のため、1msカウントにセット（1s設定できないため）
    PUT32(SYST_CVR, 0);  // 125MHz設定のため、1msカウントにセット（1s設定できないため）
    PUT32(SYST_CSR, 0x07);  // プロセッサクロック使用、例外ステータス保留、カウンター有効

    uint8_t recieve_data[6];  // センサーからの受信データ入れ物

    uint8_t buf_temp1[6];
    uint8_t buf_temp2[6];
    uint8_t buf_humid[6];

    // 各スレーブアドレス
    const uint8_t sensor_addr = 0x45; // センサーのスレーブアドレス

    // STH31初期化
    uart_send_str("Reset soft");
    uart_send_str("\r\n\n");
    write_i2c(sensor_addr, 0x30, 0xA2);  // ソフトリセット
    uart_send_str("Stop heater");
    uart_send_str("\r\n\n");
    write_i2c(sensor_addr, 0x30, 0x66);  // ヒーター停止
    uart_send_str("Delete register data");
    uart_send_str("\r\n\n");
    write_i2c(sensor_addr, 0x30, 0x41);  // @ステータスレジスタのデータ消去

    while (1)
    {
        WFI();

        // SHT31からのデータ受信
        // uart_send_str("Sensor read start ...");  // DEBUG
        // uart_send_str("\r\n\n");
        read_i2c(sensor_addr, 0x2C, 0x06, recieve_data);
        DELAY(125000);  // 1ms待ち

        // 測定値の算出
        uint16_t ST = ((uint16_t)recieve_data[0] << 8) | recieve_data[1];
        uint16_t SRH = ((uint16_t)recieve_data[3] << 8) | recieve_data[4];

        // 温度
        int32_t temp_raw = -(45 << 16) + (175 * (int32_t)ST);
        int8_t temp_c = (int8_t)(temp_raw >> 16);  // 整数部分
        int8_t temp_frac = (int8_t)(((uint32_t)(temp_raw & 0xFFFF) * 10) >> 16);  // 小数部分

        // 湿度
        uint8_t humid = (uint8_t)((100 * (int32_t)SRH) >> 16);

        int2str(temp_c, buf_temp1);
        int2str(temp_frac, buf_temp2);
        int2str(humid, buf_humid);

        uart_send_str("----- Measurement results -----");  
        uart_send_str("\r\n\n");

        uart_send_str("Temperature: ");  
        uart_send_str(buf_temp1);
        uart_send_char(0x2E);
        uart_send_str(buf_temp2);
        uart_send_char(0x20);
        uart_send_char(0xB0);
        uart_send_char(0x43);
        uart_send_str("\r\n\n");

        uart_send_str("Humidity: ");  
        uart_send_str(buf_humid);
        uart_send_char(0x20);
        uart_send_char(0x25);
        uart_send_str("\r\n\n");

        DELAY(62500000);  // 500ms待ち
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
//
