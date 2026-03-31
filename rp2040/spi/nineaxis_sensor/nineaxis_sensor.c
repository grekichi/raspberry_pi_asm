#include "rp2040.h"
#include "gy91.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

void PUT32(uint32_t, uint32_t);
uint32_t GET32(uint32_t);
void WFI(void);

static void clock_init(void)
{
    // 125MHzに設定する
    PUT32(CLK_SYS_RESUS_CTRL_RW, 0x00);             // RESUS無効化、time out を0サイクルに設定
    PUT32(XOSC_CTRL_RW, 0xAA0 | (0xFAB << 12));     // 水晶発振器を1-15MHzレンジに設定、水晶発振器enable
    PUT32(XOSC_STARTUP_RW, 0xC4);                   // 256 * 196 = 50176 cycles に設定
    while (!(GET32(XOSC_STATUS_RW) & 0x80000000));  // 発振器の動作確認
    PUT32(CLK_REF_CTRL_RW, 0x2);                    // SRC -> XOSC_CLKSRC

    // PLL_SYSのリセット状態を解除する（=使える状態にする）
    PUT32(RESETS_RESET_CLR, 1 << 12); // ビット12がPLL_SYSに相当
    while (!(GET32(RESETS_RESET_DONE_RW) & (1 << 12)));

    // PLL_SYSを125MHzに設定する
    PUT32(PLL_SYS_PWR_RW, (1 << 5) | (1 << 3) | (1 << 0));  // PWR 電源off
    PUT32(PLL_SYS_CS_RW, 0);                                // REFDIV=1
    PUT32(PLL_SYS_FBDIV_INT_RW, 125);                       // VCO=1500MHz *63に設定して、PD2=1にすると同等設定で消費電力を抑えられる
    PUT32(PLL_SYS_PWR_RW, 1 << 3);                          // POSTDIVPD=1で保持
    while (!(GET32(PLL_SYS_CS_RW) & (1 << 31)));            // PLLのロック状況を確認
    PUT32(PLL_SYS_PRIM_RW, (6 << 16) | (2 << 12));          // PD1->6, PD2->2
    PUT32(PLL_SYS_PWR_RW, 0);                               // Power -> on, PD=0, VCOPD=0, POSTDIVPD=0

    PUT32(CLK_SYS_CTRL_CLR, 1 << 0);                 // SRCを一旦CLK_REF(=XOSC)に退避する
    PUT32(CLK_SYS_CTRL_CLR, 0x7 << 5);               // AUXSRC 7:5bits クリア -> PLL_SYS
    PUT32(CLK_SYS_CTRL_RW, (0x0 << 5) | (0x1 << 0)); // AUXSRC -> PLL_SYS, CLKSRC_CLK_SYS_AUX

    PUT32(CLK_PERI_CTRL_CLR, 0x7 << 5);              // AUXSRC 7:5bits クリア -> CLK_SYS
    PUT32(CLK_PERI_CTRL_RW, (1 << 11) | (0x1 << 5)); // enable, CLKSRC_PLL_SYS
    // clk_periの有効待ち
    while (!(GET32(CLK_PERI_CTRL_RW) & (1 << 11)));

    // ウォッチドッグの設定(1MHz(=1μs)に設定)
    PUT32(WATCHDOG_TICK_RW, ((1 << 9) | 12)); // clk_tickはclk_refベースのため、XOSCの12(MHz)を設定
}

static void reset_subsys(void)
{
    // IO_BANK0のリセット状態を解除する（=使える状態にする）
    PUT32(RESETS_RESET_CLR, 1 << 5); // ビット５がIO_BANK0に相当
    // IO_BANK0のリセットが完了していればループを抜ける
    while (!(GET32(RESETS_RESET_DONE_RW) & (1 << 5)));

    // PADS_BANK0のリセット状態を解除する（=使える状態にする）
    PUT32(RESETS_RESET_CLR, 1 << 8); // ビット8がPADS_BANK0に相当
    // PADS_BANK0のリセットが完了していればループを抜ける
    while (!(GET32(RESETS_RESET_DONE_RW) & (1 << 8)));

    // I2C0のリセット状態を解除する（=使える状態にする）
    PUT32(RESETS_RESET_SET, 1 << 3); // ビット3がI2C0に相当
    PUT32(RESETS_RESET_CLR, 1 << 3); // ビット3がI2C0に相当
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

    // *PicoW用にGPIO20とする
    PUT32(SIO_GPIO_OE_CLR, 1 << 20);  // GPIO25出力を無効化（初期化のための安全動作）
    PUT32(SIO_GPIO_OUT_CLR, 1 << 20); // GPIO25の値をクリア
    ra = GET32(PADS_BANK0_GPIO20_RW);
    ra ^= 0x40; // if input disabled then enable
    ra &= 0xC0; // if output disabled then enable
    PUT32(PADS_BANK0_GPIO20_XOR, ra);
    PUT32(IO_BANK0_GPIO20_CTRL_RW, 5); // SIO
    PUT32(SIO_GPIO_OE_SET, 1 << 20);   // GPIO25出力を有効化
}

static void uart_init(void)
{
    PUT32(RESETS_RESET_CLR, (1 << 22)); // UART0のリセットを解除
    while (!(GET32(RESETS_RESET_DONE_RW) & (1 << 22)));

    PUT32(UART0_UARTCR_RW, 0x00000000); // UART初期化
    // ボーレート分周器の設定
    // 水晶発振器(XOSC)使用のため、clk_periの周波数は、125MHz = (125 * 10^6)
    // 通信速度(ボーレート)は、115200 bps に設定
    // 計算方法: clk_periの周波数 / (ボーレート * 16倍)
    // 割った結果の小数部分は、64倍した値を小数ボーレート分周値とする
    // 125000000 / (16 * 115200) = 67.8168
    // 0.8168 * 64 = 52.2752 -> + 0.5 = 52.7752
    PUT32(UART0_UARTIBRD_RW, 67); // 整数ボーレート分周値
    PUT32(UART0_UARTFBRD_RW, 53); // 小数ボーレート分周値

    // 0 11 1 0 0 0 0 = (0111 0000) が設定値
    PUT32(UART0_UARTLCR_H_RW, 0x70);                        // １フレームのデータビット数 8bits, FIFO有効化, パリティビットなし
    PUT32(UART0_UARTCR_RW, (1 << 9) | (1 << 8) | (1 << 0)); // TXE:有効、RXE:有効

    // GPIO0の設定
    PUT32(PADS_BANK0_GPIO0_CLR, (1 << 7) | (1 << 6)); // OD/IE -> disable
    PUT32(PADS_BANK0_GPIO0_SET, 1 << 3);              // Pull up enable
    PUT32(IO_BANK0_GPIO0_CTRL_RW, 2);                 // UART0_TX

    // GPIO1の設定
    PUT32(PADS_BANK0_GPIO1_SET, 1 << 6); // IE enable
    PUT32(PADS_BANK0_GPIO1_SET, 1 << 3); // Pull up enable
    PUT32(IO_BANK0_GPIO1_CTRL_RW, 2);    // UART0_RX
}

