/*
 * sbl_device_cc2650.c
 *
 *  Created on: 26/06/2019
 *  Author: vinay divakar
 *  Description: CC26x0 device specific SBL functions
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "sbl_device_cc2640.h"

/* Macros */
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

/* Struct used when splitting long transfers */
typedef struct {
    uint32_t startAddr;
    uint32_t byteCount;
    uint32_t startOffset;
    bool     bExpectAck;
} tTransfer;

/* Static Var's */
static uint32_t m_flashSize;
static uint32_t m_ramSize;
static uint32_t m_deviceId;
static uint32_t devFlashBase;
static bool m_bCommInitialized;


/* Device revision. Used internally by SBL to handle
 * early samples with different command IDs.
 */
static uint32_t m_deviceRev;

/* Static functions */
static tSblStatus cmdDownload(uint32_t ui32Address, uint32_t ui32Size);
static uint32_t addressToPage(uint32_t ui32Address);

/* Some small functions. Lets save some file space */
uint32_t getFlashSize() { return (m_flashSize);}
uint32_t getRamSize() { return m_ramSize; }
void setDeviceFlashBase(uint32_t valFlashBase) { devFlashBase = valFlashBase;}
uint32_t getDeviceFlashBase() { return(devFlashBase);}

/****************************************************************
 * Function Name : eraseFlashBank
 * Description   : Erases all customer accessible flash sectors
 *                 not protected by FCFG1
 * Returns       : Returns SBL_SUCCESS, ...
 * Params        : None
 ****************************************************************/
tSblStatus eraseFlashBank()
{
    tSblStatus retCode = SBL_SUCCESS;
    bool bResponse = false;

    if(get_filed() < 0)
        return (SBL_PORT_ERROR);

    /* Send the command */
    if((retCode = sendCmd(CMD_BANK_ERASE, NULL, 0)) != SBL_SUCCESS)
        return retCode;

    /* Get the response */
    if((retCode = getCmdResponse(&bResponse, 0)) != SBL_SUCCESS)
        return retCode;

    return (bResponse) ? SBL_SUCCESS : SBL_ERROR;
}

/****************************************************************
 * Function Name : eraseFlashRange
 * Description   : This function erases device flash pages.
 *                 Starting page is the page that includes the
 *                 address in \e startAddress. Ending page is the page
 *                 that includes the address <startAddress + byteCount>.
 *                 CC13/CC26xx erase size is 4KB.
 * Returns       : Returns SBL_SUCCESS, ...
 * Params        @ui32StartAddress: The start address in flash.
 *               @ui32ByteCount: The number of bytes to erase.
 ****************************************************************/
tSblStatus eraseFlashRange(uint32_t ui32StartAddress,
                              uint32_t ui32ByteCount)
{
    tSblStatus retCode = SBL_SUCCESS;
    bool bSuccess = false;
    uint8_t pcPayload[4];
    uint32_t devStatus;

    if(get_filed() < 0)
        return (SBL_PORT_ERROR);

    /*Calculate retry count */
    uint32_t ui32PageCount = ui32ByteCount / SBL_CC2650_PAGE_ERASE_SIZE;
    if( ui32ByteCount % SBL_CC2650_PAGE_ERASE_SIZE) ui32PageCount ++;
    setProgress( 0 );

    for(uint32_t i = 0; i < ui32PageCount; i++)
    {
        /* Build payload - 4B address (MSB first) */
        ulToCharArray(ui32StartAddress + i*(4096), &pcPayload[0]);

        /* Send command */
        if((retCode = sendCmd(CMD_SECTOR_ERASE, pcPayload, 4)) != SBL_SUCCESS)
            return (retCode);

        /* Receive command response (ACK/NAK) */
        if((retCode = getCmdResponse(&bSuccess, 0)) != SBL_SUCCESS)
            return (retCode);

        if(!bSuccess)
            return (SBL_ERROR);

        /* Check device status (Flash failed if page(s) locked) */

        readStatus(&devStatus);
        if(devStatus != CMD_RET_SUCCESS)
        {
            printf("Flash erase failed. (Status 0x%02X = %s). Flash pages may be locked.\n", devStatus, getCmdStatusString(devStatus));
            return (SBL_ERROR);
        }

        setProgress( 100*(i+1)/ui32PageCount );
    }

    return (SBL_SUCCESS);
}

/****************************************************************
 * Function Name : readMemory32
 * Description   : This function reads \e ui32UnitCount (32 bit)
                   words of data from device. Destination array
                   is 32 bit wide. The start address must be 4
                   byte aligned.
 * Returns       : Returns SBL_SUCCESS, ...
 * Params        : @ui32StartAddress: Start address in device
                   (must be 4 byte aligned).
                   @ui32UnitCount: Number of data words to read.
                   @pcData: Pointer to where read data is stored.
 ****************************************************************/
