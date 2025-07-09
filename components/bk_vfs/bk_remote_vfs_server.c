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

#include <string.h>
#include <common/bk_include.h>
#include <os/mem.h>
#include <os/os.h>
#include "bk_remote_vfs_ipc.h"
#include <driver/mb_ipc.h>
#include <driver/mb_ipc_port_cfg.h>
#include "../../../components/bk_rtos/rtos_ext.h"
#include "bk_posix.h"


#if CONFIG_CACHE_ENABLE
#include "cache.h"
#endif

#define TAG		"vfs_s"


#define LOCAL_TRACE_E     (1)
#define LOCAL_TRACE_I     (0)

#define TRACE_E(...)        do { if(LOCAL_TRACE_E) BK_LOGE(__VA_ARGS__); } while(0)
#define TRACE_I(...)        do { if(LOCAL_TRACE_I) BK_LOGI(__VA_ARGS__); } while(0)

#define VFS_SVR_PRIORITY               BEKEN_DEFAULT_WORKER_PRIORITY
#define VFS_SVR_STACK_SIZE             1536

#define VFS_SVR_CONNECT_MAX            2
/* connection ID bits, 8 bits for max 8 connections (0x01 ~ 0xFF). */
#define VFS_SVR_CONNECT_EVENTS         ((0x01 << (VFS_SVR_CONNECT_MAX)) - 1)   //  0x03
#define VFS_SVR_QUIT_EVENT             (0x100)

#define VFS_SVR_EVENTS         (VFS_SVR_CONNECT_EVENTS | VFS_SVR_QUIT_EVENT)

#define VFS_SVR_WAIT_TIME      600


static u8 s_vfs_svr_init = 0;
static rtos_event_ext_t  vfs_svr_event;


