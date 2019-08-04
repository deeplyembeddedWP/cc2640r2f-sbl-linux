/*
 * Linux_Serial.c
 *
 *  Created on: 25/06/2019
 *  Author: vinay divakar
 *  Description: Linux based serial port functions
 */

#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */

/* Custom Includes */
#include "Linux_Serial.h"

/* Static variables */
static int fd = 0;
static struct termios SerialPortSettings;

/* Static functions */
static void setBaudRate(bautSet_t baud);

/****************************************************************
 * Function Name : openPort
 * Description   : Opens the serial port
 * Returns       : 0 on success, -1 on failure
 * Params        @port: Path to the serial port
 ****************************************************************/
int openPort(const char *port)
{
    if((fd = open(port, O_RDWR | O_NOCTTY)) < 0)
        perror("USB: ERROR OPENING PORT |");
    else
        printf("USB: PORT OPEN SUCCESSFUL !\r\n");
    return(fd);
}

/****************************************************************
 * Function Name : closePort
 * Description   : Closes the serial port
 * Returns       : 0 on success, -1 on failure
 * Params        @fd: file descriptor
 ****************************************************************/
int closePort()
{
    int rc = 0;
    if((rc = close(fd)) < 0)
        perror("USB: ERROR CLOSING PORT |");
    else
        printf("USB: PORT CLOSED SUCCESSFUL !\r\n");
    return(rc);
}
/****************************************************************
 * Function Name : clearRxbuffer
 * Description   : Discards old data in the buffer
 * Returns       : None
 * Params        @None
 ****************************************************************/
void clearRxbuffer(void)
{
    /* This sleep is required for the flush to work
     * properly, seems like a bug in the kernel. For
     * more info refer to the link below:
     * https://stackoverflow.com/questions/13013387/clearing-the-serial-ports-buffer
     */
    sleep(1);
    tcflush(fd, TCIOFLUSH);
}

/****************************************************************
 * Function Name : setBaudRate
 * Description   : Set the baud rate
 * Returns       : None
 * Params        @baud: Baudrate
 ****************************************************************/
static void setBaudRate(bautSet_t baud)
{
    switch(baud)
    {
    case B_9600:
        cfsetispeed(&SerialPortSettings,B9600);
        cfsetospeed(&SerialPortSettings,B9600);
        break;
    case B_115200:
        cfsetispeed(&SerialPortSettings,B115200);
        cfsetospeed(&SerialPortSettings,B115200);
        break;
    default:
        printf("ERROR: INVALID BAUD\n");
        break;
    }
}

/****************************************************************
 * Function Name : configPort
 * Description   : Populate the termios structure
 * Returns       : None
 * Params        @None
 ****************************************************************/
void configPort(void)
{
    memset(&SerialPortSettings, 0, sizeof(SerialPortSettings));
    setBaudRate(B_115200);

    SerialPortSettings.c_cflag |= (CLOCAL | CREAD);
    SerialPortSettings.c_cflag &= ~CSIZE;
    SerialPortSettings.c_cflag |= CS8;
    SerialPortSettings.c_cflag &= ~PARENB;
    SerialPortSettings.c_cflag &= ~CSTOPB;
    SerialPortSettings.c_cflag &= ~CRTSCTS;

    /* setup for non-canonical mode */
    SerialPortSettings.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    SerialPortSettings.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    SerialPortSettings.c_oflag &= ~OPOST;

    /* fetch bytes as they become available */
    SerialPortSettings.c_cc[VMIN] = 0;
    SerialPortSettings.c_cc[VTIME] = 2;

    /* Flush out if there is any previously pending shit* */
    clearRxbuffer();

    if((tcsetattr(fd,TCSANOW,&SerialPortSettings)) != 0)
        perror("ERROR in Setting attributes |");
    else
        printf("\n  BaudRate = 115200 \n  StopBits = 1 \n  Parity   = none\n\n");
}

/****************************************************************
 * Function Name : serialWrite
 * Description   : Write bytes on Tx
 * Returns       : Number of bytes written
 * Params        @dataPtr: Pointer to the buffer to be written
 *               @dataLen: Length of the data
 ****************************************************************/
int serialWrite(uint8_t *wrPtr, uint8_t wrDataLen)
{
    int wrbytes = write(fd, wrPtr, wrDataLen);
    /* Be patient until everything is pumped out */
    tcdrain(fd);
    return(wrbytes);
}

/****************************************************************
 * Function Name : serialRead
 * Description   : Reads bytes on the RX
 * Returns       : Number of bytes read
 * Params        @dataPtr: Pointer to the buffer to be populated
 *               @dataLen: Length of the data
 ****************************************************************/
int serialRead(uint8_t *rdPtr, uint8_t rdDataLen)
{
    int rdbytes = read(fd, rdPtr, rdDataLen);
    return(rdbytes);
    /* If read does not return, we are Fuc*** !!!,
     * but should do unless the BL goes numb----*/
}

/****************************************************************
 * Function Name : get_filed
 * Description   : Returns fd
 * Returns       : None
 * Params        @None
 ****************************************************************/
int get_filed(void)
{
    return(fd);
}


