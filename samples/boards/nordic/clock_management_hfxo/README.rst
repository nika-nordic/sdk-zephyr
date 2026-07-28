.. zephyr:code-sample:: clock_management_hfxo
   :name: Clock-state concept demo (nRF54LM20B)

   Model the nRF54LM20B clock tree in devicetree, bind peripherals (consumers)
   to clock producers with rank-arbitrated clock-state nodes, and drive them at
   runtime through a reference-counted request/release interface.

Overview
********

This sample demonstrates a unified, devicetree-based abstraction for associating
*clock consumers* with *clock producers*, using the ``clock-state`` concept
(from the upstream clock-management RFC).

The whole nRF54LM20B clock tree is modeled in the sample's board overlay:

.. code-block:: none

   Sources        Muxes / derived        Outputs (pclk*)      Consumers
   -------        ---------------        ---------------      ---------
   HFINT ---+
            +--> HFCLK (mux) --+--> PCLK32M --> RADIO, PDM, TDM
   HFXTAL --+                  +--> PCLK16M --> UARTE
      |                        +--> PCLK1M
      |                        +--> HCLKCORE -> CPU
      +--------> XO24M --------+--> PCLK24M --> USBHS, PDM
   LFRC ----+
            +--> LFCLK (mux) --+--> PCLK32Ki -> GRTC
   LFXTAL --+   (SYNT from HFCLK)

Each peripheral clock output exposes a ``clock-output`` signal. Selectable
configurations are described as ``clock-state`` children of that output, each
carrying a ``rank`` used to arbitrate between them at runtime (lower is
preferred). Every consumer node binds to those states, exactly like a pinctrl
consumer binds to pinctrl states.

The application:

#. Fetches the consumer -> producer bindings from devicetree **at build time**
   into per-consumer tables of :c:struct:`clock_state`.
#. Demonstrates **rank arbitration**: for the ``pdm`` consumer (three candidate
   states) it selects the lowest-rank one.
#. Demonstrates **reference-counted sharing**: the ``uarte`` and ``radio``
   consumers both request HFXO via ``nrf_clock_control_request``; HFXO stays on
   until the last consumer releases it. This is why request/release is used
   instead of plain ``clock_control_on``/``off``.

The three producers that can be driven at runtime (HFXO, XO24M, LFCLK) map to
the existing split ``clock_control`` devices ``&xo``, ``&xo24m`` and ``&lfclk``.
The rest of the tree is descriptive. States with no ``clocks`` phandle model a
default configuration served by an always-on internal source (HFINT/LFRC) and
need no runtime action.

Because the upstream clock-management framework is not yet available in this
tree, the "abstracted interface" is a small sample-local helper
(``src/clock_state_demo.h``), a stand-in for a future ``clock_management_*`` API.
Everything is kept self-contained under this sample directory.

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

   Clock-state concept demo (nRF54LM20B, request/release)
   nRF54LM20B clock tree - consumer bindings from devicetree:
     uarte: 2 state(s)
       - xtal           producer=xo               rank= 0  16000000 Hz
       - default        producer=(none/internal)  rank=10  16000000 Hz
     ...
   == Rank arbitration (consumer 'pdm') ==
     best of 3 candidates -> 'pclk24m-xtal' (rank 0) via xo24m
     applied 'pclk24m-xtal'
   == Shared HFXO via request/release ==
       HFXO is off  (initial)
     uarte requests HFXO ...
       HFXO is on  (after uarte request)
     radio requests HFXO ...
       HFXO is on  (after radio request)
     uarte releases HFXO (radio still holds it) ...
       HFXO is on  (after uarte release)
     radio releases HFXO (last consumer) ...
       HFXO is off  (after radio release)
