Nfc brief introduction
===============================

:link_to_translation:`zh_CN:[中文]`

一.Overview
----------------------------

This document provides an overview of the NFC usage process to better analyze and solve problems.

二.Introduction to the NFC Module
----------------------------------

The MFRC522 is a highly integrated read/write IC designed for contactless communication at 13.56MHz. The MFRC522 reader supports ISO/IEC 14443 A/MIFARE and NTAG standards, with host interfaces including SPI, I2C, and UART. Currently, we use the UART protocol for communication with the MFRC522.

三.Working Process of MFRC522
------------------------------

In general, the process can be summarized into the following parts: card detection, anti-collision, card selection, password verification, and data reading.


- Card Reset Response (Card Detection): The communication protocol and data rate for M1 radio cards are predefined. When a card enters the reader's operating range, the reader communicates with it using a specific protocol to determine if it is an M1 radio card, i.e., to verify the card type.

- Anti-collision Mechanism: If multiple cards enter the reader's operating range, the anti-collision mechanism selects one card for operation, while the others remain in idle mode until the next selection. This process returns the selected card's serial number.

- Card Selection: The selected card's serial number is chosen, and the card's capacity code is simultaneously returned.

- Three-way Mutual Authentication: After selecting the card to be processed, the reader determines the sector to be accessed and performs a password verification for that sector. After three mutual authentications, communication can proceed using encrypted streams. If accessing another sector, another password verification must be performed.

- Data Reading: This involves data communication between the MFRC522 and the M1 radio card.


三.Introduction to NFC-related APIs
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

