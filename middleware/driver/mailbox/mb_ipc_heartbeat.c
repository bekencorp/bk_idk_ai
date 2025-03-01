// Copyright 2020-2022 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <string.h>

#include <os/os.h>
#include "mb_ipc_cmd.h"
#include "../../../components/bk_rtos/rtos_ext.h"

#define MOD_TAG		"hrt"

#if (CONFIG_CPU_CNT > 1)

/* define the code section will be compiled. */
#if CONFIG_SOC_SMP 
	#if ((CONFIG_SYS_CPU0) && (CONFIG_SYS_CPU2_COOP_WITH_SMP))
		#define MASTER_HB_TASK
	#endif
#else
	#if CONFIG_SYS_CPU0
		#define MASTER_HB_TASK
	#endif
	#if CONFIG_SYS_CPU1
		#define SLAVE_HB_TASK
	#endif
	#if ((CONFIG_SYS_CPU2) && (CONFIG_SYS_CPU2_COOP_WITH_SMP))
		#define SLAVE_HB_TASK
	#endif
#endif

#if !defined(MASTER_HB_TASK)
void mb_ipc_reset_notify(u32 cpu_id, u32 power_on)
{
	(void)cpu_id;
	(void)power_on;
}
int mb_ipc_cpu_is_power_on(u32 cpu_id)
{
	(void)cpu_id;

	return 0;
}
int mb_ipc_cpu_is_power_off(u32 cpu_id)
{
	(void)cpu_id;

	return 1;
}
#endif

#if defined(MASTER_HB_TASK)

//#include <os/rtos_ext.h>
#include "../../../components/bk_rtos/rtos_ext.h"

#define MB_IPC_START_CORE_FLAG		0x01
#define MB_IPC_STOP_CORE_FLAG		0x02
#define MB_IPC_POWER_UP_FLAG		0x04
#define MB_IPC_HEARTBEAT_FLAG		0x08

#define MB_IPC_ALL_FLAGS			(MB_IPC_START_CORE_FLAG | MB_IPC_STOP_CORE_FLAG | MB_IPC_POWER_UP_FLAG | MB_IPC_HEARTBEAT_FLAG)

enum
{
	CORE_POWER_OFF = 0,
	CORE_STARTING,
	CORE_POWER_ON,
};

static rtos_event_ext_t		mb_ipc_heart_event;
static u32             cpu_x_heartbeat_timestamp = 0;
static volatile u8     cpu_x_state = CORE_POWER_OFF;
static volatile u8     cpu_x_id = 0xFF;   /* invalid ID, */
static volatile u8     cpu_x_dump = 0;

extern void start_cpu1_core(void);
extern void stop_cpu1_core(void);
extern void start_cpu2_core(void);
extern void stop_cpu2_core(void);

static int ipc_heartbeat_timeout(void)
{
	u32   cur_time;

	cur_time = (u32)rtos_get_time();

	if(cpu_x_state == CORE_POWER_OFF)
	{
		return 0;
	}
	if((cpu_x_state == CORE_STARTING) || (cpu_x_dump != 0))
	{
		cpu_x_heartbeat_timestamp = cur_time;
		return 0;
	}

	if(cur_time >= cpu_x_heartbeat_timestamp)
	{
		cur_time -= cpu_x_heartbeat_timestamp;
	}
	else
	{
		cur_time += (~(cpu_x_heartbeat_timestamp)) + 1;  // wrap around. 
	}
	
	if(cur_time < CONFIG_INT_WDT_PERIOD_MS)
	{
		cpu_x_heartbeat_timestamp = (u32)rtos_get_time();
		return 0;
	}

	return 1;
}

static void restart_cpu_x(void)
{
	if(cpu_x_id == 1)
	{
		stop_cpu1_core();
		rtos_delay_milliseconds(6);
		start_cpu1_core();
		return;
	}
	
	if(cpu_x_id == 2)
	{
		stop_cpu2_core();
		rtos_delay_milliseconds(6);
		start_cpu2_core();
		return;
	}
}

static int check_cpu_id_ok(u32 cpu_id)
{
	if(cpu_x_id == 0xFF)
	{
		cpu_x_id = cpu_id;
		return 1;
	}

	if(cpu_x_id != cpu_id)
	{
		BK_LOGE(MOD_TAG, "can't manage multiple cpus!\r\n");
		return 0;
	}

	return 1;
}

