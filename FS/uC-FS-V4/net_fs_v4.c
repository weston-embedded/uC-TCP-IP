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
*                                             uC/FS V4.xx
*
* Filename : net_fs_v4.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes the following versions (or more recent) of software modules are included in
*                the project build :
*
*                (a) uC/FS  V4.07.04
*                (b) uC/Clk V3.09.04       if uC/FS V4.04 (or more recent) is included
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

#define    NET_FS_V4_MODULE
#include  "net_fs_v4.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  NET_FS_FILE_BUF_SIZE   512u


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
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetFS_WorkingFolderGet (CPU_CHAR    *p_path,
                                             CPU_SIZE_T   path_len_max);

static  CPU_BOOLEAN  NetFS_WorkingFolderSet (CPU_CHAR    *p_path);


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
const  NET_FS_API  NetFS_API_FS_V4 = {
                                       NetFS_CfgPathGetLenMax,          /*   Path max len.                              */
                                       NetFS_CfgPathGetSepChar,         /*   Path sep char.                             */
                                       NetFS_FileOpen,                  /*   Open                                       */
                                       NetFS_FileClose,                 /*   Close                                      */
                                       NetFS_FileRd,                    /*   Rd                                         */
                                       NetFS_FileWr,                    /*   Wr                                         */
                                       NetFS_FilePosSet,                /*   Set position                               */
                                       NetFS_FileSizeGet,               /*   Get size                                   */
                                       NetFS_DirOpen,                   /*   Open  dir                                  */
                                       NetFS_DirClose,                  /*   Close dir                                  */
                                       NetFS_DirRd,                     /*   Rd    dir                                  */
                                       NetFS_EntryCreate,               /*   Entry create                               */
                                       NetFS_EntryDel,                  /*   Entry del                                  */
                                       NetFS_EntryRename,               /*   Entry rename                               */
                                       NetFS_EntryTimeSet,              /*   Entry set time                             */
                                       NetFS_FileDateTimeCreateGet,     /*   Create date time                           */
                                       NetFS_WorkingFolderGet,          /*   Get working folder                         */
                                       NetFS_WorkingFolderSet,          /*   Set working folder                         */
                                     };


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

#if (NET_FS_FILE_BUF_EN == DEF_ENABLED)
static  CPU_DATA  NetFS_FileBuf[NET_FS_FILE_BUF_SIZE/sizeof(CPU_DATA)];

static  FS_FILE  *NetFS_BufFilePtr = DEF_NULL;
#endif


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
    return ((CPU_INT32U)FS_CFG_MAX_PATH_NAME_LEN);
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
    return ((CPU_CHAR)FS_CHAR_PATH_SEP);
}


/*
*********************************************************************************************************
*                                          NetFS_DirOpen()
*
* Description : Open a directory.
*
* Argument(s) : p_name  Name of the directory.
*
* Return(s)   : Pointer to a directory, if NO errors.
*               Pointer to NULL,        otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  *NetFS_DirOpen (CPU_CHAR  *p_name)
{
#ifdef  FS_DIR_MODULE_PRESENT
    FS_DIR  *p_dir;
    FS_ERR   err;


#if (NET_FS_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ---------------- VALIDATE DIR NAME ----------------- */
    if (p_name == (CPU_CHAR *)0) {                              /* Validate NULL name.                                  */
        return ((void *)0);
    }
#endif

                                                                /* -------------------- OPEN DIR ---------------------- */
    p_dir = FSDir_Open(p_name,
                      &err);
   (void)&err;                                                  /* Ignore err.                                          */

    return ((void *)p_dir);

#else
   (void)&p_name;                                               /* Prevent 'variable unused' compiler warning.          */

    return ((void *)0);
#endif
}


/*
*********************************************************************************************************
*                                         NetFS_DirClose()
*
* Description : Close a directory.
*
* Argument(s) : p_dir   Pointer to a directory.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetFS_DirClose (void  *p_dir)
{
#ifdef  FS_DIR_MODULE_PRESENT
    FS_ERR  err;


#if (NET_FS_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------ VALIDATE DIR -------------------- */
    if (p_dir == (void *)0) {                                   /* Validate NULL dir ptr.                               */
        return;
    }
#endif

                                                                /* -------------------- CLOSE DIR --------------------- */
    FSDir_Close(p_dir,
               &err);
   (void)&err;                                                  /* Ignore err.                                          */

