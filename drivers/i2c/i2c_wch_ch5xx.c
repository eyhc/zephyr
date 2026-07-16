/*
 * Copyright (c) 2025 Andrei-Edward Popa
 * Copyright (c) 2026 Smile (smile.eu)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT wch_ch5xx_i2c

#define LOG_LEVEL CONFIG_I2C_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(i2c_ch5xx);

#include <zephyr/drivers/clock_control.h>
#include <zephyr/dt-bindings/i2c/i2c.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/device.h>

#include "i2c-priv.h"

#include <hal_ch32fun.h>

typedef void (*irq_config_func_t)(const struct device *dev);

struct i2c_ch5xx_config {
	const struct pinctrl_dev_config *pcfg;
	irq_config_func_t irq_config_func;
	const struct device *clk_dev;
	I2C_TypeDef *regs;
	uint32_t bitrate;
	uint8_t clk_id;
};

struct i2c_ch5xx_data {
	struct k_sem xfer_done;
	struct {
		struct i2c_msg *msg;
		uint32_t idx;
		union {
			struct {
				uint16_t addr: 10;
				uint16_t err: 6;
			};
			uint16_t: 16;
		};
	} current;
};

static void i2c_ch5xx_handle_start_bit(const struct device *dev)
{
	const struct i2c_ch5xx_config *config = dev->config;
	struct i2c_ch5xx_data *data = dev->data;
	I2C_TypeDef *regs = config->regs;

	if ((data->current.msg->flags & I2C_MSG_RW_MASK) == I2C_MSG_WRITE) {
		regs->DATAR = ((data->current.addr << 1) & 0xFF);
	} else {
		regs->DATAR = ((data->current.addr << 1) & 0xFF) | 1;
		if (data->current.msg->len == 2U) {
			regs->CTRL1 |= RB_I2C_POS;
		}
	}
}

static void i2c_ch5xx_handle_addr(const struct device *dev)
{
	const struct i2c_ch5xx_config *config = dev->config;
	struct i2c_ch5xx_data *data = dev->data;
	I2C_TypeDef *regs = config->regs;

	if ((data->current.msg->flags & I2C_MSG_RW_MASK) == I2C_MSG_READ) {
		if (data->current.msg->len <= 2U) {
			regs->CTRL1 &= ~RB_I2C_ACK;
			if (data->current.msg->len == 2U) {
				regs->CTRL1 |= RB_I2C_POS;
			}
		}
	}

	regs->STAR1;
	regs->STAR2;
}

static void i2c_ch5xx_handle_txe(const struct device *dev)
{
	const struct i2c_ch5xx_config *config = dev->config;
	struct i2c_ch5xx_data *data = dev->data;
	I2C_TypeDef *regs = config->regs;

	if (data->current.idx < data->current.msg->len) {
		regs->DATAR = data->current.msg->buf[data->current.idx++];
		if (data->current.idx == data->current.msg->len) {
			regs->CTRL2 &= ~RB_I2C_ITBUFEN;
		}
	} else {
		if (data->current.msg->flags & I2C_MSG_STOP) {
			regs->CTRL1 |= RB_I2C_STOP;
		}

		if (regs->STAR1 & RB_I2C_BTF) {
			regs->DATAR;
		}

		k_sem_give(&data->xfer_done);
	}
}

static void i2c_ch5xx_handle_rxne(const struct device *dev)
{
	const struct i2c_ch5xx_config *config = dev->config;
	struct i2c_ch5xx_data *data = dev->data;
	I2C_TypeDef *regs = config->regs;

	if (data->current.idx < data->current.msg->len) {
		switch (data->current.msg->len - data->current.idx) {
		case 1:
			if (data->current.msg->flags & I2C_MSG_STOP) {
				regs->CTRL1 |= RB_I2C_STOP;
			}
			regs->CTRL2 &= ~RB_I2C_ITBUFEN;
			data->current.msg->buf[data->current.idx++] = regs->DATAR;
			k_sem_give(&data->xfer_done);
			break;
		case 2:
			regs->CTRL1 &= ~RB_I2C_ACK;
			regs->CTRL1 |= RB_I2C_POS;
			__fallthrough;
		default:
			data->current.msg->buf[data->current.idx++] = regs->DATAR;
		}
	} else {
		if (data->current.msg->flags & I2C_MSG_STOP) {
			regs->CTRL1 |= RB_I2C_STOP;
		}
		k_sem_give(&data->xfer_done);
	}
}

static void i2c_ch5xx_isr(const struct device *dev)
{
	const struct i2c_ch5xx_config *config = dev->config;
	struct i2c_ch5xx_data *data = dev->data;
	I2C_TypeDef *regs = config->regs;
	uint16_t status = regs->STAR1;
	bool write;

	/* error isr */
	if (status & (RB_I2C_AF | RB_I2C_ARLO | RB_I2C_BERR)) {
		if (status & RB_I2C_AF) {
			regs->CTRL1 |= RB_I2C_STOP;
		}

		data->current.err |= ((status >> 8) & 0x7);

		regs->STAR1 &= ~(RB_I2C_AF | RB_I2C_ARLO | RB_I2C_BERR);

		k_sem_give(&data->xfer_done);
	}

	/* event isr */
	write = ((data->current.msg->flags & I2C_MSG_RW_MASK) == I2C_MSG_WRITE);

	if (status & RB_I2C_SB) {
		i2c_ch5xx_handle_start_bit(dev);
	} else if (status & RB_I2C_ADDR) {
		i2c_ch5xx_handle_addr(dev);
	} else if ((status & (RB_I2C_TxE | RB_I2C_BTF)) && write) {
		i2c_ch5xx_handle_txe(dev);
	} else if ((status & (RB_I2C_RxNE | RB_I2C_BTF)) && (!write)) {
		i2c_ch5xx_handle_rxne(dev);
	}
}

