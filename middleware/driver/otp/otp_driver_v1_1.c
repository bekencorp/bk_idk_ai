// Copyright 2020-2021 Beken
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
#include <common/bk_include.h>
#include <driver/otp_types.h>
#include <driver/vault.h>
#include <os/mem.h>
#include "otp_driver.h"
#include "otp_hal.h"
#include "_otp.c"
#if CONFIG_TFM_OTP_NSC
#include "tfm_otp_nsc.h"
#endif
typedef struct {
	otp_hal_t hal;
} otp_driver_t;

/*the smallest unit length for configuration*/

/*              permission       secure_range             mask         */
/*    otp1        4 Byte            256 Byte             32 Byte       */
/*    otp2        128 Byte           NA                  128 Byte      */
/*    puf         4   Byte           32 Byte             32 Byte       */

static otp_driver_t s_otp = {0};
static bool s_otp_driver_is_init = false;

static void bk_otp_init(otp_hal_t *hal)
{
	if(s_otp_driver_is_init) {
		return ;
	}
	hal->hw = (otp_hw_t *)OTP_LL_REG_BASE();
	hal->hw2 = (otp2_hw_t *)OTP2_LL_REG_BASE();
	otp_hal_init(&s_otp.hal);

	s_otp_driver_is_init = true;

	return ;
}

static void otp_sleep()
{
#if CONFIG_ATE_TEST
	return ;
#endif
#if CONFIG_AON_ENCP
	otp_hal_sleep(&s_otp.hal);
#else
	otp_hal_deinit(&s_otp.hal);
#endif
}

static int otp_active()
{
#if CONFIG_ATE_TEST
	return 0;
#endif
	bk_otp_init(&s_otp.hal);
#if CONFIG_AON_ENCP
	return otp_hal_active(&s_otp.hal);
#else 
	return otp_hal_init(&s_otp.hal);
#endif
}
static int switch_map(uint8_t map_id)
{
	if(otp_map == NULL){
		if (map_id == 1) {
			otp_map = otp_map_1;
		} else if (map_id == 2) {
			otp_map = otp_map_2;
		} else {
			return -1;
		}
	} else {
		return -1;
	}
	return 0;
}

#define OTP_ACTIVE(map_id) \
	do { \
		int ret = switch_map(map_id); \
		if(ret != 0) { \
			return ret;\
		} \
		ret = otp_active(); \
		if(ret != 0) { \
			otp_sleep(); \
			otp_map = NULL; \
			return -1;\
		} \
	} while(0);

#define OTP_SLEEP() \
	do { \
		otp_sleep(); \
		otp_map = NULL; \
	} while(0);

static uint32_t otp_read_otp(uint32_t location)
{
	return otp_hal_read_otp(&s_otp.hal,location);
}
static uint32_t otp2_read_otp(uint32_t location)
{
	return otp2_hal_read_otp(&s_otp.hal,location);
}

static void otp2_write_otp(uint32_t location,uint32_t value)
{
	otp2_hal_write_otp(&s_otp.hal, location, value);
}

static void otp_write_otp(uint32_t location,uint32_t value)
{
	otp_hal_write_otp(&s_otp.hal,location,value);
}

static uint32_t otp_read_status()
{
	return otp_hal_read_status(&s_otp.hal);
}

static uint32_t otp_get_failcnt()
{
	return otp_hal_get_failcnt(&s_otp.hal);
}

uint32_t otp_read_otp2_permission(uint32_t location)
{
	return otp_hal_read_otp2_permission(&s_otp.hal,location);
}

static void otp_set_lck_mask(uint32_t location)
{
	otp_hal_enable_mask_lck(&s_otp.hal);
}
static uint32_t otp_read_otp_mask(uint32_t location)
{
	return otp_hal_read_otp_mask(&s_otp.hal,location);
}
static void otp_zeroize_otp(uint32_t location)
{
	otp_hal_zeroize_otp(&s_otp.hal,location);
}
static void otp_zeroize_puf(uint32_t location)
{
	otp_hal_zeroize_puf(&s_otp.hal,location);
}
static uint32_t otp_get_otp_zeroized_flag(uint32_t location)
{
	return otp_hal_read_otp_zeroized_flag(&s_otp.hal,location);
}

