/*
 * SPDX-FileCopyrightText: 2026 SMILE (smile.eu)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT wch_ch5xx_pwm

#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/dt-bindings/pwm/pwm.h>

#include <hal_ch32fun.h>

struct pwm_wch_config {
	PWM_TypeDef *regs;
	const struct device *clock_dev;
	uint8_t clock_id;
	uint8_t counter_width_cfg;
	uint8_t counter_width_val;
	uint8_t minus_one_period;
	uint8_t first_channel_idx;
	uint16_t period_cycles_grp0;
	uint16_t period_cycles_grp1;
	uint16_t clock_divider;
	const struct pinctrl_dev_config *pin_cfg;
};

static int pwm_wch_set_cycles(const struct device *dev, uint32_t channel, uint32_t period_cycles,
			      uint32_t pulse_cycles, pwm_flags_t flags)
{
	const struct pwm_wch_config *config = dev->config;
	PWM_TypeDef *regs = config->regs;

	/* 1. check period_cycles validity */
	if ((config->counter_width_val == 6 &&
	     period_cycles != (config->minus_one_period ? 63 : 64)) ||
	    (config->counter_width_val == 7 &&
	     period_cycles != (config->minus_one_period ? 127 : 128)) ||
	    (config->counter_width_val == 8 &&
	     period_cycles != (config->minus_one_period ? 255 : 256)) ||
	    (config->counter_width_val == 16 &&
	     period_cycles !=
		     (channel <= 3 ? config->period_cycles_grp0 : config->period_cycles_grp1))) {
		{
			return -ENOTSUP;
		}
	}

	/* 2. check pulse_cycles validity */
	if (pulse_cycles > period_cycles) {
		return -EDEADLK;
	}
	if (period_cycles < (uint16_t)pulse_cycles) {
		return -E2BIG;
	}
	if (period_cycles < (uint16_t)(period_cycles - pulse_cycles)) {
		return -EBADFD;
	}

	/* 3. Config polarity */
	if ((flags & PWM_POLARITY_INVERTED) != 0) {
		regs->OUT_POLAR |= BIT(channel - config->first_channel_idx);
	} else {
		regs->OUT_POLAR &= ~BIT(channel - config->first_channel_idx);
	}

	/* 4. Enable the channel output */
	if (period_cycles != 0) {
		regs->OUT_EN |= BIT(channel - config->first_channel_idx);
	} else {
		regs->OUT_POLAR &= ~BIT(channel - config->first_channel_idx);
	}

	/* 5. Set pulse_cycles */
	if (config->counter_width_val == 16) {
		switch (channel) {
		case 1:
			regs->PWM1_R16_DATA = pulse_cycles;
			break;
		case 2:
			regs->PWM2_R16_DATA = pulse_cycles;
			break;
		case 3:
			regs->PWM3_R16_DATA = pulse_cycles;
			break;
		case 4:
			regs->PWM4_R16_DATA = pulse_cycles;
			break;
		case 5:
			regs->PWM5_R16_DATA = pulse_cycles;
			break;
		default:
			return -EINVAL;
		}
	} else {
		switch (channel) {
		case 1:
			regs->PWM1_R8_DATA = pulse_cycles;
			break;
		case 2:
			regs->PWM2_R8_DATA = pulse_cycles;
			break;
		case 3:
			regs->PWM3_R8_DATA = pulse_cycles;
			break;
		case 4:
			regs->PWM4_R8_DATA = pulse_cycles;
			break;
		case 5:
			regs->PWM5_R8_DATA = pulse_cycles;
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

static int pwm_wch_get_cycles_per_sec(const struct device *dev, uint32_t channel, uint64_t *cycles)
{
	const struct pwm_wch_config *config = dev->config;
	clock_control_subsys_t clock_sys = (clock_control_subsys_t *)(uintptr_t)config->clock_id;
	uint32_t clock_rate;
	int err;

	err = clock_control_get_rate(config->clock_dev, clock_sys, &clock_rate);
	if (err != 0) {
		return err;
	}

	*cycles = clock_rate / config->clock_divider;

	return 0;
}

static DEVICE_API(pwm, pwm_wch_driver_api) = {
	.set_cycles = pwm_wch_set_cycles,
	.get_cycles_per_sec = pwm_wch_get_cycles_per_sec,
};

static int pwm_wch_init(const struct device *dev)
{
	const struct pwm_wch_config *config = dev->config;
	PWM_TypeDef *regs = config->regs;
	int err;

	/* 1. Enable clock */
	clock_control_on(config->clock_dev, (clock_control_subsys_t *)(uintptr_t)config->clock_id);

	/* 2. Config counter width, counter max and prescaler */
	regs->CONFIG &= ~0b111;
	regs->CONFIG |= (config->counter_width_cfg & 0x3) << 1;

	if (config->minus_one_period) {
		regs->CONFIG |= 0x1;
	}
	regs->CYC_VALUE = config->period_cycles_grp0;
	regs->CYC1_VALUE = config->period_cycles_grp1;

	regs->CLOCK_DIV = config->clock_divider;

	/* 3. Apply pinctrl */
	err = pinctrl_apply_state(config->pin_cfg, PINCTRL_STATE_DEFAULT);
	if (err != 0) {
		return err;
	}

	return 0;
}

#define PWM_WCH_INIT(idx)                                                                          \
	PINCTRL_DT_INST_DEFINE(idx);                                                               \
                                                                                                   \
	static const struct pwm_wch_config pwm_wch_##idx##_config = {                              \
		.regs = (PWM_TypeDef *)DT_INST_REG_ADDR(idx),                                      \
		.clock_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(idx)),                              \
		.clock_id = DT_INST_CLOCKS_CELL(idx, id),                                          \
		.pin_cfg = PINCTRL_DT_INST_DEV_CONFIG_GET(idx),                                    \
		.counter_width_cfg = DT_INST_ENUM_IDX(idx, counter_width),                         \
		.counter_width_val = DT_INST_PROP(idx, counter_width),                             \
		.minus_one_period = DT_INST_PROP(idx, minus_one_period),                           \
		.first_channel_idx = DT_INST_PROP(idx, first_channel_number),                      \
		.period_cycles_grp0 = DT_INST_PROP(idx, period_cycles_group0),                     \
		.period_cycles_grp1 = DT_INST_PROP(idx, period_cycles_group1),                     \
		.clock_divider = DT_INST_PROP(idx, prescaler),                                     \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(idx, &pwm_wch_init, NULL, NULL, &pwm_wch_##idx##_config,             \
			      POST_KERNEL, CONFIG_PWM_INIT_PRIORITY, &pwm_wch_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PWM_WCH_INIT)
