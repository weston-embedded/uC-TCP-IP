/*
*********************************************************************************************************
*                                              uC/TCP-IP
*                                      The Embedded TCP/IP Suite
*
*                    Copyright 2004-2021 Silicon Laboratories Inc. www.silabs.com
*
*                                 SPDX-License-Identifier: APACHE-2.0
*
*               This software is subject to an open source license and is distributed by
*                Silicon Laboratories Inc. pursuant to the terms of the Apache License,
*                    Version 2.0 available at www.apache.org/licenses/LICENSE-2.0.
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                   NETWORK FILE SYSTEM PORT LAYER
*
*                                              TEMPLATE
*
* Filename : net_fs_template.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes the following versions (or more recent) of software modules are included in
*                the project build :
*
*                (a) uC/FS  V4.03
*                (b) uC/Clk V3.09          if uC/FS V4.04 (or more recent) is included
*
*            (2) Should be compatible with the following TCP/IP application versions (or more recent):
*
*                (a) uC/FTPc  V1.93.01
*                (b) uC/FTPs  V1.95.02
*                (c) uC/HTTPs V1.98.00
*                (d) uC/TFTPc V1.92.01
*                (e) uC/TFTPs V1.91.02
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    NET_FS_TEMPLATE_MODULE
#include  "..\net_fs.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetFS_WorkingFolderGet(CPU_CHAR    *p_path,
                                            CPU_SIZE_T   path_len_max);

static  CPU_BOOLEAN  NetFS_WorkingFolderSet(CPU_CHAR    *p_path);


/*
*********************************************************************************************************
*                                           FILE SYSTEM API
*
* Note(s) : (1) File system API structures are used by network applications during calls.  This API structure
*               allows network application to call specific file system functions via function pointer instead
*               of by name.  This enables the network application suite to compile & operate with multiple
*               file system.
*
*           (2) In most cases, the API structure provided below SHOULD suffice for most network application
*               exactly as is with the exception that the API structure's name which MUST be unique &
*               SHOULD clearly identify the file system being implemented.  For example, the Micrium file system
*               V4's API structure should be named NetFS_API_FS_V4[].
*
*               The API structure MUST also be externally declared in the File system port header file
*               ('net_fs_&&&.h') with the exact same name & type.
*********************************************************************************************************
*/
                                                                        /* Net FS static API fnct ptrs :                */
const  NET_FS_API  NetFS_API_Template = {
                                          NetFS_CfgPathGetLenMax,       /*   Path max len.                              */
                                          NetFS_CfgPathGetSepChar,      /*   Path sep char.                             */
                                          NetFS_FileOpen,               /*   Open                                       */
                                          NetFS_FileClose,              /*   Close                                      */
                                          NetFS_FileRd,                 /*   Rd                                         */
                                          NetFS_FileWr,                 /*   Wr                                         */
                                          NetFS_FilePosSet,             /*   Set position                               */
                                          NetFS_FileSizeGet,            /*   Get size                                   */
                                          NetFS_DirOpen,                /*   Open  dir                                  */
                                          NetFS_DirClose,               /*   Close dir                                  */
                                          NetFS_DirRd,                  /*   Rd    dir                                  */
                                          NetFS_EntryCreate,            /*   Entry create                               */
                                          NetFS_EntryDel,               /*   Entry del                                  */
                                          NetFS_EntryRename,            /*   Entry rename                               */
                                          NetFS_EntryTimeSet,           /*   Entry set time                             */
                                          NetFS_FileDateTimeCreateGet,  /*   Create date time                           */
                                          NetFS_WorkingFolderGet,       /*   Get working folder                         */
                                          NetFS_WorkingFolderSet,       /*   Set working folder                         */
                                        };


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          NetFS_CfgPathGetLenMax()
*
* Description : Get maximum path lenght
*
* Argument(s) : none.
*
* Return(s)   : maximum path len.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  NetFS_CfgPathGetLenMax (void)
{
    /* $$$$ Insert code to return the maximum path length. */
}


