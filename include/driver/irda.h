// Copyright 2023-2024 Beken
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

#pragma once

#include <common/bk_include.h>
#include <driver/irda_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief	  Init the IRDA driver
 *
 * This API init the resoure common:
 *   - Power up the IRDA
 *   - Map the IRDA to dedicated GPIO port
 *	 - Init IRDA driver control memory
 *	 - Register ISR to NVIC...
 *
 * @attention 1. This API should be called before any other IRDA APIs.
 *
 * @return
 *	  - BK_OK: succeed
 *	  - others: other errors.
 */
bk_err_t bk_irda_driver_init(void);

/**
 * @brief     Deinit the IRDA driver
 *
 * This API free all resource related to IRDA.
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_irda_driver_deinit(void);

/**
 * @brief     Init the IRDA tx
 *
 * This API init the IRDA tx:
 *  - Set the IRDA clock frequent, carrier_period_cycle, carrier_duty_cycle, tx_initial_level
 *  - Start the IRDA tx
 *
 * @param tx_config tx config 
 *
 * @return
 *    - BK_ERR_IRDA_NOT_INIT: IRDA driver not init
 *    - BK_ERR_NULL_PARAM: IRDA tx_config parameter is NULL
 *    - BK_OK: succeed
 *    - others: other errors.
 * 
 * TODO:software carrier to be implemented
 */
bk_err_t bk_irda_init_tx(irda_tx_init_config_t *tx_config);

/**
 * @brief     Init the IRDA rx
 *
 * This API init the IRDA rx:
 *  - Set the IRDA clock frequent, rx_timeout_us, rx_start_threshold_us, tx_initial_level
 *  - Start the IRDA rx
 *
 * @param rx_config rx config 
 *
 * @return
 *    - BK_ERR_IRDA_NOT_INIT: IRDA driver not init
 *    - BK_ERR_NULL_PARAM: IRDA rx_config parameter is NULL
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_irda_init_rx(irda_rx_init_config_t *rx_config);

/**
 * @brief     Send uint16_t data to IRDA buffer from a given buffer and length
 *
 * @param data data buffer address
 * @param size The number of array elements
 *
 * @return
 *    - BK_ERR_IRDA_NOT_INIT: IRDA driver not init
 *    - BK_ERR_NULL_PARAM: data parameter is NULL
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_irda_write_data(const void *data, uint32_t size);

/**
 * @brief     read uint16_t data from IRDA buffer
 * 
 *
 * @param data pointer to the uint16_t buffer
 * @param size The number of array elements
 *
 * @return
 *    - BK_ERR_IRDA_NOT_INIT: IRDA driver not init
 *    - BK_ERR_NULL_PARAM: data and size is NULL
 *    - BK_ERR_IRDA_RESOURCE_BUSY: irda resource busy
 *    - others (>=0): The number of bytes read from IRDA FIFO 
 * 
 * TODO:optimize read_data,Currently the buffer is full before the data can be read.
 */
int bk_irda_read_data(void *data, uint32_t size);

#ifdef __cplusplus
}
#endif