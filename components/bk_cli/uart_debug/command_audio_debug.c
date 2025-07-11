#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <modules/audio_process.h>
#include "bk_uart.h"
#include "lib_adapter.h"

extern void bk_set_printf_sync(uint8_t enable);
extern int bk_get_printf_sync(void);

//#define AUD_DBG_TOOL_PRT  os_printf
#define AUD_DBG_TOOL_PRT  os_null_printf

#define APP_AUD_PARAS_TX_TMP_LEN   (0x20)

#define APP_SYS_PARA_TX_TOTALLEN  (0xF)
#define APP_SYS_PARA_TX_HEADERLEN (0x4)
#define APP_SYS_PARA_RX_DATALEN   (0x9)


#define APP_AEC_PARA_TX_TOTALLEN  (0x1E)
#define APP_AEC_PARA_TX_HEADERLEN (0x4)
#define APP_AEC_PARA_RX_DATALEN   (0x18)

bk_aud_intf_update_sys_config_cb_t bk_aud_intf_update_sys_config_cb = NULL;
bk_aud_intf_update_aec_config_cb_t bk_aud_intf_update_aec_config_cb = NULL;
bk_aud_intf_update_ul_eq_para_cb_t bk_aud_intf_update_ul_eq_para_cb = NULL;
bk_aud_intf_update_dl_eq_para_cb_t bk_aud_intf_update_dl_eq_para_cb = NULL;

static app_eq_t *eq_dbg_eq_para = NULL;
static app_aud_aec_config_t *aec_dbg_aec_para = NULL;
static app_aud_sys_config_t *sys_dbg_sys_para = NULL;
static app_aud_para_t * p_aud_para = NULL;

void bk_aud_debug_get_audpara(app_aud_para_t * aud_para_ptr)
{
	p_aud_para = aud_para_ptr;
}

void bk_aud_debug_register_update_dl_eq_para_cb(bk_aud_intf_update_dl_eq_para_cb_t dl_eq_para_cb)
{
    bk_aud_intf_update_dl_eq_para_cb = dl_eq_para_cb;
}

void bk_aud_debug_register_update_ul_eq_para_cb(bk_aud_intf_update_ul_eq_para_cb_t ul_eq_para_cb)
{
    bk_aud_intf_update_ul_eq_para_cb = ul_eq_para_cb;
}

void bk_aud_debug_register_update_aec_config_cb(bk_aud_intf_update_aec_config_cb_t aec_config_cb)
{
    bk_aud_intf_update_aec_config_cb = aec_config_cb;
}
void bk_aud_debug_register_update_sys_config_cb(bk_aud_intf_update_sys_config_cb_t sys_config_cb)
{
    bk_aud_intf_update_sys_config_cb = sys_config_cb;
}

