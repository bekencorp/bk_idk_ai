// Copyright 2020-2021 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include <os/os.h>
#include <os/mem.h>
#include <driver/spi.h>
#include <driver/dma.h>
#include "bk_general_dma.h"
#include <driver/gpio.h>
#include "gpio_driver.h"
#include <driver/lcd.h>
#include <driver/lcd_types.h>
#include <driver/lcd_qspi.h>
#include <driver/qspi.h>
#include <driver/qspi_types.h>
#include <driver/lcd_spi.h>
#include "bk_misc.h"

#define LCD_SPI_TAG "lcd_spi"

#define LCD_SPI_LOGI(...) BK_LOGI(LCD_SPI_TAG, ##__VA_ARGS__)
#define LCD_SPI_LOGW(...) BK_LOGW(LCD_SPI_TAG, ##__VA_ARGS__)
#define LCD_SPI_LOGE(...) BK_LOGE(LCD_SPI_TAG, ##__VA_ARGS__)
#define LCD_SPI_LOGD(...) BK_LOGD(LCD_SPI_TAG, ##__VA_ARGS__)


#define LCD_SPI_REFRESH_WITH_QSPI    1

#if (LCD_SPI_REFRESH_WITH_QSPI == 1)
#ifdef CONFIG_LCD_SPI_REFRESH_WITH_QSPI_MAPPING_MODE
#define LCD_SPI_REFRESH_WITH_QSPI_MAPPING_MODE    CONFIG_LCD_SPI_REFRESH_WITH_QSPI_MAPPING_MODE
#else
#define LCD_SPI_REFRESH_WITH_QSPI_MAPPING_MODE    0
#endif
#endif

#ifdef CONFIG_LCD_SPI_BACKLIGHT_PIN
#define LCD_SPI_BACKLIGHT_PIN       CONFIG_LCD_SPI_BACKLIGHT_PIN
#else
#define LCD_SPI_BACKLIGHT_PIN       GPIO_25
#endif

#if (CONFIG_LCD_SPI_DEVICE_NUM > 1)

#ifdef CONFIG_LCD0_SPI_RESET_PIN
#define LCD0_SPI_RESET_PIN          CONFIG_LCD0_SPI_RESET_PIN
#else
#define LCD0_SPI_RESET_PIN          GPIO_6
#endif

#ifdef CONFIG_LCD0_SPI_DC_PIN
#define LCD0_SPI_DC_PIN             CONFIG_LCD0_SPI_DC_PIN
#else
#define LCD0_SPI_DC_PIN             GPIO_7
#endif

#ifdef CONFIG_LCD1_SPI_RESET_PIN
#define LCD1_SPI_RESET_PIN          CONFIG_LCD1_SPI_RESET_PIN
#else
#define LCD1_SPI_RESET_PIN          GPIO_45
#endif

#ifdef CONFIG_LCD1_SPI_DC_PIN
#define LCD1_SPI_DC_PIN             CONFIG_LCD1_SPI_DC_PIN
#else
#define LCD1_SPI_DC_PIN             GPIO_5
#endif

#else

#ifdef CONFIG_LCD_SPI_RESET_PIN
#define LCD_SPI_RESET_PIN           CONFIG_LCD_SPI_RESET_PIN
#else
#define LCD_SPI_RESET_PIN           GPIO_28
#endif

#ifdef CONFIG_LCD_SPI_DC_PIN
#define LCD_SPI_DC_PIN              CONFIG_LCD_SPI_DC_PIN
#else
#define LCD_SPI_DC_PIN              GPIO_9
#endif

#ifdef CONFIG_LCD_SPI_TE_PIN
#define LCD_SPI_TE_PIN              CONFIG_LCD_SPI_TE_PIN
#else
#define LCD_SPI_TE_PIN              GPIO_6
#endif

#endif

#define LCD_SPI_DEVICE_CASET        0x2A
#define LCD_SPI_DEVICE_RASET        0x2B
#define LCD_SPI_DEVICE_RAMWR        0x2C

#if (!LCD_SPI_REFRESH_WITH_QSPI)
spi_config_t config = {0};
#else
#if (LCD_SPI_REFRESH_WITH_QSPI_MAPPING_MODE == 1)
static beken_semaphore_t lcd_spi_semaphore = NULL;
static dma_id_t lcd_spi_dma_id = DMA_ID_MAX;
static bool dma_is_repeat_mode = false;
#endif
#endif

static uint8_t s_lcd_spi_flag = 1;
static uint8_t lcd_spi_first_disp = 1;
#if CONFIG_LCD_SPI_TE
static beken_semaphore_t lcd_spi_te_sem = NULL;
#endif
#if (CONFIG_LCD_SPI_DEVICE_NUM > 1)
static uint8_t lcd_spi_qspi_is_init = 0;
#endif

