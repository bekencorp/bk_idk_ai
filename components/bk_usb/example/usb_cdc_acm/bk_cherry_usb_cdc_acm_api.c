#include <os/os.h>
#include <os/mem.h>

#include "usb_driver.h"
#include <driver/media_types.h>

#include "bk_cherry_usb_cdc_acm_api.h"
#include "usbh_cdc_acm.h"
#include "usbh_core.h"
#include <driver/gpio.h>
#include <modules/pm.h>

#include "mb_ipc_cmd.h"
#include <driver/timer.h>

bk_usb_driver_comprehensive_ops cdc_usb_driver;

static bool stop_tx_when_restore = false;
static uint32_t acm_cnt = 0;
static uint8_t *g_rx_buf_temp = NULL;

IPC_CDC_DATA_T *g_ipc_cdc_data = NULL;
Multi_ACM_DEVICE_TOTAL_T *g_cdc_data_tol = NULL;


struct usbh_cdc_acm * acm_device = NULL;
struct usbh_cdc_acm * g_cdc_data_device[USB_CDC_DATA_DEV_NUM_MAX];
struct usbh_cdc_acm * g_cdc_acm_device[USB_CDC_ACM_DEV_NUM_MAX];

static uint32_t g_acm_state = {0};

static beken_queue_t acm_msg_queue   = NULL;
static beken_queue_t acm_msg_rxqueue   = NULL;
static beken_queue_t acm_msg_txqueue = NULL;

static bk_err_t acm_send_msg(uint8_t type, uint32_t param)
{
	bk_err_t ret = kNoErr;
	acm_msg_t msg;

	if (acm_msg_queue)
	{
		msg.type = type;
		msg.data = param;
		ret = rtos_push_to_queue(&acm_msg_queue, &msg, BEKEN_NO_WAIT);
		if (kNoErr != ret)
		{
			USB_CDC_LOGE("acm_send_msg Fail, ret:%d\n", ret);
			return kNoResourcesErr;
		}
		return ret;
	}
	return kGeneralErr;
}

static bk_err_t acm_send_rxmsg(uint8_t type, uint32_t param)
{
	bk_err_t ret = kNoErr;
	acm_msg_t msg;

	if (acm_msg_rxqueue)
	{
		msg.type = type;
		msg.data = param;
		ret = rtos_push_to_queue(&acm_msg_rxqueue, &msg, BEKEN_NO_WAIT);
		if (kNoErr != ret)
		{
			USB_CDC_LOGE("acm_send_rxmsg Fail, ret:%d\n", ret);
			return kNoResourcesErr;
		}
		return ret;
	}
	return kGeneralErr;
}

static bk_err_t acm_send_txmsg(uint8_t type, uint32_t param)
{
	bk_err_t ret = kNoErr;
	acm_msg_t msg;

	if (acm_msg_txqueue)
	{
		msg.type = type;
		msg.data = param;
		ret = rtos_push_to_queue(&acm_msg_txqueue, &msg, BEKEN_NO_WAIT);
		if (kNoErr != ret)
		{
			USB_CDC_LOGE("acm_send_txmsg Fail, ret:%d\n", ret);
			return kNoResourcesErr;
		}
		return ret;
	}
	return kGeneralErr;
}
#if (USB_CDC_CP1_IPC)
extern void ipc_cdc_send_cmd(u8 cmd, u8 *cmd_buf, u16 cmd_len, u8 * rsp_buf, u16 rsp_buf_len);

static void bk_usb_cdc_send_ipc_cmd(IPC_CDC_SUBMSG_TYPE_T msg)
{
	if (g_ipc_cdc_data == NULL)
	{
		USB_CDC_LOGE("%s: g_ipc_cdc_data is freed, discard msg %d \n", __func__, msg);
		return;
	}

	g_ipc_cdc_data->msg = msg;
	ipc_cdc_send_cmd(IPC_USB_CDC_CP1_NOTIFY, (uint8_t *)g_ipc_cdc_data, g_ipc_cdc_data->cmd_len, NULL, 0);
}

#else
void bk_usb_cdc_upload_data(IPC_CDC_DATA_T *ipc_data)
{
//	ipc_data->bk_cdc_acm_bulkin_cb(ipc_data->idx);
}

//extern void (*usb_cdc_state_cb)(uint32_t);
void bk_usb_cdc_update_state_notify(uint8_t state)
{
//	if(usb_cdc_state_cb != NULL)
//		usb_cdc_state_cb(state);
}

