/*
 * SPDX-FileCopyrightText: 2026 SMILE (smile.eu)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_CH570_CLOCK_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_CH570_CLOCK_H_

/**
 * @file
 * @brief Clock ID configuration constants for WCH CH570/CH572 device tree bindings.
 *
 * @note This clock controller configuration header supports WCH CH570/CH572 SoCs only.
 */

/** Offset of the first SLP clock control register from the peripheral base address */
#define CH570_SLP_CLK_OFF0_OFFSET 0
/** Offset of the second SLP clock control register from the peripheral base address */
#define CH570_SLP_CLK_OFF1_OFFSET 1

/**
 * Clock configuration macro that encodes the following information into a single byte:
 * - Bits [7:3]: Offset of the target SLP clock control register (0-31)
 * - Bits [2:0]: Clock control bit in the target SLP clock control register (0-7)
 */
#define CH570_CLOCK_CONFIG(offset, bit) (((CH570_SLP_CLK_##offset##_OFFSET) << 3) | (bit))

/** Extract the SLP clock control register offset from a clock configuration ID */
#define CH570_GET_OFFSET(id) (((id) >> 3) & 0x1F)

/** Extract the clock control bit index from a clock configuration ID */
#define CH570_GET_BIT_IDX(id) ((id) & 0x7)

/** TMR clock control configuration */
#define CH570_CLOCK_TMR    CH570_CLOCK_CONFIG(OFF0, 0)
/** CMP clock control configuration */
#define CH570_CLOCK_CMP    CH570_CLOCK_CONFIG(OFF0, 1)
/** UART clock control configuration */
#define CH570_CLOCK_UART   CH570_CLOCK_CONFIG(OFF0, 4)
/** Other clock control configurations */
#define CH570_CLOCK_SPI    CH570_CLOCK_CONFIG(OFF1, 0)
/** AES/CCM clock control configuration */
#define CH570_CLOCK_AESCCM CH570_CLOCK_CONFIG(OFF1, 1)
/** PWM clock control configuration */
#define CH570_CLOCK_PWM    CH570_CLOCK_CONFIG(OFF1, 2)
/** I2C clock control configuration */
#define CH570_CLOCK_I2C    CH570_CLOCK_CONFIG(OFF1, 3)
/** USB clock control configuration */
#define CH570_CLOCK_USB    CH570_CLOCK_CONFIG(OFF1, 4)
/** BLE clock control configuration (CH572 only) */
#define CH570_CLOCK_BLE    CH570_CLOCK_CONFIG(OFF1, 7)

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_CH570_CLOCK_H_ */