otp_privilege_t bk_otp_apb_read_permission(otp1_id_t item)
{
	OTP_ACTIVE(1)
	uint32_t location = otp_map[item].offset / 4;
	otp_privilege_t permission = otp_hal_read_otp_permission(&s_otp.hal,location);
	otp_privilege_t mask = otp_hal_read_otp_mask(&s_otp.hal,location);
	otp_privilege_t ret_permission = max(permission, mask);

	OTP_SLEEP()
	return ret_permission;
}

otp_privilege_t bk_otp_ahb_read_permission(otp2_id_t item)
{
	OTP_ACTIVE(2)
	uint32_t location = otp_map[item].offset / 4;
	otp_privilege_t permission = otp_hal_read_otp2_permission(&s_otp.hal,location);
	otp_privilege_t mask = otp_hal_read_otp2_mask(&s_otp.hal,location);
	otp_privilege_t ret_permission = max(permission, mask);

	OTP_SLEEP()
	return ret_permission;
}


bk_err_t bk_otp_ahb_write_permission(otp2_id_t item, otp_privilege_t permission)
{
	if (item >= OTP2_MAX_ID) {
		return BK_ERR_OTP_INDEX_WRONG;
	}
	if(permission != OTP_READ_ONLY) {
		return BK_ERR_OTP_PERMISSION_WRONG;
	}
	OTP_ACTIVE(2)
	uint32_t location = otp_map[item].offset / 4;
	int32_t size = otp_map[item].allocated_size;
	while(size > 0) {
		otp_hal_write_otp2_permission(&s_otp.hal, location, permission);
		location++;
		size-=4;
	}
	OTP_SLEEP()
	return BK_OK;
}

bk_err_t bk_otp_apb_write_permission(otp1_id_t item, otp_privilege_t permission)
{
	if (item >= OTP1_MAX_ID) {
		return BK_ERR_OTP_INDEX_WRONG;
	}
	if(permission != OTP_READ_ONLY && permission != OTP_NO_ACCESS){
		return BK_ERR_OTP_PERMISSION_WRONG;
	}
	OTP_ACTIVE(1)
	uint32_t location = otp_map[item].offset / 4;
	int32_t size = otp_map[item].allocated_size;
	while(size > 0) {
		otp_hal_write_otp_permission(&s_otp.hal, location, permission);
		location++;
		size-=4;
	}
	OTP_SLEEP()
	return BK_OK;
}

otp_privilege_t bk_otp_apb_read_mask(otp1_id_t item)
{
	OTP_ACTIVE(1)
	uint32_t location = otp_map[item].offset / 4;
	otp_privilege_t mask = otp_hal_read_otp_mask(&s_otp.hal,location);
	OTP_SLEEP()
	return mask;
}

bk_err_t bk_otp_apb_write_mask(otp1_id_t item, uint32_t mask)
{
	if (item >= OTP1_MAX_ID) {
		return BK_ERR_OTP_INDEX_WRONG;
	}
	OTP_ACTIVE(1)
	uint32_t location = otp_map[item].offset / 4;
	int32_t size = otp_map[item].allocated_size;
	while(size > 0) {
		otp_hal_write_otp_mask(&s_otp.hal, location, mask);
		location++;
		size-=4;
	}
	OTP_SLEEP()
	return BK_OK;
}

otp_privilege_t bk_otp_ahb_read_mask(otp2_id_t item)
{
	OTP_ACTIVE(2)
	uint32_t location = otp_map[item].offset / 4;
	otp_privilege_t mask = otp_hal_read_otp2_mask(&s_otp.hal,location);
	OTP_SLEEP()
	return mask;
}

bk_err_t bk_otp_ahb_write_mask(otp2_id_t item, uint32_t mask)
{
	if (item >= OTP2_MAX_ID) {
		return BK_ERR_OTP_INDEX_WRONG;
	}
	OTP_ACTIVE(2)
	uint32_t location = otp_map[item].offset / 4;
	int32_t size = otp_map[item].allocated_size;
	while(size > 0) {
		otp_hal_write_otp2_mask(&s_otp.hal, location, mask);
		location++;
		size-=4;
	}
	OTP_SLEEP()
	return BK_OK;
}

/**
 * obtain APB OTP value in little endian with item ID:
 * 1. allowed start address of item not aligned
 * 2. allowed end address of item not aligned
 */