#else
   (void)&pdir;                                                 /* Prevent 'variable unused' compiler warning.          */
#endif
}


/*
*********************************************************************************************************
*                                           NetFS_DirRd()
*
* Description : Read a directory entry from a directory.
*
* Argument(s) : p_dir       Pointer to a directory.
*
*               p_entry     Pointer to variable that will receive directory entry information.
*
* Return(s)   : DEF_OK,   if directory entry read.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_DirRd (void          *p_dir,
                          NET_FS_ENTRY  *p_entry)
{
#ifdef  FS_DIR_MODULE_PRESENT
    CPU_INT16U     attrib;
    FS_DIR_ENTRY   entry_fs;
    FS_ERR         err;
#if (FS_VERSION >= 404u)
    CLK_DATE_TIME  stime;
    CPU_BOOLEAN    conv_success;
#else
    FS_DATE_TIME   stime;
#endif


#if (NET_FS_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------ VALIDATE PTRS ------------------- */
    if (p_dir == (void *)0) {                                   /* Validate NULL dir   ptr.                             */
        return (DEF_FAIL);
    }
    if (p_entry == (NET_FS_ENTRY *)0) {                         /* Validate NULL entry ptr.                             */
        return (DEF_FAIL);
    }
    if (p_entry->NamePtr == (CPU_CHAR *)0) {                    /* Validate Name entry ptr.                             */
        return (DEF_FAIL);
    }
#endif


                                                                /* ---------------------- RD DIR ---------------------- */
    FSDir_Rd(p_dir,
            &entry_fs,
            &err);
    if (err != FS_ERR_NONE) {
        return (DEF_FAIL);
    }

    Str_Copy_N(p_entry->NamePtr, entry_fs.Name, FS_CFG_MAX_PATH_NAME_LEN);

    attrib = DEF_BIT_NONE;
    if (DEF_BIT_IS_SET(entry_fs.Info.Attrib, FS_ENTRY_ATTRIB_RD) == DEF_YES) {
        attrib |= NET_FS_ENTRY_ATTRIB_RD;
    }
    if (DEF_BIT_IS_SET(entry_fs.Info.Attrib, FS_ENTRY_ATTRIB_WR) == DEF_YES) {
        attrib |= NET_FS_ENTRY_ATTRIB_WR;
    }
    if (DEF_BIT_IS_SET(entry_fs.Info.Attrib, FS_ENTRY_ATTRIB_HIDDEN) == DEF_YES) {
        attrib |= NET_FS_ENTRY_ATTRIB_HIDDEN;
    }
    if (DEF_BIT_IS_SET(entry_fs.Info.Attrib, FS_ENTRY_ATTRIB_DIR) == DEF_YES) {
        attrib |= NET_FS_ENTRY_ATTRIB_DIR;
    }
    p_entry->Attrib = attrib;
    p_entry->Size   = entry_fs.Info.Size;

#if (FS_VERSION >= 404u)
    conv_success = Clk_TS_UnixToDateTime(entry_fs.Info.DateTimeCreate,
                                         0,
                                        &stime);
    if (conv_success == DEF_OK) {
        p_entry->DateTimeCreate.Sec   = stime.Sec;
        p_entry->DateTimeCreate.Min   = stime.Min;
        p_entry->DateTimeCreate.Hr    = stime.Hr;
        p_entry->DateTimeCreate.Day   = stime.Day;
        p_entry->DateTimeCreate.Month = stime.Month;
        p_entry->DateTimeCreate.Yr    = stime.Yr;
    } else {
        p_entry->DateTimeCreate.Sec   = 0u;
        p_entry->DateTimeCreate.Min   = 0u;
        p_entry->DateTimeCreate.Hr    = 0u;
        p_entry->DateTimeCreate.Day   = 1u;
        p_entry->DateTimeCreate.Month = 1u;
        p_entry->DateTimeCreate.Yr    = 0u;
    }
