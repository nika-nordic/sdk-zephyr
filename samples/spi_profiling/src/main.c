/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <dmm.h>
#include <hal/nrf_spim.h>
#include <hal/nrf_timer.h>

#define FORCE_INLINE __attribute__((always_inline)) inline
#define NEVER_INLINE __attribute__ ((noinline))

#define SPI_MODE (SPI_WORD_SET(8) | SPI_LINES_SINGLE | SPI_TRANSFER_MSB)
#define SPIM_OP	 (SPI_OP_MODE_MASTER | SPI_MODE)
#define SPIM_DEV DT_NODELABEL(dut_spim) // change to `dut_spim_fast` for SPIM120 on H20

#define STOPWATCH_DEV DT_NODELABEL(stopwatch)

#define XFER_LEN 1

extern volatile uint32_t g_reg_writes;
extern volatile uint32_t g_reg_reads;

 // change to `dut_spi_dt_fast` for SPIM120 on H20
static struct spi_dt_spec spim = SPI_DT_SPEC_GET(DT_NODELABEL(dut_spi_dt), SPIM_OP, 0);

                                   // Uncomment to avoid bouncing the buffers on nRF54H20
static uint8_t spim_buffer_tx[32]; //DMM_MEMORY_SECTION(SPIM_DEV);
static uint8_t spim_buffer_rx[32]; //DMM_MEMORY_SECTION(SPIM_DEV);

BUILD_ASSERT(sizeof(spim_buffer_tx) >= XFER_LEN);
BUILD_ASSERT(sizeof(spim_buffer_rx) >= XFER_LEN);

static void raw_timer_configure(void)
{
    NRF_TIMER_Type * p_reg = (NRF_TIMER_Type *)DT_REG_ADDR(STOPWATCH_DEV);

    nrf_timer_prescaler_set(p_reg, 0); // 16 MHz timer
    nrf_timer_mode_set(p_reg, NRF_TIMER_MODE_TIMER);
    nrf_timer_bit_width_set(p_reg, NRF_TIMER_BIT_WIDTH_32);
    nrf_timer_shorts_set(p_reg, NRF_TIMER_SHORT_COMPARE0_STOP_MASK |
                                NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK);
    nrf_timer_int_disable(p_reg, UINT32_MAX);
}

static bool FORCE_INLINE raw_timer_cc_check(void)
{
    NRF_TIMER_Type * p_reg = (NRF_TIMER_Type *)DT_REG_ADDR(STOPWATCH_DEV);

    return nrf_timer_event_check(p_reg, NRF_TIMER_EVENT_COMPARE0);
}

static void FORCE_INLINE raw_timer_start(uint32_t cc)
{
    NRF_TIMER_Type * p_reg = (NRF_TIMER_Type *)DT_REG_ADDR(STOPWATCH_DEV);

    nrf_timer_task_trigger(p_reg, NRF_TIMER_TASK_STOP);
    nrf_timer_task_trigger(p_reg, NRF_TIMER_TASK_CLEAR);

    nrf_timer_event_clear(p_reg, NRF_TIMER_EVENT_COMPARE0);
    nrf_timer_cc_set(p_reg, NRF_TIMER_CC_CHANNEL0, cc);

    nrf_timer_task_trigger(p_reg, NRF_TIMER_TASK_START);
}

static uint32_t FORCE_INLINE raw_timer_capture(void)
{
    NRF_TIMER_Type * p_reg = (NRF_TIMER_Type *)DT_REG_ADDR(STOPWATCH_DEV);

    nrf_timer_task_trigger(p_reg, NRF_TIMER_TASK_CAPTURE1);
    return nrf_timer_cc_get(p_reg, NRF_TIMER_CC_CHANNEL1);
}

