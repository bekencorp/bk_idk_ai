Nfc brief introduction
===============================

:link_to_translation:`zh_CN:[中文]`

1.Overview
----------------------------

This document provides an overview of the NFC usage process to better analyze and solve problems.

2.Introduction to the NFC Module
----------------------------------

The MFRC522 is a highly integrated read/write IC designed for contactless communication at 13.56MHz. The MFRC522 reader supports ISO/IEC 14443 A/MIFARE and NTAG standards, with host interfaces including SPI, I2C, and UART. Currently, we use the UART protocol for communication with the MFRC522.

3.Working Process of MFRC522
------------------------------

In general, the process can be summarized into the following parts: card detection, anti-collision, card selection, password verification, and data reading.


- Card Reset Response (Card Detection): The communication protocol and data rate for M1 radio cards are predefined. When a card enters the reader's operating range, the reader communicates with it using a specific protocol to determine if it is an M1 radio card, i.e., to verify the card type.

- Anti-collision Mechanism: If multiple cards enter the reader's operating range, the anti-collision mechanism selects one card for operation, while the others remain in idle mode until the next selection. This process returns the selected card's serial number.

- Card Selection: The selected card's serial number is chosen, and the card's capacity code is simultaneously returned.

- Three-way Mutual Authentication: After selecting the card to be processed, the reader determines the sector to be accessed and performs a password verification for that sector. After three mutual authentications, communication can proceed using encrypted streams. If accessing another sector, another password verification must be performed.

- Data Reading: This involves data communication between the MFRC522 and the M1 radio card.


4.Introduction to NFC-related APIs
------------------------------------

The sequence of data interactions between MFRC522 and M1 cards is as follows: 1. Initialization (configure card type), 2. Card detection, 3. Anti-collision, 4. Card selection, 5. Password verification, 6. Data reading.

- 1 MFRC522 Initialization Function
    void bk_nfc_init(void);

- 2 Card Detection
    char bk_mfrc522_request(uint8_t reqCode, uint8_t \*pTagType);

    .. note::
        - @param reqCode -[in] Card detection method, 0x52 detects all cards compliant with ISO/IEC 14443A, 0x26 detects cards not in sleep mode
        - @param pTagType -[out] Card type code
        - @return  Status value, MI OK - success; MI_ERR - failure

    .. important::
        - Card type code:
        - 0x4400 = Mifare_UltraLight
        - 0x0400 = Mifare_One(S50)
        - 0x0200 = Mifare_One(S70)
        - 0x0800 = Mifare_Pro(X)
        - 0x4403 = Mifare_DESFire

- 3 Anti-collision
    char bk_mfrc522_anticoll(uint8_t \*pSnr);

    .. note::
        - param pSnr -[out]  Card serial number, 4 bytes
        - return  Status value, MI OK - success; MI_ERR - failure

- 4 Card Selection
    char bk_mfrc522_select(uint8_t \*pSnr);

    .. note::
        - param pSnr -[in]  Card serial number, 4 bytes
        - return  Status value, MI OK - success; MI_ERR - failure

- 5 Card Password Verification
    char bk_mfrc522_authState(uint8_t authMode, uint8_t addr, uint8_t \*pKey, uint8_t \*pSnr);

    .. note::
        - @param authMode -[in] Password verification mode，0x60 verify A key, 0x61 verify B key
        - @param addr -[in] Block address
        - @param pKey -[in] Password
        - @param pSnr -[in]  Card serial number, 4 bytes
        - @return  Status value, MI OK - success; MI_ERR - failure

- 6 Reading Data from M1 Card
    char bk_mfrc522_read(uint8_t addr, uint8_t \*pData);

    .. note::
        - @param addr -[in] Block address
        - @param pData -[out] Read data，16bytes
        - @return  Status value, MI OK - success; MI_ERR - failure

- 7 Writing Data to M1 Card
    char bk_mfrc522_write(uint8_t addr, uint8_t \*pData);

    .. note::
        - @param addr -[in] Block address
        - @param pData -[out] write data，16bytes
        - @return  Status value, MI OK - success; MI_ERR - failure

