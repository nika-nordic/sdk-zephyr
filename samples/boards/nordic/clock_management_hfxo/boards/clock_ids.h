/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Shared identifiers for the nRF54LM20B clock tree model. Included by both the
 * board overlay (to tag clock nodes and encode clock-state selections) and the
 * C clock-management layer (to index its runtime node model).
 */

#ifndef CLOCK_IDS_H_
#define CLOCK_IDS_H_

/* Stable per-node ids. The C resolver's node model is indexed by these. */
#define CLK_ID_HFXO     0
#define CLK_ID_HFINT    1
#define CLK_ID_LFRC     2
#define CLK_ID_LFXO     3
#define CLK_ID_HFCLK    4
#define CLK_ID_XO24M    5
#define CLK_ID_LFCLK    6
#define CLK_ID_COUNT    7

/* HFCLK mux source selection (specifier cell on &hfclk). */
#define HFCLK_SRC_HFINT 0
#define HFCLK_SRC_HFXO  1

/* LFCLK mux source selection (specifier cell on &lfclk_mux). */
#define LFCLK_SRC_LFRC  0
#define LFCLK_SRC_LFXO  1

/* XO24M gate selection (specifier cell on &xo24m_gate). */
#define XO24M_OFF       0
#define XO24M_ON        1

#endif /* CLOCK_IDS_H_ */
