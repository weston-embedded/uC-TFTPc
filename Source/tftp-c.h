/*
*********************************************************************************************************
*                                              uC/TFTPc
*                               Trivial File Transfer Protocol (client)
*
*                    Copyright 2004-2020 Silicon Laboratories Inc. www.silabs.com
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
*                                             TFTP CLIENT
*
* Filename : tftp-c.h
* Version  : V2.01.00
*********************************************************************************************************
* Note(s)  : (1) Assumes the following versions (or more recent) of software modules are included in
*                the project build :
*
*                  (a) uC/CPU    V1.29.02
*                  (b) uC/LIB    V1.38.00
*                  (c) uC/Common V1.00.00
*                  (d) uC/TCP-IP V3.02.00
*
*
*            (2) For additional details on the features available with uC/TFTPc, the API, the
*                installation, etc. Please refer to the uC/TFTPc documentation available at
*                https://doc.micrium.com.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) This header file is protected from multiple pre-processor inclusion through use of the
*               TFTPc present pre-processor macro definition.
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  TFTPc_MODULE_PRESENT                                   /* See Note #1.                                         */
#define  TFTPc_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                        TFTPs VERSION NUMBER
*
* Note(s) : (1) (a) The TFTPs module software version is denoted as follows :
*
*                       Vx.yy.zz
*
*                           where
*                                   V               denotes 'Version' label
*                                   x               denotes     major software version revision number
*                                   yy              denotes     minor software version revision number
*                                   zz              denotes sub-minor software version revision number
*
*               (b) The TFTPs software version label #define is formatted as follows :
*
*                       ver = x.yyzz * 100 * 100
*
*                           where
*                                   ver             denotes software version number scaled as an integer value
*                                   x.yyzz          denotes software version number, where the unscaled integer
*                                                       portion denotes the major version number & the unscaled
*                                                       fractional portion denotes the (concatenated) minor
*                                                       version numbers
*********************************************************************************************************
*********************************************************************************************************
*/

#define  TFTPc_VERSION                                 20100u   /* See Note #1.                                         */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef   TFTPc_MODULE
#define  TFTPc_EXT
#else
#define  TFTPc_EXT  extern
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*
* Note(s) : (1) The TFTPc module files are located in the following directories :
*
*               (a) \<Your Product Application>\tftp-c_cfg.h
*
*               (b) \<Network Protocol Suite>\Source\net_*.*
*                                            \FS\net_fs.h
*
*               (c) (1) \<TFTPc>\Source\tftp-c.h
*                                      \tftp-c.c
*
*           (2) CPU-configuration software files are located in the following directories :
*
*               (a) \<CPU-Compiler Directory>\cpu_*.*
*               (b) \<CPU-Compiler Directory>\<cpu>\<compiler>\cpu*.*
*
*                       where
*                               <CPU-Compiler Directory>        directory path for common CPU-compiler software
*                               <cpu>                           directory name for specific processor (CPU)
*                               <compiler>                      directory name for specific compiler
*
*           (3) NO compiler-supplied standard library functions SHOULD be used.
*
*               (a) Standard library functions are implemented in the custom library module(s) :
*
*                       \<Custom Library Directory>\lib_*.*
*
*                           where
*                                   <Custom Library Directory>      directory path for custom library software
*
*           (4) Compiler MUST be configured to include as additional include path directories :
*
*               (a) '\<Your Product Application>\' directory                            See Note #1a
*
*               (b) '\<Network Protocol Suite>\'   directory                            See Note #1b
*
*               (c) '\<TFTPc>\' directories                                             See Note #1c
*
*               (d) (1) '\<CPU-Compiler Directory>\'                  directory         See Note #2a
*                   (2) '\<CPU-Compiler Directory>\<cpu>\<compiler>\' directory         See Note #2b
*
*               (e) '\<Custom Library Directory>\' directory                            See Note #3a
*********************************************************************************************************
*********************************************************************************************************
*/

#include  <cpu.h>                                               /* CPU Configuration              (see Note #2b)        */
#include  <cpu_core.h>                                          /* CPU Core Library               (see Note #2a)        */

#include  <lib_def.h>                                           /* Standard        Defines        (see Note #3a)        */
#include  <lib_str.h>                                           /* Standard String Library        (see Note #3a)        */

#include  <tftp-c_cfg.h>                                        /* TFTP Client Configuration File (see Note #1a)        */

#include  <FS/net_fs.h>                                         /* File System Interface          (see Note #1b)        */

#include  <Source/net.h>                                        /* Network Protocol Suite         (see Note #1b)        */
#include  <Source/net_sock.h>

#if 1
#include  <stdio.h>
#endif

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                     TFTPc TRANSFER MODE DEFINES
*********************************************************************************************************
*/