static void i2c_ch5xx_msg_init(const struct device *dev, struct i2c_msg *msg, uint16_t addr,
			       bool first_msg)
{
	const struct i2c_ch5xx_config *config = dev->config;
	struct i2c_ch5xx_data *data = dev->data;
	I2C_TypeDef *regs = config->regs;

	k_sem_reset(&data->xfer_done);

	data->current.msg = msg;
	data->current.idx = 0U;
	data->current.err = 0U;
	data->current.addr = addr;

	regs->CTRL1 |= RB_I2C_PE;
	regs->CTRL1 |= RB_I2C_ACK;

	if (first_msg || (msg->flags & I2C_MSG_RESTART)) {
		if (regs->CTRL1 & RB_I2C_STOP) {
			regs->CTRL1 &= ~RB_I2C_STOP;
		}
		regs->CTRL1 |= RB_I2C_START;
	}
}

static int32_t i2c_ch5xx_msg_end(const struct device *dev)
{
	struct i2c_ch5xx_data *data = dev->data;

	if (!data->current.err) {
		return 0;
	}

	if (data->current.err & (RB_I2C_ARLO >> 8)) {
		LOG_DBG("ARLO");
	}

	if (data->current.err & (RB_I2C_AF >> 8)) {
		LOG_DBG("NACK");
	}

	if (data->current.err & (RB_I2C_BERR >> 8)) {
		LOG_DBG("ERR");
	}

	data->current.err = 0;

	return -EIO;
}

static void i2c_ch5xx_config_interrupts(I2C_TypeDef *regs, bool enable)
{
	if (enable) {
		regs->CTRL2 |= (RB_I2C_ITERREN | RB_I2C_ITEVTEN | RB_I2C_ITBUFEN);
	} else {
		regs->CTRL2 &= ~(RB_I2C_ITERREN | RB_I2C_ITEVTEN | RB_I2C_ITBUFEN);
	}
}

static int32_t i2c_ch5xx_begin_transfer(const struct device *dev, struct i2c_msg *msg,
					uint16_t addr, bool first_msg)
{
	const struct i2c_ch5xx_config *config = dev->config;
	struct i2c_ch5xx_data *data = dev->data;
	I2C_TypeDef *regs = config->regs;

	i2c_ch5xx_config_interrupts(regs, true);
	i2c_ch5xx_msg_init(dev, msg, addr, first_msg);

	if (k_sem_take(&data->xfer_done, K_MSEC(CONFIG_I2C_WCH_CH5XX_XFER_TIMEOUT_MS)) < 0) {
		return -ETIMEDOUT;
	}

	return i2c_ch5xx_msg_end(dev);
}

static void i2c_ch5xx_finish_transfer(const struct device *dev)
{
	const struct i2c_ch5xx_config *config = dev->config;
	I2C_TypeDef *regs = config->regs;

	i2c_ch5xx_config_interrupts(regs, false);

	while (regs->STAR2 & RB_I2C_BUSY) {
	}

	regs->CTRL1 &= ~RB_I2C_PE;
}

static int i2c_ch5xx_configure_timing(I2C_TypeDef *regs, uint32_t clock_rate, uint32_t speed)
{
	uint16_t freq_range = (uint16_t)(clock_rate / 1000000);
	uint16_t clock_config;

#ifdef I2C_RTR_TRISE
	uint16_t trise;

	switch (speed) {
	case I2C_SPEED_STANDARD:
		trise = freq_range + 1;
		break;
	case I2C_SPEED_FAST:
		trise = (freq_range * 3 / 10) + 1;
		break;
	default:
		return -EINVAL;
	}

	regs->RTR = trise;
#endif

	switch (speed) {
	case I2C_SPEED_STANDARD:
		clock_config = MAX((uint16_t)(clock_rate / (100000 * 2)), 4);
		break;
	case I2C_SPEED_FAST:
		clock_config = MAX((uint16_t)(clock_rate / (400000 * 3)), 1) | RB_I2C_F_S;
		break;
	default:
		return -EINVAL;
	}

	regs->CKCFGR = clock_config;
	regs->CTRL2 = (regs->CTRL2 & ~RB_I2C_FREQ) | freq_range;

	return 0;
}