tSblStatus readMemory32(uint32_t ui32StartAddress, uint32_t ui32UnitCount,
                        uint32_t *pui32Data)
{
    tSblStatus retCode = SBL_SUCCESS;
    bool bSuccess = false;

    /* Check input arguments */
    if((ui32StartAddress & 0x03))
    {
        printf("readMemory32(): Start address (0x%08X) must be a multiple of 4.\n", ui32StartAddress);
        return (SBL_ARGUMENT_ERROR);
    }

    setProgress(0);

    if(get_filed() < 0)
        return (SBL_PORT_ERROR);

    uint8_t pcPayload[6];
    uint32_t responseData[SBL_CC2650_MAX_MEMREAD_WORDS];
    uint32_t chunkCount = ui32UnitCount / SBL_CC2650_MAX_MEMREAD_WORDS;
    if(ui32UnitCount % SBL_CC2650_MAX_MEMREAD_WORDS) chunkCount++;
    uint32_t remainingCount = ui32UnitCount;

    for(uint32_t i = 0; i < chunkCount; i++)
    {
        uint32_t dataOffset = (i * SBL_CC2650_MAX_MEMREAD_WORDS);
        uint32_t chunkStart = ui32StartAddress + dataOffset;
        uint32_t chunkSize  = MIN(remainingCount, SBL_CC2650_MAX_MEMREAD_WORDS);
        remainingCount -= chunkSize;

        /*
         * Build payload
        - 4B address (MSB first)
        - 1B access width
        - 1B Number of accesses (in words)
         */
        ulToCharArray(chunkStart, (uint8_t *)&pcPayload[0]);
        pcPayload[4] = SBL_CC2650_ACCESS_WIDTH_32B;
        pcPayload[5] = chunkSize;

        setProgress(((i * 100) / chunkCount));

        /* Send Command */
        if((retCode = sendCmd(CMD_MEMORY_READ,pcPayload, 6)) != SBL_SUCCESS)
            return retCode;

        /* Receive command response (ACK/NAK) */
        if((retCode = getCmdResponse(&bSuccess, 0)) != SBL_SUCCESS)
            return retCode;
        if(!bSuccess)
            return (SBL_ERROR);

        /* Receive 4B response */
        uint32_t expectedBytes = chunkSize * 4;
        uint32_t recvBytes = expectedBytes;
        if((retCode = getResponseData((uint8_t*)responseData, &recvBytes, 0)) != SBL_SUCCESS)
        {
            /* Respond with NAK */
            sendCmdResponse(false);
            return retCode;
        }

        if(recvBytes != expectedBytes)
        {
            /* Respond with NAK */
            sendCmdResponse(false);
            printf("Didn't receive 4 B.\n");
            return (SBL_ERROR);
        }

        memcpy(&pui32Data[dataOffset], responseData, expectedBytes);

        /* Respond with ACK */
        sendCmdResponse(true);
    }
    /* Set progress */
    setProgress(100);

    return SBL_SUCCESS;
}

/****************************************************************
 * Function Name : ping
 * Description   : This function sends ping command to device.
 * Returns       : Returns SBL_SUCCESS, ...
 * Params        : None
 ****************************************************************/
tSblStatus ping()
{
    tSblStatus retCode = SBL_SUCCESS;
    bool bResponse = false;

    if(get_filed() < 0)
        return (SBL_PORT_ERROR);

    /* Send command */
    if((retCode = sendCmd(CMD_PING, NULL, 0)) != SBL_SUCCESS)
        return retCode;

    /* Get response */
    if((retCode = getCmdResponse(&bResponse, 0)) != SBL_SUCCESS)
        return retCode;

    return (bResponse) ? SBL_SUCCESS : SBL_ERROR;
}

/****************************************************************
 * Function Name : getDeviceRev
 * Description   : This function sends ping command to device.
 * Returns       : None
 * Params        : None
 ****************************************************************/
uint32_t getDeviceRev(uint32_t deviceId)
{
    uint32_t tmp = deviceId >> 28;
    switch(tmp)
    {
    // Early samples (Rev 1)
    case 0:
    case 1:
        return 1;
    default:
        return 2;
    }
}

/****************************************************************
 * Function Name : readDeviceId
 * Description   : This function reads device ID.
 * Returns       : Returns SBL_SUCCESS, ...
 * Params        : @pui32DeviceId: Pointer to where device ID is
 *                                 stored.
 ****************************************************************/
tSblStatus readDeviceId(uint32_t *pui32DeviceId)
{
    int retCode = SBL_SUCCESS;
    bool bSuccess = false;

    if(get_filed() < 0)
        return (SBL_PORT_ERROR);

    /* Send command */
    if((retCode = sendCmd(CMD_GET_CHIP_ID, NULL, 0)) != SBL_SUCCESS)
        return retCode;

    /* Receive command response (ACK/NAK) */
    if((retCode = getCmdResponse(&bSuccess, 0)) != SBL_SUCCESS)
        return retCode;

    if(!bSuccess)
        return (SBL_ERROR);

    /* Receive response data */
    uint8_t pId[4];
    memset(pId, 0, 4);
    uint32_t numBytes = 4;
    if((retCode = getResponseData(pId, &numBytes, 0)) != SBL_SUCCESS)
    {
        /* Respond with NAK */
        sendCmdResponse(false);
        return retCode;
    }

    if(numBytes != 4)
    {
        /* Respond with NAK */
        sendCmdResponse(false);
        printf("Didn't receive 4 B.\n");
        return (SBL_ERROR);
    }

    /* Respond with ACK */
    sendCmdResponse(true);

    /* Store retrieved ID and report success */
    *pui32DeviceId = charArrayToUL((const char*)pId);
    m_deviceId = *pui32DeviceId;

    /* Store device revision (used internally, see sbl_device_cc2650.h) */
    m_deviceRev = getDeviceRev(m_deviceId);

    return (SBL_SUCCESS);
}

/****************************************************************
 * Function Name : readFlashSize
 * Description   : This function reads device FLASH size in bytes.
 * Returns       : Returns SBL_SUCCESS, ...
 * Params        : @pui32FlashSize: Pointer to where FLASH size is
 *                  stored.
 ****************************************************************/
