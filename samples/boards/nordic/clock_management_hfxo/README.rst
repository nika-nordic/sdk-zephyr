.. zephyr:code-sample:: clock_management_hfxo
   :name: Clock-state concept demo (nRF54LM20B)

   Model the nRF54LM20B clock tree in devicetree, bind peripherals to it with
   rank-arbitrated clock-state nodes, and resolve the whole tree in a
   clock-management layer that arbitrates shared nodes and drives the producers.

Overview
********

This sample demonstrates a unified, devicetree-based abstraction for associating
*clock consumers* with *clock producers*, using the ``clock-state`` concept
(from the upstream clock-management RFC).

The nRF54LM20B clock tree is modeled in the sample's board overlay:

.. code-block:: none

   Sources        Muxes / gate            Outputs (pclk*)      Consumers
   -------        ------------            ---------------      ---------
   HFINT ---+
            +--> HFCLK (mux) --+--> PCLK32M --> RADIO, PDM, TDM
   HFXTAL --+                  +--> PCLK16M --> UARTE
      |                        +--> PCLK1M
      |                        +--> HCLKCORE -> CPU
      +--------> XO24M (gate)--+--> PCLK24M --> USBHS, PDM
   LFRC ----+
            +--> LFCLK (mux) --+--> PCLK32Ki -> GRTC
   LFXTAL --+   (SYNT from HFCLK)

Each ``clock-state`` encodes a *selection in the tree* - a mux input or a gate
setting, via ``clocks = <&node selector>`` - plus a ``rank``. Consumers bind to
these states exactly like a pinctrl consumer binds to pinctrl states.

Key idea: arbitration lives in a layer above the producer drivers
****************************************************************************

When a consumer applies a state, the sample-local clock-management layer
(``src/clock_mgmt.c``) resolves the **whole path** from the selected node up to
its source, and arbitrates shared nodes:

* **Shared-node rank arbitration.** Several consumers select the same HFCLK mux.
  The best-ranked request wins for *everyone* downstream. So when ``radio``
  (rank 0, HFXO) runs alongside ``uarte`` (rank 10, HFINT), HFCLK resolves to
  HFXO and ``uarte`` is upgraded to the crystal even though it asked for HFINT.

* **Opportunistic HFXO upgrade.** ``XO24M`` (needed by ``usbhs``) is derived
  from HFXTAL, so enabling it forces HFXO on. HFCLK then opportunistically
  sources from HFXO - HFINT consumers ride the crystal "for free" while it is
  already running.

Only the three leaf producers that can be driven at runtime (HFXO, XO24M, LFCLK)
are actually requested/released - through the reference-counted
``nrf_clock_control`` API, mapped to ``&xo``, ``&xo24m`` and ``&lfclk``. The mux
source selection is inferred (on LM20B, "HFCLK = HFXTAL" effectively means "HFXO
is requested"), so the layer reports the resolved configuration of each node.

Because the upstream clock-management framework is not yet available in this
tree, this layer is a self-contained stand-in for a future ``clock_management_*``
API. Everything is kept under this sample directory.

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

   == 1. uarte applies 'hfint' (rank 10) ==
     [uarte hfint]
       tree: HFXO=off XO24M=off LFCLK=off | HFCLK src=HFINT | LFCLK src=LFRC
       uarte  wants hfint       -> effective HFINT     (16000000 Hz)
   == 2. radio applies 'xtal' (rank 0) - shared HFCLK arbitration ==
     [uarte hfint + radio xtal]
       tree: HFXO=on XO24M=off LFCLK=off | HFCLK src=HFXO | LFCLK src=LFRC
       uarte  wants hfint       -> effective HFXO      (16000000 Hz)
       radio  wants xtal        -> effective HFXO      (32000000 Hz)
   == 4. usbhs applies '24m' - XO24M forces HFXO, HFCLK upgrades ==
     [uarte hfint + usbhs 24m]
       tree: HFXO=on XO24M=on LFCLK=off | HFCLK src=HFXO | LFCLK src=LFRC
       uarte  wants hfint       -> effective HFXO      (16000000 Hz)
       usbhs  wants 24m         -> effective HFXO(24M) (24000000 Hz)