bk_err_t bk_otp_apb_read(otp1_id_t item, uint8_t* buf, uint32_t size)
{
	if (item >= OTP1_MAX_ID) {
		return BK_ERR_OTP_INDEX_WRONG;
	}

	if( bk_otp_apb_read_permission(item) > OTP_READ_ONLY ) {
		return BK_ERR_NO_READ_PERMISSION;
	}
	OTP_ACTIVE(1)
	if(size > otp_map[item].allocated_size){
		OTP_SLEEP()
		return BK_ERR_OTP_ADDR_OUT_OF_RANGE;
	}
	uint32_t location = otp_map[item].offset / 4;
	uint32_t start = otp_map[item].offset % 4;
	uint32_t value;

	while(size > 0) {
		value = otp_read_otp(location);

		uint8_t * src_data = (uint8_t *)&value;
		int       cpy_cnt;

		src_data += start;

		cpy_cnt = (size >= (4 - start)) ? (4 - start) : size;

		switch( cpy_cnt ) {
			case 4:
				*buf++ = *src_data++;
			case 3:
				*buf++ = *src_data++;
			case 2:
				*buf++ = *src_data++;
			case 1:
				*buf++ = *src_data++;
		}

		size -= cpy_cnt;
		location++;
		start = 0;
	}
	OTP_SLEEP()
	return BK_OK;
}


bk_err_t bk_otp_apb_read_bak(uint32_t item, uint8_t* buf, uint32_t size)
{
	if( bk_otp_apb_read_permission(item) > OTP_READ_ONLY ) {
		return BK_ERR_NO_READ_PERMISSION;
	}
	OTP_ACTIVE(1)

	uint32_t location = 0x3c0 / 4;
	uint32_t start = 0x3c0 % 4;
	uint32_t value;


	while(size > 0) {
		value = otp_read_otp(location);

		uint8_t * src_data = (uint8_t *)&value;
		int       cpy_cnt;

		src_data += start;

		cpy_cnt = (size >= (4 - start)) ? (4 - start) : size;

		switch( cpy_cnt ) {
			case 4:
				*buf++ = *src_data++;
			case 3:
				*buf++ = *src_data++;
			case 2:
				*buf++ = *src_data++;
			case 1:
				*buf++ = *src_data++;
		}

		size -= cpy_cnt;
		location++;
		start = 0;
	}
	OTP_SLEEP()
	return BK_OK;
}





/**
 * update APB OTP value in little endian with item ID:
 * 1. allowed start address of item not aligned
 * 2. allowed end address of item not aligned
 * 3. check overwritable before write, return BK_ERR_OTP_CANNOT_WRTIE if failed
 * 4. verify value after write, return BK_ERR_OTP_UPDATE_NOT_EQUAL if failed
 */
bk_err_t bk_otp_apb_update(otp1_id_t item, uint8_t* buf, uint32_t size)
{
	if (item >= OTP1_MAX_ID) {
		return BK_ERR_OTP_INDEX_WRONG;
	}
	if( bk_otp_apb_read_permission(item) != OTP_READ_WRITE ) {
		return BK_ERR_NO_WRITE_PERMISSION;
	}
	OTP_ACTIVE(1)
	if(size > otp_map[item].allocated_size){
		OTP_SLEEP()
		return BK_ERR_OTP_ADDR_OUT_OF_RANGE;
	}
	uint32_t location = otp_map[item].offset / 4;
	uint32_t start = otp_map[item].offset % 4;
	uint32_t value = 0;
	uint32_t check_value = 0;

	uint8_t * dst_data;
	uint8_t * back_buf = buf;
	uint32_t  back_size = size;

	while(size > 0) {
		value = otp_read_otp(location);

		int       cmp_cnt = (size >= (4 - start)) ? (4 - start) : size;
		uint32_t  cmp_result = 0;

		dst_data = (uint8_t *)&value;
		dst_data += start;

		/* the initial data of every OTP memory bit is 0. */
		/* OTP memory bit can be changed to 1, */
		/* once it is changed to 1, it can't be reset to 0. */
		/* so check the target bit value before write to OTP memory. */
		/* if try to change OTP bit from 1 to 0, return fail. */
		switch( cmp_cnt ) {
			case 4:
				cmp_result |= (~(*buf) & (*dst_data));
				buf++; dst_data++;
			case 3:
				cmp_result |= (~(*buf) & (*dst_data));
				buf++; dst_data++;
			case 2:
				cmp_result |= (~(*buf) & (*dst_data));
				buf++; dst_data++;
			case 1:
				cmp_result |= (~(*buf) & (*dst_data));
				buf++; dst_data++;
		}

		if(cmp_result != 0)
			break;

		size -= cmp_cnt;
		location++;
		start = 0;
	}

	if(size > 0) {
		OTP_SLEEP()
		return BK_ERR_OTP_UPDATE_NOT_EQUAL;  // BK_ERR_OTP_CANNOT_WRTIE; ???
	}

	/* restore all variables. */
	location = otp_map[item].offset / 4;
	start = otp_map[item].offset % 4;
	size = back_size;
	buf = back_buf;

	while(size > 0) {
		int cpy_cnt = (size >= (4 - start)) ? (4 - start) : size;

		value = 0;
		dst_data = (uint8_t *)&value;
		dst_data += start;

		switch( cpy_cnt ) {
			case 4:
				*dst_data++ = *buf++;
			case 3:
				*dst_data++ = *buf++;
			case 2:
				*dst_data++ = *buf++;
			case 1:
				*dst_data++ = *buf++;
		}

		otp_write_otp(location, value);
		check_value = otp_read_otp(location);
		if (check_value != value) {
			return BK_ERR_OTP_UPDATE_NOT_EQUAL;
		}

		size -= cpy_cnt;
		location++;
		start = 0;
	}

	OTP_SLEEP()
	return BK_OK;
}

