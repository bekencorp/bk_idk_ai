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

#ifdef __cplusplus
extern "C" {
#endif


/*******************************************************************************
*                      Function Declarations
*******************************************************************************/

/**
 * @brief     Modem Driver initialization
 *
 * Initialize the bk modem driver, then it will build a connection to modem modules.
 * Make sure to connect to a modem mudule by usb in hardware before call this API.
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_modem_init(void);

/**
 * @brief     Modem Driver uninstallation
 *
 * Turn off the Modem driver, and it will disconnect to modem modules.
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_modem_deinit(void);


