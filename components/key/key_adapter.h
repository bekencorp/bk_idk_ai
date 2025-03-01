/*************************************************************
 *
 * This is a part of the key Framework Library.
 * Copyright (C) 2021 Agora IO
 * All rights reserved.
 *
 *************************************************************/
#ifndef __KEY_ADAPTER_H__
#define __KEY_ADAPTER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    EVENT_NONE = 0,
    VOLUME_UP,
    VOLUME_DOWN,
    SHUT_DOWN,
    POWER_ON,
    CONFIG_NETWORK,
    BRIGHTNESS_ADD,
    AI_AGENT_CONFIG,
} key_event_t;

typedef enum{
    SHORT_PRESS = 0,
    DOUBLE_PRESS = 1,
    LONG_PRESS =2
} key_action_t;

typedef struct {
    uint8_t gpio_id;
    uint8_t active_level;
    key_event_t short_event;   // 短按对应业务事件
    key_event_t double_event;  // 双按对应业务事件
    key_event_t long_event;    // 长按对应业务事件
} KeyConfig_t;

typedef struct
{
    uint8_t gpio_id;
    key_action_t action;
} KeyEventMsg_t;

typedef void (*key_handler_t)(key_event_t event);


// 初始化驱动
void bk_key_driver_init(KeyConfig_t* configs, uint8_t num_keys);

void bk_key_driver_deinit(KeyConfig_t* configs, uint8_t num_keys);
// 注册全局事件处理器（业务层实现）
void register_event_handler(key_handler_t handler);


#ifdef __cplusplus
}
#endif
#endif 