static void uart_send(uint32_t x)
{
    while (GET32(UART0_UARTFR_RW) & (1 << 5));
    PUT32(UART0_UARTDR_RW, x);
}

static void uart_send_char(uint8_t x)
{
    while (GET32(UART0_UARTFR_RW) & (1 << 5));
    PUT32(UART0_UARTDR_RW, x);
}

static void uart_send_str(uint8_t *y)
{
    while (*y != '\0')
    {
        uart_send_char(*y);
        y++;
    }
}

static void uart_line_break(void)
{
    uart_send_char('\r');
    uart_send_char('\n');
    // uart_send_char(0x50);  // DEBUG
    // uart_send_char('\r');
    uart_send_char('\n');
}

static inline void char2bin8(uint8_t val, uint8_t *buf)
{
    uint8_t u = (uint8_t)val;

    for (int i = 0; i < 8; i++)
    {
        buf[i] = ((u >> (7 - i)) & 1) ? '1' : '0';
    }
    buf[8] = '\0';
}

static inline void short2bin16(int16_t val, uint8_t *buf)
{
    uint16_t u = (uint16_t)val;

    for (int i = 0; i < 16; i++)
    {
        buf[i] = ((u >> (15 - i)) & 1) ? '1' : '0';
    }
    buf[16] = '\0';
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

static inline void int2str(int32_t value, uint8_t *buf)
{
    uint8_t i = 0;
    uint32_t uvalue;

    // 負の数の処理
    if (value < 0) {
        buf[i++] = '-';
        // 絶対値を取得 (int32_tの最小値対策としてキャストしてから反転)
        uvalue = (uint32_t)-(value + 1) + 1;
    } else {
        uvalue = (uint32_t)value;
    }

    // 桁毎に処理
    uint32_t tens[] = {1000, 100, 10, 1}; //  今回は1000の位まで使用
    uint8_t started = 0;

    for (int j = 0; j < 4; j++)
    {
        uint8_t digit = 0;

        while (uvalue >= tens[j])
        {
            uvalue -= tens[j];
            digit++;
        }

        if (digit > 0 || started || j == 3)
        {
            buf[i++] = digit + '0';
            started = 1;
        }
    }

    buf[i] = '\0';
}

// 主に加速度データのキャラクタ化に使用
static inline void short2str(int16_t value, uint8_t *buf)
{
    uint8_t i = 0;
    uint16_t u_val;

    if (value < 0)
    {
        buf[i++] = '-';
        u_val = (uint16_t)(-(value + 1)) + 1;
    }
    else
    {
        u_val = (uint16_t)value;
    }

    // 各桁の重みテーブル
    static const uint16_t powers[] = {10000, 1000, 100, 10, 1};
    uint8_t started = 0;

    for (uint8_t p = 0; p < 5; p++)
    {
        uint8_t digit = 0;
        uint16_t weight = powers[p];

        // 引き算だけで商を求める
        while (u_val >= weight)
        {
            u_val -= weight;
            digit++;
        }

        // ゼロ抑制の判定
        // すでに数字が始まっているか、現在の桁が0でないか、最後の一の位なら書き込む
        if (started || digit > 0 || p == 4)
        {
            buf[i++] = digit + '0';
            started = 1;
        }
    }

    buf[i] = '\0';
}

static inline void char2str(int8_t value, uint8_t *buf)
{
    uint8_t i = 0;
    uint8_t u_val;

    // 1. 負の数の処理
    if (value < 0)
    {
        buf[i++] = '-';
        // -128を正数に反転させる際のオーバーフロー対策
        u_val = (uint8_t)(-(value + 1)) + 1;
    }
    else
    {
        u_val = (uint8_t)value;
    }

    // 2. 桁の計算（100の位, 10の位, 1の位）
    uint8_t started = 0;

    // 100の位
    uint8_t hundreds = 0;
    while (u_val >= 100) {
        u_val -= 100;
        hundreds++;
    }
    if (hundreds > 0) {
        buf[i++] = (uint8_t)(hundreds + '0');
        started = 1;
    }

    // 10の位
    uint8_t tens = 0;
    while (u_val >= 10) {
        u_val -= 10;
        tens++;
    }
    // すでに上の桁があるか、10の位が1以上なら書き込む
    if (started || tens > 0) {
        buf[i++] = (uint8_t)(tens + '0');
    }

    // 1の位 (0であっても必ず書き込む)
    buf[i++] = (uint8_t)(u_val + '0');

    // 3. 終端文字 
    buf[i] = '\0';
}

static inline void frac2str(int32_t value, uint8_t precision, uint8_t *buf)
{
    // *** 本関数の使用方法 ********************************************************
    //  本関数は、小数点第3位まで対応
    //
    // 小数点第１位の数字の文字列を作成したい場合
    // 10倍の固定小数点数を作る: (value * 10) / 16384
    // int32_t g_fixed1 = ((int32_t)my_sensor.acc[0] * 10) >> 14;
    // frac2str(g_fixed1, 1, buffer); -> 結果は 0.9 や 1.5 など
    //    
    // 小数点第2位の数字の文字列を作成したい場合
    // 100倍の固定小数点数を作る: (value * 100) / 16384 => (value * 25) / 4096
    // int32_t g_fixed2 = ((int32_t)my_sensor.acc[1] * 25) >> 12;
    // frac2str(g_fixed2, 2, buffer); -> 結果は 0.98 や 1.50 など
    // ***************************************************************************

    uint8_t i = 0;
    uint32_t u_val;

    // 負の数の処理
    if (value < 0)
    {
        buf[i++] = '-';
        u_val = (uint32_t)(-value);
    }
    else
    {
        u_val = (uint32_t)value;
    }

    // 整数部と小数部の分離
    uint32_t integer = 0;
    uint32_t fraction = 0;

    if (precision == 3)
    {
        // 100で割る代わりにループで引く
        while (u_val >= 1000)
        {
            u_val -= 1000;
            integer++;
        }
        fraction = u_val; // 0-999
    }
    else if (precision == 2)
    {
        // 100で割る代わりにループで引く
        while (u_val >= 100)
        {
            u_val -= 100;
            integer++;
        }
        fraction = u_val; // 0-99
    }
    else
    {
        // precision == 1 (10で割る代わりにループで引く)
        while (u_val >= 10)
        {
            u_val -= 10;
            integer++;
        }
        fraction = u_val; // 0-9
    }

    // 整数部の変換(0なら'0'、それ以外は桁バラシ)
    if (integer == 0)
    {
        buf[i++] = '0';
    }
    else
    {
        uint32_t divisor = 10000; // 必要に応じて 100000 などに調整
        uint8_t started = 0;
        while (divisor > 0)
        {
            uint8_t digit = 0;
            while (integer >= divisor)
            {
                integer -= divisor;
                digit++;
            }
            if (started || digit > 0 || divisor == 1)
            {
                buf[i++] = digit + '0';
                started = 1;
            }
            // divisor /= 10 の代わり（除算したくないため）
            if      (divisor == 10000)  divisor = 1000;
            else if (divisor == 1000)   divisor = 100;
            else if (divisor == 100)    divisor = 10;
            else if (divisor == 10)     divisor = 1;
            else                        divisor = 0;
        }
    }

    // 小数点の追加
    buf[i++] = '.';

    // 小数部の変換（指定桁数分、0埋めありで出力）
    if (precision == 3)
    {
        // 100の位を抽出
        uint8_t f100 = 0;
        while (fraction >= 100)
        {
            fraction -= 100;
            f100++;
        }
        buf[i++] = f100 + '0';
        // 10の位を抽出
        uint8_t f10 = 0;
        while (fraction >= 10)
        {
            fraction -= 10;
            f10++;
        }
        buf[i++] = f10 + '0';
        // 1の位
        buf[i++] = fraction + '0'; // 残りが1の位
    }
    else if (precision == 2)
    {
        // 10の位を抽出
        uint8_t f10 = 0;
        while (fraction >= 10)
        {
            fraction -= 10;
            f10++;
        }
        buf[i++] = f10 + '0';
        // 1の位
        buf[i++] = fraction + '0'; // 残りが1の位
    }
    else
    {
        // precision == 1 の場合はそのまま１桁
        buf[i++] = fraction + '0';
    }

    // 終端文字
    buf[i] = '\0';
}

static void i2c_init(void)
{
    PUT32(I2C0_IC_ENABLE_RW, 0); // I2C0初期化
    while (GET32(I2C0_IC_ENABLE_RW) & 1);

    // FIFO trigger
    PUT32(I2C0_IC_TX_TL_RW, 0);
    PUT32(I2C0_IC_RX_TL_RW, 0);

    // DMA off
    PUT32(I2C0_IC_DMA_CR_RW, 0);

    // interrupt clear
    GET32(I2C0_IC_CLR_INTR_RW);

    // I2C0をマスターモード、高速モード、7bitアドレス送信 に設定
    PUT32(
        I2C0_IC_CON_RW,
        1 << 0 |    // MASTER_MODE -> enabled
        1 << 1 |    // SPEED -> standard
        0 << 3 |    // IC_10BITADDR_SLAVE -> Slave 7bits addressing mode
        0 << 4 |    // IC_10BITADDR_MASTER -> Master 7bits addressing mode
        1 << 5 |    // IC_RESTART_EN -> enable
        1 << 6 |    // IC_SLAVE_DISABLE -> slave_disabled
        0 << 7 |    // STOP_DET_IFADDRESSED -> disbaled
        1 << 8 |    // TX_EMPTY_CTRL -> enabled
        0 << 9      // RX_FIFO_FULL_HLD_CTRL -> disabled
    );
    // I2C0周波数 100kHzに設定上
    // I2C0 標準モードのSCLクロックのHigh期間カウント
    PUT32(I2C0_IC_SS_SCL_HCNT_RW, 500); // 500
    // I2C0 標準モードのSCLクロックのLow期間カウント
    PUT32(I2C0_IC_SS_SCL_LCNT_RW, 741); // 741
    // I2C0 スパイク抑制ロジックによってフィルタリングされる最長スパイクの持続時間
    PUT32(I2C0_IC_FS_SPKLEN_RW, 4); // 3:32ns 4:40ns 5:48ns
    // I2C0 SDAホールド持続時間
    PUT32(I2C0_IC_SDA_HOLD_RW, 75); // 600ns 最低値は0x1

    // IO_BANK0 setting
    PUT32(IO_BANK0_GPIO4_CTRL_RW, 3); // I2C0-SDA
    PUT32(IO_BANK0_GPIO5_CTRL_RW, 3); // I2C0-SCL

    // I2C0を有効化
    PUT32(I2C0_IC_ENABLE_RW, 1);
    while (!(GET32(I2C0_IC_ENABLE_RW) & 1));
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
    PUT32(I2C0_IC_TAR_RW, (address & 0x7F));
    PUT32(I2C0_IC_ENABLE_RW, 1); // データ読み込み前に必ずこのレジスタをenableに設定すること
    while (!(GET32(I2C0_IC_ENABLE_RW) & 1));

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
            cmd |= (0 << 9);
        }
        // 最後のバイト → STOP
        if (i == 1)
        {
            cmd |= (1 << 9);
        }

        PUT32(I2C0_IC_DATA_CMD_RW, cmd);

        // TX_TLのしきい値以下確認（CON設定上、実質０確認となる）
        while (!(GET32(I2C0_IC_RAW_INTR_STAT_RW) & (1 << 4))); // TX_EMPTY

        // ABORT確認
        uint8_t log[33];
        bool abort = false;
        uint32_t abort_reason = GET32(I2C0_IC_TX_ABRT_SOURCE_RW);
        if (abort_reason != 0)
        {
            int2bin32(abort_reason, log);
            uart_send(0x23);    // DEBUG #
            uart_send_str(log); // ログ出力
            uart_line_break();
            GET32(I2C0_IC_CLR_TX_ABRT_RW); // ABORTデータクリア
            abort = true;
            // 発生ABORTチェック
            if (abort_reason & (1 << 0))
            {
                uart_send_str("Write ComMsb NoAck !"); // DEBUG
                uart_line_break();
            }
            if (abort_reason & (1 << 3))
            {
                uart_send_str("Write ComLsb NoAck !"); // DEBUG
                uart_line_break();
            }

            // STOP_DET割り込みがアクティブになるのを確認
            while (!(GET32(I2C0_IC_RAW_INTR_STAT_RW) & (1 << 9)));
            GET32(I2C0_IC_CLR_STOP_DET_RW);

            return -1; // 書込失敗
        }
        if (abort)
            break;
    }

    return 0; // 成功
}