static void lcd_spi_device_gpio_init(uint8_t id)
{
    BK_LOG_ON_ERR(bk_gpio_driver_init());

    gpio_config_t config;
    config.io_mode = GPIO_OUTPUT_ENABLE;
    config.pull_mode = GPIO_PULL_DISABLE;
    config.func_mode = GPIO_SECOND_FUNC_DISABLE;

#if (CONFIG_LCD_SPI_DEVICE_NUM > 1)
    if (id == 0) {
        BK_LOG_ON_ERR(gpio_dev_unmap(LCD0_SPI_RESET_PIN));
        BK_LOG_ON_ERR(gpio_dev_unmap(LCD0_SPI_DC_PIN));
        bk_gpio_set_config(LCD0_SPI_RESET_PIN, &config);
        bk_gpio_set_config(LCD0_SPI_DC_PIN, &config);
        BK_LOG_ON_ERR(bk_gpio_set_output_high(LCD0_SPI_RESET_PIN));
        BK_LOG_ON_ERR(bk_gpio_set_output_high(LCD0_SPI_DC_PIN));
        BK_LOG_ON_ERR(bk_gpio_set_output_low(LCD0_SPI_RESET_PIN));
        rtos_delay_milliseconds(100);
        BK_LOG_ON_ERR(bk_gpio_set_output_high(LCD0_SPI_RESET_PIN));
        rtos_delay_milliseconds(120);
    } else {
        BK_LOG_ON_ERR(gpio_dev_unmap(LCD1_SPI_RESET_PIN));
        BK_LOG_ON_ERR(gpio_dev_unmap(LCD1_SPI_DC_PIN));
        bk_gpio_set_config(LCD1_SPI_RESET_PIN, &config);
        bk_gpio_set_config(LCD1_SPI_DC_PIN, &config);
        BK_LOG_ON_ERR(bk_gpio_set_output_high(LCD1_SPI_RESET_PIN));
        BK_LOG_ON_ERR(bk_gpio_set_output_high(LCD1_SPI_DC_PIN));
        BK_LOG_ON_ERR(bk_gpio_set_output_low(LCD1_SPI_RESET_PIN));
        rtos_delay_milliseconds(100);
        BK_LOG_ON_ERR(bk_gpio_set_output_high(LCD1_SPI_RESET_PIN));
        rtos_delay_milliseconds(120);
    }
#else
    BK_LOG_ON_ERR(gpio_dev_unmap(LCD_SPI_RESET_PIN));
    BK_LOG_ON_ERR(gpio_dev_unmap(LCD_SPI_DC_PIN));
    bk_gpio_set_config(LCD_SPI_RESET_PIN, &config);
    bk_gpio_set_config(LCD_SPI_DC_PIN, &config);

    BK_LOG_ON_ERR(bk_gpio_set_output_high(LCD_SPI_RESET_PIN));
    BK_LOG_ON_ERR(bk_gpio_set_output_high(LCD_SPI_DC_PIN));

    BK_LOG_ON_ERR(bk_gpio_set_output_low(LCD_SPI_RESET_PIN));
    rtos_delay_milliseconds(100);
    BK_LOG_ON_ERR(bk_gpio_set_output_high(LCD_SPI_RESET_PIN));
    rtos_delay_milliseconds(120);
#endif
}

static void lcd_spi_device_gpio_deinit(uint8_t id)
{
#if (CONFIG_LCD_SPI_DEVICE_NUM > 1)
    if (id == 0) {
        BK_LOG_ON_ERR(bk_gpio_set_output_low(LCD0_SPI_RESET_PIN));
        BK_LOG_ON_ERR(gpio_dev_unmap(LCD0_SPI_RESET_PIN));
        BK_LOG_ON_ERR(gpio_dev_unmap(LCD0_SPI_DC_PIN));
    } else {
        BK_LOG_ON_ERR(bk_gpio_set_output_low(LCD1_SPI_RESET_PIN));
        BK_LOG_ON_ERR(gpio_dev_unmap(LCD1_SPI_RESET_PIN));
        BK_LOG_ON_ERR(gpio_dev_unmap(LCD1_SPI_DC_PIN));
    }
#else
    BK_LOG_ON_ERR(bk_gpio_set_output_low(LCD_SPI_RESET_PIN));
    BK_LOG_ON_ERR(gpio_dev_unmap(LCD_SPI_RESET_PIN));
    BK_LOG_ON_ERR(gpio_dev_unmap(LCD_SPI_DC_PIN));
#endif
}


#if (LCD_SPI_REFRESH_WITH_QSPI)

static qspi_driver_t s_lcd_spi[SOC_QSPI_UNIT_NUM] = {
    {
        .hal.hw = (qspi_hw_t *)(SOC_QSPI0_REG_BASE),
    },
#if (SOC_QSPI_UNIT_NUM > 1)
    {
        .hal.hw = (qspi_hw_t *)(SOC_QSPI1_REG_BASE),
    }
#endif
};