/*
*********************************************************************************************************
*                                          NetFS_CfgPathGetSepChar()
*
* Description : Get path separator character
*
* Argument(s) : none.
*
* Return(s)   : separator charater.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_CHAR  NetFS_CfgPathGetSepChar (void)
{
    /* $$$$ Insert code to return the path separator character. */
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         DIRECTORY FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          NetFS_DirOpen()
*
* Description : Open a directory.
*
* Argument(s) : pname       Name of the directory.
*
* Return(s)   : Pointer to a directory, if NO errors.
*               Pointer to NULL,        otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  *NetFS_DirOpen (CPU_CHAR  *pname)
{
    void  *pdir;


    /* $$$$ Insert code to open a directory.    */
    pdir = (void *)0;

    return (pdir);
}


/*
*********************************************************************************************************
*                                         NetFS_DirClose()
*
* Description : Close a directory.
*
* Argument(s) : pdir        Pointer to a directory.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetFS_DirClose (void  *pdir)
{
    /* $$$$ Insert code to close a directory.   */
}


/*
*********************************************************************************************************
*                                           NetFS_DirRd()
*
* Description : Read a directory entry from a directory.
*
* Argument(s) : pdir        Pointer to a directory.
*
*               pentry      Pointer to variable that will receive directory entry information.
*
* Return(s)   : DEF_OK,   if directory entry read.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_DirRd (void          *pdir,
                          NET_FS_ENTRY  *pentry)
{
    /* $$$$ Insert code to read entry from a directory.     */


    /* $$$$ Insert code to populate pentry.                 */


    return (DEF_FAIL);
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           ENTRY FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                        NetFS_EntryCreate()
*
* Description : Create a file or directory.
*
* Argument(s) : pname       Name of the entry.
*
*               dir         Indicates whether the new entry shall be a directory :
*
*                               DEF_YES, if the entry shall be a directory.
*                               DEF_NO,  if the entry shall be a file.
*
* Return(s)   : DEF_OK,   if entry created.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_EntryCreate (CPU_CHAR     *pname,
                                CPU_BOOLEAN   dir)
{
    /* $$$$ Insert code to create a file or a directory.    */


    return (DEF_FAIL);
}


/*
*********************************************************************************************************
*                                         NetFS_EntryDel()
*
* Description : Delete a file or directory.
*
* Argument(s) : pname       Name of the entry.
*
*               file        Indicates whether the entry MAY be a file :
*
*                               DEF_YES, if the entry may     be a file.
*                               DEF_NO,  if the entry may NOT be a file.
*
* Return(s)   : DEF_OK,   if entry created.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_EntryDel (CPU_CHAR     *pname,
                             CPU_BOOLEAN   file)
{
    /* $$$$ Insert code to delete a file or a directory.    */


    return (DEF_FAIL);
}


/*
*********************************************************************************************************
*                                        NetFS_EntryRename()
*
* Description : Rename a file or directory.
*
* Argument(s) : pname_old   Old path of the entry.
*
*               pname_new   New path of the entry.
*
* Return(s)   : DEF_OK,   if entry renamed.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_EntryRename (CPU_CHAR  *pname_old,
                                CPU_CHAR  *pname_new)
{
    /* $$$$ Insert code to rename a file or a directory.    */


    return (DEF_FAIL);
}


/*
*********************************************************************************************************
*                                       NetFS_EntryTimeSet()
*
* Description : Set a file or directory's date/time.
*
* Argument(s) : pname       Name of the entry.
*
*               ptime       Pointer to date/time.
*
* Return(s)   : DEF_OK,   if date/time set.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_EntryTimeSet (CPU_CHAR           *pname,
                                 NET_FS_DATE_TIME  *ptime)
{
    /* $$$$ Insert code to set a file or directory's date/time.     */


    return (DEF_FAIL);
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           FILE FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         NetFS_FileOpen()
*
* Description : Open a file.
*
* Argument(s) : pname       Name of the file.
*
*               mode        Mode of the file :
*
*                               NetFS_FILE_MODE_APPEND        Open existing file at end-of-file OR create new file.
*                               NetFS_FILE_MODE_CREATE        Create new file OR overwrite existing file.
*                               NetFS_FILE_MODE_CREATE_NEW    Create new file OR return error if file exists.
*                               NetFS_FILE_MODE_OPEN          Open existing file.
*                               NetFS_FILE_MODE_TRUNCATE      Truncate existing file to zero length.
*
*               access      Access rights of the file :
*
*                               NetFS_FILE_ACCESS_RD          Open file in read           mode.
*                               NetFS_FILE_ACCESS_RD_WR       Open file in read AND write mode.
*                               NetFS_FILE_ACCESS_WR          Open file in          write mode
*
* Return(s)   : Pointer to a file, if NO errors.
*               Pointer to NULL,   otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  *NetFS_FileOpen (CPU_CHAR            *pname,
                       NET_FS_FILE_MODE     mode,
                       NET_FS_FILE_ACCESS   access)
{
    /* $$$$ Insert code to open a file.     */
}


