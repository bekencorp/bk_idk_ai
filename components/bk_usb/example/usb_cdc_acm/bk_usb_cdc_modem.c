#include <os/mem.h>
#include <os/str.h>
#include <os/os.h>
#include <driver/int.h>
#include <driver/pwr_clk.h>

#include <common/bk_err.h>
#include <common/bk_include.h>

#include "cli.h"
#include "mb_ipc_cmd.h"
#include "bk_usb_cdc_modem.h"

#define TAG "cdc_modem"

#define LOGI(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#if (CONFIG_BK_MODEM)
extern void bk_modem_usbh_conn_ind(uint32_t cnt);
extern void bk_modem_usbh_disconn_ind(void);
extern void bk_modem_usbh_restore_ind(void);
extern void bk_modem_usbh_close(void);
extern void bk_modem_usbh_bulkout_ind(char *p_tx, uint32_t l_tx);
extern void bk_modem_usbh_bulkin_ind(uint8_t *p_rx, uint32_t l_rx);
extern void bk_modem_usbh_poweron_ind(void);
extern uint8_t bk_modem_get_mode(void);

#else

void bk_modem_usbh_conn_ind(uint32_t cnt){ }
void bk_modem_usbh_disconn_ind(void){ }
void bk_modem_usbh_restore_ind(void){ }
void bk_modem_usbh_close(void){ }
void bk_modem_usbh_bulkout_ind(char *p_tx, uint32_t l_tx){ }
void bk_modem_usbh_bulkin_ind(uint8_t *p_rx, uint32_t l_rx){ }
void bk_modem_usbh_poweron_ind(void){ }
uint8_t bk_modem_get_mode(void){return 0;}
#endif

IPC_CDC_DATA_T *g_cdc_ipc;
Multi_ACM_DEVICE_TOTAL_T *g_multi_acm_total = NULL;

static beken_queue_t cdc_msg_queue = NULL;
static beken_thread_t cdc_demo_task = NULL;
static beken_queue_t cdc_msg_rxqueue = NULL;
static beken_thread_t cdc_demo_rxtask = NULL;
static beken_queue_t cdc_msg_txqueue = NULL;
static beken_thread_t cdc_demo_txtask = NULL;

static uint8_t g_cdc_state = 0;

static uint8_t g_temp_rx_buf[512] = {0};
static uint32_t g_temp_rx_len = 0;

#if CDC_CIRBUFFER_OUT
static uint32_t g_cdc_tx_block = 0;

static uint32_t _is_empty(uint8_t wd, uint8_t rd)
{
	return (wd == rd);
}
static uint32_t _is_full(uint8_t wd, uint8_t rd)
{
	return (((wd+1)&(CDC_TX_CIRBUFFER_NUM-1)) == rd);
}
#endif

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

static bk_err_t cdc_send_rxmsg(uint8_t type, uint32_t param)
{
	bk_err_t ret = kNoErr;
	cdc_msg_t msg;

	if (cdc_msg_rxqueue)
	{
		msg.type = type;
		msg.data = param;

		ret = rtos_push_to_queue(&cdc_msg_rxqueue, &msg, 100);//BEKEN_NO_WAIT);
		if (kNoErr != ret)
		{
			return kNoResourcesErr;
		}
		return ret;
	}
	return kGeneralErr;
}

static bk_err_t cdc_send_txmsg(uint8_t type, uint32_t data_len, uint32_t* p)
{
	bk_err_t ret = kNoErr;
	cdc_msg_t msg;

	if (cdc_msg_txqueue)
	{
		msg.type = type;
		msg.data = data_len;

        	if (data_len)
        	{
        		msg.param = os_malloc(data_len);

        		if (msg.param)
        		{
        			os_memset((uint8_t *)msg.param, 0, data_len);
        			os_memcpy((uint8_t *)msg.param, (uint8_t *)p, data_len);
        		}
        		else
        		{
        			LOGW("%s: msg %d alloc fail \n", __func__, type);
        			return BK_FAIL;
        		}
        	}
        	else
        	{
        		msg.param = (void *)p;                
        	}
             
		ret = rtos_push_to_queue(&cdc_msg_txqueue, &msg, BEKEN_NO_WAIT);//BEKEN_NO_WAIT);
		if (kNoErr != ret)
		{
          		if (data_len)
          		{
        			os_free(msg.param);
          		}
			return kNoResourcesErr;
		}
		return ret;
	}
	return kGeneralErr;
}

