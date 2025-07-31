安全数字输入/输出接口
==========================

:link_to_translation:`en:[English]`

1 功能概述
-------------------------------------
	  SDIO既支持作为Host控制TF卡，又可以作为slave无线网卡收发数据。作为Host时，SDK工程中使用fatfs文件系统对TF卡进行文件管理，用户只需要进行简单的配置就可以使用SD卡，本文档对操作fatfs文件系统的cli命令进行介绍。

2 代码路径
-------------------------------------
    - demo路径：
        | ``components\bk_cli\cli_fatfs.c``
        | ``components\fatfs\test_fatfs.c``
    - 驱动源码路径：
        | ``middleware\driver\sd_card\sd_card_driver.c``
        | ``middleware\driver\sdio_host\sdio_host_driver.c``

3 SDIO相关宏配置
-------------------------------------
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|                 NAME                 |      Description                                          |                  File                      |  value  |
	+======================================+===========================================================+============================================+=========+
	|CONFIG_FATFS                          | FATFS使能配置                                             | ``middleware\soc\bk7258\bk7258.defconfig`` |    y    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_FATFS_SDCARD                   | FATFS使能SD卡存储方式配置                                 | ``middleware\soc\bk7258\bk7258.defconfig`` |    y    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDCARD                         | SD卡使能配置                                              | ``middleware\soc\bk7258\bk7258.defconfig`` |    y    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDCARD_DEFAULT_CLOCK_FREQ      | SD卡读写数据时钟配置sdio_host_clock_freq_t,14对应20M,     | ``middleware\soc\bk7258\bk7258.defconfig`` |    14   |
	|                                      | 时钟数值可根据需求进行调整,最大支持40MHz                  |                                            |         |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDCARD_POWER_GPIO_CTRL         | SD卡使能LDO低功耗投票配置                                 | ``middleware\soc\bk7258\bk7258.defconfig`` |    y    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDCARD_DEBUG_SUPPORT           | SD卡使能DEBUG dump寄存器配置                              | ``middleware\soc\bk7258\bk7258.defconfig`` |    n    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDCARD_BUSWIDTH_4LINE          | SD卡使能四线配置                                          | ``middleware\soc\bk7258\bk7258.defconfig`` |    n    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDIO_4LINES_EN                 | SDIO使能四线配置                                          | ``middleware\soc\bk7258\bk7258.defconfig`` |    n    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDIO                           | SDIO使能配置                                              | ``middleware\soc\bk7258\bk7258.defconfig`` |    n    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDIO_V1P0                      | SDIO使能V1P0版本配置                                      | ``middleware\soc\bk7258\bk7258.defconfig`` |    n    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDIO_V2P0                      | SDIO使能V2P0版本配置,通常建议Host使用v2p0                 | ``middleware\soc\bk7258\bk7258.defconfig`` |    y    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDIO_V2P1                      | SDIO使能V2P1版本配置,通常建议slave使用v2p1                | ``middleware\soc\bk7258\bk7258.defconfig`` |    n    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDIO_GDMA_EN                   | SDIO使能DMA配置,建议slave使能,Host关闭                    | ``middleware\soc\bk7258\bk7258.defconfig`` |    n    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDIO_HOST                      | SDIO使能Host配置                                          | ``middleware\soc\bk7258\bk7258.defconfig`` |    y    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDIO_HOST_DEFAULT_CLOCK_FREQ   | SD卡初始化时钟配置sdio_host_clock_freq_t，7对应100K       | ``middleware\soc\bk7258\bk7258.defconfig`` |    7    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDIO_HOST_CLR_WRITE_INT        | 无效宏配置，老项目用                                      | ``middleware\soc\bk7258\bk7258.defconfig`` |    n    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDIO_PM_CB_SUPPORT             | SDIO Host使能低功耗投票配置                               | ``middleware\soc\bk7258\bk7258.defconfig`` |    y    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_PIN_SDIO_GROUP_1               | SDIO Host使能PIN脚组合1配置，即P14~P19                    | ``middleware\soc\bk7258\bk7258.defconfig`` |    n    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDIO_SLAVE                     | SDIO使能Slave配置                                         | ``middleware\soc\bk7258\bk7258.defconfig`` |    n    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDIO_CHANNEL_EN                | SDIO Slave使能通道管理配置                                | ``middleware\soc\bk7258\bk7258.defconfig`` |    n    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDIO_BIDIRECT_CHANNEL_EN       | SDIO Slave使能双向收发配置                                | ``middleware\soc\bk7258\bk7258.defconfig`` |    n    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_GPIO_NOTIFY_TRANSACTION_EN     | SDIO Slave使能GPIO中断配置                                | ``middleware\soc\bk7258\bk7258.defconfig`` |    n    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDIO_TEST_EN                   | SDIO Slave使能测试用例                                    | ``middleware\soc\bk7258\bk7258.defconfig`` |    n    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_SDIO_DEBUG_EN                  | SDIO Slave使能DEBUG配置                                   | ``middleware\soc\bk7258\bk7258.defconfig`` |    n    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_GPIO_1V8_EN                    | GPIO 切换至1.8V配置                                       | ``middleware\soc\bk7258\bk7258.defconfig`` |    n    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_GPIO_1V8_SRC_AUTO_SWITCH_EN    | GPIO 1.8V低功耗相关配置                                   | ``middleware\soc\bk7258\bk7258.defconfig`` |    n    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+
	|CONFIG_GPIO_DEFAULT_SET_SUPPORT       | GPIO 按照GPIO map表进行初始化                             | ``middleware\soc\bk7258\bk7258.defconfig`` |    n    |
	+--------------------------------------+-----------------------------------------------------------+--------------------------------------------+---------+

