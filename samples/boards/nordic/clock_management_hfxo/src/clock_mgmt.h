/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Sample-local clock-management layer. It sits on top of the producer drivers:
 * consumers apply named clock-states (fetched from devicetree at build time),
 * and this layer resolves the whole clock tree, arbitrates shared nodes by rank,
 * applies the opportunistic-HFXO rule, and enables/disables the leaf producers
 * (HFXO / XO24M / LFCLK) accordingly.
 *
 * It is a stand-in for a future clock-management framework.
 */

#ifndef CLOCK_MGMT_H_
#define CLOCK_MGMT_H_

#include <stddef.h>
#include <stdint.h>

#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>

/**
 * @brief One selectable configuration, resolved from a "clock-state" node.
 *
 * @param name     Human-readable name (from the consumer's clock-state-names).
 * @param node_id  Tree node this state configures (CLK_ID_*, from its mgmt-id).
 * @param source   Selector cell: mux input index or gate on/off.
 * @param rank     Arbitration rank (lower is preferred).
 * @param freq     Peripheral frequency (Hz) this state yields.
 */
struct clk_setting {
	const char *name;
	uint8_t node_id;
	uint8_t source;
	uint32_t rank;
	uint32_t freq;
};

/** @brief A clock consumer and the state it currently holds (if any). */
struct clk_consumer {
	const char *name;
	const struct clk_setting *states;
	size_t n_states;
	const struct clk_setting *active; /* NULL when released */
};

/* --- Build-time helpers to turn a consumer DT node into a states table --- */

#define _CLK_STATE_NODE(consumer, idx) DT_PHANDLE(consumer, clock_state_##idx)
#define _CLK_STATE_TARGET(state)       DT_PHANDLE_BY_IDX(state, clocks, 0)

#define _CLK_SETTING(consumer, idx)                                                                \
	COND_CODE_1(DT_NODE_HAS_PROP(consumer, clock_state_##idx),                                  \
		    ({                                                                             \
			    .name = DT_PROP_BY_IDX(consumer, clock_state_names, idx),               \
			    .node_id =                                                              \
				    DT_PROP(_CLK_STATE_TARGET(_CLK_STATE_NODE(consumer, idx)),      \
					    mgmt_id),                                              \
			    .source = DT_PHA_BY_IDX(_CLK_STATE_NODE(consumer, idx), clocks, 0,      \
						    source),                                           \
			    .rank = DT_PROP(_CLK_STATE_NODE(consumer, idx), rank),                  \
			    .freq = DT_PROP(_CLK_STATE_NODE(consumer, idx), clock_frequency),       \
		    },),                                                                           \
		    ())

/** @brief Build a `struct clk_setting[]` initializer for a consumer's states. */
#define CLK_CONSUMER_STATES(consumer)                                                              \
	{                                                                                          \
		_CLK_SETTING(consumer, 0)                                                           \
		_CLK_SETTING(consumer, 1)                                                           \
		_CLK_SETTING(consumer, 2)                                                           \
		_CLK_SETTING(consumer, 3)                                                           \
	}

/** @brief Define and initialize a consumer object from its DT node. */
#define CLK_CONSUMER_DEFINE(var, node)                                                             \
	static const struct clk_setting var##_states[] = CLK_CONSUMER_STATES(node);                \
	static struct clk_consumer var = {                                                         \
		.name = DT_NODE_FULL_NAME(node),                                                    \
		.states = var##_states,                                                             \
		.n_states = ARRAY_SIZE(var##_states),                                               \
		.active = NULL,                                                                     \
	}

/* --- Runtime API --- */

/** @brief Register a consumer with the layer (before applying states). */
void clkmgmt_register(struct clk_consumer *consumer);

/**
 * @brief Apply a named state for @p consumer, then re-resolve the whole tree.
 *
 * @retval 0 on success, -ENOENT if the state name is unknown, or a negative
 *         errno propagated from a producer request.
 */
int clkmgmt_apply(struct clk_consumer *consumer, const char *state_name);

/** @brief Release @p consumer's state, then re-resolve the whole tree. */
int clkmgmt_release(struct clk_consumer *consumer);

/** @brief Log the resolved tree state and each active consumer's effective clock. */
void clkmgmt_report(const char *ctx);

#endif /* CLOCK_MGMT_H_ */