/**
 * obtain AHB OTP value with item ID:
 * 1. allowed start address of item not aligned
 * 2. allowed end address of item not aligned
 */
bk_err_t bk_otp_ahb_read(otp2_id_t item, uint8_t* buf, uint32_t size)
{
	if (item >= OTP2_MAX_ID) {
		return BK_ERR_OTP_INDEX_WRONG;
	}
	if( bk_otp_ahb_read_permission(item) > OTP_READ_ONLY ) {
		return BK_ERR_NO_READ_PERMISSION;
	}
	OTP_ACTIVE(2)
	if(size > otp_map[item].allocated_size){
		OTP_SLEEP()
		return BK_ERR_OTP_ADDR_OUT_OF_RANGE;
	}
	uint32_t location = otp_map[item].offset / 4;
	uint32_t start = otp_map[item].offset % 4;
	uint32_t value;

	while(size > 0) {
		value = otp2_read_otp(location);

		uint8_t * src_data = (uint8_t *)&value;
		int       cpy_cnt;

		src_data += start;

		cpy_cnt = (size >= (4 - start)) ? (4 - start) : size;

		switch( cpy_cnt ) {
			case 4:
				*buf++ = *src_data++;
			case 3:
				*buf++ = *src_data++;
			case 2:
				*buf++ = *src_data++;
			case 1:
				*buf++ = *src_data++;
		}

		size -= cpy_cnt;
		location++;
		start = 0;
	}
	OTP_SLEEP()
	return BK_OK;
}

bk_err_t bk_otp_ahb_update(otp2_id_t item, uint8_t* buf, uint32_t size)
{
	if (item >= OTP2_MAX_ID) {
		return BK_ERR_OTP_INDEX_WRONG;
	}
	if( bk_otp_ahb_read_permission(item) != OTP_READ_WRITE ) {
		return BK_ERR_NO_WRITE_PERMISSION;
	}
	OTP_ACTIVE(2)
	if(size > otp_map[item].allocated_size){
		OTP_SLEEP()
		return BK_ERR_OTP_ADDR_OUT_OF_RANGE;
	}
	uint32_t location = otp_map[item].offset / 4;
	uint32_t start = otp_map[item].offset % 4;
	uint32_t value = 0;
	uint32_t check_value = 0;

	uint8_t * dst_data;
	uint8_t * back_buf = buf;
	uint32_t  back_size = size;

	while(size > 0) {
		value = otp2_read_otp(location);

		int       cmp_cnt = (size >= (4 - start)) ? (4 - start) : size;
		uint32_t  cmp_result = 0;

		dst_data = (uint8_t *)&value;
		dst_data += start;

		/* the initial data of every OTP memory bit is 0. */
		/* OTP memory bit can be changed to 1, */
		/* once it is changed to 1, it can't be reset to 0. */
		/* so check the target bit value before write to OTP memory. */
		/* if try to change OTP bit from 1 to 0, return fail. */
		switch( cmp_cnt ) {
			case 4:
				cmp_result |= (~(*buf) & (*dst_data));
				buf++; dst_data++;
			case 3:
				cmp_result |= (~(*buf) & (*dst_data));
				buf++; dst_data++;
			case 2:
				cmp_result |= (~(*buf) & (*dst_data));
				buf++; dst_data++;
			case 1:
				cmp_result |= (~(*buf) & (*dst_data));
				buf++; dst_data++;
		}

		if(cmp_result != 0)
			break;

		size -= cmp_cnt;
		location++;
		start = 0;
	}
	
	if(size > 0) {
		OTP_SLEEP()
		return BK_ERR_OTP_UPDATE_NOT_EQUAL;  // BK_ERR_OTP_CANNOT_WRTIE; ???
	}

	/* restore all variables. */
	location = otp_map[item].offset / 4;
	start = otp_map[item].offset % 4;
	size = back_size;
	buf = back_buf;
	
	while(size > 0) {
		int cpy_cnt = (size >= (4 - start)) ? (4 - start) : size;

		value = 0;
		dst_data = (uint8_t *)&value;
		dst_data += start;

		switch( cpy_cnt ) {
			case 4:
				*dst_data++ = *buf++;
			case 3:
				*dst_data++ = *buf++;
			case 2:
				*dst_data++ = *buf++;
			case 1:
				*dst_data++ = *buf++;
		}

		otp2_write_otp(location, value);
		check_value = otp2_read_otp(location);
		if (check_value != value) {
			return BK_ERR_OTP_UPDATE_NOT_EQUAL;
		}

		size -= cpy_cnt;
		location++;
		start = 0;
	}

	OTP_SLEEP()

	return BK_OK;
}