static void monitor_init(void)
{
    PUT32(IO_BANK0_GPIO6_CTRL_RW, 5); // SIO
    PUT32(IO_BANK0_GPIO7_CTRL_RW, 5); // SIO

    PUT32(SIO_GPIO_OE_CLR, 1 << 6); // GPIO6出力を無効化
    PUT32(SIO_GPIO_OE_CLR, 1 << 7); // GPIO7出力を無効化

    PUT32(SIO_GPIO_OUT_CLR, 1 << 6); // GPIO6の値をクリア
    PUT32(SIO_GPIO_OUT_CLR, 1 << 7); // GPIO7の値をクリア

    PUT32(SIO_GPIO_OE_SET, 1 << 6); // GPIO6出力を有効化
    PUT32(SIO_GPIO_OE_SET, 1 << 7); // GPIO7出力を有効化
}

static void bootup_monitor(uint8_t addr)
{
    // AQM0802初期設定
    PUT32(SIO_GPIO_OUT_SET, 1 << 7); // ディスプレイ RESET
    delay_ms(40); // 40ms待機
    uart_send_str("Monitor Init 1");
    uart_line_break();
    // 1-1
    write_i2c(addr, 0x00, 0x38); // Function set -> 8bit bus, 2-line display
    delay_us(30);                        // 30μs待機
    // 1-2
    write_i2c(addr, 0x00, 0x39); // Function set -> (IS tabel = 1) 拡張機能セット
    delay_us(30);                        // 30μs待機
    // 1-3
    write_i2c(addr, 0x00, 0x14); // @内蔵発振器の周波数設定 -> bias: 1/5, F2=1: 183Hz(3.0Vの場合)
    delay_us(30);                        // 30μs待機
    // 1-4
    write_i2c(addr, 0x00, 0x71); // コントラスト設定(Low byte) -> ?
    delay_us(30);                        // 30μs待機
    // 1-5
    write_i2c(addr, 0x00, 0x56); // @Power,ICONコントロール,コントラスト設定(High byte) -> booster on
    delay_us(30);                        // 30μs待機
    // 1-6
    write_i2c(addr, 0x00, 0x6C); // @フォロワー制御 -> 内部フォロワー回路on
    delay_ms(200);                       // 200ms待機

    uart_send_str("Monitor Init 2");
    uart_line_break();
    // 2-1
    write_i2c(addr, 0x00, 0x38); // @Function set ->  標準セット戻し
    delay_us(30);                        // 30μs待機
    // 2-2
    write_i2c(addr, 0x00, 0x0C); // @ディスプレイ制御 -> ディスプレイon
    delay_us(30);                        // 30μs待機
    // 2-3
    write_i2c(addr, 0x00, 0x01); // ディスプレイクリア
    delay_us(1100);                      // 1.1ms待機

    PUT32(SIO_GPIO_OUT_SET, 1 << 6); // モニター照明ON
}


