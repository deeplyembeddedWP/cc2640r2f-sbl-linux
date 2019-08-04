/*
 * sbl_device_cc2640.h
 *
 *  Created on: 25/06/2019
 *      Author: vinaydivakar
 */

#ifndef SBL_DEVICE_H_
#define SBL_DEVICE_H_
#include <stdbool.h>
#include "Linux_Serial.h"

typedef enum {
    CMD_PING             = 0x20,
    CMD_DOWNLOAD         = 0x21,
    CMD_GET_STATUS       = 0x23,
    CMD_SEND_DATA        = 0x24,
    CMD_RESET            = 0x25,
    CMD_SECTOR_ERASE     = 0x26,
    CMD_CRC32            = 0x27,
    CMD_GET_CHIP_ID      = 0x28,
    CMD_MEMORY_READ      = 0x2A,
    CMD_MEMORY_WRITE     = 0x2B,
    CMD_BANK_ERASE       = 0x2C,
    CMD_SET_CCFG         = 0x2D,
}cmd_t;


typedef enum {
    SBL_SUCCESS = 0,
    SBL_ERROR,
    SBL_ARGUMENT_ERROR,
    SBL_TIMEOUT_ERROR,
    SBL_PORT_ERROR,
    SBL_ENUM_ERROR,
    SBL_UNSUPPORTED_FUNCTION,
    SBL_MALLOC_ERROR
} tSblStatus;

/* Early samples had different command IDs */
typedef enum
{
    REV1_CMD_BANK_ERASE   = 0x2A,
    REV1_CMD_SET_CCFG     = 0x2B,
    REV1_CMD_MEMORY_READ  = 0x2C,
    REV1_CMD_MEMORY_WRITE = 0x2D,
}cmdRev1_t;

typedef enum {
    CMD_RET_SUCCESS      = 0x40,
    CMD_RET_UNKNOWN_CMD  = 0x41,
    CMD_RET_INVALID_CMD  = 0x42,
    CMD_RET_INVALID_ADR  = 0x43,
    CMD_RET_FLASH_FAIL   = 0x44,
}cmdRespStatus_t;

extern tSblStatus sendAutoBaud(bool *bBaudSetOk);
extern tSblStatus getCmdResponse(bool *bAck, uint8_t maxRetries);
extern tSblStatus sendCmdResponse(bool bAck);
extern tSblStatus getResponseData(uint8_t *pcData, uint32_t *ui32MaxLen,
                                  uint32_t ui32MaxRetries);
extern uint8_t generateCheckSum(cmd_t cmdType, const char *pcData,
                                      uint32_t ui32DataLen);
extern uint32_t calcCrcLikeChip(const uint8_t *pData, uint32_t ulByteCount);
extern tSblStatus setProgress(uint32_t ui32Progress);
extern tSblStatus sendCmd(cmd_t cmdType, const uint8_t *pcSendData/* = NULL*/,
                   uint32_t ui32SendLen/* = 0*/);
extern tSblStatus readStatus(uint32_t *pui32Status);
extern char *getCmdStatusString(cmdRespStatus_t ui32Status);
extern char *getCmdString(cmd_t ui32Cmd);
extern void byteSwap(uint8_t *pcArray);
extern void ulToCharArray(const uint32_t ui32Src, uint8_t *pcDst);
extern uint32_t charArrayToUL(const char *pcSrc);
extern void setupCallbacks(void);
//
// Typedefs for callback functions to report status and progress to application
//
typedef void (*tStatusFPTR)(char *pcText, bool bError);
typedef void (*tProgressFPTR)(uint32_t ui32Value);


#endif /* SBL_DEVICE_H_ */
