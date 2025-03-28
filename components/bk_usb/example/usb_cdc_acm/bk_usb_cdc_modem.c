
#include "cli.h"
#include <os/mem.h>
#include <os/str.h>
#include <os/os.h>
#include <driver/int.h>
#include <common/bk_err.h>
#include <common/bk_include.h>

#include "mb_ipc_cmd.h"
#include "bk_usb_cdc_modem.h"

#include <driver/pwr_clk.h>
#include "amp_lock_api.h"

#define TAG "cdc_modem"

#define LOGI(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

IPC_CDC_DATA_t dbg_cdc_ipc[2];

static beken_queue_t cdc_msg_queue = NULL;
static beken_thread_t cdc_demo_task = NULL;

static uint8_t __maybe_unused g_modem_mode = 0;
static uint8_t g_cdc_tx_valid = 0;
static uint8_t g_cdc_rx_valid = 0;
static uint8_t *g_tx_buf_temp = NULL;
static Multi_ACM_DEVICE_EX_T g_multi_acm_ex = {0};

void (*usb_cdc_state_cb)(uint32_t);
void bk_usb_cdc_connect_init_cb(void (*cb)(uint32_t))
{
	if (!usb_cdc_state_cb) {
		usb_cdc_state_cb = cb;
	}
}

static bk_err_t cdc_send_msg(uint8_t type, uint32_t param)
{
	bk_err_t ret = kNoErr;
	cdc_msg_t msg;

	if (cdc_msg_queue)
	{
		msg.type = type;
		msg.data = param;

		ret = rtos_push_to_queue(&cdc_msg_queue, &msg, BEKEN_NO_WAIT);
		if (kNoErr != ret)
		{
			LOGE("cdc_send_msg Fail, ret:%d\n", ret);
			return kNoResourcesErr;
		}
		return ret;
	}
	return kGeneralErr;
}

#if (CONFIG_BK_MODEM)
extern void bk_modem_usbh_conn_ind(void);
extern void bk_modem_usbh_disconn_ind(void);
extern void bk_modem_usbh_close(void);
extern void bk_modem_usbh_bulkout_ind(char *p_tx, uint32_t l_tx);
extern void bk_modem_usbh_bulkin_ind(uint8_t *p_rx, uint32_t l_rx);
extern void bk_modem_usbh_poweron_ind(void);
extern uint8 bk_modem_get_mode(void);
#else

void bk_modem_usbh_conn_ind(void){ }
void bk_modem_usbh_disconn_ind(void){ }
void bk_modem_usbh_close(void){ }
void bk_modem_usbh_bulkout_ind(char *p_tx, uint32_t l_tx){ }
void bk_modem_usbh_bulkin_ind(uint8_t *p_rx, uint32_t l_rx){ }
void bk_modem_usbh_poweron_ind(void){ }
uint8 bk_modem_get_mode(void){}

#endif


#if (USB_CDC_CP0_IPC)

extern void ipc_cdc_send_cmd(u8 cmd, u8 *cmd_buf, u16 cmd_len, u8 * rsp_buf, u16 rsp_buf_len);

static int32_t bk_usb_cdc_get_txvalid(uint32_t len)
{
	if (g_cdc_tx_valid == 1) {
		cdc_send_msg(CDC_STATUS_OUT_DELAY, len);
		return 0;
	} else {
        amp_res_acquire(AMP_RES_ID_USB_CDC, 2);
		g_cdc_tx_valid = 1;
        amp_res_release(AMP_RES_ID_USB_CDC);
		return 1;
	}
}


void bk_usb_cdc_open(void)
{
	ipc_cdc_send_cmd(IPC_CPU0_OPEN_USB_CDC, NULL, 0, NULL, 0);
}

void bk_usb_cdc_close(void)
{
	ipc_cdc_send_cmd(IPC_CPU0_CLOSE_USB_CDC, NULL, 0, NULL, 0);
}

void bk_cdc_acm_bulkout(IPC_CDC_DATA_t *ipc_cdc)
{
	ipc_cdc_send_cmd(IPC_CPU0_SET_USB_CDC_CMD, (uint8_t *)ipc_cdc, ipc_cdc->cmd_len, NULL, 0);
}

void bk_usb_cdc_param_init(IPC_CDC_DATA_t *ipc_cdc)
{
	ipc_cdc_send_cmd(IPC_CPU0_INIT_USB_CDC_PARAM, (uint8_t *)ipc_cdc, ipc_cdc->cmd_len, NULL, 0);
}

int32_t bk_cdc_acm_modem_write(char *p_tx, uint32_t l_tx)
{
	if (l_tx > CDC_EXTX_MAX_SIZE) {
		LOGE("[+]%s, Transbuf overflow!\r\n", __func__);
	}

	g_modem_mode = bk_modem_get_mode();

	if (bk_usb_cdc_get_txvalid(l_tx) == 1 && g_tx_buf_temp)
	{
		os_memcpy(g_tx_buf_temp, p_tx, l_tx);
		cdc_send_msg(CDC_STATUS_BULKOUT, l_tx);
	}
	return 0;
}