#else
    FSTime_TS_to_Time(entry_fs.Info.DateTimeCreate,
                     &stime,
                     &err);
    if (err == FS_ERR_NONE) {
        p_entry->DateTimeCreate.Sec   = stime.Sec;
        p_entry->DateTimeCreate.Min   = stime.Min;
        p_entry->DateTimeCreate.Hr    = stime.Hr;
        p_entry->DateTimeCreate.Day   = stime.Day;
        p_entry->DateTimeCreate.Month = stime.Month + 1u;
        p_entry->DateTimeCreate.Yr    = stime.Yr    + FS_TIME_YEAR_OFFSET;
    } else {
        p_entry->DateTimeCreate.Sec   = 0u;
        p_entry->DateTimeCreate.Min   = 0u;
        p_entry->DateTimeCreate.Hr    = 0u;
        p_entry->DateTimeCreate.Day   = 1u;
        p_entry->DateTimeCreate.Month = 1u;
        p_entry->DateTimeCreate.Yr    = 0u;
    }
#endif

    return (DEF_OK);


#else
   (void)&p_dir;                                                /* Prevent 'variable unused' compiler warning.          */
   (void)&p_entry;

    return (DEF_FAIL);
#endif
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
* Argument(s) : p_name  Name of the entry.
*
*               dir     Indicates whether the new entry shall be a directory :
*
*                           DEF_YES, if the entry shall be a directory.
*                           DEF_NO,  if the entry shall be a file.
*
* Return(s)   : DEF_OK,   if entry created.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_EntryCreate (CPU_CHAR     *p_name,
                                CPU_BOOLEAN   dir)
{
#if (FS_CFG_RD_ONLY_EN != DEF_ENABLED)
    FS_ERR       err;
    CPU_BOOLEAN  ok;
#if (FS_VERSION >= 404u)
    FS_FLAGS     entry_type;
#endif


#if (NET_FS_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ---------------- VALIDATE FILE NAME ---------------- */
    if (p_name == (CPU_CHAR *)0) {                              /* Validate NULL name.                                  */
        return (DEF_FAIL);
    }
#endif

                                                                /* ----------------- CREATE FILE/DIR ------------------ */
#if (FS_VERSION >= 404u)
    if (dir == DEF_YES) {
        entry_type = FS_ENTRY_TYPE_DIR;
    } else {
        entry_type = FS_ENTRY_TYPE_FILE;
    }

    FSEntry_Create(p_name,
                   entry_type,
                   DEF_YES,
                  &err);
#else
    FSEntry_Create(pname,
                   dir,
                   DEF_YES,
                  &err);
#endif

    ok = (err == FS_ERR_NONE) ? DEF_OK : DEF_FAIL;

    return (ok);

#else
   (void)&p_name;                                               /* Prevent 'variable unused' compiler warning.          */
   (void)&dir;

    return (DEF_FAIL);
#endif
}


/*
*********************************************************************************************************
*                                         NetFS_EntryDel()
*
* Description : Delete a file or directory.
*
* Argument(s) : p_name  Name of the entry.
*
*               file    Indicates whether the entry MAY be a file :
*
*                           DEF_YES, if the entry MAY be a file.
*                           DEF_NO,  if the entry is NOT a file.
*
* Return(s)   : DEF_OK,   if entry created.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_EntryDel (CPU_CHAR     *p_name,
                             CPU_BOOLEAN   file)
{
#if (FS_CFG_RD_ONLY_EN != DEF_ENABLED)
    FS_ERR       err;
    CPU_BOOLEAN  ok;
#if (FS_VERSION >= 404u)
    FS_FLAGS     entry_type;
#endif


#if (NET_FS_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ---------------- VALIDATE FILE NAME ---------------- */
    if (p_name == (CPU_CHAR *)0) {                              /* Validate NULL name.                                  */
        return (DEF_FAIL);
    }
#endif

                                                                /* --------------------- DEL FILE --------------------- */
#if (FS_VERSION >= 404u)
    if (file == DEF_YES) {
        entry_type = FS_ENTRY_TYPE_ANY;
    } else {
        entry_type = FS_ENTRY_TYPE_DIR;
    }
    FSEntry_Del(p_name,
                entry_type,
               &err);
#else
    FSEntry_Del(p_name,
                file,
               &err);
#endif

    ok = (err == FS_ERR_NONE) ? DEF_OK : DEF_FAIL;

    return (ok);

#else
   (void)&p_name;                                               /* Prevent 'variable unused' compiler warning.          */
   (void)&file;

    return (DEF_FAIL);
#endif
}


