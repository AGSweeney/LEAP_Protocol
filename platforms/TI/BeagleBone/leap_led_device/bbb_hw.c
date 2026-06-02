#include "bbb_hw.h"

#define REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))

#define WDT1_BASE          0x44E35000u
#define WDT_WWPS           0x34u
#define WDT_WSPR           0x48u

#define UART0_BASE         0x44E09000u
#define UART_THR           0x00u
#define UART_LSR           0x14u
#define UART_LSR_THRE      (1u << 5)

#define GPIO1_BASE         0x4804C000u
#define GPIO2_BASE         0x481AC000u
#define GPIO0_BASE         0x44E07000u
#define GPIO3_BASE         0x481AE000u
#define GPIO_OE            0x134u
#define GPIO_DATAIN        0x138u
#define GPIO_SETDATAOUT    0x194u
#define GPIO_CLEARDATAOUT  0x190u

#define SOC_CONTROL_REGS   0x44E10000u
#define SOC_PRCM_REGS      0x44E00000u
#define CM_PER_GPIO1_CLKCTRL 0xACu
#define CM_PER_GPIO2_CLKCTRL 0xB0u
#define CM_PER_GPIO3_CLKCTRL 0xB4u
#define CM_PER_MODULEMODE_ENABLE 0x2u

#define CONTROL_CONF_GPMC_ADVN_ALE   0x840u /* P8_7  -> gpio2_2 */
#define CONTROL_CONF_GPMC_OEN_REN    0x848u /* P8_8  -> gpio2_3 */
#define CONTROL_CONF_GPMC_WEN        0x83Cu /* P8_10 -> gpio2_4 */
#define CONTROL_CONF_GPMC_BEN0_CLE   0x878u /* P8_9  -> gpio2_5 */
#define CONTROL_CONF_GPMC_AD8        0x820u /* P8_19 -> gpio0_22 (DO0) */
#define CONTROL_CONF_GPMC_AD9        0x824u /* P8_13 -> gpio0_23 (DO1) */
#define CONTROL_CONF_GPMC_AD10       0x828u /* P8_14 -> gpio0_26 (DO2) */
#define CONTROL_CONF_GPMC_AD11       0x82Cu /* P8_17 -> gpio0_27 (DO3) */
#define CONTROL_CONF_GPMC_AD12       0x830u /* P8_12 -> gpio1_12 (DI0) */
#define CONTROL_CONF_GPMC_AD13       0x834u /* P8_11 -> gpio1_13 (DI1) */
#define CONTROL_CONF_GPMC_AD14       0x838u /* P8_16 -> gpio1_14 (DI2) */
#define CONTROL_CONF_GPMC_AD15       0x83Cu /* P8_15 -> gpio1_15 (DI3) */
#define CONTROL_CONF_MCASP0_AHCLKX   0x9ACu /* P9_25 -> gpio3_21 (DI4) */
#define CONTROL_CONF_MCASP0_FSR      0x9A4u /* P9_27 -> gpio3_19 (DI5) */
#define CONTROL_CONF_MCASP0_FSX      0x994u /* P9_29 -> gpio3_15 (DI6) */
#define CONTROL_CONF_MCASP0_ACLKX    0x990u /* P9_31 -> gpio3_14 (DI7) */

#define BBB_USR0_BIT       (1u << 21)
#define BBB_USR1_BIT       (1u << 22)
#define BBB_USR2_BIT       (1u << 23)
#define BBB_USR3_BIT       (1u << 24)
#define BBB_USR_LED_MASK   (BBB_USR0_BIT | BBB_USR1_BIT | BBB_USR2_BIT | BBB_USR3_BIT)

