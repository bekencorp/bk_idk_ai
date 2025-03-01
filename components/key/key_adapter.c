#include <stdio.h>
#include <os/os.h>
#include <key_main.h>
#include <key_adapter.h>
#include <driver/gpio.h>
#include <gpio_driver.h>

#define KEY_MSG_QUEUE_NAME "key_queue"
#define KEY_MSG_QUEUE_COUNT (10)
static beken_queue_t s_key_msgqueue = NULL;


#define KEY_THREAD_PRIORITY (4)
#define KEY_THREAD_NAME "key_thread"
#define KEY_THREAD_STACK_SIZE (0x2<<10)
static beken_thread_t s_key_thread = NULL;

static KeyConfig_t *key_configs = NULL;
static key_handler_t global_handler = NULL;
static uint8_t key_count = 0;



static void short_press_cb(void *param);
static void double_press_cb(void *param);
static void long_press_cb(void *param);
static void key_thread(void *param);

void bk_init_keys()
{
    key_initialization();
    
}

void bk_deinit_keys()
{
    key_uninitialization();
 
}


void bk_configure_key(KeyConfig_t *KeyConfig)
{
    
    bk_err_t ret;

#if 1
    if((KeyConfig->short_event == POWER_ON) || (KeyConfig->double_event == POWER_ON) || (KeyConfig->long_event == POWER_ON))
	{
		if(KeyConfig->active_level == LOW_LEVEL_TRIGGER)
			bk_gpio_register_wakeup_source(KeyConfig->gpio_id, GPIO_INT_TYPE_FALLING_EDGE);
		else
			bk_gpio_register_wakeup_source(KeyConfig->gpio_id, GPIO_INT_TYPE_RISING_EDGE);
	}
#endif

    ret = key_item_configure(KeyConfig->gpio_id, 
                                 KeyConfig->active_level, 
                                 short_press_cb, 
                                 double_press_cb, 
                                 long_press_cb, 
                                 NULL);
    
    if (ret != BK_OK)
    {
        os_printf("key_config failed\r\n");
        return;
    }
}


void bk_key_driver_init(KeyConfig_t* configs, uint8_t num_keys) {

    bk_err_t ret;

    bk_init_keys();
    key_configs = configs;
    key_count = num_keys;
    for (uint8_t i = 0; i < num_keys; i++) {
        bk_configure_key(configs+i);
    }

    ret = rtos_init_queue(
							&s_key_msgqueue,
							KEY_MSG_QUEUE_NAME,
							sizeof(KeyEventMsg_t),
							KEY_MSG_QUEUE_COUNT
						);

	if (kNoErr != ret)
	{
		os_printf("init queue ret=%d", ret);
        goto err_exit;
	}

    	ret = rtos_create_thread(
								&s_key_thread,
								KEY_THREAD_PRIORITY,
								KEY_THREAD_NAME,
								key_thread,
								KEY_THREAD_STACK_SIZE,
								NULL
							);


    if (kNoErr != ret)
	{
		os_printf("init thread ret=%d", ret);
		goto err_exit;
	}
    
    return ;

err_exit:
	if(s_key_msgqueue)
	{
		rtos_deinit_queue(&s_key_msgqueue);
	}

	if(s_key_thread)
	{
		rtos_delete_thread(&s_key_thread);
        s_key_thread = NULL;
	}

    return ;
}

void bk_key_driver_deinit(KeyConfig_t* configs, uint8_t num_keys){

    for (uint8_t i = 0; i < num_keys; i++) {
        key_item_unconfigure((configs+i)->gpio_id);
    }

    bk_deinit_keys();
}

void register_event_handler(key_handler_t handler) {
    if (handler == NULL){
        os_printf("null ptr funtion is %s\r\n",__func__);
    }
    global_handler = handler;
}

// 内部事件处理
static void process_key_event(uint8_t gpio_id, key_action_t action) {
    
    if(!global_handler) {
        return;
    }

    for(uint8_t i=0; i<key_count; i++) {
        if(key_configs[i].gpio_id == gpio_id) {
            key_event_t event = EVENT_NONE;
            switch(action) {
                case SHORT_PRESS: 
                    event = key_configs[i].short_event; 
                    break;
                case DOUBLE_PRESS: 
                    event = key_configs[i].double_event; 
                    break;
                case LONG_PRESS: 
                    event = key_configs[i].long_event; 
                    break;
                default:
                    break;
            }

            if(event != EVENT_NONE) {
                global_handler(event); 
            }
            break;
        }
    }
}

static void key_thread(void *param){

    KeyEventMsg_t rec_msg;

    while (1)
    {
        if(rtos_pop_from_queue(&s_key_msgqueue, &rec_msg, BEKEN_WAIT_FOREVER) == kNoErr){

            process_key_event(rec_msg.gpio_id, rec_msg.action);
        }
    }
    
}

void short_press_cb(void *param) {

    bk_err_t ret;

    BUTTON_S * handle = (BUTTON_S *)param;
   
    uint32_t gpio_id = (uint32_t)(handle->user_data);

        KeyEventMsg_t msg = {
        .gpio_id = gpio_id,
        .action = SHORT_PRESS
    };

   ret = rtos_push_to_queue(&s_key_msgqueue, &msg, 1000);

    if (kNoErr != ret){
		os_printf("key send msg failed");
	}

   
}

void double_press_cb(void *param) {

    bk_err_t ret;

    BUTTON_S * handle = (BUTTON_S *)param;
    uint32_t gpio_id = (uint32_t)(handle->user_data);
    
    KeyEventMsg_t msg = {
        .gpio_id = gpio_id,
        .action = DOUBLE_PRESS
    };

    ret = rtos_push_to_queue(&s_key_msgqueue, &msg, 1000);

    if (kNoErr != ret){
		os_printf("key send msg failed");
	}

     
}

void long_press_cb(void *param) {

    bk_err_t ret;

    BUTTON_S * handle = (BUTTON_S *)param;
    uint32_t gpio_id = (uint32_t)(handle->user_data);

    KeyEventMsg_t msg = {
        .gpio_id = gpio_id,
        .action = LONG_PRESS
    };

    ret = rtos_push_to_queue(&s_key_msgqueue, &msg, 1000);

    if (kNoErr != ret){
		os_printf("key send msg failed");
	}
    
   
}
