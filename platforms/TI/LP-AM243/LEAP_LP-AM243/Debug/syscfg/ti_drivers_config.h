/*
 *  Copyright (C) 2021 Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Auto generated file
 */

#ifndef TI_DRIVERS_CONFIG_H_
#define TI_DRIVERS_CONFIG_H_

#include <stdint.h>
#include <drivers/hw_include/cslr_soc.h>
#include <drivers/hw_include/hw_types.h>
#include "ti_dpl_config.h"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Common Functions
 */
void System_init(void);
void System_deinit(void);

/*
 * I2C
 */

/* I2C Instance Macros */
#define CONFIG_I2C0 (0U)

/* I2C Includes */
#include <drivers/i2c.h>
#include <kernel/dpl/ClockP.h>

#define CONFIG_I2C_HLD_NUM_INSTANCES (1U)

/*
 * GPIO
 */
#include <drivers/gpio.h>
#include <kernel/dpl/AddrTranslateP.h>

/* GPIO PIN Macros */
#define LEAP_GPIO_LED_TEST1_GRN_BASE_ADDR (CSL_GPIO0_BASE)
#define LEAP_GPIO_LED_TEST1_GRN_PIN (22)
#define LEAP_GPIO_LED_TEST1_GRN_DIR (GPIO_DIRECTION_OUTPUT)
#define LEAP_GPIO_LED_TEST1_GRN_TRIG_TYPE (GPIO_TRIG_TYPE_NONE)
#define LEAP_GPIO_LED_TEST2_RED_BASE_ADDR (CSL_GPIO1_BASE)
#define LEAP_GPIO_LED_TEST2_RED_PIN (55)
#define LEAP_GPIO_LED_TEST2_RED_DIR (GPIO_DIRECTION_OUTPUT)
#define LEAP_GPIO_LED_TEST2_RED_TRIG_TYPE (GPIO_TRIG_TYPE_NONE)
#define LEAP_GPIO_LED_TEST3_RED_BASE_ADDR (CSL_GPIO0_BASE)
#define LEAP_GPIO_LED_TEST3_RED_PIN (20)
#define LEAP_GPIO_LED_TEST3_RED_DIR (GPIO_DIRECTION_OUTPUT)
#define LEAP_GPIO_LED_TEST3_RED_TRIG_TYPE (GPIO_TRIG_TYPE_NONE)
#define LEAP_GPIO_LED_TEST4_GRN_BASE_ADDR (CSL_GPIO0_BASE)
#define LEAP_GPIO_LED_TEST4_GRN_PIN (84)
#define LEAP_GPIO_LED_TEST4_GRN_DIR (GPIO_DIRECTION_OUTPUT)
#define LEAP_GPIO_LED_TEST4_GRN_TRIG_TYPE (GPIO_TRIG_TYPE_NONE)
#define LEAP_GPIO_BTN_USER_BASE_ADDR (CSL_GPIO1_BASE)
#define LEAP_GPIO_BTN_USER_PIN (54)
#define LEAP_GPIO_BTN_USER_DIR (GPIO_DIRECTION_INPUT)
#define LEAP_GPIO_BTN_USER_TRIG_TYPE (GPIO_TRIG_TYPE_NONE)
#define CONFIG_GPIO_NUM_INSTANCES (5U)

/* ENET MACROS */

/*
 * PRUICSS
 */
#include <drivers/pruicss.h>

/* PRUICSS Instance Macros */
#define CONFIG_PRU_ICSS0 (0U)
#define CONFIG_PRU_ICSS0_CORE_CLK_FREQ_HZ     (333333333U)
#define CONFIG_PRU_ICSS0_CORE_CLK_PERIOD_NSEC (3)
#define CONFIG_PRU_ICSS0_IEP_CLK_FREQ_HZ      (200000000U)
#define CONFIG_PRU_ICSS0_IEP_CLK_PERIOD_NSEC  (5)
#define CONFIG_PRU_ICSS0_UART_CLK_FREQ_HZ      (192000000U)
#define CONFIG_PRU_ICSS0_UART_CLK_PERIOD_NSEC  (5.208333333333333)
#define CONFIG_PRUICSS_NUM_INSTANCES (1U)

/*
 * UDMA
 */
#include <drivers/udma.h>

/* UDMA Instance Macros */
#define CONFIG_UDMA_PKTDMA_0 (0U)
#define CONFIG_UDMA_NUM_INSTANCES (1U)

/* UDMA Driver Objects */
extern Udma_DrvObject   gUdmaDrvObj[CONFIG_UDMA_NUM_INSTANCES];

/* UDMA functions as specified in SYSCONFIG */
/* For instance CONFIG_UDMA_PKTDMA_0 */
extern uint64_t Udma_defaultVirtToPhyFxn(const void *virtAddr, uint32_t chNum, void *appData);
extern void *Udma_defaultPhyToVirtFxn(uint64_t phyAddr, uint32_t chNum, void *appData);


/*
 * UART
 */
#include <drivers/uart.h>
/* UART Instance Macros */
#define CONFIG_UART0 (0U)
#define CONFIG_UART_NUM_INSTANCES (1U)
#define CONFIG_UART_NUM_DMA_INSTANCES (0U)

/*
 *  ICSS_INTC
 */
#include <pru_io/driver/icss_intc_defines.h>
extern PRUICSS_IntcInitData icss1_intc_initdata;


#include <drivers/soc.h>
#include <kernel/dpl/CycleCounterP.h>

/*
 * MCU_LBIST
 */
void SDL_lbist_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* TI_DRIVERS_CONFIG_H_ */
