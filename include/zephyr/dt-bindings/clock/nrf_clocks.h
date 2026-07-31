/*
 * Copyright (c) 2026 Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_NRF_CLOCKS_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_NRF_CLOCKS_H_

/*
 * Optional clock specifiers for the simplified ("on/off") clock management scheme.
 *
 * In the simplified scheme a peripheral references its clock producer device
 * directly via the `clocks` property, and the mere presence of the property
 * makes the driver request (start) that producer. The desired source is encoded
 * by *which* producer node is referenced, so a specifier is only needed to refine
 * the request (e.g. tuned crystal). It is passed as the optional second cell of
 * the `clocks` property.
 */
#define NRF_DT_CLK_DEFAULT	0 /* no specifier: request the producer as-is */
#define NRF_XO_TUNED		1 /* request tuned crystal (XOTUNED) */
#define NRF_HFINT		2 /* request internal oscillator */
#define NRF_OPEN_LOOP		3 /* request open-loop mode */

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_NRF_CLOCKS_H_ */
