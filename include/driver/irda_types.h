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

#ifdef __cplusplus
extern "C" {
#endif

#define BK_ERR_IRDA_NOT_INIT              (BK_ERR_IRDA_BASE - 1) /**< IRDA driver not init */
#define BK_ERR_IRDA_RESOURCE_BUSY         (BK_ERR_IRDA_BASE - 2) /**< IRDA resource is occupied*/

typedef struct {
	uint32_t clk_freq_input : 7;        /**< Clock Divison Factor,IRDA default clock is 26Mhz */
	uint32_t carrier_period_cycle : 8; /**< carrier period cycle, unit:1us */
	uint32_t carrier_duty_cycle : 7;   /**< carrier duty cycle, unit:1us */
	uint32_t tx_initial_level : 1;   /*tx initial level*/
	uint32_t reserved        :  9;
} irda_tx_init_config_t;

typedef struct {
	uint16_t rx_timeout_us;                /**< The system determines that data transmission has concluded when no signal edge
											transitions are detected on the RX for a continuous duration rx_timeout_us, unit:1us*/
	
	uint32_t clk_freq_input : 7;          /** < Clock Divison Factor,IRDA default clock is 26Mhz*/
	uint32_t rx_start_threshold_us : 12;  /**< the first data packet exceed rx_start_threshold as the beginning os the transfer, unit:1us*/
	uint32_t rx_initial_level : 1;       /** <rx initial level*/
	uint32_t reserved        :  12;
} irda_rx_init_config_t;

#ifdef __cplusplus
}
#endif