/*
*********************************************************************************************************
*                                         NetFS_FileClose()
*
* Description : Close a file.
*
* Argument(s) : pfile       Pointer to a file.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetFS_FileClose (void  *pfile)
{
    /* $$$$ Insert code to close a file.    */
}


/*
*********************************************************************************************************
*                                          NetFS_FileRd()
*
* Description : Read from a file.
*
* Argument(s) : pfile       Pointer to a file.
*
*               pdest       Pointer to destination buffer.
*
*               size        Number of octets to read.
*
*               psize_rd    Pointer to variable that will receive the number of octets read.
*
* Return(s)   : DEF_OK,   if no error occurred during read (see Note #1).
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) If the read request could not be fulfilled because the EOF was reached, the return
*                   value should be 'DEF_OK'.  The application should compare the value in 'psize_rd' to
*                   the value passed to 'size' to detect an EOF reached condition.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_FileRd (void        *pfile,
                           void        *pdest,
                           CPU_SIZE_T   size,
                           CPU_SIZE_T  *psize_rd)
{
    /* $$$$ Insert code to read from a file.    */


   *psize_rd = 0;
    return (DEF_FAIL);
}


/*
*********************************************************************************************************
*                                          NetFS_FileWr()
*
* Description : Write to a file.
*
* Argument(s) : pfile       Pointer to a file.
*
*               psrc        Pointer to source buffer.
*
*               size        Number of octets to write.
*
*               psize_wr    Pointer to variable that will receive the number of octets written.
*
* Return(s)   : DEF_OK,   if no error occurred during write.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_FileWr (void        *pfile,
                           void        *psrc,
                           CPU_SIZE_T   size,
                           CPU_SIZE_T  *psize_wr)
{
    /* $$$$ Insert code to write to a file.     */


   *psize_wr = 0;
    return (DEF_FAIL);
}


/*
*********************************************************************************************************
*                                        NetFS_FilePosSet()
*
* Description : Set file position indicator.
*
* Argument(s) : pfile       Pointer to a file.
*
*               offset      Offset from the file position specified by 'origin'.
*
*               origin      Reference position for offset :
*
*                               NetFS_SEEK_ORIGIN_START    Offset is from the beginning of the file.
*                               NetFS_SEEK_ORIGIN_CUR      Offset is from current file position.
*                               NetFS_SEEK_ORIGIN_END      Offset is from the end       of the file.
*
* Return(s)   : DEF_OK,   if file position set.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_FilePosSet (void        *pfile,
                               CPU_INT32S   offset,
                               CPU_INT08U   origin)
{
    /* $$$$ Insert code to set file position indicator.     */


    return (DEF_FAIL);
}


/*
*********************************************************************************************************
*                                        NetFS_FileSizeGet()
*
* Description : Get file size.
*
* Argument(s) : pfile       Pointer to a file.
*
*               psize       Pointer to variable that will receive the file size.
*
* Return(s)   : DEF_OK,   if file size gotten.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_FileSizeGet (void        *pfile,
                                CPU_INT32U  *psize)
{
    /* $$$$ Insert code to get file size.     */


    return (DEF_FAIL);
}


