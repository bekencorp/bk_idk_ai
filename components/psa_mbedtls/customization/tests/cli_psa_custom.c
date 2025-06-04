// Copyright 2024-2025 Beken
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

#include "cli.h"
#include <os/mem.h>
#include "tz_test.h"

static void cli_psa_help(void)
{
	CLI_LOGI("psa_crypto [aes_cbc|aes_gcm|ecdh|ecdsa|hmac|tls_client\r\n");
	CLI_LOGI("psa_ps [genkey|encdec|attr|get|destroy\r\n");
}


static void cli_psa_custom_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	if (argc < 2) {
		return;
	}

	if (os_strcmp(argv[1], "aes_cbc") == 0) {
		tz_aes_cbc_test_main();
	} else if (os_strcmp(argv[1], "aes_gcm") == 0) {
		tz_aes_gcm_test_main();
	} else if (os_strcmp(argv[1], "ecdh") == 0) {
		//ecdh_main();
	} else if (os_strcmp(argv[1], "ecdsa") == 0) {
		//tz_ecdsa_test_main();
	} else if (os_strcmp(argv[1], "hmac") == 0) {
		//tz_hmac_test_main();
	} else if (os_strcmp(argv[1], "sha256") == 0) {
		//sha256_main();
	} else if (os_strcmp(argv[1], "tls_client") == 0) {
		//psa_tls_client_main();
	} else {

	}
}

#if CONFIG_TFM_PS
static void cli_psa_key_manage_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	if (argc < 2) {
		return;
	}
	char kn[16];
	uint32_t key_id;

	if (os_strcmp(argv[1], "genkey") == 0) {
		os_strcpy(kn, argv[2]);
		key_id = os_strtoul(argv[3], NULL, 10);
		ps_generate_key_manage(kn, key_id);
	} else if (os_strcmp(argv[1], "encdec") == 0) {
		os_strcpy(kn, argv[2]);
		ps_aes_encrypt_decrypt(kn);
	} else if (os_strcmp(argv[1], "attr") == 0) {
		os_strcpy(kn, argv[2]);
		ps_get_key_attributes(kn);
	} else if (os_strcmp(argv[1], "get") == 0) {
		os_strcpy(kn, argv[2]);
		ps_get_key_id_by_name(kn);
	} else if (os_strcmp(argv[1], "destroy") == 0) {
		os_strcpy(kn, argv[2]);
		ps_destroy_key_by_name(kn);
	} else if (os_strcmp(argv[1], "destroyid") == 0) {
		key_id = os_strtoul(argv[2], NULL, 10);
		ps_destroy_key_by_id(key_id);
	} else
	{
		cli_psa_help();
	}
}
#endif

#if CONFIG_TFM_DUBHE_KEY_LADDER_NSC
static void cli_psa_key_ladder_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	if (argc < 2) {
		return;
	}
	if (os_strcmp(argv[1], "cbc") == 0) {
		os_printf("key_ladder cbc test ret =%d\r\n ",te200_key_ladder_aes_cbc_main());
	}else if (os_strcmp(argv[1], "ecb") == 0) {
		os_printf("key_ladder ecb test ret =%d\r\n ",te200_key_ladder_aes_ecb_main());
	} else
	{
		cli_psa_help();
	}
	
}
#endif

#define PSA_CUSTOMIZATION_CMD_CNT (sizeof(s_psa_customization_commands) / sizeof(struct cli_command))
static const struct cli_command s_psa_customization_commands[] = {
	{"psa_custom", "psa_custom [aes_cbc|aes_gcm|ecdh|ecdsa|hmac|tls_client", cli_psa_custom_cmd},
#if CONFIG_TFM_PS
	{"psa_ps", "psa_ps [genkey|encdec|attr|destroy", cli_psa_key_manage_cmd},
#endif
#if CONFIG_TFM_DUBHE_KEY_LADDER_NSC
	{"key_ladder", "key_ladder [cbc|ecb]", cli_psa_key_ladder_cmd},
#endif
};
int cli_psa_customization_init(void)
{
	return cli_register_commands(s_psa_customization_commands, PSA_CUSTOMIZATION_CMD_CNT);
}
