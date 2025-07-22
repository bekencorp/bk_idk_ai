#include "cli.h"

#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <cli.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "bk_posix.h"

#include "driver/flash_partition.h"

#if defined(CONFIG_OVERRIDE_FLASH_PARTITION) && defined(BK_PARTITION_LITTLEFS_USER)
#include "vendor_flash_partition.h"
#define BK_PARTITION_FS_ID BK_PARTITION_LITTLEFS_USER
#else
#define BK_PARTITION_FS_ID BK_PARTITION_USR_CONFIG
#endif

extern const bk_logic_partition_t bk_flash_partitions[BK_PARTITION_MAX];

#define FLASH_START   bk_flash_partitions[BK_PARTITION_FS_ID].partition_start_addr
#define FLASH_SIZE    bk_flash_partitions[BK_PARTITION_FS_ID].partition_length

static int test_format_lfs(void) {
	struct bk_little_fs_partition partition;
	char *fs_name = NULL;

	int ret;

	fs_name = "littlefs";
	partition.part_type = LFS_FLASH;
	partition.part_flash.start_addr = FLASH_START;
	partition.part_flash.size = FLASH_SIZE;

	ret = mkfs("PART_NONE", fs_name, &partition);
	
	return ret;
}

static int test_mount_lfs(char *mount_point) {
	struct bk_little_fs_partition partition;
	char *fs_name = NULL;

	int ret;

	fs_name = "littlefs";
	partition.part_type = LFS_FLASH;
	partition.part_flash.start_addr = FLASH_START;
	partition.part_flash.size = FLASH_SIZE;
	partition.mount_path = mount_point;

	ret = mount("SOURCE_NONE", partition.mount_path, fs_name, 0, &partition);

	return ret;
}

#define spi_flash_start 0x1000
#define spi_flash_size	0x100000		// 1 MB

static int test_format_spi_lfs(void) {
	struct bk_little_fs_partition partition;
	char *fs_name = NULL;

	int ret;

	fs_name = "littlefs";
	partition.part_type = LFS_SPI_FLASH;
	partition.part_flash.start_addr = spi_flash_start;
	partition.part_flash.size = spi_flash_size;

	ret = mkfs("PART_NONE", fs_name, &partition);
	
	return ret;
}

static int test_mount_spi_lfs(char *mount_point) {
	struct bk_little_fs_partition partition;
	char *fs_name = NULL;

	int ret;

	fs_name = "littlefs";
	partition.part_type = LFS_SPI_FLASH;
	partition.part_flash.start_addr = spi_flash_start;
	partition.part_flash.size = spi_flash_size;
	partition.mount_path = mount_point;

	ret = mount("SOURCE_NONE", partition.mount_path, fs_name, 0, &partition);

	return ret;
}

static int test_format_qspi_lfs(void) {
	struct bk_little_fs_partition partition;
	char *fs_name = NULL;

	int ret;

	fs_name = "littlefs";
	partition.part_type = LFS_QSPI_FLASH;
	partition.part_flash.start_addr = 0;
	partition.part_flash.size = 0x400000;

	ret = mkfs("PART_NONE", fs_name, &partition);

	return ret;
}

static int test_mount_qspi_lfs(char *mount_point) {
	struct bk_little_fs_partition partition;
	char *fs_name = NULL;

	int ret;

	fs_name = "littlefs";
	partition.part_type = LFS_QSPI_FLASH;
	partition.part_flash.start_addr = 0;
	partition.part_flash.size = 0x400000;
	partition.mount_path = mount_point;

	ret = mount("SOURCE_NONE", partition.mount_path, fs_name, 0, &partition);

	return ret;
}

static int test_format_fatfs(void) {
	struct bk_fatfs_partition partition;
	char *fs_name = NULL;

	int ret;

	fs_name = "fatfs";
	partition.part_type = FATFS_DEVICE;
	partition.part_dev.device_name = FATFS_DEV_SDCARD;

	ret = mkfs("PART_NONE", fs_name, &partition);
	
	return ret;
}

