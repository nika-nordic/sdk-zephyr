/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_CLOCK_MANAGEMENT_NORDIC_NRF5340_CLOCK_DRIVERS_H_
#define ZEPHYR_DRIVERS_CLOCK_MANAGEMENT_NORDIC_NRF5340_CLOCK_DRIVERS_H_

#include <zephyr/devicetree.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @cond INTERNAL_HIDDEN */

/*
 * Specifier-to-config-data macros for nRF5340 clock-management node drivers.
 *
 * Only the HFCLK multiplexer is referenced from clock-state "clocks" lists, so
 * only it needs these macros. The single "source" specifier cell (the index
 * into the mux "input-sources" array) is passed verbatim as the configuration
 * blob to the mux configure() callback.
 */
#define Z_CLOCK_MANAGEMENT_DATA_DEFINE_nordic_nrf5340_hfclk(node_id, prop, idx)
#define Z_CLOCK_MANAGEMENT_DATA_GET_nordic_nrf5340_hfclk(node_id, prop, idx)    \
	DT_PHA_BY_IDX(node_id, prop, idx, source)

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_CLOCK_MANAGEMENT_NORDIC_NRF5340_CLOCK_DRIVERS_H_ */