// --- SPI CSピン定義 ---
#define BIT_CS_MPU  (1 << 13)

static inline uint8_t spi_xfer(uint8_t data)
{
    // 送信処理
    while (!(GET32(SPI1_SSPSR_RW) & (1 << 1))); // TNF (送信バッファ空き待ち)
    PUT32(SPI1_SSPDR_RW, data); // 送信FIFOの書き込み
    
    // 受信待ち
    while (!(GET32(SPI1_SSPSR_RW) & (1 << 2))); // RNE (受信バッファ入り待ち)
    
    return (uint8_t)GET32(SPI1_SSPDR_RW); // 受信FIFOの読み込み
}

// 1バイト書き込み用
static inline void write_spi_reg(uint32_t cs_bit, uint8_t reg, uint8_t val)
{
    PUT32(SIO_GPIO_OUT_SET, cs_bit); // CS High
    delay_us(10);

    // ゴミを読み捨てる
    while ((GET32(SPI1_SSPSR_RW) & (1 << 2))) // RX FIFO Not Empty を確認
    {
        (void)GET32(SPI1_SSPDR_RW);  // ゴミを読んで捨てる
    }

    PUT32(SIO_GPIO_OUT_CLR, cs_bit); // CS Low
    delay_us(2);
    
    spi_xfer(reg & 0x7F);  // 書き込み時は最上位bit 0
    spi_xfer(val);
    
    // SPIの物理的な転送が完全に終わるのを待つ
    while (GET32(SPI1_SSPSR_RW) & (1 << 4)); // BSY (Bit 4)
    PUT32(SIO_GPIO_OUT_SET, cs_bit); // CS High
}

static inline uint8_t read_spi_reg(uint32_t cs_bit, uint8_t reg)
{
    uint8_t val = 0;
    PUT32(SIO_GPIO_OUT_SET, cs_bit); // CS High
    delay_us(10);

    // ゴミを読み捨てる
    while ((GET32(SPI1_SSPSR_RW) & (1 << 2))) // RX FIFO Not Empty を確認
    {
        (void)GET32(SPI1_SSPDR_RW);  // ゴミを読んで捨てる
    }

    PUT32(SIO_GPIO_OUT_CLR, cs_bit); // CS Low
    delay_us(2);
    
    spi_xfer(reg | 0x80); // Readフラグ(MSB 1)
    val = spi_xfer(0x00); // ダミー送信

    // SPIの物理的な転送が完全に終わるのを待つ
    while (GET32(SPI1_SSPSR_RW) & (1 << 4)); // BSY (Bit 4)
    PUT32(SIO_GPIO_OUT_SET, cs_bit); // CS High
    
    return val;
}

static inline void read_spi_regs(uint32_t cs_bit, uint8_t reg, uint8_t *buf, uint32_t len)
{
    PUT32(SIO_GPIO_OUT_SET, cs_bit); // CS High
    delay_us(10);

    // ゴミを読み捨てる
    while ((GET32(SPI1_SSPSR_RW) & (1 << 2))) // RX FIFO Not Empty を確認
    {
        (void)GET32(SPI1_SSPDR_RW);  // ゴミを読んで捨てる
    }

    PUT32(SIO_GPIO_OUT_CLR, cs_bit); // CS Low
    delay_us(2);
    
    spi_xfer(reg | 0x80); // Readフラグ(MSB 1)

    for (uint32_t i = 0; i < len; i++)
    {
        buf[i] = spi_xfer(0x00); // ダミー送信
    }

    // SPIの物理的な転送が完全に終わるのを待つ
    while (GET32(SPI1_SSPSR_RW) & (1 << 4)); // BSY (Bit 4)
    PUT32(SIO_GPIO_OUT_SET, cs_bit); // CS High
}