extern uint8_t bk_modem_get_mode(void);
extern void bk_modem_usbh_bulkin_ind(uint8_t *p_rx, uint32_t l_rx);
#endif

/*********************************************************************************************************/
usb_osal_sem_t acm_event_wait;
static beken_thread_t acm_class_task = NULL;
static beken_thread_t acm_class_rxtask = NULL;
static beken_thread_t acm_class_txtask = NULL;

beken2_timer_t acm_count_dev_onetimer;

typedef struct
{
	uint32_t ipc_cdc_rx_len;
	uint32_t cdc_tx_state;
	uint32_t cdc_tx_finish;
}Multi_ACM_DEVICE_T;
Multi_ACM_DEVICE_T g_multi_acm = {0};

#if (CDC_CIRBUFFER_IN)
static uint32_t g_cdc_rx_block = 0;
static uint32_t _is_empty(uint8_t wd, uint8_t rd)
{
	return (wd == rd);
}
static uint32_t _is_full(uint8_t wd, uint8_t rd)
{
	return (((wd+1)&(CDC_RX_CIRBUFFER_NUM-1)) == rd);
}

#endif
static uint8_t g_first_rx_urb_set = 0;

/*********************************************************************************************************/

int32_t bk_cdc_acm_io_read(void);

void bk_acm_trigger_rx(uint8_t *p_buf);
void bk_acm_trigger_tx(void);

#if (CONFIG_USB_DEVICE && CONFIG_USB_HOST)
extern bk_err_t bk_usb_otg_manual_convers_mod(E_USB_MODE close_mod, E_USB_MODE open_mod);
#endif

/*********************************************************************************************************/

static int32_t bk_usbh_cdc_acm_register_in_transfer_callback(struct usbh_cdc_acm *cdc_acm_class, void *callback, void *arg)
{
	if (cdc_acm_class)
	{
		struct usbh_urb *urb = &cdc_acm_class->bulkin_urb;
		urb->complete = (usbh_complete_callback_t)callback;
		urb->arg = arg;
	}
	return 0;
}

static int32_t bk_usbh_cdc_acm_register_out_transfer_callback(struct usbh_cdc_acm *cdc_acm_class, void *callback, void *arg)
{
	if (cdc_acm_class)
	{
		struct usbh_urb *urb = &cdc_acm_class->bulkout_urb;
		urb->complete = (usbh_complete_callback_t)callback;
		urb->arg = arg;
	}
	return 0;
}

void bk_cdc_acm_bulkin_callback(void *arg, int nbytes)
{
	if (nbytes > 0)
	{
		uint8_t wd = g_cdc_data_tol->p_data->p_cdc_data_rx->wd;
		wd = (wd+1)&(CDC_RX_CIRBUFFER_NUM-1);
		g_cdc_data_tol->p_data->p_cdc_data_rx->data[wd]->len = nbytes;
		g_cdc_data_tol->p_data->p_cdc_data_rx->wd = wd;

		acm_send_rxmsg(ACM_UPLOAD_IND, nbytes);

		acm_device->bulkin_urb.transfer_buffer_length = 0;
	}
	else if (nbytes < 0) {
		USB_CDC_LOGE("Error bulkin status, ret:%d\n", nbytes);
	}
}

void bk_cdc_acm_bulkout_callback(void *arg, int nbytes)
{
	USB_CDC_LOGD("[+]%s, nbytes:%d\r\n", __func__, nbytes);
	if (nbytes < 0)
	{
		USB_CDC_LOGE("modem tx ERROR %d\n", nbytes);
		return;
	}

	if (g_first_rx_urb_set == 0)
	{
		acm_send_rxmsg(ACM_BULKIN_IND, 0);
		g_first_rx_urb_set = 1;
	} 
}

void bk_acm_trigger_rx(uint8_t *p_buf)
{
	acm_device->bulkin_urb.complete = bk_cdc_acm_bulkin_callback;
	acm_device->bulkin_urb.pipe     = acm_device->bulkin;
	acm_device->bulkin_urb.transfer_buffer = p_buf;
	acm_device->bulkin_urb.transfer_buffer_length = CDC_RX_MAX_SIZE;
	acm_device->bulkin_urb.timeout = 0;
	acm_device->bulkin_urb.actual_length = 0;
}

