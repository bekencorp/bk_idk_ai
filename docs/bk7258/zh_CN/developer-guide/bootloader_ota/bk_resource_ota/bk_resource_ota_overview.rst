资源升级概述
---------------------------------

:link_to_translation:`en:[English]`

概述
------------------

目前，SDK中提供了新的API接口支持固件升级和资源升级。例如：可以通过升级资源文件，并存到SD_Nand、TF卡中。需要注意点如下：


为了兼容以前的软件接口，下载固件有两套接口　：

  - 1、int bk_http_ota_download(const char \*uri)
  
  - 2、int bk_ota_start_download(const char \*url,  ota_wr_destination_t ota_dest_id)
  
.. note::

    - 其中：接口1 只能用于实现固件升级到flash中，不支持资源升级；
  
    - 接口2即可以实现固件升级也可以实现资源升级，根据参数2来决定；

        - 当ota_dest_id =0时，代表下载的固件存到flash;
        - 当ota_dest_id =1时，代表下载的资源文件，存到SD_Nand中；
        - 禁止同时升级固件和资源文件的行为；

其中，对于下载资源文件到SD_Nand中，需注意：即存取到文件系统中的路径是根据url来决定的，比如url:http://192.168.32.177/D%3A/upgrade/app_pack6666.rbl,那么，存取path是app_pack6666；
