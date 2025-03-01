// Copyright 2020-2024 Beken
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
#include <driver/flash.h>

static volatile int    flash_svr_test_start = 1;

static u8    test_buff[512];
static u8    test_init = 0;

void flash_svr_test_task(void * param)
{
	// wait start.
	while(flash_svr_test_start == 0)
	{
		bk_printf("==> test_start addr=0x%x.\r\n", &flash_svr_test_start);
		rtos_delay_milliseconds(500);
	}

	rtos_delay_milliseconds(2000);

	#if CONFIG_SYS_CPU1
	u32  flash_start_addr = 0x288000;
	#endif
	#if CONFIG_SYS_CPU2
	u32  flash_start_addr = 0x288000 + 0x80000;
	#endif
	#if CONFIG_SYS_CPU0
	u32  flash_start_addr = 0x288000 + 0x40000;
	#endif

	// u32  flash_len = 0x30000;
	int  ret_val = 0;
	u32  test_addr;

	for(int i = 0; i < 2000; i++)
	{
		bk_printf("\r\n==== loop %d !====\r\n", i);

		for(int j = 0; j < 16; j++)
		{
			int    k = 0;
			
			rtos_delay_milliseconds(5);
			
			test_addr = flash_start_addr + j * 0x1000;

			//bk_printf("test-addr = 0x%x!\r\n", test_addr);

			ret_val = bk_flash_erase_sector(test_addr);

			if(ret_val != 0)
			{
				bk_printf("erase failed, addr=0x%x!\r\n", test_addr);
				continue;;
			}
			
			ret_val = bk_flash_read_bytes(test_addr, test_buff, sizeof(test_buff));

			if(ret_val != 0)
			{
				bk_printf("read failed, addr=0x%x!\r\n", test_addr);
				continue;;
			}

			for(k = 0; k < sizeof(test_buff); k++)
			{
				if(test_buff[k] != 0xFF)
				{
					bk_printf("erase data failed, addr=0x%x, %02x!\r\n", test_addr+k, test_buff[k]);
					break;
				}

				test_buff[k] = (k & 0xFF);
			}

			if(k < sizeof(test_buff))
				continue;;
			
			ret_val = bk_flash_write_bytes(test_addr, test_buff, sizeof(test_buff));
			if(ret_val != 0)
			{
				bk_printf("write failed, addr=0x%x!\r\n", test_addr);
				continue;;
			}

			memset(test_buff, 0, sizeof(test_buff));

			ret_val = bk_flash_read_bytes(test_addr, test_buff, sizeof(test_buff));

			if(ret_val != 0)
			{
				bk_printf("read2 failed, addr=0x%x!\r\n", test_addr);
				continue;;
			}

			for(k = 0; k < sizeof(test_buff); k++)
			{
				if(test_buff[k] != (k & 0xFF))
				{
					bk_printf("write data failed, addr=0x%x, %02x!\r\n", test_addr+k, test_buff[k]);
					break;
				}
			}
			
		}
	}
	
	bk_printf("task exit\r\n");
	rtos_delete_thread(NULL);
	
}

int flash_svr_test_init(void)
{
	if(test_init != 0)
		return 0;

	test_init = 1;
	
	rtos_create_thread(NULL, 4, "flash_test", flash_svr_test_task, 2048, NULL);

	return 0;
}