bk_err_t bk_otp_read_random_number(uint32_t* value, uint32_t size)
{
	OTP_ACTIVE(1)
	for(int i = 0; i < size; ++i){
		value[i] = otp_hal_read_random_number(&s_otp.hal);
	}
	OTP_SLEEP()
	return BK_OK;
}

/*Initial Phase,done at factory*/
#if CONFIG_ATE_TEST
static bk_err_t bk_otp_init_puf()
{
	if(otp_hal_read_enroll(&s_otp.hal) == 0xF){
		return BK_OK;
	} else if(otp_hal_read_enroll(&s_otp.hal) == 0x0){
		otp_hal_do_puf_enroll(&s_otp.hal);
	}
	uint32_t flag = otp_hal_read_status(&s_otp.hal);
	if(flag & OTP_ERROR){
		OTP_LOGE("puf enrollment fail.\r\n");
		return BK_ERR_OTP_OPERATION_ERROR;
	} else if(flag & OTP_WARNING) {
		OTP_LOGW("puf enrollment has been executed.\r\n");
	} else if(flag & OTP_WRONG) {
		OTP_LOGE("PTM data is not correctly set.\r\n");
		return BK_ERR_OTP_OPERATION_WRONG;
	} else if(flag & OTP_FORBID) {
		OTP_LOGE("TMlck is locked.\r\n");
		return BK_ERR_OTP_OPERATION_FORBID;
	}
	if(otp_hal_read_enroll(&s_otp.hal) != 0xF){
		OTP_LOGE("Error code:0x%x\r\n",otp_hal_read_status(&s_otp.hal));
		return BK_ERR_OTP_OPERATION_FAIL;
	}
	return BK_OK;
}

bk_err_t bk_otp_read_raw_entropy_output(uint32_t* value)
{
	otp_hal_set_fre_cont(&s_otp.hal, 1);
	*value = otp_hal_read_random_number(&s_otp.hal);
	otp_hal_set_fre_cont(&s_otp.hal, 0);
	return BK_OK;
}

static bk_err_t bk_otp_check_puf_quality()
{
	uint32_t flag = otp_hal_do_puf_quality_check(&s_otp.hal);
	if(flag & OTP_ERROR){
		OTP_LOGE("quality check fail.\r\n");
		return BK_ERR_OTP_OPERATION_ERROR;
	} else if(flag & OTP_WARNING) {
		OTP_LOGE("please init puf before quality check.\r\n");
		return BK_ERR_OTP_OPERATION_WARNING;
	} else if(flag & OTP_WRONG) {
		OTP_LOGE("PTM data is not correctly set.\r\n");
		return BK_ERR_OTP_OPERATION_WRONG;
	}
	return BK_OK;
}