void bk_acm_trigger_tx(void)
{
	acm_device->bulkout_urb.complete = bk_cdc_acm_bulkout_callback;
	acm_device->bulkout_urb.pipe     = acm_device->bulkout;
	acm_device->bulkout_urb.transfer_buffer = NULL;
	acm_device->bulkout_urb.transfer_buffer_length = CDC_TX_MAX_SIZE;
	acm_device->bulkout_urb.timeout = 100;
	acm_device->bulkout_urb.actual_length = 0;
}

int32_t bk_cdc_acm_io_read(void)
{
	USB_CDC_LOGD("[+]%s\n", __func__);
	int32_t ret = 0;

	if (acm_device == NULL)
	{
		USB_CDC_LOGE("acm_device is NULL!\r\n");
		return -1;
	}

    	uint8_t * p_buf = NULL;
    	uint8_t rd = g_cdc_data_tol->p_data->p_cdc_data_rx->rd;
    	uint8_t wd = g_cdc_data_tol->p_data->p_cdc_data_rx->wd;
    	while (1)
    	{
    		uint8_t t_rd = g_cdc_data_tol->p_data->p_cdc_data_rx->rd;
    		if (rd == t_rd)
    			break;
    		else
    			rd = t_rd;
    	}
        
    	while (1)
    	{
    	    	if (!_is_full(wd, rd))
    	    	{
    	    	    	g_cdc_rx_block = 0;
    	    	    	break;
    	    	} 
    	    	else 
    	    	{
    	    	    	g_cdc_rx_block++;
    	    	    	//if (g_cdc_rx_block > 2*CDC_RX_CIRBUFFER_NUM)
    	    	    	{
    	    	    	    	USB_CDC_LOGE("g_cdc_rx_block:%d, r:%d, w:%d\n", g_cdc_rx_block, rd, wd);
    	    	    	}
    	    	    	rtos_delay_milliseconds(2);
    	    	}
    	    	rd = g_cdc_data_tol->p_data->p_cdc_data_rx->rd;
    	}
        
    	wd = ((wd+1)&(CDC_RX_CIRBUFFER_NUM-1));
    	p_buf = g_cdc_data_tol->p_data->p_cdc_data_rx->data[wd]->data;

    	bk_acm_trigger_rx(p_buf);
    	ret = usbh_cdc_acm_bulk_in_transfer(acm_device, p_buf, CDC_RX_MAX_SIZE, 0);
    	if(ret < 0)
    	{
    	    	USB_CDC_LOGD("bk_cdc_acm_io_read is TIMEOUT! ret:%d\r\n", ret);
    	}

	return ret;
}

int32_t bk_cdc_acm_io_write_cmd(IPC_CDC_DATA_T *p_cdc_data)
{
	int32_t ret = 0;
	uint8_t *buf = g_cdc_data_tol->p_cmd->p_cdc_cmd_tx->tx_buf;
	uint32_t tx_len = g_cdc_data_tol->p_cmd->p_cdc_cmd_tx->l_tx;
	uint32_t timeout = 100;

	if (acm_device == NULL)
	{
		USB_CDC_LOGE("acm_device is NULL!\r\n");
		return -1;
	}
	bk_acm_trigger_tx();

	if (tx_len > CDC_TX_MAX_SIZE)
	{
		uint32_t sum_len = tx_len;
		uint32_t ops = 0;
		uint32_t one_len = 0;
		while(ops < sum_len)
		{
			one_len = CDC_TX_MAX_SIZE;
			if((sum_len - ops) < CDC_TX_MAX_SIZE)
			{
				one_len = sum_len - ops;
			}
			ret = usbh_cdc_acm_bulk_out_transfer(acm_device, buf+ops, one_len, timeout);

			if(ret == one_len)
			{
				ops+= one_len;
			}
			else
			{
				rtos_delay_milliseconds(3);
			}
					
		}
	}
	else 
	{
		ret = usbh_cdc_acm_bulk_out_transfer(acm_device, (uint8_t *)buf, tx_len, timeout);
	}
	return ret;
}

