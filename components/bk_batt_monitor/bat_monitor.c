#include <common/bk_include.h>
#include <common/bk_typedef.h>
#include "bk_arm_arch.h"
#include "bk_gpio.h"
#include "bat_monitor.h"
#include "multi_button.h"
#include <os/os.h>
#include <os/mem.h>
#include <common/bk_kernel_err.h>
#include <driver/gpio.h>
#include <driver/hal/hal_gpio_types.h>
#include "gpio_driver.h"
#include "adc_hal.h"
#include "adc_statis.h"
#include "adc_driver.h"
#include <driver/adc.h>
#include "sys_driver.h"
#include "iot_adc.h"
#include "bk_saradc.h"
#if CONFIG_PM_ENABLE
#include <modules/pm.h>
#endif

#if CONFIG_BAT_MONITOR
static bool s_charging_init_status_flag = 0;
beken_thread_t  charge_detect_thread_hdl = NULL;


#define BAT_DETECT_ONESHOT_TIMER               1
#define BAT_DETEC_ADC_CLK	                    203125
#define BAT_DETEC_ADC_SAMPLE_RATE	            0
#define BAT_DETEC_ADC_STEADY_CTRL	            7

#define ADC_VOL_BUFFER_SIZE                        (5+5)//(+5 for skip)
#define ADC_READ_SEMAPHORE_WAIT_TIME	1000

static uint16_t *s_raw_voltage_data = NULL;

uint16_t tempd_calculate_voltage(void)
{
    uint32_t sum = 0, index, count = 0;

    if (s_raw_voltage_data == NULL) {
        printf("Error: s_raw_voltage_data is NULL.\r\n");
        return 0;
    }

    for (index = 5; index < ADC_VOL_BUFFER_SIZE; index++) {
        /* 0 is invalid, but SAR ADC may return 0 in power save mode */
        if ((s_raw_voltage_data[index] != 0) && (s_raw_voltage_data[index] != 2048)) {
            sum += s_raw_voltage_data[index];
            count++;
        }
    }

    if (count == 0) {
        s_raw_voltage_data[0] = 0;
    } else {
        s_raw_voltage_data[0] = (uint16_t)(sum / count);
    }

    return s_raw_voltage_data[0];
}


void start_battery_adc_get_one_time()
{
	uint16_t vol = 0;
	BK_LOG_ON_ERR(bk_adc_acquire());
	BK_LOG_ON_ERR(bk_adc_init(ADC_0));
	adc_config_t config = {0};

	config.chan = ADC_0;
	config.adc_mode = ADC_CONTINUOUS_MODE;
	config.src_clk = ADC_SCLK_XTAL_26M;
	config.clk = BAT_DETEC_ADC_CLK;//
	config.saturate_mode = 4;
	config.steady_ctrl= BAT_DETEC_ADC_STEADY_CTRL;
	config.adc_filter = 0;
	config.sample_rate = BAT_DETEC_ADC_SAMPLE_RATE;

	if(config.adc_mode == ADC_CONTINUOUS_MODE)
	{
		config.sample_rate = 0;
	}

	BK_LOG_ON_ERR(bk_adc_set_config(&config));
	BK_LOG_ON_ERR(bk_adc_enable_bypass_clalibration());
	BK_LOG_ON_ERR(bk_adc_start());

	bk_err_t ret = bk_adc_read_raw(s_raw_voltage_data, ADC_VOL_BUFFER_SIZE, ADC_READ_SEMAPHORE_WAIT_TIME);
	if (ret != BK_OK) {
		printf("Error: Failed to read ADC data, error code: %d\r\n", ret);
		return;
	}

	vol = tempd_calculate_voltage();
	printf("ADC Voltage in start_battery_adc_get_one_time: %d.\r\n", vol);

	BK_LOG_ON_ERR(bk_adc_stop());
    BK_LOG_ON_ERR(bk_adc_deinit(ADC_0));
    BK_LOG_ON_ERR(bk_adc_release());

}

void check_charge_status()
{
	int charge_state = bk_gpio_get_input(GPIO_CHARGE);
    int full_state = bk_gpio_get_input(GPIO_FULL);
	//printf("charge_state = %d.\r\n",charge_state);
	//printf("full_state = %d.\r\n",full_state);
    if (charge_state == 1 && full_state == 1) {
        printf("Device is charging...\n");
    } else if (charge_state == 1 && full_state == 0) {
        printf("Battery is full.\n");
    } else {
        printf("Not power input.\n");
    }
}

static void charge_detect_test_main(void)
{
    while (s_charging_init_status_flag)
    {
        check_charge_status();
        //start_battery_adc_get_one_time();

        //uint16_t vol = tempd_calculate_voltage();
        //printf("Battery vol in charge_detect_test_main: %d.\r\n", vol);

        rtos_delay_milliseconds(60*1000);
    }

    if (s_raw_voltage_data) {
        os_free(s_raw_voltage_data);
        s_raw_voltage_data = NULL;
    }

    charge_detect_thread_hdl = NULL;
    rtos_delete_thread(NULL);
}

static bk_err_t charge_detect_task_init(void)
{
    if (charge_detect_thread_hdl != NULL)
    {
        printf("Charge detect task already running.\r\n");
        return BK_OK;
    }

    s_raw_voltage_data = (uint16_t *)os_malloc(ADC_VOL_BUFFER_SIZE * sizeof(uint16_t));
    if (s_raw_voltage_data == NULL) {
        printf("Error: Failed to allocate memory for s_raw_voltage_data.\r\n");
        return BK_ERR_NO_MEM;
    }

    bk_err_t ret = rtos_create_thread(&charge_detect_thread_hdl,
                                      4,
                                      "charge_detect",
                                      (beken_thread_function_t)charge_detect_test_main,
                                      1024,
                                      (beken_thread_arg_t)NULL);
    if (ret != BK_OK)
    {
        charge_detect_thread_hdl = NULL;
        os_free(s_raw_voltage_data);
        s_raw_voltage_data = NULL;
        printf("Error: Failed to create charge_detect test task: %d\r\n", ret);
        return BK_ERR_NOT_INIT;
    }
    return BK_OK;
}


void charging_detect_init(void)
{
	if(s_charging_init_status_flag) {
		BAT_MONITOR_PRT("charging detect has inited\n");
		return;
	}

	if(charge_detect_task_init()!= kNoErr)
	{
		BAT_MONITOR_PRT("charging detect task create failed!\n");
		return;
	}
	s_charging_init_status_flag = 1;
}
void charging_detect_deinit(void)
{
	if(!s_charging_init_status_flag) {
		BAT_MONITOR_PRT("charging detect deinit\n");
		return;
	}
	s_charging_init_status_flag = 0;
	if (charge_detect_thread_hdl) {
		rtos_delete_thread(&charge_detect_thread_hdl);
		charge_detect_thread_hdl = NULL;
	}

	BAT_MONITOR_PRT("Charging detect deinitialized\n");
}
#endif