/*
*********************************************************************************************************
*                                        NetFS_EntryRename()
*
* Description : Rename a file or directory.
*
* Argument(s) : p_name_old  Old path of the entry.
*
*               p_name_new  New path of the entry.
*
* Return(s)   : DEF_OK,   if entry renamed.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_EntryRename (CPU_CHAR  *p_name_old,
                                CPU_CHAR  *p_name_new)
{
#if (FS_CFG_RD_ONLY_EN != DEF_ENABLED)
    FS_ERR       err;
    CPU_BOOLEAN  ok;


#if (NET_FS_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------ VALIDATE PTRS ------------------- */
    if (p_name_old == (CPU_CHAR *)0) {                          /* Validate NULL name.                                  */
        return (DEF_FAIL);
    }
    if (p_name_old == (CPU_CHAR *)0) {                          /* Validate NULL name.                                  */
        return (DEF_FAIL);
    }
#endif

                                                                /* ------------------- RENAME FILE -------------------- */
    FSEntry_Rename(p_name_old,
                   p_name_new,
                   DEF_YES,
                  &err);

    ok = (err == FS_ERR_NONE) ? DEF_OK : DEF_FAIL;

    return (ok);

#else
   (void)&p_name_old;                                           /* Prevent 'variable unused' compiler warning.          */
   (void)&p_name_new;

    return (DEF_FAIL);
#endif
}


/*
*********************************************************************************************************
*                                       NetFS_EntryTimeSet()
*
* Description : Set a file or directory's date/time.
*
* Argument(s) : p_name  Name of the entry.
*
*               p_time  Pointer to date/time.
*
* Return(s)   : DEF_OK,   if date/time set.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_EntryTimeSet (CPU_CHAR          *p_name,
                                 NET_FS_DATE_TIME  *p_time)
{
#if (FS_CFG_RD_ONLY_EN != DEF_ENABLED)
    FS_ERR         err;
    CPU_BOOLEAN    ok;
#if (FS_VERSION >= 404u)
    CLK_DATE_TIME  stime;
#else
    FS_DATE_TIME   stime;
#endif


#if (NET_FS_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------ VALIDATE PTRS ------------------- */
    if (p_name == (CPU_CHAR *)0) {                              /* Validate NULL name.                                  */
        return (DEF_FAIL);
    }
    if (p_time == (NET_FS_DATE_TIME *)0) {                      /* Validate NULL date/time.                             */
        return (DEF_FAIL);
    }
#endif

                                                                /* ------------------ SET DATE/TIME ------------------- */
#if (FS_VERSION >= 404u)
    stime.Sec   = p_time->Sec;
    stime.Min   = p_time->Min;
    stime.Hr    = p_time->Hr;
    stime.Day   = p_time->Day;
    stime.Month = p_time->Month;
    stime.Yr    = p_time->Yr;
#else
    stime.Sec   = p_time->Sec;
    stime.Min   = p_time->Min;
    stime.Hr    = p_time->Hr;
    stime.Day   = p_time->Day;
    stime.Month = p_time->Month - 1u;
    stime.Yr    = p_time->Yr    - FS_TIME_YEAR_OFFSET;
#endif

    FSEntry_TimeSet(p_name,
                   &stime,
                    FS_DATE_TIME_ALL,
                   &err);

    ok = (err == FS_ERR_NONE) ? DEF_OK : DEF_FAIL;

    return (ok);

#else
   (void)&p_name;                                               /* Prevent 'variable unused' compiler warning.          */
   (void)&p_time;

    return (DEF_FAIL);