int32_t bk_cdc_acm_io_write_data(IPC_CDC_DATA_T *p_cdc_data)
{
	uint8_t *buf = NULL;
	uint32_t tx_len = 0;
	int32_t ret = 0;
	uint32_t timeout = 100;
	uint8_t rd = g_cdc_data_tol->p_data->p_cdc_data_tx->rd;
	uint8_t wd = g_cdc_data_tol->p_data->p_cdc_data_tx->wd;

	while (1)
	{
		uint8_t t_wd = g_cdc_data_tol->p_data->p_cdc_data_tx->wd;
		if (wd == t_wd)
			break;
		else
			wd = t_wd;
	}
	USB_CDC_LOGD("[++]%s, rd:%d, wd:%d\r\n", __func__, rd, wd);
	while (!_is_empty(wd, rd))
	{
		rd = (rd+1)&(CDC_TX_CIRBUFFER_NUM-1);
		buf = g_cdc_data_tol->p_data->p_cdc_data_tx->data[rd]->data;
		tx_len = g_cdc_data_tol->p_data->p_cdc_data_tx->data[rd]->len;

		if (acm_device == NULL)
		{
			USB_CDC_LOGE("acm_device is NULL!\r\n");
			return -1;
		}

		bk_acm_trigger_tx();
		ret = usbh_cdc_acm_bulk_out_transfer(acm_device, buf, tx_len, timeout);
		if(ret != tx_len)
		{
			USB_CDC_LOGE("bk_cdc_acm_io_write_data fail, ret %d\r\n", ret);
			if ((ret == -110) || (ret == -116))
			{
				bk_usb_cdc_send_ipc_cmd(CPU1_NOTIFY_RESTORE);
				stop_tx_when_restore = true;
				break;
			}
		}

		rtos_delay_milliseconds(2);
	}
    
	g_cdc_data_tol->p_data->p_cdc_data_tx->rd = rd;
    
	return ret;
}


int32_t bk_cdc_acm_io_write_t(IPC_CDC_DATA_T *p_cdc_data)
{
	int32_t ret = 0;

	if (stop_tx_when_restore)
	{
		USB_CDC_LOGI("%s: stop tx when restoring. \n", __func__);
		return ret;
	}

	ret = bk_cdc_acm_io_write_data(p_cdc_data);
	if (ret < 0) 
	{
		USB_CDC_LOGE("[-]%s, fail, ret:%d\r\n", __func__, ret);
	}

	return ret;
}

int32_t bk_cdc_acm_io_read_t(void)
{
	int32_t ret = 0;
	ret = bk_cdc_acm_io_read();
	if (ret < 0) 
	{
		USB_CDC_LOGE("[-]%s, fail, ret:%d\r\n", __func__, ret);
	}
	return 1;
}

void bk_cdc_acm_bulkout(void)
{
	acm_send_txmsg(ACM_BULKOUT_IND, 0);
}

static void bk_usb_cdc_open_ind(void)
{
#if (CONFIG_USB_DEVICE && CONFIG_USB_HOST)
	bk_usb_otg_manual_convers_mod(USB_DEVICE_MODE, USB_HOST_MODE);
#elif (CONFIG_USB_HOST)
	bk_usb_open(USB_HOST_MODE);
	bk_usb_power_ops(CONFIG_USB_VBAT_CONTROL_GPIO_ID, 1);
#endif
}

static void bk_usb_cdc_close_ind(void)
{
#if (CONFIG_USB_DEVICE && CONFIG_USB_HOST)
	bk_usb_otg_manual_convers_mod(USB_HOST_MODE, USB_DEVICE_MODE);
#elif (CONFIG_USB_HOST)
	bk_usb_close();
	bk_usb_power_ops(CONFIG_USB_VBAT_CONTROL_GPIO_ID, 0);
#endif
	if (g_rx_buf_temp)
	{
		psram_free(g_rx_buf_temp);
		g_rx_buf_temp = NULL;
	}
}

static void bk_usb_cdc_init_ind(void)
{
	if (!g_rx_buf_temp)
	{
		g_rx_buf_temp = (uint8_t *)psram_malloc(sizeof(uint8_t) * CDC_RX_MAX_SIZE);
		if (!g_rx_buf_temp)
			USB_CDC_LOGE("psram malloc fail\r\n");
	}
}

void bk_usb_cdc_open(void)
{
	acm_send_msg(ACM_OPEN_IND, 0);
}

void bk_usb_cdc_close(void)
{
	acm_send_msg(ACM_CLOSE_IND, 0);
}

void bk_usb_cdc_exit(void)
{
	bk_usb_cdc_free_enumerate_resources();
}