#define BBB_OUT4_BIT       (1u << 2)  /* gpio2_2  -> P8_7  */
#define BBB_OUT5_BIT       (1u << 3)  /* gpio2_3  -> P8_8  */
#define BBB_OUT6_BIT       (1u << 4)  /* gpio2_4  -> P8_10 */
#define BBB_OUT7_BIT       (1u << 5)  /* gpio2_5  -> P8_9  */
#define BBB_GPIO2_OUT_MASK (BBB_OUT4_BIT | BBB_OUT5_BIT | BBB_OUT6_BIT | BBB_OUT7_BIT)
#define BBB_OUT0_BIT       (1u << 22) /* gpio0_22 -> P8_19 */
#define BBB_OUT1_BIT       (1u << 23) /* gpio0_23 -> P8_13 */
#define BBB_OUT2_BIT       (1u << 26) /* gpio0_26 -> P8_14 */
#define BBB_OUT3_BIT       (1u << 27) /* gpio0_27 -> P8_17 */
#define BBB_GPIO0_OUT_MASK (BBB_OUT0_BIT | BBB_OUT1_BIT | BBB_OUT2_BIT | BBB_OUT3_BIT)
#define BBB_IN0_BIT        (1u << 12) /* gpio1_12 <- P8_12 */
#define BBB_IN1_BIT        (1u << 13) /* gpio1_13 <- P8_11 */
#define BBB_IN2_BIT        (1u << 14) /* gpio1_14 <- P8_16 */
#define BBB_IN3_BIT        (1u << 15) /* gpio1_15 <- P8_15 */
#define BBB_GPIO1_IN_MASK  (BBB_IN0_BIT | BBB_IN1_BIT | BBB_IN2_BIT | BBB_IN3_BIT)
#define BBB_IN4_BIT        (1u << 21) /* gpio3_21 <- P9_25 */
#define BBB_IN5_BIT        (1u << 19) /* gpio3_19 <- P9_27 */
#define BBB_IN6_BIT        (1u << 15) /* gpio3_15 <- P9_29 */
#define BBB_IN7_BIT        (1u << 14) /* gpio3_14 <- P9_31 */
#define BBB_GPIO3_IN_MASK  (BBB_IN4_BIT | BBB_IN5_BIT | BBB_IN6_BIT | BBB_IN7_BIT)

static void bbb_clk_enable(unsigned int clkctrl_offset)
{
    REG32(SOC_PRCM_REGS + clkctrl_offset) =
        (REG32(SOC_PRCM_REGS + clkctrl_offset) & ~0x3u) | CM_PER_MODULEMODE_ENABLE;

    while ((REG32(SOC_PRCM_REGS + clkctrl_offset) & 0x3u) != CM_PER_MODULEMODE_ENABLE) {
    }
}

static void bbb_pinmux_to_gpio(unsigned int conf_offset)
{
    /* mode7 = gpio, pull-up/down disabled, receiver disabled for push output */
    REG32(SOC_CONTROL_REGS + conf_offset) = 0x07u;
}

static void bbb_pinmux_to_gpio_input_pullup(unsigned int conf_offset)
{
    /* mode7 gpio + pull enabled/pull-up + receiver enabled */
    REG32(SOC_CONTROL_REGS + conf_offset) = 0x2Fu;
}

void bbb_delay(volatile uint32_t count)
{
    while (count-- != 0u) {
        __asm__ volatile("nop");
    }
}

static void wdt_wait_posted(uint32_t base)
{
    uint32_t timeout = 1000000u;

    while ((REG32(base + WDT_WWPS) != 0u) && (timeout-- != 0u)) {
        __asm__ volatile("nop");
    }
}

static void wdt_disable(uint32_t base)
{
    REG32(base + WDT_WSPR) = 0xAAAAu;
    wdt_wait_posted(base);
    REG32(base + WDT_WSPR) = 0x5555u;
    wdt_wait_posted(base);
}

void bbb_wdt_disable_all(void)
{
    /*
     * WDT0 (0x44E33000) is in WKUP and faults unless its PRCM clock is on;
     * U-Boot only uses WDT1 on BBB. Disable WDT1 before the LEAP loop runs.
     */
    wdt_disable(WDT1_BASE);
}

void bbb_uart_putc(uint8_t ch)
{
    while ((REG32(UART0_BASE + UART_LSR) & UART_LSR_THRE) == 0u) {
        __asm__ volatile("nop");
    }

    REG32(UART0_BASE + UART_THR) = ch;
}

