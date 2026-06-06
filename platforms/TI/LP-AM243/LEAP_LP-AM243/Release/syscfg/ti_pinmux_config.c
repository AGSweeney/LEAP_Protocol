/*
 *  Copyright (C) 2021-2026 Texas Instruments Incorporated
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
#include "ti_drivers_config.h"
#include <drivers/pinmux.h>

static Pinmux_PerCfg_t gPinMuxMainDomainCfg[] = {
            /* I2C0 pin config */
    /* I2C0_SCL -> I2C0_SCL (B16) */
    {
        PIN_I2C0_SCL,
        ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* I2C0 pin config */
    /* I2C0_SDA -> I2C0_SDA (B15) */
    {
        PIN_I2C0_SDA,
        ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },

                /* GPIO0_22 -> GPMC0_AD7 (U19) */
    {
        PIN_GPMC0_AD7,
        ( PIN_MODE(7) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
                /* GPIO0_26 -> GPMC0_AD11 (W20) */
    {
        PIN_GPMC0_AD11,
        ( PIN_MODE(7) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
                /* GPIO0_27 -> GPMC0_AD12 (Y20) */
    {
        PIN_GPMC0_AD12,
        ( PIN_MODE(7) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
                /* GPIO0_18 -> GPMC0_AD3 (V21) */
    {
        PIN_GPMC0_AD3,
        ( PIN_MODE(7) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
                /* GPIO0_82 -> GPMC0_AD4 (U21) */
    {
        PIN_GPMC0_AD4,
        ( PIN_MODE(7) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
                /* GPIO0_83 -> GPMC0_AD5 (T20) */
    {
        PIN_GPMC0_AD5,
        ( PIN_MODE(7) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
                /* GPIO0_21 -> GPMC0_AD6 (T18) */
    {
        PIN_GPMC0_AD6,
        ( PIN_MODE(7) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
                /* GPIO0_23 -> GPMC0_AD8 (U18) */
    {
        PIN_GPMC0_AD8,
        ( PIN_MODE(7) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
                /* GPIO1_54 -> UART0_CTSn (B9) */
    {
        PIN_UART0_CTSN,
        ( PIN_MODE(7) | PIN_INPUT_ENABLE | PIN_PULL_DIRECTION )
    },
                /* GPIO0_24 -> GPMC0_AD9 (U20) */
    {
        PIN_GPMC0_AD9,
        ( PIN_MODE(7) | PIN_INPUT_ENABLE | PIN_PULL_DIRECTION )
    },
                /* GPIO0_25 -> GPMC0_AD10 (V20) */
    {
        PIN_GPMC0_AD10,
        ( PIN_MODE(7) | PIN_INPUT_ENABLE | PIN_PULL_DIRECTION )
    },
                /* GPIO0_28 -> GPMC0_AD13 (Y19) */
    {
        PIN_GPMC0_AD13,
        ( PIN_MODE(7) | PIN_INPUT_ENABLE | PIN_PULL_DIRECTION )
    },
                /* GPIO0_29 -> GPMC0_AD14 (Y18) */
    {
        PIN_GPMC0_AD14,
        ( PIN_MODE(7) | PIN_INPUT_ENABLE | PIN_PULL_DIRECTION )
    },
                /* GPIO0_30 -> GPMC0_AD15 (AA19) */
    {
        PIN_GPMC0_AD15,
        ( PIN_MODE(7) | PIN_INPUT_ENABLE | PIN_PULL_DIRECTION )
    },
                /* GPIO1_43 -> SPI0_CS1 (B7) */
    {
        PIN_SPI0_CS1,
        ( PIN_MODE(7) | PIN_INPUT_ENABLE | PIN_PULL_DIRECTION )
    },
                /* GPIO1_44 -> SPI0_CLK (B8) */
    {
        PIN_SPI0_CLK,
        ( PIN_MODE(7) | PIN_INPUT_ENABLE | PIN_PULL_DIRECTION )
    },

            /* PRU_ICSSG1_MDIO0 pin config */
    /* PRG1_MDIO0_MDC -> PRG1_MDIO0_MDC (W1) */
    {
        PIN_PRG1_MDIO0_MDC,
        ( PIN_MODE(0) | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_MDIO0 pin config */
    /* PRG1_MDIO0_MDIO -> PRG1_MDIO0_MDIO (V2) */
    {
        PIN_PRG1_MDIO0_MDIO,
        ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_IEP0 pin config */
    /* PRG1_IEP0_EDC_LATCH_IN0 -> PRG1_PRU0_GPO18 (Y4) */
    {
        PIN_PRG1_PRU0_GPO18,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_IEP0 pin config */
    /* PRG1_IEP0_EDC_SYNC_OUT0 -> PRG1_PRU0_GPO19 (U3) */
    {
        PIN_PRG1_PRU0_GPO19,
        ( PIN_MODE(2) | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII1 pin config */
    /* PRG1_RGMII1_RD0 -> PRG1_PRU0_GPO0 (V4) */
    {
        PIN_PRG1_PRU0_GPO0,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII1 pin config */
    /* PRG1_RGMII1_RD1 -> PRG1_PRU0_GPO1 (W5) */
    {
        PIN_PRG1_PRU0_GPO1,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII1 pin config */
    /* PRG1_RGMII1_RD2 -> PRG1_PRU0_GPO2 (AA4) */
    {
        PIN_PRG1_PRU0_GPO2,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII1 pin config */
    /* PRG1_RGMII1_RD3 -> PRG1_PRU0_GPO3 (Y5) */
    {
        PIN_PRG1_PRU0_GPO3,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII1 pin config */
    /* PRG1_RGMII1_RXC -> PRG1_PRU0_GPO6 (Y2) */
    {
        PIN_PRG1_PRU0_GPO6,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII1 pin config */
    /* PRG1_RGMII1_RX_CTL -> PRG1_PRU0_GPO4 (AA5) */
    {
        PIN_PRG1_PRU0_GPO4,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII1 pin config */
    /* PRG1_RGMII1_TD0 -> PRG1_PRU0_GPO11 (V5) */
    {
        PIN_PRG1_PRU0_GPO11,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII1 pin config */
    /* PRG1_RGMII1_TD1 -> PRG1_PRU0_GPO12 (W2) */
    {
        PIN_PRG1_PRU0_GPO12,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII1 pin config */
    /* PRG1_RGMII1_TD2 -> PRG1_PRU0_GPO13 (V6) */
    {
        PIN_PRG1_PRU0_GPO13,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII1 pin config */
    /* PRG1_RGMII1_TD3 -> PRG1_PRU0_GPO14 (AA7) */
    {
        PIN_PRG1_PRU0_GPO14,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII1 pin config */
    /* PRG1_RGMII1_TXC -> PRG1_PRU0_GPO16 (W6) */
    {
        PIN_PRG1_PRU0_GPO16,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII1 pin config */
    /* PRG1_RGMII1_TX_CTL -> PRG1_PRU0_GPO15 (Y7) */
    {
        PIN_PRG1_PRU0_GPO15,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
            /* PRU_ICSSG1_RGMII2 pin config */
    /* PRG1_RGMII2_RD0 -> PRG1_PRU1_GPO0 (AA10) */
    {
        PIN_PRG1_PRU1_GPO0,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII2 pin config */
    /* PRG1_RGMII2_RD1 -> PRG1_PRU1_GPO1 (Y10) */
    {
        PIN_PRG1_PRU1_GPO1,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII2 pin config */
    /* PRG1_RGMII2_RD2 -> PRG1_PRU1_GPO2 (Y11) */
    {
        PIN_PRG1_PRU1_GPO2,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII2 pin config */
    /* PRG1_RGMII2_RD3 -> PRG1_PRU1_GPO3 (V12) */
    {
        PIN_PRG1_PRU1_GPO3,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII2 pin config */
    /* PRG1_RGMII2_RXC -> PRG1_PRU1_GPO6 (V10) */
    {
        PIN_PRG1_PRU1_GPO6,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII2 pin config */
    /* PRG1_RGMII2_RX_CTL -> PRG1_PRU1_GPO4 (Y12) */
    {
        PIN_PRG1_PRU1_GPO4,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII2 pin config */
    /* PRG1_RGMII2_TD0 -> PRG1_PRU1_GPO11 (Y6) */
    {
        PIN_PRG1_PRU1_GPO11,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII2 pin config */
    /* PRG1_RGMII2_TD1 -> PRG1_PRU1_GPO12 (AA8) */
    {
        PIN_PRG1_PRU1_GPO12,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII2 pin config */
    /* PRG1_RGMII2_TD2 -> PRG1_PRU1_GPO13 (Y9) */
    {
        PIN_PRG1_PRU1_GPO13,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII2 pin config */
    /* PRG1_RGMII2_TD3 -> PRG1_PRU1_GPO14 (W9) */
    {
        PIN_PRG1_PRU1_GPO14,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII2 pin config */
    /* PRG1_RGMII2_TXC -> PRG1_PRU1_GPO16 (Y8) */
    {
        PIN_PRG1_PRU1_GPO16,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* PRU_ICSSG1_RGMII2 pin config */
    /* PRG1_RGMII2_TX_CTL -> PRG1_PRU1_GPO15 (V9) */
    {
        PIN_PRG1_PRU1_GPO15,
        ( PIN_MODE(2) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },


            /* USART0 pin config */
    /* UART0_RXD -> UART0_RXD (B10) */
    {
        PIN_UART0_RXD,
        ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
    },
    /* USART0 pin config */
    /* UART0_TXD -> UART0_TXD (B11) */
    {
        PIN_UART0_TXD,
        ( PIN_MODE(0) | PIN_PULL_DISABLE )
    },

    {PINMUX_END, PINMUX_END}
};

static Pinmux_PerCfg_t gPinMuxMcuDomainCfg[] = {
        
                                                                                                                                
                

        
    {PINMUX_END, PINMUX_END}
};

/*
 * Pinmux
 */


void Pinmux_init(void)
{



    SOC_fixFastDriveStrength();
    Pinmux_config(gPinMuxMainDomainCfg, PINMUX_DOMAIN_ID_MAIN);
    
    Pinmux_config(gPinMuxMcuDomainCfg, PINMUX_DOMAIN_ID_MCU);
}


