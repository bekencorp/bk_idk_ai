/**
 ****************************************************************************************
 *
 * @file bk_modem_usbh_if.c
 *
 * @brief USB host interface file.
 *
 ****************************************************************************************
 */

#include <os/os.h>
#include <components/log.h>
#include "cli.h"


#define TAG "modem_usb"

#define LOGI(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


#if (CONFIG_SYS_CPU0)

#include "bk_cli.h"
#include "bk_modem_main.h"
#include "bk_modem_dte.h"

#if (CONFIG_USB_CDC_MODEM)
#include "bk_usb_cdc_modem.h"

extern void bk_cdc_acm_init(void);
extern void bk_usb_cdc_open(void);

extern void bk_usb_cdc_close(void);
extern int32_t bk_cdc_acm_modem_write(char *p_tx, uint32_t l_tx);

extern bk_err_t bk_cdc_acm_startup(void);

extern void bk_usb_cdc_modem(void);
extern void bk_usb_cdc_open(void);


void bk_modem_usbh_conn_ind(uint32_t cnt)
{
	LOGI("[+]%s, %d\n", __func__, cnt);
	bk_modem_send_msg(MSG_MODEM_CONN_IND, cnt,0,0);
}

void bk_modem_usbh_disconn_ind(void)
{
	LOGI("[+]%s\n", __func__);
	bk_modem_send_msg(MSG_MODEM_DISC_IND, 0,0,0);
}

void bk_modem_usbh_restore_ind(void)
{
	LOGI("[+]%s\n", __func__);
	bk_modem_set_state(PPP_STOP);
	bk_modem_send_msg(MSG_PPP_STOP, RESTORE_STOP, 0, NULL);
}

uint8_t bk_modem_get_mode(void)
{
	if (bk_modem_env.bk_modem_ppp_mode == PPP_CMD_MODE)
	{
		return 1;
	}

	if (bk_modem_env.bk_modem_ppp_mode == PPP_DATA_MODE)
	{
		return 2;
	}

	return 0;
}

void bk_modem_usbh_close(void)
{
	bk_usb_cdc_close();
}

void bk_modem_usbh_bulkout_ind(char *p_tx, uint32_t l_tx)
{
	bk_cdc_acm_modem_write(p_tx, l_tx);
}

void bk_modem_usbh_bulkin_ind(uint8_t *p_rx, uint32_t l_rx)
{
	bk_modem_dte_recv_data(l_rx, (uint8_t *)p_rx);
}


void bk_modem_usbh_poweron_ind(void)
{

	bk_usb_cdc_modem();
	bk_cdc_acm_init();

	bk_usb_cdc_open();
}

#else

void bk_modem_usbh_conn_ind(void){}

void bk_modem_usbh_disconn_ind(void){}

void bk_modem_usbh_close(void){}

void bk_modem_usbh_bulkout_ind(char *p_tx, uint32_t l_tx){}

void bk_modem_usbh_bulkin_ind(uint8_t *p_rx, uint32_t l_rx){}

void bk_modem_usbh_poweron_ind(void)
{
	LOGE("Need Open the Macro about CDC\n");
}

#endif

#endif