void bk_usb_cdc_param_init(IPC_CDC_DATA_T *p_cdc_data)
{
	if (!p_cdc_data || !p_cdc_data->p_info)
		USB_CDC_LOGE("Invalid Parameter!\n");

	g_ipc_cdc_data = p_cdc_data;
	g_cdc_data_tol = (Multi_ACM_DEVICE_TOTAL_T *)p_cdc_data->p_info;

	acm_send_msg(ACM_INIT_IND, 0);
}

static void bk_usb_acm_count_dev_callback(void *data1, void *data2)
{
	if (rtos_is_oneshot_timer_running(&acm_count_dev_onetimer))
	{
		rtos_stop_oneshot_timer(&acm_count_dev_onetimer);
	}
	acm_send_msg(g_acm_state, 0);
}

static void bk_usb_acm_count_dev_checktimer(uint32 data)
{
	if (g_acm_state != data)
	{
		USB_CDC_LOGI("clear acm_cnt. state from %d to %d\n", g_acm_state, data);
		acm_cnt = 0;
	}

	if (rtos_is_oneshot_timer_running(&acm_count_dev_onetimer))
	{
		rtos_stop_oneshot_timer(&acm_count_dev_onetimer);
	}
	rtos_start_oneshot_timer(&acm_count_dev_onetimer);
	g_acm_state = data;
}

static int32_t bk_usb_acm_find_ppp_dev(void)
{
	int32_t i = 0;
	for (i = 0; i < acm_cnt; i++)
	{
		if (g_cdc_data_device[i]->function == USBH_CDC_FUNCTION_PPP)
			return i;
	}
	for (i = 0; i < acm_cnt; i++)
	{
		if (g_cdc_data_device[i]->function == USBH_CDC_FUNCTION_AT) {
			USB_CDC_LOGI("Find AT dev, not ppp dev!\r\n");
			return i;
		}
	}
	return -1;
}


static void bk_usb_acm_upload_rcv_data(uint32_t len)
{

}

void bk_cdc_acm_main(void)
{
	int32_t ret = BK_OK;
	rtos_init_oneshot_timer(&acm_count_dev_onetimer,CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT+100,bk_usb_acm_count_dev_callback,(void *)0,(void *)0);

	while (1)
	{
		acm_msg_t msg;
		ret = rtos_pop_from_queue(&acm_msg_queue, &msg, BEKEN_WAIT_FOREVER);
		if (kNoErr == ret)
		{
			switch (msg.type)
			{
				case ACM_OPEN_IND:
					USB_CDC_LOGI("%s: ACM_OPEN_IND. %d\n", __func__, stop_tx_when_restore);
					bk_usb_cdc_open_ind();
					stop_tx_when_restore = false;
					break;
				case ACM_INIT_IND:
					bk_usb_cdc_init_ind();
					break;
				case ACM_CONNECT_IND:
					{
						USB_CDC_LOGI("%s: ACM_CONNECT_IND. %d\n", __func__, stop_tx_when_restore);
						g_first_rx_urb_set = 0;                      
						int32_t idx = bk_usb_acm_find_ppp_dev();
						if (idx < 0) {
							USB_CDC_LOGE("Can't find dev!!!!\r\n");
							BK_ASSERT(idx >= 0);
						}
						g_cdc_data_tol->idx = idx;
						g_cdc_data_tol->p_status->dev_cnt = (acm_cnt<<16)|(idx);
						g_cdc_data_tol->p_status->status = CDC_STATUS_CONN;
						acm_device = g_cdc_data_device[g_cdc_data_tol->idx];
						bk_usbh_cdc_sw_activate_epx(acm_device->hport, acm_device, acm_device->intf);
						bk_usb_cdc_send_ipc_cmd(CPU1_UPDATE_USB_CDC_STATE);
					}
					break;
				case ACM_DISCONNECT_IND:
					{
						USB_CDC_LOGI("%s: ACM_DISCONNECT_IND. %d\n", __func__, stop_tx_when_restore);
						if (!stop_tx_when_restore)
						{
							g_cdc_data_tol->p_status->dev_cnt = acm_cnt = 0;
							g_cdc_data_tol->p_status->status = CDC_STATUS_DISCON;
							acm_device = NULL;
							bk_usb_cdc_send_ipc_cmd(CPU1_UPDATE_USB_CDC_STATE);
						}
					}
					break;
				case ACM_EXIT_IND:
					{
						USB_CDC_LOGD("%s: ACM_EXIT_IND. %d\n", __func__, stop_tx_when_restore);
					//	bk_usb_device_set_using_status(0, USB_CDC_DEVICE);
					//	bk_usb_cdc_update_state_notify(CDC_STATUS_DISCON);
					//	goto exit;
					}
					break;
				case ACM_CLOSE_IND:
					{
						USB_CDC_LOGI("%s: ACM_CLOSE_IND. %d\n", __func__, stop_tx_when_restore);
						bk_usb_cdc_close_ind();
					//	goto exit;
					}
					break;
				default:
					break;
			}
		}
	}
}