void aud_eq_load_param(void)
{
	int32_t log_level = bk_get_printf_sync();
	bk_set_printf_sync(1);
	{
		uint32_t tx_len = 12;
		uint8_t tmp[APP_AUD_PARAS_TX_TMP_LEN] = {0};
		tmp[0] = 0x01;
		tmp[1] = 0xe0;
		tmp[2] = 0xfc;
		tmp[3] = tx_len - 4;
		tmp[4] = 0xb2;
		tmp[5] = 0xfe;
		tmp[6]  = (p_aud_para->eq_dl_voice.eq_load.f_gain)&0xFF;
		tmp[7]  = (p_aud_para->eq_dl_voice.eq_load.f_gain>>8)&0xFF;
		tmp[8]  = (p_aud_para->eq_dl_voice.eq_load.f_gain>>16)&0xFF;
		tmp[9]  = (p_aud_para->eq_dl_voice.eq_load.f_gain>>24)&0xFF;
		tmp[10] = (p_aud_para->eq_dl_voice.eq_load.samplerate)&0xFF;
		tmp[11] = (p_aud_para->eq_dl_voice.eq_load.samplerate>>8)&0xFF;

	#if 1
		for(uint32_t i = 0; i <(tx_len); i++)
		{
			BK_LOG_RAW("%02x", tmp[i]);
		}
		BK_LOG_RAW("\n");
	#endif
	}

	{
		uint32_t tx_len = 0x15;
		uint8_t tmp[APP_AUD_PARAS_TX_TMP_LEN] = {0};
		tmp[0] = 0x01;
		tmp[1] = 0xe0;
		tmp[2] = 0xfc;
		tmp[3] = tx_len - 4;
		tmp[4] = 0xb2;
		tmp[5] = 0xfa;

		for (uint8_t index = 0; index < 15; index++)
		{
			tmp[6] = index;
			tmp[7] = p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].enable;

			tmp[8]  = (p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].freq>>24)&0xFF;
			tmp[9]  = (p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].freq>>16)&0xFF;
			tmp[10] = (p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].freq>>8)&0xFF;
			tmp[11] = (p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].freq>>0)&0xFF;

			tmp[12] = (p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].gain>>24)&0xFF;
			tmp[13] = (p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].gain>>16)&0xFF;
			tmp[14] = (p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].gain>>8)&0xFF;
			tmp[15] = (p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].gain>>0)&0xFF;

			tmp[16] = (p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].q_val>>24)&0xFF;
			tmp[17] = (p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].q_val>>16)&0xFF;
			tmp[18] = (p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].q_val>>8)&0xFF;
			tmp[19] = (p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].q_val>>0)&0xFF;

			tmp[20] = p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].type;

			rtos_delay_milliseconds(10);
		#if 1
			for(uint32_t i = 0; i <(tx_len); i++)
			{
				BK_LOG_RAW("%02x", tmp[i]);
			}
			BK_LOG_RAW("\n");
		#endif
		}
	}
	bk_set_printf_sync(log_level);
}