tSblStatus readFlashSize(uint32_t *pui32FlashSize)
{
    tSblStatus retCode = SBL_SUCCESS;

    /* Read CC2650 DIECFG0 (contains FLASH size information) */
    uint32_t addr = SBL_CC2650_FLASH_SIZE_CFG;
    uint32_t value;
    if((retCode = readMemory32(addr, 1, &value)) != SBL_SUCCESS)
    {
        printf("Failed to read device FLASH size\n");
        return retCode;
    }

    /* Calculate flash size (The number of flash sectors are at bits [7:0]) */
    value &= 0xFF;
    *pui32FlashSize = value*SBL_CC2650_PAGE_ERASE_SIZE;

    m_flashSize = *pui32FlashSize;

    return (SBL_SUCCESS);
}

/****************************************************************
 * Function Name : readRamSize
 * Description   : This function reads device RAM size in bytes.
 * Returns       : Returns SBL_SUCCESS, ...
 * Params        : @pui32RamSize: Pointer to where RAM size is
                     stored.
 ****************************************************************/
tSblStatus readRamSize(uint32_t *pui32RamSize)
{
    int retCode = SBL_SUCCESS;

    /* Read CC2650 DIECFG0 (contains RAM size information */
    uint32_t addr = SBL_CC2650_RAM_SIZE_CFG;
    uint32_t value;
    if((retCode = readMemory32(addr, 1, &value)) != SBL_SUCCESS)
    {
        printf("Failed to read device RAM size");
        return (retCode);
    }

    /* Calculate RAM size in bytes (Ram size bits are at bits [1:0]) */
    value &= 0x03;
    if(m_deviceRev == 1)
    {
        /* Early samples has less RAM */
        switch(value)
        {
        case 3: *pui32RamSize = 0x4000; break;    // 16 KB
        case 2: *pui32RamSize = 0x2000; break;    // 8 KB
        case 1: *pui32RamSize = 0x1000; break;    // 4 KB
        case 0:                                   // 2 KB
        default:*pui32RamSize = 0x0800; break;    // All invalid values are interpreted as 2 KB
        }
    }
    else
    {
        switch(value)
        {
        case 3: *pui32RamSize = 0x5000; break;    // 20 KB
        case 2: *pui32RamSize = 0x4000; break;    // 16 KB
        case 1: *pui32RamSize = 0x2800; break;    // 10 KB
        case 0:                                   // 4 KB
        default:*pui32RamSize = 0x1000; break;    // All invalid values are interpreted as 4 KB
        }
    }

    /* Save RAM size internally */
    m_ramSize = *pui32RamSize;

    return (retCode);
}

/****************************************************************
 * Function Name : reset
 * Description   : This function reset the device. Communication
                   to the device must be reinitialized after calling
                   this function.
 * Returns       : Returns SBL_SUCCESS, ...
 * Params        : None.
 ****************************************************************/
tSblStatus reset()
{
    tSblStatus retCode = SBL_SUCCESS;
    bool bSuccess = false;

    if(get_filed() < 0)
        return (SBL_PORT_ERROR);

    /* Send CMD */
    if((retCode = sendCmd(CMD_RESET, NULL, 0)) != SBL_SUCCESS)
        return retCode;

    /* Receive command response (ACK/NAK) */
    if((retCode = getCmdResponse(&bSuccess, 0)) != SBL_SUCCESS)
        return retCode;

    if(!bSuccess)
    {
        printf("Reset command NAKed by device.\n");
        return (SBL_ERROR);
    }

    m_bCommInitialized = false;
    return (SBL_SUCCESS);
}

/****************************************************************
 * Function Name : readMemory8
 * Description   : This function reads \e unitCount bytes of data
 *                  from device. Destination array is 8 bit wide.
 * Returns       : Returns SBL_SUCCESS, ...
 * Params        : None.
 ****************************************************************/
tSblStatus readMemory8(uint32_t ui32StartAddress, uint32_t ui32UnitCount,
                       uint8_t *pcData)
{
    int retCode = SBL_SUCCESS;
    bool bSuccess = false;

    /* Check input arguments */
    if(ui32UnitCount == 0)
    {
        printf("readMemory8(): Read count is zero. Must be at least 1.\n");
        return (SBL_ARGUMENT_ERROR);
    }

    if(get_filed() < 0)
        return (SBL_PORT_ERROR);

    uint8_t pcPayload[6];
    uint32_t chunkCount = ui32UnitCount / SBL_CC2650_MAX_MEMREAD_BYTES;
    if(ui32UnitCount % SBL_CC2650_MAX_MEMREAD_BYTES) chunkCount++;
    uint32_t remainingCount = ui32UnitCount;

    for(uint32_t i = 0; i < chunkCount; i++)
    {
        uint32_t dataOffset = (i * SBL_CC2650_MAX_MEMREAD_BYTES);
        uint32_t chunkStart = ui32StartAddress + dataOffset;
        uint32_t chunkSize  = MIN(remainingCount, SBL_CC2650_MAX_MEMREAD_BYTES);
        remainingCount -= chunkSize;

        /*
         * Build payload
            - 4B address (MSB first)
            - 1B access width
            - 1B number of accesses (in bytes)
         */
        ulToCharArray(chunkStart, &pcPayload[0]);
        pcPayload[4] = SBL_CC2650_ACCESS_WIDTH_8B;
        pcPayload[5] = chunkSize;

        /* Set progress */
        setProgress(((i*100) / chunkCount));

        /* Send command */
        if((retCode = sendCmd(CMD_MEMORY_READ, pcPayload, 6)) != SBL_SUCCESS)
            return (retCode);

        /* Receive command response (ACK/NAK) */
        if((retCode = getCmdResponse(&bSuccess, 0)) != SBL_SUCCESS)
            return retCode;

        if(!bSuccess)
            return (SBL_ERROR);

        /* Receive response */
        uint32_t expectedBytes = chunkSize;
        if((retCode = getResponseData(&pcData[dataOffset], &chunkSize, 0)) != SBL_SUCCESS)
        {
            /* Respond with NAK */
            sendCmdResponse(false);
            return retCode;
        }

        if(chunkSize != expectedBytes)
        {
            /* Respond with NAK */
            sendCmdResponse(false);
            printf("readMemory8(): Received %d bytes (%d B expected) in iteration %d.\n", chunkSize, expectedBytes, i);
            return (SBL_ERROR);
        }

        /* Respond with ACK */
        sendCmdResponse(true);
    }

    /* Set progress */
    setProgress(100);

    return (SBL_SUCCESS);
}

