/*
 * myFile.h
 *
 *  Created on: 28/06/2019
 *      Author: vinaydivakar
 */

#ifndef MYFILE_H_
#define MYFILE_H_
#include <stdio.h>

extern FILE *openFile(const char *file);
extern int closeFile(FILE *fp);
extern long int getFileSize(FILE *fp);

#endif /* MYFILE_H_ */