static void raw_spi_configure(void)
{
    NRF_SPIM_Type * p_reg = (NRF_SPIM_Type *)DT_REG_ADDR(SPIM_DEV);

    nrf_spim_int_disable(p_reg, UINT32_MAX);
    nrf_spim_orc_set(p_reg, 0xAA);
#if NRF_SPIM_HAS_FREQUENCY
    nrf_spim_frequency_set(p_reg, NRF_SPIM_FREQ_8M);
#elif NRF_SPIM_HAS_PRESCALER
    nrf_spim_prescaler_set(p_reg, 2 /*16 MHz / 2 = 8 MHz SCK*/);
#else
    #error
#endif

    nrf_spim_configure(p_reg, NRF_SPIM_MODE_0, NRF_SPIM_BIT_ORDER_MSB_FIRST);
}

static void raw_spi_cleanup(void)
{
    NRF_SPIM_Type * p_reg = (NRF_SPIM_Type *)DT_REG_ADDR(SPIM_DEV);

    nrf_spim_int_disable(p_reg, UINT32_MAX);
    nrf_spim_task_trigger(p_reg, NRF_SPIM_TASK_STOP);
    k_sleep(K_MSEC(1));
    nrf_spim_disable(p_reg);
    nrf_spim_event_clear(p_reg, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(p_reg, NRF_SPIM_EVENT_STOPPED);
    nrf_spim_event_clear(p_reg, NRF_SPIM_EVENT_STARTED);
}


static void FORCE_INLINE raw_spi_xfer_pre_setup(void)
{
    NRF_SPIM_Type * p_reg = (NRF_SPIM_Type *)DT_REG_ADDR(SPIM_DEV);

    nrf_spim_tx_buffer_set(p_reg, spim_buffer_tx, XFER_LEN);
    nrf_spim_rx_buffer_set(p_reg, spim_buffer_rx, XFER_LEN);
    nrf_spim_enable(p_reg);
}

static void FORCE_INLINE raw_spi_xfer_post_setup(void)
{
    NRF_SPIM_Type * p_reg = (NRF_SPIM_Type *)DT_REG_ADDR(SPIM_DEV);

    nrf_spim_disable(p_reg);
}

static void FORCE_INLINE raw_spi_transceive(void)
{
    NRF_SPIM_Type * p_reg = (NRF_SPIM_Type *)DT_REG_ADDR(SPIM_DEV);

    nrf_spim_event_clear(p_reg, NRF_SPIM_EVENT_END);
    nrf_spim_task_trigger(p_reg, NRF_SPIM_TASK_START);
    while (!nrf_spim_event_check(p_reg, NRF_SPIM_EVENT_END)) {
    }
}

static void NEVER_INLINE raw_spi_reg_writes_in_second(void)
{
    printf("Raw register write start at %llu\n", k_uptime_get());

    NRF_SPIM_Type * p_reg = (NRF_SPIM_Type *)DT_REG_ADDR(SPIM_DEV);

    raw_timer_start(UINT32_MAX);

#define RAW_REG_WRITE_CNT 1000
#define RAW_REG_WRITER(i, _) nrf_spim_orc_set(p_reg, 0xAA)
    LISTIFY(RAW_REG_WRITE_CNT, RAW_REG_WRITER, (;));

    uint32_t cc = raw_timer_capture();
    uint32_t us = cc / 16;
    uint32_t op = (cc * 1000) / (RAW_REG_WRITE_CNT * 16);

    printf("Raw register write end at %llu. Time needed for %u ops = %u [us]. One op = %u [ns]\n",
           k_uptime_get(), RAW_REG_WRITE_CNT, us, op);
}

static void NEVER_INLINE raw_spi_reg_reads_in_second(void)
{
    printf("Raw register read start at %llu\n", k_uptime_get());

    NRF_SPIM_Type * p_reg = (NRF_SPIM_Type *)DT_REG_ADDR(SPIM_DEV);

    raw_timer_start(UINT32_MAX);

#define RAW_REG_READ_CNT 1000
#define RAW_REG_READER(i, _) nrf_spim_rx_maxcnt_get(p_reg)
    LISTIFY(RAW_REG_READ_CNT, RAW_REG_READER, (;));

    uint32_t cc = raw_timer_capture();
    uint32_t us = cc / 16;
    uint32_t op = (cc * 1000) / (RAW_REG_READ_CNT * 16);

    printf("Raw register read end at %llu. Time needed for %u ops = %u [us]. One op = %u [ns]\n",
           k_uptime_get(), RAW_REG_READ_CNT, us, op);
}

static void raw_spi_ops_in_second(void)
{
    printf("Raw SPI start at %llu\n", k_uptime_get());

    uint32_t op_cnt = 0;

    raw_spi_configure();
    raw_spi_xfer_pre_setup();

    raw_timer_start(16 * 1000 * 1000 /* 16 MHz / 16M ticks == 1 second */);
    while (!raw_timer_cc_check()) {
        raw_spi_transceive();
        op_cnt++;
    }

    raw_spi_xfer_post_setup();

    printf("Raw SPI end at %llu. Ops = %u\n", k_uptime_get(), op_cnt);
}

static void NEVER_INLINE zephyr_spi_ops_in_second(void)
{
    printf("Zephyr SPI start at %llu\n", k_uptime_get());

    struct spi_buf_set sets[4];
    struct spi_buf_set *mtx_set;
    struct spi_buf_set *mrx_set;
    struct spi_buf bufs[8];
    uint32_t op_cnt = 0;

    bufs[0].buf = spim_buffer_tx;
    bufs[0].len = XFER_LEN;

    bufs[1].buf = spim_buffer_rx;
    bufs[1].len = XFER_LEN;

    sets[0].buffers = &bufs[0];
    sets[0].count = 1;
    mtx_set = &sets[0];

    sets[1].buffers = &bufs[1];
    sets[1].count = 1;
    mrx_set = &sets[1];

    raw_timer_start(16 * 1000 * 1000 /* 16 MHz / 16M ticks == 1 second */);

    while (!raw_timer_cc_check()) {
        (void)spi_transceive_dt(&spim, mtx_set, mrx_set);
        op_cnt++;
    }

    printf("Zephyr SPI end at %llu. Ops = %u\n", k_uptime_get(), op_cnt);
}

static void NEVER_INLINE zephyr_spi_reg_access_in_op(void)
{
    printf("Zephyr SPI register access count start at %llu\n", k_uptime_get());

    struct spi_buf_set sets[4];
    struct spi_buf_set *mtx_set;
    struct spi_buf_set *mrx_set;
    struct spi_buf bufs[8];

    bufs[0].buf = spim_buffer_tx;
    bufs[0].len = XFER_LEN;

    bufs[1].buf = spim_buffer_rx;
    bufs[1].len = XFER_LEN;

    sets[0].buffers = &bufs[0];
    sets[0].count = 1;
    mtx_set = &sets[0];

    sets[1].buffers = &bufs[1];
    sets[1].count = 1;
    mrx_set = &sets[1];

    g_reg_writes = 0;
    g_reg_reads = 0;

    raw_timer_start(UINT32_MAX);

    (void)spi_transceive_dt(&spim, mtx_set, mrx_set);

    uint32_t cc = raw_timer_capture();
    uint32_t us = cc / 16;

    printf("Zephyr SPI register access count end at %llu. Reads=%u, writes=%u, total_time=%u [us].\n",
           k_uptime_get(), g_reg_reads, g_reg_writes, us);
}

int main(void)
{
    printf("SPI performance test on %s\n", CONFIG_BOARD_TARGET);

    memset(spim_buffer_tx, 0xAA, sizeof(spim_buffer_tx));
    raw_timer_configure();
    raw_spi_cleanup();

    raw_spi_reg_writes_in_second();
    raw_spi_reg_reads_in_second();
    raw_spi_ops_in_second();

    raw_spi_cleanup();
    // Comment for implicit PM
    // Uncomment for explicit PM
    //(void)pm_device_runtime_get(spim.bus);
    zephyr_spi_reg_access_in_op();
    zephyr_spi_ops_in_second();
    // Comment for implicit PM
    // Uncomment for explicit PM
    //(void)pm_device_runtime_put(spim.bus);

    printf("SPI performance done\n");

    return 0;
}