#else
extern void bk_cdc_acm_bulkout(IPC_CDC_DATA_t *ipc_cdc);
extern void bk_usb_cdc_param_init(IPC_CDC_DATA_t *p_cdc_data);

#endif

static void bk_cdc_acm_bulkin_cb(uint32_t idx)
{
	cdc_send_msg(CDC_STATUS_BULKIN, 0);
}


static bk_err_t bk_cdc_acm_init_malloc(void)
{
	for(uint32_t i = 0; i < USB_CDC_DEV_MAX_NUM; i++)
	{
		g_multi_acm_ex.rx_buf[i] = (uint8_t *)psram_malloc(sizeof(uint8_t) * CDC_EXRX_MAX_SIZE);
		if (g_multi_acm_ex.rx_buf[i] == NULL)
		{
			LOGE("psram malloc error!\r\n");
			return BK_FAIL;
		}
		g_multi_acm_ex.tx_buf[i] = (uint8_t *)psram_malloc(sizeof(uint8_t) * CDC_EXTX_MAX_SIZE);
		if (g_multi_acm_ex.tx_buf[i] == NULL)
		{
			LOGE("psram malloc error!\r\n");
			return BK_FAIL;
		}
	}
	
	g_tx_buf_temp = (uint8_t *)psram_malloc(sizeof(uint8_t) * CDC_EXTX_MAX_SIZE);
	if (g_tx_buf_temp == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}
	return BK_OK;
}

static void bk_cdc_acm_init_free(void)
{
	for(uint32_t i = 0; i < USB_CDC_DEV_MAX_NUM; i++) {
		if (g_multi_acm_ex.rx_buf[i])
		{
			psram_free(g_multi_acm_ex.rx_buf[i]);
			g_multi_acm_ex.rx_buf[i] = NULL;
		}
		if (g_multi_acm_ex.tx_buf[i])
		{
			psram_free(g_multi_acm_ex.rx_buf[i]);
			g_multi_acm_ex.tx_buf[i] = NULL;
		}
	}
	if (g_tx_buf_temp)
	{
		psram_free(g_tx_buf_temp);
		g_tx_buf_temp = NULL;
	}
}

void bk_cdc_acm_init(void)
{
	int ret = BK_FAIL;
	ret = bk_cdc_acm_init_malloc();
	BK_ASSERT(ret == BK_OK);

	for(uint32_t i = 0; i < USB_CDC_DEV_MAX_NUM; i++)
	{
		os_memset(&dbg_cdc_ipc[i],0x00,sizeof(IPC_CDC_DATA_t));
		dbg_cdc_ipc[i].mode = bk_modem_get_mode();
		dbg_cdc_ipc[i].rx_data = (uint32_t)(g_multi_acm_ex.rx_buf[i]);
		dbg_cdc_ipc[i].tx_data = (uint32_t)(g_multi_acm_ex.tx_buf[i]);
		dbg_cdc_ipc[i].rx_len  = (uint32_t)(&g_multi_acm_ex.l_rx[i]);
		dbg_cdc_ipc[i].cmd_len = sizeof(IPC_CDC_DATA_t);
		dbg_cdc_ipc[i].bk_cdc_acm_bulkin_cb  = bk_cdc_acm_bulkin_cb;

		dbg_cdc_ipc[i].tx_valid = (uint32_t)(&g_cdc_tx_valid);
		dbg_cdc_ipc[i].rx_valid = (uint32_t)(&g_cdc_rx_valid);

		cdc_send_msg(CDC_STATUS_UPDATE_PARAM, i);
	}
}


static void bk_cdc_acm_state_cb(uint32_t state)
{
	switch(state)
	{
		case CDC_STATUS_CONN:
			LOGD("CDC_STATUS_CONN\n");
			cdc_send_msg(CDC_STATUS_CONN, 0);
			break;
		case CDC_STATUS_DISCON:
			LOGD("CDC_STATUS_DISCON\n");
			cdc_send_msg(CDC_STATUS_DISCON, 0);
			break;
		case CDC_STATUS_BULKIN:
			LOGD("CDC_STATUS_BULKIN\n");
			break;
		case CDC_STATUS_BULKOUT:
			LOGD("CDC_STATUS_BULKOUT\n");
			break;
		case CDC_STATUS_IDLE:
			break;
		case CDC_STATUS_ABNORMAL:
		default:
			break;
	}
}