extern void ipc_cdc_send_cmd(u8 cmd, u8 *cmd_buf, u16 cmd_len, u8 * rsp_buf, u16 rsp_buf_len);
static void bk_usb_cdc_send_ipc_cmd(IPC_CDC_SUBMSG_TYPE_T msg)
{
	if (g_cdc_ipc == NULL)
	{
		LOGW("%s: g_cdc_ipc is freed, discard msg %d \n", __func__, msg);
		return;
	}

	g_cdc_ipc->msg = msg;
	ipc_cdc_send_cmd(IPC_USB_CDC_CP0_NOTIFY, (uint8_t *)g_cdc_ipc, g_cdc_ipc->cmd_len, NULL, 0);
}

void bk_cdc_acm_bulkin_cmd(void)
{
	cdc_send_rxmsg(CDC_STATUS_BULKIN_CMD, 0);
}
void bk_cdc_acm_bulkin_data(void)
{
	cdc_send_rxmsg(CDC_STATUS_BULKIN_DATA, 0);
}

void bk_cdc_acm_restore()
{
	bk_modem_usbh_restore_ind();
}

void bk_cdc_acm_state_notify(CDC_STATUS_t * dev_state)
{
	uint32_t state = dev_state->status;
	switch(state)
	{
		case CDC_STATUS_CONN:
			cdc_send_msg(CDC_STATUS_CONN, dev_state->dev_cnt);
			break;
		case CDC_STATUS_DISCON:
			cdc_send_msg(CDC_STATUS_DISCON, 0);
			break;
		default:
			break;
	}
}

void bk_usb_cdc_rcv_notify_cp0(IPC_CDC_DATA_T *p)
{
	IPC_CDC_SUBMSG_TYPE_T msg = p->msg;
	switch(msg)
	{
		case CPU1_UPDATE_USB_CDC_STATE:
			{
				Multi_ACM_DEVICE_TOTAL_T * p_dbg = (Multi_ACM_DEVICE_TOTAL_T *)(p->p_info);
				bk_cdc_acm_state_notify(p_dbg->p_status);
			}
			break;
		case CPU1_UPLOAD_USB_CDC_CMD:
			bk_cdc_acm_bulkin_cmd();
			break;
		case CPU1_UPLOAD_USB_CDC_DATA:
			bk_cdc_acm_bulkin_data();
			break;
		case CPU1_NOTIFY_RESTORE:
			bk_cdc_acm_restore();
			break;
		default:
			break;
	}
}

void bk_usb_cdc_open(void)
{
	cdc_send_msg(CDC_STATUS_OPEN, 0);
}

void bk_usb_cdc_close(void)
{
	cdc_send_msg(CDC_STATUS_CLOSE, 0);
}

int32_t bk_cdc_acm_modem_write(char *p_tx, uint32_t l_tx)
{
    cdc_send_txmsg(CDC_STATUS_BULKOUT_DATA, l_tx, (uint32_t*)p_tx);
    return 0;
}