static void lcd_spi_driver_init_with_qspi(qspi_id_t qspi_id, lcd_qspi_clk_t clk)
{
    qspi_config_t lcd_qspi_config;
    os_memset(&lcd_qspi_config, 0, sizeof(lcd_qspi_config));

    if (s_lcd_spi_flag) {
        os_memset(&s_lcd_spi, 0, sizeof(s_lcd_spi));
        s_lcd_spi_flag = 0;
    }

    s_lcd_spi[qspi_id].hal.id = qspi_id;
    qspi_hal_init(&s_lcd_spi[qspi_id].hal);

    switch (clk) {
        case LCD_QSPI_80M:
            lcd_qspi_config.src_clk = QSPI_SCLK_480M;
            lcd_qspi_config.src_clk_div = 5;
            BK_LOG_ON_ERR(bk_qspi_init(qspi_id, &lcd_qspi_config));
            break;

        case LCD_QSPI_64M:
            lcd_qspi_config.src_clk = QSPI_SCLK_320M;
            lcd_qspi_config.src_clk_div = 4;
            BK_LOG_ON_ERR(bk_qspi_init(qspi_id, &lcd_qspi_config));
            break;

        case LCD_QSPI_60M:
            lcd_qspi_config.src_clk = QSPI_SCLK_480M;
            lcd_qspi_config.src_clk_div = 7;
            BK_LOG_ON_ERR(bk_qspi_init(qspi_id, &lcd_qspi_config));
            break;

        case LCD_QSPI_53M:
            lcd_qspi_config.src_clk = QSPI_SCLK_480M;
            lcd_qspi_config.src_clk_div = 8;
            BK_LOG_ON_ERR(bk_qspi_init(qspi_id, &lcd_qspi_config));
            break;

        case LCD_QSPI_48M:
            lcd_qspi_config.src_clk = QSPI_SCLK_480M;
            lcd_qspi_config.src_clk_div = 9;
            BK_LOG_ON_ERR(bk_qspi_init(qspi_id, &lcd_qspi_config));
            break;

        case LCD_QSPI_40M:
            lcd_qspi_config.src_clk = QSPI_SCLK_480M;
            lcd_qspi_config.src_clk_div = 11;
            BK_LOG_ON_ERR(bk_qspi_init(qspi_id, &lcd_qspi_config));
            break;

        case LCD_QSPI_32M:
            lcd_qspi_config.src_clk = QSPI_SCLK_320M;
            lcd_qspi_config.src_clk_div = 9;
            BK_LOG_ON_ERR(bk_qspi_init(qspi_id, &lcd_qspi_config));
            break;

        case LCD_QSPI_30M:
            lcd_qspi_config.src_clk = QSPI_SCLK_480M;
            lcd_qspi_config.src_clk_div = 15;
            BK_LOG_ON_ERR(bk_qspi_init(qspi_id, &lcd_qspi_config));
            break;

        default:
            lcd_qspi_config.src_clk = QSPI_SCLK_480M;
            lcd_qspi_config.src_clk_div = 11;
            BK_LOG_ON_ERR(bk_qspi_init(qspi_id, &lcd_qspi_config));
            break;
    }

    qspi_hal_disable_soft_reset(&s_lcd_spi[qspi_id].hal);
    delay_us(10);
    qspi_hal_enable_soft_reset(&s_lcd_spi[qspi_id].hal);

#if (LCD_SPI_REFRESH_WITH_QSPI_MAPPING_MODE == 1)
    qspi_hal_set_cmd_a_cfg2(&s_lcd_spi[qspi_id].hal, 0x80000000);
#endif
}

static void lcd_spi_driver_deinit_with_qspi(qspi_id_t qspi_id)
{
    BK_LOG_ON_ERR(bk_qspi_deinit(qspi_id));

    s_lcd_spi_flag = 1;
}

static void lcd_spi_send_data_with_qspi_direct_mode(qspi_id_t qspi_id, uint8_t *data, uint32_t data_len)
{
    uint32_t value = 0;

    qspi_hal_set_cmd_c_l(&s_lcd_spi[qspi_id].hal, 0);
    qspi_hal_set_cmd_c_h(&s_lcd_spi[qspi_id].hal, 0);
    qspi_hal_set_cmd_c_cfg1(&s_lcd_spi[qspi_id].hal, 0);
    qspi_hal_set_cmd_c_cfg2(&s_lcd_spi[qspi_id].hal, 0);

    for (uint8_t i = 0; i < data_len; i++) {
        value |= (data[i] << (i * 8));
    }

    qspi_hal_set_cmd_c_h(&s_lcd_spi[qspi_id].hal, value);
    qspi_hal_set_cmd_c_cfg1(&s_lcd_spi[qspi_id].hal, 0x3 << (data_len * 2));
    qspi_hal_cmd_c_start(&s_lcd_spi[qspi_id].hal);
    qspi_hal_wait_cmd_done(&s_lcd_spi[qspi_id].hal);
}

