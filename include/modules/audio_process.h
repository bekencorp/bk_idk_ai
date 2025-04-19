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
    int32_t    a[2];
    int32_t    b[3];
}app_eq_para_t;

typedef struct _app_eq_t
{
    uint32_t framecnt;
    uint32_t filters;
    int32_t globle_gain;
    app_eq_para_t eq_para[16];
}app_eq_t;


typedef struct _app_aud_para_t
{
    app_eq_t eq_dl_voice;
    app_eq_t eq_ul_voice;
}app_aud_para_t;




void app_aud_eq_init(app_eq_t  *cust_eq_coe_ptr, uint32_t eq_id);
void app_aud_eq_process(int16_t *buff, uint16_t size, uint32 eq_id);
bk_err_t audio_para_init(app_aud_para_t *aud_para_ptr);
void voice_dl_process(int16 *buf, uint32 sample_points);
void voice_dl_process_init(void);
#ifdef __cplusplus
}
#endif