void bk_cdc_acm_rxmain(void)
{
	int32_t ret = BK_OK;

	while (1)
	{
		acm_msg_t msg;
		ret = rtos_pop_from_queue(&acm_msg_rxqueue, &msg, BEKEN_WAIT_FOREVER);
		if (kNoErr == ret)
		{
			USB_CDC_LOGD("[+]%s, msg=%d\n", __func__, msg.type);
			switch (msg.type)
			{
				case ACM_BULKIN_IND:
					bk_cdc_acm_io_read_t();
					break;
				case ACM_UPLOAD_IND:
					{
						uint32_t len = (uint32_t)msg.data;
						if (len > 0)
						{
							bk_usb_cdc_send_ipc_cmd(CPU1_UPLOAD_USB_CDC_DATA);
							acm_send_rxmsg(ACM_BULKIN_IND, 0);
							USB_CDC_LOGD("[+]ACM_UPLOAD_DATAIND:%d \r\n", len);
						}
					}
					break;
				default:
					break;
			}
		}
	}
}

void bk_cdc_acm_txmain(void)
{
	int32_t ret = BK_OK;

	while (1)
	{
		acm_msg_t msg;
		ret = rtos_pop_from_queue(&acm_msg_txqueue, &msg, BEKEN_WAIT_FOREVER);
		if (kNoErr == ret)
		{
			switch (msg.type)
			{
				case ACM_BULKOUT_IND:
					bk_cdc_acm_io_write_t(NULL);
					break;
				default:
					break;
			}
		}
	}
}

bk_err_t bk_cdc_acm_startup(void)
{
	int32_t ret = kNoErr;
	if (acm_class_task && acm_class_rxtask)
		return ret;

	/* step 1: init acm_task */
	if (acm_msg_queue == NULL) {
		ret = rtos_init_queue(&acm_msg_queue, "acm_class_queue", sizeof(acm_msg_t), 64);
		if (ret != kNoErr)
		{
			goto error;
		}
	}
	if (acm_msg_rxqueue == NULL) {
		ret = rtos_init_queue(&acm_msg_rxqueue, "acm_class_rxqueue", sizeof(acm_msg_t), 64);
		if (ret != kNoErr)
		{
			goto error;
		}
	}
	if (acm_msg_txqueue == NULL) {
		ret = rtos_init_queue(&acm_msg_txqueue, "acm_class_txqueue", sizeof(acm_msg_t), 64);
		if (ret != kNoErr)
		{
			goto error;
		}
	}    
	if (acm_class_task == NULL)
	{
		ret = rtos_create_thread(&acm_class_task,
								4,
								"acm_class_task",
								(beken_thread_function_t)bk_cdc_acm_main,
								1024 * 4,
								(beken_thread_arg_t)0);
	}

	if (acm_class_rxtask == NULL)
	{
		ret = rtos_create_thread(&acm_class_rxtask,
								4,
								"acm_class_rxtask",
								(beken_thread_function_t)bk_cdc_acm_rxmain,
								1024 * 4,
								(beken_thread_arg_t)0);
	}

	if (acm_class_txtask == NULL)
	{
		ret = rtos_create_thread(&acm_class_txtask,
								4,
								"acm_class_txtask",
								(beken_thread_function_t)bk_cdc_acm_txmain,
								1024 * 4,
								(beken_thread_arg_t)0);
	}
    
	if (ret == kNoErr)
	{
		bk_pm_module_vote_cpu_freq(PM_DEV_ID_USB_1, PM_CPU_FRQ_480M);
	}
	else 
	{
		goto error;
	}

	return ret;

error:
	if (acm_msg_queue)
	{
		rtos_deinit_queue(&acm_msg_queue);
		acm_msg_queue = NULL;
	}
	if (acm_class_task)
	{
		rtos_delete_thread(&acm_class_task);
		acm_class_task = NULL;
	}
	if (acm_msg_rxqueue)
	{
		rtos_deinit_queue(&acm_msg_rxqueue);
		acm_msg_rxqueue = NULL;
	}
	if (acm_class_rxtask)
	{
		rtos_delete_thread(&acm_class_rxtask);
		acm_class_rxtask = NULL;
	}
	if (acm_msg_txqueue)
	{
		rtos_deinit_queue(&acm_msg_txqueue);
		acm_msg_txqueue = NULL;
	}
	if (acm_class_txtask)
	{
		rtos_delete_thread(&acm_class_txtask);
		acm_class_txtask = NULL;
	}    

	bk_pm_module_vote_cpu_freq(PM_DEV_ID_USB_1, PM_CPU_FRQ_DEFAULT);
	return ret;
}