static void mb_ipc_task( void *para )
{
	bk_err_t	ret_val;
	u32    events;
	u32    check_time = BEKEN_WAIT_FOREVER;
	
	ret_val = rtos_init_event_ex(&mb_ipc_heart_event);

	if(ret_val != BK_OK)
	{
		rtos_delete_thread(NULL);
		return;
	}

	while(1)
	{
		events = rtos_wait_event_ex(&mb_ipc_heart_event, MB_IPC_ALL_FLAGS, true, check_time);

		if(events == 0)
		{
			// timeout, so check heartbeat.
			events = MB_IPC_HEARTBEAT_FLAG;
		}

		if(events & MB_IPC_STOP_CORE_FLAG)  // process this event at first!!!!
		{
			if(cpu_x_state == CORE_POWER_OFF)
			{
				events = 0;  // clear all events.
			}
		}
		
		if(events & MB_IPC_START_CORE_FLAG)
		{
			u8   retry_cnt = 0;

			while(cpu_x_state == CORE_STARTING)
			{
				ipc_heartbeat_timeout();
				
				if(events & MB_IPC_POWER_UP_FLAG)
				{
					if(cpu_x_state == CORE_STARTING)
					{
						cpu_x_state = CORE_POWER_ON;
						break;  // cpu1 power on. 
					}
				}
				else
				{
					if(retry_cnt > 0)
					{
						BK_LOGE(MOD_TAG, "IPC retry to start core%d\r\n", cpu_x_id);
						// restart_cpu_x();
						break;
					}
					else
					{
						events = rtos_wait_event_ex(&mb_ipc_heart_event, MB_IPC_POWER_UP_FLAG, true, 2000);
					}
				}

				retry_cnt++;
			}

			// discard this event when not in CORE_STARTING state.
		}
		
		if(events & MB_IPC_HEARTBEAT_FLAG)
		{
			if(ipc_heartbeat_timeout())
			{
				BK_LOGE(MOD_TAG, "IPC heartbeat timeout%d\r\n", cpu_x_id);
				/*when cpu1 heatbeat timerout, then system reboot*/
				BK_ASSERT(false);
				// restart_cpu_x();
			}
		}

		if(cpu_x_state == CORE_POWER_OFF)
		{
			check_time = BEKEN_WAIT_FOREVER;
		}
		else
		{
			check_time = CONFIG_INT_WDT_PERIOD_MS;
		}
	}
}

void mb_ipc_reset_notify(u32 cpu_id, u32 power_on)
{
	if(check_cpu_id_ok(cpu_id) == 0)
	{
		return;
	}
	
	if(power_on)
	{
		if(cpu_x_state != CORE_POWER_ON)
		{
			cpu_x_state = CORE_STARTING;
			rtos_set_event_ex(&mb_ipc_heart_event, MB_IPC_START_CORE_FLAG);
		}
	}
	else
	{
		cpu_x_state = CORE_POWER_OFF;
		rtos_set_event_ex(&mb_ipc_heart_event, MB_IPC_STOP_CORE_FLAG);
	}
}

void mb_ipc_heartbeat_notify(u32 cpu_id)
{
	if(check_cpu_id_ok(cpu_id) == 0)
	{
		return;
	}
	
	rtos_set_event_ex(&mb_ipc_heart_event, MB_IPC_HEARTBEAT_FLAG);
}

void mb_ipc_power_on_notify(u32 cpu_id)
{
	if(check_cpu_id_ok(cpu_id) == 0)
	{
		return;
	}
	
	rtos_set_event_ex(&mb_ipc_heart_event, MB_IPC_POWER_UP_FLAG);
}

void mb_ipc_dump_notify(u32 cpu_id, u32 dump)
{
	if(check_cpu_id_ok(cpu_id) == 0)
	{
		return;
	}
	
	cpu_x_dump = (dump != 0);
}

int mb_ipc_cpu_is_power_on(u32 cpu_id)
{
	if((cpu_x_id == 0xFF) || (cpu_x_id != cpu_id))
	{
		return 0;
	}
	
	if(cpu_x_state == CORE_POWER_ON)
	{
		return 1;
	}

	return 0;
}

int mb_ipc_cpu_is_power_off(u32 cpu_id)
{
	if((cpu_x_id == 0xFF) || (cpu_x_id != cpu_id))
	{
		return 1;
	}
	
	if(cpu_x_state == CORE_POWER_OFF)
	{
		return 1;
	}

	return 0;
}

#endif

#if defined(SLAVE_HB_TASK)

#define MB_IPC_HEARTBEAT_TIME		2000

static void mb_ipc_task( void *para )
{
	ipc_send_power_up();

	while(1)
	{
		rtos_delay_milliseconds(MB_IPC_HEARTBEAT_TIME);
		ipc_send_heart_beat(0);
	}
}

#endif

bk_err_t mb_ipc_heartbeat_init(void)
{
	bk_err_t	ret_val = BK_FAIL;

#if defined(MASTER_HB_TASK) || defined(SLAVE_HB_TASK)
	ret_val = rtos_create_thread(NULL, BEKEN_DEFAULT_WORKER_PRIORITY, "heartbeat", mb_ipc_task, 1536, 0);
#endif

	if(ret_val != BK_OK)
	{
		BK_LOGE(MOD_TAG, "heartbeat task failed at line %d: %d\r\n", __LINE__, ret_val);
	}

	return ret_val;	
}

#endif

