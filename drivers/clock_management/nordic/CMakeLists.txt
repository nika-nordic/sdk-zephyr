# Copyright (c) 2026 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources_ifdef(CONFIG_CLOCK_MANAGEMENT_NORDIC nrf5340_clock.c)

# Header defining the Z_CLOCK_MANAGEMENT_xxx_DATA_DEFINE/GET specifier macros
# for the nRF5340 clock node compatibles.
add_clock_management_header(nrf5340_clock_drivers.h)
