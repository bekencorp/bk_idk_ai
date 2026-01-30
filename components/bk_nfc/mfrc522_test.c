/*********************************************************************
 * INCLUDES
 */
#include "os/os.h"
#include "os/mem.h"
#include <string.h>
#include "board_mfrc522.h"
#include "components/bk_nfc.h"
#include "cli.h"
#include <driver/gpio.h>

extern bk_err_t bk_pm_module_vote_ctrl_external_ldo(gpio_ctrl_ldo_module_e module,gpio_id_t gpio_id,gpio_output_state_e value);
extern uint8_t bk_mfrc522_read_card_id(uid_num_t *uid, uint8_t validBits);
extern MFRC522_PICC_Type_t bk_mfrc522_get_type(uint8_t sak);
extern void bk_mfrc522_set_low_power(void);
/**
 @brief 测试MFRC522的功能， Search for card --防碰撞---选定卡---验证卡片密码--写数据---读数据
*/
static int nfc_test_main(void)
{
	uint8_t i;
	uint8_t Card_Type1[2];
	uint8_t Card_ID[4];
	uint8_t Card_KEY[6] = {0xff,0xff,0xff,0xff,0xff,0xff};    //{0x11,0x11,0x11,0x11,0x11,0x11};
	uint8_t Card_Data[16];
	uint8_t status;

	MFRC522_LOGI("enter test !!!!!!!\n\r",__FUNCTION__);
	bk_pm_module_vote_ctrl_external_ldo(GPIO_CTRL_LDO_MODULE_NFC, CONFIG_LDO3V3_CTRL_GPIO, GPIO_OUTPUT_STATE_HIGH);
	bk_nfc_init();
	while(1)
	{
		rtos_delay_milliseconds(10);
		if(MI_OK==bk_mfrc522_request(MFRC522_PICC_REQALL, Card_Type1))
		{
			uint16_t cardType = (Card_Type1[0]<<8)|Card_Type1[1];
			MFRC522_LOGI("Card Type(0x%04X):",cardType);
			switch(cardType){
			case 0x4400:
					MFRC522_LOGI("Mifare UltraLight\n\r");
					break;
			case 0x0400:
					MFRC522_LOGI("Mifare One(S50)\n\r");
					break;
			case 0x0200:
					MFRC522_LOGI("Mifare One(S70)\n\r");
					break;
			case 0x0800:
					MFRC522_LOGI("Mifare Pro(X)\n\r");
					break;
			case 0x4403:
					MFRC522_LOGI("Mifare DESFire\n\r");
					break;
			default:
					MFRC522_LOGI("Unknown Card\n\r");
					continue;
			}
			//rtos_delay_milliseconds(10);
			status = bk_mfrc522_anticoll(Card_ID);//����ײ
			if(status != MI_OK){
				MFRC522_LOGE("Anticoll Error\n\r");
				continue;
			}else{
					MFRC522_LOGI("Serial Number:%02X%02X%02X%02X\n\r",Card_ID[0],Card_ID[1],Card_ID[2],Card_ID[3]);
			}
			status = bk_mfrc522_select(Card_ID);  //ѡ��
			if(status != MI_OK){
				MFRC522_LOGE("Select Card Error\n\r");
				continue;
			}
			MFRC522_LOGI("Select Card ok\n\r");
			status = bk_mfrc522_authState(MFRC522_PICC_AUTHENT1A,5,Card_KEY,Card_ID);
			if(status != MI_OK){
				MFRC522_LOGE("Auth State Error\n\r");
				continue;
			}
			MFRC522_LOGI("Auth State ok\n\r");
			memset(Card_ID,1,4);
			memset(Card_Data,1,16);
			Card_Data[0]=0xaa;
			status = bk_mfrc522_write(5,Card_Data);                   //д��0XAA,0X01,0X01����
			if(status != MI_OK){
				MFRC522_LOGE("Card Write Error\n\r");
				continue;
			}
			MFRC522_LOGI("Card Write ok\n\r");
			memset(Card_Data,0,16);
			rtos_delay_milliseconds(8);
			
			status = bk_mfrc522_read(5,Card_Data);                    //��һ�ΰ�����ȡ����16�ֽڵĿ�Ƭ����
			if(status != MI_OK){
				MFRC522_LOGE("Card Read Error\n\r");
				continue;
			}else{
				MFRC522_LOGI("Card Read ok\n\r");
				for(i=0;i<16;i++){
					MFRC522_LOGI("%02X ",Card_Data[i]);
				}
				MFRC522_LOGI("\n\r");
			}
			
			memset(Card_Data,2,16);
			Card_Data[0]=0xbb;
			rtos_delay_milliseconds(8);
			status = bk_mfrc522_write(5,Card_Data);                   //д��0Xbb,0X02,0X02����
			if(status != MI_OK){
				MFRC522_LOGE("Card Write Error\n\r");
				continue;
			}
			rtos_delay_milliseconds(8);
			MFRC522_LOGI("Card Write ok2\n\r");
			status = bk_mfrc522_read(5,Card_Data);                    //��һ�ΰ�����ȡ����16�ֽڵĿ�Ƭ����
			if(status != MI_OK){
				MFRC522_LOGE("Card Read Error\n\r");
				continue;
			}else{
				MFRC522_LOGI("Card read ok2\n\r");
				for(i=0;i<16;i++){
					MFRC522_LOGI("%02X ",Card_Data[i]);
				}
				MFRC522_LOGI("\n\r");
			}
			memset(Card_Data,0,16);
			bk_nfc_deinit();
			bk_pm_module_vote_ctrl_external_ldo(GPIO_CTRL_LDO_MODULE_NFC, CONFIG_LDO3V3_CTRL_GPIO, GPIO_OUTPUT_STATE_LOW);
		}
		return 0 ;
	}
}