/*
*********************************************************************************************************
*                                   NetFS_FileDateTimeCreateGet()
*
* Description : Get file creation date/time.
*
* Argument(s) : pfile       Pointer to a file.
*
*               ptime       Pointer to variable that will receive the date/time :
*
*                               Default/epoch date/time,        if any error(s);
*                               Current       date/time,        otherwise.
*
* Return(s)   : DEF_OK,   if file creation date/time gotten.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) Pointers to variables that return values MUST be initialized PRIOR to all other
*                   validation or function handling in case of any error(s).
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_FileDateTimeCreateGet (void               *pfile,
                                          NET_FS_DATE_TIME  *ptime)
{
    /* $$$$ Insert code to get file time of creation.     */


    return (DEF_FAIL);
}


/*
*********************************************************************************************************
*                                      NetFS_WorkingFolderGet()
*
* Description : Get current working folder.
*
* Argument(s) : p_path  String buffer that will receive the working directory path.
*
* Return(s)   : DEF_OK,   if file creation date/time gotten.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) Pointers to variables that return values MUST be initialized PRIOR to all other
*                   validation or function handling in case of any error(s).
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetFS_WorkingFolderGet (CPU_CHAR    *p_path,
                                             CPU_SIZE_T   path_len_max)
{
    /* $$$$ Insert code to get the working folder.      */

    return (DEF_FAIL);
}


/*
*********************************************************************************************************
*                                      NetFS_WorkingFolderSet()
*
* Description : Set current working folder.
*
* Argument(s) : p_path  String that specifies EITHER...
*                           (a) the absolute working directory path to set;
*                           (b) a relative path that will be applied to the current working directory.
*
* Return(s)   : DEF_OK,   if file creation date/time gotten.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) Pointers to variables that return values MUST be initialized PRIOR to all other
*                   validation or function handling in case of any error(s).
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetFS_WorkingFolderSet (CPU_CHAR    *p_path)
{
    /* $$$$ Insert code to set the working folder.  */

    return (DEF_FAIL);
}


/*
*********************************************************************************************************
*                                           NetFS_fopen()
*
* Description : Open a file.
*
* Argument(s) : name_full   Name of the file.
*
*               str_mode    Access mode of the file (see Note #1a).
*
* Return(s)   : Pointer to a file, if NO errors.
*               Pointer to NULL,   otherwise.
*
* Caller(s)   : Application.
*
*               This function is a file system suite application interface (API) function & MAY be called
*               by application function(s).
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'fopen() : DESCRIPTION' states that :
*
*                   (a) "If ['str_mode'] is one of the following, the file is open in the indicated mode." :
*
*                           "r or rb           Open file for reading."
*                           "w or wb           Truncate to zero length or create file for writing."
*                           "a or ab           Append; open and create file for writing at end-of-file."
*                           "r+ or rb+ or r+b  Open file for update (reading and writing)."
*                           "w+ or wb+ or w+b  Truncate to zero length or create file for update."
*                           "a+ or ab+ or a+b  Append; open or create for update, writing at end-of-file.
*
*                   (b) "The character 'b' shall have no effect"
*
*                   (c) "Opening a file with read mode ... shall fail if the file does not exist or
*                        cannot be read"
*
*                   (d) "Opening a file with append mode ... shall cause all subsequent writes to the
*                        file to be forced to the then current end-of-file"
*
*                   (e) "When a file is opened with update mode ... both input and output may be performed....
*                        However, the application shall ensure that output is not directly followed by
*                        input without an intervening call to 'fflush()' or to a file positioning function
*                      ('fseek()', 'fsetpos()', or 'rewind()'), and input is not directly followed by output
*                        without an intervening call to a file positioning function, unless the input
*                        operation encounters end-of-file."
*
*               (2) IEEE Std 1003.1, 2004 Edition, Section 'fopen() : RETURN VALUE' states that "[u]pon
*                   successful completion 'fopen()' shall return a pointer to the object controlling the
*                   stream.  Otherwise a null pointer shall be returned'.
*********************************************************************************************************
*/

void  *NetFS_fopen (const  char  *name_full,
                    const  char  *str_mode)
{
    /* $$$$ Insert code to open a file. */
}


