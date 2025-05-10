Nfc简介
=================

:link_to_translation:`en:[English]`

一.概述
----------------------------

该文档主要介绍NFC的使用流程，更好分析和解决问题。

二.NFC模块介绍
----------------------------

MFRC522是一款高度集成的读写IC，用于在13.56MHz频率下进行无接触通信。MFRC522读写器支持 ISO/IEC 14443 A/MIFARE 和 NTAG 标准,支持的主机接口有SPI、I2C、UART;目前，我们使用UART协议与MFRC522之间进行通讯。

三.MFRC522的工作流程介绍
----------------------------

总的概括来说，主要分为以下部分:寻卡、防碰撞、选定卡片、验证密码、读取数据。

- 复位应答(寻卡):M1射频卡的通讯协议和通讯波特率是定义好的，当有卡片进入读写器的操作范围时，读写器以特定的协议与它通讯，从而确定该卡是否为M1射频卡，即验证卡片的卡型。

- 防冲突机制:当有多张卡进入读写器操作范围时，防冲突机制会从其中选择一张进行操作，未选中的则处于空闲模式等待下一次选卡，该过程会返回被选卡的序列号。

- 选择卡片:选择被选中的卡的序列号，并同时返回卡的容量代码。

- 三次互相确认:选定要处理的卡片之后，读写器就确定要访问的扇区号，并对该扇区密码进行密码校验，在三次相互认证之后就可以通过加密流进行通讯。(在选择另一扇区时，则必须进行另一扇区密码校验。)

- 读取数据:即MFRC522与M1射频卡进行数据通讯。

三.NFC相关api介绍
----------------------------

MFRC522与M1卡之间数据交互的顺序流程是:1、先进行初始化(配置卡的类型)，2、进行寻卡，3、防碰撞，4、选定卡片，5、密码验证，6、读取数据

- 1 MFRC522的初始化函数
    void bk_nfc_init(void);

- 2 寻卡
    char bk_mfrc522_request(uint8_t reqCode, uint8_t \*pTagType);

    .. note::
        - @param reqCode -[in] 寻卡方式，0x52 寻感应区内所有符合1443A标准的卡，0x26 寻未进入休眠状态的卡
        - @param pTagType -[out] 卡片类型代码
        - @return 状态值，MI OK - 成功;MI_ERR - 失败

    .. important::
        - 卡片类型代码:
        - 0x4400 = Mifare_UltraLight
        - 0x0400 = Mifare_One(S50)
        - 0x0200 = Mifare_One(S70)
        - 0x0800 = Mifare_Pro(X)
        - 0x4403 = Mifare_DESFire

- 3 防冲撞
    char bk_mfrc522_anticoll(uint8_t \*pSnr);

    .. note::
        - param pSnr -[out] 卡片序列号，4字节
        - return 状态值，MI OK - 成功;MI_ERR - 失败

- 4 选定卡片
    char bk_mfrc522_select(uint8_t \*pSnr);

    .. note::
        - param pSnr -[in] 卡片序列号，4字节
        - return 状态值，MI OK - 成功;MI_ERR - 失败

- 5 验证卡片密码
    char bk_mfrc522_authState(uint8_t authMode, uint8_t addr, uint8_t \*pKey, uint8_t \*pSnr);

    .. note::
        - @param authMode -[in] 密码验证模式，0x60 验证A密钥，0x61 验证B密钥
        - @param addr -[in] 块地址
        - @param pKey -[in] 密码
        - @param pSnr -[in] 卡片序列号，4字节
        - @return 状态值，MI OK - 成功;MI_ERR - 失败

- 6 读取M1卡一块数据
    char bk_mfrc522_read(uint8_t addr, uint8_t \*pData);

    .. note::
        - @param addr -[in] 块地址
        - @param pData -[out] 读出的数据，16字节
        - @return 状态值，MI OK - 成功;MI_ERR - 失败*/

- 7 写入M1卡一块数据
    char bk_mfrc522_write(uint8_t addr, uint8_t \*pData);

    .. note::
        - @param addr -[in] 块地址
        - @param pData -[out] 写入的数据，16字节
        - @return 状态值，MI OK - 成功;MI_ERR - 失败

