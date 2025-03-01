Quick Start Guide
==============================================

:link_to_translation:`zh_CN:[中文]`

This article demonstrates:

 - Code download
 - Environment deployment and compilation
 - How to configure Armino project
 - How to build the project and flash the bin to board

Preparation
--------------------------------------------------------

Hardware：

 - BK7258 Demo board ( :ref:`Introduction to Development Board <bk7258>` ， `Purchase link <https://item.taobao.com/item.htm?spm=a1z10.1-c.w4004-25005897050.4.ffd61d55Y0Kdbv&id=795814346530&skuId=5429467124246&addressId=18607518338>`_ )
 - Serial port burning tool
 - PC

Armino SDK Code download
--------------------------------------------------------------------

We can download Armino from gitlab::

    mkdir -p ~/armino
    cd ~/armino
    git clone https://gitlab.bekencorp.com/armino/bk_idk.git


We also can download Armino from github::

	mkdir -p ~/armino
	cd ~/armino
	git clone https://github.com/bekencorp/bk_idk.git


Then switch to the stable branch Tag node, such as v2.0.1.32::

    git checkout -B your_branch_name v2.0.1.32

.. warning::

    When using git clone to download the code on Windows, there may be issues with symbolic link failure and line ending problems, which can lead to compilation failure. Please solve them as follows:

    - Symbolic link failure issue:

    1. Configure the Git environment variable before downloading the code::
      
        git config --global core.symlinks true

    2. Execute the git clone command with administrator privileges.


    - Line ending issue:

    1.Configure the Git environment variable before downloading the code::

        git config --global core.autocrlf false


.. note::

    The GitHub code is relatively lagging behind the GitLab code. If you want to obtain the latest SDK code, please download it from GitLab. Please contact the your BK7258 project owner to get relevant accounts.


Environment Deployment and Compilation
------------------------------------------------

We provide an environment deployment and compilation solution based on Docker containers, which supports efficient compilation work on Linux, macOS, and Windows systems. With Docker containerization technology, you do not need to manually install various libraries and toolchains required for compilation, which significantly simplifies the deployment and compilation process. This solution is suitable for users who are familiar with the Docker environment and understand its basic usage, helping you quickly deploy and compile the environment.


For users who are not familiar with Docker technology or cannot use the Docker environment due to network restrictions, we also provide a local compilation deployment solution based on script commands. The local deployment solution currently only supports compilation on the Linux system.


.. toctree::
    :maxdepth: 1

        Local Deployment <env-manual>
        Docker Deployment <env-docker>


Configuration project
------------------------------------

We can also use the project configuration file for differentiated configuration::

    Project Profile Override Chip Profile Override Default Configuration
    Example： bk7258/config >> bk7258.defconfig >> KConfig
    + Example of project configuration file：
        projects/app/config/bk7258/config
    + Sample chip configuration file：
        middleware/soc/bk7258/bk7258.defconfig
    + Sample KConfig configuration file：
        middleware/arch/cm33/Kconfig
        components/bk_cli/Kconfig

- Modules Excutes In Which CPUx

    + At present, BK7258 is a three-core AMP system architecture. The software of CPU0, CPU1, and CPU2 is compiled independently, but the SDK is a set, so some functional differences between CPU0, CPU1, and CPU2 need to be distinguished using macros.
    + For example, there is only one copy of the TRNG random number controller. When using multi-core configuration, the application needs to be mutually exclusive in which system (CPU0/CPU1/CPU2) it is executed.
    + The module's function switch CONFIG_TRNG macro is turned off by default. Whichever CPU needs it, it can be turned on in the chip configuration file of that CPU.
      Assuming that CPU0 needs to use TRNG and CPU1 does not need to use it, then CONFIG_TRNG=y in bk7258.defconfig.
    + In software code, use the CONFIG_TRNG macro to isolate calls.


    + Example codes::

         #if CONFIG_TRNG             #Uses module macro to seperate CPUx software whether enable TRNG
         #include "driver/trng.h"
         #endif
         ...
         #if CONFIG_TRNG             #Uses module macro to seperate CPUx software whether enable TRNG
         bk_rand();
         #endif

New project
------------------------------------

The default project is projects/app. For new projects, please refer to projects/app project


Burn Code
------------------------------------

On the Windows platform, Armino currently supports UART burning.
After the app project is compiled, generate all-app.bin in the build/app/bk7258 directory and burn it using this bin file. When burning the security project for the first time, it is necessary to first burn bootloader.bin, and then burn all-app.bin.



Burn through serial port
****************************************

.. note::

    Armino supports UART burning. It is recommended to use the CH340 serial port tool board to download.

Serial port burning tool is shown in the figure below:

.. figure:: ../../_static/download_tool_uart.png
    :align: center
    :alt: Uart
    :figclass: align-center

    UART

Download burning tools (BKFILL.exe)：

	http://dl.bekencorp.com/tools/flash/
	Get the latest version in this directory. Ex：BEKEN_BKFIL_V2.1.6.0_20231123.zip

The snapshot of BKFILL.exe downloading.

.. figure:: ../../_static/download_uart_bk7236_en.png
    :align: center
    :alt: BKFIL GUI
    :figclass: align-center

    BKFIL GUI


Burn the serial port DL_UART0, click ``Download`` to burn the image, and then power down and restart the device after burning.


Serial port Log and Command Line
------------------------------------

Currently the BK7258 use the DL_UART0 as the Log output and Cli input; You can get the supported command list through the help command.
