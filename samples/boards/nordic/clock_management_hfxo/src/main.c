/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/logging/log.h>

#include "clock_state_demo.h"

LOG_MODULE_REGISTER(clock_state_demo, LOG_LEVEL_INF);

/* Consumer nodes described in the board overlay (nRF54LM20B clock tree). */
#define UARTE_NODE DT_NODELABEL(uarte_consumer)
#define RADIO_NODE DT_NODELABEL(radio_consumer)
#define PDM_NODE   DT_NODELABEL(pdm_consumer)
#define USBHS_NODE DT_NODELABEL(usbhs_consumer)
#define GRTC_NODE  DT_NODELABEL(grtc_consumer)

/* The HFXO producer device, used to observe the shared clock state. */
static const struct device *const hfxo_dev = DEVICE_DT_GET(DT_NODELABEL(xo));

/*
 * The consumer -> producer bindings are fetched from devicetree at build time.
 * Each table entry knows which producer to request, at what rank and frequency,
 * with none of that hard-coded in the logic below.
 */
static const struct clock_state uarte_states[] = CLOCK_STATE_DEMO_STATES(UARTE_NODE);
static const struct clock_state radio_states[] = CLOCK_STATE_DEMO_STATES(RADIO_NODE);
static const struct clock_state pdm_states[] = CLOCK_STATE_DEMO_STATES(PDM_NODE);
static const struct clock_state usbhs_states[] = CLOCK_STATE_DEMO_STATES(USBHS_NODE);
static const struct clock_state grtc_states[] = CLOCK_STATE_DEMO_STATES(GRTC_NODE);

struct consumer {
	const char *name;
	const struct clock_state *states;
	size_t count;
};

static const struct consumer consumers[] = {
	{"uarte", uarte_states, ARRAY_SIZE(uarte_states)},
	{"radio", radio_states, ARRAY_SIZE(radio_states)},
	{"pdm", pdm_states, ARRAY_SIZE(pdm_states)},
	{"usbhs", usbhs_states, ARRAY_SIZE(usbhs_states)},
	{"grtc", grtc_states, ARRAY_SIZE(grtc_states)},
};

static const char *status_str(enum clock_control_status status)
{
	switch (status) {
	case CLOCK_CONTROL_STATUS_STARTING:
		return "starting";
	case CLOCK_CONTROL_STATUS_OFF:
		return "off";
	case CLOCK_CONTROL_STATUS_ON:
		return "on";
	default:
		return "unknown";
	}
}

static const char *producer_name(const struct clock_state *state)
{
	return (state->producer != NULL) ? state->producer->name : "(none/internal)";
}

static void hfxo_report(const char *ctx)
{
	LOG_INF("    HFXO is %s  (%s)", status_str(clock_control_get_status(hfxo_dev, NULL)), ctx);
}

static void dump_tree(void)
{
	LOG_INF("nRF54LM20B clock tree - consumer bindings from devicetree:");

	for (size_t c = 0; c < ARRAY_SIZE(consumers); c++) {
		const struct consumer *cons = &consumers[c];

		LOG_INF("  %s: %zu state(s)", cons->name, cons->count);
		for (size_t i = 0; i < cons->count; i++) {
			const struct clock_state *s = &cons->states[i];

			LOG_INF("    - %-14s producer=%-16s rank=%2u  %u Hz", s->name,
				producer_name(s), s->rank, s->frequency);
		}
	}
}

/* Demonstrate rank arbitration between the candidate states of one consumer. */
static void demo_rank_arbitration(void)
{
	const struct consumer *pdm = &consumers[2];
	const struct clock_state *chosen;

	LOG_INF("== Rank arbitration (consumer '%s') ==", pdm->name);

	chosen = clock_state_select_by_rank(pdm->states, pdm->count, CLOCK_STATE_DEMO_MAX_RANK);
	LOG_INF("  best of %zu candidates -> '%s' (rank %u) via %s", pdm->count, chosen->name,
		chosen->rank, producer_name(chosen));

	if (clock_state_apply(chosen) == 0) {
		LOG_INF("  applied '%s'", chosen->name);
	}
	k_sleep(K_MSEC(500));
	(void)clock_state_release(chosen);
}

/*
 * Demonstrate reference-counted sharing: two consumers both need HFXO. It stays
 * on until the last one releases it - impossible to model with plain on/off.
 */
static void demo_shared_hfxo(void)
{
	const struct clock_state *uarte_xtal =
		clock_state_find(uarte_states, ARRAY_SIZE(uarte_states), "xtal");
	const struct clock_state *radio_xtal =
		clock_state_find(radio_states, ARRAY_SIZE(radio_states), "xtal");

	LOG_INF("== Shared HFXO via request/release ==");
	hfxo_report("initial");

	LOG_INF("  uarte requests HFXO ...");
	(void)clock_state_apply(uarte_xtal);
	hfxo_report("after uarte request");

	LOG_INF("  radio requests HFXO ...");
	(void)clock_state_apply(radio_xtal);
	hfxo_report("after radio request");

	LOG_INF("  uarte releases HFXO (radio still holds it) ...");
	(void)clock_state_release(uarte_xtal);
	hfxo_report("after uarte release");

	LOG_INF("  radio releases HFXO (last consumer) ...");
	(void)clock_state_release(radio_xtal);
	hfxo_report("after radio release");
}

int main(void)
{
	LOG_INF("Clock-state concept demo (nRF54LM20B, request/release)");

	if (!device_is_ready(hfxo_dev)) {
		LOG_ERR("HFXO device not ready");
		return 0;
	}

	dump_tree();

	while (1) {
		demo_rank_arbitration();
		k_sleep(K_SECONDS(1));
		demo_shared_hfxo();
		k_sleep(K_SECONDS(3));
	}

	return 0;
}