#define  TFTPc_MODE_NETASCII                               1
#define  TFTPc_MODE_OCTET                                  2
#define  TFTPc_MODE_MAIL                                   3


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                     TFTPc ERROR CODES DATA TYPE
*********************************************************************************************************
*/

typedef  enum  tftpc_err {
    TFTPc_ERR_NONE,

    TFTPc_ERR_LOCK,                                     /* TFTPc Lock error.                                    */
    TFTPc_ERR_FAULT_INIT,                               /* Initialization faulted.                              */
    TFTPc_ERR_MEM_ALLOC,                                /* Memory allocation error.                             */
    TFTPc_ERR_CFG_INVALID,                              /* Invalid Configuration.                               */

    TFTPc_ERR_NO_SOCK,                                  /* No socket available.                                 */
    TFTPc_ERR_RX,                                       /* Rx err.                                              */
    TFTPc_ERR_RX_TIMEOUT,                               /* Rx timeout.                                          */
    TFTPc_ERR_ERR_PKT_RX,                               /* Err pkt rx'd.                                        */
    TFTPc_ERR_TX,                                       /* Tx err.                                              */
    TFTPc_ERR_NULL_PTR,                                 /* Ptr arg(s) passed NULL ptr(s).                       */
    TFTPc_ERR_INVALID_MODE,                             /* Invalid TFTP transfer mode.                          */
    TFTPc_ERR_INVALID_OPCODE,                           /* Invalid opcode for req.                              */
    TFTPc_ERR_INVALID_OPCODE_RX,                        /* Invalid opcode rx'd.                                 */
    TFTPC_ERR_FILE_OPEN,                                /* Could not open the specified file.                   */
    TFTPc_ERR_FILE_RD,                                  /* Err rd'ing from file.                                */
    TFTPc_ERR_FILE_WR,                                  /* Err wr'ing to   file.                                */
    TFTPc_ERR_INVALID_STATE,                            /* Invalid state for TFTP client state machine.         */
    TFTPc_ERR_INVALID_PROTO_FAMILY                      /* Invalid or unsupported protocol family.              */
} TFTPc_ERR;


/*
*********************************************************************************************************
*                                        TFTPc MODE DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT08U  TFTPc_MODE;


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

CPU_BOOLEAN  TFTPc_Init       (const  TFTPc_CFG      *p_cfg,
                                      TFTPc_ERR      *p_err);

CPU_BOOLEAN  TFTPc_SetDfltCfg (const  TFTPc_CFG      *p_cfg,
                                      TFTPc_ERR      *p_err);

CPU_BOOLEAN  TFTPc_Get        (const  TFTPc_CFG      *p_cfg,
                                      CPU_CHAR       *p_filename_local,
                                      CPU_CHAR       *p_filename_remote,
                                      TFTPc_MODE      mode,
                                      TFTPc_ERR      *p_err);

CPU_BOOLEAN  TFTPc_Put        (const  TFTPc_CFG      *p_cfg,
                                      CPU_CHAR       *p_filename_local,
                                      CPU_CHAR       *p_filename_remote,
                                      TFTPc_MODE      mode,
                                      TFTPc_ERR      *p_err);


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               TRACING
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                /* Trace level, default to TRACE_LEVEL_OFF              */
#ifndef  TRACE_LEVEL_OFF
#define  TRACE_LEVEL_OFF                                   0
#endif

#ifndef  TRACE_LEVEL_INFO
#define  TRACE_LEVEL_INFO                                  1
#endif

#ifndef  TRACE_LEVEL_DBG
#define  TRACE_LEVEL_DBG                                   2
#endif

#ifndef  TFTPc_TRACE_LEVEL
#define  TFTPc_TRACE_LEVEL                   TRACE_LEVEL_OFF
#endif

#ifndef  TFTPc_TRACE
#define  TFTPc_TRACE                                  printf
#endif

#if    ((defined(TFTPc_TRACE))       && \
        (defined(TFTPc_TRACE_LEVEL)) && \
        (TFTPc_TRACE_LEVEL >= TRACE_LEVEL_INFO) )

    #if  (TFTPc_TRACE_LEVEL >= TRACE_LEVEL_DBG)
        #define  TFTPc_TRACE_DBG(msg)     TFTPc_TRACE  msg
    #else
        #define  TFTPc_TRACE_DBG(msg)
    #endif

    #define  TFTPc_TRACE_INFO(msg)        TFTPc_TRACE  msg

#else
    #define  TFTPc_TRACE_DBG(msg)
    #define  TFTPc_TRACE_INFO(msg)
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif  /* TFTPc_MODULE_PRESENT  */

