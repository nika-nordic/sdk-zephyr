/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * nRF5340 clock-management node drivers.
 *
 * These drivers model the portion of the nRF5340 HFCLK domain needed to
 * showcase the clock-states concept: two root clocks (the internal RC
 * oscillator HFINT and the crystal oscillator HFXO) feeding a source
 * multiplexer (HFCLK). A consumer taps the multiplexer through a
 * "clock-output" leaf node and selects its source by applying a clock state.
 *
 * The root clocks report the peripheral-visible frequency (PCLK) they produce;
 * the only functional difference between them is clock accuracy. The actual
 * crystal start/stop is performed by the multiplexer when its source selection
 * changes.
 */

#include <zephyr/drivers/clock_management/clock_driver.h>
#include <hal/nrf_clock.h>

/* ------------------------------------------------------------------------- */
/* Root clocks (HFINT / HFXO)                                                */
/* ------------------------------------------------------------------------- */

struct nrf5340_root_config {
	uint32_t rate;
};

static clock_freq_t nrf5340_root_get_rate(const struct clk *clk_hw)
{
	const struct nrf5340_root_config *config = clk_hw->hw_data;

	return config->rate;
}

static const struct clock_management_root_api nrf5340_root_api = {
	.get_rate = nrf5340_root_get_rate,
};

#define DT_DRV_COMPAT nordic_nrf5340_hfint
#define NRF5340_HFINT_DEFINE(inst)                                             \
	static const struct nrf5340_root_config hfint_config_##inst = {        \
		.rate = DT_INST_PROP(inst, clock_frequency),                   \
	};                                                                     \
	ROOT_CLOCK_DT_INST_DEFINE(inst, &hfint_config_##inst, &nrf5340_root_api);
DT_INST_FOREACH_STATUS_OKAY(NRF5340_HFINT_DEFINE)
#undef DT_DRV_COMPAT

#define DT_DRV_COMPAT nordic_nrf5340_hfxo
#define NRF5340_HFXO_DEFINE(inst)                                              \
	static const struct nrf5340_root_config hfxo_config_##inst = {         \
		.rate = DT_INST_PROP(inst, clock_frequency),                   \
	};                                                                     \
	ROOT_CLOCK_DT_INST_DEFINE(inst, &hfxo_config_##inst, &nrf5340_root_api);
DT_INST_FOREACH_STATUS_OKAY(NRF5340_HFXO_DEFINE)
#undef DT_DRV_COMPAT

/* ------------------------------------------------------------------------- */
/* HFCLK source multiplexer                                                  */
/* ------------------------------------------------------------------------- */

#define DT_DRV_COMPAT nordic_nrf5340_hfclk

/* Source indices must match the order of the "input-sources" phandle array. */
#define NRF5340_HFCLK_SRC_HFINT 0U
#define NRF5340_HFCLK_SRC_HFXO  1U

struct nrf5340_hfclk_config {
	/* Must be first: consumed by the clock subsystem (parents/parent_cnt). */
	MUX_CLK_SUBSYS_DATA_DEFINE
};

static int nrf5340_hfclk_get_parent(const struct clk *clk_hw)
{
	nrf_clock_hfclk_t src = NRF_CLOCK_HFCLK_LOW_ACCURACY;

	ARG_UNUSED(clk_hw);

	(void)nrf_clock_is_running(NRF_CLOCK, NRF_CLOCK_DOMAIN_HFCLK, &src);

	return (src == NRF_CLOCK_HFCLK_HIGH_ACCURACY) ?
		NRF5340_HFCLK_SRC_HFXO : NRF5340_HFCLK_SRC_HFINT;
}

static int nrf5340_hfclk_configure(const struct clk *clk_hw, const void *data)
{
	const struct nrf5340_hfclk_config *config = clk_hw->hw_data;
	uint32_t source = (uint32_t)(uintptr_t)data;

	if (source >= config->parent_cnt) {
		return -EINVAL;
	}

	if (source == NRF5340_HFCLK_SRC_HFXO) {
		/* Switch HFCLK to the crystal oscillator and wait for it. */
		nrf_clock_event_clear(NRF_CLOCK, NRF_CLOCK_EVENT_HFCLKSTARTED);
		nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKSTART);
		while (!nrf_clock_event_check(NRF_CLOCK,
					      NRF_CLOCK_EVENT_HFCLKSTARTED)) {
			/* Busy-wait until the crystal has ramped up. */
		}
	} else {
		/* Release the crystal; HFCLK falls back to the internal RC. */
		nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKSTOP);
	}

	return 0;
}

static const struct clock_management_mux_api nrf5340_hfclk_api = {
	.shared.configure = nrf5340_hfclk_configure,
	.get_parent = nrf5340_hfclk_get_parent,
};

#define GET_MUX_INPUT(node_id, prop, idx)                                      \
	CLOCK_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

#define NRF5340_HFCLK_DEFINE(inst)                                             \
	static const struct clk *const hfclk_parents_##inst[] = {              \
		DT_INST_FOREACH_PROP_ELEM(inst, input_sources, GET_MUX_INPUT)};\
	static const struct nrf5340_hfclk_config hfclk_config_##inst = {       \
		MUX_CLK_SUBSYS_DATA_INIT(hfclk_parents_##inst,                 \
					 DT_INST_PROP_LEN(inst, input_sources))\
	};                                                                     \
	MUX_CLOCK_DT_INST_DEFINE(inst, &hfclk_config_##inst, &nrf5340_hfclk_api);
DT_INST_FOREACH_STATUS_OKAY(NRF5340_HFCLK_DEFINE)