/****************************************************************
 * Function Name : addressInBLWorkMemory
 * Description   : This function checks if the specified \e
                     ui32StartAddress (and range) overlaps the
                     bootloader's working memory or stack area.

    The bootloader does not protect against writing to these ranges,
    but doing so is almost guaranteed to crash the bootloader and
    requires a reboot. SRAM ranges used by the bootloader:
       \li Work memory @ 0x20000000-0x2000016F
       \li Stack area  @ 0x20000FC0-0x20000FFF

 * Returns       : Returns true if the address/range is within
                     the device RAM.
 * Params        @ui32StartAddress:The start address of the range
 *               @ui32ByteCount: (Optional) The number of bytes
 *               in the range.
 ****************************************************************/
bool addressInBLWorkMemory(uint32_t ui32StartAddress,
                           uint32_t ui32ByteCount/* = 1*/)
{
    uint32_t ui32EndAddr = ui32StartAddress + ui32ByteCount;

    if(ui32StartAddress <= SBL_CC2650_BL_WORK_MEMORY_END)
        return true;

    if((ui32StartAddress >= SBL_CC2650_BL_STACK_MEMORY_START) &&
            (ui32StartAddress <= SBL_CC2650_BL_STACK_MEMORY_END))
        return true;

    if((ui32EndAddr >= SBL_CC2650_BL_STACK_MEMORY_START) &&
            (ui32EndAddr <= SBL_CC2650_BL_STACK_MEMORY_END))
        return true;

    return false;
}
/****************************************************************
 * Function Name : writeMemory32
 * Description   : This function writes \e unitCount words of
                   data to device SRAM.vMax 61 32-bit words
                   supported. Source array is 32 bit wide. \e
                   ui32StartAddress must be 4 byte aligned.
 * Returns       : Returns SBL_SUCCESS, ...
 * Params        : @ui32StartAddress: Start address in device.
 *                 @ui32UnitCount: Number of data words (32bit)
 *                 to write.
 *                 @pui32Data: Pointer to source data.
 ****************************************************************/
tSblStatus writeMemory32(uint32_t ui32StartAddress,
                         uint32_t ui32UnitCount,
                         const uint32_t *pui32Data)
{
    tSblStatus retCode = SBL_SUCCESS;
    bool bSuccess = false;

    /* Check input arguments */
    if((ui32StartAddress & 0x03))
    {
        printf("writeMemory32(): Start address (0x%08X) must 4 byte aligned.\n", ui32StartAddress);
        return (SBL_ARGUMENT_ERROR);
    }
    if(addressInBLWorkMemory(ui32StartAddress, ui32UnitCount * 4))
    {
        // Issue warning
        printf("writeMemory32(): Writing to bootloader work memory/stack:\n(0x%08X-0x%08X, 0x%08X-0x%08X)\n",
               SBL_CC2650_BL_WORK_MEMORY_START,SBL_CC2650_BL_WORK_MEMORY_END, SBL_CC2650_BL_STACK_MEMORY_START,SBL_CC2650_BL_STACK_MEMORY_END);
        return (SBL_ARGUMENT_ERROR);
    }

    if(get_filed() < 0)
        return (SBL_PORT_ERROR);

    uint32_t chunkCount = (ui32UnitCount / SBL_CC2650_MAX_MEMWRITE_WORDS);
    if(ui32UnitCount % SBL_CC2650_MAX_MEMWRITE_WORDS) chunkCount++;
    uint32_t remainingCount = ui32UnitCount;
    uint8_t* pcPayload = (uint8_t*)calloc((5 + (SBL_CC2650_MAX_MEMWRITE_WORDS*4)), sizeof(uint8_t));

    if(!pcPayload)
        return (SBL_MALLOC_ERROR);

    for(uint32_t i = 0; i < chunkCount; i++)
    {
        uint32_t chunkOffset = i * SBL_CC2650_MAX_MEMWRITE_WORDS;
        uint32_t chunkStart  = ui32StartAddress + (chunkOffset * 4);
        uint32_t chunkSize   = MIN(remainingCount, SBL_CC2650_MAX_MEMWRITE_WORDS);
        remainingCount -= chunkSize;

        /*
         *  Build payload
         - 4B address (MSB first)
         - 1B access width
         - 1-SBL_CC2650_MAX_MEMWRITE_WORDS data (MSB first)
         */
        ulToCharArray(chunkStart, &pcPayload[0]);
        pcPayload[4] = SBL_CC2650_ACCESS_WIDTH_32B;
        for(uint32_t j = 0; j < chunkSize; j++)
            ulToCharArray(pui32Data[j + chunkOffset], &pcPayload[5 + j*4]);

        /* Set progress */
        setProgress( ((i * 100) / chunkCount) );

        /* Send CMD */
        if((retCode = sendCmd(CMD_MEMORY_WRITE, pcPayload, 5 + chunkSize*4)) != SBL_SUCCESS)
            return (retCode);

        /* Receive command response (ACK/NAK) */
        if((retCode = getCmdResponse(&bSuccess, 5)) != SBL_SUCCESS)
            return (retCode);

        if(!bSuccess)
        {
            printf("writeMemory32(): Device NAKed command for address 0x%08X.\n", chunkStart);
            return (SBL_ERROR);
        }
    }

    /* Set progress */
    setProgress(100);

    /* Cleanup */
    free(pcPayload);

    return (SBL_SUCCESS);
}