三.NFC测试命令介绍
----------------------------
以工程beken_genie为例来介绍NFC的测试使用流程，首先编译工程beken_genie:make bk7258 PROJECT=beken_genie,编译成功之后，烧录build目录下的all-app.bin;
烧录成功之后，即可测试。NFC卡的测试可细分为两种:

  .. important::
    nfc的功能默认是关闭的，若想开启(以工程beken_genie为例)，请在projects/beken_genie/config/bk7258/config中将宏CONFIG_NFC_ENABLE设置成y。

  -1) 第一种为主动测试:板端有个task会默认定期只进行寻卡操作;
    - 1.1)板端有个task会默认定期进行寻卡操作，(如不想执行该task,可把该行代码注释掉;路径是:projects/beken_genie/main/app_main.c)

    .. figure:: ../../../../common/_static/nfc_task.png
        :align: center
        :alt: nfc_task
        :figclass: align-center

        图1 nfc_task

    - 1.2)若无NFC卡靠近或没有安装天线。会定期打印如下log (nfc request fail);

    .. figure:: ../../../../common/_static/fail.png
        :align: center
        :alt: fail
        :figclass: align-center

        图2 nfc_fail

    -1.3)此时可将NFC卡靠近板端(前提是板端要安装上天线如下图)可完成寻卡操作，会定期打印如下log ("nfc request ok以及nfc post ok");

    .. figure:: ../../../../common/_static/antenna.png
        :align: center
        :alt: antenna
        :figclass: align-center

        图3 天线图

    .. figure:: ../../../../common/_static/success.png
        :align: center
        :alt: success(:
        :figclass: align-center

        图4 success

  -2) 第二种为手动测试:需要手动发送cli命令来进行NFC测试;(路径是:bk_avdk/bk_idk/components/bk_nfc/mfrc522_test.c)
    - 2.1)因为板端有个task会定期在进行寻卡操作，所以需要先delete该task,通过发送以下命令:nfc_cmd_test deinit 

    .. figure:: ../../../../common/_static/deinit.png
        :align: center
        :alt: deinit
        :figclass: align-center

        图5 deinit

    - 2.2) 支持一键cli命令完成nfc所有测试(寻卡、防碰撞、选定卡片、密码验证、读取数据):
    - 测试命令:nfc_test

    .. figure:: ../../../../common/_static/nfc_test.png
        :align: center
        :alt: nfc_test
        :figclass: align-center

        图6 nfc_test

    - 2.3) 也支持分步测试，
    - step1:即cli命令完成nfc初始化:nfc_cmd_test init

    .. figure:: ../../../../common/_static/nfc_init.png
        :align: center
        :alt: nfc_init
        :figclass: align-center

        图7 nfc_init

    - step2:即cli命令完成nfc进行寻卡:nfc_cmd_test request

    .. figure:: ../../../../common/_static/nfc_request.png
        :align: center
        :alt: nfc_request
        :figclass: align-center

        图8 nfc_request

    - step3:即cli命令完成nfc防碰撞:nfc_cmd_test anticoll

    .. figure:: ../../../../common/_static/nfc_anticoll.png
        :align: center
        :alt: anticoll
        :figclass: align-center

        图9 nfc_anticoll

    - step4:即cli命令完成nfc选定卡片:nfc_cmd_test select

    .. figure:: ../../../../common/_static/nfc_select.png
        :align: center
        :alt: nfc_select
        :figclass: align-center

        图10 nfc_select

    - step5:即cli命令完成nfc密码验证:nfc_cmd_test authState

    .. figure:: ../../../../common/_static/nfc_authstate.png
        :align: center
        :alt: nfc_authState
        :figclass: align-center

        图11 nfc_authState


    - step6:即cli命令完成nfc写数据:nfc_write_test write 'a'

    .. figure:: ../../../../common/_static/nfc_write.png
        :align: center
        :alt: nfc_write
        :figclass: align-center

        图12 nfc_write
    
    
    - step7:即cli命令完成nfc读取数据:nfc_write_test read 0

    .. figure:: ../../../../common/_static/nfc_read.png
        :align: center
        :alt: nfc_read
        :figclass: align-center

        图13 nfc_read