/*
 * myFile.c
 *
 *  Created on: 28/06/2019
 *  Author: vinay divakar
 *  Description: File operations needed by the SBL
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

/****************************************************************
 * Function Name : openFile
 * Description   : Opens the file
 * Returns       : NULL on failure
 * Params        @file: Path to the file to be opened
 ****************************************************************/
FILE *openFile(const char *file)
{
    FILE *fp = fopen(file, "rb");
    return(fp);
}

/****************************************************************
 * Function Name : getFileSize
 * Description   : Gets the size of the file to be read
 * Returns       : 0 on failure
 * Params        @fp: File descriptor
 ****************************************************************/
long int getFileSize(FILE *fp)
{
    /* Go to end of file */
    if(fseek(fp, 0L, SEEK_END))
        return (0);

    /* Get the size */
    long int sz = ftell(fp);

    /* Put the curser back to 0 */
    if(fseek(fp,0L,SEEK_SET))
        return (0);
    return sz;
}

/****************************************************************
 * Function Name : closeFile
 * Description   : closes the file
 * Returns       : EOF on failure
 * Params        @fp: file descriptor
 ****************************************************************/
int closeFile(FILE *fp)
{
    return(fclose(fp));
}