static void lcd_spi_send_data_with_qspi_indirect_mode(qspi_id_t qspi_id, uint8_t *data, uint32_t data_len)
{
    uint32_t value = 0;
    uint32_t send_len = 0;
    uint32_t remain_len = data_len;
    uint8_t *data_tmp = data;

    qspi_hal_set_cmd_c_l(&s_lcd_spi[qspi_id].hal, 0);
    qspi_hal_set_cmd_c_h(&s_lcd_spi[qspi_id].hal, 0);
    qspi_hal_set_cmd_c_cfg1(&s_lcd_spi[qspi_id].hal, 0);
    qspi_hal_set_cmd_c_cfg2(&s_lcd_spi[qspi_id].hal, 0);

    while (remain_len > 0) {
        if (remain_len <= 4) {
            lcd_spi_send_data_with_qspi_direct_mode(qspi_id, data_tmp, remain_len);
            break;
        }

        value = (data_tmp[3] << 24) | (data_tmp[2] << 16) | (data_tmp[1] << 8) | data_tmp[0];
        remain_len -= 4;
        send_len = remain_len < 0x100 ? remain_len : 0x100;
        qspi_hal_set_cmd_c_h(&s_lcd_spi[qspi_id].hal, value);
        qspi_hal_set_cmd_c_cfg1(&s_lcd_spi[qspi_id].hal, 0x300);
        qspi_hal_set_cmd_c_cfg2(&s_lcd_spi[qspi_id].hal, send_len << 2);
        data_tmp += 4;
        bk_qspi_write(qspi_id, data_tmp, send_len);
        qspi_hal_cmd_c_start(&s_lcd_spi[qspi_id].hal);
        qspi_hal_wait_cmd_done(&s_lcd_spi[qspi_id].hal);
        remain_len -= send_len;
        data_tmp += send_len;
    }
}

#if (LCD_SPI_REFRESH_WITH_QSPI_MAPPING_MODE == 1)
static bk_err_t lcd_spi_get_dma_repeat_once_len(const lcd_device_t *device)
{
    uint32_t len = 0;
    uint32_t value = 0;
    uint8_t i = 0;

    for (i = 4; i < 13; i++) {
        len = device->spi->frame_len / i;
        if (len <= 0x10000) {
            value = device->spi->frame_len % i;
            if (!value) {
                return len;
            }
        }
    }

    LCD_SPI_LOGE("%s Error dma length, please check the resolution of qspi lcd\r\n", __func__);

    return len;
}

static void lcd_spi_dma_finish_isr(void)
{
    bk_err_t ret = BK_OK;

    if (dma_is_repeat_mode) {
        uint32_t value = bk_dma_get_repeat_wr_pause(lcd_spi_dma_id);
        if (value) {
            bk_dma_stop(lcd_spi_dma_id);
            ret = rtos_set_semaphore(&lcd_spi_semaphore);
            if (ret != BK_OK) {
                LCD_SPI_LOGE("%s [%d] lcd spi semaphore set failed\r\n", __func__, __LINE__);
            }
        }
    } else {
        ret = rtos_set_semaphore(&lcd_spi_semaphore);
        if (ret != BK_OK) {
            LCD_SPI_LOGE("%s [%d] lcd spi semaphore set failed\r\n", __func__, __LINE__);
        }
    }
}

bk_err_t lcd_spi_quad_write_start(qspi_id_t qspi_id)
{
    qspi_hal_set_cmd_c_l(&s_lcd_spi[qspi_id].hal, 0);
    qspi_hal_set_cmd_c_h(&s_lcd_spi[qspi_id].hal, 0);
    qspi_hal_set_cmd_c_cfg1(&s_lcd_spi[qspi_id].hal, 0);
    qspi_hal_set_cmd_c_cfg2(&s_lcd_spi[qspi_id].hal, 0);

    qspi_hal_force_spi_cs_low_enable(&s_lcd_spi[qspi_id].hal);

#if (CONFIG_SOC_BK7236XX)
    qspi_hal_io_cpu_mem_select(&s_lcd_spi[qspi_id].hal, 1);
#endif

    qspi_hal_disable_cmd_sck_enable(&s_lcd_spi[qspi_id].hal);

    return BK_OK;
}

bk_err_t lcd_spi_quad_write_stop(qspi_id_t qspi_id)
{
    qspi_hal_disable_cmd_sck_disable(&s_lcd_spi[qspi_id].hal);
    qspi_hal_force_spi_cs_low_disable(&s_lcd_spi[qspi_id].hal);

#if (CONFIG_SOC_BK7236XX)
    qspi_hal_io_cpu_mem_select(&s_lcd_spi[qspi_id].hal, 0);
#endif

    return BK_OK;
}

