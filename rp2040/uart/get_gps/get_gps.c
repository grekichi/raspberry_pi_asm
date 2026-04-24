#include "rp2040.h"
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

// baudrate=9600の場合  : ibrd->813, fbrd->52
// baudrate=57600の場合 : ibrd->135, fbrd->41
// baudrate=115200の場合: ibrd-> 67, fbrd->53
static void set_uart1_baudrate(uint32_t ibrd, uint32_t fbrd)
{
    PUT32(UART1_CR_RW, 0x00000000); // UART1を一旦無効化

    PUT32(UART1_IBRD_RW, ibrd); // 整数ボーレート分周値
    PUT32(UART1_FBRD_RW, fbrd); // 小数ボーレート分周値

    // 0 11 1 0 0 0 0 = (0111 0000) が設定値
    PUT32(UART1_LCR_H_RW, 0x70);                        // １フレームのデータビット数 8bits, FIFO有効化, パリティビットなし
    PUT32(UART1_CR_RW, (1 << 9) | (1 << 8) | (1 << 0)); // TXE:有効、RXE:有効
}

static void uart0_init(void)
{
    PUT32(RESETS_RESET_CLR, (1 << 22)); // UART0のリセットを解除
    while (!(GET32(RESETS_RESET_DONE_RW) & (1 << 22)));

    PUT32(UART0_CR_RW, 0x00000000); // UART初期化
    // ボーレート分周器の設定
    // 水晶発振器(XOSC)使用のため、clk_periの周波数は、125MHz = (125 * 10^6)
    // 通信速度(ボーレート)は、115200 bps に設定
    // 計算方法: clk_periの周波数 / (ボーレート * 16倍)
    // 割った結果の小数部分は、64倍した値を小数ボーレート分周値とする
    // 125000000 / (16 * 115200) = 67.8168
    // 0.8168 * 64 = 52.2752 -> 52.2752+ 0.5 = 52.7752
    PUT32(UART0_IBRD_RW, 67); // 整数ボーレート分周値
    PUT32(UART0_FBRD_RW, 53); // 小数ボーレート分周値

    // 0 11 1 0 0 0 0 = (0111 0000) が設定値
    PUT32(UART0_LCR_H_RW, 0x70);                        // １フレームのデータビット数 8bits, FIFO有効化, パリティビットなし
    PUT32(UART0_CR_RW, (1 << 9) | (1 << 8) | (1 << 0)); // TXE:有効、RXE:有効

    // GPIO0の設定
    PUT32(PADS_BANK0_GPIO0_CLR, (1 << 7) | (1 << 6)); // OD/IE -> disable
    PUT32(PADS_BANK0_GPIO0_SET, 1 << 3);              // Pull up enable
    PUT32(IO_BANK0_GPIO0_CTRL_RW, 2);                 // UART0_TX

    // GPIO1の設定
    PUT32(PADS_BANK0_GPIO1_SET, 1 << 6); // IE enable
    PUT32(PADS_BANK0_GPIO1_SET, 1 << 3); // Pull up enable
    PUT32(IO_BANK0_GPIO1_CTRL_RW, 2);    // UART0_RX
}