/****************************************************************
 * Function Name : writeMemory8
 * Description   : Write \e unitCount words of data to device SRAM.
                   Source array is 8 bit wide. Max 244 bytes of
                   data. Source array is 32 bit wide. Parameters \e
                   startAddress and \e unitCount must be a a multiple
                   of 4.
 * Returns       : Returns SBL_SUCCESS, ...
 * Params        : @ui32StartAddress: Start address in device.
 *                 @ui32UnitCount: Number of bytes to write.
 *                 @pcData:Pointer to source data.
 ****************************************************************/
tSblStatus writeMemory8(uint32_t ui32StartAddress,
                        uint32_t ui32UnitCount,
                        const uint8_t *pcData)
{
    tSblStatus retCode = SBL_SUCCESS;
    bool bSuccess = false;

    /* Check input arguments */
    if(addressInBLWorkMemory(ui32StartAddress, ui32UnitCount))
    {
        /* Issue warning */
        printf("writeMemory8(): Writing to bootloader work memory/stack:\n(0x%08X-0x%08X, 0x%08X-0x%08X)\n",
               SBL_CC2650_BL_WORK_MEMORY_START,SBL_CC2650_BL_WORK_MEMORY_END, SBL_CC2650_BL_STACK_MEMORY_START,SBL_CC2650_BL_STACK_MEMORY_END);
        return (SBL_ARGUMENT_ERROR);
    }

    if(get_filed() < 0)
        return (SBL_PORT_ERROR);

    uint32_t chunkCount = (ui32UnitCount / SBL_CC2650_MAX_MEMWRITE_BYTES);
    if(ui32UnitCount % SBL_CC2650_MAX_MEMWRITE_BYTES) chunkCount++;
    uint32_t remainingCount = ui32UnitCount;

    uint8_t* pcPayload = (uint8_t*)calloc((5 + (SBL_CC2650_MAX_MEMWRITE_WORDS*4)), sizeof(uint8_t));

    if(!pcPayload)
        return (SBL_MALLOC_ERROR);

    for(uint32_t i = 0; i < chunkCount; i++)
    {
        uint32_t chunkOffset = i * SBL_CC2650_MAX_MEMWRITE_BYTES;
        uint32_t chunkStart  = ui32StartAddress + chunkOffset;
        uint32_t chunkSize   = MIN(remainingCount, SBL_CC2650_MAX_MEMWRITE_BYTES);
        remainingCount -= chunkSize;
        /*
         * Build payload
             - 4B address (MSB first)
             - 1B access width
             - 1-SBL_CC2650_MAX_MEMWRITE_BYTES bytes data
         */

        ulToCharArray(chunkStart, &pcPayload[0]);
        pcPayload[4] = SBL_CC2650_ACCESS_WIDTH_8B;
        memcpy(&pcPayload[5], &pcData[chunkOffset], chunkSize);

        /* Set progress */
        setProgress( ((i * 100) / chunkCount) );

        /* Send CMD */
        if((retCode = sendCmd(CMD_MEMORY_WRITE, pcPayload, 5 + chunkSize)) != SBL_SUCCESS)
            return (retCode);


        /* Receive command response (ACK/NAK) */
        if((retCode = getCmdResponse(&bSuccess, 5)) != SBL_SUCCESS)
            return (retCode);

        if(!bSuccess)
        {
            printf("writeMemory8(): Device NAKed command for address 0x%08X.\n", chunkStart);
            return (SBL_ERROR);
        }
    }

    /* Set progress */
    setProgress(100);

    /* Clean up */
    free(pcPayload);

    return SBL_SUCCESS;
}

/****************************************************************
 * Function Name : cmdSendData
 * Description   : This function sends the CC2650 SendData command
                     and handles the device response.
 * Returns       :  Returns SBL_SUCCESS if command and response was
                     successful.
 * Params        : @pcData: Pointer to the data to send.
 *                 @ui32ByteCount: The number of bytes to send.
 ****************************************************************/
tSblStatus cmdSendData(const uint8_t *pcData, uint32_t ui32ByteCount)
{
    tSblStatus retCode = SBL_SUCCESS;
    bool bSuccess = false;

    /* Check input arg's */
    if(ui32ByteCount > SBL_CC2650_MAX_BYTES_PER_TRANSFER)
    {
        printf("Error: Byte count (%d) exceeds maximum transfer size %d.\n", ui32ByteCount, SBL_CC2650_MAX_BYTES_PER_TRANSFER);
        return (SBL_ERROR);
    }

    /* Send CMD */
    if((retCode = sendCmd(CMD_SEND_DATA, pcData, ui32ByteCount)) != SBL_SUCCESS)
        return (retCode);

    /* Receive command response (ACK/NAK) */
    if((retCode = getCmdResponse(&bSuccess, 3)) != SBL_SUCCESS)
        return (retCode);

    if(!bSuccess)
        return (SBL_ERROR);

    return (SBL_SUCCESS);
}