static void spi_init(void)
{
    // SPI1(=17) IO_BANK0のリセット解除
    PUT32(RESETS_RESET_CLR, (1 << 17));
    while (!(GET32(RESETS_RESET_DONE_RW) & (1 << 17)));

    // GPIOピンの入出力設定
    PUT32(PADS_BANK0_GPIO12_SET, ((1 << 7) | (1 << 6) | (1 << 3))); // OD -> 1, IE -> 1
    
    // SPI1ピン設定 (GPIO 12:MISO, 14:SCK, 15:MOSI)
    // FUNCSEL=1 (=SPI)
    PUT32(IO_BANK0_GPIO12_CTRL_RW, (0x2 << 12 | 1));
    PUT32(IO_BANK0_GPIO14_CTRL_RW, 1);
    PUT32(IO_BANK0_GPIO15_CTRL_RW, 1);

    // GPIO13(NCS)はSIO(=5)として初期化(MPU9250用)
    PUT32(IO_BANK0_GPIO13_CTRL_RW, 5);
    // GPIO10でCSBをI2Cの張り付きリセットスイッチとして使うためSIO(=5)設定
    PUT32(IO_BANK0_GPIO10_CTRL_RW, 5);

    PUT32(SIO_GPIO_OUT_SET, BIT_CS_MPU); // GPIO13の値をHighにセット
    PUT32(SIO_GPIO_OE_SET, BIT_CS_MPU);  // GPIO13の出力を有効化
    PUT32(SIO_GPIO_OUT_SET, (1 << 10)); // GPIO10の値をHighにセット
    PUT32(SIO_GPIO_OE_SET, (1 << 10));  // GPIO10の出力を有効化

    // SPI1一旦無効化
    PUT32(SPI1_SSPCR1_RW, 0x00); // SSE -> disable
    // ボーレート設定(1MHz程度)
    // Fclk=125MHz / (CPSR=124 * (1 + SCR=0)) ≒ 1MHz
    PUT32(SPI1_SSPCPSR_RW, 124); // 2-254の偶数でなければいけない
    PUT32(SPI1_SSPCR0_RW, 0x07); // (SCR=0,) SPH=0, SPO=0, FRF=00(Motorola mode), 8bit mode(0111)
    // SPI1有効化
    PUT32(SPI1_SSPCR1_RW, 0x02); // SSE -> enabled

    delay_ms(10); // 電源安定化のための待ち時間
}

// MPU9250初期化設定
static uint32_t mpu9250_init(void)
{
    // ハードウェアリセット (確実に初期状態に戻す)
    write_spi_reg(BIT_CS_MPU, MPU9250_PWR_MGMT_1, 0x80); 
    delay_ms(100);
    // スリープ解除＆クロック安定化
    write_spi_reg(BIT_CS_MPU, MPU9250_PWR_MGMT_1, 0x01);  // CLKSEL[2:0] -> 1
    delay_ms(20);
    // 一旦バイパスをONにして、内部のマスター回路をバスから切り離す
    write_spi_reg(BIT_CS_MPU, MPU9250_INT_PIN_CFG, 0x02); // Bypass Enable
    delay_ms(10);
    // I2Cバイパスをオフ（マスターモード使用の準備）
    write_spi_reg(BIT_CS_MPU, MPU9250_INT_PIN_CFG, 0x00);
    delay_ms(10);
    // すべての加速度・ジャイロ軸を有効にする
    write_spi_reg(BIT_CS_MPU, MPU9250_PWR_MGMT_2, 0x00);
    delay_ms(10);

    // I2CスレーブCTRLレジスタを一旦リセットする
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV0_CTRL, 0x00);
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV1_CTRL, 0x00);
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV2_CTRL, 0x00);
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV3_CTRL, 0x00);
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_CTRL, 0x00);

    // I2Cマスター回路のリセット
    write_spi_reg(BIT_CS_MPU, MPU9250_USER_CTRL, 0x10);
    delay_ms(10);
    write_spi_reg(BIT_CS_MPU, MPU9250_USER_CTRL, 0x10 | 0x10); // I2C_MST_RST を追加
    delay_ms(20); // リセット完了を待つ

    // SPIモードを確定し、AK8963との通信用にI2Cマスターモードを有効にする
    write_spi_reg(BIT_CS_MPU, MPU9250_USER_CTRL, 0x30); // I2C_MST_EN | I2C_IF_DIS
    delay_ms(10);
    
    // I2Cクロックを400kHzに設定 (I2C_MST_CTRL) & Stop Condition強制
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_MST_CTRL, 0x1D); // I2C_MST_P_NSR | I2C_MST_CLK[3:0] -> 9 
    // サンプルレートを100Hzに落として、I2Cマスターを落ち着かせる
    write_spi_reg(BIT_CS_MPU, MPU9250_SMPLRT_DIV, 0x00);  // この処理は重要
    // I2Cマスターのディレイをリセットする
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_MST_DELAY_CTRL, 0x00);
    // I2C_SLV4_CTRLをリセットする
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_CTRL, 0x00);  // I2C_MST_DLYを設定する際に使用

    // MPU9250 ID確認
    uint8_t wai = 0;
    wai = read_spi_reg(BIT_CS_MPU, MPU9250_WHO_AM_I);
    if (wai != MPU9250_RESP_WAI)
    {
        uart_send_str("MPU9250 ID could not be confirmed");
        uart_line_break();
        return 1;
    }

    return 0;  // 成功
}

// I2C_SLV4の処理完了を待つ関数
static int32_t check_i2c_slv4_done(void)
{
    uint32_t timeout = 10000;
    uint8_t status;

    while (timeout > 0)
    {
        status = read_spi_reg(BIT_CS_MPU, MPU9250_I2C_MST_STATUS);

        // Bit6(I2C_SLV4_DONE)が 1 になれば通信完了
        if (status & 0x40) return 0; // 成功

        timeout--;
    }

    return -1; // タイムアウトエラー
}

// 磁力計 補正変数の格納用
typedef struct {
    uint16_t asa[3];     // 感度調整値
    int32_t  offset[3];  // ゼロ点補正
} magnetic_t;

static magnetic_t my_magsensor;  // グローバル変数として管理

// ASA値を読み出して構造体に格納する関数
static int32_t ak8963_update_asa(magnetic_t *dev)
{
    const uint8_t mag_addr = AK8963_I2C_ADDRESS;

    // Power-downモードへ移行
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_CTRL, 0x00);
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_ADDR, (mag_addr & 0x7F));
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_REG, AK8963_REG_CNTL1);
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_DO, 0x00);
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_CTRL, 0x80);
    if (check_i2c_slv4_done() != 0) return -1;

    // Fuse ROM アクセスモードへ移行
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_CTRL, 0x00);
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_DO, 0x0F);
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_CTRL, 0x80);
    if (check_i2c_slv4_done() != 0) return -2;
    delay_ms(1);

    // ASA値を連続読み出して、構造体へ格納
    for (int i = 0; i < 3; i++)
    {
        write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_CTRL, 0x00);
        write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_ADDR, (mag_addr | 0x80));
        write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_REG, 0x10 + i);
        write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_CTRL, 0x80);

        if (check_i2c_slv4_done() != 0) return -(3 + i);

        // 構造体のメンバーに直接書き込む
        dev->asa[i] = read_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_DI);
    }

    // Power-down に戻して終了
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_CTRL, 0x00);
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_ADDR, (mag_addr & 0x7F));
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_REG, AK8963_REG_CNTL1);
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_DO, 0x00);
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_CTRL, 0x80);
    check_i2c_slv4_done();

    // SLV4レジスタをクリア
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_CTRL, 0x00);

    return 0; // 成功
}

