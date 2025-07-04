#pragma once

#ifdef __cplusplus
extern "C" {
#endif



#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>


#define EQ_ID_DL_VOICE     0
#define EQ_ID_UL_VOICE     1
#define EQ_ID_AUDIO        2


typedef struct _app_eq_para_t
{
    int32_t a[2];
    int32_t b[3];
}app_eq_para_t;

typedef struct _app_eq_load_para_t
{
	uint32_t freq;
	uint32_t gain;
	uint32_t q_val;
	uint8_t  type;
	uint8_t  enable;
}app_eq_load_para_t;

typedef struct _app_eq_load_t
{
	uint32_t f_gain;
	uint32_t samplerate;
	app_eq_load_para_t eq_load_para[16];
}app_eq_load_t;

typedef struct _app_eq_t
{
	uint8_t eq_en;
	uint32_t framecnt;
	uint32_t filters;
	int32_t globle_gain;
	app_eq_para_t eq_para[16];
	app_eq_load_t eq_load;
}app_eq_t;


typedef struct _app_aud_sys_config_t
{
    uint8_t mic0_digital_gain;
    uint8_t mic0_analog_gain;
    uint8_t mic1_digital_gain;
    uint8_t mic1_analog_gain;

    uint8_t speaker_chan0_digital_gain;
    uint8_t speaker_chan0_analog_gain;
    uint8_t speaker_chan1_digital_gain;
    uint8_t speaker_chan1_analog_gain;

    uint8_t dmic_enable; // digital mic
    uint8_t main_mic_select;
    uint8_t adc_sample_rate;
    uint8_t dac_sample_rate;

    uint8_t mic_mode;  //signed_end/diffen
    uint8_t spk_mode;  //signed_end/diffen
    uint8_t mic_vbias; //0b00=2.4v, 0b11=1.8v

    uint8_t extend[5];
}app_aud_sys_config_t;

typedef enum
{
	NS_CLOSE = 0,
	NS_AI    = 1,
	NS_TRADITION = 2,
	NS_MODE_MAX,
}AUD_NS_TYPE;

typedef struct _app_aud_aec_config_t
{
    uint8_t aec_enable;
    uint8_t ec_filter;
    uint16_t init_flags;

    uint8_t ns_filter;
    int8_t  ref_scale;
    uint8_t drc_gain;
    uint8_t voice_vol;

    uint32_t ec_depth;
    uint32_t mic_delay;

    uint8_t ns_level;
    uint8_t ns_para;
    uint8_t ns_type;
    uint8_t vad_enable;

    int16_t vad_start_threshold;
    int16_t vad_stop_threshold;
    int16_t vad_silence_threshold;
    int16_t vad_eng_threshold;

    uint8_t dual_mic_enable;
    uint8_t dual_mic_distance;
    uint8_t rsvd[2];

}app_aud_aec_config_t;


typedef struct _app_aud_para_t
{
    app_aud_sys_config_t sys_config_voice;
    app_eq_t eq_dl_voice;
    app_eq_t eq_ul_voice;
    app_aud_aec_config_t aec_config_voice;
    
}app_aud_para_t;


typedef bk_err_t (*bk_aud_intf_update_sys_config_cb_t)(app_aud_sys_config_t *sys_config_ptr);
typedef bk_err_t (*bk_aud_intf_update_aec_config_cb_t)(app_aud_aec_config_t *aec_config_ptr);
typedef bk_err_t (*bk_aud_intf_update_ul_eq_para_cb_t)(app_eq_t *ul_eq_para_ptr);
typedef bk_err_t (*bk_aud_intf_update_dl_eq_para_cb_t)(app_eq_t *dl_eq_para_ptr);
#define VAD_DELAY_BUF_NUM 32

typedef struct {
    int16_t vad_delay_buf[VAD_DELAY_BUF_NUM*320];
    uint32_t read_ptr;
    uint32_t write_ptr;
    int16_t vad_flag[VAD_DELAY_BUF_NUM];
    uint32_t vad_read_ptr;
    uint32_t vad_write_ptr;
} vad_delay_ring_buf_t;
extern app_aud_para_t aud_para;

void app_aud_eq_init(app_eq_t  *cust_eq_coe_ptr, uint32_t eq_id);
void app_aud_eq_process(int16_t *buff, uint16_t size, uint32 eq_id);
bk_err_t audio_para_init(app_aud_para_t *aud_para_ptr);
void voice_dl_process(int16 *buf, uint32 sample_points);
void voice_process_init();
void voice_ul_post_process(int16 *buf, uint32 sample_points,int16 vad_flag,int16 *debug_buf);
void voice_ul_pre_process(int16 *buf, uint32 sample_points);
app_aud_para_t * get_app_aud_cust_para(void);
void aud_tras_update_tx_size(int tx_size);
void vad_delay_ring_buf_init(uint32_t samp_rate_points,uint32_t delay_num);
#ifdef __cplusplus
}
#endif