static void cli_nfc_test(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	nfc_test_main();
}

static uint8_t s_card_id[4];

static void cli_nfc_cmd_test(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	uint8_t Card_Type1[2];
	uint8_t Card_KEY[6] = {0xff,0xff,0xff,0xff,0xff,0xff};    //{0x11,0x11,0x11,0x11,0x11,0x11};
	uint8_t status;

	MFRC522_LOGI("enter cmd test !!!!!!!\n\r",__FUNCTION__);

	if (argc < 2)
	{
		MFRC522_LOGI("param is invaild !!!!!!!\n\r",__FUNCTION__);
		return;
	}
	bk_pm_module_vote_ctrl_external_ldo(GPIO_CTRL_LDO_MODULE_NFC, CONFIG_LDO3V3_CTRL_GPIO, GPIO_OUTPUT_STATE_HIGH);
	if (os_strcmp(argv[1], "init") == 0) {
		bk_nfc_init();
		MFRC522_LOGE("init ok\n\r");
	}else if (os_strcmp(argv[1], "request") == 0) {
		if(MI_OK==bk_mfrc522_request(MFRC522_PICC_REQALL, Card_Type1))
		{
			uint16_t cardType = (Card_Type1[0]<<8)|Card_Type1[1];
			MFRC522_LOGI("Card Type(0x%04X):",cardType);
			switch(cardType){
			case 0x4400:
					MFRC522_LOGI("Mifare UltraLight\n\r");
					break;
			case 0x0400:
					MFRC522_LOGI("Mifare One(S50)\n\r");
					MFRC522_LOGI("request ok\n\r");
					break;
			case 0x0200:
					MFRC522_LOGI("Mifare One(S70)\n\r");
					break;
			case 0x0800:
					MFRC522_LOGI("Mifare Pro(X)\n\r");
					break;
			case 0x4403:
					MFRC522_LOGI("Mifare DESFire\n\r");
					break;
			default:
					MFRC522_LOGI("Unknown Card\n\r");
			}
		}
	} else if (os_strcmp(argv[1], "anticoll") == 0) {
		status = bk_mfrc522_anticoll(s_card_id);
		if(status != MI_OK){
			MFRC522_LOGE("Anticoll Error\n\r");
		}else{
			MFRC522_LOGI("Serial Number:%02X%02X%02X%02X\n\r",s_card_id[0],s_card_id[1],s_card_id[2],s_card_id[3]);
			MFRC522_LOGI("Anticoll ok\n\r");
		}
	} else if(os_strcmp(argv[1], "select") == 0) {
		status = bk_mfrc522_select(s_card_id);
		if(status != MI_OK){
			MFRC522_LOGE("Select Card Error\n\r");
		}
		else{
			MFRC522_LOGI("Select Card ok\n\r");
	    }
	} else if(os_strcmp(argv[1], "authState") == 0) {
		status = bk_mfrc522_authState(MFRC522_PICC_AUTHENT1A,5,Card_KEY,s_card_id);
		if(status != MI_OK){
			MFRC522_LOGE("Auth State Error\n\r");
		}
		else{
			MFRC522_LOGI("Auth State ok\n\r");
		}
	} else if(os_strcmp(argv[1], "deinit") == 0){
		extern void nfc_delete_get_id_task(void);
		nfc_delete_get_id_task();
		bk_nfc_deinit();
		bk_pm_module_vote_ctrl_external_ldo(GPIO_CTRL_LDO_MODULE_NFC, CONFIG_LDO3V3_CTRL_GPIO, GPIO_OUTPUT_STATE_LOW);
	}else {

		return;
	}

}