#endif
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
* Argument(s) : p_name  Name of the file.
*
*               mode    Mode of the file :
*
*                           NET_FS_FILE_MODE_APPEND        Open existing file at end-of-file OR create new file.
*                           NET_FS_FILE_MODE_CREATE        Create new file OR overwrite existing file.
*                           NET_FS_FILE_MODE_CREATE_NEW    Create new file OR return error if file exists.
*                           NET_FS_FILE_MODE_OPEN          Open existing file.
*                           NET_FS_FILE_MODE_TRUNCATE      Truncate existing file to zero length.
*
*               access  Access rights of the file :
*
*                           NET_FS_FILE_ACCESS_RD          Open file in read           mode.
*                           NET_FS_FILE_ACCESS_RD_WR       Open file in read AND write mode.
*                           NET_FS_FILE_ACCESS_WR          Open file in          write mode
*
*
* Return(s)   : Pointer to a file, if NO errors.
*               Pointer to NULL,   otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  *NetFS_FileOpen (CPU_CHAR            *p_name,
                       NET_FS_FILE_MODE     mode,
                       NET_FS_FILE_ACCESS   access)
{
    FS_ERR      err;
    FS_FLAGS    mode_flags;
    FS_FILE    *p_file;
#if (NET_FS_FILE_BUF_EN == DEF_ENABLED)
    CPU_SR_ALLOC();
#endif


#if (NET_FS_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------ VALIDATE PTRS ------------------- */
    if (p_name     == (CPU_CHAR *)0) {                          /* Validate NULL name.                                  */
        return ((void *)0);
    }
#endif


                                                                /* -------------------- OPEN FILE --------------------- */
    mode_flags = FS_FILE_ACCESS_MODE_NONE;

    switch (mode) {
        case NET_FS_FILE_MODE_APPEND:
             DEF_BIT_SET(mode_flags, FS_FILE_ACCESS_MODE_APPEND);
             DEF_BIT_SET(mode_flags, FS_FILE_ACCESS_MODE_CREATE);
             break;


        case NET_FS_FILE_MODE_CREATE:
             DEF_BIT_SET(mode_flags, FS_FILE_ACCESS_MODE_CREATE);
             DEF_BIT_SET(mode_flags, FS_FILE_ACCESS_MODE_TRUNCATE);
             break;


        case NET_FS_FILE_MODE_CREATE_NEW:
             DEF_BIT_SET(mode_flags, FS_FILE_ACCESS_MODE_CREATE);
             DEF_BIT_SET(mode_flags, FS_FILE_ACCESS_MODE_EXCL);
             break;


        case NET_FS_FILE_MODE_OPEN:
             break;


        case NET_FS_FILE_MODE_TRUNCATE:
             DEF_BIT_SET(mode_flags, FS_FILE_ACCESS_MODE_TRUNCATE);
             break;


        default:
             return (DEF_NULL);
    }


    switch (access) {
        case NET_FS_FILE_ACCESS_RD:
             DEF_BIT_SET(mode_flags, FS_FILE_ACCESS_MODE_RD);
             break;


        case NET_FS_FILE_ACCESS_RD_WR:
             DEF_BIT_SET(mode_flags, FS_FILE_ACCESS_MODE_RD);
             DEF_BIT_SET(mode_flags, FS_FILE_ACCESS_MODE_WR);
             break;


        case NET_FS_FILE_ACCESS_WR:
             DEF_BIT_SET(mode_flags, FS_FILE_ACCESS_MODE_WR);
             break;


        default:
             return (DEF_NULL);
    }


    p_file = FSFile_Open(p_name,
                         mode_flags,
                        &err);
   (void)&err;                                                  /* Ignore err.                                          */

#if (NET_FS_FILE_BUF_EN == DEF_ENABLED)
    if (p_file == DEF_NULL) {
        return DEF_NULL;
    }

    CPU_CRITICAL_ENTER();
    if (NetFS_BufFilePtr == DEF_NULL) {
        NetFS_BufFilePtr = p_file;
        CPU_CRITICAL_EXIT();

        FSFile_BufAssign(p_file, &NetFS_FileBuf, FS_FILE_BUF_MODE_RD_WR, NET_FS_FILE_BUF_SIZE, &err);
        if (err != FS_ERR_NONE) {
            CPU_CRITICAL_ENTER();
            NetFS_BufFilePtr = DEF_NULL;
            CPU_CRITICAL_EXIT();
        }

    } else {
        CPU_CRITICAL_EXIT();
    }
#endif


    return ((void *)p_file);
}


/*
*********************************************************************************************************
*                                         NetFS_FileClose()
*
* Description : Close a file.
*
* Argument(s) : p_file  Pointer to a file.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetFS_FileClose (void  *p_file)
{
    FS_ERR  err;
#if (NET_FS_FILE_BUF_EN == DEF_ENABLED)
    CPU_SR_ALLOC();
#endif


#if (NET_FS_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ---------------- VALIDATE FILE PTR ----------------- */
    if (p_file == (void *)0) {                                  /* Validate NULL file ptr.                              */
        return;
    }
#endif