static uint32_t ak8963_init(void)
{
    uint8_t mag_addr = AK8963_I2C_ADDRESS;    
    // 磁力計(0x0C)の WIA(0x00) レジスタを読み出す設定
    uint8_t mag_id = 0;
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_CTRL, 0x00);
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_ADDR, (mag_addr | 0x80)); // Read from 0x0C
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_REG, AK8963_WAI); // AK8963のWIA(デバイスID)レジスタ
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_CTRL, 0x80); // I2C_SLV0_EN | 読み出し実行
    delay_ms(1);

    uint8_t status = 0;
    status = read_spi_reg(BIT_CS_MPU, MPU9250_I2C_MST_STATUS);  // I2C_MST_STATUS

    if (!(status & 0x40))
    {
        uart_send_str("SLV4 Timeout (I2C engine frozen)");
        uart_line_break();
        return 1;
    }

    if (status & 0x10)
    {
        uart_send_str("AK8963 NACK (Bus Lock or Power Problem)");
        uart_line_break();
        return 1;
    }
    
    // MPU9250が吸い出した値を読み出す
    mag_id = read_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_DI);
    if (mag_id != AK8963_RESP_WAI)
    {
        uart_send_str("AK8963 ID could not be confirmed");
        uart_line_break();
        return 2;
    }

    // ASA値の更新
    uint8_t buf_ret[5];
    int32_t ret = ak8963_update_asa(&my_magsensor);
    int2str(ret, buf_ret);
    if (ret != 0)
    {
        uart_send_str("Error: ASA read failed. Code: ");
        uart_send_str(buf_ret);
        uart_line_break();
        return 1;
    }
    // ----- デバッグ用出力 -----
    // 工場出荷値は通常128近辺(120〜140程度)
    uint8_t buf_asax[5];
    uint8_t buf_asay[5];
    uint8_t buf_asaz[5];
    short2str((int16_t)my_magsensor.asa[0], buf_asax);
    short2str((int16_t)my_magsensor.asa[1], buf_asay);
    short2str((int16_t)my_magsensor.asa[2], buf_asaz);

    uart_send_str("ASA values: ");
    uart_send_str("X = ");
    uart_send_str(buf_asax);
    uart_send_str(", ");
    uart_send_str("Y = ");
    uart_send_str(buf_asay);
    uart_send_str(", ");
    uart_send_str("Z = ");
    uart_send_str(buf_asaz);
    uart_line_break();
    
    // 磁力計(AK8963)を「16bit連続測定モード2」に設定(SVL4を使用)
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_CTRL, 0x00);
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_ADDR, (mag_addr & 0x7F));  // I2C Write指示(Bit7=0)
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_REG, AK8963_REG_CNTL1);  // AK8963のCNTL1レジスタ
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_DO, 0x16);  // 16bit出力, 連続測定モード2(100Hz)
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_CTRL, 0x80);  // I2C_SLV0_EN | I2C_SLV0_LENG[3:0] -> 1 byte
    // 内部転送完了待ち
    if (check_i2c_slv4_done() != 0)
    {
        uart_send_str("Error: Mag Mode Write Timeout");
        uart_line_break();
        return 1;
    }
    delay_ms(10);

    // 2. ★DRDYが1になるまで待つ★
    uart_send_str("Waiting for first Mag DRDY... ");
    uint16_t timeout = 500; 
    while (timeout > 0) {
        // ST1(0x02)をSLV4で読み出す
        write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_ADDR, (mag_addr | 0x80));
        write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_REG, AK8963_REG_ST1);
        write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_CTRL, 0x80);
        check_i2c_slv4_done();
        
        uint8_t st1 = read_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_DI);
        if (st1 & 0x01) {
            uart_send_str("Success !");
            uart_line_break();
            break;
        }
        delay_ms(1);
        timeout--;
    }
    if (timeout == 0) {
        uart_send_str("Failed (Timeout)");
        uart_line_break();
    }

    // ----- AK8963自動読み出し設定 -----
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV0_CTRL, 0x00);
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV0_ADDR, (mag_addr | 0x80)); // I2C_SLV0_RNW | I2C Read指示
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV0_REG, AK8963_REG_ST1); // 読み始めレジスタを指定
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV0_CTRL, 0x88); // I2C_SLV0_EN | 8 bytes
    
    // 分周比（1/5）を設定する
    // I2C_MST_DELAY_CTRL で SLV0 にディレイをかける許可を出す
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_MST_DELAY_CTRL, 0x01); // I2C_SLV0_DLY_EN -> 1
    // 下位4ビットがディレイ値： 1 / (1 + 4) = 1/5
    write_spi_reg(BIT_CS_MPU, MPU9250_I2C_SLV4_CTRL, 0x04);
    
    delay_ms(10); // 転送完了待ち

    return 0;
}

static uint32_t verify_accelerometer(void)
{
    // デバッグ出力の代わりに、もし可能ならこれらの値をチェックして下さい
    // 期待値: pwr2=0x00, acc_cfg=0x00, acc_cfg2=0x00 -> 「全軸有効」

    // 加速度計の動作確認（PWR_MGMT_2)
    // bit 2:0 が加速度計の各軸(Z,Y,X)のスタンバイ設定
    uint8_t pwr2 = read_spi_reg(BIT_CS_MPU, MPU9250_PWR_MGMT_2);
    if (pwr2 != 0x00)
    {
        uart_send_str("PWR_MGMT_2 is invalid");
        uart_line_break();

        return 1;
    }

    // 2. 加速度計の設定１(ACCEL_CONFIG)
    // bit 4:3 がフルスケール設定。 00 で ±2G
    uint8_t acc_cfg = read_spi_reg(BIT_CS_MPU, MPU9250_ACCEL_CONFIG);
    if (acc_cfg != 0x00)
    {
        uart_send_str("ACCEL_CONFIG_1 is invalid");
        uart_line_break();

        return 2;
    }

    // 3. 加速度計の設定２(ACCEL_CONFIG_2)
    // bit 3 はfchoice_b (0でDLPF有効)、bit 2:0 が A_DLPCFG(フィルタ周波数)
    // ここを0x00に設定すると、最も素直な(フィルタが弱い)特性になる
    uint8_t acc_cfg2 = read_spi_reg(BIT_CS_MPU, MPU9250_ACCEL_CONFIG_2);
    if (acc_cfg2  != 0x00)
    {
        uart_send_str("ACCEL_CONFIG_2 is invalid");
        uart_line_break();

        return 3;
    }

    return 0;
}