void bbb_uart_puts(const char* text)
{
    while (text != NULL && *text != '\0') {
        if (*text == '\n') {
            bbb_uart_putc('\r');
        }
        bbb_uart_putc((uint8_t)*text);
        text++;
    }
}

static void bbb_uart_put_hex_nibble(uint8_t value)
{
    value &= 0x0Fu;
    if (value < 10u) {
        bbb_uart_putc((uint8_t)((uint8_t)'0' + value));
    } else {
        bbb_uart_putc((uint8_t)((uint8_t)'A' + value - 10u));
    }
}

void bbb_uart_put_hex8(uint8_t value)
{
    bbb_uart_put_hex_nibble((uint8_t)(value >> 4));
    bbb_uart_put_hex_nibble(value);
}

void bbb_uart_put_hex16(uint16_t value)
{
    bbb_uart_put_hex8((uint8_t)(value >> 8));
    bbb_uart_put_hex8((uint8_t)value);
}

void bbb_uart_put_hex32(uint32_t value)
{
    bbb_uart_put_hex16((uint16_t)(value >> 16));
    bbb_uart_put_hex16((uint16_t)value);
}

void bbb_leds_init(void)
{
    bbb_clk_enable(CM_PER_GPIO1_CLKCTRL);
    bbb_clk_enable(CM_PER_GPIO2_CLKCTRL);
    bbb_clk_enable(CM_PER_GPIO3_CLKCTRL);

    bbb_pinmux_to_gpio(CONTROL_CONF_GPMC_ADVN_ALE);
    bbb_pinmux_to_gpio(CONTROL_CONF_GPMC_OEN_REN);
    bbb_pinmux_to_gpio(CONTROL_CONF_GPMC_WEN);
    bbb_pinmux_to_gpio(CONTROL_CONF_GPMC_BEN0_CLE);
    bbb_pinmux_to_gpio(CONTROL_CONF_GPMC_AD8);
    bbb_pinmux_to_gpio(CONTROL_CONF_GPMC_AD9);
    bbb_pinmux_to_gpio(CONTROL_CONF_GPMC_AD10);
    bbb_pinmux_to_gpio(CONTROL_CONF_GPMC_AD11);
    bbb_pinmux_to_gpio_input_pullup(CONTROL_CONF_GPMC_AD12);
    bbb_pinmux_to_gpio_input_pullup(CONTROL_CONF_GPMC_AD13);
    bbb_pinmux_to_gpio_input_pullup(CONTROL_CONF_GPMC_AD14);
    bbb_pinmux_to_gpio_input_pullup(CONTROL_CONF_GPMC_AD15);
    bbb_pinmux_to_gpio_input_pullup(CONTROL_CONF_MCASP0_AHCLKX);
    bbb_pinmux_to_gpio_input_pullup(CONTROL_CONF_MCASP0_FSR);
    bbb_pinmux_to_gpio_input_pullup(CONTROL_CONF_MCASP0_FSX);
    bbb_pinmux_to_gpio_input_pullup(CONTROL_CONF_MCASP0_ACLKX);

    REG32(GPIO0_BASE + GPIO_OE) &= ~BBB_GPIO0_OUT_MASK;
    REG32(GPIO1_BASE + GPIO_OE) &= ~BBB_USR_LED_MASK;
    REG32(GPIO1_BASE + GPIO_OE) |= BBB_GPIO1_IN_MASK;
    REG32(GPIO2_BASE + GPIO_OE) &= ~BBB_GPIO2_OUT_MASK;
    REG32(GPIO3_BASE + GPIO_OE) |= BBB_GPIO3_IN_MASK;
    bbb_status_leds_apply(0u);
    bbb_gpio_outputs_apply(0u);
}