static const uint32_t crc32_table[] =
{
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static uint32_t calc_crc32_xor(uint32_t crc, const void *buf, size_t size)
{
    const uint8_t *p;

    p = (const uint8_t *)buf;
    crc = crc ^ ~0U;

    while (size--) {
        crc = crc32_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ ~0U;
}

static uint32_t calc_crc32(uint32_t crc, const uint8_t *buf, int len)
{
	while(len--)
	{
		crc = (crc >> 8)^(crc32_table[(crc^*buf++)&0xff]);
	}

	return crc;
}

/*
extern uint32_t calc_crc32_xor(uint32_t crc, const void *buf, size_t size);
extern uint32_t calc_crc32(uint32_t crc, const uint8_t *buf, int len); */

static void vfs_error_handler(u32 handle, u8 user_cmd)
{
	vfs_cmd_t   cmd_buff;
	
	memset(&cmd_buff, 0, sizeof(cmd_buff));
	cmd_buff.ret_status = BK_FAIL;
	mb_ipc_send(handle, user_cmd, (u8 *)&cmd_buff, sizeof(cmd_buff), VFS_SVR_WAIT_TIME);
}



static int bk_remote_vfs_mount_fatfs(char *mount_point) {
	struct bk_fatfs_partition partition;
	char *fs_name = NULL;

	int ret;

	fs_name = "fatfs";
	partition.part_type = FATFS_DEVICE;
	partition.part_dev.device_name = FATFS_DEV_SDCARD;
	partition.mount_path = mount_point;

	ret = mount("SOURCE_NONE", partition.mount_path, fs_name, 0, &partition);

	return ret;
}


static void vfs_mount_handler(u32 handle, vfs_cmd_t *cmd_buff)
{
	BK_LOGE(TAG, "enter %s @%d.\r\n", __FUNCTION__, __LINE__);

	cmd_buff->ret_status = bk_remote_vfs_mount_fatfs("/");

	int ret_val = mb_ipc_send(handle, VFS_CMD_MOUNT, (u8 *)cmd_buff, sizeof(vfs_cmd_t), VFS_SVR_WAIT_TIME);

	if(ret_val != 0)
		TRACE_E(TAG, "0x%x, mount:ret_status=%d, ret_val=%d.\r\n", handle, cmd_buff->ret_status, ret_val);

	(void)ret_val;
}

static void vfs_umount_handler(u32 handle, vfs_cmd_t *cmd_buff)
{
	BK_LOGE(TAG, "enter %s @%d.\r\n", __FUNCTION__, __LINE__);

	cmd_buff->ret_status = umount("/");

	int ret_val = mb_ipc_send(handle, VFS_CMD_UMOUNT, (u8 *)cmd_buff, sizeof(vfs_cmd_t), VFS_SVR_WAIT_TIME);

	if(ret_val != 0)
		TRACE_E(TAG, "0x%x, umount:ret_status=%d, ret_val=%d.\r\n", handle, cmd_buff->ret_status, ret_val);

	(void)ret_val;
}


static void vfs_open_handler(u32 handle, vfs_cmd_t *cmd_buff)
{
	int     fd;
	char *path =  os_malloc( strlen(cmd_buff->path) + 1 );



	int oflag = cmd_buff->oflag;

	strncpy(path, cmd_buff->path, (strlen(cmd_buff->path)+1));

	fd = open(path, oflag);

	cmd_buff->fd = fd;


	TRACE_I(TAG, "%s @%d, 0x%x open debug, cmd_buff->fd=%d, path=%s.  !\r\n", __FUNCTION__, __LINE__, handle, cmd_buff->fd, path);


	int ret_val = mb_ipc_send(handle, VFS_CMD_OPEN, (u8 *)cmd_buff, sizeof(vfs_cmd_t), VFS_SVR_WAIT_TIME);

	if(ret_val != 0)
		TRACE_E(TAG, "0x%x, open fd: %d, ret = %d.\r\n", handle, cmd_buff->fd, ret_val);


	os_free(path);

	(void)ret_val;
}

static void vfs_close_handler(u32 handle, vfs_cmd_t *cmd_buff)
{
	int     fd;
	fd = cmd_buff->fd;

	cmd_buff->ret_status = close(fd);

	int ret_val = mb_ipc_send(handle, VFS_CMD_CLOSE, (u8 *)cmd_buff, sizeof(vfs_cmd_t), VFS_SVR_WAIT_TIME);

	if(ret_val != 0)
		TRACE_E(TAG, "0x%x, close fd:%d, ret_status= %d, ret_val=%d.\r\n", handle, cmd_buff->fd, cmd_buff->ret_status, ret_val);

	(void)ret_val;
}


static void vfs_read_handler(u32 handle, vfs_cmd_t *cmd_buff, u8 connect_id)
{
	int      read_status = BK_OK;

	u8     * read_buff = os_malloc(cmd_buff->count);

	if(read_buff == NULL)
	{
		vfs_error_handler(handle, VFS_CMD_READ);

		TRACE_E(TAG, "%s @%d, 0x%x memory overrun!\r\n", __FUNCTION__, __LINE__, handle);

		return;
	}


	//if success, return real file buffer length
	read_status = read(cmd_buff->fd, read_buff, cmd_buff->count);

	TRACE_I(TAG, "DEBUG %s @%d, 0x%x read failed, read_status = %d, cmd_buff->fd=%d, cmd_buff->count=%d !\r\n", __FUNCTION__, __LINE__, handle, read_status, cmd_buff->fd, cmd_buff->count);

	cmd_buff->ret_status = read_status;
	cmd_buff->buff = read_buff;
	if(read_status > 0)
	{
		cmd_buff->crc = calc_crc32(0, read_buff, read_status);
	}

	int     ret_val;

	ret_val = mb_ipc_send(handle, VFS_CMD_READ, (u8 *)cmd_buff, sizeof(vfs_cmd_t), VFS_SVR_WAIT_TIME);

	//file read real length may less than expect
	if(read_status <= 0)
	{
		os_free(read_buff);
		TRACE_I(TAG, "%s @%d, 0x%x read failed, read_status = %d!\r\n", __FUNCTION__, __LINE__, handle, read_status);
		return;
	}

	if(ret_val != 0)
	{
		(*read_buff) ^= 0x01;   // make crc not match
		os_free(read_buff);
		TRACE_E(TAG, "%s @%d, 0x%x send failed!\r\n", __FUNCTION__, __LINE__, handle);
		return;
	}

	u32 event = rtos_wait_event_ex(&vfs_svr_event, (0x01 << connect_id), 1, VFS_SVR_WAIT_TIME);

	if(event == 0)
	{
		(*read_buff) ^= 0x01;   // make crc not match
		os_free(read_buff);
		TRACE_E(TAG, "%s @%d, 0x%x read done failed!\r\n", __FUNCTION__, __LINE__, handle);
		return;
	}

	memset(cmd_buff, 0, sizeof(vfs_cmd_t));

	u8     user_cmd = INVALID_USER_CMD_ID;

	ret_val = mb_ipc_recv(handle, &user_cmd, (u8 *)cmd_buff,  sizeof(vfs_cmd_t), VFS_SVR_WAIT_TIME);

	if((ret_val != sizeof(vfs_cmd_t)) || (user_cmd != VFS_CMD_READ_DONE))
	{
		TRACE_E(TAG, "%s @%d, 0x%x recv cmd%d, %d, 0x%x!\r\n", __FUNCTION__, __LINE__, handle, user_cmd, ret_val, event);
	}
	else
	{
		memset(cmd_buff, 0, sizeof(vfs_cmd_t));
		cmd_buff->ret_status = BK_OK;

		// it is just a handshake. every recv from client must have a send.
		mb_ipc_send(handle, VFS_CMD_READ_DONE, (u8 *)cmd_buff, sizeof(vfs_cmd_t), VFS_SVR_WAIT_TIME);
	}

	(*read_buff) ^= 0x01;   // make crc not match

	os_free(read_buff);

	TRACE_I(TAG, "0x%x read done, user_cmd: %d.\r\n", handle, user_cmd);

	return;
}


static void vfs_write_handler(u32 handle, vfs_cmd_t *cmd_buff)
{
	u32     cpy_len, write_len = 0;
	int     write_status = BK_OK;
	int     line_num = 0;

	u8    * write_buff = os_malloc(cmd_buff->count);
	u32     buff_len = cmd_buff->count;

	//if write length is 0, return valid
	if(cmd_buff->count <= 0)
	{
		return;
	}

	if(write_buff == NULL)
	{
		vfs_error_handler(handle, VFS_CMD_WRITE);
		TRACE_E(TAG, "%s @%d, 0x%x memory overrun!\r\n", __FUNCTION__, __LINE__, handle);
		return;
	}

	while(cmd_buff->count > write_len)
	{
		#if CONFIG_CACHE_ENABLE
		flush_dcache(cmd_buff->buff, cmd_buff->count);
		#endif

		cpy_len = MIN(buff_len, (cmd_buff->count - write_len));
		memcpy(write_buff, cmd_buff->buff + write_len, cpy_len);

		// check data validity after memcpy.
		if(cmd_buff->crc != calc_crc32(0, cmd_buff->buff, cmd_buff->count))
		{
			write_status = BK_FAIL;
			line_num = __LINE__;
			break;
		}
		
		write_status = write(cmd_buff->fd, (u8 *)write_buff, cpy_len);

		if(write_status == 0)
		{
			line_num = __LINE__;
			break;
		}
		
		write_len += cpy_len;
	}
	
	cmd_buff->ret_status = write_len;

	int ret_val = mb_ipc_send(handle, VFS_CMD_WRITE, (u8 *)cmd_buff, sizeof(vfs_cmd_t), VFS_SVR_WAIT_TIME);

	os_free(write_buff);

	if(ret_val != 0)
		TRACE_E(TAG, "0x%x, write fd:%d, ret_status=%d, ret_val= %d, line=%d.\r\n", handle, cmd_buff->fd, cmd_buff->ret_status, ret_val, line_num);


	(void)ret_val;
}


static void vfs_stat_handler(u32 handle, vfs_cmd_t *cmd_buff)
{
	int ret_val = 0;
	char *path =  os_malloc( strlen(cmd_buff->path) + 1 );

#if CONFIG_CACHE_ENABLE
	flush_dcache(cmd_buff->buff, sizeof(struct stat));
	flush_dcache(cmd_buff->path, (strlen(cmd_buff->path)+1));
#endif

	strncpy(path, cmd_buff->path, (strlen(cmd_buff->path)+1));

	ret_val = stat(path, (struct stat *)cmd_buff->buff);

	cmd_buff->ret_status = ret_val;
	TRACE_I(TAG, "%s @%d, 0x%x stat debug, cmd_buff->ret_status=%d, path=%s  !\r\n", __FUNCTION__, __LINE__, handle, cmd_buff->ret_status, path);


	ret_val = mb_ipc_send(handle, VFS_CMD_STAT, (u8 *)cmd_buff, sizeof(vfs_cmd_t), VFS_SVR_WAIT_TIME);
	if(ret_val != 0)
		TRACE_E(TAG, "0x%x, vfs_stat: ret = %d.\r\n", handle, ret_val);

	os_free(path);
	(void)ret_val;
}

static void vfs_opendir_handler(u32 handle, vfs_cmd_t *cmd_buff)
{
	int ret_val = 0;
	char *path =  os_malloc( strlen(cmd_buff->path) + 1 );

#if CONFIG_CACHE_ENABLE
	flush_dcache(cmd_buff->path, (strlen(cmd_buff->path)+1));
#endif

	strncpy(path, cmd_buff->path, (strlen(cmd_buff->path)+1));

	cmd_buff->buff = opendir(path);

	TRACE_I(TAG, "%s @%d, 0x%x debug, path=%s  !\r\n", __FUNCTION__, __LINE__, handle, path);

	ret_val = mb_ipc_send(handle, VFS_CMD_OPENDIR, (u8 *)cmd_buff, sizeof(vfs_cmd_t), VFS_SVR_WAIT_TIME);
	if(ret_val != 0)
		TRACE_E(TAG, "0x%x, ret = %d.\r\n", handle, ret_val);

	os_free(path);
	(void)ret_val;
}

static void vfs_readdir_handler(u32 handle, vfs_cmd_t *cmd_buff)
{
	int ret_val = 0;
	DIR *path =  os_malloc(sizeof(bk_dir));

#if CONFIG_CACHE_ENABLE
	flush_dcache(cmd_buff->path, sizeof(bk_dir));
#endif
	os_memcpy(path, cmd_buff->path, sizeof(bk_dir));

	cmd_buff->buff = readdir(path);

	TRACE_I(TAG, "%s @%d, 0x%x debug, path=%s  !\r\n", __FUNCTION__, __LINE__, handle, path);


	ret_val = mb_ipc_send(handle, VFS_CMD_READDIR, (u8 *)cmd_buff, sizeof(vfs_cmd_t), VFS_SVR_WAIT_TIME);
	if(ret_val != 0)
		TRACE_E(TAG, "0x%x, ret = %d.\r\n", handle, ret_val);

	os_free(path);
	(void)ret_val;
}

static void vfs_closedir_handler(u32 handle, vfs_cmd_t *cmd_buff)
{
	int ret_val = 0;
	DIR *path =  os_malloc(sizeof(bk_dir));

#if CONFIG_CACHE_ENABLE
	flush_dcache(cmd_buff->path, sizeof(bk_dir));
#endif
	os_memcpy(path, cmd_buff->path, sizeof(bk_dir));

	cmd_buff->ret_status = closedir(path);

	TRACE_I(TAG, "%s @%d, 0x%x debug, path=%s  !\r\n", __FUNCTION__, __LINE__, handle, path);


	ret_val = mb_ipc_send(handle, VFS_CMD_CLOSEDIR, (u8 *)cmd_buff, sizeof(vfs_cmd_t), VFS_SVR_WAIT_TIME);
	if(ret_val != 0)
		TRACE_E(TAG, "0x%x, ret = %d.\r\n", handle, ret_val);

	os_free(path);
	(void)ret_val;
}

static void vfs_mkdir_handler(u32 handle, vfs_cmd_t *cmd_buff)
{
	int ret_val = 0;
	char *path =  os_malloc( strlen(cmd_buff->path) + 1 );

#if CONFIG_CACHE_ENABLE
	flush_dcache(cmd_buff->path, (strlen(cmd_buff->path)+1));
#endif
	strncpy(path, cmd_buff->path, (strlen(cmd_buff->path)+1));

	cmd_buff->ret_status = mkdir(path, cmd_buff->oflag);

	TRACE_I(TAG, "%s @%d, 0x%x mkdir debug, path=%s  !\r\n", __FUNCTION__, __LINE__, handle, path);

	ret_val = mb_ipc_send(handle, VFS_CMD_MKDIR, (u8 *)cmd_buff, sizeof(vfs_cmd_t), VFS_SVR_WAIT_TIME);
	if(ret_val != 0)
		TRACE_E(TAG, "0x%x, ret = %d.\r\n", handle, ret_val);

	os_free(path);
	(void)ret_val;
}

static void vfs_ftruncate_handler(u32 handle, vfs_cmd_t *cmd_buff)
{
	int ret_val = 0;
	int fd = cmd_buff->fd;
	int offset = cmd_buff->oflag;

	cmd_buff->ret_status = ftruncate(fd, offset);

	TRACE_I(TAG, "%s @%d, 0x%x debug, fd=%d, offset=%d!\r\n", __FUNCTION__, __LINE__, handle, fd, offset);

	ret_val = mb_ipc_send(handle, VFS_CMD_FTRUNCATE, (u8 *)cmd_buff, sizeof(vfs_cmd_t), VFS_SVR_WAIT_TIME);
	if(ret_val != 0)
		TRACE_E(TAG, "0x%x, ret = %d.\r\n", handle, ret_val);

	(void)ret_val;
}


static void vfs_cmd_handler(u32 handle, u8 connect_id)
{
	int   recv_len = mb_ipc_get_recv_data_len(handle);
	
	vfs_cmd_t   cmd_buff;
	u8            user_cmd = INVALID_USER_CMD_ID;

	memset(&cmd_buff, 0, sizeof(cmd_buff));

	if(recv_len != sizeof( cmd_buff))
	{
		mb_ipc_recv(handle, &user_cmd, NULL, 0, 0);  // data_buff == NULL or buff_len == 0 just discard all data.

		// if client wait_forever, then must send a response even if user_cmd == INVALID_USER_CMD_ID.
		// vfs client does not wait forever, so only respond then cmd is valid.
		if(user_cmd != INVALID_USER_CMD_ID)
			vfs_error_handler(handle, user_cmd);

		TRACE_E(TAG, "recv user_cmd=%d, data len failed! %d\r\n", user_cmd, recv_len);

		return;
	}

	recv_len = mb_ipc_recv(handle, &user_cmd, (u8 *)&cmd_buff, sizeof(cmd_buff), 0);

	if(recv_len != sizeof( cmd_buff))
	{
		// if client wait_forever, then must send a response even if user_cmd == INVALID_USER_CMD_ID.
		// vfs client does not wait forever, so only respond then cmd is valid.
		if(user_cmd != INVALID_USER_CMD_ID)
			vfs_error_handler(handle, user_cmd);

		TRACE_E(TAG, "recv user_cmd=%d, data len failed! %d\r\n", user_cmd, recv_len);

		return;
	}

	switch(user_cmd)
	{
		// vfs driver cmds.  
		// these vfs driver cmds should be turned off when all APPs have been updated to use vfs partition APIs.
		case VFS_CMD_MOUNT:
			vfs_mount_handler(handle, &cmd_buff);
			break;

		case VFS_CMD_UMOUNT:
			vfs_umount_handler(handle, &cmd_buff);
			break;

		case VFS_CMD_OPEN:
			vfs_open_handler(handle, &cmd_buff);
			break;

		case VFS_CMD_CLOSE:
			vfs_close_handler(handle, &cmd_buff);
			break;

		case VFS_CMD_READ:
			vfs_read_handler(handle, &cmd_buff, connect_id);
			break;
			
		case VFS_CMD_WRITE:
			vfs_write_handler(handle, &cmd_buff);
			break;

		case VFS_CMD_STAT:
			vfs_stat_handler(handle, &cmd_buff);
			break;

		case VFS_CMD_OPENDIR:
			vfs_opendir_handler(handle, &cmd_buff);
			break;

		case VFS_CMD_READDIR:
			vfs_readdir_handler(handle, &cmd_buff);
			break;

		case VFS_CMD_CLOSEDIR:
			vfs_closedir_handler(handle, &cmd_buff);
			break;

		case VFS_CMD_MKDIR:
			vfs_mkdir_handler(handle, &cmd_buff);
			break;

		case VFS_CMD_FTRUNCATE:
			vfs_ftruncate_handler(handle, &cmd_buff);

		default:
			vfs_error_handler(handle, user_cmd);
			TRACE_E(TAG, "recv unknown user_cmd=%d\r\n", user_cmd);
			break;
	}

	return;
}

/* ========================================================
 *
 *        VFS-SERVER
 *
 ==========================================================*/

static u32 vfs_svr_rx_callback(u32 handle, u32 connect_id)
{
	u32  connect_flag;

	if(connect_id >= VFS_SVR_CONNECT_MAX)
		return 0;

	connect_flag = 0x01 << connect_id;

	rtos_set_event_ex(&vfs_svr_event, connect_flag);

	return 0;	
}

static void vfs_svr_connect_handler(u32 handle, u8 connect_id)
{
	u32  cmd_id;

	int ret_val = mb_ipc_get_recv_event(handle, &cmd_id);
	
	if(ret_val != 0)  // failed
	{
		TRACE_E(TAG, "get evt fail %x %d.\r\n", handle, ret_val);
		return;
	}

	if(cmd_id > MB_IPC_CMD_MAX)
	{
		TRACE_E(TAG, "cmd-id error %d.\r\n", cmd_id);
		return;
	}
	
	u8  src =0, dst = 0;

	extern int mb_ipc_get_connection(u32 handle, u8 *src, u8 * dst);

	mb_ipc_get_connection(handle, &src, &dst);

	if(cmd_id == MB_IPC_SEND_CMD)
	{
		vfs_cmd_handler(handle, connect_id);
	}
	#if 0
	else if(cmd_id == MB_IPC_DISCONNECT_CMD)
	{
		TRACE_I(TAG, "disconnect 0x%x, %x-%x.\r\n", handle, src, dst);
	}
	else if(cmd_id == MB_IPC_CONNECT_CMD)
	{
		TRACE_I(TAG, "connect 0x%x, %x-%x.\r\n", handle, src, dst);
	}
	#endif
	else  /* any other commands. */
	{
		TRACE_I(TAG, "cmd=%d, 0x%x, %x-%x.\r\n", cmd_id, handle, src, dst);
	}

}

static void vfs_server_task(void * param)
{
	u32  handle;
	u32  connect_handle;
	
	if(rtos_init_event_ex(&vfs_svr_event) != BK_OK)
	{
		rtos_delete_thread(NULL);
		return;
	}

	handle = mb_ipc_socket(IPC_GET_ID_PORT(VFS_SERVER), vfs_svr_rx_callback);

	if(handle == 0)
	{
		TRACE_E("ipc_svr", "create_socket failed.\r\n");

		rtos_deinit_event_ex(&vfs_svr_event);
		rtos_delete_thread(NULL);
		
		return ;
	}

	s_vfs_svr_init = 2;

	while(1)
	{
		u32 event = rtos_wait_event_ex(&vfs_svr_event, VFS_SVR_EVENTS, 1, BEKEN_WAIT_FOREVER);

		if(event == 0)  // timeout.
		{
			continue;
		}
		
		if(event & VFS_SVR_QUIT_EVENT)
		{
			break;
		}

		for(int i = 0; i < VFS_SVR_CONNECT_MAX; i++)
		{
			if(event & (0x01 << i))
			{
				connect_handle = mb_ipc_server_get_connect_handle(handle, i);
				vfs_svr_connect_handler(connect_handle, i);
			}
		}
	}

	mb_ipc_server_close(handle, VFS_SVR_WAIT_TIME);
	
	rtos_deinit_event_ex(&vfs_svr_event);

	s_vfs_svr_init = 0;
	
	rtos_delete_thread(NULL);
}

bk_err_t bk_vfs_svr_init(void)
{
	if(s_vfs_svr_init)
		return BK_OK;

	BK_LOGE(TAG, "enter %s @%d.\r\n", __FUNCTION__, __LINE__);

	s_vfs_svr_init = 1;
	int ret_val;

#if CONFIG_SYS_CPU0 && CONFIG_PSRAM_AS_SYS_MEMORY
	ret_val = rtos_create_psram_thread(NULL, VFS_SVR_PRIORITY, "vfs_svr",
					vfs_server_task, VFS_SVR_STACK_SIZE, NULL);
#else
	ret_val = rtos_create_thread(NULL, VFS_SVR_PRIORITY, "vfs_svr", 
					vfs_server_task, VFS_SVR_STACK_SIZE, NULL);
#endif

	return ret_val;
}

bk_err_t bk_vfs_svr_deinit(void)
{
	if(s_vfs_svr_init != 2)
		return BK_OK;

	s_vfs_svr_init = 1;

	rtos_set_event_ex(&vfs_svr_event, VFS_SVR_QUIT_EVENT);

	return BK_OK;
}