static int32_t bk_cdc_acm_modem_write_handle(char *p_tx, uint32_t l_tx)
{
	bk_err_t __maybe_unused ret = BK_FAIL;
	if (l_tx > CDC_EXTX_MAX_SIZE) 
	{
		LOGE("[+]%s, Transbuf overflow!\r\n", __func__);
		return ret;
	}

	if ((g_cdc_state == CDC_STATUS_DISCON) || (g_cdc_state == CDC_STATUS_CLOSE))
		return ret;

	uint8_t * p_buf = NULL;
	uint8_t rd = g_multi_acm_total->p_data->p_cdc_data_tx->rd;
	uint8_t wd = g_multi_acm_total->p_data->p_cdc_data_tx->wd;

	while (1)
	{
		uint8_t t_rd = g_multi_acm_total->p_data->p_cdc_data_tx->rd;
		if (rd == t_rd)
			break;
		else
			rd = t_rd;
	}

	// Sens a null pack if the tx length is a multiple of CDC_TX_MAX_SIZE.
	uint8_t segment_cnt = l_tx / CDC_TX_MAX_SIZE + 1;
	uint32_t ops = 0;
	int data_cnt = 0;
	uint32_t data_len = 0;

	while (1)
	{
		if (!_is_full(wd, rd))
		{
			wd = (wd+1)&(CDC_TX_CIRBUFFER_NUM-1);

			data_len = (l_tx-ops > CDC_TX_MAX_SIZE)? CDC_TX_MAX_SIZE: (l_tx-ops);

			p_buf = g_multi_acm_total->p_data->p_cdc_data_tx->data[wd]->data;
			g_multi_acm_total->p_data->p_cdc_data_tx->data[wd]->len = data_len;
			os_memcpy(p_buf, p_tx+ops, data_len);

			ops += data_len;
			data_cnt ++;
			if (data_cnt == segment_cnt)
			{
				g_cdc_tx_block = 0;
				break;
			}
		}
		else
		{
			g_cdc_tx_block++;
			if (g_cdc_tx_block > (2*CDC_TX_CIRBUFFER_NUM))
			{
				LOGE("g_cdc_tx_block:%d, w:%d, r:%d\n", g_cdc_tx_block, wd, rd);
			}
			rtos_delay_milliseconds(2);
		}

		if ((g_cdc_state == CDC_STATUS_DISCON) || (g_cdc_state == CDC_STATUS_CLOSE))
		{
			LOGI("%s: stop write. \n", __func__);
			break;
		}

		rd = g_multi_acm_total->p_data->p_cdc_data_tx->rd;
	}

	g_multi_acm_total->p_data->p_cdc_data_tx->wd = wd;
	bk_usb_cdc_send_ipc_cmd(CPU0_BULKOUT_USB_CDC_DATA);

	return 0;
}