/****************************************************************
 * Function Name : writeFlashRange
 * Description   : Write \e unitCount words of data to device FLASH.
                   Source array is 8 bit wide. Parameters \e
                   startAddress and \e unitCount must be a a
                   multiple of 4. This function does not erase the
                   flash before writing data, this must be done
                   using e.g. eraseFlashRange().
 * Returns       : Returns SBL_SUCCESS, ...
 * Params        : @ui32StartAddress: Start address in device. Must
                     be a multiple of 4.
 *                 @ui32UnitCount: Must be a multiple of 4.
 *                 @pcData:Pointer to source data.
 ****************************************************************/
tSblStatus writeFlashRange(uint32_t ui32StartAddress,
                           uint32_t ui32ByteCount, const char *pcData)
{
    uint32_t devStatus = CMD_RET_UNKNOWN_CMD;
    tSblStatus retCode = SBL_SUCCESS;
    uint32_t bytesLeft, dataIdx, bytesInTransfer;
    uint32_t transferNumber = 1;
    bool bIsRetry = false;
    bool bBlToBeDisabled = false;
    tTransfer pvTransfer[2];
    uint32_t ui32TotChunks = (ui32ByteCount / SBL_CC2650_MAX_BYTES_PER_TRANSFER);
    if(ui32ByteCount % SBL_CC2650_MAX_BYTES_PER_TRANSFER) ui32TotChunks++;
    uint32_t ui32CurrChunk = 0;

    /* Calculate BL configuration address (depends on flash size) */
    uint32_t ui32BlCfgAddr = SBL_CC2650_FLASH_START_ADDRESS +      \
            getFlashSize() -                                            \
            SBL_CC2650_PAGE_ERASE_SIZE +                                \
            SBL_CC2650_BL_CONFIG_PAGE_OFFSET;

    /* Calculate BL configuration buffer index */
    uint32_t ui32BlCfgDataIdx = ui32BlCfgAddr - ui32StartAddress;

    /* Is BL configuration part of buffer? */
    if(ui32BlCfgDataIdx <= ui32ByteCount)
    {
        if(((pcData[ui32BlCfgDataIdx]) & 0xFF) != SBL_CC2650_BL_CONFIG_ENABLED_BM)
        {
            bBlToBeDisabled = false;
            printf("Warning: CC2650 bootloader will be disabled.\n");
        }
    }

    if(bBlToBeDisabled)
    {
        /*
         * Make two split transfers.
         * Make transfer (before lock bit )
         */
        pvTransfer[0].bExpectAck  = true;
        pvTransfer[0].startAddr   = ui32StartAddress;
        pvTransfer[0].byteCount   = (ui32BlCfgAddr - ui32StartAddress) & (~0x03);
        pvTransfer[0].startOffset = 0;

        /* The transfer locking the backdoor */
        pvTransfer[1].bExpectAck  = false;
        pvTransfer[1].startAddr   = ui32BlCfgAddr - (ui32BlCfgAddr % 4);
        pvTransfer[1].byteCount   = ui32ByteCount - pvTransfer[0].byteCount;
        pvTransfer[1].startOffset = ui32BlCfgDataIdx - (ui32BlCfgDataIdx % 4);
    }
    else
    {
        pvTransfer[0].bExpectAck  = true;
        pvTransfer[0].byteCount = ui32ByteCount;
        pvTransfer[0].startAddr = ui32StartAddress;
        pvTransfer[0].startOffset = 0;
    }

    /* For each transfer */
    for(uint32_t i = 0; i < 1; i++)
    {
        /* Sanity check */
        if(pvTransfer[i].byteCount == 0)
            continue;

        /* Set progress */
        setProgress(addressToPage(pvTransfer[i].startAddr));

        /* Send download command */
        if((retCode = cmdDownload(pvTransfer[i].startAddr,
                                  pvTransfer[i].byteCount)) != SBL_SUCCESS)
            return (retCode);

        /* Check status after download command */
        retCode = readStatus(&devStatus);
        if(retCode != SBL_SUCCESS)
        {
            printf("Error during download initialization. Failed to read device status after sending download command.\n");
            return (retCode);
        }
        if(devStatus != CMD_RET_SUCCESS)
        {
            printf("Error during download initialization. Device returned status %d (%s).\n", devStatus, getCmdStatusString(devStatus));
            return (SBL_ERROR);
        }

        /* Send data in chunks */
        bytesLeft = pvTransfer[i].byteCount;
        dataIdx   = pvTransfer[i].startOffset;
        while(bytesLeft)
        {
            /* Set progress */
            //setProgress(addressToPage(ui32StartAddress + dataIdx));
            setProgress( ((100*(++ui32CurrChunk))/ui32TotChunks) );

            /* Limit transfer count */
            bytesInTransfer = MIN(SBL_CC2650_MAX_BYTES_PER_TRANSFER, bytesLeft);

            /* Send Data command */
            if((retCode = cmdSendData((const uint8_t*)&pcData[dataIdx], bytesInTransfer)) != SBL_SUCCESS)
            {
                printf("Error during flash download. \n- Start address 0x%08X (page %d). \n- Tried to transfer %d bytes. \n- This was transfer %d.\n",
                       (ui32StartAddress+dataIdx),
                       addressToPage(ui32StartAddress+dataIdx),
                       bytesInTransfer,
                       (transferNumber));
                return (retCode);
            }

            if(pvTransfer[i].bExpectAck)
            {
                /* Check status after send data command */
                devStatus = 0;
                retCode = readStatus(&devStatus);
                if(retCode != SBL_SUCCESS)
                {
                    printf("Error during flash download. Failed to read device status.\n- Start address 0x%08X (page %d). \n- Tried to transfer %d bytes. \n- This was transfer %d in chunk %d.\n",
                           (ui32StartAddress+dataIdx),
                           addressToPage(ui32StartAddress + dataIdx),
                           (bytesInTransfer), (transferNumber),
                           (i));
                    return (retCode);
                }
                if(devStatus != CMD_RET_SUCCESS)
                {
                    printf("Device returned status %s\n", getCmdStatusString(devStatus));
                    if(bIsRetry)
                    {
                        /* We have failed a second time. Aborting. */
                        printf("Error retrying flash download.\n- Start address 0x%08X (page %d). \n- Tried to transfer %d bytes. \n- This was transfer %d in chunk %d.\n",
                               (ui32StartAddress+dataIdx),
                               addressToPage(ui32StartAddress + dataIdx),
                               (bytesInTransfer), (transferNumber),
                               (i));
                        return (SBL_ERROR);
                    }

                    /* Retry to send data one more time. */
                    bIsRetry = true;
                    continue;
                }
            }
            else
            {
                /* We're locking device and will lose access */
                m_bCommInitialized = false;
            }

            /* Update index and bytesLeft */
            bytesLeft -= bytesInTransfer;
            dataIdx += bytesInTransfer;
            transferNumber++;
            bIsRetry = false;
        }
    }
    return (SBL_SUCCESS);
}

