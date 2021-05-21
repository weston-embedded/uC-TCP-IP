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
*                                NETWORK FILE SYSTEM PORT HEADER FILE
*
* Filename : net_fs.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Network File System (API) Layer module is required for :
*
*                (a) Applications that require network application programming interface (API) :
*                    (1) Network socket API with error handling
*                    (2) Network time delays
*
*            (2) Should be compatible with the following TCP/IP application versions (or more recent):
*
*                (a) uC/FTPc  V1.93.02
*                (b) uC/FTPs  V1.95.03
*                (c) uC/HTTPs V1.98.01
*                (d) uC/TFTPc V1.92.02
*                (e) uC/TFTPs V1.91.03
*                (f) Segger emSSL V2.54a
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  NET_FS_PRESENT
#define  NET_FS_PRESENT


/*
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*/

#ifdef   NET_FS_MODULE
#define  NET_FS_EXT
#else
#define  NET_FS_EXT  extern
#endif


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu.h>
#include  <cpu_core.h>

#include  <lib_def.h>
#include  <lib_str.h>
#include  <lib_ascii.h>

#include  <net_cfg.h>


/*
*********************************************************************************************************
*                                        DEFAULT CONFIGURATION
*********************************************************************************************************
*/

#ifndef  NET_FS_CFG_ARG_CHK_EXT_EN
#define  NET_FS_CFG_ARG_CHK_EXT_EN     DEF_DISABLED
#endif


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

#define  NET_FS_PATH_SEP_CHAR                    ASCII_CHAR_REVERSE_SOLIDUS

#define  NET_FS_MAX_FILE_NAME_LEN                NET_FS_CFG_MAX_FILE_NAME_LEN
#define  NET_FS_MAX_PATH_NAME_LEN                NET_FS_CFG_MAX_PATH_NAME_LEN

#define  NET_FS_SEEK_ORIGIN_START                         1u    /* Origin is beginning of file.                         */
#define  NET_FS_SEEK_ORIGIN_CUR                           2u    /* Origin is current file position.                     */
#define  NET_FS_SEEK_ORIGIN_END                           3u    /* Origin is end of file.                               */

#define  NET_FS_ENTRY_ATTRIB_RD                  DEF_BIT_00     /* Entry is readable.                                   */
#define  NET_FS_ENTRY_ATTRIB_WR                  DEF_BIT_01     /* Entry is writeable.                                  */
#define  NET_FS_ENTRY_ATTRIB_HIDDEN              DEF_BIT_02     /* Entry is hidden from user-level processes.           */
#define  NET_FS_ENTRY_ATTRIB_DIR                 DEF_BIT_03     /* Entry is a directory.                                */


/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                        DATE / TIME DATA TYPE
*********************************************************************************************************
*/

typedef  struct  net_fs_date_time {
    CPU_INT16U         Sec;                                     /* Seconds [0..60].                                     */
    CPU_INT16U         Min;                                     /* Minutes [0..59].                                     */
    CPU_INT16U         Hr;                                      /* Hour [0..23].                                        */
    CPU_INT16U         Day;                                     /* Day of month [1..31].                                */
    CPU_INT16U         Month;                                   /* Month of year [1..12].                               */
    CPU_INT16U         Yr;                                      /* Year [0, ..., 2000, 2001, ...].                      */
} NET_FS_DATE_TIME;

/*
*********************************************************************************************************
*                                           ENTRY DATA TYPE
*********************************************************************************************************
*/

typedef  struct  net_fs_entry {
    CPU_INT16U         Attrib;                                  /* Entry attributes.                                    */
    CPU_INT32U         Size;                                    /* File size in octets.                                 */
    NET_FS_DATE_TIME   DateTimeCreate;                          /* Date/time of creation.                               */
    CPU_CHAR          *NamePtr;                                 /* Name.                                                */
} NET_FS_ENTRY;


/*
*********************************************************************************************************
*                                              FILE MODES
*********************************************************************************************************
*/

typedef  enum  net_fs_file_mode {                               /* File modes. See NetFS_FileOpen() note #1.            */
    NET_FS_FILE_MODE_NONE,
    NET_FS_FILE_MODE_APPEND,
    NET_FS_FILE_MODE_CREATE,
    NET_FS_FILE_MODE_CREATE_NEW,
    NET_FS_FILE_MODE_OPEN,
    NET_FS_FILE_MODE_TRUNCATE
} NET_FS_FILE_MODE;