/*
*********************************************************************************************************
*                                           NetFS_fseek()
*
* Description : Set file position indicator.
*
* Argument(s) : p_file      Pointer to a file.
*
*               offset      Offset from file position specified by 'whence'.
*
*               origin      Reference position for offset :
*
*                               FS_SEEK_SET    Offset is from the beginning of the file.
*                               FS_SEEK_CUR    Offset is from current file position.
*                               FS_SEEK_END    Offset is from the end       of the file.
*
* Return(s)   :  0, if the function succeeds.
*               -1, otherwise.
*
* Caller(s)   : Application,
*               fs_rewind().
*
*               This function is a file system suite application interface (API) function & MAY be called
*               by application function(s).
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'fread() : DESCRIPTION' states that :
*
*                   (a) "If a read or write error occurs, the error indicator for the stream shall be set"
*
*                   (b) "The new position measured in bytes from the beginning of the file, shall be
*                        obtained by adding 'offset' to the position specified by 'whence'. The specified
*                        point is ..."
*
*                       (1) "... the beginning of the file                        for SEEK_SET"
*
*                       (2) "... the current value of the file-position indicator for SEEK_CUR"
*
*                       (3) "... end-of-file                                      for SEEK_END"
*
*                   (c) "A successful call to 'fseek()' shall clear the end-of-file indicator"
*
*                   (d) "The 'fseek()' function shall allow the file-position indicator to be set beyond
*                        the end of existing data in the file.  If data is later written at this point,
*                        subsequent reads of data in the gap shall return bytes with the value 0 until
*                        data is actually written into the gap."
*
*               (2) IEEE Std 1003.1, 2004 Edition, Section 'fread() : RETURN VALUE' states that "[t]he
*                   'fseek()' and 'fseeko()' functions shall return 0 if they succeeds.  Otherwise, they
*                   shall return -1".
*
*               (3) If the file position indicator is set beyond the file's current data, the file MUST
*                   be opened in write or read/write mode.
*********************************************************************************************************
*/

int  NetFS_fseek (void       *p_file,
                  long  int   offset,
                  int         origin)
{
    /* $$$$ Insert code to set file position indicator. */
}


/*
*********************************************************************************************************
*                                           NetFS_ftell()
*
* Description : Get file position indicator.
*
* Argument(s) : p_file     Pointer to a file.
*
* Return(s)   : The current file system position, if the function succeeds.
*               -1,                               otherwise.
*
* Caller(s)   : Application.
*
*               This function is a file system suite application interface (API) function & MAY be called
*               by application function(s).
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'ftell() : RETURN VALUE' states that :
*
*                   (a) "Upon successful completion, 'ftell()' and 'ftello()' shall return the current
*                        value of the file-position indicator for the stream measured in bytes from the
*                        beginning of the file."
*
*                   (b) "Otherwise, 'ftell()' and 'ftello()' shall return -1, cast to 'long' and 'off_t'
*                        respectively, and set errno to indicate the error."
*
*               (2) #### Check for overflow in cast.
*********************************************************************************************************
*/

long  int  NetFS_ftell (void  *p_file)
{
    /* $$$$ Insert code to get file position indicator. */
}


/*
*********************************************************************************************************
*                                           NetFS_rewind()
*
* Description : Reset file position indicator of a file.
*
* Argument(s) : p_file      Pointer to a file.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a file system suite application interface (API) function & MAY be called
*               by application function(s).
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'rewind() : DESCRIPTION' states that :
*
*                       "[T]he call 'rewind(stream)' shall be equivalent to '(void)fseek(stream, 0L, SEEK_SET)'
*                        except that 'rewind()' shall also clear the error indicator."
*********************************************************************************************************
*/


void  NetFS_rewind (void  *p_file)
{
    /* $$$$ Insert code to reset file position indicator of a file. */
}


