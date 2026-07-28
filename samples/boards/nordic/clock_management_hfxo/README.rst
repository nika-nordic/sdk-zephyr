.. zephyr:code-sample:: clock_management_hfxo
   :name: Clock-state HFXO concept demo (nRF54LM20)

   Bind a clock consumer to a clock producer (HFXO) in devicetree and drive it
   at runtime through an abstracted, rank-arbitrated interface.

Overview
********

This sample demonstrates a unified, devicetree-based abstraction for associating
a *clock consumer* with a *clock producer*, using the ``clock-state`` concept
(from the upstream clock-management RFC).

The SoC clock producer (HFXO) exposes a ``clock-output`` signal. Selectable
configurations are described as ``clock-state`` child nodes of that output, each
carrying a ``rank`` used to arbitrate between them at runtime (lower is
preferred). A demo consumer node (``clockdemo``) binds to these states exactly
like a pinctrl consumer binds to pinctrl states.

The application:

#. Fetches the consumer -> producer binding from devicetree **at build time**
   into a table of :c:struct:`clock_state` entries.
#. Selects the best (lowest-rank) state at runtime.
#. Applies the selected state by **really starting HFXO** through the classic
   ``clock_control`` API, then switches to the ``sleep`` state to stop it.

Because the upstream clock-management framework is not yet available in this
tree, the "abstracted interface" is a small sample-local helper
(``src/clock_state_demo.h``). It is a stand-in for a future
``clock_management_*`` API and keeps everything self-contained under this
sample directory.

Requirements
************

This sample is written for the nRF54LM20 DK application core.

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/nordic/clock_management_hfxo
   :board: nrf54lm20dk/nrf54lm20a/cpuapp
   :goals: build flash
   :compact:

Sample Output
*************

.. code-block:: console

   Clock-state HFXO concept demo
   Consumer 'clock-consumer-demo' is bound to 2 clock state(s):
     [0] name=active producer=xo       rank=0 frequency=32000000 Hz
     [1] name=sleep  producer=xo       rank=10 frequency=32000000 Hz
   Requesting best state with rank <= 4294967295 ...
     selected 'active' (rank 0, 32000000 Hz)
     producer 'xo' is now on
   Switching to 'sleep' state, releasing HFXO ...
     producer 'xo' is now off
