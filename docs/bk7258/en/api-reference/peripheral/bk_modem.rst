Modem Driver
================

:link_to_translation:`zh_CN:[中文]`


Modem driver API
------------------

+---------------------------------------------+---------+
| API                                         | BK7258  |
+=============================================+=========+
| :cpp:func:`bk_modem_init`                   | Y       |
+---------------------------------------------+---------+
| :cpp:func:`bk_modem_deinit`                 | Y       |
+---------------------------------------------+---------+


Basic Usage
------------------

The BK7258 AIDK support connection and use with 4G modules. 

The AIDK also supports 4G network configuration via an APP, and utilizes AI-related functions. For more details, please refer to 

https://docs.bekencorp.com/arminodoc/bk_aidk/bk7258/zh_CN/v2.0.1/projects/beken_genie/index.html#id23


Test Example
------------------

1. After calling bk_modem_init(), the BK7258 will establish a connection with the 4G module. Successful log output is as follows:

.. figure:: ../../../../common/_static/bk_modem_init.png
    :align: center
    :alt: modem driver init 
    :figclass: align-center


2、After calling bk_modem_init(), the BK7258 will broken a connection with the 4G module. Log output is as follows:

.. figure:: ../../../../common/_static/bk_modem_deinit.png
    :align: center
    :alt: modem driver deinit
    :figclass: align-center


Note: Please ensure the BK7258 and 4G module hardware are connected correctly. Refer to :ref:`Modem Griver Usage Guide <Modem Griver Usage Guide>`



USB API Reference
---------------------

.. include:: ../../_build/inc/modem_driver.inc