void bbb_status_leds_apply(uint8_t status_mask)
{
    uint32_t gpio1_set_mask = 0u;

    if ((status_mask & 0x01u) != 0u) {
        gpio1_set_mask |= BBB_USR0_BIT;
    }
    if ((status_mask & 0x02u) != 0u) {
        gpio1_set_mask |= BBB_USR1_BIT;
    }
    if ((status_mask & 0x04u) != 0u) {
        gpio1_set_mask |= BBB_USR2_BIT;
    }
    if ((status_mask & 0x08u) != 0u) {
        gpio1_set_mask |= BBB_USR3_BIT;
    }

    REG32(GPIO1_BASE + GPIO_CLEARDATAOUT) = BBB_USR_LED_MASK;
    REG32(GPIO1_BASE + GPIO_SETDATAOUT) = gpio1_set_mask;
}

void bbb_gpio_outputs_apply(uint16_t outputs)
{
    uint32_t gpio0_set_mask = 0u;
    uint32_t gpio2_set_mask = 0u;

    /*
     * USR LEDs are reserved for service status; digital outputs use header pins.
     * Mapping:
     *   bit0 -> P8_19 (gpio0_22)
     *   bit1 -> P8_13 (gpio0_23)
     *   bit2 -> P8_14 (gpio0_26)
     *   bit3 -> P8_17 (gpio0_27)
     *   bit4 -> P8_7  (gpio2_2)
     *   bit5 -> P8_8  (gpio2_3)
     *   bit6 -> P8_10 (gpio2_4)
     *   bit7 -> P8_9  (gpio2_5)
     */
    if ((outputs & 0x0001u) != 0u) {
        gpio0_set_mask |= BBB_OUT0_BIT;
    }
    if ((outputs & 0x0002u) != 0u) {
        gpio0_set_mask |= BBB_OUT1_BIT;
    }
    if ((outputs & 0x0004u) != 0u) {
        gpio0_set_mask |= BBB_OUT2_BIT;
    }
    if ((outputs & 0x0008u) != 0u) {
        gpio0_set_mask |= BBB_OUT3_BIT;
    }
    if ((outputs & 0x0010u) != 0u) {
        gpio2_set_mask |= BBB_OUT4_BIT;
    }
    if ((outputs & 0x0020u) != 0u) {
        gpio2_set_mask |= BBB_OUT5_BIT;
    }
    if ((outputs & 0x0040u) != 0u) {
        gpio2_set_mask |= BBB_OUT6_BIT;
    }
    if ((outputs & 0x0080u) != 0u) {
        gpio2_set_mask |= BBB_OUT7_BIT;
    }

    REG32(GPIO0_BASE + GPIO_CLEARDATAOUT) = BBB_GPIO0_OUT_MASK;
    REG32(GPIO0_BASE + GPIO_SETDATAOUT) = gpio0_set_mask;
    REG32(GPIO2_BASE + GPIO_CLEARDATAOUT) = BBB_GPIO2_OUT_MASK;
    REG32(GPIO2_BASE + GPIO_SETDATAOUT) = gpio2_set_mask;
}

void bbb_leds_apply(uint16_t outputs)
{
    /* Legacy helper: keep old call sites functional. */
    bbb_gpio_outputs_apply(outputs);
}

uint16_t bbb_gpio_inputs_read(void)
{
    uint32_t datain1 = REG32(GPIO1_BASE + GPIO_DATAIN);
    uint32_t datain3 = REG32(GPIO3_BASE + GPIO_DATAIN);
    uint16_t inputs = 0u;

    if ((datain1 & BBB_IN0_BIT) != 0u) {
        inputs |= 0x0001u;
    }
    if ((datain1 & BBB_IN1_BIT) != 0u) {
        inputs |= 0x0002u;
    }
    if ((datain1 & BBB_IN2_BIT) != 0u) {
        inputs |= 0x0004u;
    }
    if ((datain1 & BBB_IN3_BIT) != 0u) {
        inputs |= 0x0008u;
    }
    if ((datain3 & BBB_IN4_BIT) != 0u) {
        inputs |= 0x0010u;
    }
    if ((datain3 & BBB_IN5_BIT) != 0u) {
        inputs |= 0x0020u;
    }
    if ((datain3 & BBB_IN6_BIT) != 0u) {
        inputs |= 0x0040u;
    }
    if ((datain3 & BBB_IN7_BIT) != 0u) {
        inputs |= 0x0080u;
    }

    return inputs;
}
