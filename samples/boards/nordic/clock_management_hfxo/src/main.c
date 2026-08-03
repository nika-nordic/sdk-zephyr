/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

#include "clock_mgmt.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Consumers described in the board overlay (nRF54LM20B clock tree). */
CLK_CONSUMER_DEFINE(uarte, DT_NODELABEL(uarte_consumer));
CLK_CONSUMER_DEFINE(radio, DT_NODELABEL(radio_consumer));
CLK_CONSUMER_DEFINE(pdm, DT_NODELABEL(pdm_consumer));
CLK_CONSUMER_DEFINE(usbhs, DT_NODELABEL(usbhs_consumer));
CLK_CONSUMER_DEFINE(grtc, DT_NODELABEL(grtc_consumer));

static void step(const char *what)
{
	LOG_INF("%s", what);
	k_sleep(K_MSEC(800));
}

int main(void)
{
	LOG_INF("Clock-state concept demo (nRF54LM20B, tree resolution + rank)");

	clkmgmt_register(&uarte);
	clkmgmt_register(&radio);
	clkmgmt_register(&pdm);
	clkmgmt_register(&usbhs);
	clkmgmt_register(&grtc);

	while (1) {
		/*
		 * 1. UARTE tolerates HFINT and nobody needs the crystal yet, so
		 *    HFCLK resolves to HFINT and HFXO stays off.
		 */
		step("== 1. uarte applies 'hfint' (rank 10) ==");
		clkmgmt_apply(&uarte, "hfint");
		clkmgmt_report("uarte hfint");

		/*
		 * 2. RADIO needs the crystal (rank 0). Arbitration on the shared
		 *    HFCLK node makes HFXO win - and UARTE, though it asked for
		 *    HFINT, is upgraded to HFXO too.
		 */
		step("== 2. radio applies 'xtal' (rank 0) - shared HFCLK arbitration ==");
		clkmgmt_apply(&radio, "xtal");
		clkmgmt_report("uarte hfint + radio xtal");

		step("== 3. radio releases - HFCLK falls back to HFINT ==");
		clkmgmt_release(&radio);
		clkmgmt_report("uarte hfint only");

		/*
		 * 4. USBHS needs XO24M, which requires HFXO. HFCLK opportunistically
		 *    upgrades to HFXO, so UARTE rides the crystal again "for free".
		 */
		step("== 4. usbhs applies '24m' - XO24M forces HFXO, HFCLK upgrades ==");
		clkmgmt_apply(&usbhs, "24m");
		clkmgmt_report("uarte hfint + usbhs 24m");

		/* 5. PDM arbitrates its own three candidate states -> best rank (24m). */
		step("== 5. pdm applies best-ranked state (rank arbitration) ==");
		clkmgmt_apply(&pdm, pdm.states[0].name);
		clkmgmt_report("+ pdm 24m");

		/* 6. GRTC brings up the LF domain independently. */
		step("== 6. grtc applies 'lfxo' - independent LF domain ==");
		clkmgmt_apply(&grtc, "lfxo");
		clkmgmt_report("+ grtc lfxo");

		/* 7. Tear everything down; HFXO/XO24M/LFCLK all drop. */
		step("== 7. release all ==");
		clkmgmt_release(&uarte);
		clkmgmt_release(&usbhs);
		clkmgmt_release(&pdm);
		clkmgmt_release(&grtc);
		clkmgmt_report("idle");

		k_sleep(K_SECONDS(3));
	}

	return 0;
}