static void bk_cdc_demo_task(beken_thread_arg_t arg)
{
	int ret = BK_OK;
	cdc_msg_t msg;
	while (1)
	{
		ret = rtos_pop_from_queue(&cdc_msg_queue, &msg, BEKEN_WAIT_FOREVER);
		LOGD("[+]%s, type %d\n", __func__, msg.type);
		if (kNoErr == ret)
		{
			switch (msg.type)
			{
				case CDC_STATUS_CONN:
					bk_modem_usbh_conn_ind();
					break;
				case CDC_STATUS_DISCON:
					{
						bk_cdc_acm_init_free();
						bk_modem_usbh_disconn_ind();
					//	goto exit;
					}
					break;
				case CDC_STATUS_BULKIN:
					{
						bk_modem_usbh_bulkin_ind((uint8_t *)g_multi_acm_ex.rx_buf[0], g_multi_acm_ex.l_rx[0]);
					#if (USB_CDC_CP0_IPC)
						cdc_send_msg(CDC_STATUS_BULKIN_DONE, 0);
					#endif
						g_multi_acm_ex.l_rx[0] = 0;
					}
					break;
				case CDC_STATUS_BULKOUT:
					{
					#if (USB_CDC_CP0_IPC)
						uint32_t idx = 0;
						uint32_t len = (uint32_t)(msg.data);

						g_multi_acm_ex.l_tx[idx] = len;
						g_multi_acm_ex.acm_mode[idx] = g_modem_mode;
						os_memcpy(g_multi_acm_ex.tx_buf[idx], g_tx_buf_temp, len);

						dbg_cdc_ipc[idx].idx = idx;
						dbg_cdc_ipc[idx].tx_data = (uint32_t)g_multi_acm_ex.tx_buf[idx];
						dbg_cdc_ipc[idx].tx_len = (uint32_t)&g_multi_acm_ex.l_tx[idx];
						dbg_cdc_ipc[idx].cmd_len = sizeof(IPC_CDC_DATA_t);
						dbg_cdc_ipc[idx].mode = g_multi_acm_ex.acm_mode[idx];
						bk_cdc_acm_bulkout(&dbg_cdc_ipc[idx]);
					#endif
					}
					break;
				case CDC_STATUS_BULKIN_DONE:
					{
					#if (USB_CDC_CP0_IPC)
                        amp_res_acquire(AMP_RES_ID_USB_CDC, 2);
						*((uint8_t *)dbg_cdc_ipc[0].rx_valid) = 0;
						g_cdc_rx_valid = 0;
                        amp_res_release(AMP_RES_ID_USB_CDC);
					#endif
					}
					break;

				case CDC_STATUS_UPDATE_PARAM:
					{
						uint32_t idx = (uint32_t)msg.data;
						bk_usb_cdc_param_init(&dbg_cdc_ipc[idx]);
					}
					break;
				case CDC_STATUS_OUT_DELAY:
					{
						uint32_t len = (uint32_t)msg.data;
						if (g_cdc_tx_valid == 1)
						{
							rtos_delay_milliseconds(2);
							cdc_send_msg(CDC_STATUS_OUT_DELAY, len);
						} else
						{
							cdc_send_msg(CDC_STATUS_BULKOUT, len);
						}
					}
					break;
				default:
					break;
			}
		}
	}
#if 0
exit:
	if (cdc_msg_queue)
	{
		rtos_deinit_queue(&cdc_msg_queue);
		cdc_msg_queue = NULL;
	}
	if (cdc_demo_task)
	{
		cdc_demo_task = NULL;
		rtos_delete_thread(NULL);
	}
#endif
}

void bk_usb_cdc_modem(void)
{
	int ret = kNoErr;
	bk_usb_cdc_connect_init_cb(bk_cdc_acm_state_cb);

//#if USB_CDC_CP0_IPC
//	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_JPEG_EN, PM_POWER_MODULE_STATE_ON);
//#endif

	if (cdc_msg_queue == NULL)
	{
		ret = rtos_init_queue(&cdc_msg_queue, "cdc_msg_queue", sizeof(cdc_msg_t), 20);
		if (ret != kNoErr)
		{
			LOGE("init cdc_msg_queue failed\r\n");
			goto error;
		}
	}

	if (cdc_demo_task == NULL)
	{
		ret = rtos_create_thread(&cdc_demo_task,
							2,
							"cdc_demo_task",
							(beken_thread_function_t)bk_cdc_demo_task,
							2*1024,
							NULL);

		if (ret != kNoErr)
		{
			goto error;
		}
	}

	amp_res_init(AMP_RES_ID_USB_CDC);

	return;
error:
	if (cdc_msg_queue)
	{
		rtos_deinit_queue(&cdc_msg_queue);
		cdc_msg_queue = NULL;
	}
	if (cdc_demo_task)
	{
		cdc_demo_task = NULL;
		rtos_delete_thread(NULL);
	}
}