static bk_err_t bk_otp_check_puf_health()
{
	uint32_t flag = otp_hal_do_puf_health_check(&s_otp.hal);
	while(flag & OTP_BUSY);
	flag = otp_hal_read_status(&s_otp.hal);
	if(flag & OTP_ERROR){
		OTP_LOGE("health check fail.\r\n");
		return BK_ERR_OTP_OPERATION_ERROR;
	} else if(flag & OTP_WARNING) {
		OTP_LOGE("please init puf before quality check.\r\n");
		return BK_ERR_OTP_OPERATION_WARNING;
	} else if(flag & OTP_WRONG) {
		OTP_LOGE("PTM data is not correctly set.\r\n");
		return BK_ERR_OTP_OPERATION_WRONG;
	}
	return BK_OK;
}

static bk_err_t bk_otp_do_auto_repair()
{
	uint32_t status = otp_hal_do_auto_repair(&s_otp.hal);
	uint32_t flag = status & 0x1F;
	uint32_t failcnt = (status & 0xF00) >> 8;

	if(failcnt != 0){
		OTP_LOGE("The total number of failed address is 0x%x.\r\n",failcnt);
	}
	if(failcnt > 4){
		return failcnt;
	}
	if(flag & OTP_ERROR){
		OTP_LOGE("auto repair fail.\r\n");
		return BK_ERR_OTP_OPERATION_ERROR;
	} else if(flag & OTP_WARNING) {
		OTP_LOGW("auto repair has been previously executed.\r\n");
	} else if(flag & OTP_WRONG) {
		OTP_LOGE("PTM data is not correctly set.\r\n");
		return BK_ERR_OTP_OPERATION_WRONG;
	} else if(flag & OTP_FORBID) {
		OTP_LOGE("TMLCK is locked.\r\n");
		return BK_ERR_OTP_OPERATION_FORBID;
	}
	return BK_OK;
}

static bk_err_t bk_otp_enable_shuffle_read()
{
	otp_hal_set_flag(&s_otp.hal,0x99);
	uint32_t flag = otp_hal_read_status(&s_otp.hal);
	if(flag & OTP_ERROR){
		OTP_LOGE("enable shuffle read fail.\r\n");
		return BK_ERR_OTP_OPERATION_ERROR;
	} else if(flag & OTP_WARNING) {
		OTP_LOGW("shuffle read has been re-programmed.\r\n");
	} else if(flag & OTP_WRONG) {
		OTP_LOGE("PTM data is not correctly set.\r\n");
		return BK_ERR_OTP_OPERATION_WRONG;
	} else if(flag & OTP_FORBID) {
		OTP_LOGE("TMLCK is locked.\r\n");
		return BK_ERR_OTP_OPERATION_FORBID;
	}
	volatile int delay = 1000;
	while(otp_hal_read_shfren(&s_otp.hal) != 0xF && delay){
		--delay;
	}
	otp_hal_deinit(&s_otp.hal);
	if(otp_hal_init(&s_otp.hal) != BK_OK){
		return BK_ERR_OTP_INIT_FAIL;
	}
	while(otp_hal_check_busy(&s_otp.hal));
	return BK_OK;
}

static bk_err_t bk_otp_enable_shuffle_write()
{
	otp_hal_set_flag(&s_otp.hal,0xc2);
	uint32_t flag = otp_hal_read_status(&s_otp.hal);
	while(flag & OTP_BUSY);
	flag = otp_hal_read_status(&s_otp.hal);
	if(flag & OTP_ERROR){
		OTP_LOGE("enable shuffle write fail.\r\n");
		return BK_ERR_OTP_OPERATION_ERROR;
	} else if(flag & OTP_WARNING) {
		OTP_LOGW("shuffle write has been re-programmed.\r\n");
	} else if(flag & OTP_WRONG) {
		OTP_LOGE("PTM data is not correctly set.\r\n");
		return BK_ERR_OTP_OPERATION_WRONG;
	} else if(flag & OTP_FORBID) {
		OTP_LOGE("TMLCK is locked.\r\n");
		return BK_ERR_OTP_OPERATION_FORBID;
	}
	volatile int delay = 1000;
	while(otp_hal_read_shfwen(&s_otp.hal) != 0xF && delay){
		--delay;
	}
	otp_hal_deinit(&s_otp.hal);
	if(otp_hal_init(&s_otp.hal) != BK_OK){
		return BK_ERR_OTP_INIT_FAIL;
	}
	while(otp_hal_check_busy(&s_otp.hal));
	return BK_OK;
}