-----------------------------------------------------

    - 适配TF卡注意事项：
        a. 作为Host要加外部上拉，上拉电阻通常为10K；
        b. SDIO Host接TF卡时，建议关闭如下两个宏，前者是为了确保时序正确，后者宏已默认禁用 防止TF卡使用时自动下电；
            | CONFIG_SDIO_GDMA_EN
            | CONFIG_SDCARD_POWER_GPIO_CTRL_AUTO_POWERDOWN_WHEN_IDLE

-----------------------------------------------------

    - 作为Host搭载TF卡或SD Nand时宏配置示例：

        | #
        | # SDIO Host config
        | #
        | CONFIG_FATFS=y
        | CONFIG_FATFS_SDCARD=y
        | CONFIG_SDCARD=y
        | CONFIG_SDCARD_DEFAULT_CLOCK_FREQ=14
        | CONFIG_SDCARD_POWER_GPIO_CTRL=y
        | CONFIG_SDCARD_DEBUG_SUPPORT=n
        | CONFIG_SDCARD_BUSWIDTH_4LINE=n
        | CONFIG_SDIO_4LINES_EN=n
        | CONFIG_SDIO=y
        | CONFIG_SDIO_V1P0=n
        | CONFIG_SDIO_V2P0=y
        | CONFIG_SDIO_V2P1=n
        | CONFIG_SDIO_GDMA_EN=n
        | CONFIG_SDIO_HOST=y
        | CONFIG_SDIO_HOST_DEFAULT_CLOCK_FREQ=7
        | CONFIG_SDIO_HOST_CLR_WRITE_INT=y
        | CONFIG_SDIO_PM_CB_SUPPORT=y
        | CONFIG_PIN_SDIO_GROUP_1=n
        | CONFIG_SDIO_SLAVE=n
        | # end of SDIO

-----------------------------------------------------

    - 作为Slave功能宏配置示例：
        | #
        | # SDIO slave config
        | #
        | CONFIG_FATFS=n
        | CONFIG_FATFS_SDCARD=n
        | CONFIG_SDCARD=n
        | CONFIG_GPIO_DEFAULT_SET_SUPPORT=y
        | CONFIG_SDIO=y
        | CONFIG_SDIO_V1P0=n
        | CONFIG_SDIO_V2P0=n
        | CONFIG_SDIO_V2P1=y
        | CONFIG_SDIO_HOST=n
        | CONFIG_SDIO_SLAVE=y
        | CONFIG_SDIO_CHANNEL_EN=y
        | CONFIG_SDIO_BIDIRECT_CHANNEL_EN=y
        | CONFIG_SDIO_GDMA_EN=y
        | CONFIG_GPIO_NOTIFY_TRANSACTION_EN=y
        | CONFIG_SDIO_4LINES_EN=n
        | CONFIG_SDIO_TEST_EN=y
        | CONFIG_SDIO_DEBUG_EN=y
        | CONFIG_GPIO_1V8_EN=n
        | CONFIG_GPIO_1V8_SRC_AUTO_SWITCH_EN=n
        | # end of SDIO

-----------------------------------------------------

4	SDIO相关PIN脚描述
-------------------------------------
    - SDIO对应两个PIN脚组合，用户可以根据硬件分布情况，使用其中一组：

	+-----------+---------+---------+
	| PIN功能   | 组0     | 组1     |
	+===========+=========+=========+
	| CLK       | GPIO2   | GPIO14  |
	+-----------+---------+---------+
	| CMD       | GPIO3   | GPIO15  |
	+-----------+---------+---------+
	| Data0     | GPIO4   | GPIO16  |
	+-----------+---------+---------+
	| Data1     | GPIO5   | GPIO17  |
	+-----------+---------+---------+
	| Data2     | GPIO10  | GPIO18  |
	+-----------+---------+---------+
	| Data3     | GPIO11  | GPIO19  |
	+-----------+---------+---------+