void app_eq_dbg(uint8_t* params)
{
	uint16_t enable;
	uint16_t total_gain;
	uint8_t eqType;
	uint8_t index;

	AUD_DBG_TOOL_PRT("app_eq_dbg 0x%x\r\n", params[0]);
	switch(params[0])
	{
		case 0xF0:
			aud_eq_load_param();
			break;
		case 0xFA:
	        index  = params[1];
	        enable = params[2];
	        if(eq_dbg_eq_para)
	        {
	            eq_dbg_eq_para->eq_para[index].a[0] = (params[3] | (params[4] << 8) | (params[5] << 16) | (params[6] << 24));
	            eq_dbg_eq_para->eq_para[index].a[1] = (params[7] | (params[8] << 8) | (params[9] << 16) | (params[10] << 24));
	            eq_dbg_eq_para->eq_para[index].b[0] = (params[11] | (params[12] << 8) | (params[13] << 16) | (params[14] << 24));
	            eq_dbg_eq_para->eq_para[index].b[1] = (params[15] | (params[16] << 8) | (params[17] << 16) | (params[18] << 24));
	            eq_dbg_eq_para->eq_para[index].b[2] = (params[19] | (params[20] << 8) | (params[21] << 16) | (params[22] << 24)); 

				eq_dbg_eq_para->eq_load.eq_load_para[index].freq   = (params[23]<<24)|(params[24]<<16)|(params[25]<<8)|(params[26]);
				eq_dbg_eq_para->eq_load.eq_load_para[index].gain   = (params[27]<<24)|(params[28]<<16)|(params[29]<<8)|(params[30]);
				eq_dbg_eq_para->eq_load.eq_load_para[index].q_val  = (params[31]<<24)|(params[32]<<16)|(params[33]<<8)|(params[34]);
				eq_dbg_eq_para->eq_load.eq_load_para[index].type   = params[35];
				eq_dbg_eq_para->eq_load.eq_load_para[index].enable = params[2];

	            if(enable)
	            {
	                eq_dbg_eq_para->filters++;
	            }
	            eq_dbg_eq_para->eq_en = eq_dbg_eq_para->filters > 0 ? 1 : 0;

			#if 1
				p_aud_para->eq_dl_voice.eq_para[index].a[0]   = eq_dbg_eq_para->eq_para[index].a[0];
				p_aud_para->eq_dl_voice.eq_para[index].a[1]   = eq_dbg_eq_para->eq_para[index].a[1];
				p_aud_para->eq_dl_voice.eq_para[index].b[0]   = eq_dbg_eq_para->eq_para[index].b[0];
				p_aud_para->eq_dl_voice.eq_para[index].b[1]   = eq_dbg_eq_para->eq_para[index].b[1];
				p_aud_para->eq_dl_voice.eq_para[index].b[2]   = eq_dbg_eq_para->eq_para[index].b[2]; 

				p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].freq    = eq_dbg_eq_para->eq_load.eq_load_para[index].freq;
				p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].gain    = eq_dbg_eq_para->eq_load.eq_load_para[index].gain;
				p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].q_val   = eq_dbg_eq_para->eq_load.eq_load_para[index].q_val;
				p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].type    = eq_dbg_eq_para->eq_load.eq_load_para[index].type;
				p_aud_para->eq_dl_voice.eq_load.eq_load_para[index].enable  = eq_dbg_eq_para->eq_load.eq_load_para[index].enable;

				p_aud_para->eq_dl_voice.filters = eq_dbg_eq_para->filters;
				p_aud_para->eq_dl_voice.eq_en   = eq_dbg_eq_para->eq_en;
			#endif

				BK_LOG_RAW("rcv eq_%d_params: \n", index);
	        }
	        else
	        {
	            AUD_DBG_TOOL_PRT("eq_dbg_eq_para is NULL\r\n");
	        }
			break;
		case 0xFB:

			break;
		case 0xFC:

			break;
		case 0xFD:
			if(params[1] == 0x01)
	        {
	            if(eq_dbg_eq_para == NULL)
	            {
	              eq_dbg_eq_para = os_malloc(sizeof(app_eq_t));
	            }
	            if(eq_dbg_eq_para)
	            {
	                eq_dbg_eq_para->filters = 0;
	                AUD_DBG_TOOL_PRT("eq_dbg start\r\n");
	            }
	        }
	        else if(params[1] == 0x00)
	        {

	            AUD_DBG_TOOL_PRT("eq_dbg enable=%d filters=%d\r\n",eq_dbg_eq_para->eq_en,eq_dbg_eq_para->filters);
            #if 0
	            for(i = 0; i < eq_dbg_eq_para->filters; i++)
	            {
	                AUD_DBG_TOOL_PRT("eq_dbg a[0]=%d,a[1]=%d,b[0]=%d,b[1]=%d,b[2]=%d\r\n",eq_dbg_eq_para->eq_para[i].a[0],\
	                        eq_dbg_eq_para->eq_para[i].a[1],eq_dbg_eq_para->eq_para[i].b[0],eq_dbg_eq_para->eq_para[i].b[1],\
	                        eq_dbg_eq_para->eq_para[i].b[2]);

	            }
            #endif
	            if((bk_aud_intf_update_dl_eq_para_cb)&&(eq_dbg_eq_para))
	            {
	                bk_aud_intf_update_dl_eq_para_cb(eq_dbg_eq_para);

	            }
	            if(eq_dbg_eq_para)
	            {
	                os_free(eq_dbg_eq_para);
	                eq_dbg_eq_para = NULL;
	                AUD_DBG_TOOL_PRT("eq_dbg end\r\n");
	            }

	        }
			break;
		case 0xFE:
	        enable = (params[2] << 8 | params[1]);
	        total_gain = (params[4] << 8 | params[3]);
	        eqType = params[5]; /// unused params now
	        if(eq_dbg_eq_para)
	        {
				eq_dbg_eq_para->globle_gain = (uint32_t)(1.12f * total_gain);
				eq_dbg_eq_para->eq_load.f_gain      = (params[9]<<24)|(params[8]<<16)|(params[7]<<8)|(params[6]);
				eq_dbg_eq_para->eq_load.samplerate  = (params[11]<<8)|(params[10]);
				AUD_DBG_TOOL_PRT("eq_dbg enable:%d, total_gain:%d, eqType:%d\r\n", enable, total_gain, eqType);

			#if 1
				p_aud_para->eq_dl_voice.globle_gain = eq_dbg_eq_para->globle_gain;
				p_aud_para->eq_dl_voice.eq_load.f_gain      = eq_dbg_eq_para->eq_load.f_gain;
				p_aud_para->eq_dl_voice.eq_load.samplerate  = eq_dbg_eq_para->eq_load.samplerate;
			#endif
			}
			break;
		default:
			break;
	}
}

