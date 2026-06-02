#include "bbb_hw.h"

#include "beaglebone.h"
#include "dmtimer.h"
#include "soc_AM335x.h"

/*
 * DMTimer2 on BBB: module clock is typically 24 MHz from the 32 kHz path /
 * PRCM (see StarterWare DMTimer2ModuleClkConfig). Divide by 24 for ~1 us ticks.
 */
#define BBB_TIMER_REGS     SOC_DMTIMER_2_REGS
#define BBB_TIMER_TICK_HZ  24000000u
#define BBB_TIMER_US_DIV   (BBB_TIMER_TICK_HZ / 1000000u)

static uint32_t g_timer_base_ticks;

void bbb_timer_init(void)
{
    DMTimer2ModuleClkConfig();
    DMTimerResetConfigure(BBB_TIMER_REGS, DMTIMER_SFT_RESET_DISABLE);
    DMTimerModeConfigure(BBB_TIMER_REGS, DMTIMER_AUTORLD_NOCMP_ENABLE);
    DMTimerCounterSet(BBB_TIMER_REGS, 0u);
    DMTimerReloadSet(BBB_TIMER_REGS, 0xFFFFFFFFu);
    DMTimerEnable(BBB_TIMER_REGS);
    g_timer_base_ticks = DMTimerCounterGet(BBB_TIMER_REGS);
}

uint64_t bbb_monotonic_us(void)
{
    uint32_t ticks = DMTimerCounterGet(BBB_TIMER_REGS);
    uint32_t delta = ticks - g_timer_base_ticks;

    return (uint64_t)delta / (uint64_t)BBB_TIMER_US_DIV;
}