5 cli命令简介
-------------------------------------
	SDK工程中已经移植好fatfs文件系统，fatfs相关API介绍见：``http://elm-chan.org/fsw/ff/00index_e.html``

	demo支持的cli命令如下表：

	+----------------------------------------+------------------------------------------------+----------------------------------------+
	|             Command                    |            Param                               |              Description               |
	+========================================+================================================+========================================+
	|                                        | {M|U}: mount|unmount                           |                                        |
	|  fatfstest {M|U} {DISK_NUMBER}         +------------------------------------------------+  apply for or release the workspace    |
	|                                        | {DISK_NUMBER}:logical driver number            |  for logical drivers                   |
	+----------------------------------------+------------------------------------------------+----------------------------------------+
	| fatfstest G {DISK_NUMBER}              | {DISK_NUMBER}:logical driver number            | get the size of remaining disk space   |
	+----------------------------------------+------------------------------------------------+----------------------------------------+
	| fatfstest S {DISK_NUMBER}              | {DISK_NUMBER}:logical driver number            | scan all files on disk                 |
	+----------------------------------------+------------------------------------------------+----------------------------------------+
	| fatfstest F {DISK_NUMBER}              | {DISK_NUMBER}:logical driver number            | format disk                            |
	+----------------------------------------+------------------------------------------------+----------------------------------------+
	|                                        | {DISK_NUMBER}:logical driver number            |                                        |
	| fatfstest R {DISK_NUMBER}{file_name}   +------------------------------------------------+                                        |
	|                                        | {file_name}:file to story read data            | read specified length of data          |
	| {length}                               +------------------------------------------------+                                        |
	|                                        | {length}: length to be read                    | from the file                          |
	+----------------------------------------+------------------------------------------------+----------------------------------------+
	|                                        | {DISK_NUMBER}:logical driver number            |                                        |
	| fatfstest W {DISK_NUMBER}{file_name}   +------------------------------------------------+                                        |
	|                                        | {file_name}:file to be written                 | write data to a file                   |
	| {content_p}{content_len}               +------------------------------------------------+                                        |
	|                                        | {content_p}: pointer to the data to be written |                                        |
	|                                        +------------------------------------------------+                                        |
	|                                        | {content_len}:length to be written             |                                        |
	+----------------------------------------+------------------------------------------------+----------------------------------------+
	|                                        | {DISK_NUMBER}:logical driver number            |                                        |
	| fatfstest D {DISK_NUMBER}{file_name}   +------------------------------------------------+                                        |
	|                                        | {file_name}:file to be written                 | read the specified length of data from |
	| {start_addr}{content_len}              +------------------------------------------------+ the specified address and write it     |
	|                                        | {start_addr}: start address for reading        | to the specified file                  |
	|                                        +------------------------------------------------+                                        |
	|                                        | {content_len}:length to be written             |                                        |
	+----------------------------------------+------------------------------------------------+----------------------------------------+
	|                                        | {DISK_NUMBER}:logical driver number            |  auto test,write the data to the file  |
	| fatfstest A {DISK_NUMBER}{file_name}   +------------------------------------------------+  and then read it, and compare the     |
	|                                        | {file_name}:file to be written                 |  result                                |
	| {content_len}{test_cnt} {start_addr}   +------------------------------------------------+                                        |
	|                                        | {content_len}: length of comparison            | note: the data written to the SD card  |
	|                                        +------------------------------------------------+ is read from the specified start_addr  |
	|                                        | {test_cnt}:number of cycle tests               |                                        |
	|                                        +------------------------------------------------+                                        |
	|                                        | {start_addr}:start address for reading         |                                        |
	+----------------------------------------+------------------------------------------------+----------------------------------------+

	disk_number的定义：

   ::

		typedef enum
	{
	    DISK_NUMBER_RAM  = 0,
	    DISK_NUMBER_SDIO_SD = 1,
	    DISK_NUMBER_UDISK   = 2,
	    DISK_NUMBER_FLASH   = 3,
	    DISK_NUMBER_COUNT,
	} DISK_NUMBER;