// 構造体設定
typedef struct
{
    int16_t acc[3];         // 加速度 X, Y, Z
    int16_t acc_offset[3];  // 加速度校正用
    int16_t gyro[3];        // ジャイロ X, Y, Z
    int16_t gyro_offset[3]; // ジャイロ校正用
    int16_t mag[3];         // 磁力 X, Y, Z
    int16_t temp;           // 温度
} sensor_data_t;

// ジャイロ校正用オフセットデータの取得
static void calibrate_gyro(uint32_t cs_bit, sensor_data_t *data)
{
    int32_t sum[3] = {0, 0, 0};
    uint8_t buffer[22];
    const uint16_t samples = 512;

    uart_send_str("Calibrating Gyro ... Don't move !");
    uart_line_break();

    for (uint16_t i = 0; i < samples; i++)
    {
        // センサーから最新データの読み出し
        read_spi_regs(cs_bit, MPU9250_ACCEL_XOUT_H, buffer, 22);

        // ジャイロの値を累積する (対象Indexは 8-13）
        sum[0] += (int16_t)((buffer[8]  << 8) | buffer[9]);
        sum[1] += (int16_t)((buffer[10] << 8) | buffer[11]);
        sum[2] += (int16_t)((buffer[12] << 8) | buffer[13]);

        delay_ms(2); // サンプリング周期に合わせる
    }

    // 平均値をオフセットとして保存
    data->gyro_offset[0] = (int16_t)(sum[0] >> 9);
    data->gyro_offset[1] = (int16_t)(sum[1] >> 9);
    data->gyro_offset[2] = (int16_t)(sum[2] >> 9);

    uart_send_str("Calibration done.");
    uart_line_break();
}


#define ACCEL_SENSITIVITY   16384  // ± 2g時
// #define ACCEL_SENSITIVITY   8192   // ± 4g時
// #define ACCEL_SENSITIVITY   4096   // ± 8g時
// #define ACCEL_SENSITIVITY   2048   // ±16g時

static void calibrate_accel(uint32_t cs_bit, sensor_data_t *data)
{
    int32_t sum[3] = {0, 0, 0};
    uint8_t buffer[6];  // 加速度のみ読み出す場合
    const uint16_t samples = 512;

    for (uint16_t i = 0; i < samples; i++)
    {
        read_spi_regs(cs_bit, MPU9250_ACCEL_XOUT_H, buffer, 6);
        sum[0] += (int16_t)((buffer[0] << 8) | buffer[1]);
        sum[1] += (int16_t)((buffer[2] << 8) | buffer[3]);
        sum[2] += (int16_t)((buffer[4] << 8) | buffer[5]);
        delay_ms(2);
    }

    data->acc_offset[0] = (int16_t)(sum[0] >> 9);
    data->acc_offset[1] = (int16_t)(sum[1] >> 9);
    // Z軸は1G(16384)を引いた残りがオフセット
    data->acc_offset[2] = (int16_t)((sum[2] >> 9) - ACCEL_SENSITIVITY);
}

static inline void read_sensor_data(magnetic_t *dev, sensor_data_t *data)
{
    uint8_t buffer[22];  // ACCEL(6) + TEMP(2) + GYRO(6) + EXT_SENS(8) = 22 bytes

    // MPU9250の加速度レジスタ(0x3B)から、磁力計データ(0x50)までを一気に読み出す
    // SPIで連続して読み出すことで、各軸のサンプリングタイミングのズレを最小限にします
    read_spi_regs(BIT_CS_MPU, MPU9250_ACCEL_XOUT_H, buffer, 22);

    // 加速度（Big Endian読み出し）
    int16_t raw_accx = (int16_t)((buffer[0] << 8) | buffer[1]);
    int16_t raw_accy = (int16_t)((buffer[2] << 8) | buffer[3]);
    int16_t raw_accz = (int16_t)((buffer[4] << 8) | buffer[5]);

    // 加速度校正を適用する
    data->acc[0] = raw_accx - data->acc_offset[0];
    data->acc[1] = raw_accy - data->acc_offset[1];
    data->acc[2] = raw_accz - data->acc_offset[2];

    // 基盤温度（Big Endian読み出し）
    data->temp = (int16_t)((buffer[6] << 8) | buffer[7]);

    // ジャイロ（Big Endian読み出し）
    int16_t raw_gx = (int16_t)((buffer[8]  << 8) | buffer[9]);
    int16_t raw_gy = (int16_t)((buffer[10] << 8) | buffer[11]);
    int16_t raw_gz = (int16_t)((buffer[12] << 8) | buffer[13]);

    // ジャイロ校正を適用する
    data->gyro[0] = raw_gx - data->gyro_offset[0];
    data->gyro[1] = raw_gy - data->gyro_offset[1];
    data->gyro[2] = raw_gz - data->gyro_offset[2];

    // 磁力（Little Endian読み出し）
    // ak8963_initで SLV0_REG = AK8963_REG_ST1, LENG = 8 に設定している前提
    // buffer[14] = ST1
    // buffer[15..16] = HXL, HXH
    // buffer[17..18] = HYL, HYH
    // buffer[19..20] = HZL, HZH
    // buffer[21] = ST2
    // ST2(buffer[21])に磁気センサオーバーフロー(HOFL)がないか確認
    if (!(buffer[21] & 0x08))
    {
        // 生データを一旦取得する
        int16_t raw_x = (int16_t)((buffer[16] << 8) | buffer[15]);
        int16_t raw_y = (int16_t)((buffer[18] << 8) | buffer[17]);
        int16_t raw_z = (int16_t)((buffer[20] << 8) | buffer[19]);
        // ASA補正をかける
        data->mag[0] = (int16_t)(((int32_t)raw_x * (dev->asa[0] + 128)) >> 8);
        data->mag[1] = (int16_t)(((int32_t)raw_y * (dev->asa[1] + 128)) >> 8);
        data->mag[2] = (int16_t)(((int32_t)raw_z * (dev->asa[2] + 128)) >> 8);
    }
}

volatile uint32_t systick_ms = 0;
void Systick_Handler(void)
{
    systick_ms++;
    if (systick_ms >= 1000)
    {
        systick_ms = 0;
        PUT32(SIO_GPIO_OUT_XOR, 1 << 20); // GPIO25出力を切替(picoWでは20とする)
    }
}

