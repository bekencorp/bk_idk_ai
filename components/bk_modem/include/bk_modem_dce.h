
#ifndef _BK_MODEM_DCE_H_
#define _BK_MODEM_DCE_H_

#endif

#include <common/bk_include.h>

extern bool bk_modem_dce_send_at(void);
extern bool bk_modem_dce_check_sim(void);
extern bool bk_modem_dce_check_signal(void);
extern bool bk_modem_dce_check_register(void);
extern bool bk_modem_dce_set_apn(void);
extern bool bk_modem_dce_check_attach(void);
extern bool bk_modem_dce_start_ppp(void);
extern bool bk_modem_dce_enter_cmd_mode(void);
extern bool bk_modem_dce_stop_ppp(void);