static bk_err_t bk_cdc_acm_init_malloc(void)
{
	uint32_t i = 0;

	LOGI("%s\r\n", __func__);

	g_cdc_ipc = (IPC_CDC_DATA_T *)psram_malloc(sizeof(IPC_CDC_DATA_T));
	if (g_cdc_ipc == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}
	os_memset(g_cdc_ipc, 0x00, sizeof(IPC_CDC_DATA_T));

	g_multi_acm_total = (Multi_ACM_DEVICE_TOTAL_T *)psram_malloc(sizeof(Multi_ACM_DEVICE_TOTAL_T));
	if (g_multi_acm_total == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}
	os_memset(g_multi_acm_total, 0x00, sizeof(Multi_ACM_DEVICE_TOTAL_T));

	/* p_status */
	g_multi_acm_total->p_status = (CDC_STATUS_t *)psram_malloc(sizeof(CDC_STATUS_t));
	if (g_multi_acm_total->p_status == NULL)
	{
		LOGE("psram malloc error!\r\n");
	}
	g_multi_acm_total->p_status->dev_cnt = 0;
	g_multi_acm_total->p_status->status = 0;

	/* p_cmd */
	g_multi_acm_total->p_cmd = (Multi_ACM_DEVICE_CMD_T *)psram_malloc(sizeof(Multi_ACM_DEVICE_CMD_T));
	if (g_multi_acm_total->p_cmd == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}
	g_multi_acm_total->p_cmd->p_cdc_cmd_tx = (CDC_CIRBUF_CMD_TX_T *)psram_malloc(sizeof(CDC_CIRBUF_CMD_TX_T));
	if (g_multi_acm_total->p_cmd->p_cdc_cmd_tx == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}
	g_multi_acm_total->p_cmd->p_cdc_cmd_tx->tx_buf = (uint8_t *)psram_malloc(CDC_EXTX_MAX_SIZE);
	if (g_multi_acm_total->p_cmd->p_cdc_cmd_tx->tx_buf == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}
	g_multi_acm_total->p_cmd->p_cdc_cmd_rx = (CDC_CIRBUF_CMD_RX_T *)psram_malloc(sizeof(CDC_CIRBUF_CMD_RX_T));
	if (g_multi_acm_total->p_cmd->p_cdc_cmd_rx == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}
	g_multi_acm_total->p_cmd->p_cdc_cmd_rx->rx_buf = (uint8_t *)psram_malloc(CDC_RX_MAX_SIZE);
	if (g_multi_acm_total->p_cmd->p_cdc_cmd_rx->rx_buf == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}

	/* p_data */
	g_multi_acm_total->p_data = (Multi_ACM_DEVICE_DATA_T *)psram_malloc(sizeof(Multi_ACM_DEVICE_DATA_T));
	if (g_multi_acm_total->p_data == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}
//////
	g_multi_acm_total->p_data->p_cdc_data_tx = (CDC_CIRBUF_DATA_TX_T *)psram_malloc(sizeof(CDC_CIRBUF_DATA_TX_T));
	if (g_multi_acm_total->p_data->p_cdc_data_tx == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}

	for (i = 0; i < CDC_TX_CIRBUFFER_NUM; i++)
	{
		g_multi_acm_total->p_data->p_cdc_data_tx->data[i] = (CDC_CIRBUF_TX_ELE_T *)psram_malloc(sizeof(CDC_CIRBUF_TX_ELE_T));
		if (g_multi_acm_total->p_data->p_cdc_data_tx->data[i] == NULL)
		{
			LOGE("psram malloc error!\r\n");
			return BK_FAIL;
		}
		g_multi_acm_total->p_data->p_cdc_data_tx->data[i]->data = (uint8_t *)psram_malloc(CDC_EXTX_MAX_SIZE * sizeof(uint8_t));
		if (g_multi_acm_total->p_data->p_cdc_data_tx->data[i]->data == NULL)
		{
			LOGE("psram malloc error!\r\n");
			return BK_FAIL;
		}
		g_multi_acm_total->p_data->p_cdc_data_tx->data[i]->len = 0;
	}
//////
	g_multi_acm_total->p_data->p_cdc_data_rx = (CDC_CIRBUF_DATA_RX_T *)psram_malloc(sizeof(CDC_CIRBUF_DATA_RX_T));
	if (g_multi_acm_total->p_data->p_cdc_data_rx == NULL)
	{
		LOGE("psram malloc error!\r\n");
		return BK_FAIL;
	}
	for (i = 0; i < CDC_RX_CIRBUFFER_NUM; i++)
	{
		g_multi_acm_total->p_data->p_cdc_data_rx->data[i] = (CDC_CIRBUF_RX_ELE_T *)psram_malloc(sizeof(CDC_CIRBUF_RX_ELE_T));
		if (g_multi_acm_total->p_data->p_cdc_data_rx->data[i] == NULL)
		{
			LOGE("psram malloc error!\r\n");
			return BK_FAIL;
		}
		g_multi_acm_total->p_data->p_cdc_data_rx->data[i]->data = (uint8_t *)psram_malloc(CDC_RX_MAX_SIZE * sizeof(uint8_t));
		if (g_multi_acm_total->p_data->p_cdc_data_rx->data[i]->data == NULL)
		{
			LOGE("psram malloc error!\r\n");
			return BK_FAIL;
		}
		g_multi_acm_total->p_data->p_cdc_data_rx->data[i]->len = 0;
	}
	return BK_OK;
}

