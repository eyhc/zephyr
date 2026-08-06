.. zephyr:code-sample:: ch57x_comparator_based_adc
   :name: Comparator based ADC sample
   :relevant-api: comparator_interface comparator_wch_ch5xx_interface

   Use WCH CH57x comparator to implement a simple 4-bit ADC based on a binary search algorithm.

Overview
********

A driver specific sample that demonstrates how to use the internal negative reference voltage of
WCH CH57x comparator to implement a simple 4-bit ADC based on a binary search algorithm.

Requirements
************

Software requirements
=====================

On boards with CH570 or CH572 MCUs, the sample requires a node with compatible property set to
``wch,ch5xx-comparator`` in the devicetree. This node must have an alias named ``adc-cmp``.
On the devicetree the comparator node ``negative-mux-input`` property must be set to ``"VREF"``.

Hardware requirements
=====================

The simplest way to drive the comparator input is to use a potentiometer and a resistor divider.
The potentiometer wiper is connected to the comparator positive input pin. The reference voltage
level can be set from 50 mV to 800 mV in 50 mV steps. Interesting results can be obtained if the
comparator positive input is between 0 and 850 mV. In this case, the resistor divider value is:

.. math::

   R = \frac{V_{\text{cc}} - V_{\text{in}}}{V_{\text{in}}} \cdot R_{\text{pot}}

where :math:`V_{\text{cc}}` is the supply voltage, :math:`V_{\text{in}}` is the maximum positive
input voltage (850 mV is a good choice), :math:`R_{\text{pot}}` is the potentiometer resistance and
:math:`R` is the divider resistor.

The divider resistor is connected to the (divided) supply voltage and one end of the potentiometer.
The potentiometer wiper is connected to the comparator positive input pin (PA3 or PA7 depending on
the overlay) and the other potentiometer end is connected to ground.

For example, if the supply voltage is 3.3 V, the potentiometer is 10 kΩ and the maximum input
voltage is 850 mV, the divider resistor value is approximately 29 kΩ (the nearest standard value).

Building and Running
********************

This sample can be built for boards with WCH CH570 or CH572 MCUs, in this example we will build it
for the nanoCH57x board:

.. zephyr-app-commands::
   :zephyr-app: samples/boards/wch/ch57x_comparator_based_adc
   :board: nano_ch57x/ch570
   :goals: build
   :compact:

After startup, the program retrieves the device referenced by the ``adc-cmp`` alias and configures
the comparator. During each iteration of the main loop, the program performs a binary search to find
the input voltage level and prints both digital value and voltage range to the console.

Note: The first threshold is 100 mV because the comparator reference voltage starts at 50 mV rather
than 0 mV.