static void uart1_init(void)
{
    PUT32(RESETS_RESET_CLR, (1 << 23)); // UART1のリセットを解除
    while (!(GET32(RESETS_RESET_DONE_RW) & (1 << 23)));

    PUT32(UART1_CR_RW, 0x00000000); // UART1初期化
    // ボーレート分周器の設定
    // 水晶発振器(XOSC)使用のため、clk_periの周波数は、125MHz = (125 * 10^6)
    // 通信速度(ボーレート)は、起動時のみ一旦 9600 bps に設定
    // 計算方法: clk_periの周波数 / (ボーレート * 16倍)
    // 割った結果の小数部分は、64倍した値を小数ボーレート分周値とする
    // 125000000 / (16 * 9600) = 813.8021
    // 0.8021 * 64 = 51.3344 -> + 0.5 = 51.8344
    PUT32(UART1_IBRD_RW, 813); // 整数ボーレート分周値
    PUT32(UART1_FBRD_RW, 52); // 小数ボーレート分周値

    // 0 11 1 0 0 0 0 = (0111 0000) が設定値
    PUT32(UART1_LCR_H_RW, 0x70);                        // １フレームのデータビット数 8bits, FIFO有効化, パリティビットなし
    PUT32(UART1_CR_RW, (1 << 9) | (1 << 8) | (1 << 0)); // TXE:有効、RXE:有効

    // GPIO8の設定
    PUT32(PADS_BANK0_GPIO8_CLR, (1 << 7) | (1 << 6)); // OD/IE -> disable
    PUT32(PADS_BANK0_GPIO8_SET, 1 << 3);              // Pull up enable
    PUT32(IO_BANK0_GPIO8_CTRL_RW, 2);                 // UART1_TX

    // GPIO9の設定
    PUT32(PADS_BANK0_GPIO9_SET, 1 << 6); // IE enable
    PUT32(PADS_BANK0_GPIO9_SET, 1 << 3); // Pull up enable
    PUT32(IO_BANK0_GPIO9_CTRL_RW, 2);    // UART1_RX
}

static uint8_t uart_receive_blocking(void)
{
    while (GET32(UART0_FR_RW) & (1 << 4));
    return (uint8_t)(GET32(UART0_DR_RW) & 0xFF);
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
}

