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
#include "lwip/err.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/ip_addr.h"
#include "lwip/api.h"
#include "lwip/dns.h"

#include <string.h> /* memset */
#include <stdlib.h> /* atoi */

#include "pthread_internal.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "os/mem.h"
#include "bk_lwip_adapter.h"


static int _pthread_key_create_wrapper(uint32_t *key, void *destructor)
{
	return pthread_key_create(key, (pthread_destructor_t)destructor);
}

static void *_pthread_getspecific_wrapper(uint32_t key)
{
	return pthread_getspecific(key);
}

static int _pthread_setspecific_wrapper(uint32_t key, const void *value)
{
	return pthread_setspecific(key, value);
}

static const lwip_os_funcs_t g_lwip_os_funcs = {
	._pthread_key_create = _pthread_key_create_wrapper,
	._pthread_getspecific = _pthread_getspecific_wrapper,
	._pthread_setspecific = _pthread_setspecific_wrapper,
};

bk_err_t bk_lwip_adapter_init(void)
{
    bk_err_t ret = BK_OK;

    if (lwip_osi_funcs_init((void*)&g_lwip_os_funcs) != 0)
    {
        return BK_FAIL;
    }

    return ret;
}