int nineaxis(void)
{
    clock_init();
    reset_subsys();
    led_init();
    uart_init();
    i2c_init();
    monitor_init();
    spi_init();
    
    PUT32(SYST_RVR, 125000 - 1); // 125MHz設定のため、1msカウントにセット（1s設定できないため）
    PUT32(SYST_CVR, 0);          // 125MHz設定のため、1msカウントにセット（1s設定できないため）
    PUT32(SYST_CSR, 0x07);       // プロセッサクロック使用、例外ステータス保留、カウンター有効
    
    mpu9250_init();
    ak8963_init();
    verify_accelerometer();

    // モニターのスレーブアドレス
    const uint8_t monitor_addr = 0x3E;
    bootup_monitor(monitor_addr);
    
    // 9軸データ読み取り用変数
    sensor_data_t my_sensor;
    // ジャイロキャリブレーションの実行
    calibrate_gyro(BIT_CS_MPU, &my_sensor);
    // 加速度キャリブレーションの実行
    calibrate_accel(BIT_CS_MPU, &my_sensor);

    while (1)
    {
        WFI();

        read_sensor_data(&my_magsensor, &my_sensor);

        // 10倍の固定小数点数を作る: (value * 10) / 16384 (*加速度の計算式)
        // int32_t g_fixed1 = ((int32_t)my_sensor.acc[1] * 10) >> 14;
        // 100倍の固定小数点数を作る: (value * 25) / 4096 (*加速度の計算式)
        // int32_t g_fixed2 = ((int32_t)my_sensor.acc[1] * 25) >> 12;
        // 1000倍の固定小数点数を作る: (value * 125) / 2048 (*加速度の計算式)
        // int32_t g_fixed3 = ((int32_t)my_sensor.acc[1] * 125) >> 11;

        // 加速度
        uint8_t buf_axlx[12];
        uint8_t buf_axly[12];
        uint8_t buf_axlz[12];
        int32_t axl_x = ((int32_t)my_sensor.acc[0] * 125) >> 11;
        int32_t axl_y = ((int32_t)my_sensor.acc[1] * 125) >> 11;
        int32_t axl_z = ((int32_t)my_sensor.acc[2] * 125) >> 11;
        frac2str(axl_x, 3, buf_axlx);
        frac2str(axl_y, 3, buf_axly);
        frac2str(axl_z, 3, buf_axlz);

        // ジャイロ
        uint8_t buf_gyrx[12];
        uint8_t buf_gyry[12];
        uint8_t buf_gyrz[12];
        /**
         * @brief ジャイロの生値を10倍の固定小数点数(dps)に変換する（除算なし）
         * @param raw_value オフセット除去後のジャイロ生値
         * @return int32_t 10倍されたdps値 (例: 123 -> 12.3 dps)
         */
        int32_t gyro_x = ((int32_t)my_sensor.gyro[0] * 5003) >> 16;
        int32_t gyro_y = ((int32_t)my_sensor.gyro[1] * 5003) >> 16;
        int32_t gyro_z = ((int32_t)my_sensor.gyro[2] * 5003) >> 16;
        frac2str(gyro_x, 1, buf_gyrx);
        frac2str(gyro_y, 1, buf_gyry);
        frac2str(gyro_z, 1, buf_gyrz);

        // 基盤温度
        uint8_t buf_temp[12];
        int32_t temp = (((int32_t)my_sensor.temp * 1963) >> 16) + 210;
        frac2str(temp, 1, buf_temp);

        // 磁力
        uint8_t buf_magx[12];
        uint8_t buf_magy[12];
        uint8_t buf_magz[12];
        int32_t mag_x = (int32_t)my_sensor.mag[0] * 150;
        int32_t mag_y = (int32_t)my_sensor.mag[1] * 150;
        int32_t mag_z = (int32_t)my_sensor.mag[2] * 150;
        frac2str(mag_x, 3, buf_magx);
        frac2str(mag_y, 3, buf_magy);
        frac2str(mag_z, 3, buf_magz);

        // UART読み出し
        // 加速度
        uart_send_str("Axl_x : ");
        uart_send_str(buf_axlx);
        uart_send_str(", ");
        uart_send_str("Axl_y : ");
        uart_send_str(buf_axly);
        uart_send_str(", ");
        uart_send_str("Axl_z : ");
        uart_send_str(buf_axlz);
        uart_line_break();
        // ジャイロ
        uart_send_str("Gyr_x : ");
        uart_send_str(buf_gyrx);
        uart_send_str(", ");
        uart_send_str("Gyr_y : ");
        uart_send_str(buf_gyry);
        uart_send_str(", ");
        uart_send_str("Gyr_z : ");
        uart_send_str(buf_gyrz);
        uart_line_break();
        // 基盤温度
        uart_send_str("Temperature : ");
        uart_send_str(buf_temp);
        uart_line_break();
        // 磁力
        uart_send_str("Mag_x : ");
        uart_send_str(buf_magx);
        uart_send_str(", ");
        uart_send_str("Mag_y : ");
        uart_send_str(buf_magy);
        uart_send_str(", ");
        uart_send_str("Mag_z : ");
        uart_send_str(buf_magz);
        uart_line_break();

        uart_send_str("**********************************************************");
        uart_line_break();

        // 画面クリア
        write_i2c(monitor_addr, 0x00, 0x01); // ディスプレイクリア
        delay_ms(1);                         // 1ms待機
        // アドレス記述
        write_i2c(monitor_addr, 0x00, (0x00 | 0x80));
        delay_us(30); // 30μs待機
        // accel_xの表示
        write_i2c(monitor_addr, 0x40, 0x58); // X
        delay_us(30);                        // 30μs待機
        for (int32_t i = 0; buf_axlx[i] != '\0'; i++)
        {
            write_i2c(monitor_addr, 0x40, buf_axlx[i]);
        }
        delay_us(30);                        // 30μs待機
        write_i2c(monitor_addr, 0x40, 0x20); // space
        delay_us(30);                        // 30μs待機

        // accel_yの表示
        write_i2c(monitor_addr, 0x40, 0x59); // Y
        delay_us(30);                        // 30μs待機
        for (int32_t i = 0; buf_axly[i] != '\0'; i++)
        {
            write_i2c(monitor_addr, 0x40, buf_axly[i]);
        }
        delay_us(30); // 30μs待機

        // accel_zの表示
        // アドレス記述改行
        write_i2c(monitor_addr, 0x00, (0x40 | 0x80));
        delay_us(30);                        // 30μs待機
        write_i2c(monitor_addr, 0x40, 0x5A); // Z
        delay_us(30);                        // 30μs待機
        for (int32_t i = 0; buf_axlz[i] != '\0'; i++)
        {
            write_i2c(monitor_addr, 0x40, buf_axlz[i]);
        }
        delay_us(30); // 30μs待機

        delay_ms(1000); // 1s待ち
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