/*
*********************************************************************************************************
*                                              FILE ACCESS
*********************************************************************************************************
*/

typedef  enum  net_fs_file_access {                            /* File access. See NetFS_FileOpen() note #2.            */
    NET_FS_FILE_ACCESS_RD,
    NET_FS_FILE_ACCESS_RD_WR,
    NET_FS_FILE_ACCESS_WR
} NET_FS_FILE_ACCESS;


/*
*********************************************************************************************************
*                                               FS API
*********************************************************************************************************
*/

                                                                        /* ---------------- NET FS API ---------------- */
                                                                        /* Net FS API fnct ptrs :                       */
typedef  struct  net_fs_api {
                                                                        /* ------- GENERIC  NET FS API MEMBERS -------- */
    CPU_INT32U    (*CfgPathGetLenMax)   (void);                         /*   Get path maximum length.                   */

    CPU_CHAR      (*CfgPathGetSepChar)  (void);                         /*   Get path separator character.              */

                                                                        /*   Open file                                  */
    void         *(*Open)               (CPU_CHAR            *p_name,
                                         NET_FS_FILE_MODE     mode,
                                         NET_FS_FILE_ACCESS   access);

                                                                        /*   Close file.                                */
    void          (*Close)              (void                *p_file);

                                                                        /*   Read file.                                 */
    CPU_BOOLEAN   (*Rd)                 (void                *p_file,
                                         void                *p_dest,
                                         CPU_SIZE_T           size,
                                         CPU_SIZE_T          *p_size_rd);

                                                                        /*   Write file.                                */
    CPU_BOOLEAN   (*Wr)                 (void                *p_file,
                                         void                *p_src,
                                         CPU_SIZE_T           size,
                                         CPU_SIZE_T          *p_size_wr);

                                                                        /*   Set file position.                         */
    CPU_BOOLEAN   (*SetPos)             (void                *p_file,
                                         CPU_INT32S           offset,
                                         CPU_INT08U           origin);

                                                                        /*   Get file size.                             */
    CPU_BOOLEAN   (*GetSize)            (void                *p_file,
                                         CPU_INT32U          *p_size);

                                                                        /*   Open directory.                            */
    void         *(*DirOpen)            (CPU_CHAR            *p_name);

                                                                        /*   Close directory.                           */
    void          (*DirClose)           (void                *p_dir);

                                                                        /*   Read directory.                            */
    CPU_BOOLEAN   (*DirRd)              (void                *p_dir,
                                         NET_FS_ENTRY        *p_entry);

                                                                        /*   Entry create.                              */
    CPU_BOOLEAN   (*EntryCreate)        (CPU_CHAR            *p_name,
                                         CPU_BOOLEAN          dir);

                                                                        /*   Entry delete.                              */
    CPU_BOOLEAN   (*EntryDel)           (CPU_CHAR            *p_name,
                                         CPU_BOOLEAN          file);

                                                                        /*   Entry rename.                              */
    CPU_BOOLEAN   (*EntryRename)        (CPU_CHAR            *p_name_old,
                                         CPU_CHAR            *p_name_new);

                                                                        /*   Entry time set.                            */
    CPU_BOOLEAN   (*EntryTimeSet)       (CPU_CHAR            *p_name,
                                         NET_FS_DATE_TIME    *p_time);

                                                                        /*   Create a date time.                        */
    CPU_BOOLEAN   (*DateTimeCreate)     (void                *p_file,
                                         NET_FS_DATE_TIME    *p_time);

                                                                        /*   Get working folder.                        */
    CPU_BOOLEAN   (*WorkingFolderGet)   (CPU_CHAR            *p_path,
                                         CPU_SIZE_T           path_len_max);

                                                                        /*   Set working folder.                        */
    CPU_BOOLEAN   (*WorkingFolderSet)   (CPU_CHAR            *p_path);
} NET_FS_API;


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               MACRO'S
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/


                                                                              /* ------------- CFG FNCTS ------------ */
CPU_INT32U    NetFS_CfgPathGetLenMax     (void);

CPU_CHAR      NetFS_CfgPathGetSepChar    (void);

                                                                              /* ------------- DIR FNCTS ------------ */
void         *NetFS_DirOpen              (CPU_CHAR             *p_name);      /* Open  directory.                     */