static void cli_nfc_write_read_test(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	uint8_t status;
	uint8_t i;
	uint8_t card_Data[16];
	uint8_t write_Data_val = 0;

	if(os_strcmp(argv[1], "write") == 0)
		write_Data_val = os_strtoul(argv[2], NULL, 10);

	MFRC522_LOGI("enter write_Data_val :0x%x !!!!!!!\n\r",__FUNCTION__,write_Data_val);
	if(os_strcmp(argv[1], "write") == 0)
	{
		memset(card_Data, write_Data_val, 16);
		//card_Data[0]=0xaa;
		status = bk_mfrc522_write(5,card_Data);
		if(status != MI_OK)
		{
			MFRC522_LOGE("Card Write Error\n\r");
		}
		MFRC522_LOGI("Card Write ok\n\r");
	}
	else if(os_strcmp(argv[1], "read") == 0)
	{
		memset(card_Data,0,16);
		rtos_delay_milliseconds(8);
		status = bk_mfrc522_read(5,card_Data);
		if(status != MI_OK)
		{
			MFRC522_LOGE("Card Read Error\n\r");
		}
		else
		{
			MFRC522_LOGI("Card Read ok\n\r");
			for(i=0;i<16;i++)
			{
				MFRC522_LOGI("%02X ",card_Data[i]);
			}
			MFRC522_LOGI("\n\r");
		}
   }
}