static void lcd_spi_dma_config(qspi_id_t qspi_id, uint8_t *data, uint32_t data_len)
{
    bk_err_t ret = BK_OK;

    dma_config_t dma_config = {0};
    dma_config.mode = DMA_WORK_MODE_SINGLE;
    dma_config.chan_prio = 0;

    dma_config.src.dev = DMA_DEV_DTCM;
    dma_config.src.width = DMA_DATA_WIDTH_32BITS;
    dma_config.src.addr_inc_en = DMA_ADDR_INC_ENABLE;
    dma_config.src.start_addr = (uint32_t)data;
    dma_config.src.end_addr = (uint32_t)(data + data_len);

    dma_config.dst.dev = DMA_DEV_DTCM;
    dma_config.dst.width = DMA_DATA_WIDTH_32BITS;
    dma_config.dst.addr_inc_en = DMA_ADDR_INC_ENABLE;

    if (qspi_id == QSPI_ID_0) {
        dma_config.dst.start_addr = (uint32_t)LCD_QSPI0_DATA_ADDR;
        dma_config.dst.end_addr = (uint32_t)(LCD_QSPI0_DATA_ADDR + data_len);
    } else if (qspi_id == QSPI_ID_1) {
        dma_config.dst.start_addr = (uint32_t)LCD_QSPI1_DATA_ADDR;
        dma_config.dst.end_addr = (uint32_t)(LCD_QSPI1_DATA_ADDR + data_len);
    }

    ret = bk_dma_init(lcd_spi_dma_id, &dma_config);
    if (ret != BK_OK) {
        LCD_SPI_LOGE("bk_dma_init failed!\r\n");
        return;
    }

    LCD_SPI_LOGD("%s data_len = %d\r\n", __func__, data_len);
    bk_dma_set_transfer_len(lcd_spi_dma_id, data_len);
    bk_dma_register_isr(lcd_spi_dma_id, NULL, (void *)lcd_spi_dma_finish_isr);
    bk_dma_enable_finish_interrupt(lcd_spi_dma_id);
}

static void lcd_spi_send_data_with_qspi_mapping_mode(qspi_id_t qspi_id, uint8_t *data, uint32_t data_len)
{
    bk_err_t ret = BK_OK;

    if (dma_is_repeat_mode) {
        if (qspi_id == QSPI_ID_0) {
            dma_set_dst_pause_addr(lcd_spi_dma_id, LCD_QSPI0_DATA_ADDR + data_len);
            bk_dma_stateless_judgment_configuration((void *)LCD_QSPI0_DATA_ADDR, (void *)data, data_len, lcd_spi_dma_id, (void *)lcd_spi_dma_finish_isr);
        } else if (qspi_id == QSPI_ID_1) {
            dma_set_dst_pause_addr(lcd_spi_dma_id, LCD_QSPI1_DATA_ADDR + data_len);
            bk_dma_stateless_judgment_configuration((void *)LCD_QSPI1_DATA_ADDR, (void *)data, data_len, lcd_spi_dma_id, (void *)lcd_spi_dma_finish_isr);
        } else {
            LCD_SPI_LOGE("unsupported lcd qspi id\r\n");
            return;
        }
        dma_set_src_pause_addr(lcd_spi_dma_id, (uint32_t)data + data_len);
    } else {
        lcd_spi_dma_config(qspi_id, data, data_len);
    }

    lcd_spi_quad_write_start(qspi_id);

    bk_dma_start(lcd_spi_dma_id);

    ret = rtos_get_semaphore(&lcd_spi_semaphore, 3000);
    if (ret != kNoErr) {
        LCD_SPI_LOGE("ret = %d, lcd qspi get semaphore failed!\r\n", ret);
        return;
    }
    delay_us(15);
    lcd_spi_quad_write_stop(qspi_id);
}

static void lcd_spi_dma_init(uint8_t id, const lcd_device_t *device)
{
    bk_err_t ret = BK_OK;
    uint32_t dma_repeat_once_len = 0;

    ret = rtos_init_semaphore(&lcd_spi_semaphore, 1);
    if (ret != kNoErr) {
        LCD_SPI_LOGE("lcd spi semaphore init failed.\r\n");
        return;
    }

    ret = bk_dma_driver_init();
    if (ret != BK_OK) {
        LCD_SPI_LOGE("dma driver init failed!\r\n");
        return;
    }

    lcd_spi_dma_id = bk_dma_alloc(DMA_DEV_DTCM);
    if ((lcd_spi_dma_id < DMA_ID_0) || (lcd_spi_dma_id >= DMA_ID_MAX)) {
        LCD_SPI_LOGE("lcd spi dma id malloc failed!\r\n");
        return;
    }

#if (CONFIG_SPE)
    bk_dma_set_src_sec_attr(lcd_spi_dma_id, DMA_ATTR_SEC);
    bk_dma_set_dest_sec_attr(lcd_spi_dma_id, DMA_ATTR_SEC);
    bk_dma_set_dest_burst_len(lcd_spi_dma_id, BURST_LEN_INC16);
    bk_dma_set_src_burst_len(lcd_spi_dma_id, BURST_LEN_INC16);
#endif

    if (device->spi->frame_len > 0x10000) {
        dma_is_repeat_mode = true;
        dma_repeat_once_len = lcd_spi_get_dma_repeat_once_len(device);
    } else {
        dma_is_repeat_mode = false;
        dma_repeat_once_len = device->spi->frame_len;
    }

    LCD_SPI_LOGI("dma_repeat_once_len = %d\r\n", dma_repeat_once_len);
    bk_dma_set_transfer_len(lcd_spi_dma_id, dma_repeat_once_len);
}