/****************************************************************
 * Function Name : setCCFG
 * Description   : Writes the CC26xx defined CCFG fields to the
 *                  flash CCFG area with the values received in
 *                  the data bytes of this command.
 * Returns       :  Returns SBL_SUCCESS, ...
 * Params        : @ui32Field: CCFG Field ID which identifies the
 *                  CCFG parameter to be written.
 *                 @ui32FieldValue:  Field value to be programmed.
 ****************************************************************/
tSblStatus setCCFG(uint32_t ui32Field, uint32_t ui32FieldValue)
{
    tSblStatus retCode = SBL_SUCCESS;
    bool bSuccess = false;

    if(get_filed() < 0)
        return (SBL_PORT_ERROR);

    //
    // Generate payload
    // - 4B Field ID
    // - 4B Field value
    //
    char pcPayload[8];
    ulToCharArray(ui32Field, (uint8_t*)&pcPayload[0]);
    ulToCharArray(ui32FieldValue, (uint8_t*)&pcPayload[4]);

    /* Send command */
    if((retCode = sendCmd(CMD_SET_CCFG, (const uint8_t*)pcPayload, 8)) != SBL_SUCCESS)
        return (retCode);

    /* Receive command response (ACK/NAK) */
    if((retCode = getCmdResponse(&bSuccess, 0)) != SBL_SUCCESS)
        return (retCode);

    if(!bSuccess)
    {
        printf("Set CCFG command NAKed by device.\n");
        return (SBL_ERROR);
    }

    return (SBL_SUCCESS);
}

/****************************************************************
 * Function Name : addressToPage
 * Description   : This function returns the page within which
 *                  address \e ui32Address is located.
 * Returns       :  Returns the flash page within which an address
 *                  is located.
 * Params        : @ui32Address: The address.
 ****************************************************************/
uint32_t addressToPage(uint32_t ui32Address)
{
    return ((ui32Address - SBL_CC2650_FLASH_START_ADDRESS) /                  \
            SBL_CC2650_PAGE_ERASE_SIZE);
}

/****************************************************************
 * Function Name : addressInRam
 * Description   : This function checks if the specified \e
                     ui32StartAddress (and range)is located within
                     the device RAM area.
 * Returns       :  Returns true if the address/range is within
 *                  the device RAM.
 * Params        : @ui32Address: The start address of the range.
 *                 @pui32Bytecount:The number of bytes in the range.
 ****************************************************************/
bool addressInRam(uint32_t ui32StartAddress,
                  uint32_t ui32ByteCount/* = 1*/)
{
    uint32_t ui32EndAddr = ui32StartAddress + ui32ByteCount;

    if(ui32StartAddress < SBL_CC2650_RAM_START_ADDRESS)
        return false;

    if(ui32EndAddr > (SBL_CC2650_RAM_START_ADDRESS + getRamSize()))
        return false;

    return true;
}

/****************************************************************
 * Function Name : addressInFlash
 * Description   : This function checks if the specified \e
 *                  ui32StartAddress (and range) is located within
 *                  the device FLASH area.
 * Returns       :  Returns true if the address/range is within the
 *                  device flash.
 * Params        : @ui32Address: The start address of the range
 *                 @pui32Bytecount:The number of bytes in the range.
 ****************************************************************/
bool addressInFlash(uint32_t ui32StartAddress,
                    uint32_t ui32ByteCount/* = 1*/)
{
    uint32_t ui32EndAddr = ui32StartAddress + ui32ByteCount;

    if(ui32StartAddress < SBL_CC2650_FLASH_START_ADDRESS)
        return false;

    if(ui32EndAddr > (SBL_CC2650_FLASH_START_ADDRESS + getFlashSize()))
        return false;

    return true;
}

/****************************************************************
 * Function Name : cmdDownload
 * Description   : This function sends the CC2650 download command
 *                  and handles the device response.
 * Returns       :  Returns SBL_SUCCESS if command and response was
 *                  successful.
 * Params        : @ui32Address:  The start address in CC2650 flash.
 *                 @pui32Bytecount:The total number of bytes to
 *                 program on the device.
 ****************************************************************/
