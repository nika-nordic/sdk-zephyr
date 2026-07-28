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

/* The clock consumer whose producer binding is described in devicetree. */
#define CLOCK_CONSUMER_NODE DT_NODELABEL(clockdemo)

BUILD_ASSERT(DT_NODE_EXISTS(CLOCK_CONSUMER_NODE),
	     "clockdemo consumer node is missing (check the board overlay)");

/*
 * The entire consumer -> producer binding is fetched from devicetree at build
 * time here. Each entry knows which producer device to drive, at what rank, and
 * for what frequency, without any of that being hard-coded in the logic below.
 */
static const struct clock_state clock_states[] = CLOCK_STATE_DEMO_STATES(CLOCK_CONSUMER_NODE);

static const char *status_str(enum clock_control_status status)
{
	switch (status) {
	case CLOCK_CONTROL_STATUS_STARTING:
		return "starting";
	case CLOCK_CONTROL_STATUS_OFF:
		return "off";
	case CLOCK_CONTROL_STATUS_ON:
		return "on";
	case CLOCK_CONTROL_STATUS_UNKNOWN:
	default:
		return "unknown";
	}
}

static void report_producer(const struct clock_state *state)
{
	enum clock_control_status status = clock_control_get_status(state->producer, NULL);

	LOG_INF("  producer '%s' is now %s", state->producer->name, status_str(status));
}

static void dump_states(void)
{
	LOG_INF("Consumer '%s' is bound to %zu clock state(s):",
		DT_NODE_FULL_NAME(CLOCK_CONSUMER_NODE), ARRAY_SIZE(clock_states));

	for (size_t i = 0; i < ARRAY_SIZE(clock_states); i++) {
		const struct clock_state *state = &clock_states[i];

		LOG_INF("  [%zu] name=%-6s producer=%-8s rank=%u frequency=%u Hz", i, state->name,
			state->producer->name, state->rank, state->frequency);
	}
}

/* Pick the best eligible state, apply it, and confirm the hardware effect. */
static void run_state_by_rank(uint32_t max_rank)
{
	const struct clock_state *state;
	int err;

	LOG_INF("Requesting best state with rank <= %u ...", max_rank);

	state = clock_state_select_by_rank(clock_states, ARRAY_SIZE(clock_states), max_rank);
	if (state == NULL) {
		LOG_WRN("  no eligible clock state for this constraint");
		return;
	}

	LOG_INF("  selected '%s' (rank %u, %u Hz)", state->name, state->rank, state->frequency);

	err = clock_state_apply(state);
	if (err < 0) {
		LOG_ERR("  failed to apply state '%s' (%d)", state->name, err);
		return;
	}

	report_producer(state);
}

int main(void)
{
	LOG_INF("Clock-state HFXO concept demo");

	for (size_t i = 0; i < ARRAY_SIZE(clock_states); i++) {
		if (!device_is_ready(clock_states[i].producer)) {
			LOG_ERR("producer '%s' not ready", clock_states[i].producer->name);
			return 0;
		}
	}

	dump_states();

	while (1) {
		/*
		 * Unconstrained request: the lowest-rank ("active", high-accuracy
		 * HFXO) state wins and HFXO is started.
		 */
		run_state_by_rank(CLOCK_STATE_DEMO_MAX_RANK);
		k_sleep(K_SECONDS(2));

		/*
		 * Switch to the "sleep" state to release the producer, i.e. stop
		 * HFXO. This mirrors a consumer that no longer needs the clock.
		 */
		LOG_INF("Switching to 'sleep' state, releasing HFXO ...");
		{
			const struct clock_state *sleep_state =
				&clock_states[ARRAY_SIZE(clock_states) - 1];
			int err = clock_state_release(sleep_state);

			if (err < 0) {
				LOG_ERR("  failed to release '%s' (%d)", sleep_state->name, err);
			} else {
				report_producer(sleep_state);
			}
		}
		k_sleep(K_SECONDS(2));
	}

	return 0;
}