void app_sys_config_dbg(uint8_t* params)
{
	AUD_DBG_TOOL_PRT("app_sys_config_dbg 0x%x\r\n", params[0]);
	switch(params[0])
	{
		case 0xF0:
		{
			uint32_t tx_len = APP_SYS_PARA_TX_TOTALLEN;
			uint8_t tmp[APP_AUD_PARAS_TX_TMP_LEN] = {0};
			tmp[0] = 0x01; tmp[1] = 0xe0; tmp[2] = 0xfc;
			tmp[3] = APP_SYS_PARA_TX_TOTALLEN - APP_SYS_PARA_TX_HEADERLEN;
			tmp[4] = 0xb3; tmp[5] = 0xf9;
			tmp[6] = p_aud_para->sys_config_voice.mic0_digital_gain;
			tmp[7] = p_aud_para->sys_config_voice.mic0_analog_gain;
			tmp[8] = p_aud_para->sys_config_voice.mic1_analog_gain;
			tmp[9] = p_aud_para->sys_config_voice.speaker_chan0_digital_gain;
			tmp[10] = p_aud_para->sys_config_voice.speaker_chan0_analog_gain;
			tmp[11] = p_aud_para->sys_config_voice.main_mic_select;
			tmp[12] = p_aud_para->sys_config_voice.mic_mode;
			tmp[13] = p_aud_para->sys_config_voice.spk_mode;
			tmp[14] = p_aud_para->sys_config_voice.mic_vbias;

		#if 1
			for(uint32_t i = 0; i <(tx_len); i++)
			{
				BK_LOG_RAW("%02x", tmp[i]);
			}
			BK_LOG_RAW("\n");
		#endif
		}
			break;
		case 0xF9:
		{
			if(sys_dbg_sys_para == NULL)
				sys_dbg_sys_para = os_malloc(sizeof(app_aud_sys_config_t));

			if(sys_dbg_sys_para)
			{
				sys_dbg_sys_para->mic0_digital_gain          = params[1];
				sys_dbg_sys_para->mic0_analog_gain           = params[2];
				sys_dbg_sys_para->mic1_analog_gain           = params[3];
				sys_dbg_sys_para->speaker_chan0_digital_gain = params[4];
				sys_dbg_sys_para->speaker_chan0_analog_gain  = params[5];
				sys_dbg_sys_para->main_mic_select            = params[6];
				sys_dbg_sys_para->mic_mode                   = params[7];
				sys_dbg_sys_para->spk_mode                   = params[8];
				sys_dbg_sys_para->mic_vbias                  = params[9];

				p_aud_para->sys_config_voice.mic0_digital_gain           = sys_dbg_sys_para->mic0_digital_gain;
				p_aud_para->sys_config_voice.mic0_analog_gain            = sys_dbg_sys_para->mic0_analog_gain;
				p_aud_para->sys_config_voice.mic1_analog_gain            = sys_dbg_sys_para->mic1_analog_gain;
				p_aud_para->sys_config_voice.speaker_chan0_digital_gain  = sys_dbg_sys_para->speaker_chan0_digital_gain;
				p_aud_para->sys_config_voice.speaker_chan0_analog_gain   = sys_dbg_sys_para->speaker_chan0_analog_gain;
				p_aud_para->sys_config_voice.main_mic_select             = sys_dbg_sys_para->main_mic_select;
				p_aud_para->sys_config_voice.mic_mode                    = sys_dbg_sys_para->mic_mode;
				p_aud_para->sys_config_voice.spk_mode                    = sys_dbg_sys_para->spk_mode;
				p_aud_para->sys_config_voice.mic_vbias                   = sys_dbg_sys_para->mic_vbias;

			#if 1
				BK_LOG_RAW("rcv sys_params: ");
				for(uint32_t i = 1; i <(APP_SYS_PARA_RX_DATALEN+1); i++)
				{
					BK_LOG_RAW("%02x ", params[i]);
				}
				BK_LOG_RAW("\n");
			#endif
				if(bk_aud_intf_update_sys_config_cb)
				{
					bk_aud_intf_update_sys_config_cb(sys_dbg_sys_para);
				}
				if(sys_dbg_sys_para)
				{
					os_free(sys_dbg_sys_para);
					sys_dbg_sys_para = NULL;
					AUD_DBG_TOOL_PRT("sys_dbg_sys_para end\r\n");
				}
			}
			else
			{
				AUD_DBG_TOOL_PRT("sys_dbg_sys_para is NULL\r\n");
			}
		}
			break;
		default:
			break;
	}
}