static void bk_cdc_acm_init_free(void)
{
	uint32_t i = 0;

	LOGI("%s\r\n", __func__);

	if (g_cdc_ipc) {
		psram_free(g_cdc_ipc);
		g_cdc_ipc = NULL;
	}

	if (g_multi_acm_total) {

		for (i = 0; i < CDC_TX_CIRBUFFER_NUM; i++)
		{
			if (g_multi_acm_total->p_data->p_cdc_data_tx->data[i]->data) {
				psram_free(g_multi_acm_total->p_data->p_cdc_data_tx->data[i]->data);
				g_multi_acm_total->p_data->p_cdc_data_tx->data[i]->data = NULL;
			}
			if (g_multi_acm_total->p_data->p_cdc_data_tx->data[i]) {
				psram_free(g_multi_acm_total->p_data->p_cdc_data_tx->data[i]);
				g_multi_acm_total->p_data->p_cdc_data_tx->data[i] = NULL;
			}
		}

		if (g_multi_acm_total->p_data->p_cdc_data_tx) {
			psram_free(g_multi_acm_total->p_data->p_cdc_data_tx);
			g_multi_acm_total->p_data->p_cdc_data_tx = NULL;
		}

		for (i = 0; i < CDC_RX_CIRBUFFER_NUM; i++)
		{
			if (g_multi_acm_total->p_data->p_cdc_data_rx->data[i]->data) {
				psram_free(g_multi_acm_total->p_data->p_cdc_data_rx->data[i]->data);
				g_multi_acm_total->p_data->p_cdc_data_rx->data[i]->data = NULL;
			}
			if (g_multi_acm_total->p_data->p_cdc_data_rx->data[i]) {
				psram_free(g_multi_acm_total->p_data->p_cdc_data_rx->data[i]);
				g_multi_acm_total->p_data->p_cdc_data_rx->data[i] = NULL;
			}
		}

		if (g_multi_acm_total->p_data->p_cdc_data_rx) {
			psram_free(g_multi_acm_total->p_data->p_cdc_data_rx);
			g_multi_acm_total->p_data->p_cdc_data_rx = NULL;
		}

		if (g_multi_acm_total->p_data) {
			psram_free(g_multi_acm_total->p_data);
			g_multi_acm_total->p_data = NULL;
		}

		if (g_multi_acm_total->p_cmd->p_cdc_cmd_tx->tx_buf) {
			psram_free(g_multi_acm_total->p_cmd->p_cdc_cmd_tx->tx_buf);
			g_multi_acm_total->p_cmd->p_cdc_cmd_tx->tx_buf = NULL;
		}

		if (g_multi_acm_total->p_cmd->p_cdc_cmd_tx) {
			psram_free(g_multi_acm_total->p_cmd->p_cdc_cmd_tx);
			g_multi_acm_total->p_cmd->p_cdc_cmd_tx = NULL;
		}

		if (g_multi_acm_total->p_cmd->p_cdc_cmd_rx->rx_buf) {
			psram_free(g_multi_acm_total->p_cmd->p_cdc_cmd_rx->rx_buf);
			g_multi_acm_total->p_cmd->p_cdc_cmd_rx->rx_buf = NULL;
		}

		if (g_multi_acm_total->p_cmd->p_cdc_cmd_rx) {
			psram_free(g_multi_acm_total->p_cmd->p_cdc_cmd_rx);
			g_multi_acm_total->p_cmd->p_cdc_cmd_rx = NULL;
		}

		if (g_multi_acm_total->p_cmd) {
			psram_free(g_multi_acm_total->p_cmd);
			g_multi_acm_total->p_cmd = NULL;
		}

		if (g_multi_acm_total->p_status) {
			psram_free(g_multi_acm_total->p_status);
			g_multi_acm_total->p_status == NULL;
		}

		if (g_multi_acm_total) {
			psram_free(g_multi_acm_total);
			g_multi_acm_total = NULL;
		}
	}
}

void bk_cdc_acm_deinit(void)
{
	if (g_multi_acm_total)
	{
		g_multi_acm_total->p_data->p_cdc_data_tx->rd = 0;
		g_multi_acm_total->p_data->p_cdc_data_tx->wd = 0;

		g_multi_acm_total->p_data->p_cdc_data_rx->rd = 0;
		g_multi_acm_total->p_data->p_cdc_data_rx->wd = 0;

		g_multi_acm_total->idx = 0;
		g_multi_acm_total->mode = 0;
	}
}

