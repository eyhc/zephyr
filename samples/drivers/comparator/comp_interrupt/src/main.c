/*
 * SPDX-FileCopyrightText: 2026 SMILE (smile.eu)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/comparator.h>
#include <zephyr/drivers/comparator/comparator_wch_ch5xx.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

#define SLEEP_TIME_MS 50

static struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});
static const struct device *cmp_dev = DEVICE_DT_GET(DT_ALIAS(sample_cmp));

static void comp_callback(const struct device *dev, void *user_data)
{
	printk("Rising edge detected at %" PRIu32 "\n", k_cycle_get_32());
}

int main(void)
{
	int ret;

	if (!device_is_ready(cmp_dev)) {
		printk("Error: comparator device %s is not ready\n", cmp_dev->name);
		return 0;
	}

	if (led.port && !gpio_is_ready_dt(&led)) {
		printk("Error: LED device %s is not ready; ignoring it\n", led.port->name);
		led.port = NULL;
	}

	if (led.port) {
		ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT);
		if (ret != 0) {
			printk("Error %d: failed to configure LED device %s pin %d\n", ret,
			       led.port->name, led.pin);
			led.port = NULL;
		}
	}

	comparator_set_trigger(cmp_dev, COMPARATOR_TRIGGER_RISING_EDGE);
	comparator_set_trigger_callback(cmp_dev, comp_callback, NULL);

	printk("Starting comparator interrupt sample\n");
	printk("Change the voltage on the comparator input to toggle the LED state.\n");

	while (1) {
		int val = comparator_get_output(cmp_dev);

		gpio_pin_set_dt(&led, val);
		k_msleep(SLEEP_TIME_MS);
	}
}