static void lcd_spi_dma_deinit(void)
{
    bk_err_t ret = BK_OK;

    bk_dma_stop(lcd_spi_dma_id);
    bk_dma_free(DMA_DEV_DTCM, lcd_spi_dma_id);
//    BK_LOG_ON_ERR(bk_dma_driver_deinit());

    ret = rtos_deinit_semaphore(&lcd_spi_semaphore);
    if (ret != kNoErr) {
        LCD_SPI_LOGE("lcd qspi semaphore deinit failed.\r\n");
        return;
    }
}
#endif

static void lcd_spi_send_cmd_with_qspi(qspi_id_t qspi_id, uint8_t cmd)
{
    qspi_hal_set_cmd_c_l(&s_lcd_spi[qspi_id].hal, 0);
    qspi_hal_set_cmd_c_h(&s_lcd_spi[qspi_id].hal, 0);
    qspi_hal_set_cmd_c_cfg1(&s_lcd_spi[qspi_id].hal, 0);
    qspi_hal_set_cmd_c_cfg2(&s_lcd_spi[qspi_id].hal, 0);

    qspi_hal_set_cmd_c_h(&s_lcd_spi[qspi_id].hal, cmd);
    qspi_hal_set_cmd_c_cfg1(&s_lcd_spi[qspi_id].hal, 0xC);
    qspi_hal_cmd_c_start(&s_lcd_spi[qspi_id].hal);
    qspi_hal_wait_cmd_done(&s_lcd_spi[qspi_id].hal);
}

static void lcd_spi_send_data_with_qspi(qspi_id_t qspi_id, uint8_t *data, uint32_t data_len)
{
#if (LCD_SPI_REFRESH_WITH_QSPI_MAPPING_MODE == 1)
    if (data_len <= 0x100) {
        lcd_spi_send_data_with_qspi_indirect_mode(qspi_id, data, data_len);
    } else {
        lcd_spi_send_data_with_qspi_mapping_mode(qspi_id, data, data_len);
    }
#else
    lcd_spi_send_data_with_qspi_indirect_mode(qspi_id, data, data_len);
#endif
}
#endif

static void lcd_spi_send_cmd(uint8_t id, uint8_t cmd)
{
#if (CONFIG_LCD_SPI_DEVICE_NUM > 1)
    if (id == 0) {
        BK_LOG_ON_ERR(bk_gpio_set_output_low(LCD0_SPI_DC_PIN));
    } else {
        BK_LOG_ON_ERR(bk_gpio_set_output_low(LCD1_SPI_DC_PIN));
    }
#else
    BK_LOG_ON_ERR(bk_gpio_set_output_low(LCD_SPI_DC_PIN));
#endif

#if LCD_SPI_REFRESH_WITH_QSPI
    lcd_spi_send_cmd_with_qspi(id, cmd);
#else
    bk_spi_write_bytes(id, &cmd, 1);
#endif
}

static void lcd_spi_send_data(uint8_t id, uint8_t *data, uint32_t data_len)
{
#if (CONFIG_LCD_SPI_DEVICE_NUM > 1)
    if (id == 0) {
        BK_LOG_ON_ERR(bk_gpio_set_output_high(LCD0_SPI_DC_PIN));
    } else {
        BK_LOG_ON_ERR(bk_gpio_set_output_high(LCD1_SPI_DC_PIN));
    }
#else
    BK_LOG_ON_ERR(bk_gpio_set_output_high(LCD_SPI_DC_PIN));
#endif

#if LCD_SPI_REFRESH_WITH_QSPI
    lcd_spi_send_data_with_qspi(id, data, data_len);
#else

#if CONFIG_SPI_DMA
    if (data_len > 32) {
        bk_spi_dma_write_bytes(id, data, data_len);
    } else {
        bk_spi_write_bytes(id, data, data_len);
    }
#else
    bk_spi_write_bytes(id, data, data_len);
#endif
#endif
}

#if (!LCD_SPI_REFRESH_WITH_QSPI)
static void lcd_spi_driver_init(spi_id_t id)
{
    bk_spi_driver_init();

    config.role = SPI_ROLE_MASTER;
    config.bit_width = SPI_BIT_WIDTH_8BITS;
    config.polarity = SPI_POLARITY_HIGH;
    config.phase = SPI_PHASE_2ND_EDGE;
    config.wire_mode = SPI_4WIRE_MODE;
    config.baud_rate = 30000000;
    config.bit_order = SPI_MSB_FIRST;

#if CONFIG_SPI_DMA
    config.dma_mode = SPI_DMA_MODE_ENABLE;
    config.spi_tx_dma_chan = bk_dma_alloc(DMA_DEV_GSPI0);
    config.spi_rx_dma_chan = bk_dma_alloc(DMA_DEV_GSPI0_RX);
    config.spi_tx_dma_width = DMA_DATA_WIDTH_8BITS;
    config.spi_rx_dma_width = DMA_DATA_WIDTH_8BITS;
#else
    config.dma_mode = SPI_DMA_MODE_DISABLE;
#endif

    BK_LOG_ON_ERR(bk_spi_init(id, &config));
}