void bk_cdc_acm_init(void)
{
	int ret = BK_FAIL;
	ret = bk_cdc_acm_init_malloc();
	BK_ASSERT(ret == BK_OK);

	g_multi_acm_total->p_data->p_cdc_data_tx->rd = 0;
	g_multi_acm_total->p_data->p_cdc_data_tx->wd = 0;

	g_multi_acm_total->p_data->p_cdc_data_rx->rd = 0;
	g_multi_acm_total->p_data->p_cdc_data_rx->wd = 0;

	g_multi_acm_total->idx = 0;
	g_multi_acm_total->mode = 0;

	g_cdc_ipc->p_info = (uint32_t)g_multi_acm_total;
	g_cdc_ipc->cmd_len = sizeof(IPC_CDC_DATA_T);

	cdc_send_msg(CDC_STATUS_INIT_PARAM, 0);
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
				case CDC_STATUS_OPEN:
					LOGI("%s: CDC_STATUS_OPEN, %d \n", __func__, g_cdc_state);
					g_cdc_state = CDC_STATUS_OPEN;
					bk_usb_cdc_send_ipc_cmd(CPU0_OPEN_USB_CDC);
					break;
				case CDC_STATUS_CLOSE:
					LOGI("%s: CDC_STATUS_CLOSE, %d \n", __func__, g_cdc_state);
					g_cdc_state = CDC_STATUS_CLOSE;
					bk_usb_cdc_send_ipc_cmd(CPU0_CLOSE_USB_CDC);
					break;
				case CDC_STATUS_CONN:
					{
						LOGI("%s: CDC_STATUS_CONN, %d \n", __func__, g_cdc_state);
						g_cdc_state = CDC_STATUS_CONN;
						uint32_t cnt = (uint32_t)msg.data;
						bk_modem_usbh_conn_ind(cnt);
					}
					break;
				case CDC_STATUS_DISCON:
					{
						LOGI("%s: CDC_STATUS_DISCON, %d \n", __func__, g_cdc_state);
						bk_cdc_acm_deinit();
						bk_modem_usbh_disconn_ind();
						if (g_cdc_state == CDC_STATUS_CLOSE)
							bk_cdc_acm_init_free();
						g_cdc_state = CDC_STATUS_DISCON;
					}
					break;
				case CDC_STATUS_INIT_PARAM:
					bk_usb_cdc_send_ipc_cmd(CPU0_INIT_USB_CDC_PARAM);
					break;
				default:
					break;
			}
		}
	}
}

static void bk_cdc_usbh_upload_ind(void)
{
	uint8_t *p_buf = NULL;
	uint32_t len = 0;
	uint8_t rd = g_multi_acm_total->p_data->p_cdc_data_rx->rd;
	uint8_t wd = g_multi_acm_total->p_data->p_cdc_data_rx->wd;

	while (1)
	{
		uint8_t t_wd = g_multi_acm_total->p_data->p_cdc_data_rx->wd;
		if (wd == t_wd)
			break;
		else
			wd = t_wd;
	}

	LOGD("[++]%s, rd:%d, wd:%d\r\n", __func__, rd, wd);
	while (!_is_empty(wd, rd))
	{
		rd = (rd+1)&(CDC_RX_CIRBUFFER_NUM-1);
		p_buf = g_multi_acm_total->p_data->p_cdc_data_rx->data[rd]->data;
		len = g_multi_acm_total->p_data->p_cdc_data_rx->data[rd]->len;
		if (len == 0 || len > 512) {
			LOGE("Error inLen!!!, rd:%d, len:%d\r\n", rd, len);
		}
		g_temp_rx_len = len;
		os_memcpy(&g_temp_rx_buf[0], p_buf, g_temp_rx_len);

		bk_modem_usbh_bulkin_ind(&g_temp_rx_buf[0], g_temp_rx_len);
	}
	g_multi_acm_total->p_data->p_cdc_data_rx->rd = rd;
}

