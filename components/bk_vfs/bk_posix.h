#ifndef __POSIX_FILE_H_
#define __POSIX_FILE_H_

#define BK_VFS_COMPAT

#include "bk_vfs.h"
#include "bk_filesystem.h"

#ifdef BK_VFS_COMPAT
#undef open
#define open		bk_vfs_open
#undef close
#define close		bk_vfs_close
#undef read
#define read		bk_vfs_read
#undef write
#define write		bk_vfs_write
#undef lseek
#define lseek		bk_vfs_lseek

#define unlink		bk_vfs_unlink
#define stat(path,buf)	bk_vfs_stat(path,buf)
#define fstat		bk_vfs_fstat
#define rename		bk_vfs_rename

#undef fsync
#define fsync		bk_vfs_fsync
#define ftruncate	bk_vfs_ftruncate
#ifndef fcntl
#define fcntl		bk_vfs_fcntl
#endif

#define mkdir		bk_vfs_mkdir
#define rmdir		bk_vfs_rmdir

#define opendir		bk_vfs_opendir
#define closedir	bk_vfs_closedir
#define readdir		bk_vfs_readdir
#define seekdir		bk_vfs_seekdir
#define telldir		bk_vfs_telldir
#define rewinddir	bk_vfs_rewinddir

#define chdir		bk_vfs_chdir
#define getcwd		bk_vfs_getcwd

#define mount		bk_vfs_mount
#define umount		bk_vfs_umount
#define umount2		bk_vfs_umount2
#define mkfs		bk_vfs_mkfs
#define statfs(path,buf)	bk_vfs_statfs(path,buf)
#if CONFIG_STARBURST_AIDIALOG_SDK
#define ftell		bk_vfs_ftell
#undef feof
#define feof		bk_vfs_feof
#endif

#endif /* BK_VFS_COMPAT */

#endif
