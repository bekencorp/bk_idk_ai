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

#define USB_CDC_ACM_DEV_NUM_MAX  (4)
#define USB_CDC_DATA_DEV_NUM_MAX (4)

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
	CDC_STATUS_OPEN = 0,
	CDC_STATUS_CLOSE,
	CDC_STATUS_CONN,         /// 2
	CDC_STATUS_DISCON,       /// 3
	CDC_STATUS_INIT_PARAM,   /// 4
	CDC_STATUS_BULKOUT_CMD,
	CDC_STATUS_BULKOUT_DATA,

	CDC_STATUS_BULKIN_CMD,
	CDC_STATUS_BULKIN_DATA, //8

	CDC_STATUS_BULKIN_CMD_DONE,
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
	uint32_t *param;
}cdc_msg_t;

#if 1

typedef enum
{
	/* cp0 ---> cp1 */
	CPU0_OPEN_USB_CDC = 0,
	CPU0_CLOSE_USB_CDC,
	CPU0_INIT_USB_CDC_PARAM,

	CPU0_BULKOUT_USB_CDC_CMD,
	CPU0_BULKOUT_USB_CDC_DATA,

	/* cp1 ---> cp0 */
	CPU1_UPDATE_USB_CDC_STATE,
	CPU1_UPLOAD_USB_CDC_CMD,   ///6
	CPU1_UPLOAD_USB_CDC_DATA,  ///7
	CPU1_NOTIFY_RESTORE,   ///8
}IPC_CDC_SUBMSG_TYPE_T;

typedef struct
{
	uint32_t dev_cnt;
	E_CDC_STATUS_T status;
}CDC_STATUS_t;


typedef struct
{
	uint32_t l_tx;
	uint8_t *tx_buf;   /// 2048Bytes
}CDC_CIRBUF_CMD_TX_T;   //cp0 alloc

typedef struct
{
	uint32_t l_rx;
	uint8_t *rx_buf;   /// 512Bytes
}CDC_CIRBUF_CMD_RX_T;   //cp1 alloc

typedef struct
{
	CDC_CIRBUF_CMD_RX_T *p_cdc_cmd_rx;   //cp1 alloc
	CDC_CIRBUF_CMD_TX_T *p_cdc_cmd_tx;   //cp0 alloc
}Multi_ACM_DEVICE_CMD_T;   

#define CDC_CIRBUFFER_IN  1
#define CDC_CIRBUFFER_OUT 1

#define CDC_RX_CIRBUFFER_NUM   32
#define CDC_TX_CIRBUFFER_NUM   48 //16

typedef struct
{
	uint32_t len;
	uint8_t *data;
}CDC_CIRBUF_RX_ELE_T;

typedef struct
{
	volatile uint8_t wd;
	volatile uint8_t rd;
	CDC_CIRBUF_RX_ELE_T *data[CDC_RX_CIRBUFFER_NUM];
}CDC_CIRBUF_DATA_RX_T;

typedef struct
{
	uint32_t len;
	uint8_t *data;
}CDC_CIRBUF_TX_ELE_T;

typedef struct
{
	volatile uint8_t wd;
	volatile uint8_t rd;
	CDC_CIRBUF_TX_ELE_T *data[CDC_TX_CIRBUFFER_NUM];   /// 2048
}CDC_CIRBUF_DATA_TX_T;


typedef struct
{
	CDC_CIRBUF_DATA_RX_T *p_cdc_data_rx;   //cp1 alloc
	CDC_CIRBUF_DATA_TX_T *p_cdc_data_tx;   //cp0 alloc
}Multi_ACM_DEVICE_DATA_T;


typedef struct
{
	uint8_t idx;
	uint8_t mode;

	CDC_STATUS_t *p_status;
	Multi_ACM_DEVICE_CMD_T *p_cmd;
	Multi_ACM_DEVICE_DATA_T *p_data;
}Multi_ACM_DEVICE_TOTAL_T;

typedef struct
{
	IPC_CDC_SUBMSG_TYPE_T msg;
	uint32_t cmd_len;
	uint32_t p_info;   /// Multi_ACM_DEVICE_TOTAL_T *p     p_info = p
}IPC_CDC_DATA_T;

#endif

#ifdef __cplusplus
}
#endif
