# Copyright (c) 2026 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(clock_management_hfxo)

target_sources(app PRIVATE src/main.c src/clock_mgmt.c)
target_include_directories(app PRIVATE boards)
