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
/*******************************************************************************
 * Implementation of Threading for free RTOS MBEDTLS
 * follows same format as pthreading version but uses rtos semaphores (threading.h)
 * Used when set MBEDTLS config: MBEDTLS_THREADING_ALT
 *******************************************************************************
 */

#pragma once

#include "FreeRTOS.h"
#include "semphr.h"

typedef struct
{
    SemaphoreHandle_t mutex;
    char is_valid;
} mbedtls_threading_mutex_t;

// register these functions in mbedtls_threading_set_alt
static inline void threading_mutex_init_freertos( mbedtls_threading_mutex_t *mutex )
{
    if( mutex == NULL )
        return;
    mutex->mutex = xSemaphoreCreateMutex();
    mutex->is_valid = mutex->mutex != NULL;
}

static inline void threading_mutex_free_freertos( mbedtls_threading_mutex_t *mutex )
{
    if( mutex == NULL || ! mutex->is_valid )
        return;

    (void) vSemaphoreDelete(mutex->mutex);
    mutex->is_valid = 0;
}

static inline int threading_mutex_lock_freertos( mbedtls_threading_mutex_t *mutex )
{
    if( mutex == NULL || ! mutex->is_valid )
        return( MBEDTLS_ERR_THREADING_BAD_INPUT_DATA );

    if( xSemaphoreTake( mutex->mutex, portMAX_DELAY ) != pdTRUE )
        return( MBEDTLS_ERR_THREADING_MUTEX_ERROR );

    return( 0 );
}

static inline int threading_mutex_unlock_freertos( mbedtls_threading_mutex_t *mutex )
{
    if( mutex == NULL || ! mutex->is_valid )
        return( MBEDTLS_ERR_THREADING_BAD_INPUT_DATA );

    if( xSemaphoreGive( mutex->mutex ) != pdTRUE )
        return( MBEDTLS_ERR_THREADING_MUTEX_ERROR );

    return( 0 );
}