#if (NET_FS_FILE_BUF_EN == DEF_ENABLED)
    CPU_CRITICAL_ENTER();
    if (NetFS_BufFilePtr == p_file) {
        NetFS_BufFilePtr  = DEF_NULL;
    }
    CPU_CRITICAL_EXIT();
#endif


                                                                /* -------------------- CLOSE FILE -------------------- */
    FSFile_Close(p_file,
                &err);

    p_file = DEF_NULL;

   (void)&err;                                                  /* Ignore err.                                          */
}


/*
*********************************************************************************************************
*                                          NetFS_FileRd()
*
* Description : Read from a file.
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_dest      Pointer to destination buffer.
*
*               size        Number of octets to read.
*
*               p_size_rd   Pointer to variable that will receive the number of octets read.
*
* Return(s)   : DEF_OK,   if no error occurred during read (see Note #2).
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) Pointers to variables that return values MUST be initialized PRIOR to all other
*                   validation or function handling in case of any error(s).
*
*               (2) If the read request could not be fulfilled because the EOF was reached, the return
*                   value should be 'DEF_OK'.  The application should compare the value in 'psize_rd' to
*                   the value passed to 'size' to detect an EOF reached condition.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_FileRd (void        *p_file,
                           void        *p_dest,
                           CPU_SIZE_T   size,
                           CPU_SIZE_T  *p_size_rd)
{
    FS_ERR       err;
    CPU_BOOLEAN  ok;
    CPU_SIZE_T   size_rd;


#if (NET_FS_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ---------------- VALIDATE SIZE PTR ----------------- */
    if (p_size_rd == (CPU_SIZE_T *)0) {                         /* Validate NULL size ptr.                              */
        return (DEF_FAIL);
    }
#endif

   *p_size_rd = 0u;                                             /* Init to dflt size for err (see Note #1).             */

#if (NET_FS_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------ VALIDATE PTRS ------------------- */
    if (p_file == (void *)0) {                                  /* Validate NULL file ptr.                              */
        return (DEF_FAIL);
    }
    if (p_dest == (void *)0) {                                  /* Validate NULL dest ptr.                              */
        return (DEF_FAIL);
    }
#endif

                                                                /* --------------------- RD FILE ---------------------- */
    size_rd  = FSFile_Rd(p_file,
                         p_dest,
                         size,
                        &err);

    ok        = ((err == FS_ERR_NONE) ||
                 (err == FS_ERR_EOF)) ? DEF_OK : DEF_FAIL;

   *p_size_rd = size_rd;

    return (ok);
}


/*
*********************************************************************************************************
*                                          NetFS_FileWr()
*
* Description : Write to a file.
*
* Argument(s) : p_file      Pointer to a file.
*
*               p_src       Pointer to source buffer.
*
*               size        Number of octets to write.
*
*               p_size_wr   Pointer to variable that will receive the number of octets written.
*
* Return(s)   : DEF_OK,   if no error occurred during write.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) Pointers to variables that return values MUST be initialized PRIOR to all other
*                   validation or function handling in case of any error(s).
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_FileWr (void        *p_file,
                           void        *p_src,
                           CPU_SIZE_T   size,
                           CPU_SIZE_T  *p_size_wr)
{
#if (FS_CFG_RD_ONLY_EN != DEF_ENABLED)
    FS_ERR       err;
    CPU_BOOLEAN  ok;
    CPU_SIZE_T   size_wr;
#endif


#if (NET_FS_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ---------------- VALIDATE SIZE PTR ----------------- */
    if (p_size_wr == (CPU_SIZE_T *)0) {                         /* Validate NULL size ptr.                              */
        return (DEF_FAIL);
    }
#endif

   *p_size_wr = 0u;                                             /* Init to dflt size (see Note #1).                     */

#if (NET_FS_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ------------------ VALIDATE PTRS ------------------- */
    if (p_file == (void *)0) {                                  /* Validate NULL file ptr.                              */
        return (DEF_FAIL);
    }
    if (p_src  == (void *)0) {                                  /* Validate NULL src  ptr.                              */
        return (DEF_FAIL);
    }
#endif

                                                                /* --------------------- WR FILE ---------------------- */
#if (FS_CFG_RD_ONLY_EN != DEF_ENABLED)
    size_wr   =  FSFile_Wr(p_file,
                           p_src,
                           size,
                          &err);

    ok        = (err == FS_ERR_NONE) ? DEF_OK : DEF_FAIL;
   *p_size_wr =  size_wr;

    return (ok);

