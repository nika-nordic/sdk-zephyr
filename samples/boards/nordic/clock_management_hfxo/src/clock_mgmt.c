/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/logging/log.h>

#include "clock_ids.h"
#include "clock_mgmt.h"

LOG_MODULE_REGISTER(clkmgmt, LOG_LEVEL_INF);

#define CLK_REQUEST_TIMEOUT K_MSEC(500)
#define MAX_CONSUMERS       16

/*
 * Leaf producers that are actually driven at runtime. The rest of the tree
 * (muxes, gate, derived clocks) is resolved in software: on nRF54LM20B,
 * "HFCLK = HFXTAL" effectively means "HFXO is requested", so the mux source is
 * inferred rather than programmed via a dedicated register.
 */
static const struct device *const hfxo_dev = DEVICE_DT_GET(DT_NODELABEL(xo));
static const struct device *const xo24m_dev = DEVICE_DT_GET(DT_NODELABEL(xo24m));
static const struct device *const lfclk_dev = DEVICE_DT_GET(DT_NODELABEL(lfclk));

static struct clk_consumer *consumers[MAX_CONSUMERS];
static size_t n_consumers;

/* Current leaf hold state, so re-resolution only issues the deltas. */
static bool hfxo_held;
static bool xo24m_held;
static bool lfclk_held;

/* Result of the most recent resolution. */
static struct {
	uint8_t hfclk_src; /* HFCLK_SRC_* */
	uint8_t lfclk_src; /* LFCLK_SRC_* */
	bool xo24m_on;
	bool lfclk_on;
	bool hfxo_on;
} resolved;

void clkmgmt_register(struct clk_consumer *consumer)
{
	__ASSERT_NO_MSG(n_consumers < MAX_CONSUMERS);
	consumers[n_consumers++] = consumer;
}

static void leaf_set(const struct device *dev, bool *held, bool want)
{
	if (want && !*held) {
		int err = nrf_clock_control_request_sync(dev, NULL, CLK_REQUEST_TIMEOUT);

		if (err < 0) {
			LOG_ERR("request '%s' failed (%d)", dev->name, err);
			return;
		}
		*held = true;
	} else if (!want && *held) {
		(void)nrf_clock_control_release(dev, NULL);
		*held = false;
	}
}

/*
 * The heart of the layer: recompute the whole tree from every consumer's held
 * state, arbitrate shared nodes by rank, apply the opportunistic-HFXO rule, and
 * drive the leaf producers to match.
 */
static void clkmgmt_resolve(void)
{
	bool xo24m_on = false;
	bool lfclk_on = false;
	bool hfclk_requested = false;
	uint32_t hfclk_best_rank = UINT32_MAX;
	uint8_t hfclk_src = HFCLK_SRC_HFINT;
	uint32_t lfclk_best_rank = UINT32_MAX;
	uint8_t lfclk_src = LFCLK_SRC_LFRC;

	for (size_t i = 0; i < n_consumers; i++) {
		const struct clk_setting *s = consumers[i]->active;

		if (s == NULL) {
			continue;
		}

		switch (s->node_id) {
		case CLK_ID_HFCLK:
			hfclk_requested = true;
			if (s->rank < hfclk_best_rank) {
				hfclk_best_rank = s->rank;
				hfclk_src = s->source;
			}
			break;
		case CLK_ID_XO24M:
			xo24m_on = xo24m_on || (s->source == XO24M_ON);
			break;
		case CLK_ID_LFCLK:
			lfclk_on = true;
			if (s->rank < lfclk_best_rank) {
				lfclk_best_rank = s->rank;
				lfclk_src = s->source;
			}
			break;
		default:
			break;
		}
	}

	/*
	 * HFXO must run if the HFCLK mux arbitration picked it, or if XO24M is in
	 * use (XO24M is derived from HFXTAL).
	 */
	bool hfxo_on = (hfclk_requested && hfclk_src == HFCLK_SRC_HFXO) || xo24m_on;

	/*
	 * Opportunistic upgrade: if HFXO is running for any reason, HFCLK sources
	 * from it - so even consumers that only asked for HFINT get the crystal
	 * "for free".
	 */
	resolved.hfclk_src = hfxo_on ? HFCLK_SRC_HFXO : HFCLK_SRC_HFINT;
	resolved.lfclk_src = lfclk_src;
	resolved.xo24m_on = xo24m_on;
	resolved.lfclk_on = lfclk_on;
	resolved.hfxo_on = hfxo_on;

	/* Drive the real producers to match the resolved tree. */
	leaf_set(hfxo_dev, &hfxo_held, hfxo_on);
	leaf_set(xo24m_dev, &xo24m_held, xo24m_on);
	leaf_set(lfclk_dev, &lfclk_held, lfclk_on);
}

int clkmgmt_apply(struct clk_consumer *consumer, const char *state_name)
{
	for (size_t i = 0; i < consumer->n_states; i++) {
		if (strcmp(consumer->states[i].name, state_name) == 0) {
			consumer->active = &consumer->states[i];
			clkmgmt_resolve();
			return 0;
		}
	}

	LOG_ERR("consumer '%s' has no state '%s'", consumer->name, state_name);
	return -ENOENT;
}

int clkmgmt_release(struct clk_consumer *consumer)
{
	consumer->active = NULL;
	clkmgmt_resolve();
	return 0;
}

/* Effective source seen by a consumer, after tree resolution. */
static const char *consumer_effective(const struct clk_consumer *consumer)
{
	const struct clk_setting *s = consumer->active;

	if (s == NULL) {
		return "-";
	}

	switch (s->node_id) {
	case CLK_ID_HFCLK:
		return (resolved.hfclk_src == HFCLK_SRC_HFXO) ? "HFXO" : "HFINT";
	case CLK_ID_XO24M:
		return "HFXO(24M)";
	case CLK_ID_LFCLK:
		return (resolved.lfclk_src == LFCLK_SRC_LFXO) ? "LFXO" : "LFRC";
	default:
		return "?";
	}
}

static const char *onoff(bool on)
{
	return on ? "on" : "off";
}

void clkmgmt_report(const char *ctx)
{
	LOG_INF("  [%s]", ctx);
	LOG_INF("    tree: HFXO=%s XO24M=%s LFCLK=%s | HFCLK src=%s | LFCLK src=%s",
		onoff(clock_control_get_status(hfxo_dev, NULL) == CLOCK_CONTROL_STATUS_ON),
		onoff(resolved.xo24m_on), onoff(resolved.lfclk_on),
		(resolved.hfclk_src == HFCLK_SRC_HFXO) ? "HFXO" : "HFINT",
		(resolved.lfclk_src == LFCLK_SRC_LFXO) ? "LFXO" : "LFRC");

	for (size_t i = 0; i < n_consumers; i++) {
		const struct clk_consumer *c = consumers[i];

		if (c->active == NULL) {
			continue;
		}

		LOG_INF("    %-6s wants %-11s -> effective %-9s (%u Hz)", c->name, c->active->name,
			consumer_effective(c), c->active->freq);
	}
}