static bk_err_t bk_otp_set_ptc_page(uint32_t page)
{
	otp_hal_set_ptc_page(&s_otp.hal,page);
	return BK_OK;
}

static bk_err_t bk_otp_test_page(uint32_t start, uint32_t end)
{
	uint32_t data = 0x5A5A5A5A;
	uint8_t bit = 0x1;
	for(uint32_t i = start;i < end;i++){
		otp_hal_write_test_row(&s_otp.hal,i,data);
		if(otp_hal_read_test_row(&s_otp.hal,i) != data){
			OTP_LOGE("test row %d = 0x%x,should be 0x%x.\r\n",i,otp_hal_read_test_row(&s_otp.hal,i),data);
			return BK_ERR_OTP_OPERATION_ERROR;
		}
		for(uint32_t j = 0; j < 4;j++){
			otp_hal_set_ptc_page(&s_otp.hal,j);
			otp_hal_write_test_column(&s_otp.hal,i,bit);
			if(otp_hal_read_test_column(&s_otp.hal,i) != bit){
				OTP_LOGE("test column %d  in page %d = 0x%x,should be 0x%x.\r\n",i,otp_hal_read_test_column(&s_otp.hal,i),bit);
				return BK_ERR_OTP_OPERATION_ERROR;
			}
		}
	}
	return BK_OK;
}

static bk_err_t bk_otp_off_margin_read()
{
	otp_hal_enable_off_margin_read(&s_otp.hal);
	if(bk_otp_test_page(8,12) == BK_OK){
		otp_hal_disable_off_margin_read(&s_otp.hal);
		return BK_OK;
	}
	OTP_LOGE("off margin read fail.\r\n");
	return BK_ERR_OTP_OPERATION_ERROR;
}

static bk_err_t bk_otp_on_margin_read()
{
	otp_hal_enable_on_margin_read(&s_otp.hal);
	if(bk_otp_test_page(12,16) == BK_OK){
		otp_hal_disable_on_margin_read(&s_otp.hal);
		return BK_OK;
	}
	OTP_LOGE("on margin read fail.\r\n");
	return BK_ERR_OTP_OPERATION_ERROR;
}

static bk_err_t bk_otp_initial_off_check()
{
	uint32_t flag = otp_hal_init_off_check(&s_otp.hal);
	if(flag & OTP_ERROR){
		OTP_LOGE("initial_off_check fail.\r\n");
		return BK_ERR_OTP_OPERATION_ERROR;
	} else if(flag & OTP_WRONG) {
		OTP_LOGE("PTM data is not correctly set.\r\n");
		return BK_ERR_OTP_OPERATION_WRONG;
	} else if(flag & OTP_FORBID) {
		OTP_LOGE("TMLCK is locked.\r\n");
		return BK_ERR_OTP_OPERATION_FORBID;
	}
	return BK_OK;
}

static bk_err_t bk_otp_enable_program_protect_function()
{
	if(otp_hal_read_pgmprt(&s_otp.hal) != 0xF){
		return BK_OK;
	}
	otp_hal_set_flag(&s_otp.hal,0xb6);
	uint32_t flag = otp_hal_read_status(&s_otp.hal);
	if(flag & OTP_ERROR){
		OTP_LOGE("program_protect fail.\r\n");
		return BK_ERR_OTP_OPERATION_ERROR;
	} else if(flag & OTP_WARNING) {
		OTP_LOGW("program_protect has been previously executed.\r\n");
	} else if(flag & OTP_WRONG) {
		OTP_LOGW("PTM data is not correctly set.\r\n");
		return BK_OK;
	} else if(flag & OTP_FORBID) {
		OTP_LOGE("TMLCK is locked.\r\n");
		return BK_ERR_OTP_OPERATION_FORBID;
	}
	if(otp_hal_read_pgmprt(&s_otp.hal) != 0xF){
		OTP_LOGE("program_protect fail.\r\n");
		return BK_ERR_OTP_OPERATION_FAIL;
	}
	return BK_OK;
}

