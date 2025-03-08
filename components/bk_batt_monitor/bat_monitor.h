#ifndef _BAT_MONITOR_H_
#define _BAT_MONITOR_H_

#include "bk_gpio.h"
#include "bk_uart.h"
#include "multi_button.h"

#define BAT_MONITOR_DEBUG

#ifdef BAT_MONITOR_DEBUG
#define BAT_MONITOR_PRT                 os_printf
#define BAT_MONITOR_WPRT                warning_prf
#else
#define BAT_MONITOR_PRT                 os_null_printf
#define BAT_MONITOR_WPRT                os_null_printf
#endif

#define GPIO_CHARGE      GPIO_51  // 充电状态 GPIO
#define GPIO_FULL        GPIO_26  // 充满状态 GPIO

void check_charge_status(void);
void charging_detect_init(void);
void charging_detect_deinit(void);


#endif //