#else
   (void)&p_file;                                               /* Prevent 'variable unused' compiler warning.          */
   (void)&p_src;
   (void)&size;

    return (DEF_FAIL);
#endif
}


/*
*********************************************************************************************************
*                                        NetFS_FilePosSet()
*
* Description : Set file position indicator.
*
* Argument(s) : p_file  Pointer to a file.
*
*               offset  Offset from the file position specified by 'origin'.
*
*               origin  Reference position for offset :
*
*                           NET_FS_SEEK_ORIGIN_START    Offset is from the beginning of the file.
*                           NET_FS_SEEK_ORIGIN_CUR      Offset is from current file position.
*                           NET_FS_SEEK_ORIGIN_END      Offset is from the end       of the file.
*
* Return(s)   : DEF_OK,   if file position set.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_FilePosSet (void        *p_file,
                               CPU_INT32S   offset,
                               CPU_INT08U   origin)
{
    FS_ERR       err;
    CPU_BOOLEAN  ok;
    FS_STATE     origin_fs;


#if (NET_FS_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ---------------- VALIDATE FILE PTR ----------------- */
    if (p_file == (void *)0) {                                  /* Validate NULL file ptr.                              */
        return (DEF_FAIL);
    }
#endif

                                                                /* ------------------ SET FILE POS -------------------- */
    switch (origin) {
        case NET_FS_SEEK_ORIGIN_START:
             origin_fs = FS_FILE_ORIGIN_START;
             break;


        case NET_FS_SEEK_ORIGIN_CUR:
             origin_fs = FS_FILE_ORIGIN_CUR;
             break;


        case NET_FS_SEEK_ORIGIN_END:
             origin_fs = FS_FILE_ORIGIN_END;
             break;


        default:
             return (DEF_FAIL);
    }

    FSFile_PosSet(p_file,
                  offset,
                  origin_fs,
                 &err);

    ok = (err == FS_ERR_NONE) ? DEF_OK : DEF_FAIL;

    return (ok);
}


/*
*********************************************************************************************************
*                                        NetFS_FileSizeGet()
*
* Description : Get file size.
*
* Argument(s) : p_file  Pointer to a file.
*
*               p_size  Pointer to variable that will receive the file size.
*
* Return(s)   : DEF_OK,   if file size gotten.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) Pointers to variables that return values MUST be initialized PRIOR to all other
*                   validation or function handling in case of any error(s).
*********************************************************************************************************
*/

