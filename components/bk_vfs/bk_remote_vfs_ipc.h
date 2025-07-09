#ifndef _VFS_IPC_H_
#define _VFS_IPC_H_

#include <common/bk_include.h>

enum
{
	VFS_CMD_MOUNT = 0,
	VFS_CMD_OPEN,
	VFS_CMD_READ,
	VFS_CMD_READ_DONE,
	VFS_CMD_WRITE,
	VFS_CMD_CLOSE,
	VFS_CMD_UMOUNT,
	VFS_CMD_STAT,
	VFS_CMD_OPENDIR,
	VFS_CMD_READDIR,
	VFS_CMD_CLOSEDIR,
	VFS_CMD_MKDIR,
	VFS_CMD_FTRUNCATE,
} ;


//const char *path, int oflag
//int fd, void *buf, size_t count

typedef struct
{
	void    *path;
	int     oflag;
	int     fd;
	void    * buff;
	u32     count;
	int     ret_status;
	u32     crc;
} vfs_cmd_t;

#define VFS_IPC_READ_SIZE     0x200
#define VFS_IPC_WRITE_SIZE    0x200

#endif //_VFS_IPC_H_
// eof

