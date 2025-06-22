#include <os/os.h>
#include "cli.h"
#include <driver/irda.h>

static uint16_t send_data[]={9020, 4410, 570, 1670 ,570, 1670, 570, 1670, 570,580 ,1670, 570, 1670 ,570 ,570, 580 ,570 ,1670, 570, 1670, 580 ,1650 ,580, 570 ,570, 570, 580 ,570 ,580, 1670, 570 ,1670 ,570, 570, 580, 550,10938};

static void cli_irda_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	if (os_strcmp(argv[1], "tx") == 0) {
		irda_tx_init_config_t tx_config = {0};
		tx_config.tx_initial_level = 0;
		tx_config.clk_freq_input = 26;
		tx_config.carrier_period_cycle = 26;
		tx_config.carrier_duty_cycle = 10;
		bk_irda_init_tx(&tx_config);

		bk_irda_write_data(send_data, sizeof(send_data)/sizeof(send_data[0]));

		CLI_LOGI("irda tx test\r\n");
	} else if (os_strcmp(argv[1], "rx") == 0) {
		irda_rx_init_config_t rx_config = {0};
		rx_config.clk_freq_input = 26;
		rx_config.rx_initial_level = 0;
		rx_config.rx_timeout_us = 8000;
		rx_config.rx_start_threshold_us = 1000;
		bk_irda_init_rx(&rx_config);

		uint16_t recv_buf[50];

		int recv_num = bk_irda_read_data(recv_buf, sizeof(recv_buf)/sizeof(recv_buf[0]));
		CLI_LOGI("recv_num is %d\r\n",recv_num);
		for (int i = 0; i < recv_num; i++) {
			os_printf("recv_buf[%d]:%d\r\n", i, recv_buf[i]);
		}
		CLI_LOGI("irda rx test\r\n");
	} else {

	}
}

#define IRDA_CMD_CNT (sizeof(s_irda_commands) / sizeof(struct cli_command))
static const struct cli_command s_irda_commands[] = {
	{"irda", "irda {start|stop|feed} [...]", cli_irda_cmd}
};

int cli_irda_init(void)
{
	BK_LOG_ON_ERR(bk_irda_driver_init());
	return cli_register_commands(s_irda_commands, IRDA_CMD_CNT);
}