void bk_usb_cdc_free_enumerate_resources(void)
{
//	for (uint32_t i = 0; i < USB_CDC_DATA_DEV_NUM_MAX; i++)
	for (uint32_t i = 0; i < acm_cnt; i++)
	{
		if (g_cdc_data_device[i])
		{
			if (g_cdc_data_device[i]->hport && g_cdc_data_device[i]->hport->raw_config_desc)
			{
				bk_usbh_cdc_sw_deinit(g_cdc_data_device[i]->hport, g_cdc_data_device[i]->intf, USB_DEVICE_CLASS_CDC_DATA);
			}
			g_cdc_data_device[i] = NULL;
		}
		if (g_cdc_acm_device[i])
		{
			if (g_cdc_acm_device[i]->hport && g_cdc_acm_device[i]->hport->raw_config_desc)
			{
				bk_usbh_cdc_sw_deinit(g_cdc_acm_device[i]->hport, g_cdc_acm_device[i]->intf, USB_DEVICE_CLASS_CDC);
			}
			g_cdc_acm_device[i] = NULL;
		}
	}
	acm_cnt = 0;
	acm_send_msg(ACM_EXIT_IND, 0);
}

void bk_usb_update_cdc_interface(void *hport, uint8_t bInterfaceNumber, uint8_t interface_sub_class)
{
#if 0
	USB_CDC_LOGD("[+]%s bInterfaceNumber:%d, interface_sub_class:%d\r\n", __func__, bInterfaceNumber, interface_sub_class);
	if (interface_sub_class != CDC_SUBCLASS_ACM || !hport)
		return;

	struct usbh_cdc_acm * t_acm_device = NULL;
	struct usbh_hubport * u_hport = (struct usbh_hubport *)hport;
	g_u_hport = (struct usbh_hubport *)hport;
	g_bInterfaceNumber = bInterfaceNumber;
	g_interface_sub_class = interface_sub_class;

	t_acm_device = (struct usbh_cdc_acm *)usbh_find_class_instance(u_hport->config.intf[bInterfaceNumber].devname);
	if (t_acm_device == NULL)
		USB_LOG_ERR("don't find /dev/ttyACM%d\r\n", t_acm_device->minor);
	if (g_acm_device[t_acm_device->minor] == NULL)
	{
		g_acm_device[t_acm_device->minor] = t_acm_device;
		if (t_acm_device->minor == 0) {
			bk_usbh_cdc_acm_register_in_transfer_callback(g_acm_device[t_acm_device->minor], bk_cdc_acm_bulkin_callback, NULL);
			bk_usbh_cdc_acm_register_out_transfer_callback(g_acm_device[t_acm_device->minor], bk_cdc_acm_bulkout_callback, NULL);
		} else if (t_acm_device->minor == 1) {
			bk_usbh_cdc_acm_register_in_transfer_callback(g_acm_device[t_acm_device->minor], bk_cdc_acm2_bulkin_callback, NULL);
			bk_usbh_cdc_acm_register_out_transfer_callback(g_acm_device[t_acm_device->minor], bk_cdc_acm2_bulkout_callback, NULL);
		}
		acm_cnt++;
	}

	bk_cdc_acm_startup();
#endif
}