CPU_BOOLEAN  NetFS_FileSizeGet (void        *p_file,
                                CPU_INT32U  *p_size)
{
    FS_ENTRY_INFO  info;
    FS_ERR         err;


#if (NET_FS_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ---------------- VALIDATE SIZE PTR ----------------- */
    if (p_size == (CPU_INT32U *)0) {                            /* Validate NULL size ptr.                              */
        return (DEF_FAIL);
    }
#endif

   *p_size = 0u;                                                /* Init to dflt size for err (see Note #1).             */

#if (NET_FS_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ---------------- VALIDATE FILE PTR ----------------- */
    if (p_file == (void *)0) {                                  /* Validate NULL file ptr.                              */
        return (DEF_FAIL);
    }
#endif

                                                                /* ------------------ GET FILE SIZE ------------------- */
    FSFile_Query(p_file,
                &info,
                &err);
    if (err != FS_ERR_NONE) {
        return (DEF_FAIL);
    }

   *p_size = (CPU_INT32U)info.Size;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                   NetFS_FileDateTimeCreateGet()
*
* Description : Get file creation date/time.
*
* Argument(s) : p_file  Pointer to a file.
*
*               p_time  Pointer to variable that will receive the date/time :
*
*                           Default/epoch date/time,        if any error(s);
*                           Current       date/time,        otherwise.
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

CPU_BOOLEAN  NetFS_FileDateTimeCreateGet (void              *p_file,
                                          NET_FS_DATE_TIME  *p_time)
{
    CLK_DATE_TIME  time;
    CLK_TS_SEC     time_ts_sec;
    CPU_BOOLEAN    rtn_code;
    FS_ENTRY_INFO  info;
    FS_ERR         err;


#if (NET_FS_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ---------------- VALIDATE DATE/TIME PTR ------------ */
    if (p_time == (NET_FS_DATE_TIME *)0) {
        return (DEF_FAIL);
    }
#endif
                                                                /* Init to dflt date/time for err (see Note #1).        */
    p_time->Yr    = 0u;
    p_time->Month = 0u;
    p_time->Day   = 0u;
    p_time->Hr    = 0u;
    p_time->Min   = 0u;
    p_time->Sec   = 0u;

#if (NET_FS_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                  /* ---------------- VALIDATE FILE PTR ----------------- */
    if (p_file == (void *)0) {                                  /* Validate NULL file ptr.                              */
        return (DEF_FAIL);
    }
#endif

                                                                /* ---------- GET FILE CREATION DATE/TIME ------------- */
    FSFile_Query(p_file,
                &info,
                &err);
    if (err != FS_ERR_NONE) {
        return (DEF_FAIL);
    }

    time_ts_sec = info.DateTimeCreate;
    rtn_code    = Clk_TS_ToDateTime(time_ts_sec, 0, &time);     /* Convert clk timestamp to date/time structure.        */
    if (rtn_code != DEF_OK) {
        return (DEF_FAIL);
    }
                                                                /* Set each creation date/time field to be rtn'd.       */
    p_time->Yr    = time.Yr;
    p_time->Month = time.Month;
    p_time->Day   = time.Day;
    p_time->Hr    = time.Hr;
    p_time->Min   = time.Min;
    p_time->Sec   = time.Sec;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                      NetFS_WorkingFolderGet()
*
* Description : Get current working folder.
*
* Argument(s) : p_path          Pointer to string that will receive the working folder
*
* Return(s)   : DEF_OK,   if p_path successfully copied.
*
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
    FS_ERR  err;


    FS_WorkingDirGet(p_path, path_len_max, &err);
    switch (err) {
        case FS_ERR_NONE:
             return (DEF_OK);


        case FS_ERR_NULL_PTR:
        case FS_ERR_NULL_ARG:
        case FS_ERR_NAME_BUF_TOO_SHORT:
        case FS_ERR_VOL_NONE_EXIST:
        default:
             break;
    }

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

static  CPU_BOOLEAN  NetFS_WorkingFolderSet (CPU_CHAR  *p_path)
{
    FS_ERR  err;


    FS_WorkingDirSet(p_path, &err);
    switch (err) {
        case FS_ERR_NONE:
             return (DEF_OK);


        case FS_ERR_NAME_INVALID:
        case FS_ERR_NAME_PATH_TOO_LONG:
        case FS_ERR_NULL_PTR:
        case FS_ERR_VOL_NONE_EXIST:
        case FS_ERR_WORKING_DIR_NONE_AVAIL:
        case FS_ERR_WORKING_DIR_INVALID:
        default:
             break;
    }

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
    FS_FILE  *p_file;


    p_file = fs_fopen(name_full, str_mode);

    return (void *)p_file;
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
    int rtn_value;


    rtn_value = fs_fseek((FS_FILE *)p_file,
                                    offset,
                                    origin);

    return (rtn_value);
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
*********************************************************************************************************
*/

long  int  NetFS_ftell (void  *p_file)
{
    long  int  pos;


    pos = fs_ftell((FS_FILE *)p_file);


    return (pos);
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
    fs_rewind((FS_FILE *) p_file);
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
*********************************************************************************************************
*/

CPU_SIZE_T  NetFS_fread ( void          *p_dest,
                          CPU_SIZE_T     size,
                          CPU_SIZE_T     nitems,
                          void          *p_file)
{
    CPU_SIZE_T  len_rd;


    len_rd = fs_fread(           p_dest,
                                 size,
                                 nitems,
                      (FS_FILE *)p_file);


    return (len_rd);
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
*********************************************************************************************************
*/

CPU_SIZE_T  NetFS_fwrite (const   void          *p_src,
                                  CPU_SIZE_T     size,
                                  CPU_SIZE_T     nitems,
                                  void          *p_file)
{
    CPU_SIZE_T  len_wr;


    len_wr = fs_fwrite(           p_src,
                                  size,
                                  nitems,
                       (FS_FILE *)p_file);

    return (len_wr);
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
    int  rtn_value;


    rtn_value = fs_fclose((FS_FILE *)p_file);

    return (rtn_value);
}