static int i2c_ch5xx_configure(const struct device *dev, uint32_t dev_config)
{
	const struct i2c_ch5xx_config *config = dev->config;
	I2C_TypeDef *regs = config->regs;
	clock_control_subsys_t clk_sys;
	uint32_t clock_rate;
	int err;

	if (!(dev_config & I2C_MODE_CONTROLLER)) {
		return -ENOTSUP;
	}

	if (dev_config & I2C_ADDR_10_BITS) {
		return -ENOTSUP;
	}

	clk_sys = (clock_control_subsys_t)(uintptr_t)config->clk_id;

	err = clock_control_get_rate(config->clk_dev, clk_sys, &clock_rate);
	if (err != 0) {
		return err;
	}

	regs->CTRL1 &= ~RB_I2C_PE;

	err = i2c_ch5xx_configure_timing(regs, clock_rate, I2C_SPEED_GET(dev_config));
	if (err != 0) {
		return err;
	}

	return 0;
}

static int i2c_ch5xx_transfer(const struct device *dev, struct i2c_msg *msg, uint8_t num_msgs,
			      uint16_t addr)
{
	int ret = 0;

	for (uint8_t i = 1; i < num_msgs; i++) {
		if ((msg[i - 1].flags & I2C_MSG_RW_MASK) != (msg[i].flags & I2C_MSG_RW_MASK)) {
			if (!(msg[i].flags & I2C_MSG_RESTART)) {
				return -EINVAL;
			}
		}

		if (msg[i - 1].flags & I2C_MSG_STOP) {
			return -EINVAL;
		}
	}

	for (uint8_t i = 0; i < num_msgs && ret == 0; i++) {
		ret = i2c_ch5xx_begin_transfer(dev, &msg[i], addr, i == 0);
	}

	i2c_ch5xx_finish_transfer(dev);

	return ret;
}

static int i2c_ch5xx_init(const struct device *dev)
{
	const struct i2c_ch5xx_config *config = dev->config;
	struct i2c_ch5xx_data *data = dev->data;
	clock_control_subsys_t clk_sys;
	int err;

	k_sem_init(&data->xfer_done, 0, 1);

	clk_sys = (clock_control_subsys_t)(uintptr_t)config->clk_id;

	err = clock_control_on(config->clk_dev, clk_sys);
	if (err < 0) {
		return err;
	}

	err = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
	if (err < 0) {
		return err;
	}

	err = i2c_ch5xx_configure(dev, I2C_MODE_CONTROLLER | i2c_map_dt_bitrate(config->bitrate));
	if (err < 0) {
		return err;
	}

	config->irq_config_func(dev);

	return 0;
}

static DEVICE_API(i2c, i2c_ch5xx_api) = {
	.configure = i2c_ch5xx_configure,
	.transfer = i2c_ch5xx_transfer,
};

#define I2C_CH5XX_INIT(inst)                                                                       \
	PINCTRL_DT_INST_DEFINE(inst);                                                              \
                                                                                                   \
	static void i2c_ch5xx_config_func_##inst(const struct device *dev);                        \
                                                                                                   \
	static struct i2c_ch5xx_config i2c_ch5xx_cfg_##inst = {                                    \
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(inst),                                      \
		.irq_config_func = i2c_ch5xx_config_func_##inst,                                   \
		.clk_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(inst)),                               \
		.regs = (I2C_TypeDef *)DT_INST_REG_ADDR(inst),                                     \
		.bitrate = DT_INST_PROP(inst, clock_frequency),                                    \
		.clk_id = DT_INST_CLOCKS_CELL(inst, id)};                                          \
                                                                                                   \
	static struct i2c_ch5xx_data i2c_ch5xx_data_##inst;                                        \
                                                                                                   \
	I2C_DEVICE_DT_INST_DEFINE(inst, i2c_ch5xx_init, NULL, &i2c_ch5xx_data_##inst,              \
				  &i2c_ch5xx_cfg_##inst, PRE_KERNEL_1, CONFIG_I2C_INIT_PRIORITY,   \
				  &i2c_ch5xx_api);                                                 \
                                                                                                   \
	static void i2c_ch5xx_config_func_##inst(const struct device *dev)                         \
	{                                                                                          \
		ARG_UNUSED(dev);                                                                   \
                                                                                                   \
		IRQ_CONNECT(DT_INST_IRQN(inst), DT_INST_IRQ(inst, priority), i2c_ch5xx_isr,        \
			    DEVICE_DT_INST_GET(inst), 0);                                          \
		irq_enable(DT_INST_IRQN(inst));                                                    \
	}

DT_INST_FOREACH_STATUS_OKAY(I2C_CH5XX_INIT)