static void lcd_spi_driver_deinit(spi_id_t id)
{
    BK_LOG_ON_ERR(bk_spi_deinit(id));

#if CONFIG_SPI_DMA
    bk_dma_free(DMA_DEV_GSPI0, config.spi_tx_dma_chan);
    bk_dma_free(DMA_DEV_GSPI0_RX, config.spi_rx_dma_chan);
#endif
}
#endif

void lcd_spi_backlight_open(void)
{
    BK_LOG_ON_ERR(gpio_dev_unmap(LCD_SPI_BACKLIGHT_PIN));
    gpio_config_t config;
    config.io_mode = GPIO_OUTPUT_ENABLE;
    config.pull_mode = GPIO_PULL_DISABLE;
    config.func_mode = GPIO_SECOND_FUNC_DISABLE;
    bk_gpio_set_config(LCD_SPI_BACKLIGHT_PIN, &config);
    BK_LOG_ON_ERR(bk_gpio_set_output_high(LCD_SPI_BACKLIGHT_PIN));
}

void lcd_spi_backlight_close(void)
{
    BK_LOG_ON_ERR(bk_gpio_set_output_low(LCD_SPI_BACKLIGHT_PIN));
    bk_gpio_disable_output(LCD_SPI_BACKLIGHT_PIN);
    BK_LOG_ON_ERR(gpio_dev_unmap(LCD_SPI_BACKLIGHT_PIN));
}

#if CONFIG_LCD_SPI_TE
static void lcd_spi_te_int_isr(gpio_id_t id)
{
    if (lcd_spi_te_sem && id == LCD_SPI_TE_PIN)
    {
        rtos_set_semaphore(&lcd_spi_te_sem);
    }
}

static bk_err_t lcd_spi_te_init(void)
{
    bk_err_t ret = BK_OK;

    ret = rtos_init_semaphore(&lcd_spi_te_sem, 1);
    if (ret != kNoErr)
    {
        LCD_SPI_LOGE("%s lcd_spi_te_sem init failed\r\n", __func__);
        return ret;
    }

    gpio_config_t config;
    config.io_mode = GPIO_INPUT_ENABLE;
    config.pull_mode = GPIO_PULL_UP_EN;
    config.func_mode = GPIO_SECOND_FUNC_DISABLE;

    BK_LOG_ON_ERR(gpio_dev_unmap(LCD_SPI_TE_PIN));
    bk_gpio_set_config(LCD_SPI_TE_PIN, &config);

    int int_type = GPIO_INT_TYPE_FALLING_EDGE;
    bk_gpio_register_isr(LCD_SPI_TE_PIN , lcd_spi_te_int_isr);
    BK_LOG_ON_ERR(bk_gpio_set_interrupt_type(LCD_SPI_TE_PIN, int_type));
    bk_gpio_enable_interrupt(LCD_SPI_TE_PIN);

    return ret;
}

static bk_err_t lcd_spi_te_deinit(void)
{
    bk_err_t ret = BK_OK;

    bk_gpio_disable_interrupt(LCD_SPI_TE_PIN);
    BK_LOG_ON_ERR(gpio_dev_unmap(LCD_SPI_TE_PIN));

    ret = rtos_deinit_semaphore(&lcd_spi_te_sem);
    if (ret != kNoErr)
    {
        LCD_SPI_LOGE("%s lcd_spi_te_sem deinit failed\r\n", __func__);
        return ret;
    }

    return ret;
}

void lcd_spi_te_wait(uint32_t wait_ms)
{
    if (lcd_spi_te_sem)
    {
        bk_err_t ret = rtos_get_semaphore(&lcd_spi_te_sem, wait_ms);
        if (ret != kNoErr)
        {
            LCD_SPI_LOGE("lcd_spi_te_sem get failed\r\n");
        }
    }
}
#endif