void app_aec_para_dbg(uint8_t* params)
{
	AUD_DBG_TOOL_PRT("app_aec_para_dbg 0x%x\r\n", params[0]);
	switch(params[0])
	{
		case 0xF0:
		{
			uint32_t tx_len = APP_AEC_PARA_TX_TOTALLEN;
			uint8_t tmp[APP_AUD_PARAS_TX_TMP_LEN] = {0};
			tmp[0] = 0x01; tmp[1] = 0xe0; tmp[2] = 0xfc;
			tmp[3] = APP_AEC_PARA_TX_TOTALLEN - APP_AEC_PARA_TX_HEADERLEN;
			tmp[4] = 0xb4; tmp[5] = 0xff;
			tmp[6]  = p_aud_para->aec_config_voice.aec_enable;
			tmp[7]  = p_aud_para->aec_config_voice.ec_filter;
			tmp[8]  = (p_aud_para->aec_config_voice.init_flags>>8)&0xFF;
			tmp[9]  = (p_aud_para->aec_config_voice.init_flags)&0xFF;
			tmp[10] = p_aud_para->aec_config_voice.ns_filter;
			tmp[11] = p_aud_para->aec_config_voice.ref_scale;
			tmp[12] = p_aud_para->aec_config_voice.drc_gain;
			tmp[13] = p_aud_para->aec_config_voice.voice_vol;
			tmp[14] = p_aud_para->aec_config_voice.ec_depth;
			tmp[15] = p_aud_para->aec_config_voice.mic_delay;
			tmp[16] = p_aud_para->aec_config_voice.ns_level;
			tmp[17] = p_aud_para->aec_config_voice.ns_para;
			tmp[18] = p_aud_para->aec_config_voice.ns_type;
			tmp[19] = p_aud_para->aec_config_voice.vad_enable;
			tmp[20] = (p_aud_para->aec_config_voice.vad_start_threshold>>8)&0xFF;
			tmp[21] = (p_aud_para->aec_config_voice.vad_start_threshold)&0xFF;
			tmp[22] = (p_aud_para->aec_config_voice.vad_stop_threshold>>8)&0xFF;
			tmp[23] = (p_aud_para->aec_config_voice.vad_stop_threshold)&0xFF;
			tmp[24] = (p_aud_para->aec_config_voice.vad_silence_threshold>>8)&0xFF;
			tmp[25] = (p_aud_para->aec_config_voice.vad_silence_threshold)&0xFF;
			tmp[26] = (p_aud_para->aec_config_voice.vad_eng_threshold>>8)&0xFF;
			tmp[27] = (p_aud_para->aec_config_voice.vad_eng_threshold)&0xFF;
			tmp[28] = p_aud_para->aec_config_voice.dual_mic_enable;
			tmp[29] = (p_aud_para->aec_config_voice.dual_mic_distance);
		#if 1
			for(uint32_t i = 0; i <(tx_len); i++)
			{
				BK_LOG_RAW("%02x", tmp[i]);
			}
			BK_LOG_RAW("\n");
		#endif
		}
			break;
		case 0xFF:
		{
			if(aec_dbg_aec_para == NULL)
				aec_dbg_aec_para = os_malloc(sizeof(app_aud_aec_config_t));

			if(aec_dbg_aec_para)
			{
				aec_dbg_aec_para->aec_enable = params[1];
				aec_dbg_aec_para->ec_filter  = params[2];
				aec_dbg_aec_para->init_flags = (params[3]<<8)|(params[4]);
				aec_dbg_aec_para->ns_filter  = params[5];
				aec_dbg_aec_para->ref_scale  = params[6];
				aec_dbg_aec_para->drc_gain   = params[7];
				aec_dbg_aec_para->voice_vol  = params[8];
				aec_dbg_aec_para->ec_depth   = params[9];
				aec_dbg_aec_para->mic_delay  = params[10];
				aec_dbg_aec_para->ns_level   = params[11];
				aec_dbg_aec_para->ns_para    = params[12];
				aec_dbg_aec_para->ns_type    = params[13];
				aec_dbg_aec_para->vad_enable = params[14];
				aec_dbg_aec_para->vad_start_threshold   = (params[15]<<8)|(params[16]);
				aec_dbg_aec_para->vad_stop_threshold    = (params[17]<<8)|(params[18]);
				aec_dbg_aec_para->vad_silence_threshold = (params[19]<<8)|(params[20]);
				aec_dbg_aec_para->vad_eng_threshold     = (params[21]<<8)|(params[22]);
				aec_dbg_aec_para->dual_mic_enable   = params[23];
				aec_dbg_aec_para->dual_mic_distance = params[24];

				p_aud_para->aec_config_voice.aec_enable            = aec_dbg_aec_para->aec_enable;
				p_aud_para->aec_config_voice.ec_filter             = aec_dbg_aec_para->ec_filter;
				p_aud_para->aec_config_voice.init_flags            = aec_dbg_aec_para->init_flags;
				p_aud_para->aec_config_voice.ns_filter             = aec_dbg_aec_para->ns_filter;
				p_aud_para->aec_config_voice.ref_scale             = aec_dbg_aec_para->ref_scale;
				p_aud_para->aec_config_voice.drc_gain              = aec_dbg_aec_para->drc_gain;
				p_aud_para->aec_config_voice.voice_vol             = aec_dbg_aec_para->voice_vol;
				p_aud_para->aec_config_voice.ec_depth              = aec_dbg_aec_para->ec_depth;
				p_aud_para->aec_config_voice.mic_delay             = aec_dbg_aec_para->mic_delay;
				p_aud_para->aec_config_voice.ns_level              = aec_dbg_aec_para->ns_level;
				p_aud_para->aec_config_voice.ns_para               = aec_dbg_aec_para->ns_para;
				p_aud_para->aec_config_voice.ns_type               = aec_dbg_aec_para->ns_type;
				p_aud_para->aec_config_voice.vad_enable            = aec_dbg_aec_para->vad_enable;
				p_aud_para->aec_config_voice.vad_start_threshold   = aec_dbg_aec_para->vad_start_threshold;
				p_aud_para->aec_config_voice.vad_stop_threshold    = aec_dbg_aec_para->vad_stop_threshold;
				p_aud_para->aec_config_voice.vad_silence_threshold = aec_dbg_aec_para->vad_silence_threshold;
				p_aud_para->aec_config_voice.vad_eng_threshold     = aec_dbg_aec_para->vad_eng_threshold;
				p_aud_para->aec_config_voice.dual_mic_enable       = aec_dbg_aec_para->dual_mic_enable;
				p_aud_para->aec_config_voice.dual_mic_distance     = aec_dbg_aec_para->dual_mic_distance;

			#if 1
				BK_LOG_RAW("rcv aec_params: ");
				for(uint32_t i = 1; i < (APP_AEC_PARA_RX_DATALEN+1); i++)
				{
					BK_LOG_RAW("%02x ", params[i]);
				}
				BK_LOG_RAW("\n");
			#endif

				if(bk_aud_intf_update_aec_config_cb)
				{
					bk_aud_intf_update_aec_config_cb(aec_dbg_aec_para);
				}
				if(aec_dbg_aec_para)
				{
					os_free(aec_dbg_aec_para);
					aec_dbg_aec_para = NULL;
					AUD_DBG_TOOL_PRT("aec_dbg_aec_para end\r\n");
				}
			}
			else
			{
				AUD_DBG_TOOL_PRT("aec_dbg_aec_para is NULL\r\n");
			}
		}
			break;
		default:
			break;
	}
}

