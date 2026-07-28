/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal, sample-local "abstracted interface" that stands in for a future
 * clock-management framework. It turns the devicetree binding between a clock
 * consumer and its clock producer(s) into a build-time table, arbitrates
 * between candidate states by rank, and applies the selected state by really
 * starting/stopping the producer through the classic clock_control API.
 */

#ifndef CLOCK_STATE_DEMO_H_
#define CLOCK_STATE_DEMO_H_

#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/sys/util.h>

/**
 * @brief One resolved clock state fetched from devicetree.
 *
 * All fields are populated at build time from a "clock-state" node:
 *  - @ref producer is resolved from the state's `clocks` phandle,
 *  - @ref rank / @ref frequency come from the `rank` / `clock-frequency` props.
 */
struct clock_state {
	const char *name;
	const struct device *producer;
	uint32_t rank;
	uint32_t frequency;
};

/** Rank constraint that accepts any state (no upper bound). */
#define CLOCK_STATE_DEMO_MAX_RANK UINT32_MAX

/** Resolve a clock-state node to its (first) producer device. */
#define CLOCK_STATE_PRODUCER(state_node)                                                           \
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(state_node, clocks, 0))

/* Build one struct clock_state initializer for consumer's clock-state-<idx>. */
#define CLOCK_STATE_DEMO_ENTRY(consumer, idx)                                                      \
	COND_CODE_1(DT_NODE_HAS_PROP(consumer, clock_state_##idx),                                  \
		    ({                                                                             \
			    .name = DT_PROP_BY_IDX(consumer, clock_state_names, idx),               \
			    .producer =                                                             \
				    CLOCK_STATE_PRODUCER(DT_PHANDLE(consumer, clock_state_##idx)),  \
			    .rank = DT_PROP(DT_PHANDLE(consumer, clock_state_##idx), rank),         \
			    .frequency = DT_PROP(DT_PHANDLE(consumer, clock_state_##idx),           \
						 clock_frequency),                                     \
		    },),                                                                           \
		    ())

/**
 * @brief Build a `struct clock_state[]` initializer for all states of a consumer.
 *
 * Supports up to three states (clock-state-0..2), mirroring the sample binding.
 */
#define CLOCK_STATE_DEMO_STATES(consumer)                                                          \
	{                                                                                          \
		CLOCK_STATE_DEMO_ENTRY(consumer, 0)                                                 \
		CLOCK_STATE_DEMO_ENTRY(consumer, 1)                                                 \
		CLOCK_STATE_DEMO_ENTRY(consumer, 2)                                                 \
	}

/** Apply a clock state: start its producer (blocking). */
static inline int clock_state_apply(const struct clock_state *state)
{
	return clock_control_on(state->producer, NULL);
}

/** Release a clock state: stop its producer. */
static inline int clock_state_release(const struct clock_state *state)
{
	return clock_control_off(state->producer, NULL);
}

/**
 * @brief Select the best (lowest-rank) state not exceeding @p max_rank.
 *
 * Rank arbitration: lower is preferred (0 = best). The @p max_rank constraint
 * is "sticky" - states ranked above it are never eligible.
 *
 * @return Pointer to the selected state, or NULL if none is eligible.
 */
static inline const struct clock_state *
clock_state_select_by_rank(const struct clock_state *states, size_t count, uint32_t max_rank)
{
	const struct clock_state *best = NULL;

	for (size_t i = 0; i < count; i++) {
		if (states[i].rank > max_rank) {
			continue;
		}

		if ((best == NULL) || (states[i].rank < best->rank)) {
			best = &states[i];
		}
	}

	return best;
}

#endif /* CLOCK_STATE_DEMO_H_ */
