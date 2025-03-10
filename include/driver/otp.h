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

#pragma once

#include <common/bk_include.h>
#include <driver/otp_types.h>

#ifdef __cplusplus
extern "C" {
#endif


/* @brief Overview about this API header
 *
 */

/* @brief     OTP initialization
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_otp_init(void);

/**
 * @brief     OTP finalization
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_otp_deinit(void);
/**
 * @brief OTP API
 * @defgroup bk_api_otp OTP API group
 * @{
 */
 
/**
 * @brief     OTP read operation
 *
 * @param buffer point to the buffer that reads the data
 * @param addr read address, address range 0x0--0x800
 * @param len read length
 *
 * @attention 1. addr+len should be less than 0x800
 * @attention 2. this API can only access non-secure OTP bank
 * @attention 3. this api blocks for 5ms waiting for OTP to stabilize
 *
 * @return
 *    - BK_OK: succeed
 *    - BK_ERR_EFUSE_DRIVER_NOT_INIT: EFUSE driver not init
 *    - BK_ERR_EFUSE_ADDR_OUT_OF_RANGE: EFUSE address is out of range
 *    - others: other errors.
 */
bk_err_t bk_otp_read_bytes_nonsecure(uint8_t *buffer, uint32_t addr, uint32_t len);
/**
 * @brief     OTP read with item type
 *
 * @param item the item to read
 * @param buf point to the buffer that reads the data
 * @param size length of item to read
 *
 * @return
 *    - BK_OK: succeed
 *    - BK_ERR_NO_READ_PERMISSION: wrong permission to read
 *    - BK_ERR_OTP_ADDR_OUT_OF_RANGE: param size not match item real size
 *    - others: other errors.
 */
#if CONFIG_OTP_V1
bk_err_t bk_otp_apb_read(otp1_id_t item, uint8_t *buf, uint32_t size);
/**
 * @brief     update OTP write with item type
 *
 * @param item the item to update
 * @param buf point to the buffer that updates the data
 * @param size length of buffer to update
 *
 * @return
 *    - BK_OK: succeed
 *    - BK_ERR_NO_WRITE_PERMISSION: wrong permission to write
 *    - BK_ERR_OTP_ADDR_OUT_OF_RANGE: param size exceeds item size
 *    - BK_ERR_OTP_UPDATE_NOT_EQUAL: updated value not match expectation 
 *    - others: other errors.
 */
bk_err_t bk_otp_apb_update(otp1_id_t item, uint8_t* buf, uint32_t size);
/**
 * @brief     OTP2 read with item type
 *
 * @param item the item to read
 * @param buf point to the buffer that reads the data
 * @param size length of item to read
 *
 * @return
 *    - BK_OK: succeed
 *    - BK_ERR_NO_READ_PERMISSION: wrong permission to read
 *    - BK_ERR_OTP_ADDR_OUT_OF_RANGE: param size not match item real size
 *    - others: other errors.
 */
bk_err_t bk_otp_ahb_read(otp2_id_t item, uint8_t* buf, uint32_t size);
/**
 * @brief     update OTP2 write with item type
 *
 * @param item the item to update
 * @param buf point to the buffer that updates the data
 * @param size length of buffer to update
 *
 * @return
 *    - BK_OK: succeed
 *    - BK_ERR_NO_WRITE_PERMISSION: wrong permission to write
 *    - BK_ERR_OTP_ADDR_OUT_OF_RANGE: param size exceeds item size
 *    - BK_ERR_OTP_UPDATE_NOT_EQUAL: updated value not match expectation
 *    - others: other errors.
 */
bk_err_t bk_otp_ahb_update(otp2_id_t item, uint8_t* buf, uint32_t size);

/**
 * @brief     read OTP1 permission
 *
 * @param item the item to be read
 *
 * @return
 *    - OTP_NO_ACCESS: No Access permission
 *    - OTP_READ_ONLY: Read Only permission
 *    - OTP_READ_WRITE: Read Write permission
 *    - BK_FAIL: error
 */
otp_privilege_t bk_otp_apb_read_permission(otp1_id_t item);

/**
 * @brief     write OTP1 permission
 *
 * @param item the item to be written
 * @param permission the permission to be written
 *
 * @return
 *    - BK_OK: succeed
 *    - BK_FAIL: error
 */
bk_err_t bk_otp_apb_write_permission(otp1_id_t item, otp_privilege_t permission);

/**
 * @brief     read OTP1 mask
 *
 * @param item the item to be read
 *
 * @return
 *    - OTP_READ_ONLY: Read Only permission
 *    - OTP_READ_WRITE: Read Write permission
 *    - BK_FAIL: error
 */
otp_privilege_t bk_otp_apb_read_mask(otp1_id_t item);

/**
 * @brief     write OTP1 permission
 *
 * @param item the item to be written
 * @param permission the permission to be written
 *
 * @return
 *    - BK_OK: succeed
 *    - BK_FAIL: error
 */
bk_err_t bk_otp_apb_write_mask(otp1_id_t item, otp_privilege_t permission);

/**
 * @brief     read random number
 *
 * @param buf the buffer to store value
 * @param size the size of buffer
 *
 * @return
 *    - BK_OK: succeed
 *    - BK_FAIL: error
*/
bk_err_t bk_otp_read_random_number(uint32_t* buf, uint32_t size);
#else
#define bk_otp_apb_read(item, buf, size) (-1)
#endif
/**
 * @}
 */
/**
 * @}
 */

#ifdef __cplusplus
}
#endif


