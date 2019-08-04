/*
 * Linux_Serial.h
 *
 *  Created on: 25/06/2019
 *  Author: vinay divakar
 */

#ifndef LINUX_SERIAL_H_
#define LINUX_SERIAL_H_
#include <stdint.h>

typedef enum {
    B_9600,
    B_115200
}bautSet_t;

extern int openPort(const char *port);
extern int closePort();
extern void configPort(void);
extern int serialWrite(uint8_t *wrPtr, uint8_t wrDataLen);
extern int serialRead(uint8_t *rdPtr, uint8_t rdDataLen);
extern int get_filed(void);

#endif /* LINUX_SERIAL_H_ */