5.NFC Testing Command Introduction
------------------------------------
Using the bekengenie project as an example, this section introduces the NFC testing procedure. First, compile the bekengenie project: make bk7258 PROJECT=bekengenie. After successful compilation, burn the all-app.bin file from the build directory.
NFC Card Testing The NFC card testing can be divided into two types:

  .. important::
    The NFC function is disabled by default. To enable it (for example, in the bekken_genie project), please go to projects/beken_genie/config/bk7258/config and set the macro CONFIG_NFC_ENABLE to 'y'.

  -1) Active Testing:The board has a task that periodically performs card detection by default;
    - 1.1)A task is enabled by default on the board to periodically perform a card detection operation. If you do not want to run this task, you can comment out the corresponding line of code, which can be found in the following file path: projects/beken_genie/main/app_main.c

    .. figure:: ../../../../common/_static/nfc_task.png
        :align: center
        :alt: nfc_task
        :figclass: align-center

        Fig1 nfc_task

    - 1.2)If no NFC card is nearby or no antenna is installed, it will periodically print the following log: "nfc request fail".

    .. figure:: ../../../../common/_static/fail.png
        :align: center
        :alt: nfc_fail
        :figclass: align-center

        Fig2 nfc_fail

    -1.3)When an NFC card is brought near the board (with the antenna installed), the card detection is completed, and it will periodically print the following log: "nfc request ok" and "nfc post ok".

    .. figure:: ../../../../common/_static/antenna.png
        :align: center
        :alt: antenna
        :figclass: align-center

        Fig3 antenna

    .. figure:: ../../../../common/_static/success.png
        :align: center
        :alt: success(:
        :figclass: align-center

        Fig4 success

  -2) Manual Testing:Manual testing requires sending CLI commands to perform NFC testing.;(the route is:bk_avdk/bk_idk/components/bk_nfc/mfrc522_test.c)
    - 2.1)Because there is a task at the board end that periodically performs a card search operation, you need to delete this task first by sending the following command:

    .. figure:: ../../../../common/_static/deinit.png
        :align: center
        :alt: deinit
        :figclass: align-center

        Fig5 deinit

    - 2.2) Support one-click CLI command to complete all NFC tests (card detection, anti-collision, card selection, password verification, data reading)
    - test command:nfc_test

    .. figure:: ../../../../common/_static/nfc_test.png
        :align: center
        :alt: nfc_test
        :figclass: align-center

        Fig6 nfc_test

    - 2.3) Also supports step-by-step testing，
    - step1:Initialize NFC using the CLI command:nfc_cmd_test init

    .. figure:: ../../../../common/_static/nfc_init.png
        :align: center
        :alt: nfc_init
        :figclass: align-center

        Fig7 nfc_init

    - step2:Perform card detection using the CLI command:nfc_cmd_test request

    .. figure:: ../../../../common/_static/nfc_request.png
        :align: center
        :alt: nfc_request
        :figclass: align-center

        Fig8 nfc_request

    - step3:Perform anti-collision using the CLI command:nfc_cmd_test anticoll

    .. figure:: ../../../../common/_static/nfc_anticoll.png
        :align: center
        :alt: anticoll
        :figclass: align-center

        Fig9 nfc_anticoll

    - step4:Select the card using the CLI command:nfc_cmd_test select

    .. figure:: ../../../../common/_static/nfc_select.png
        :align: center
        :alt: nfc_select
        :figclass: align-center

        Fig10 nfc_select

    - step5: Perform password verification using the CLI command:nfc_cmd_test authState

    .. figure:: ../../../../common/_static/nfc_authstate.png
        :align: center
        :alt: nfc_authState
        :figclass: align-center

        Fig11 nfc_authState


    - step6:Write data using the CLI command:nfc_write_test write 'a'

    .. figure:: ../../../../common/_static/nfc_write.png
        :align: center
        :alt: nfc_write
        :figclass: align-center

        Fig12 nfc_write


    - step7:Read data using the CLI command:nfc_write_test read 0

    .. figure:: ../../../../common/_static/nfc_read.png
        :align: center
        :alt: nfc_read
        :figclass: align-center

        Fig13 nfc_read