6 演示介绍
-------------------------------------
	demo执行的步骤如下：

	1、将SD卡插入开发板，GPIO 连接方式如下（因为GPIO复用，此demo SDIO配置为单线模式）：

	::

		SD_CLK----GPIO2
		SD_CMD----GPIO3
		SD_D0-----GPIO4
		SD_D1-----GPIO5
		SD_D2-----GPIO10
		SD_D3-----GPIO11

	2、SD卡操作

    以下命令是基于CP的，AP侧可在命令前添加“cpu1”字段。

	- **fatfstest M 1**    //挂载SD卡

   ::

	[16:06:10.103]发→◇fatfstest M 1
	[16:06:10.108]收←◆fatfstest M 1
	error file name,use defaultfilename.txt
	sd_card:I(203942):sd card has inited
	fmt=2
	fmt2=0
	Fatfs:I(203944):f_mount OK!
	Fatfs:I(203944):----- test_mount 1 over  -----


	- **fatfstest S 1**   //扫描SD卡

   ::

		[16:11:39.041]发→◇fatfstest S 1
		[16:11:39.046]收←◆fatfstest S 1
		error file name,use defaultfilename.txt
		Fatfs:I(532878):
		----- scan_file_system 1 start -----
		Fatfs:I(532878):1:/
		Fatfs:I(532880):1:/autotest_400.txt
		Fatfs:I(532882):1:/dump_1.txt
		Fatfs:I(532884):scan_files OK!
		Fatfs:I(532886):----- scan_file_system 1 over  -----


	- **fatfstest W 1 test.txt acl_bk7236_write_to_test 24**   //向文件test.txt中写入字符串"acl_bk7236_write_to_test"

   ::

		[16:15:02.687]发→◇fatfstest W 1 test.txt acl_bk7236_write_to_test 24

		[16:15:02.696]收←◆fatfstest W 1 test.txt acl_bkFatfs:I(736530):
		----- test_fatfs 1 start -----
		Fatfs:I(736530):f_open "1:/test.txt"
		Fatfs:I(736530):.7236_write_to_test 24

		[16:15:02.837]收←◆TODO:FATFS sync feature
		Fatfs:I(736678):f_close OK
		Fatfs:I(736678):----- test_fatfs 1 over  -----

		append and write:test.txt
		[16:15:02.866]收←◆,acl_bk7236_write_to_test


	- **fatfstest R 1 test.txt 32**     //从文件test.txt中读取32字节数据

   ::

	[16:18:30.473]发→◇fatfstest R 1 test.txt 32
	[16:18:30.478]收←◆fatfstest R 1 test.txt 32
	Fatfs:I(944312):
	----- test_fatfs 1 start -----
	Fatfs:I(944312):f_open "1:/test.txt"
	Fatfs:I(944314):will read left_len = 24
	Fatfs:I(944314):f_read start:24 bytes

	====== f_read one cycle - dump(len=24) begin ======
	61 63 6c 5f 62 6b 37 32 35 36 5f 77 72 69 74 65
	5f 74 6f 5f 74 65 73 74
	====== f_read one cycle - dump(len=24)   end ======

	Fatfs:I(944328):f_read one cycle finish:left_len = 0
	Fatfs:I(944332):f_read: read total byte = 24
	Fatfs:I(944336):f_close OK
	Fatfs:I(944338):----- test_fatfs 1 over  -----

	read test.txt, len_h = 0, len_l = 32

	- **fatfstest A 1 autotest.txt 2222 1 0**   //从flash 0x0 地址读取2222字节数据保存到SD卡autotest.txt中，再将数据从autotest.txt中读取出来进行比较；此操作进行1次

   ::

	[16:31:11.077]发→◇fatfstest A 1 autotest.txt 2222 1 0
	[16:31:11.083]收←◆fatfstest A 1 autotest.txt 2222 1 0

	[16:31:11.143]收←◆TODO:FATFS sync feature
	Fatfs:I(195362):auto test succ

	- **fatfstest D 1 dump.txt 671285248 10240**    //从0x0地址读取10240字节数据保存到文件dump.txt中

   ::

	[16:33:15.934]发→◇fatfstest D 1 dump.txt 671285248 10240
	[16:33:15.939]收←◆fatfstest D 1 dump.txt 671285248 10240
	Fatfs:I(320154):
	----- test_fatfs_dump 1 start -----
	Fatfs:I(320154):file_name=dump.txt,start_addr=0x28030000‬,len=10240
	Fatfs:I(320154):f_open start "1:/dump.txt"
	Fatfs:I(320154):f_write start
	Fatfs:I(320158):f_write end len = 10240
	Fatfs:I(320158):f_lseek start
	Fatfs:I(320158):f_close start

	- **fatfstest F 1**   //对SD卡进行格式化

   ::

	[17:43:49.985]发→◇fatfstest F 1
	[17:43:49.990]收←◆fatfstest F 1
	error file name,use defaultfilename.txt
	sd_card:I(327564):sd card has inited
	part=0
	sd_card:I(327564):card ver=2.0,size:0x01dacc00 sector(sector=512bytes)
	sdcard sector cnt=31116288
	Fatfs:I(333054):f_mkfs OK!
	format :1

