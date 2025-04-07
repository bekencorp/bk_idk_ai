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

#pragma once

#include <components/log.h>
#include <components/usb.h>
#include <components/usb_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#if (CONFIG_CPU_CNT > 1)

#if (CONFIG_SYS_CPU1 && CONFIG_USB_CDC && CONFIG_USB_CDC_MODEM)
#define USB_CDC_CP1_IPC 1
#else
#define USB_CDC_CP1_IPC 0
#endif

#if (CONFIG_SYS_CPU0 && !CONFIG_USB_CDC && CONFIG_USB_CDC_MODEM)
#define USB_CDC_CP0_IPC 1
#else
#define USB_CDC_CP0_IPC 0
#endif

#endif

#define USB_CDC_DEV_MAX_NUM (1)

#define MAX_BULK_TRS_SIZE (1024)
#define CDC_TX_MAX_SIZE     512
#define CDC_RX_MAX_SIZE     512

#define CDC_EXTX_MAX_SIZE     2048
#define CDC_EXRX_MAX_SIZE     2048

typedef enum
{
	CDC_ACM_AT_MODE = 0,
	CDC_ACM_DATA_MODE, //1
} E_CDC_MODE_T;

typedef enum
{
	CDC_STATUS_OPEN,
	CDC_STATUS_CLOSE,
	CDC_STATUS_CONN,
	CDC_STATUS_DISCON,
	CDC_STATUS_BULKIN,
	CDC_STATUS_BULKOUT,
	CDC_STATUS_BULKIN_DONE,
	CDC_STATUS_UPDATE_PARAM,
	CDC_STATUS_OUT_DELAY,
	CDC_STATUS_ABNORMAL,
	CDC_STATUS_IDLE,
} E_CDC_STATUS_T;

typedef enum
{
	MODEM_USB_CONN,
	MODEM_USB_DISCONN,
	MODEM_USB_IDLE,

}BK_MODEM_USB_STATE_T;

typedef struct {
	uint8_t  type;
	uint32_t data;
}cdc_msg_t;


typedef struct
{
	uint32_t idx; //port_idx OR acm_dev_idx ???
	E_CDC_MODE_T mode;// Command/Data
	uint32_t tx_len;
	uint32_t tx_data;

	uint32_t rx_len;
	uint32_t rx_data;

	uint32_t cmd_len;
	uint32_t state;

	void (*bk_cdc_acm_bulkin_cb)(uint32_t idx);

	uint32_t rx_valid;
	uint32_t tx_valid;
}IPC_CDC_DATA_t;


typedef struct
{
	uint8_t *rx_buf[USB_CDC_DEV_MAX_NUM];
	uint32_t l_rx[USB_CDC_DEV_MAX_NUM];
	uint8_t *tx_buf[USB_CDC_DEV_MAX_NUM];
	uint32_t l_tx[USB_CDC_DEV_MAX_NUM];
	E_CDC_MODE_T acm_mode[USB_CDC_DEV_MAX_NUM];
}Multi_ACM_DEVICE_EX_T;


#ifdef __cplusplus
}
#endif