static int test_mount_fatfs(char *mount_point) {
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

static int test_umount_vfs(char *mount_point) {
	int ret;
	
	ret = umount(mount_point);
	return ret;
}

static int show_file(int fd) {
	#define CHUNK 18
	char buffer[CHUNK+1];
	int len;
	int total = 0;
	char *ptr;
	char *ptr2;

	buffer[CHUNK] = '\0';
	while(1) {
		len = read(fd, buffer, CHUNK);
		if (len <= 0)
			break;

		if (len == 0)
			break;

		total += len;

		ptr = buffer;
		while(ptr < buffer + len) {
			ptr2 = strchr(ptr, '\0');
			if (!ptr2)	//impossible
				break;
			if (ptr2 < buffer + len) {
				os_printf("%s\n", ptr);
			} else {
				os_printf("%s", ptr);
			}
			ptr = ptr2 + 1;
		}
	}

	return total;
	#undef CHUNK
}

static int test_read_vfs(char *file_name)
{
	int fd;
	int ret;

	fd = open(file_name, O_RDONLY);
	if (fd < 0) {
		os_printf("can't open %s\n", file_name);
		return -1;
	}
	
	ret = show_file(fd);
	//os_printf("read from %s, ret=%d\n", file_name, ret);
	close(fd);
	return ret;
}

static int test_write_vfs(char *file_name, char *content)
{
	int fd;
	int ret;

	fd = open(file_name, O_RDWR | O_CREAT | O_APPEND);
	if (fd < 0) {
		os_printf("can't open %s\n", file_name);
		return -1;
	}
	
	ret = write(fd, content, strlen(content) + 1);
	os_printf("write to %s, ret=%d\n", file_name, ret);

	close(fd);
	return ret;
}

static int test_unlink_vfs(char *file_name)
{
	int ret;

	ret = unlink(file_name);
	return ret;
}

static int vfs_scan_files(char *path)
{
    int ret = BK_OK;
    DIR *dir;
    struct dirent *dir_info;

    dir = opendir(path);                 /* Open the directory */
    if (dir != NULL)
    {
        os_printf("%s/\r\n", (path+1));
        while (1)
        {
            dir_info = readdir(dir);         /* Read a directory item */
            if (dir_info == NULL)
            {
                break;  /* Break on error */
            }
            if (dir_info->d_name[0] == 0)
            {
                break;  /* Break on end of dir */
            }
            if (dir_info->d_type == DT_DIR)
            {
                /* It is a directory */
                char *pathTemp = os_malloc(strlen(path)+strlen(dir_info->d_name)+2);
                if(pathTemp == NULL)
                {
                    os_printf("%s:os_malloc dir fail \r\n", __func__);
                    ret = BK_FAIL;
                    break;
                }
                sprintf(pathTemp, "%s/%s", path, dir_info->d_name);
                ret = vfs_scan_files(pathTemp);      /* Enter the directory */
                if (ret != BK_OK)
                {
                    os_free(pathTemp);
                    pathTemp = 0;
                    break;
                }
                if(pathTemp)
                {
                    os_free(pathTemp);
                    pathTemp = 0;
                }
            }
            else
            {
                /* It is a file. */
                os_printf("%s/%s\r\n", (path+1), dir_info->d_name);
            }
        }
        closedir(dir);
    }
    else
    {
        os_printf("opendir %s failed\r\n", path);
        ret = BK_FAIL;
    }

    return ret;
}

static int test_scan_vfs(char *dir_name)
{
	int ret;
	ret = vfs_scan_files(dir_name);
	return ret;
}

static int test_stat_vfs(const char *pathname)
{
	int ret;

	struct stat statbuf;

	ret = stat(pathname, &statbuf);

	os_printf("statbuf->st_size =%d, statbuf->st_mode = %d \r\n", statbuf.st_size , statbuf.st_mode);
	return ret;
}


#define TEST_PERF_FILE_NAME     "/perf_test.bin"
#define VFS_TEST_MAX_FILE_LEN   20

static void test_vfs_perf_with_param(uint32_t TEST_BUF_SIZE, uint32_t TEST_ITERATIONS) {
    int fd;        // File object
    int res;
    uint32_t start_time, end_time;
    uint8_t *buffer= os_malloc(TEST_BUF_SIZE);
    float f_perf_speed = 0;
    uint32_t total_time = 0;

    char cFileName[VFS_TEST_MAX_FILE_LEN];
    sprintf(cFileName, "%s", TEST_PERF_FILE_NAME);
    CLI_LOGI("open \"%s\"\r\n", cFileName);

    if(buffer == NULL){
        CLI_LOGE("%s os_malloc fail, size = %d. \n", __func__, TEST_BUF_SIZE);
        return;
    }

    // Initialize test buffer with pattern
    for(int i=0; i<TEST_BUF_SIZE; i++) {
        buffer[i] = (uint8_t)(i % 256);
    }

    // Test write performance
    start_time = rtos_get_time();
    for(int i=0; i<TEST_ITERATIONS; i++) {
        fd = open(cFileName, O_RDWR | O_CREAT | O_APPEND);
        if(fd < 0) {
            CLI_LOGE("Open for write failed, fd: %d\n", fd);
            break;
        }

        res = write(fd, buffer, TEST_BUF_SIZE);
        if(res != TEST_BUF_SIZE) {
            CLI_LOGE("Write failed, fd: %d, bytes: %d\n", fd, res);
        }

        close(fd);
    }
    end_time = rtos_get_time();
    CLI_LOGI(">>> Write test: %d iterations, %d bytes each, total time: %d ms\n", 
             TEST_ITERATIONS, TEST_BUF_SIZE, end_time - start_time);

    total_time = end_time - start_time;
    f_perf_speed = (TEST_BUF_SIZE * TEST_ITERATIONS) / (total_time / 1000.0f) / 1024;
    if(f_perf_speed < 0) {
        CLI_LOGE("[ERROR] Write test failed\n");
    } else {
        CLI_LOGI("===== Write Throughput: %.2f KB/s======\n", f_perf_speed);
    }


    // Test read performance
    start_time = rtos_get_time();
    for(int i=0; i<TEST_ITERATIONS; i++) {
        fd = open(cFileName, O_RDONLY);
        if(fd < 0) {
            CLI_LOGE("Open for read failed, fd: %d\n", fd);
            break;
        }

        res = read(fd, buffer, TEST_BUF_SIZE);
        if(res != TEST_BUF_SIZE) {
            CLI_LOGE("Read failed, fd: %d, bytes: %d\n", fd, res);
        }

        close(fd);
    }
    end_time = rtos_get_time();
    CLI_LOGI(">>> Read test: %d iterations, %d bytes each, total time: %d ms\n", 
             TEST_ITERATIONS, TEST_BUF_SIZE, end_time - start_time);

    total_time = end_time - start_time;
    f_perf_speed = (TEST_BUF_SIZE * TEST_ITERATIONS) / (total_time / 1000.0f) / 1024;
    if(f_perf_speed < 0) {
        CLI_LOGE("[ERROR] Read test failed\n");
    } else {
        CLI_LOGI("===== Read Throughput: %.2f KB/s======\n", f_perf_speed);
    }
    os_free(buffer);
}

static void test_vfs_performance(void) {
	BK_DUMP_OUT("+++++++++++++++++++++++++\r\n");
	test_vfs_perf_with_param(102400, 100);
	rtos_delay_milliseconds(50);
	BK_DUMP_OUT("-------------------------\r\n\r\n\r\n\r\n");

	BK_DUMP_OUT("+++++++++++++++++++++++++\r\n");
	test_vfs_perf_with_param(10240, 100);
	rtos_delay_milliseconds(50);
	BK_DUMP_OUT("-------------------------\r\n\r\n\r\n\r\n");

	BK_DUMP_OUT("+++++++++++++++++++++++++\r\n");
	test_vfs_perf_with_param(8192, 100);
	rtos_delay_milliseconds(50);
	BK_DUMP_OUT("-------------------------\r\n\r\n\r\n\r\n");

	BK_DUMP_OUT("+++++++++++++++++++++++++\r\n");
	test_vfs_perf_with_param(2048, 100);
	rtos_delay_milliseconds(50);
	BK_DUMP_OUT("-------------------------\r\n\r\n\r\n\r\n");

	BK_DUMP_OUT("+++++++++++++++++++++++++\r\n");
	test_vfs_perf_with_param(1024, 100);
	rtos_delay_milliseconds(50);
	BK_DUMP_OUT("-------------------------\r\n\r\n\r\n\r\n");

	BK_DUMP_OUT("+++++++++++++++++++++++++\r\n");
	test_vfs_perf_with_param(640, 100);
	rtos_delay_milliseconds(50);
	BK_DUMP_OUT("-------------------------\r\n\r\n\r\n\r\n");

	BK_DUMP_OUT("+++++++++++++++++++++++++\r\n");
	test_vfs_perf_with_param(102400, 100);
	rtos_delay_milliseconds(50);
	BK_DUMP_OUT("-------------------------\r\n\r\n\r\n\r\n");
}


void cli_vfs_test(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	int ret;

	if (argc < 2) {
		os_printf("usage : vfs format|mount|umount|read|write|unlink\n");
		return;
	}

	if (os_strcmp(argv[1], "format") == 0) {
		if (argc < 3) {
			os_printf("usage : vfs format lfs|fatfs\n");
			return;
		}
		if (os_strcmp(argv[2], "lfs") == 0)
			ret = test_format_lfs();
		else if (os_strcmp(argv[2], "spi_lfs") == 0)
			ret = test_format_spi_lfs();
		else if (os_strcmp(argv[2], "qspi_lfs") == 0)
			ret = test_format_qspi_lfs();
		else if (os_strcmp(argv[2], "fatfs") == 0)
			ret = test_format_fatfs();
		else {
			os_printf("usage : vfs format lfs|fatfs\n");
			return;
		}
		os_printf("format ret=%d\n", ret);
	} else if (os_strcmp(argv[1], "mount") == 0) {
		char *mount_point;
		
		if (argc < 4) {
			os_printf("usage : vfs mount lfs|fatfs MOUNT_POINT\n");
			return;
		}
		mount_point = argv[3];

		if (os_strcmp(argv[2], "lfs") == 0)
			ret = test_mount_lfs(mount_point);
		else if (os_strcmp(argv[2], "spi_lfs") == 0)
			ret = test_mount_spi_lfs(mount_point);
		else if (os_strcmp(argv[2], "qspi_lfs") == 0)
			ret = test_mount_qspi_lfs(mount_point);
		else if (os_strcmp(argv[2], "fatfs") == 0)
			ret = test_mount_fatfs(mount_point);
		else {
			os_printf("usage : vfs mount lfs|fatfs MOUNT_POINT\n");
			return;
		}
		os_printf("mount ret=%d\n", ret);
	} else if (os_strcmp(argv[1], "umount") == 0) {
		char *mount_point;

		if (argc < 3) {
			os_printf("usage : vfs umount MOUNT_POINT\n");
			return;
		}
		
		mount_point = argv[2];

		ret = test_umount_vfs(mount_point);
		os_printf("umount ret=%d\n", ret);
	} else if (os_strcmp(argv[1], "read") == 0) {
		char *file_name;
		
		if (argc < 3) {
			os_printf("usage : vfs read FULL_FILE_NAME\n");
			return;
		}
		file_name = argv[2];

		ret = test_read_vfs(file_name);
		os_printf("read ret=%d\n", ret);
	} else if (os_strcmp(argv[1], "write") == 0) {
		char *file_name;
		char *content;
		
		if (argc < 4) {
			os_printf("usage : vfs write FULL_FILE_NAME CONTENT\n");
			return;
		}
		file_name = argv[2];
		content = argv[3];

		ret = test_write_vfs(file_name, content);
		os_printf("write ret=%d\n", ret);
	} else if (os_strcmp(argv[1], "unlink") == 0) {
		char *file_name;
		
		if (argc < 3) {
			os_printf("usage : vfs unlink FULL_FILE_NAME\n");
			return;
		}
		file_name = argv[2];

		ret = test_unlink_vfs(file_name);
		os_printf("unlink ret=%d\n", ret);
	} else if (os_strcmp(argv[1], "scan") == 0) {
		char *dir_name;

		if (argc < 3) {
			os_printf("usage : vfs scan FULL_FILE_NAME\n");
			return;
		}
		dir_name = argv[2];

		ret = test_scan_vfs(dir_name);
		os_printf("scan ret=%d\n", ret);
	} else if (os_strcmp(argv[1], "stat") == 0) {
		char *path_name;
		
		if (argc < 3) {
			os_printf("usage : vfs stat FULL_PATH_NAME\n");
			return;
		}
		path_name = argv[2];

		ret = test_stat_vfs(path_name);
		os_printf("stat ret=%d\n", ret);

	} else if (os_strcmp(argv[1], "perf") == 0) {
		os_printf("==== vfs perf enter ====\r\n");
		test_vfs_performance();
		os_printf("==== vfs perf exit ====\r\n");
	} else {
		os_printf("vfs unknown sub cmd %s\n", argv[1]);
	}
}

#define VFS_CMD_CNT (sizeof(vfs_commands) / sizeof(struct cli_command))
static const struct cli_command vfs_commands[] = {
	{"vfs", "vfs format|mount|umount|read|write|unlink", cli_vfs_test},
};

int cli_vfs_init(void)
{
	return cli_register_commands(vfs_commands, VFS_CMD_CNT);
}

