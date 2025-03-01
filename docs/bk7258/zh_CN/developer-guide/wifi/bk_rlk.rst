Armino Wi-Fi BK-RLK 使用指南
=======================================================

:link_to_translation:`en:[English]`

BK-RLK 概述
-------------------------------------------------------

BK-RLK是一种博通集成公司自定义的无线Wi-Fi通信协议。
BK-RLK通信过程中，应用层直接通过数据链路层进行设备间通信。
BK-RLK具有超低功耗，超低延迟，高带宽，开发更具有灵活性以及携带更轻量级控制系统等优势，可广泛应用于各种支持Wi-Fi通信的智能电子产品中。

BK-RLK 信息说明
-------------------------------------------------------
BK-RLK精简了设备间的通信流程，其通信识别核心从TCP/IP层直接移向数据链路层。为了便于应用层的使用，
BK-RLK MAC层维护对端设备信息链表，具体维护设备信息如下表所示：

+------------------+-------------------------------------------------------------------------+
| param            | Description                                                             |
+==================+=========================================================================+
| MAC Address      | 匹配设备的MAC地址, 长度 6 字节                                          |
+------------------+-------------------------------------------------------------------------+
| Channel          | 匹配设备所在通信信道, 范围 1-14 信道                                    |
+------------------+-------------------------------------------------------------------------+
| Encrypt          | 是否与匹配设备加密通信                                                  |
+------------------+-------------------------------------------------------------------------+
| State            | 应用层用于添加匹配设备连接状态等信息                                    |
+------------------+-------------------------------------------------------------------------+

BK-RLK 使用说明
-------------------------------------------------------
BK-RLK 基本通信流程包括：

 - BK-RLK 初始化
 - BK-RLK 添加配对设备
 - BK-RLK 数据发送与接收
 - BK-RLK 低功耗配置(可选)
 - BK-RLK 其它通信参数配置(可选)

BK-RLK 初始化：

应用层使用BK-RLK需要进行初始化：
 - 调用bk_rlk_init()初始化BK-RLK，注册BK-RLK身份；
 - 调用bk_rlk_register_send_cb()配置发送回调函数接口，用于判断数据在底层是否发送成功；
 - 调用bk_rlk_register_recv_cb()配置接收回调函数接口，用于接收底层接收到的数据；
 - 调用bk_rlk_set_channel()配置BK-RLK通信信道；

BK-RLK 添加配对设备：

BK-RLK 在发送数据前需要使用bk_rlk_add_peer()添加对端设备信息。如果是发送广播帧或多播帧，则将添加的对端设备MAC地址设为 ff:ff:ff:ff:ff:ff 。如果是单播帧请添加实际通信对端设备MAC地址。

BK-RLK 可添加配对设备的数量无限制，数量越多所占用内存越多，建议根据实际情况合理添加配对信息。可以使用bk_rlk_del_peer()接口及时删除不需要的配对信息。本地设备的信道需与配对设备的信道一致才可进行通信，建议在发送数据前检查信道是否一致。

注意：未添加对端设备信息到bk_rlk_add_peer()无法进行通信。

BK-RLK 数据发送与接收：

BK-RLK提供了两种数据发送接口，bk_rlk_send()和bk_rlk_send_ex()。
bk_rlk_send()适用于任何数据包发送需求，bk_rlk_register_send_cb()注册的发送回调函数接口用于反馈bk_rlk_send()发送的数据是否在底层发送成功。
bk_rlk_send_ex()是为应用层提供的特殊需求发送接口，该接口中可以直接配置所需的参数，应用层也可直接注册独立的发送回调函数，其配置参数如下表所示:

+------------------+-------------------------------------------------------------------------+
| param            | Description                                                             |
+==================+=========================================================================+
| data             | 传输数据                                                                |
+------------------+-------------------------------------------------------------------------+
| len              | 传输数据长度                                                            |
+------------------+-------------------------------------------------------------------------+
|                  | 发送回调函数, 用于反馈底层是否发送成功, cb包含两个参数:                 |
| cb               | 1) args, 应用层自定义参数;                                              |
|                  | 2) status, 底层反馈数据是否发送成功的状态标志;                          |
+------------------+-------------------------------------------------------------------------+
| args             | cb标志参数, 用于识别发送的数据, 应用层可以自定义                        |
+------------------+-------------------------------------------------------------------------+
| tx_rate          | 数据传输速率                                                            |
+------------------+-------------------------------------------------------------------------+
| tx_power         | 数据传输使用功率                                                        |
+------------------+-------------------------------------------------------------------------+
| tx_retry_cnt     | 数据传输重传次数                                                        |
+------------------+-------------------------------------------------------------------------+

BK-RLK提供了数据接收接口回调函数 bk_rlk_register_recv_cb()， 应用层使用BK-RLK接收数据之前需要注册接收回调函数。BK-RLK接收接口反馈的参数如下表所示：

+------------------+-------------------------------------------------------------------------+
| param            | Description                                                             |
+==================+=========================================================================+
| data             | 传输数据                                                                |
+------------------+-------------------------------------------------------------------------+
| len              | 传输数据长度                                                            |
+------------------+-------------------------------------------------------------------------+
| src_address      | 接收BK-RLK数据包的源地址 ，长度6个字节                                  |
+------------------+-------------------------------------------------------------------------+
| des_address      | 接收BK-RLK数据包的目的地址，长度6个字节                                 |
+------------------+-------------------------------------------------------------------------+
| rssi             | 接收BK-RLK数据包的rssi值                                                |
+------------------+-------------------------------------------------------------------------+

BK-RLK 低功耗配置：
BK-RLK 可以调用bk_rlk_sleep()启动休眠模式，调用bk_rlk_wakeup()关闭休眠模式。

BK-RLK 其它通信参数配置：
为了便于应用层开发使用，BK-RLK给予了更广阔的数据传输参数配置接口。

 - 调用bk_rlk_set_tx_ac()可以配置数据传输优先级；
 - 调用bk_rlk_set_tx_timeout_ms()可以配置数据传输超时时间；
 - 调用bk_rlk_set_tx_power()可以配置数据传输功率；
 - 调用bk_rlk_set_tx_rate()可以配置数据传输速率；

默认情况下，BK-RLK已经赋予了较为合理的参数配置。如果默认参数不满足开发需求时，应用层可以通过上述接口进行参数调整。

BK-RLK 参考链接
-------------------------------------------------------
  BK-RLK 具体API接口，参考demo示例以及具体操作说明可以参考以下路径：

    `API参考: <../../api-reference/network/bk_wifi.html>`_ 介绍了BK-RLK API接口

    `BK-RLK工程: <../../projects_work/wifi/bk_rlk_media.html>`_ 介绍了BK-RLK相关工程