/*
*********************************************************************************************************
*                                             fs_fread()
*
* Description : Read from a file.
*
* Argument(s) : p_dest       Pointer to destination buffer.
*
*               size        Size of each item to read.
*
*               nitems      Number of items to read.
*
*               p_file      Pointer to a file.
*
* Return(s)   : Number of items read.
*
* Caller(s)   : Application.
*
*               This function is a file system suite application interface (API) function & MAY be called
*               by application function(s).
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'fread() : DESCRIPTION' states that :
*
*                   (a) "The 'fread()' function shall read into the array pointed to by 'ptr' up to 'nitems'
*                        elements whose size is specified by 'size' in bytes"
*
*                   (b) "The file position indicator for the stream ... shall be advanced by the number of
*                        bytes successfully read"
*
*               (2) IEEE Std 1003.1, 2004 Edition, Section 'fread() : RETURN VALUE' states that "[u]pon
*                   completion, 'fread()' shall return the number of elements which is less than 'nitems'
*                   only if a read error or end-of-file is encountered".
*
*               (3) See 'fs_fopen()  Note #1e'.
*
*               (4) The file MUST have been opened in read or update (read/write) mode.
*
*               (5) If an error occurs while reading from the file, a value less than 'nitems' will
*                   be returned.  To determine whether the premature return was caused by reaching the
*                   end-of-file, the 'fs_feof()' function should be used :
*
*                       rtn = fs_fread(pbuf, 1, 1000, pfile);
*                       if (rtn < 1000) {
*                           eof = fs_feof();
*                           if (eof != 0) {
*                               // File has reached EOF
*                           } else {
*                               // Error has occurred
*                           }
*                       }
*
*               (6) #### Check for multiplication overflow.
*********************************************************************************************************
*/

CPU_SIZE_T  NetFS_fread ( void          *p_dest,
                          CPU_SIZE_T     size,
                          CPU_SIZE_T     nitems,
                          void          *p_file)
{
    /* $$$$ Insert code to read from a file.    */
}


/*
*********************************************************************************************************
*                                           NetFS_fwrite()
*
* Description : Write to a file.
*
* Argument(s) : p_src       Pointer to source buffer.
*
*               size        Size of each item to write.
*
*               nitems      Number of items to write.
*
*               p_file      Pointer to a file.
*
* Return(s)   : Number of items written.
*
* Caller(s)   : Application.
*
*               This function is a file system suite application interface (API) function & MAY be called
*               by application function(s).
*
* Note(s)     : (1) IEEE Std 1003.1, 2004 Edition, Section 'fwrite() : DESCRIPTION' states that :
*
*                   (a) "The 'fwrite()' function shall write, from the array pointed to by 'ptr', up to
*                       'nitems' elements whose size is specified by 'size', to the stream pointed to by
*                       'stream'"
*
*                   (b) "The file position indicator for the stream ... shall be advanced by the number of
*                        bytes successfully written"
*
*               (2) IEEE Std 1003.1, 2004 Edition, Section 'fwrite() : RETURN VALUE' states that
*                   "'fwrite()' shall return the number of elements successfully written, which may be
*                   less than 'nitems' if a write error is encountered".
*
*               (3) See 'fs_fopen()  Notes #1d & #1e'.
*
*               (4) The file MUST have been opened in write or update (read/write) mode.
*
*               (5) #### Check for multiplication overflow.
*********************************************************************************************************
*/

CPU_SIZE_T  NetFS_fwrite (const   void          *p_src,
                                  CPU_SIZE_T     size,
                                  CPU_SIZE_T     nitems,
                                  void          *p_file)
{
    /* $$$$ Insert code to write to a file.     */
}

/*
*********************************************************************************************************
*                                           NetFS_fclose()
*
* Description : Close & free a file.
*
* Argument(s) : p_file      Pointer to a file.
*
* Return(s)   : 0,      if the file was successfully closed.
*               FS_EOF, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a file system suite application interface (API) function & MAY be called
*               by application function(s).
*
* Note(s)     : (1) After a file is closed, the application MUST desist from accessing its file pointer.
*                   This could cause file system corruption, since this handle may be re-used for a
*                   different file.
*
*               (2) (a) If the most recent operation is output (write), all unwritten data is written
*                       to the file.
*
*                   (b) Any buffer assigned with 'fs_setbuf()' or 'fs_setvbuf()' shall no longer be
*                       accessed by the file system & may be re-used by the application.
*********************************************************************************************************
*/

int  NetFS_fclose  (void  *p_file)
{
    /* $$$$ Insert code to close & free a file. */
}


