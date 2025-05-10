#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <modules/audio_process.h>


//#define AUD_DBG_TOOL_PRT  os_printf
#define AUD_DBG_TOOL_PRT  os_null_printf


bk_aud_intf_update_sys_config_cb_t bk_aud_intf_update_sys_config_cb = NULL;
bk_aud_intf_update_aec_config_cb_t bk_aud_intf_update_aec_config_cb = NULL;
bk_aud_intf_update_ul_eq_para_cb_t bk_aud_intf_update_ul_eq_para_cb = NULL;
bk_aud_intf_update_dl_eq_para_cb_t bk_aud_intf_update_dl_eq_para_cb = NULL;


 static  app_eq_t *eq_dbg_eq_para = NULL;

void bk_aud_debug_register_update_dl_eq_para_cb(bk_aud_intf_update_dl_eq_para_cb_t dl_eq_para_cb)
{
    bk_aud_intf_update_dl_eq_para_cb = dl_eq_para_cb;
    AUD_DBG_TOOL_PRT("bk_aud_debug_register_update_dl_eq_para_cb %p\r\n",dl_eq_para_cb);
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

void app_eq_dbg(uint8_t* params)
{
    uint16_t enable;
    uint16_t total_gain;
    uint8_t eqType;
    uint8_t index;


    AUD_DBG_TOOL_PRT("app_eq_dbg 0x%x\r\n", params[0]);
	switch(params[0])
	{
	case 0xFA:
        index = params[1];
        enable =params[2];  
        if(eq_dbg_eq_para)
        {


            eq_dbg_eq_para->eq_para[index].a[0] = (params[3] | (params[4] << 8) | (params[5] << 16) | (params[6] << 24));
            eq_dbg_eq_para->eq_para[index].a[1] = (params[7] | (params[8] << 8) | (params[9] << 16) | (params[10] << 24));
            eq_dbg_eq_para->eq_para[index].b[0] = (params[11] | (params[12] << 8) | (params[13] << 16) | (params[14] << 24));
            eq_dbg_eq_para->eq_para[index].b[1] = (params[15] | (params[16] << 8) | (params[17] << 16) | (params[18] << 24));
            eq_dbg_eq_para->eq_para[index].b[2] = (params[19] | (params[20] << 8) | (params[21] << 16) | (params[22] << 24)); 
            AUD_DBG_TOOL_PRT("eq_dbg index=%d, enable=%d,a[0]=%d,a[1]=%d,b[0]=%d,b[1]=%d,b[2]=%d\r\n",index,enable,\
                        eq_dbg_eq_para->eq_para[index].a[0],eq_dbg_eq_para->eq_para[index].a[1],\
                        eq_dbg_eq_para->eq_para[index].b[0],eq_dbg_eq_para->eq_para[index].b[1],eq_dbg_eq_para->eq_para[index].b[2]);
            if(enable)
            {
                eq_dbg_eq_para->filters++;
            }
            eq_dbg_eq_para->eq_en = eq_dbg_eq_para->filters > 0 ? 1 : 0;
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
        eqType = params[5];
        if(eq_dbg_eq_para)
        {
            eq_dbg_eq_para->globle_gain = (uint32_t)(1.12f * total_gain);  
            AUD_DBG_TOOL_PRT("eq_dbg enable:%d, total_gain:%d, eqType:%d\r\n", enable, total_gain, eqType);
        }

		break;
	default: break;
	}
}


void app_aec_para_dbg(uint8_t* params)
{
    
}


void app_sys_config_dbg(uint8_t* params)
{

}