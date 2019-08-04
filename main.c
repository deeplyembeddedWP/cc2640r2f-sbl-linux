/*
 ============================================================================
 Name        : Linux CC26x0 SBL
 Author      : Vinay Divakar
 Version     : 1.0
 Description : Serial Boot Loader (SBL) main entry.
 Reference   : sblAppEx_1_02_00_00 VC++ SBL provided by TI
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

/* Custom Includes */
#include "Linux_Serial.h"
#include "sbl_device.h"
#include "sbl_device_cc2640.h"
#include "myFile.h"

/* read only variables */
const char *portName = NULL;
const char *filename = NULL; //Path of .bin to be flashed
static FILE *fPtr = NULL;

/* CMD line cases */
enum cmdArgs{
    ONE = 1,
    TWO = 2,
    THREE = 3
};


int main(int argc, char **argv)
{
    printf("\n+-----------------------------------------------------------------------------------------------\n");
    printf("Serial Bootloader Library for cc26x0\n");
    printf("Platform: Linux                     \n");
    printf("Compiler: GCC                       \n");
    printf("+-----------------------------------------------------------------------------------------------\n\n");

    /* Do some initial command line checks */
    switch(argc)
    {

    case THREE:
        portName = argv[1];
        filename = argv[2];
        printf("SBL Port i/p: %s\r\n", portName);
        printf("Firmware i/p: %s\r\n\n", filename);
        printf("All Good :)\r\n");
        break;

    default:
        printf("INVALID ARG'S...EXITING :(\r\n");
        exit(EXIT_FAILURE);
        break;
    }



    uint8_t *memPtr = NULL;         /* Ptr to hold read data */
    long fileSz = 0;                /* Holds the size of the .bin file, in bytes */
    uint32_t fileCrc, devCrc;       /* Variables to save CRC checksum */
    uint32_t tmp = 0;
    bool ackChk = false;

    /* Open the port */
    openPort(portName);

    /* Configure port */
    configPort();

    /* Setup callbacks */
    setupCallbacks();

    /* Set flash base for cc2640 */
    setDeviceFlashBase(CC26XX_FLASH_BASE);

    /* Detect baud rate */
    if(detectAutoBaud() != SBL_SUCCESS)
    {
        printf("ERROR: baud detect  failed\n");
        closePort();
        exit(EXIT_FAILURE);
    }
    else
        printf("Baudrate detected !\n");

    /* Check if the host is reachable */
    if(ping() != SBL_SUCCESS)
    {
        printf("ERROR: Host unreachable\n");
        closePort();
        exit(EXIT_FAILURE);
    }
    else
        printf("PING: Host detected !\n");

    if(readFlashSize(&tmp) != SBL_SUCCESS)
    {
        printf("ERROR: Unable to read flash size\n");
        closePort();
        exit(EXIT_FAILURE);
    }
    else
        printf("Flash size: %u\n",getFlashSize());

    if(readRamSize(&tmp) != SBL_SUCCESS)
    {
        printf("ERROR: Unable to read RAM size\n");
        closePort();
        exit(EXIT_FAILURE);
    }
    else
        printf("RAM size: %u\n",getRamSize());

    /* Open the file */
    if((fPtr = openFile(filename)) == NULL)
    {
        printf("ERROR: opening file\n");
        closePort();
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("FILE open OK\n");
        if(!(fileSz = getFileSize(fPtr)))
        {
            printf("ERROR: getting file size\n");
            closePort();
            closeFile(fPtr);
            exit(EXIT_FAILURE);
        }
        else
        {
            printf("FILE size OK, fileSz: %lu\n",fileSz);

            /* Allocate memory to read the file */
            memPtr = (uint8_t*)calloc(fileSz, sizeof(uint8_t));
            if(memPtr)
            {
                uint32_t fileRd = 0;

                printf("CALLOC OK\n");
                if((fileRd = fread(memPtr,1 , fileSz, fPtr)) != fileSz)
                {
                    printf("ERROR: File read failed\n");
                    closePort();
                    closeFile(fPtr);
                    exit(EXIT_FAILURE);
                }
                else
                    printf("FILE read OK, fileRd: %u \n", fileRd);
            }
            else
            {
                printf("ERROR: calloc failed\n");
                closePort();
                closeFile(fPtr);
                exit(EXIT_FAILURE);
            }
        }
    }// End of all file operations

    /* Calculate file CRC checksum */
    fileCrc = calcCrcLikeChip(memPtr, fileSz);
    printf("fileCrc: %u\n", fileCrc);

    /* Erase as much flash needed to program the new firmware */
    printf("Erasing flash ...\n");
    if(eraseFlashRange(getDeviceFlashBase(), fileSz) != SBL_SUCCESS)
    {
        printf("ERROR: Erase failed\n");
        free(memPtr);
        closePort();
        closeFile(fPtr);
        exit(EXIT_FAILURE);
    }
    else
        printf("ERASE OK\n");

    /* Write file to device flash memory */
    printf("Writing flash ...\n");
    if(writeFlashRange(getDeviceFlashBase(), fileSz, (char*)memPtr) != SBL_SUCCESS)
    {
        printf("ERROR: Write failed\n");
        free(memPtr);
        closePort();
        closeFile(fPtr);
        exit(EXIT_FAILURE);
    }
    else
        printf("WRITE OK\n");

    /* Calculate CRC checksum of flashed content */
    printf("Calculating CRC of flashed content ...\n");
    if(calculateCrc32(getDeviceFlashBase(), fileSz, &devCrc) != SBL_SUCCESS)
    {
        printf("ERROR: CRC failed\n");
        free(memPtr);
        closePort();
        closeFile(fPtr);
        exit(EXIT_FAILURE);
    }
    printf("devCrc: %u\n", devCrc);

    /* Comparing the CRC checksums */
    if(fileCrc == devCrc)
    {
        printf("CRC OK, devCrc = fileCrc = %u\n", fileCrc);
    }
    else
    {
        printf("ERROR: CRC mismatch!\n");
        free(memPtr);
        closePort();
        closeFile(fPtr);
        exit(EXIT_FAILURE);
    }

    /* Reset the device */
    if(reset() != SBL_SUCCESS)
    {
        printf("ERROR: RST failed!\n");
        free(memPtr);
        closePort();
        closeFile(fPtr);
        exit(EXIT_FAILURE);
    }
    else
        printf("RST OK\n");

    /* If we got here, means all succeeded */
    printf("+-----------------------------------\n");
    printf("CC2640 FIRMWARE UPGRADE COMPLETED !-\n");
    printf("+-----------------------------------\n\n");

    /* De-allocate the memory */
    free(memPtr);

    /* Close all */
    closeFile(fPtr);
    closePort();

    /* exit on success */
    exit(EXIT_SUCCESS);
}
