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

#ifndef _LWIP_ADAPTER__H_
#define _LWIP_ADAPTER__H_

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
	/// os pthread function pointer
	int (*_pthread_key_create)(uint32_t *key, void *destructor);
	void *(*_pthread_getspecific)(uint32_t key);
	int (*_pthread_setspecific)(uint32_t key, const void *value);
} lwip_os_funcs_t;

bk_err_t bk_lwip_adapter_init(void);
int lwip_osi_funcs_init(void *osi_funcs);
void sys_thread_hostent_init();
struct hostent *sys_thread_hostent(const struct hostent *src);

#ifdef __cplusplus
}
#endif
#endif //_LWIP_ADAPTER__H_