void          NetFS_DirClose             (void                 *p_dir);       /* Close directory.                     */

CPU_BOOLEAN   NetFS_DirRd                (void                 *p_dir,        /* Read entry from directory.           */
                                          NET_FS_ENTRY         *p_entry);


                                                                              /* ----------- ENTRY FNCTS ------------ */
CPU_BOOLEAN   NetFS_EntryCreate          (CPU_CHAR             *p_name,       /* Create file or directory.            */
                                          CPU_BOOLEAN           dir);

CPU_BOOLEAN   NetFS_EntryDel             (CPU_CHAR             *p_name,       /* Delete file or directory.            */
                                          CPU_BOOLEAN           file);

CPU_BOOLEAN   NetFS_EntryRename          (CPU_CHAR             *p_name_old,   /* Rename file or directory.            */
                                          CPU_CHAR             *p_name_new);

CPU_BOOLEAN   NetFS_EntryTimeSet         (CPU_CHAR             *p_name,       /* Set a  file or directory's date/time.*/
                                          NET_FS_DATE_TIME     *p_time);


                                                                              /* ------------ FILE FNCTS ------------ */
void         *NetFS_FileOpen             (CPU_CHAR             *p_name,       /* Open  file.                          */
                                          NET_FS_FILE_MODE      mode,
                                          NET_FS_FILE_ACCESS    access);

void          NetFS_FileClose            (void                 *p_file);      /* Close file.                          */


CPU_BOOLEAN   NetFS_FileRd               (void                 *p_file,       /* Read from file.                      */
                                          void                 *p_dest,
                                          CPU_SIZE_T            size,
                                          CPU_SIZE_T           *p_size_rd);

CPU_BOOLEAN   NetFS_FileWr               (void                 *p_file,       /* Write to  file.                      */
                                          void                 *p_src,
                                          CPU_SIZE_T            size,
                                          CPU_SIZE_T           *p_size_wr);

CPU_BOOLEAN   NetFS_FilePosSet           (void                 *p_file,       /* Set file position.                   */
                                          CPU_INT32S            offset,
                                          CPU_INT08U            origin);

CPU_INT32U    NetFS_FilePosGet           (void                 *pfile,
                                          CPU_BOOLEAN          *p_err);

CPU_BOOLEAN   NetFS_FileSizeGet          (void                 *p_file,       /* Get file size.                       */
                                          CPU_INT32U           *p_size);

CPU_BOOLEAN   NetFS_FileDateTimeCreateGet(void                 *p_file,       /* Get file creation date.              */
                                          NET_FS_DATE_TIME     *p_time);


                                                                              /* ---------- STD POSIX API ----------- */
void         *NetFS_fopen                (const  char          *name_full,    /* Open a file.                         */
                                          const  char          *str_mode);

int           NetFS_fseek                (       void          *p_file,       /* Set file position indicator.         */
                                                 long  int      offset,
                                                 int            origin);

long  int     NetFS_ftell               (        void          *p_file);      /* Get file position indicator.         */

void          NetFS_rewind              (        void          *p_file);      /* Reset file position indicator.       */

CPU_SIZE_T    NetFS_fread               (        void          *p_dest,       /* Read from a file.                    */
                                                 CPU_SIZE_T     size,
                                                 CPU_SIZE_T     nitems,
                                                 void          *p_file);

CPU_SIZE_T    NetFS_fwrite              (const   void          *p_src,        /* Write to a file.                     */
                                                 CPU_SIZE_T     size,
                                                 CPU_SIZE_T     nitems,
                                                 void          *p_file);

int           NetFS_fclose              (        void          *p_file);      /* Close & free a file.                 */


/*
*********************************************************************************************************
*                                          CONFIGURATION ERRORS
*********************************************************************************************************
*/

#if    ((NET_FS_CFG_ARG_CHK_EXT_EN != DEF_DISABLED) && \
        (NET_FS_CFG_ARG_CHK_EXT_EN != DEF_ENABLED ))
#error  "NET_FS_CFG_ARG_CHK_EXT_EN       illegally #define'd in 'app_cfg.h'"
#error  "                                [MUST be  DEF_DISABLED]           "
#error  "                                [     ||  DEF_ENABLED ]           "
#endif

/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif                                                          /* End of Apps FS module include.                       */