static void bk_cdc_demo_rxtask(beken_thread_arg_t arg)
{
	int ret = BK_OK;
	cdc_msg_t msg;
	while (1)
	{
		ret = rtos_pop_from_queue(&cdc_msg_rxqueue, &msg, BEKEN_WAIT_FOREVER);
		LOGD("[+]%s, type %d\n", __func__, msg.type);
		if (kNoErr == ret)
		{
			switch (msg.type)
			{
				case CDC_STATUS_BULKIN_CMD:
					bk_modem_usbh_bulkin_ind(g_multi_acm_total->p_cmd->p_cdc_cmd_rx->rx_buf, g_multi_acm_total->p_cmd->p_cdc_cmd_rx->l_rx);
					g_multi_acm_total->p_cmd->p_cdc_cmd_rx->l_rx = 0;
					break;
				case CDC_STATUS_BULKIN_DATA:
					bk_cdc_usbh_upload_ind();
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

static void bk_cdc_demo_txtask(beken_thread_arg_t arg)
{
	int ret = BK_OK;
	cdc_msg_t msg;
	while (1)
	{
		ret = rtos_pop_from_queue(&cdc_msg_txqueue, &msg, BEKEN_WAIT_FOREVER);
		LOGD("[+]%s, type %d\n", __func__, msg.type);
		if (kNoErr == ret)
		{
			switch (msg.type)
			{
				case CDC_STATUS_BULKOUT_CMD:
					bk_usb_cdc_send_ipc_cmd(CPU0_BULKOUT_USB_CDC_CMD);
					break;
				case CDC_STATUS_BULKOUT_DATA:
				{
					bk_cdc_acm_modem_write_handle((char *)msg.param, msg.data);                    
					break;
				}
				default:
					break;
			}
		}
		if (msg.data)
			os_free(msg.param);        
	}
}

void bk_usb_cdc_modem(void)
{
	int ret = kNoErr;

//#if USB_CDC_CP0_IPC
//	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_JPEG_EN, PM_POWER_MODULE_STATE_ON);
//#endif

	if (cdc_msg_queue == NULL)
	{
		ret = rtos_init_queue(&cdc_msg_queue, "cdc_msg_queue", sizeof(cdc_msg_t), 64);
		if (ret != kNoErr)
		{
			LOGE("init cdc_msg_queue failed\r\n");
			goto error;
		}
	}
	if (cdc_demo_task == NULL)
	{
		ret = rtos_create_thread(&cdc_demo_task,
							4,
							"cdc_modem_task",
							(beken_thread_function_t)bk_cdc_demo_task,
							4*1024,
							NULL);

		if (ret != kNoErr)
		{
			goto error;
		}
	}
	if (cdc_msg_rxqueue == NULL)
	{
		ret = rtos_init_queue(&cdc_msg_rxqueue, "cdc_msg_rxqueue", sizeof(cdc_msg_t), 64);
		if (ret != kNoErr)
		{
			LOGE("init cdc_msg_queue failed\r\n");
			goto error;
		}
	}
	if (cdc_demo_rxtask == NULL)
	{
		ret = rtos_create_thread(&cdc_demo_rxtask,
							4,
							"cdc_modem_rxtask",
							(beken_thread_function_t)bk_cdc_demo_rxtask,
							4*1024,
							NULL);

		if (ret != kNoErr)
		{
			goto error;
		}
	}

	if (cdc_msg_txqueue == NULL)
	{
		ret = rtos_init_queue(&cdc_msg_txqueue, "cdc_msg_txqueue", sizeof(cdc_msg_t), 64);
		if (ret != kNoErr)
		{
			LOGE("init cdc_msg_queue failed\r\n");
			goto error;
		}
	}
	if (cdc_demo_txtask == NULL)
	{
		ret = rtos_create_thread(&cdc_demo_txtask,
							4,
							"cdc_modem_txtask",
							(beken_thread_function_t)bk_cdc_demo_txtask,
							4*1024,
							NULL);

		if (ret != kNoErr)
		{
			goto error;
		}
	}    
	return;
error:
	if (cdc_msg_queue)
	{
		rtos_deinit_queue(&cdc_msg_queue);
		cdc_msg_queue = NULL;
	}
	if (cdc_demo_task)
	{
		rtos_delete_thread(cdc_demo_task);
		cdc_demo_task = NULL;        
	}
	if (cdc_msg_rxqueue)
	{
		rtos_deinit_queue(&cdc_msg_rxqueue);
		cdc_msg_rxqueue = NULL;
	}
	if (cdc_demo_rxtask)
	{
		rtos_delete_thread(cdc_demo_rxtask);
		cdc_demo_rxtask = NULL;        
	}
	if (cdc_msg_txqueue)
	{
		rtos_deinit_queue(&cdc_msg_txqueue);
		cdc_msg_txqueue = NULL;
	}
	if (cdc_demo_txtask)
	{
		rtos_delete_thread(cdc_demo_txtask);
		cdc_demo_txtask = NULL;        
	}    
}

