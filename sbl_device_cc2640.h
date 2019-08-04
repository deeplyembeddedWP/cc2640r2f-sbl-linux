/*
 * sbl_device_cc2650.h
 *
 *  Created on: 26/06/2019
 *  Author: vinay divakar
 */

#ifndef SBL_DEVICE_CC2640_H_
#define SBL_DEVICE_CC2640_H_

#include "sbl_device.h"
#include "Linux_Serial.h"

#define CC26XX_FLASH_BASE                   0x00000000
#define SBL_CC2650_PAGE_ERASE_SIZE          4096
#define SBL_CC2650_FLASH_START_ADDRESS      0x00000000
#define SBL_CC2650_RAM_START_ADDRESS        0x20000000
#define SBL_CC2650_ACCESS_WIDTH_32B         1
#define SBL_CC2650_ACCESS_WIDTH_8B          0
#define SBL_CC2650_PAGE_ERASE_TIME_MS       20
#define SBL_CC2650_MAX_BYTES_PER_TRANSFER   252
#define SBL_CC2650_MAX_MEMWRITE_BYTES       247
#define SBL_CC2650_MAX_MEMWRITE_WORDS       61
#define SBL_CC2650_MAX_MEMREAD_BYTES        253
#define SBL_CC2650_MAX_MEMREAD_WORDS        63
#define SBL_CC2650_FLASH_SIZE_CFG           0x4003002C
#define SBL_CC2650_RAM_SIZE_CFG             0x40082250
#define SBL_CC2650_BL_CONFIG_PAGE_OFFSET    0xFDB
#define SBL_CC2650_BL_CONFIG_ENABLED_BM     0xC5
#define SBL_CC2650_BL_WORK_MEMORY_START     0x20000000
#define SBL_CC2650_BL_WORK_MEMORY_END       0x2000016F
#define SBL_CC2650_BL_STACK_MEMORY_START    0x20000FC0
#define SBL_CC2650_BL_STACK_MEMORY_END      0x20000FFF

extern tSblStatus eraseFlashBank();
extern tSblStatus ping();
extern tSblStatus reset();
extern void setDeviceFlashBase(uint32_t valFlashBase);
extern uint32_t getDeviceFlashBase();
extern uint32_t getFlashSize();
extern uint32_t getRamSize();
extern tSblStatus writeFlashRange(uint32_t ui32StartAddress,
                           uint32_t ui32ByteCount, const char *pcData);
extern tSblStatus eraseFlashRange(uint32_t ui32StartAddress,
                              uint32_t ui32ByteCount);
extern tSblStatus calculateCrc32(uint32_t ui32StartAddress,
                                 uint32_t ui32ByteCount, uint32_t *pui32Crc);
extern tSblStatus detectAutoBaud(void);
extern tSblStatus readFlashSize(uint32_t *pui32FlashSize);
extern tSblStatus readRamSize(uint32_t *pui32RamSize);

#endif /* SBL_DEVICE_CC2640_H_ */
