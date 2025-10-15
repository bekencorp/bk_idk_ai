#include <stdlib.h>
#include "cli.h"
#include <components/bk_uid.h>
#include <driver/otp.h>

static void cli_uid_help(void)
{
	CLI_LOGI("uid [init/get] \r\n");
}

static void cli_uid_ops_cmd(char* pcWriteBuffer, int xWriteBufferLen, int argc, char** argv)
{
	if (argc < 2) {
		cli_uid_help();
		return;
	}

	if (os_strcmp(argv[1], "init") == 0) {
		BK_LOG_ON_ERR(bk_uid_driver_init());
	} else if (os_strcmp(argv[1], "get") == 0) {
		unsigned char data[32];
		BK_LOG_ON_ERR(bk_uid_get_data(data));
		for(int j = 0;j < 32; j++)
		{
			CLI_LOGI("%x index:%d\r\n", data[j], j);
		}
	#if (CONFIG_WANSON_CN_LICENSE || CONFIG_WANSON_FL_LICENSE)
		uint8_t chipid_buff[8] = { 0 };
		int rets = bk_otp_apb_read(29, chipid_buff, 8);
		if (rets != BK_OK) {
			bk_printf("chipid_read fail, rets: %d \n", rets);
		} else {
			bk_set_printf_enable(0);
			shell_log_flush();
			for (int j = 0; j < 8; j++) {
				BK_DUMP_OUT("chipid[%d] = 0x%02X\r\n", j, chipid_buff[j]);
			}
			bk_set_printf_enable(1);
		}
	#endif
	} else {
		cli_uid_help();
		return;
	}
}

#if (CONFIG_WANSON_CN_LICENSE || CONFIG_WANSON_FL_LICENSE)
static void lic_ops_cmd(char* pcWriteBuffer, int xWriteBufferLen, int argc, char** argv)
{
	if (argc < 2) {
		return;
	}

	if (os_strcmp(argv[1], "init") == 0) {

	} else if (os_strcmp(argv[1], "get") == 0)
	{
		unsigned char customer_buff[8] = { 0 };
		int  rets = bk_otp_ahb_read(OTP_CUSTOMER_KEY, customer_buff, 8);
		if (rets != BK_OK) {
			bk_printf("%s, %d, bk_otp_ahb_read fail\n", __func__, __LINE__);
			return;
		}

		bk_set_printf_enable(0);
		shell_log_flush();
		for (int j = 0; j < 8; j++) {
			BK_DUMP_OUT("license[%d] = 0x%02X\r\n", j, customer_buff[j]);
		}
		bk_set_printf_enable(1);
	} else if (os_strcmp(argv[1], "fget") == 0)
	{
		unsigned char customer_buff[256] = { 0 };
		int  rets = bk_otp_ahb_read(OTP_FOREIGN_KEY, customer_buff, 256);
		if (rets != BK_OK) {
			bk_printf("%s, %d, bk_otp_ahb_read fail\n", __func__, __LINE__);
			return;
		}
		bk_set_printf_enable(0);
		shell_log_flush();
		for (int j = 0; j < 256; j++) {
			BK_DUMP_OUT("license[%d] = 0x%02X\r\n", j, customer_buff[j]);
		}
		bk_set_printf_enable(1);
	} else
	{
		return;
	}
}
#endif

#define UID_CMD_CNT (sizeof(s_uid_commands) / sizeof(struct cli_command))
static const struct cli_command s_uid_commands[] = {
	{"uid", "uid [init/get]/", cli_uid_ops_cmd},
#if (CONFIG_WANSON_CN_LICENSE || CONFIG_WANSON_FL_LICENSE)
	{"lic", "lic [init/get/fget]/", lic_ops_cmd},
#endif
};

int cli_uid_init(void)
{
	return cli_register_commands(s_uid_commands, UID_CMD_CNT);
}
