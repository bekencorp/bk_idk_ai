
#ifndef _BK_MODEM_USBH_IF_H_
#define _BK_MODEM_USBH_IF_H_

#include <common/bk_include.h>

void bk_modem_usbh_close(void);
void bk_modem_usbh_poweron_ind(void);
void bk_modem_usbh_conn_ind(void);
void bk_modem_usbh_disconn_ind(void);
void bk_modem_usbh_bulkout_ind(char *p_tx, uint32_t l_tx);
void bk_modem_set_usbdev_idx(uint32_t idx);

#endif
