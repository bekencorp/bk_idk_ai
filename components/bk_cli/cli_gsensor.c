#include <stdlib.h>
#include "cli.h"
#include <components/bk_gsensor.h>

static void cli_gsensor_help(void)
{
	CLI_LOGI("gsensor [init/deinit/open/close/set_normal/set_wakeup] \r\n");
}

extern bk_err_t gsensor_demo_init(void);
extern void gsensor_demo_deinit(void);
extern bk_err_t gsensor_demo_open();
extern bk_err_t gsensor_demo_close();
extern bk_err_t gsensor_demo_set_normal();
extern bk_err_t gsensor_demo_set_wakeup();
extern bk_err_t gsensor_demo_lowpower_wakeup();

static void cli_gsensor_ops_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	if (argc < 2)
	{
		cli_gsensor_help();
		return;
	}

	if (os_strcmp(argv[1], "init") == 0) {
		BK_LOG_ON_ERR(gsensor_demo_init());
	}else if (os_strcmp(argv[1], "deinit") == 0) {
		gsensor_demo_deinit();
	} else if (os_strcmp(argv[1], "open") == 0) {
		BK_LOG_ON_ERR(gsensor_demo_open());
	} else if(os_strcmp(argv[1], "close") == 0) {
		BK_LOG_ON_ERR(gsensor_demo_close());
	} else if(os_strcmp(argv[1], "set_normal") == 0) {
		BK_LOG_ON_ERR(gsensor_demo_set_normal());
	} else if(os_strcmp(argv[1], "set_wakeup") == 0) {
		BK_LOG_ON_ERR(gsensor_demo_set_wakeup());
	} else if(os_strcmp(argv[1], "set_lowpower") == 0) {
		BK_LOG_ON_ERR(gsensor_demo_lowpower_wakeup());
	} else {
		cli_gsensor_help();
		return;
	}
}

#define GSENSOR_CMD_CNT (sizeof(s_gsensor_commands) / sizeof(struct cli_command))
static const struct cli_command s_gsensor_commands[] = {
	{"gsensor", "gsensor [init/deinit/open/close/set_normal/set_wakeup]/", cli_gsensor_ops_cmd},
};

int cli_gsensor_init(void)
{
	return cli_register_commands(s_gsensor_commands, GSENSOR_CMD_CNT);
}
