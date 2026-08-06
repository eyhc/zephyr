/*
 * SPDX-FileCopyrightText: 2026 SMILE (smile.eu)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/comparator.h>
#include <zephyr/drivers/comparator/comparator_wch_ch5xx.h>

#define SLEEP_TIME_MS 1000

static const struct device *cmp = DEVICE_DT_GET(DT_ALIAS(adc_cmp));

int main(void)
{
	if (!device_is_ready(cmp)) {
		printk("Error: comparator device %s is not ready\n", cmp->name);
		return 0;
	}

	printk("Starting measurement:\n");

	while (1) {
		uint8_t value = 0;

		/* 16 comparator reference levels : 50 mV -> 100 mV -> 150 mV -> ... -> 800 mV */
		/* Output value:
		 *   voltage < 100 mV           -> 0
		 *   100 mV <= voltage < 150 mV -> 1
		 *   150 mV <= voltage < 200 mV -> 2
		 *   ...
		 *   750 mV <= voltage < 800 mV -> 14
		 *   voltage >= 800 mV          -> 15
		 *
		 *
		 * The output value is computed using a binary search on the comparator reference
		 * levels.
		 *
		 * Note: There is no 0 mV reference level, so the first output threshold is 100 mV
		 *       rather than 50 mV.
		 */
		for (int i = 3; i >= 0; i--) {
			comparator_wch_set_nref_level(cmp, (enum comp_nref_level)(value | BIT(i)));
			if (comparator_get_output(cmp)) {
				value |= BIT(i);
			}
		}

		/* compute voltage range in mV */
		uint16_t voltage_mv_lower = 50 * (value + 1);
		uint16_t voltage_mv_upper = 50 * (value + 2);

		printk("value = %u -> ", value);
		if (value == 0) {
			printk("voltage is below %d mV\n", voltage_mv_upper);
		} else if (value == 15) {
			printk("voltage is above %d mV\n", voltage_mv_lower);
		} else {
			printk("voltage is between %d mV and %d mV\n", voltage_mv_lower,
			       voltage_mv_upper);
		}
		k_msleep(SLEEP_TIME_MS);
	}
}