static bk_err_t bk_otp_enable_test_mode_lock_function()
{
	if(otp_hal_read_tmlck(&s_otp.hal) == 0xF){
		return BK_OK;
	}
	otp_hal_set_flag(&s_otp.hal,0x71);
	uint32_t flag = otp_hal_read_status(&s_otp.hal);
	if(flag & OTP_ERROR){
		OTP_LOGE("test_mode_lock fail.\r\n");
		return BK_ERR_OTP_OPERATION_ERROR;
	} else if(flag & OTP_WARNING) {
		OTP_LOGW("test_mode_lock has been previously executed.\r\n");
	} else if(flag & OTP_WRONG) {
		OTP_LOGE("PTM data is not correctly set.\r\n");
		return BK_ERR_OTP_OPERATION_WRONG;
	} else if(flag & OTP_FORBID) {
		OTP_LOGE("TMLCK is locked.\r\n");
		return BK_ERR_OTP_OPERATION_FORBID;
	}
	if(otp_hal_read_tmlck(&s_otp.hal) != 0xF){
		OTP_LOGE("test_mode_lock fail.\r\n");
		return BK_ERR_OTP_OPERATION_FAIL;
	}
	return BK_OK;
}

uint32_t bk_otp_fully_flow_test()
{
	OTP_ACTIVE(1)
	uint32_t ret;
	uint32_t retry_count = 10;
	uint32_t fail_code = 0;
	/*test mode lck means test has been success*/
	if(otp_hal_read_tmlck(&s_otp.hal) == 0xF){
		return BK_OK;
	}

	ret = bk_otp_init_puf();
	if(ret != BK_OK){
		fail_code |= 1 << 0;
		goto exit;
	}
	ret = bk_otp_check_puf_quality();
	if(ret != BK_OK){
		fail_code |= 1 << 1;
		goto exit;
	}
	ret = bk_otp_check_puf_health();
	if(ret != BK_OK){
		fail_code |= 1 << 2;
		goto exit;
	}
	ret = bk_otp_do_auto_repair();
	if(ret != BK_OK){
		fail_code |= 1 << 3;
		if (ret > 0)
			fail_code |= ret << 12;
		goto exit;
	}

	while(otp_hal_read_shfwen(&s_otp.hal) != 0xF && retry_count){
		ret = bk_otp_enable_shuffle_write();
		--retry_count;
		if(ret != BK_OK){
			fail_code |= 1 << 4;
			goto exit;
		}
	}
	if(otp_hal_read_shfwen(&s_otp.hal) != 0xF){
		OTP_LOGE("enable shuffle write fail.\r\n");
		ret = BK_ERR_OTP_OPERATION_FAIL;
		goto exit;
	}

	retry_count = 10;
	while(otp_hal_read_shfren(&s_otp.hal) != 0xF && retry_count){
		ret = bk_otp_enable_shuffle_read();
		--retry_count;
		if(ret != BK_OK){
			fail_code |= 1 << 5;
			goto exit;
		}
	}

	if(otp_hal_read_shfren(&s_otp.hal) != 0xF){
		OTP_LOGE("enable shuffle read fail.\r\n");
		ret = BK_ERR_OTP_OPERATION_FAIL;
		goto exit;
	}

	ret = bk_otp_test_page(0,8);
	if(ret != BK_OK){
		fail_code |= 1 << 6;
		goto exit;
	}
	ret = bk_otp_off_margin_read();
	if(ret != BK_OK){
		fail_code |= 1 << 7;
		goto exit;
	}
	ret = bk_otp_on_margin_read();
	if(ret != BK_OK){
		fail_code |= 1 << 8;
		goto exit;
	}
	ret = bk_otp_initial_off_check();
	if(ret != BK_OK){
		fail_code |= 1 << 9;
		goto exit;
	}
	ret = bk_otp_enable_program_protect_function();
	if(ret != BK_OK){
		fail_code |= 1 << 10;
		goto exit;
	}
	ret = bk_otp_enable_test_mode_lock_function();
	if(ret != BK_OK){
		fail_code |= 1 << 11;
		goto exit;
	}

exit:
	OTP_SLEEP()
	switch (ret)
	{
		case BK_ERR_OTP_OPERATION_ERROR:
			fail_code |= 1 << 16;
			break;

		case BK_ERR_OTP_OPERATION_WRONG:
			fail_code |= 1 << 17;
			break;

		case BK_ERR_OTP_OPERATION_FORBID:
			fail_code |= 1 << 18;
			break;

		case BK_ERR_OTP_OPERATION_FAIL:
			fail_code |= 1 << 19;
			break;

		case BK_ERR_OTP_INIT_FAIL:
			fail_code |= 1 << 20;
			break;

		default:
			break;
	}
	return fail_code;
}
#endif /*CONFIG_ATE_TEST*/