void lcd_spi_init(uint8_t id, const lcd_device_t *device)
{
    if (device == NULL) {
        LCD_SPI_LOGE("lcd spi device not found\r\n");
        return;
    }

#if LCD_SPI_REFRESH_WITH_QSPI
#if (CONFIG_LCD_SPI_DEVICE_NUM > 1)
    if (lcd_spi_qspi_is_init == 0) {
        lcd_spi_driver_init_with_qspi(LCD_SPI_ID0, device->spi->clk);
        lcd_spi_driver_init_with_qspi(LCD_SPI_ID1, device->spi->clk);
        #if (LCD_SPI_REFRESH_WITH_QSPI_MAPPING_MODE == 1)
            lcd_spi_dma_init(id, device);
        #endif
        lcd_spi_qspi_is_init = 1;
    }
#else //(CONFIG_LCD_SPI_DEVICE_NUM > 1)
    lcd_spi_driver_init_with_qspi(id, device->spi->clk);
    #if (LCD_SPI_REFRESH_WITH_QSPI_MAPPING_MODE == 1)
        lcd_spi_dma_init(id, device);
    #endif
#endif

#else // LCD_SPI_REFRESH_WITH_QSPI
    lcd_spi_driver_init(id);
#endif
    lcd_spi_device_gpio_init(id);

    if (device->spi->init_cmd != NULL) {
        const lcd_qspi_init_cmd_t *init = device->spi->init_cmd;
        for (uint32_t i = 0; i < device->spi->device_init_cmd_len; i++) {
            if (init->data_len == 0xFF) {
                rtos_delay_milliseconds(init->data[0]);
            } else {
                lcd_spi_send_cmd(id, init->cmd);
                if (init->data_len != 0) {
                    lcd_spi_send_data(id, (uint8_t *)init->data, init->data_len);
                }
            }
            init++;
        }
    } else {
        LCD_SPI_LOGE("lcd spi device init cmd is null\r\n");
        return;
    }

#if CONFIG_LCD_SPI_TE
    lcd_spi_te_init();
#endif

    LCD_SPI_LOGI("%s[%d] is complete\r\n", __func__, id);
}

void lcd_spi_deinit(uint8_t id)
{
    if (lcd_spi_first_disp == 0) {
        lcd_spi_backlight_close();
        lcd_spi_first_disp = 1;
    }

#if CONFIG_LCD_SPI_TE
    lcd_spi_te_deinit();
#endif

    lcd_spi_device_gpio_deinit(id);

#if LCD_SPI_REFRESH_WITH_QSPI
#if (LCD_SPI_REFRESH_WITH_QSPI_MAPPING_MODE == 1)
    #if (CONFIG_LCD_SPI_DEVICE_NUM > 1)
        if (lcd_spi_qspi_is_init == 1) {
            lcd_spi_dma_deinit();
            lcd_spi_qspi_is_init = 0;
        }
    #else
        lcd_spi_dma_deinit();
    #endif
#endif

    lcd_spi_driver_deinit_with_qspi(id);
    #if (CONFIG_LCD_SPI_DEVICE_NUM > 1)
        lcd_spi_qspi_is_init = 0;
    #endif
#else
    lcd_spi_driver_deinit(id);
#endif

    LCD_SPI_LOGI("%s is complete\r\n", __func__);
}

void lcd_spi_display_frame(uint8_t id, uint8_t *frame_buffer, uint32_t width, uint32_t height)
{
    uint8_t column_value[4] = {0};
    uint8_t row_value[4] = {0};
    column_value[2] = ((width - 1) >> 8) & 0xFF;
    column_value[3] = (width - 1) & 0xFF;
    row_value[2] = ((height - 1) >> 8) & 0xFF;
    row_value[3] = (height - 1) & 0xFF;

    if (lcd_spi_first_disp == 1) {
        lcd_spi_backlight_open();
        lcd_spi_first_disp = 0;
    }

    lcd_spi_send_cmd(id, LCD_SPI_DEVICE_CASET);
    lcd_spi_send_data(id, column_value, 4);
    lcd_spi_send_cmd(id, LCD_SPI_DEVICE_RASET);
    lcd_spi_send_data(id, row_value, 4);

    lcd_spi_send_cmd(id, LCD_SPI_DEVICE_RAMWR);
    lcd_spi_send_data(id, frame_buffer, width * height * 2);
}

void lcd_spi_set_display_area(uint8_t id, uint16_t x_start, uint16_t x_end, uint16_t y_start, uint16_t y_end)
{
    uint8_t column_value[4] = {0};
    uint8_t row_value[4] = {0};

    if (lcd_spi_first_disp == 1) {
#if (LCD_SPI_REFRESH_WITH_QSPI_MAPPING_MODE == 1)
        dma_is_repeat_mode = false;
#endif
        lcd_spi_backlight_open();
        lcd_spi_first_disp = 0;
    }

    column_value[0] = (x_start >> 8) & 0xFF;
    column_value[1] = x_start & 0xFF;
    column_value[2] = (x_end >> 8) & 0xFF;
    column_value[3] = x_end & 0xFF;
    row_value[0] = (y_start >> 8) & 0xFF;
    row_value[1] = y_start & 0xFF;
    row_value[2] = (y_end >> 8) & 0xFF;
    row_value[3] = y_end & 0xFF;

    lcd_spi_send_cmd(id, LCD_SPI_DEVICE_CASET);
    lcd_spi_send_data(id, column_value, 4);
    lcd_spi_send_cmd(id, LCD_SPI_DEVICE_RASET);
    lcd_spi_send_data(id, row_value, 4);

    lcd_spi_send_cmd(id, LCD_SPI_DEVICE_RAMWR);
}

bk_err_t lcd_spi_partial_display(uint8_t id, uint8_t *disp_buffer, uint32_t data_len)
{
    lcd_spi_send_data(id, disp_buffer, data_len);

    return BK_OK;
}