static void uart_line_break(void)
{
    uart_send_char('\r');
    uart_send_char('\n');
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

static inline void int2str_for_gps(uint32_t val, uint8_t *buf)
{
    uint32_t tens[] = {10000, 1000, 100, 10, 1};
    uint8_t idx = 0;

    for (int j = 0; j < 6; j++)
    {
        uint8_t digit = 0;
        while (val >= tens[j])
        {
            val -= tens[j];
            digit++;
        }
        buf[idx++] = digit + '0';
    }
    buf[idx] = '\0';
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

static inline void frac2str4gps(int32_t value, uint8_t *buf)
{
    uint8_t i = 0;
    uint32_t u_val;

    // 1. 負の数の処理
    if (value < 0) {
        buf[i++] = '-';
        u_val = (uint32_t)(-(value + 1)) + 1;
    } else {
        u_val = (uint32_t)value;
    }

    // 2. 整数部と小数部の分離 (100万 = 1,000,000)
    uint32_t integer = 0;
    while (u_val >= 1000000) {
        u_val -= 1000000;
        integer++;
    }
    uint32_t fraction = u_val; // 0-999,999

    // 3. 整数部の出力 (経度180度まで対応: 百の位から開始)
    uint32_t div_int = 100;
    uint8_t started = 0;
    while (div_int > 0) {
        uint8_t digit = 0;
        while (integer >= div_int) {
            integer -= div_int;
            digit++;
        }
        if (started || digit > 0 || div_int == 1) {
            buf[i++] = digit + '0';
            started = 1;
        }
        // div_int /= 10 の代わり
        if      (div_int == 100) div_int = 10;
        else if (div_int == 10)  div_int = 1;
        else                     div_int = 0;
    }

    // 4. 小数点
    buf[i++] = '.';

    // 5. 小数部の出力 (常に6桁固定)
    uint32_t div_frac = 100000;
    while (div_frac > 0) {
        uint8_t f_digit = 0;
        while (fraction >= div_frac) {
            fraction -= div_frac;
            f_digit++;
        }
        buf[i++] = f_digit + '0';

        // div_frac /= 10 の代わり
        if      (div_frac == 100000) div_frac = 10000;
        else if (div_frac == 10000)  div_frac = 1000;
        else if (div_frac == 1000)   div_frac = 100;
        else if (div_frac == 100)    div_frac = 10;
        else if (div_frac == 10)     div_frac = 1;
        else                         div_frac = 0;
    }

    buf[i] = '\0';
}

typedef struct
{
    // 日付(JST)
    uint8_t year; // 00 ~ 99 (2000年 ~ 2099年)
    uint8_t month;
    uint8_t day;
    // 時刻(JST)
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    // 座標(マイクロ度: 1.0度 = 1,000,000)
    // 34.686280度 -> 34686280
    int32_t latitude;
    int32_t longitude;
    // 速度(100倍km/h)
    // 12.53km/h -> 1253
    uint32_t speed;
    // ステータス(1: 有効, 0:無効)
    uint8_t valid;
} GPS_Data;

// 10倍速算
uint32_t mul10(uint32_t n)
{
    return (n << 3) + (n << 1);
}

// 文字列から整数への変換
uint32_t atoui(const char *s, uint32_t len)
{
    uint32_t res = 0;
    for (uint32_t i = 0; i < len; i++)
    {
        if (s[i] >= '0' && s[i] <= '9')
        {
            res = mul10(res) + (uint32_t)(s[i] - '0');
        }
        else
        {
            break;
        }
    }
    return res;
}

/* --- 除算の代用関数 --- */
// n/10 を計算 (上位ビット救出法)
uint32_t div10(uint32_t n)
{
    // マジックナンバー0x66666667(2^34/10相当)を分割して掛ける
    // (n >> 1) * 0.2に相当する計算を構成
    uint32_t i;
    uint32_t q = (n >> 1) + (n >> 2);
    q = q + (q >> 4);
    q = q + (q >> 8);
    q = q + (q >> 16);
    q = q >> 3;
    // 最後に誤差を補正 (n - q*10 が10以上ならqをインクリメント)
    uint32_t r = n - mul10(q);
    return q + ((r + 6) >> 4);  // 簡易補正
}

uint32_t div100(uint32_t n)
{
    return div10(div10(n));
}

uint32_t div1000(uint32_t n)
{
    return div10(div100(n));
}


// --- カレンダー補正処理 ---
// 各月の末日テーブル
// ※ 最初の"0"はダミー
static const uint8_t lastday_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void adjust_jst_calender(GPS_Data *out)
{
    // 日にちを+1
    out->day++;

    // 閏年判定
    uint8_t last_day = lastday_in_month[out->month];
    if (out->month == 2)
    {
        // 2000年〜2099年限定の簡易判定：4で割り切れれば閏年
        // (year & 0x03) == 0 は、「4で割った余りが0」と同じ
        if ((out->year & 0x03) == 0)
        {
            last_day = 29;
        }
    }

    // 月末の繰り上がりチェック
    if (out->day > last_day)
    {
        out->day = 1;  // 1日に戻す
        out->month++;  // 月を+1

        // 年末の繰り上がりチェック
        if (out->month > 12)
        {
            out->month = 1;  // 1月に戻す
            out->year++;     // 年を+1
        }
    }
}

// 度分(DDmm.mmmm)をマイクロ度に変換
static int32_t parse_coord(char *s)
{
    if (!s || *s == '\0') return 0;

    char *dot = 0;
    for (char *p = s; *p; p++)
    {
        if (*p == '.')
        {
            dot = p;
            break;
        }
    }
    if (!dot || (dot - s) < 2) return 0;

    // DEBUG 1
    uart_send_str("[DEBUG] Raw String: ");
    uart_send_str(s);
    uart_line_break();
    
    // 度の抽出
    int32_t deg_len = (int32_t)(dot - 2 - s); // 小数点の２文字前までが「度」
    uint32_t deg = atoui(s, (uint32_t)deg_len);
    
    // 分(整数)の抽出（ドットの直前２文字）
    uint32_t min_int = atoui(dot - 2, 2);
    
    // 分(少数)の抽出（5桁固定で取得する）
    uint32_t min_frac = 0;
    for (int i = 0; i < 5; i++)
    {
        char c = dot[1 + i];

        if (c >= '0' && c <= '9')
        {
            min_frac = (min_frac * 10) + (c - '0');
        }
        else
        {
            // 小数桁が５桁に満たない場合の補正
            // 5桁に満たない場合は、後ろに0を補間する
            for (int j = i; j < 5; j++)
            {
                min_frac *= 10;
            }
            break;
        }
    }

    // DEBUG 2
    uint8_t def_deg[6];
    uint8_t def_minint[10];
    uint8_t def_minfrac[10];
    int2str(deg, def_deg);
    int2str(min_int, def_minint);
    int2str_for_gps(min_frac, def_minfrac);
    uart_send_str("[DEBUG] deg: ");
    uart_send_str(def_deg);
    uart_send_str(" min_int: ");
    uart_send_str(def_minint);
    uart_send_str(" min_frac: ");
    uart_send_str(def_minfrac);
    uart_line_break();
    
    // 「分」をマイクロ単位(10^-6)に統合 ex: 41.16963分 -> 41,169,630 (*6桁化のため10倍)
    uint32_t total_min_u = (min_int * 1000000) + (min_frac * 10);

    // 分/60を計算する(引き算ステップ法)
    uint32_t min_in_deg = 0;
    uint32_t steps[] = {6000000, 600000, 60000, 6000, 600, 60};
    uint32_t vals[] = {100000, 10000, 1000, 100, 10, 1};

    for (int i = 0; i < 6; i++)
    {
        while (total_min_u >= steps[i])
        {
            total_min_u -= steps[i];
            min_in_deg += vals[i];
        }
    }

    // 100万倍した度を返す
    return (deg * 1000000) + min_in_deg;
}

static void parse_gprmc(char *line, GPS_Data *out) {
    char *fields[11];
    uint8_t f_idx = 0;
    char *p = line;

    // カンマ分割
    fields[f_idx++] = p;
    while (*p)
    {
        if (*p == ',')
        {
            *p = '\0';
            if (f_idx < 11)
            {
                fields[f_idx++] = p + 1;
            }
        }
        p++;
    }

    if (f_idx < 10 || fields[2][0] != 'A')
    {
        out->valid = 0;
        return;
    }
    out->valid = 1;

    // 時刻の解析 (UTC -> JST)
    uint8_t h = (uint8_t)atoui(fields[1], 2);
    h += 9;
    // 日付の繰り上がりフラグ
    uint8_t day_offset = 0;
    if (h >= 24)
    {
        h -= 24; // 単純な減算で24の剰余
        day_offset = 1;
    }

    out->hour   = h;
    out->minute = (uint8_t)atoui(fields[1] + 2, 2);
    out->second = (uint8_t)atoui(fields[1] + 4, 2);

    // 日付の解析
    // fields[9]に"140426" (2026年4月14日)の形式で格納されている
    out->day   = (uint8_t)atoui(fields[9], 2);
    out->month = (uint8_t)atoui(fields[9] + 2, 2);
    out->year  = (uint8_t)atoui(fields[9] + 4, 2);

    // 日本時間補正による日付の繰り上がり処理
    if (day_offset)
    {
        adjust_jst_calender(out);
    }

    // 座標
    out->latitude = parse_coord(fields[3]);
    if (fields[4][0] == 'S') out->latitude = -out->latitude;
    
    out->longitude = parse_coord(fields[5]);
    if (fields[6][0] == 'W') out->longitude = -out->longitude;

    // 速度 (knot -> km/h x100)
    // 1 knot = 1.852 km/h -> x185.2
    // 割り算を避けるため 185倍して100で割るのではなく、
    // 固定小数として (knot * 185) + (knot * 2 / 10)
    uint32_t knots = atoui(fields[7], 5); // 小数点無視で取得
    out->speed = div100(knots * 185);
}

// NMEAセンテンスのチェックサムの検証
static int32_t verify_gps_checksum(const char *s)
{
    uint8_t crc = 0;

    // 先頭の '$' をスキップ
    if (*s == '$') s++;

    // '*' または終端文字が来るまでXOR計算
    while (*s && *s != '*')
    {
        crc ^= (uint8_t)*s;
        s++;
    }

    // '*' が見つからなかった場合はエラー
    if (*s != '*') return 0;
    s++;

    // 文字列内のチェックサム(16進数2桁)を数値に変換
    uint8_t received_crc = 0;
    for (int i = 0; i < 2; i++)
    {
        uint8_t c = (uint8_t)s[i];
        received_crc <<= 4;
        if (c >= '0' && c <= '9') received_crc += (c - '0');
        else if (c >= 'A' && c <= 'F') received_crc += (c - 'A' + 10);
        else if (c >= 'a' && c <= 'f') received_crc += (c - 'a' + 10);
        else return 0; // 不正な文字
    }

    // 計算値と受信値を比較
    return (crc == received_crc);
}

// データが$GPRMCであるかを判定する関数
static int32_t check_gprmc(const char *s)
{
    if (s[0] == '$' &&
        s[1] == 'G' &&
        s[2] == 'P' &&
        s[3] == 'R' &&
        s[4] == 'M' &&
        s[5] == 'C')
    {
        return 1;
    }
    return 0;
}

static void check_gps_receive_rate(void)
{
    static uint32_t rmc_count = 0;
    static uint32_t last_time_us = 0;

    uint32_t current_time_us = GET32(TIMER_TIMELR_RW);

    rmc_count++;

    // 1秒経過判定
    if (current_time_us - last_time_us >= 1000000)
    {
        uint8_t buf_count[6];
        int2str(rmc_count, buf_count);
        uart_send_str("[GPS Stats] ");
        uart_send_str(buf_count);
        uart_send_str(" Hz");
        uart_line_break();

        rmc_count = 0;
        last_time_us = current_time_us;
    }
}

#define MAX_NMEA_LENGTH 128

static void receive_gps(GPS_Data *out)
{
    static char buf_gps[MAX_NMEA_LENGTH];
    static uint8_t idx = 0;

    // データの有無を確認
    while (!(GET32(UART1_FR_RW) & (1 << 4)))
    {
        // データがある場合
        uint8_t c = (uint8_t)(GET32(UART1_DR_RW) & 0xFF);

        if (c == '$') idx = 0;

        if (idx < (MAX_NMEA_LENGTH - 1))
        {
            buf_gps[idx++] = c;

            if (c == '\n')
            {
                buf_gps[idx] = '\0';

                // // DEBUG
                // check_gps_receive_rate();
                // uart_send_str("RAW_GPS: ");
                // uart_send_str(buf_gps);
                // uart_line_break();
                // parse_gprmc(buf_gps, out);

                // チェックサムの正誤検証
                if (verify_gps_checksum(buf_gps))
                {
                    uart_send_str("UART Checksum OK");
                    uart_line_break();
                    // // チェックサムが正しい場合のみ以下の処理を実行
                    parse_gprmc(buf_gps, out);
                    uart_send_str("Parse Success!");
                    uart_line_break();
                }
                else
                {
                    uart_send_str("GPS Checksum error");
                    uart_line_break();
                }
                idx = 0; // 次の受信のためにリセット
            }
        }
    }
}

static void receive_gps_temp(GPS_Data *out)
{
    static char gps_buffer[MAX_NMEA_LENGTH];
    static uint8_t idx = 0;

    while (!(GET32(UART1_FR_RW) & (1 << 4))) 
    {
        uint8_t c = (uint8_t)(GET32(UART1_DR_RW) & 0xFF);

        // ASCIIフィルタ
        if (c >= 0x80) continue;
        // 制御文字も改行(10,13)以外は捨てる
        if (c < 32 && c != 10 && c != 13) continue;

        if (c == '$')
        {
            idx = 0;
            gps_buffer[idx++] = c;
        }
        else if (idx > 0 && idx < (MAX_NMEA_LENGTH - 1))
        {
            gps_buffer[idx++] = c;
            if (c == '\n' || c == '\r')
            {
                gps_buffer[idx] = '\0';

                // $GPRMCのみを抽出
                if (gps_buffer[1] == 'G' && gps_buffer[2] == 'P' && gps_buffer[3] == 'R')
                {
                    parse_gprmc(gps_buffer, out);
                    // ここで一気に出す！(一文字ずつの表示は厳禁)
                    uart_send_str("RAW_GPS: ");
                    uart_send_str(gps_buffer);
                    uart_line_break();
                }
                idx = 0;
            }
        }
    }
}

// baudrate=9600の場合  : ibrd->813, fbrd->52
// baudrate=57600の場合 : ibrd->135, fbrd->41
// baudrate=115200の場合: ibrd-> 67, fbrd->53
static void update_gps_baudrate(uint8_t *array, uint32_t ibrd, uint32_t fbrd)
{
    for (int i = 0; i < 28; i++)
    {
        while (GET32(UART1_FR_RW) & (1 << 5));
        PUT32(UART1_DR_RW, array[i]);
    }

    // 送信完了を待機
    while (!(GET32(UART1_FR_RW) & (1 << 7)));
    while (GET32(UART1_FR_RW) & (1 << 3));
    delay_ms(10);

    // RP2040側のボーレート変更設定
    set_uart1_baudrate(ibrd, fbrd);
    delay_ms(10);
}

static void set_ubx_checksum(uint8_t *array, int size)
{
    uint8_t ck_a = 0;
    uint8_t ck_b = 0;

    // インデックス2(Class)からsize-2(チェックサムの手前)まで計算
    for (int i = 2; i < size - 2; i++)
    {
        ck_a = ck_a + array[i];
        ck_b = ck_b + ck_a;
    }

    array[size - 2] = ck_a;
    array[size - 1] = ck_b;
}

static void change_neo6_setting(uint8_t *array, uint32_t len)
{
    for (int i = 0; i < len; i++)
    {
        while (GET32(UART1_FR_RW) & (1 << 5));
        PUT32(UART1_DR_RW, array[i]);
    }

    // 送信完了を待機
    while (!(GET32(UART1_FR_RW) & (1 << 7)));
    while (GET32(UART1_FR_RW) & (1 << 3));
}

static void set_gps_msg_only_rmc(void)
{
    // 停止対象メッセージID: GGA(00), GLL(01), GSA(02), GSV(03), VTG(05)
    uint8_t ids[] = {0x00, 0x01, 0x02, 0x03, 0x05};

    for (int i = 0; i < 5; i++)
    {
        uint8_t cmd[] = {
            0xB5, 0x62,   // Header
            0x06, 0x01,   // CFG-MSG
            0x03, 0x00,   // Length
            0xF0, ids[i], // NMEA Class, Message ID
            0x00,         // Rate: 0 (無効化)
            0x00, 0x00    // Checksum
        };
        // 明示的に11バイトと指定
        set_ubx_checksum(cmd, 11);

        // 送信
        change_neo6_setting(cmd, 11);
        delay_ms(100);
    }
    delay_ms(400);
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
    // uart_send_str("Monitor Init 1");
    // uart_line_break();
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

    // uart_send_str("Monitor Init 2");
    // uart_line_break();
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

int get_gps(void)
{
    clock_init();
    reset_subsys();
    led_init();
    uart0_init();
    uart1_init();
    i2c_init();
    monitor_init();
    
    PUT32(SYST_RVR, 125000 - 1); // 125MHz設定のため、1msカウントにセット（1s設定できないため）
    PUT32(SYST_CVR, 0);          // 125MHz設定のため、1msカウントにセット（1s設定できないため）
    PUT32(SYST_CSR, 0x07);       // プロセッサクロック使用、例外ステータス保留、カウンター有効
    
    // モニターのスレーブアドレス
    const uint8_t monitor_addr = 0x3E;
    bootup_monitor(monitor_addr);

    static const uint8_t ubx_cfg_baud_57600[] = {
        0xB5, 0x62,             // Sync Char (Header)
        0x06, 0x00,             // Class: 0x06(CFG), ID: 0x00(PRT)
        0x14, 0x00,             // Payload Length: 20 bytes (Little Endian)
        0x01,                   // Port ID (1 = UART1)
        0x00,                   // Reserved
        0x00, 0x00,             // TX Ready Pins(0)
        0xD0, 0x08, 0x00, 0x00, // Mode: 8bit, No parity, Stop bit 1 (0x000008D0)
        0x00, 0xE1, 0x00, 0x00, // Baudrate: 57600 (0x0000E100)
        0x03, 0x00,             // InProto Mask: UBX + NMEA (0x0003)
        0x03, 0x00,             // OutProto Mask: UBX + NMEA (0x0003)
        0x00, 0x00,             // Flags (0)
        0x00, 0x00,             // Reserved (0)
        0x0E, 0xB1              // Checksum (CK_A, CK_B)
    };

    static const uint8_t ubx_cfg_rate_5hz[] = {
        0xB5, 0x62, // Sync Char (Header)
        0x06, 0x08, // Class: 0x06 (CFG), ID: 0x08 (RATE)
        0x06, 0x00, // Payload Length: 6 bytes (Little Endian)
        0xC8, 0x00, // measRate: 200ms (1/0.2 = 5Hz)
        0x01, 0x00, // navRate: 1 (Every 1 measurement)
        0x01, 0x00, // timeRef: 1 (GPS Time)
        0xDE, 0x6A  // Checksum (CK_A, CK_B)
    };

    // update_gps_baudrate(ubx_cfg_baud_57600, 135, 41);
    // uart_send_str("NEO6M baudrate updated to 57,600 bps");
    // uart_line_break();
    set_gps_msg_only_rmc();
    uart_send_str("NEO6M setting changed: GPRMC only reading");
    uart_line_break();
    change_neo6_setting(ubx_cfg_rate_5hz, 14);
    uart_send_str("NEO6M setting changed: CFG rate 5Hz");
    uart_line_break();

    // GPS受信変数
    GPS_Data NEO6M;
    uint8_t buf_lat[16];
    uint8_t buf_lon[16];

    while (1)
    {
        WFI();

        receive_gps(&NEO6M);

        frac2str4gps(NEO6M.latitude, buf_lat);
        frac2str4gps(NEO6M.longitude, buf_lon);

        uart_send_str(buf_lat);
        uart_line_break();
        uart_send_str(buf_lon);
        uart_line_break();

        // // 画面クリア
        // write_i2c(monitor_addr, 0x00, 0x01); // ディスプレイクリア
        // delay_ms(1);                         // 1ms待機
        // // アドレス記述
        // write_i2c(monitor_addr, 0x00, (0x00 | 0x80));
        // delay_us(30); // 30μs待機

        // write_i2c(monitor_addr, 0x40, 0x58); // X
        // delay_us(30); // 30μs待機

        // // DEBUG
        // // UART1 (GPS) から文字が届いたら
        // if (!(GET32(UART1_FR_RW) & (1 << 4))) { 
        //     uint8_t c = (uint8_t)(GET32(UART1_DR_RW) & 0xFF);
            
        //     // UART0 (PC) へそのまま流す
        //     while (GET32(UART0_FR_RW) & (1 << 5)); 
        //     PUT32(UART0_DR_RW, c);
        // }

        // delay_ms(1000); // 1s待ち
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