/**
 @brief Compatibility test: supports both MIFARE Classic and Ultralight cards
*/
static void cli_nfc_compat_test(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	uint8_t Card_Type1[2];
	uint8_t status;
	uid_num_t uid;
	MFRC522_PICC_Type_t type;

	MFRC522_LOGI("=== NFC Compatibility Test Start ===\r\n");
	bk_pm_module_vote_ctrl_external_ldo(GPIO_CTRL_LDO_MODULE_NFC, CONFIG_LDO3V3_CTRL_GPIO, GPIO_OUTPUT_STATE_HIGH);
	// Initialize NFC module
	bk_nfc_init();
	MFRC522_LOGI("init config ok\r\n");
	// Search for card
	if(bk_mfrc522_request(MFRC522_PICC_REQALL, Card_Type1) != MI_OK)
	{
		MFRC522_LOGE("Card request failed!\r\n");
		bk_mfrc522_set_low_power();
		bk_pm_module_vote_ctrl_external_ldo(GPIO_CTRL_LDO_MODULE_NFC, CONFIG_LDO3V3_CTRL_GPIO, GPIO_OUTPUT_STATE_LOW);
		return;
	}

	MFRC522_LOGI("Card detected, reading UID...\r\n");

	// Read card UID
	status = bk_mfrc522_read_card_id(&uid, 0);
	if(status != MI_OK)
	{
		MFRC522_LOGE("Read card ID failed!\r\n");
		bk_mfrc522_set_low_power();
		bk_pm_module_vote_ctrl_external_ldo(GPIO_CTRL_LDO_MODULE_NFC, CONFIG_LDO3V3_CTRL_GPIO, GPIO_OUTPUT_STATE_LOW);
		return;
	}

	// Display UID information
	MFRC522_LOGI("UID Size: %d bytes\r\n", uid.size);
	MFRC522_LOGI("UID: ");
	for(int i = 0; i < uid.size && i < 10; i++)
	{
		MFRC522_LOGI("%02X ", uid.uidByte[i]);
	}
	MFRC522_LOGI("\r\n");
	MFRC522_LOGI("SAK: 0x%02X\r\n", uid.sak);

	// Get card type
	type = bk_mfrc522_get_type(uid.sak);
	MFRC522_LOGI("Card Type: %d\r\n", type);

	// Execute corresponding test based on card type
	if((type == MFRC522_PICC_TYPE_MIFARE_1K) || 
	   (type == MFRC522_PICC_TYPE_MIFARE_4K) || 
	   (type == MFRC522_PICC_TYPE_MIFARE_MINI))
	{
		// MIFARE Classic card test
		MFRC522_LOGI("=== Testing MIFARE Classic Card ===\r\n");
		uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
		// Authorization
		MFRC522_LOGI("Authenticating...\r\n");
		if(bk_mfrc522_authState(MFRC522_PICC_AUTHENT1A, 5, key, uid.uidByte) == MI_OK)
		{
			MFRC522_LOGI("Auth OK\r\n");
			// Read block 5
			uint8_t read_data[16];
			uint8_t write_data[16];
			memset(read_data, 0, 16);
			memset(write_data, 0, 16);

			status = bk_mfrc522_read(5, read_data);
			if(status == MI_OK)
			{
				MFRC522_LOGI("Read Block 5: ");
				for(int i = 0; i < 16; i++)
				{
					MFRC522_LOGI("%02X ", read_data[i]);
				}
				MFRC522_LOGI("\r\n");
			}

			// Write block 5 (optional, comment out to avoid destroying data)
			memset(write_data, 0xAA, 16);
			status = bk_mfrc522_write(5, write_data);
			if(status == MI_OK)
			{
			    MFRC522_LOGI("Write Block 5 OK\r\n");
			}
			else
			{
				MFRC522_LOGE("Write Block 5 Failed\r\n");
			}
			// Read block 5, compare the written data and the read data
			status = bk_mfrc522_read(5, read_data);
			if(status == MI_OK)
			{
				if(memcmp(read_data, write_data, 16) == 0)
				{
					MFRC522_LOGI("Read Block 5 data is same as write data\r\n");
				}
				else
				{
					MFRC522_LOGE("Read Block 5 data is not same as write data\r\n");
				}
			}
		}
		else
		{
			MFRC522_LOGE("Auth Failed\r\n");
		}
		bk_mfrc522_set_low_power();
		mfrc522_deinit();
		bk_mfrc522_clear_bit_mask(MFRC522_REG_STATUS2, 0x08);
		mfrc522_uart_deinit();
	}
	else if(type == MFRC522_PICC_TYPE_MIFARE_UL)
	{
		// MIFARE Ultralight card test
		MFRC522_LOGI("=== Testing MIFARE Ultralight Card ===\r\n");

		uint8_t readBuf[4];
		uint8_t writeData[4];
		memset(readBuf, 0, 4);
		memset(writeData, 0, 4);

		// Read page 5 (4 bytes per page)
		MFRC522_LOGI("Reading Page 5...\r\n");
		status = bk_mfrc522_read(5, readBuf);
		if(status == MI_OK)
		{
			MFRC522_LOGI("Page 5: %02X %02X %02X %02X\r\n", 
			            readBuf[0], readBuf[1], readBuf[2], readBuf[3]);
		}
		else
		{
			MFRC522_LOGE("Read Page 5 Failed\r\n");
		}

		// Write page 5 (optional, comment out to avoid destroying data)
		writeData[0] = 0xAA;
		writeData[1] = 0xBB;
		writeData[2] = 0xCC;
		writeData[3] = 0xDD;
		status = bk_mfrc522_write(5, writeData);
		if(status == MI_OK)
		{
		    MFRC522_LOGI("Write Page 5 OK\r\n");
		    rtos_delay_milliseconds(10);

		    // Read page 5 again to verify
		    status = bk_mfrc522_read(5, readBuf);
		    if(status == MI_OK)
		    {
				if(memcmp(readBuf, writeData, 4) == 0)
				{
					MFRC522_LOGI("Read Page 5 data is same as write data\r\n");
				}
				else
				{
					MFRC522_LOGE("Read Page 5 data is not same as write data\r\n");
				}
		    }
		}

		// Write 16 bytes data
		uint8_t data16[16];
		memset(data16, 0xAA, 16);
		for(int i = 0; i < 4; i++)
		{
			status = bk_mfrc522_write(5 + i, &data16[i * 4]);
			if(status == MI_OK)
			{
				MFRC522_LOGI("Write Page %d OK\r\n", 5 + i);
			}
			else
			{
				MFRC522_LOGE("Write Page %d Failed\r\n", 5 + i);
			}
		}
		// Read 16 bytes data
		for(int i = 0; i < 4; i++)
		{
			status = bk_mfrc522_read(5 + i, readBuf);
			if(status == MI_OK)
			{
				if(memcmp(readBuf, &data16[i * 4], 4) == 0)
				{
					MFRC522_LOGI("Read Page %d data is same as write data\r\n", 5 + i);
				}
				else
				{
					MFRC522_LOGE("Read Page %d data is not same as write data\r\n", 5 + i);
				}
			}
			else
			{
				MFRC522_LOGE("Read Page %d Failed\r\n", 5 + i);
			}
		}
		bk_mfrc522_set_low_power();
		bk_nfc_deinit();
	}
	else
	{
		MFRC522_LOGW("Unsupported card type: %d\r\n", type);
	}

	MFRC522_LOGI("=== NFC Compatibility Test End ===\r\n");

	bk_pm_module_vote_ctrl_external_ldo(GPIO_CTRL_LDO_MODULE_NFC, CONFIG_LDO3V3_CTRL_GPIO, GPIO_OUTPUT_STATE_LOW);
}


#define NFC_CMD_CNT (sizeof(s_nfc_commands) / sizeof(struct cli_command))
static const struct cli_command s_nfc_commands[] = {
	{"nfc_test", "nfc_test", cli_nfc_test},
	{"nfc_cmd_test", "nfc_cmd_test[init/request/anticoll/select/authState/write/read/deinit]", cli_nfc_cmd_test},
	{"nfc_write_test", "nfc_write_test[mode][value]", cli_nfc_write_read_test},
	{"nfc_compat_test", "nfc_compat_test - Compatibility test for MIFARE Classic and Ultralight", cli_nfc_compat_test},
};

int cli_nfc_init(void)
{
	return cli_register_commands(s_nfc_commands, NFC_CMD_CNT);
}

