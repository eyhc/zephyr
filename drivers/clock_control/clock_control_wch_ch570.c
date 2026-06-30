/*
 * SPDX-FileCopyrightText: 2026 SMILE (smile.eu)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT wch_ch570_clkctrl

#include <inttypes.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/sys/util_macro.h>

#include <zephyr/dt-bindings/clock/ch570-clock.h>
#include <hal_ch32fun.h>
#include "soc.h"

#define FREQ_LSI   DT_PROP(DT_NODELABEL(clk_lsi), clock_frequency)
#define FREQ_HSE   DT_PROP(DT_NODELABEL(clk_hse), clock_frequency)
#define PLL_SOURCE DT_CLOCKS_CTLR(DT_NODELABEL(pll))
#define FREQ_PLL   (DT_PROP(PLL_SOURCE, clock_frequency) * 18.75)

enum clocks {
	HSE,
	LSI,
	PLL
};

struct clock_control_wch_ch570_config {
	void *regs_clkctrl;
	void *regs_slpctrl;
	enum clocks parent_clk;
	uint8_t hse_current;
	uint8_t hse_capacity;
	uint8_t divider;
};

/* See kernel/sys_clock_hw_cycles.c */
extern unsigned int z_clock_hw_cycles_per_sec;

static int clock_control_wch_ch570_on(const struct device *dev, clock_control_subsys_t sys)
{
	uint32_t irq_key, val;

	const struct clock_control_wch_ch570_config *config = dev->config;
	uint8_t id = (uintptr_t)sys;
	volatile uint8_t *reg = config->regs_slpctrl;

	reg += CH570_GET_OFFSET(id);

	val = *reg;
	val &= ~(1 << CH570_GET_BIT_IDX(id));

	irq_key = sys_safe_access_enable();
	*reg = val;
	sys_safe_access_disable(irq_key);

	return 0;
}

static int clock_control_wch_ch570_off(const struct device *dev, clock_control_subsys_t sys)
{
	uint32_t irq_key, val;

	const struct clock_control_wch_ch570_config *config = dev->config;
	uint8_t id = (uintptr_t)sys;
	volatile uint8_t *reg = config->regs_slpctrl;

	reg += CH570_GET_OFFSET(id);

	val = *reg;
	val |= BIT(CH570_GET_BIT_IDX(id));

	irq_key = sys_safe_access_enable();
	*reg = val;
	sys_safe_access_disable(irq_key);

	return 0;
}

static int clock_control_wch_ch570_get_rate(const struct device *dev, clock_control_subsys_t sys,
					    uint32_t *rate)
{
	ARG_UNUSED(sys);
	const struct clock_control_wch_ch570_config *config = dev->config;

	/*
	 * FckLSI = 24~42KHz;
	 * Fck32m = 32MHz;
	 * Fpll = Fck32m * 18.75 = 600MHz;
	 * Fsys = CLK_MODE == LSI ? FckLSI : (CLK_MODE == PLL ? Fpll : Fck32m) / CLK_DIV;
	 */
	if (config->parent_clk == LSI) {
		*rate = FREQ_LSI;
	} else {
		*rate = (config->parent_clk == PLL) ? FREQ_PLL : FREQ_HSE;
		*rate /= (config->divider == 0) ? 32 : config->divider;
	}
	return 0;
}

static DEVICE_API(clock_control, clock_control_wch_ch570_api) = {
	.on = clock_control_wch_ch570_on,
	.off = clock_control_wch_ch570_off,
	.get_rate = clock_control_wch_ch570_get_rate,
};

static int clock_control_wch_ch570_init(const struct device *dev)
{
	uint32_t irq_key;
	volatile uint8_t *reg;
	uint8_t hse_tune, clk_cfg;
	const struct clock_control_wch_ch570_config *config = dev->config;

	/* Configure HSE */
	reg = config->regs_clkctrl;
	reg += 0x46;
	hse_tune = (config->hse_current & 0x03) | ((config->hse_capacity << 4) & 0x70);
	irq_key = sys_safe_access_enable();
	*reg = hse_tune;
	sys_safe_access_disable(irq_key);

	/* Configure source clock (for HCLK) */
	reg = config->regs_clkctrl;
	clk_cfg = 0x00;

	switch (config->parent_clk) {
	case HSE:
		clk_cfg |= (0b00 << 6);
		break;
	case PLL:
		clk_cfg |= (0b01 << 6);
		break;
	case LSI:
		clk_cfg |= (0b11 << 6);
		break;
	}

	clk_cfg |= config->divider & 0x1F;

	irq_key = sys_safe_access_enable();
	*reg = clk_cfg;
	sys_safe_access_disable(irq_key);

	/* Disable clock for all devices */
	reg = config->regs_slpctrl;
	irq_key = sys_safe_access_enable();
	*reg = 0x1F;
	*(reg + 1) = 0xFF;
	sys_safe_access_disable(irq_key);

	/* Set runtime frequency */
	clock_control_wch_ch570_get_rate(dev, NULL, &z_clock_hw_cycles_per_sec);

	return 0;
}

#define CLOCK_CONTROL_WCH_SOURCE(idx)                                                              \
	(DT_SAME_NODE(DT_INST_CLOCKS_CTLR(idx), DT_NODELABEL(clk_hse))   ? HSE                     \
	 : DT_SAME_NODE(DT_INST_CLOCKS_CTLR(idx), DT_NODELABEL(clk_lsi)) ? LSI                     \
	 : DT_SAME_NODE(DT_INST_CLOCKS_CTLR(idx), DT_NODELABEL(pll))     ? PLL                     \
									 : HSE)

#define CLOCK_CONTROL_WCH_INIT(idx)                                                                \
	static const struct clock_control_wch_ch570_config                                         \
		clock_control_wch_ch570_##idx##_config = {                                         \
			.regs_clkctrl = (void *)DT_INST_REG_ADDR_BY_NAME(idx, config_ctrl),        \
			.regs_slpctrl = (void *)DT_INST_REG_ADDR_BY_NAME(idx, sleep_ctrl),         \
			.parent_clk = CLOCK_CONTROL_WCH_SOURCE(idx),                               \
			.hse_current = DT_INST_ENUM_IDX(idx, hse_bias_current_percent),            \
			.hse_capacity = DT_INST_ENUM_IDX(idx, hse_load_capacitance_pf),            \
			.divider = DT_INST_PROP(idx, clock_divider),                               \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(idx, clock_control_wch_ch570_init, NULL, NULL,                       \
			      &clock_control_wch_ch570_##idx##_config, PRE_KERNEL_1,               \
			      CONFIG_CLOCK_CONTROL_INIT_PRIORITY, &clock_control_wch_ch570_api);

/* There is only ever one */
CLOCK_CONTROL_WCH_INIT(0)