tSblStatus cmdDownload(uint32_t ui32Address, uint32_t ui32Size)
{
    int retCode = SBL_SUCCESS;
    bool bSuccess = false;

    // Check input arguments
    if(!addressInFlash(ui32Address, ui32Size))
    {
        printf("Flash download: Address range (0x%08X + %d bytes) is not in device FLASH nor RAM.\n", ui32Address, ui32Size);
        return (SBL_ARGUMENT_ERROR);
    }
    if(ui32Size & 0x03)
    {
        printf("Flash download: Byte count must be a multiple of 4\n");
        return (SBL_ARGUMENT_ERROR);
    }

    //
    // Generate payload
    // - 4B Program address
    // - 4B Program size
    //
    char pcPayload[8];
    ulToCharArray(ui32Address, (uint8_t*)&pcPayload[0]);
    ulToCharArray(ui32Size, (uint8_t*)&pcPayload[4]);

    /* Send command */
    if((retCode = sendCmd(CMD_DOWNLOAD, (const uint8_t*)pcPayload, 8)) != SBL_SUCCESS)
        return retCode;

    /* Receive command response (ACK/NAK) */
    if((retCode = getCmdResponse(&bSuccess, 0)) != SBL_SUCCESS)
        return retCode;

    /* Return command response */
    return (bSuccess) ? SBL_SUCCESS : SBL_ERROR;
}

/****************************************************************
 * Function Name : calculateCrc32
 * Description   : Calculate CRC over \e byteCount bytes, starting
 *                  at address  \e startAddress.
 * Returns       :  Returns SBL_SUCCESS, ...
 * Params        : @ui32StartAddress:  Start address in device.
 *                 @ui32ByteCount:Number of bytes to calculate CRC32
 *                 over.
 *                 @pui32Crc: Pointer to where checksum from device
 *                 is stored.
 ****************************************************************/
tSblStatus calculateCrc32(uint32_t ui32StartAddress,
                          uint32_t ui32ByteCount, uint32_t *pui32Crc)
{
    tSblStatus retCode = SBL_SUCCESS;
    bool bSuccess = false;
    char pcPayload[12];
    uint32_t ui32RecvCount = 0;

    /* Check input arguments */
    if(!addressInFlash(ui32StartAddress, ui32ByteCount) &&
            !addressInRam(ui32StartAddress, ui32ByteCount))
    {
        printf("Specified address range (0x%08X + %d bytes) is not in device FLASH nor RAM.\n", ui32StartAddress, ui32ByteCount);
        return (SBL_ARGUMENT_ERROR);
    }

    if(get_filed() < 0)
        return (SBL_PORT_ERROR);

    /* Set progress */
    setProgress(0);

    //
    // Build payload
    // - 4B address (MSB first)
    // - 4B byte count(MSB first)
    //
    ulToCharArray(ui32StartAddress, (uint8_t*)&pcPayload[0]);
    ulToCharArray(ui32ByteCount, (uint8_t*)&pcPayload[4]);
    pcPayload[8] = 0x00;
    pcPayload[9] = 0x00;
    pcPayload[10] = 0x00;
    pcPayload[11] = 0x00;

    /* Send command */
    if((retCode = sendCmd(CMD_CRC32, (const uint8_t*)pcPayload, 12)) != SBL_SUCCESS)
        return (retCode);

    /* Receive command response (ACK/NAK) */
    if((retCode = getCmdResponse(&bSuccess, 5)) != SBL_SUCCESS)
        return (retCode);

    if(!bSuccess)
    {
        printf("Device NAKed CRC32 command.\n");
        return (SBL_ERROR);
    }

    /* Get data response */
    ui32RecvCount = 4;
    if((retCode = getResponseData((uint8_t*)pcPayload, &ui32RecvCount, 0)) != SBL_SUCCESS)
    {
        sendCmdResponse(false);
        return (retCode);
    }

    *pui32Crc = charArrayToUL(pcPayload);

    /* Send ACK/NAK to command */
    bool bAck = (ui32RecvCount == 4) ? true : false;
    sendCmdResponse(bAck);

    /* Set progress */
    setProgress(100);

    return SBL_SUCCESS;
}

/****************************************************************
 * Function Name : detectAutoBaud
 * Description   : Detect the baud rate
 * Returns       : SBL_SUCCESS ...
 * Params        : None.
 ****************************************************************/
tSblStatus detectAutoBaud(void)
{
    uint8_t wrPkt[2] = {0x55, 0x55};
    uint8_t rdPkt[2] = {0, 0};
    if(serialWrite(wrPkt, 2) != 2)
        return (SBL_ERROR);

    if(serialRead(rdPkt, 2) != 2)
        return(SBL_ERROR);

    if(rdPkt[0] == 0x00 && rdPkt[1] == 0xCC)
      {
          printf("Auto baud detected 0x%02X 0x%02X.\n", rdPkt[0], rdPkt[1]);
          return (SBL_SUCCESS);
      }
      else if(rdPkt[0] == 0x00 && rdPkt[1] == 0x33)
      {
          printf("NACK received 0x%02X 0x%02X.\n", rdPkt[0], rdPkt[1]);
          return (SBL_SUCCESS);
      }
      else
      {
          printf("ACK/NAK not received. Expected 0x00 0xCC or 0x00 0x33, received 0x%02X 0x%02X.\n", rdPkt[0], rdPkt[1]);
          return (SBL_ERROR);
      }
}
