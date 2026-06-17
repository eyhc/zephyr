.. zephyr:code-sample:: comp_interrupt
   :name: Comparator interrupt
   :relevant-api: comparator_interface

   Handle Comparator inputs with interrupts.

Overview
********

This sample demonstrates how to use the :ref:`comparator_api` API.
The sample prints a message to the console each time a rising edge is detected on the input pin.

Requirements
************

The simplest way to run this sample is to connect the negative input of the comparator to a voltage
reference between 0 and VCC (eg. with a voltage divider), and connect the positive input to a push
button. When the button is pressed, the comparator will detect a rising edge and trigger an
interrupt.

The comparator label used in this sample is ``sample-cmp``. You must ensure that your board has a
comparator with this label, or change the label in the source code to match your board's comparator.

The sample additionally supports an optional ``led0`` devicetree alias. This is the same alias used
by the :zephyr:code-sample:`blinky` sample. If this is provided, the LED will be turned on when
the comparator output is high, and turned off when the output is low.

Devicetree details
==================

This section provides more details on devicetree configuration.

Here is a minimal devicetree fragment which supports this sample. This only
includes a ``sample-cmp`` alias; the optional ``led0`` alias is left out for
simplicity.

.. code-block:: devicetree

   / {
           aliases {
                   sample-cmp = &comparator0;
           };

           comparator0: comparator@0 {
                   status = "okay";
                   /* ... */
           };
   };

The above situation is for the common case where:

- ``comparator0`` is an example node label referring to a comparator device node
  with status "okay"
- ``sample-cmp`` is an alias referring to that comparator device node

Building and Running
********************

This sample can be built for multiple boards, in this example we will build it
for the nanoch57x board:

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/comparator
   :board: nanoch57x/ch570
   :goals: build flash
   :compact:

After startup, the program looks up a predefined comparator device and configures it to generate
interrupts on the rising edge of its output signal. During each iteration of the main loop, the
comparator output state is monitored and, if an LED is defined, its state is updated accordingly.
When a rising-edge interrupt occurs, the interrupt handler prints information about the event and
its timestamp.
