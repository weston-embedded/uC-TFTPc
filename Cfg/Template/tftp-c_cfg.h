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
*                                   TFTP CLIENT CONFIGURATION FILE
*
*                                              TEMPLATE
*
* Filename : tftp-c_cfg.h
* Version  : V2.01.00
*********************************************************************************************************
*/

#ifndef TFTPc_CFG_MODULE_PRESENT
#define TFTPc_CFG_MODULE_PRESENT

#include  <Source/tftp-c_type.h>


/*
*********************************************************************************************************
*                                  TFTPc ARGUMENT CHECK CONFIGURATION
*
* Note(s) : (1) Configure TFTPc_CFG_ARG_CHK_EXT_EN to enable/disable the TFTP client external argument
*               check feature :
*
*               (a) When ENABLED,  ALL arguments received from any port interface provided by the developer
*                   are checked/validated.
*
*               (b) When DISABLED, NO  arguments received from any port interface provided by the developer
*                   are checked/validated.
*********************************************************************************************************
*/
                                                                /* Configure external argument check feature ...        */
                                                                /* See Note 1.                                          */
#define  TFTPc_CFG_ARG_CHK_EXT_EN                    DEF_DISABLED
                                                                /* DEF_DISABLED     External argument check DISABLED    */
                                                                /* DEF_ENABLED      External argument check ENABLED     */

/*
*********************************************************************************************************
*                                TFTPc RUN-TIME STRUCTURE CONFIGURATION
*
* Note(s) : (1) This structure should be defined into a 'C' file.
*********************************************************************************************************
*/

extern  const  TFTPc_CFG       TFTPc_Cfg;                       /* Must always be defined.                              */


/*
*********************************************************************************************************
*                                               TRACING
*********************************************************************************************************
*/

#ifndef  TRACE_LEVEL_OFF
#define  TRACE_LEVEL_OFF                                   0
#endif

#ifndef  TRACE_LEVEL_INFO
#define  TRACE_LEVEL_INFO                                  1
#endif

#ifndef  TRACE_LEVEL_DBG
#define  TRACE_LEVEL_DBG                                   2
#endif

#define  TFTPc_TRACE_LEVEL                   TRACE_LEVEL_DBG
#define  TFTPc_TRACE                                  printf


#endif