void bk_usb_get_cdc_instance(struct usbh_hubport *hport, uint8_t intf, uint32_t class)
{
	struct usbh_cdc_acm *usb_device = NULL;

	usb_device = (struct usbh_cdc_acm *)usbh_find_class_instance(hport->config.intf[intf].devname);
	if (usb_device == NULL) {
		USB_CDC_LOGE("don't find /dev/ttyACM%d\r\n", usb_device->minor);
	}

	if (class == USB_DEVICE_CLASS_CDC)
	{
		if (g_cdc_acm_device[acm_cnt])
		{
			g_cdc_acm_device[acm_cnt] = NULL;
		}
		if (g_cdc_acm_device[acm_cnt] == NULL)
		{
			g_cdc_acm_device[acm_cnt] = usb_device;
			g_cdc_acm_device[acm_cnt]->hport = usb_device->hport;
		//	if (usb_device->minor == 0)
			{
			//	bk_usbh_cdc_acm_register_in_transfer_callback(g_cdc_acm_device[acm_cnt], bk_cdc_acm_bulkin_callback, NULL);
			//	bk_usbh_cdc_acm_register_out_transfer_callback(g_cdc_acm_device[acm_cnt], bk_cdc_acm_bulkout_callback, NULL);
			}
	//		else if (usb_device->minor == 1) {
	//			bk_usbh_cdc_acm_register_in_transfer_callback(g_cdc_acm_device[acm_cnt], bk_cdc_acm_modem_bulkin_callback, NULL);
	//			bk_usbh_cdc_acm_register_out_transfer_callback(g_cdc_acm_device[acm_cnt], bk_cdc_acm_modem_bulkout_callback, NULL);
	//		}
		}
	}	
	else if (class == USB_DEVICE_CLASS_CDC_DATA)
	{
		if (g_cdc_data_device[acm_cnt])
		{
			g_cdc_data_device[acm_cnt] = NULL;
		}
		if (g_cdc_data_device[acm_cnt] == NULL)
		{
			g_cdc_data_device[acm_cnt] = usb_device;
			g_cdc_data_device[acm_cnt]->hport = usb_device->hport;
		//	if (usb_device->minor == 0)
			{
				bk_usbh_cdc_acm_register_in_transfer_callback(g_cdc_data_device[acm_cnt], bk_cdc_acm_bulkin_callback, NULL);
				bk_usbh_cdc_acm_register_out_transfer_callback(g_cdc_data_device[acm_cnt], bk_cdc_acm_bulkout_callback, NULL);
			}
	//		else if (usb_device->minor == 1) {
	//			bk_usbh_cdc_acm_register_in_transfer_callback(g_cdc_data_device[acm_cnt], bk_cdc_acm_modem_bulkin_callback, NULL);
	//			bk_usbh_cdc_acm_register_out_transfer_callback(g_cdc_data_device[acm_cnt], bk_cdc_acm_modem_bulkout_callback, NULL);
	//		}
		}
		USB_CDC_LOGI("bk_usb_get_cdc_instance %d, device:0x%x, intf:%d\r\n", acm_cnt, usb_device,  g_cdc_data_device[acm_cnt]->intf);
		acm_cnt++;
	} 
	else
	{
		USB_CDC_LOGI("bk_usb_get_cdc_instance %d, discard class %d, intf:%d\r\n", acm_cnt, class, intf);
	}
}

void bk_usb_cdc_connect_notify(struct usbh_hubport *hport, uint8_t intf, uint32_t class)
{
	bk_usb_acm_count_dev_checktimer(ACM_CONNECT_IND);

	bk_usb_get_cdc_instance(hport, intf, class);
}

void bk_usb_cdc_disconnect_notify(struct usbh_hubport *hport, uint8_t intf, uint32_t class)
{
	bk_usb_acm_count_dev_checktimer(ACM_DISCONNECT_IND);
}

void bk_usb_cdc_rcv_notify_cp1(IPC_CDC_DATA_T *p)
{
	IPC_CDC_SUBMSG_TYPE_T msg = p->msg;
	USB_CDC_LOGD("[+]%s, msg:%d\r\n", __func__, msg);
	switch(msg)
	{
		case CPU0_OPEN_USB_CDC:
			bk_usb_cdc_open();
			break;
		case CPU0_CLOSE_USB_CDC:
			bk_usb_cdc_close();
			break;
		case CPU0_INIT_USB_CDC_PARAM:
			bk_usb_cdc_param_init(p);
			break;
		case CPU0_BULKOUT_USB_CDC_CMD:
		case CPU0_BULKOUT_USB_CDC_DATA:
			bk_cdc_acm_bulkout();
			break;
		default:
			break;
	}
}


