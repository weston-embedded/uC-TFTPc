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
* Filename : tftp-c.c
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
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    TFTPc_MODULE
#include  "tftp-c.h"
#include  <Source/net_util.h>
#include  <Source/net_app.h>
#include  <KAL/kal.h>

/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       FILE OPEN MODE DEFINES
*********************************************************************************************************
*/

#define  TFTPc_FILE_OPEN_RD                                0
#define  TFTPc_FILE_OPEN_WR                                1


/*
*********************************************************************************************************
*                                         TFTP OPCODE DEFINES
*********************************************************************************************************
*/

#define  TFTP_OPCODE_RRQ                                   1
#define  TFTP_OPCODE_WRQ                                   2
#define  TFTP_OPCODE_DATA                                  3
#define  TFTP_OPCODE_ACK                                   4
#define  TFTP_OPCODE_ERR                                   5


/*
*********************************************************************************************************
*                                       TFTP PKT OFFSET DEFINES
*********************************************************************************************************
*/

#define  TFTP_PKT_OFFSET_OPCODE                            0
#define  TFTP_PKT_OFFSET_FILENAME                          2
#define  TFTP_PKT_OFFSET_BLK_NBR                           2
#define  TFTP_PKT_OFFSET_ERR_CODE                          2
#define  TFTP_PKT_OFFSET_ERR_MSG                           4
#define  TFTP_PKT_OFFSET_DATA                              4


/*
*********************************************************************************************************
*                                     TFTP PKT FIELD SIZE DEFINES
*********************************************************************************************************
*/

#define  TFTP_PKT_SIZE_OPCODE                              2
#define  TFTP_PKT_SIZE_BLK_NBR                             2
#define  TFTP_PKT_SIZE_ERR_CODE                            2
#define  TFTP_PKT_SIZE_NULL                                1


/*
*********************************************************************************************************
*                                     TFTP TRANSFER MODE DEFINES
*********************************************************************************************************
*/

#define  TFTP_MODE_NETASCII_STR                    "netascii"
#define  TFTP_MODE_NETASCII_STR_LEN                        8
#define  TFTP_MODE_BINARY_STR                         "octet"
#define  TFTP_MODE_BINARY_STR_LEN                          5


/*
*********************************************************************************************************
*                                          TFTP PKT DEFINES
*********************************************************************************************************
*/

#define  TFTPc_DATA_BLOCK_SIZE                           512
#define  TFTPc_PKT_BUF_SIZE                     (TFTPc_DATA_BLOCK_SIZE + TFTP_PKT_SIZE_OPCODE + TFTP_PKT_SIZE_BLK_NBR)

#define  TFTPc_MAX_NBR_TX_RETRY                            3


/*
*********************************************************************************************************
*                                          TFTP ERROR CODES
*********************************************************************************************************
*/

#define  TFTP_ERR_CODE_NOT_DEF                             0    /* Not defined, see err message (if any).               */
#define  TFTP_ERR_CODE_FILE_NOT_FOUND                      1    /* File not found.                                      */
#define  TFTP_ERR_CODE_ACCESS_VIOLATION                    2    /* Access violation.                                    */
#define  TFTP_ERR_CODE_DISK_FULL                           3    /* Disk full of allocation exceeded.                    */
#define  TFTP_ERR_CODE_ILLEGAL_OP                          4    /* Illegal TFTP operation.                              */
#define  TFTP_ERR_CODE_UNKNOWN_ID                          5    /* Unknown transfer ID.                                 */
#define  TFTP_ERR_CODE_FILE_EXISTS                         6    /* File already exists.                                 */
#define  TFTP_ERR_CODE_NO_USER                             7    /* No such user.                                        */


/*
*********************************************************************************************************
*                                        TFTPc ERROR MESSAGES
*********************************************************************************************************
*/

#define  TFTPc_ERR_MSG_WR_ERR              "File write error"
#define  TFTPc_ERR_MSG_RD_ERR              "File read error"


/*
*********************************************************************************************************
*                                     TFTPc CLIENT STATE DEFINES
*********************************************************************************************************
*/

#define  TFTPc_STATE_DATA_GET                              1
#define  TFTPc_STATE_DATA_PUT                              2
#define  TFTPc_STATE_DATA_PUT_WAIT_LAST_ACK                3
#define  TFTPc_STATE_TRANSFER_COMPLETE                     4


/*
*********************************************************************************************************
*********************************************************************************************************
*                                        LOCAL DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                    FILE ACCESS MODE DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT08U  TFTPc_FILE_ACCESS;


/*
*********************************************************************************************************
*                                    TFTPc BLOCK NUMBER DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT16U  TFTPc_BLK_NBR;


/*
*********************************************************************************************************
*                                   TFTPc SERVER OBJECT DATA TYPE
*********************************************************************************************************
*/

typedef  struct  tftpc_server_obj {
    CPU_CHAR           *HostnamePtr;
    NET_PORT_NBR        PortNbr;
    NET_IP_ADDR_FAMILY  AddrFamily;
} TFTPc_SERVER_OBJ;


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

static  KAL_LOCK_HANDLE      TFTPc_LockHandle;                  /* Lock Handle.                                         */

static  TFTPc_CFG           *TFTPc_DfltCfgPtr;                  /* Default TFTP Configuration Pointer.                  */

static  NET_IP_ADDR_FAMILY   TFTPc_ServerAddrFamily;            /* IP address family of TFTP server.                    */

static  CPU_INT16U           TFTPc_RxBlkNbrNext;                /* Next rx'd blk nbr expected.                          */

static  CPU_INT08U           TFTPc_RxPktBuf[TFTPc_PKT_BUF_SIZE];/* Last rx'd pkt buf.                                   */
static  CPU_INT32S           TFTPc_RxPktLen;                    /* Last rx'd pkt len.                                   */
static  CPU_INT16U           TFTPc_RxPktOpcode;                 /* Last rx'd pkt opcode.                                */

static  CPU_INT16U           TFTPc_TxPktBlkNbr;                 /* Last tx'd pkt blk nbr.                               */
static  CPU_INT08U           TFTPc_TxPktBuf[TFTPc_PKT_BUF_SIZE];/* Last tx'd pkt buf.                                   */
static  CPU_INT16U           TFTPc_TxPktLen;                    /* Last tx'd pkt len.                                   */
static  CPU_INT08U           TFTPc_TxPktRetry;                  /* Nbr of time last tx'd pkt had been sent.             */

static  NET_SOCK_ID          TFTPc_SockID;                      /* Client sock id.                                      */
static  NET_SOCK_ADDR        TFTPc_SockAddr;                    /* Server sock addr IP.                                 */

static  CPU_BOOLEAN          TFTPc_TID_Set;                     /* Indicates whether the terminal ID is set or not.     */

static  CPU_INT08U           TFTPc_State;                       /* Cur state of TFTPc state machine.                    */

static  void                *TFTPc_FileHandle;                  /* Handle to cur opened file.                           */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                /* -------------------- LOCK FNCT --------------------- */
static  void                TFTPc_LockAcquire   (       TFTPc_ERR           *p_err);

static  void                TFTPc_LockRelease   (void);

                                                                /* -------------------- INIT FNCT --------------------- */
static  void                TFTPc_InitSession   (void);

static  CPU_BOOLEAN         TFTPc_SockInit      (       CPU_CHAR            *p_server_hostname,
                                                        NET_PORT_NBR         server_port,
                                                        NET_IP_ADDR_FAMILY   ip_family,
                                                        TFTPc_ERR           *p_err);


                                                                /* ----------------- PROCESSING FNCTS ----------------- */
static  void                TFTPc_Processing    (const  TFTPc_CFG           *p_cfg,
                                                        TFTPc_ERR           *p_err);

static  void                TFTPc_StateDataGet  (       TFTPc_ERR           *p_err);

static  void                TFTPc_StateDataPut  (       TFTPc_ERR           *p_err);

static  CPU_INT16U          TFTPc_GetRxBlkNbr   (void);


                                                                /* ---------------- FILE ACCESS FNCTS ----------------- */
static  void               *TFTPc_FileOpenMode  (       CPU_CHAR            *p_filename,
                                                        TFTPc_FILE_ACCESS    file_access);

static  CPU_INT16U          TFTPc_DataWr        (       TFTPc_ERR           *p_err);

static  CPU_INT16U          TFTPc_DataRd        (       TFTPc_ERR           *p_err);


                                                                /* --------------------- RX FNCTS --------------------- */
static  NET_SOCK_RTN_CODE   TFTPc_RxPkt         (       NET_SOCK_ID          sock_id,
                                                        void                *p_pkt,
                                                        CPU_INT16U           pkt_len,
                                                        TFTPc_ERR           *p_err);


                                                                /* --------------------- TX FNCTS --------------------- */
static  void                TFTPc_TxReq         (       CPU_INT16U           req_opcode,
                                                        CPU_CHAR            *p_filename,
                                                        TFTPc_MODE           mode,
                                                        TFTPc_ERR           *p_err);

static  void                TFTPc_TxData        (       TFTPc_BLK_NBR        blk_nbr,
                                                        CPU_INT16U           data_len,
                                                        TFTPc_ERR           *p_err);

static  void                TFTPc_TxAck         (       TFTPc_BLK_NBR        blk_nbr,
                                                        TFTPc_ERR           *p_err);

static  void                TFTPc_TxErr         (       CPU_INT16U           err_code,
                                                        CPU_CHAR            *p_err_msg,
                                                        TFTPc_ERR           *p_err);

static  NET_SOCK_RTN_CODE   TFTPc_TxPkt         (       NET_SOCK_ID          sock_id,
                                                        void                *p_pkt,
                                                        CPU_INT16U           pkt_len,
                                                        NET_SOCK_ADDR       *p_addr_remote,
                                                        NET_SOCK_ADDR_LEN    addr_len,
                                                        TFTPc_ERR           *p_err);


static  void                TFTPc_Terminate     (void);


/*
*********************************************************************************************************
*                                             TFTPc_Init()
*
* Description : (1) Initialize the TFTPc suite.
*
*                   (a) Create TFTPc Lock.
*                   (b) Save pointer to TFTPc Configuration.
*
*
* Argument(s) : p_cfg   Pointer to TFTPc Configuration to use as default.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           TFTPc_ERR_NONE          Initialization was successful.
*                           TFTPc_ERR_NULL_PTR      Null pointer was passed as argument.
*                           TFTPc_ERR_MEM_ALLOC     Memory error while creating lock.
*                           TFTPc_ERR_FAULT_INIT    TFTPc Initialization faulted.
*
*                           ------------ RETURNED BY TFTPc_SetDfltCfg() ------------
*                           See TFTPc_SetDfltCfg() for additional return error codes.
*
* Return(s)   : DEF_OK,   if initialization was successful.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : None.
*********************************************************************************************************
*/

CPU_BOOLEAN  TFTPc_Init (const  TFTPc_CFG  *p_cfg,
                                TFTPc_ERR  *p_err)
{
    CPU_BOOLEAN  result;
    KAL_ERR      err_kal;


#if (TFTPc_CFG_ARG_CHK_EXT_EN == DEF_ENABELD)
    if (p_err == DEF_NULL) {
        CPU_SW_EXCEPTION(;);
    }

    if (p_cfg == DEF_NULL) {
       *p_err = TFTPc_ERR_NULL_PTR;
        goto exit;
    }
#endif

                                                                /* ---------------- CREATE TFTPC LOCK ----------------- */
    TFTPc_LockHandle = KAL_LockCreate("TFTPc Lock",
                                       KAL_OPT_CREATE_NONE,
                                      &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             break;

        case KAL_ERR_MEM_ALLOC:
             result = DEF_FAIL;
            *p_err  = TFTPc_ERR_MEM_ALLOC;
             goto exit;

        default:
             result = DEF_FAIL;
            *p_err  = TFTPc_ERR_FAULT_INIT;
             goto exit;
    }

                                                                /* ------------ SET DEFAULT CONFIGURATION ------------- */
   (void)TFTPc_SetDfltCfg(p_cfg, p_err);
    if (*p_err != TFTPc_ERR_NONE) {
         result = DEF_FAIL;
         goto exit;
    }

    result = DEF_OK;
   *p_err = TFTPc_ERR_NONE;


exit:
    return (result);
}


/*
*********************************************************************************************************
*                                          TFTPc_SetDfltCfg()
*
* Description : Set default TFTPc Configuration.
*
* Argument(s) : p_cfg   Pointer to TFTPc Configuration to set as default.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           TFTPc_ERR_NONE          Configuration set up was successful.
*                           TFTPc_ERR_NULL_PTR      Null pointer was passed as argument.
*
*                           ------------ RETURNED BY TFTPc_LockAcquire() ------------
*                           See TFTPc_LockAcquire() for additional return error codes.
*
* Return(s)   : DEF_OK,   if configuration was set up successfully.
*               DEF_FAIl, otherwise.
*
* Caller(s)   : Application,
*               TFTPc_Init().
*
* Note(s)     : None.
*********************************************************************************************************
*/

CPU_BOOLEAN  TFTPc_SetDfltCfg (const  TFTPc_CFG  *p_cfg,
                                      TFTPc_ERR  *p_err)
{
    CPU_BOOLEAN  result;


#if (TFTPc_CFG_ARG_CHK_EXT_EN == DEF_ENABELD)
    if (p_err == DEF_NULL) {
        CPU_SW_EXCEPTION(;);
    }

    if (p_cfg == DEF_NULL) {
       *p_err = TFTPc_ERR_NULL_PTR;
        goto exit;
    }
#endif

    TFTPc_LockAcquire(p_err);
    if (*p_err != TFTPc_ERR_NONE) {
        result = DEF_FAIL;
        goto exit;
    }

    TFTPc_DfltCfgPtr = (TFTPc_CFG *)p_cfg;

    TFTPc_ServerAddrFamily = p_cfg->ServerAddrFamily;

    result = DEF_OK;
   *p_err  = TFTPc_ERR_NONE;

    TFTPc_LockRelease();


exit:
    return (result);
}


/*
*********************************************************************************************************
*                                             TFTPc_Get()
*
* Description : Get a file from the TFTP server.
*
* Argument(s) : p_cfg               Pointer to TFTPc Configuration to use.
*
*                                       DEF_NULL, if default configuration must be used.
*
*               p_filename_local    Pointer to name of the file to be written by   the client.
*
*               p_filename_remote   Pointer to name of the file to be read    from the server.
*
*               mode                TFTP transfer mode :
*
*                                       TFTPc_MODE_NETASCII     ASCII  mode.
*                                       TFTPc_MODE_OCTET        Binary mode.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               TFTPc_ERR_NONE          TFTP operation was successful.
*                               TFTPC_ERR_FILE_OPEN     File opening failed.
*                               TFTPc_ERR_TX            Transmission of TFTP request faulted.
*
*                               ------------ RETURNED BY TFTPc_LockAcquire() ------------
*                               See TFTPc_LockAcquire() for additional return error codes.
*
*                               ------------ RETURNED BY TFTPc_SockInit() ------------
*                               See TFTPc_SockInit() for additional return error codes.
*
*                               ------------ RETURNED BY TFTPc_Processing() ------------
*                               See TFTPc_Processing() for additional return error codes.
*
* Return(s)   : DEF_OK,   if file was get from server successfully.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a TFTP client application interface (API) function & MAY be called by
*               application function(s).
*
* Note(s)     : None.
*********************************************************************************************************
*/

CPU_BOOLEAN  TFTPc_Get (const  TFTPc_CFG     *p_cfg,
                               CPU_CHAR      *p_filename_local,
                               CPU_CHAR      *p_filename_remote,
                               TFTPc_MODE     mode,
                               TFTPc_ERR     *p_err)
{
    TFTPc_CFG           *p_cfg_to_use;
    CPU_CHAR            *p_server_hostname;
    NET_PORT_NBR         server_port;
    NET_IP_ADDR_FAMILY   ip_family;
    NET_IP_ADDR_FAMILY   ip_family_tmp;
    CPU_BOOLEAN          result;
    CPU_BOOLEAN          is_hostname;
    CPU_BOOLEAN          retry;


#if (TFTPc_CFG_ARG_CHK_EXT_EN == DEF_ENABELD)
    if (p_err == DEF_NULL) {
        CPU_SW_EXCEPTION(;);
    }

    if (p_filename_local == DEF_NULL) {
       *p_err = TFTPc_ERR_NULL_PTR;
        goto exit;
    }

    if (p_filename_remote == DEF_NULL) {
       *p_err = TFTPc_ERR_NULL_PTR;
        goto exit;
    }
#endif

    TFTPc_LockAcquire(p_err);
    if (*p_err != TFTPc_ERR_NONE) {
        result = DEF_FAIL;
        goto exit;
    }

    TFTPc_TRACE_INFO(("TFTPc_Get: Request for %s\n\r", p_filename_remote));

    TFTPc_InitSession();

    if (p_cfg == DEF_NULL) {
        p_cfg_to_use      = TFTPc_DfltCfgPtr;
        p_server_hostname = TFTPc_DfltCfgPtr->ServerHostnamePtr;
        server_port       = TFTPc_DfltCfgPtr->ServerPortNbr;
        ip_family         = TFTPc_ServerAddrFamily;
    } else {
        p_cfg_to_use      = (TFTPc_CFG *)p_cfg;
        p_server_hostname = p_cfg->ServerHostnamePtr;
        server_port       = p_cfg->ServerPortNbr;
        ip_family         = p_cfg->ServerAddrFamily;
    }

    if (ip_family == NET_IP_ADDR_FAMILY_NONE) {
        ip_family_tmp = NET_IP_ADDR_FAMILY_IPv6;
    } else {
        ip_family_tmp = ip_family;
    }

                                                                /* Open file                                            */
    TFTPc_FileHandle = TFTPc_FileOpenMode(p_filename_local, TFTPc_FILE_OPEN_WR);
    if (TFTPc_FileHandle == (void *)0) {
        TFTPc_Terminate();
        result = DEF_FAIL;
       *p_err  = TFTPC_ERR_FILE_OPEN;
        goto exit_release;
    }

    retry     = DEF_YES;
    while (retry == DEF_YES) {
        is_hostname = TFTPc_SockInit(p_server_hostname,         /* Init sock.                                           */
                                     server_port,
                                     ip_family_tmp,
                                     p_err);
        if (*p_err != TFTPc_ERR_NONE) {
            if ((ip_family     == NET_IP_ADDR_FAMILY_NONE) &&
                (ip_family_tmp == NET_IP_ADDR_FAMILY_IPv6) &&
                (is_hostname   == DEF_YES)                ) {
                 retry         = DEF_YES;
                 ip_family_tmp = NET_IP_ADDR_FAMILY_IPv4;
            } else {
                TFTPc_Terminate();
                retry  = DEF_NO;
                result = DEF_FAIL;
                goto exit_release;
            }
        } else {
            retry = DEF_NO;
        }

        if (retry == DEF_NO) {
                                                                /* Tx rd req.                                           */
            TFTPc_TxReq(TFTP_OPCODE_RRQ, p_filename_remote, mode, p_err);
            if (*p_err != TFTPc_ERR_NONE) {
                if ((ip_family     == NET_IP_ADDR_FAMILY_NONE) &&
                    (ip_family_tmp == NET_IP_ADDR_FAMILY_IPv6) &&
                    (is_hostname   == DEF_YES)                ) {
                     retry         = DEF_YES;
                     ip_family_tmp = NET_IP_ADDR_FAMILY_IPv4;
                } else {
                    TFTPc_Terminate();
                    retry  = DEF_NO;
                    result = DEF_FAIL;
                   *p_err  = TFTPc_ERR_TX;
                    goto exit_release;
                }
            } else {
                retry = DEF_NO;
            }

            if (retry == DEF_NO) {
                                                                /* Process req.                                         */
                TFTPc_RxBlkNbrNext = 1;
                TFTPc_State        = TFTPc_STATE_DATA_GET;

                TFTPc_Processing(p_cfg_to_use, p_err);
                if (*p_err != TFTPc_ERR_NONE) {
                     result = DEF_FAIL;
                     goto exit_release;
                }

                TFTPc_ServerAddrFamily = ip_family_tmp;
            }

        }
    }

    result = DEF_OK;
   *p_err  = TFTPc_ERR_NONE;


exit_release:
    TFTPc_LockRelease();

exit:
    return (result);
}


/*
*********************************************************************************************************
*                                             TFTPc_Put()
*
* Description : Put a file on the TFTP server.
*
* Argument(s) : p_cfg               Pointer to TFTPc Configuration to use.
*
*                                       DEF_NULL, if default configuration must be used.
*
*               p_filename_local    Pointer to name of the file to be read    by the client.
*
*               p_filename_remote   Pointer to name of the file to be written to the server.
*
*               mode                TFTP transfer mode :
*
*                                       TFTPc_MODE_NETASCII     ASCII  mode.
*                                       TFTPc_MODE_OCTET        Binary mode.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               TFTPc_ERR_NONE          TFTP operation was successful.
*                               TFTPC_ERR_FILE_OPEN     File opening failed.
*                               TFTPc_ERR_TX            Transmission of TFTP request faulted.
*
*                               ------------ RETURNED BY TFTPc_LockAcquire() ------------
*                               See TFTPc_LockAcquire() for additional return error codes.
*
*                               ------------ RETURNED BY TFTPc_SockInit() ------------
*                               See TFTPc_SockInit() for additional return error codes.
*
*                               ------------ RETURNED BY TFTPc_Processing() ------------
*                               See TFTPc_Processing() for additional return error codes.
*
* Return(s)   : DEF_OK,   if file was put on server successfully.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a TFTP client application interface (API) function & MAY be called by
*               application function(s).
*
* Note(s)     : None.
*********************************************************************************************************
*/

CPU_BOOLEAN  TFTPc_Put (const  TFTPc_CFG   *p_cfg,
                               CPU_CHAR    *p_filename_local,
                               CPU_CHAR    *p_filename_remote,
                               TFTPc_MODE   mode,
                               TFTPc_ERR   *p_err)
{
    TFTPc_CFG           *p_cfg_to_use;
    CPU_CHAR            *p_server_hostname;
    NET_PORT_NBR         server_port;
    NET_IP_ADDR_FAMILY   ip_family;
    NET_IP_ADDR_FAMILY   ip_family_tmp;
    CPU_BOOLEAN          result;
    CPU_BOOLEAN          is_hostname;
    CPU_BOOLEAN          retry;


#if (TFTPc_CFG_ARG_CHK_EXT_EN == DEF_ENABELD)
    if (p_err == DEF_NULL) {
        CPU_SW_EXCEPTION(;);
    }

    if (p_filename_local == DEF_NULL) {
       *p_err = TFTPc_ERR_NULL_PTR;
        goto exit;
    }

    if (p_filename_remote == DEF_NULL) {
       *p_err = TFTPc_ERR_NULL_PTR;
        goto exit;
    }
#endif

    TFTPc_LockAcquire(p_err);
    if (*p_err != TFTPc_ERR_NONE) {
        result = DEF_FAIL;
        goto exit;
    }

    TFTPc_TRACE_INFO(("TFTPc_Put: Request for %s\n\r", p_filename_local));

    TFTPc_InitSession();

    if (p_cfg == DEF_NULL) {
        p_cfg_to_use      = TFTPc_DfltCfgPtr;
        p_server_hostname = TFTPc_DfltCfgPtr->ServerHostnamePtr;
        server_port       = TFTPc_DfltCfgPtr->ServerPortNbr;
        ip_family         = TFTPc_ServerAddrFamily;
    } else {
        p_cfg_to_use      = (TFTPc_CFG *)p_cfg;
        p_server_hostname = p_cfg->ServerHostnamePtr;
        server_port       = p_cfg->ServerPortNbr;
        ip_family         = p_cfg->ServerAddrFamily;
    }

    if (ip_family == NET_IP_ADDR_FAMILY_NONE) {
        ip_family_tmp = NET_IP_ADDR_FAMILY_IPv6;
    } else {
        ip_family_tmp = ip_family;
    }

                                                                /* Open file.                                           */
    TFTPc_FileHandle = TFTPc_FileOpenMode(p_filename_local, TFTPc_FILE_OPEN_RD);
    if (TFTPc_FileHandle == (void *)0) {
        result = DEF_FAIL;
       *p_err  = TFTPC_ERR_FILE_OPEN;
        goto exit_release;
    }

    retry     = DEF_YES;
    while (retry == DEF_YES) {

        is_hostname = TFTPc_SockInit(p_server_hostname,         /* Init sock.                                           */
                                     server_port,
                                     ip_family_tmp,
                                     p_err);
        if (*p_err != TFTPc_ERR_NONE) {
            if ((ip_family     == NET_IP_ADDR_FAMILY_NONE) &&
                (ip_family_tmp == NET_IP_ADDR_FAMILY_IPv6) &&
                (is_hostname   == DEF_YES)                ) {
                 retry     = DEF_YES;
                 ip_family_tmp = NET_IP_ADDR_FAMILY_IPv4;

            } else {
                TFTPc_Terminate();
                retry  = DEF_NO;
                result = DEF_FAIL;
                goto exit_release;
            }
        } else {
            retry = DEF_NO;
        }

        if (retry == DEF_NO) {
                                                                /* Tx rd to server.                                     */
            TFTPc_TxReq(TFTP_OPCODE_WRQ, p_filename_remote, mode, p_err);
            if (*p_err != TFTPc_ERR_NONE) {
                if ((ip_family     == NET_IP_ADDR_FAMILY_NONE) &&
                    (ip_family_tmp == NET_IP_ADDR_FAMILY_IPv6) &&
                    (is_hostname   == DEF_YES)                ) {
                     retry         = DEF_YES;
                     ip_family_tmp = NET_IP_ADDR_FAMILY_IPv4;
                } else {
                    TFTPc_Terminate();
                    retry  = DEF_NO;
                    result = DEF_FAIL;
                   *p_err  = TFTPc_ERR_TX;
                    goto exit_release;
                }
            } else {
                retry = DEF_NO;
            }

            if (retry == DEF_NO) {
                                                                /* Process req.                                         */
                TFTPc_TxPktBlkNbr = 0;
                TFTPc_State       = TFTPc_STATE_DATA_PUT;

                TFTPc_Processing(p_cfg_to_use, p_err);
                if (*p_err != TFTPc_ERR_NONE) {
                     result = DEF_FAIL;
                     goto exit_release;
                }

                TFTPc_ServerAddrFamily = ip_family_tmp;
            }
        }
    }

    result = DEF_OK;
   *p_err  = TFTPc_ERR_NONE;


exit_release:
    TFTPc_LockRelease();

exit:
    return (result);
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          TFTPc_LockAcquire()
*
* Description : $$$$ Add function description.
*
* Argument(s) : p_err   $$$$ Add description for 'p_err'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  TFTPc_LockAcquire (TFTPc_ERR  *p_err)
{
    KAL_ERR  err;


    KAL_LockAcquire(TFTPc_LockHandle, KAL_OPT_PEND_NONE, 0, &err);
    if (err != KAL_ERR_NONE) {
       *p_err = TFTPc_ERR_LOCK;
        goto exit;
    }

   *p_err = TFTPc_ERR_NONE;


exit:
    return;
}


/*
*********************************************************************************************************
*                                          TFTPc_LockRelease()
*
* Description : $$$$ Add function description.
*
* Argument(s) : none.
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  TFTPc_LockRelease (void)
{
    KAL_ERR  err;


    KAL_LockRelease(TFTPc_LockHandle, &err);
}


/*
*********************************************************************************************************
*                                          TFTPc_InitSession()
*
* Description : Initialize the TFTP session.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : TFTPc_Get(),
*               TFTPc_Put().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  TFTPc_InitSession (void)
{
    TFTPc_SockID     =  NET_SOCK_ID_NONE;
    TFTPc_FileHandle = (void *)0;

    TFTPc_RxPktLen   =  0;
    TFTPc_TxPktLen   =  0;

    TFTPc_TxPktRetry =  0;

    TFTPc_TID_Set    =  DEF_NO;
}


/*
*********************************************************************************************************
*                                          TFTPc_SockInit()
*
* Description : Initialize the communication socket.
*
* Argument(s) : p_server_hostname     Pointer to hostname or IP address string of the TFTP server.
*
*               server_port           Port number of the TFTP server.
*
* Return(s)   : Error message:
*
*               TFTPc_ERR_NONE                      No error.
*               TFTPc_ERR_NO_SOCK                   Could not open socket.
*
* Caller(s)   : TFTPc_Get(),
*               TFTPc_Put().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  TFTPc_SockInit (CPU_CHAR            *p_server_hostname,
                                     NET_PORT_NBR         server_port,
                                     NET_IP_ADDR_FAMILY   ip_family,
                                     TFTPc_ERR           *p_err)
{
    NET_SOCK_ID    *p_sock_id;
    NET_SOCK_ADDR  *p_server_sock_addr;
    CPU_BOOLEAN     is_hostname;
    NET_ERR         err;


    p_sock_id          = &TFTPc_SockID;
    p_server_sock_addr = &TFTPc_SockAddr;

    (void)NetApp_ClientDatagramOpenByHostname(p_sock_id,
                                              p_server_hostname,
                                              server_port,
                                              ip_family,
                                              p_server_sock_addr,
                                             &is_hostname,
                                             &err);
    if (err != NET_APP_ERR_NONE) {
       *p_err = TFTPc_ERR_INVALID_PROTO_FAMILY;
        goto exit;
    }

   (void)NetSock_CfgBlock(*p_sock_id, NET_SOCK_BLOCK_SEL_BLOCK, &err);

   *p_err = TFTPc_ERR_NONE;


exit:
    return (is_hostname);
}


/*
*********************************************************************************************************
*                                         TFTPc_Processing()
*
* Description : Process data transfer.
*
* Argument(s) : p_cfg       Pointer to TFTPc configuration object.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               TFTPc_ERR_NONE                  Data transfer successful.
*
*                                                               ------- RETURNED BY TFTPc_RxPkt() : -------
*                               TFTPc_ERR_RX_TIMEOUT            Receive timeout.
*                               TFTPc_ERR_RX                    Error receiving packet.
*
*                                                               ----- RETURNED BY TFTPc_StateDataGet() : -----
*                               TFTPc_ERR_ERR_PKT_RX            Error packet   received.
*                               TFTPc_ERR_INVALID_OPCODE_RX     Invalid opcode received.
*                               TFTPc_ERR_FILE_WR               Error writing to file.
*                               TFTPc_ERR_TX                    Error transmitting packet.
*
*                                                               ----- RETURNED BY TFTPc_StateDataPut() : -----
*                               TFTPc_ERR_ERR_PKT_RX            Error packet   received.
*                               TFTPc_ERR_INVALID_OPCODE_RX     Invalid opcode received.
*                               TFTPc_ERR_INVALID_STATE         Invalid state machine state.
*                               TFTPc_ERR_FILE_RD               Error reading file.
*                               TFTPc_ERR_TX                    Error transmitting packet.
*
*                                                               ----- RETURNED BY TFTPc_TxPkt() : -----
*                               TFTPc_ERR_TX                    Error transmitting packet.
*
* Return(s)   : none.
*
* Caller(s)   : TFTPc_Get(),
*               TFTPc_Put().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  TFTPc_Processing (const  TFTPc_CFG  *p_cfg,
                                       TFTPc_ERR  *p_err)
{
    NET_SOCK_RTN_CODE  rx_pkt_len;
    NET_SOCK_ADDR_LEN  sock_addr_size;
    CPU_INT32U         timeout;
    NET_ERR            err_net;


                                                                /* Set rx sock timeout.                                 */
    timeout = p_cfg->RxInactivityTimeout_ms;
    NetSock_CfgTimeoutRxQ_Set(TFTPc_SockID,
                              timeout,
                             &err_net);



    while (TFTPc_State != TFTPc_STATE_TRANSFER_COMPLETE) {

        rx_pkt_len = TFTPc_RxPkt((NET_SOCK_ID    ) TFTPc_SockID,
                                 (void          *)&TFTPc_RxPktBuf[0],
                                 (CPU_INT16U     ) sizeof(TFTPc_RxPktBuf),
                                 (TFTPc_ERR     *) p_err);
        switch (*p_err) {
            case TFTPc_ERR_NONE:
                 TFTPc_RxPktLen    = rx_pkt_len;
                 TFTPc_RxPktOpcode = NET_UTIL_VAL_GET_NET_16(&TFTPc_RxPktBuf[TFTP_PKT_OFFSET_OPCODE]);

                 switch (TFTPc_State) {
                     case TFTPc_STATE_DATA_GET:
                          TFTPc_StateDataGet(p_err);
                          break;


                     case TFTPc_STATE_DATA_PUT:
                     case TFTPc_STATE_DATA_PUT_WAIT_LAST_ACK:
                          TFTPc_StateDataPut(p_err);
                          break;


                     default:
                         *p_err = TFTPc_ERR_INVALID_STATE;
                          break;
                 }

                 break;


            case TFTPc_ERR_RX_TIMEOUT:
                 if (TFTPc_TxPktLen > 0) {                      /* If pkt tx'd ...                                      */
                                                                /* ... and max retry NOT reached, ...                   */
                     if (TFTPc_TxPktRetry < TFTPc_MAX_NBR_TX_RETRY) {
                                                                /* ... re-tx last tx'd pkt.                             */
                          sock_addr_size = sizeof(NET_SOCK_ADDR);
                         (void)TFTPc_TxPkt((NET_SOCK_ID      ) TFTPc_SockID,
                                           (void            *)&TFTPc_TxPktBuf[0],
                                           (CPU_INT16U       ) TFTPc_TxPktLen,
                                           (NET_SOCK_ADDR   *)&TFTPc_SockAddr,
                                           (NET_SOCK_ADDR_LEN) sock_addr_size,
                                           (TFTPc_ERR       *) p_err);

                         TFTPc_TxPktRetry++;
                     }
                 }
                 break;


            case TFTPc_ERR_RX:
            default:
                 break;
        }

        if (*p_err != TFTPc_ERR_NONE) {
             TFTPc_TRACE_INFO(("TFTPc_Processing: Error, session terminated\n\r"));
             TFTPc_State = TFTPc_STATE_TRANSFER_COMPLETE;
        }
    }

    TFTPc_Terminate();
}


/*
*********************************************************************************************************
*                                        TFTPc_StateDataGet()
*
* Description : Process received packets for a read request.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               TFTPc_ERR_NONE                  No error.
*                               TFTPc_ERR_ERR_PKT_RX            Error packet   received.
*                               TFTPc_ERR_INVALID_OPCODE_RX     Invalid opcode received.
*
*                                                               ------- RETURNED BY TFTPc_DataWr() : -------
*                               TFTPc_ERR_FILE_WR               Error writing to file.
*
*                                                               ------- RETURNED BY TFTPc_TxAck() : -------
*                               TFTPc_ERR_TX                    Error transmitting packet.
* Return(s)   : none.
*
* Caller(s)   : TFTPc_Processing().
*
* Note(s)     : (1) If the data block received is not the expected one, nothing is written in the file,
*                   and the function silently returns.
*********************************************************************************************************
*/

static  void  TFTPc_StateDataGet (TFTPc_ERR  *p_err)
{
    CPU_INT16U  rx_blk_nbr;
    CPU_INT16U  wr_data_len;
    TFTPc_ERR   err;


    switch (TFTPc_RxPktOpcode) {
        case TFTP_OPCODE_DATA:
             TFTPc_TRACE_INFO(("TFTPc_StateDataGet: Opcode DATA rx'd\n\r"));
            *p_err = TFTPc_ERR_NONE;
             break;


        case TFTP_OPCODE_ERR:
             TFTPc_TRACE_INFO(("TFTPc_StateDataGet: Opcode ERROR rx'd\n\r"));
            *p_err = TFTPc_ERR_ERR_PKT_RX;
             break;


        case TFTP_OPCODE_ACK:
        case TFTP_OPCODE_WRQ:
        case TFTP_OPCODE_RRQ:
        default:
             TFTPc_TRACE_INFO(("TFTPc_StateDataGet: Invalid opcode rx'd\n\r"));
             TFTPc_TxErr((CPU_INT16U ) TFTP_ERR_CODE_ILLEGAL_OP,
                         (CPU_CHAR  *) 0,
                         (TFTPc_ERR *)&err);
            *p_err = TFTPc_ERR_INVALID_OPCODE_RX;
             break;
    }

    if (*p_err != TFTPc_ERR_NONE) {
         return;
    }


    rx_blk_nbr = TFTPc_GetRxBlkNbr();                           /* Get rx'd pkt's blk nbr.                              */

    if (rx_blk_nbr == TFTPc_RxBlkNbrNext) {                     /* If data blk nbr expected, (see Note #1) ...          */
        wr_data_len = TFTPc_DataWr(p_err);                      /* ... wr data to file                ...               */

        if (*p_err == TFTPc_ERR_NONE) {
            TFTPc_TxAck(rx_blk_nbr, &err);                      /* ... and tx ack.                                      */
            TFTPc_TxPktRetry = 0;

            if (wr_data_len < TFTPc_DATA_BLOCK_SIZE) {          /* If rx'd data len < TFTP blk size, ...                */
                TFTPc_State = TFTPc_STATE_TRANSFER_COMPLETE;    /* ... last blk rx'd.                                   */

            } else {
                TFTPc_RxBlkNbrNext++;
            }

        } else {                                                /* Err wr'ing data to file.                             */
            TFTPc_TxErr((CPU_INT16U ) TFTP_ERR_CODE_NOT_DEF,
                        (CPU_CHAR  *) TFTPc_ERR_MSG_WR_ERR,
                        (TFTPc_ERR *)&err);
        }
    }
}


/*
*********************************************************************************************************
*                                        TFTPc_StateDataPut()
*
* Description : Process received packet for a write request.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               TFTPc_ERR_NONE                  No error.
*                               TFTPc_ERR_ERR_PKT_RX            Error packet   received.
*                               TFTPc_ERR_INVALID_OPCODE_RX     Invalid opcode received.
*                               TFTPc_ERR_INVALID_STATE         Invalid state machine state.
*
*                                                               ------- RETURNED BY TFTPc_DataRd() : -------
*                               TFTPc_ERR_FILE_RD               Error reading file.
*
*                                                               ------- RETURNED BY TFTPc_TxData() : -------
*                               TFTPc_ERR_TX                    Error transmitting packet.
*
* Return(s)   : none.
*
* Caller(s)   : TFTPc_Processing().
*
* Note(s)     : (1) If the acknowledge block received is not the expected one, nothing is done, and the
*                   function silently returns.  This is done in order to prevent the 'Sorcerer's Apprentice'
*                   bug.  Only a receive timeout is supposed to trigger a block retransmission.
*********************************************************************************************************
*/

static  void  TFTPc_StateDataPut (TFTPc_ERR  *p_err)
{
    CPU_INT16U  rx_blk_nbr;
    CPU_INT16U  rd_data_len;
    TFTPc_ERR   err;


    switch (TFTPc_RxPktOpcode) {
        case TFTP_OPCODE_ACK:
             TFTPc_TRACE_INFO(("TFTPc_StateDataGet: Opcode ACK rx'd\n\r"));
            *p_err = TFTPc_ERR_NONE;
             break;


        case TFTP_OPCODE_ERR:
             TFTPc_TRACE_INFO(("TFTPc_StateDataPut: Opcode ERROR rx'd\n\r"));
            *p_err = TFTPc_ERR_ERR_PKT_RX;
             break;


        case TFTP_OPCODE_DATA:
        case TFTP_OPCODE_WRQ:
        case TFTP_OPCODE_RRQ:
        default:
             TFTPc_TRACE_INFO(("TFTPc_StateDataPut: Invalid opcode rx'd\n\r"));
             TFTPc_TxErr((CPU_INT16U ) TFTP_ERR_CODE_ILLEGAL_OP,
                         (CPU_CHAR  *) 0,
                         (TFTPc_ERR *)&err);
            *p_err = TFTPc_ERR_INVALID_OPCODE_RX;
             break;
    }

    if (*p_err != TFTPc_ERR_NONE) {
         return;
    }



    rx_blk_nbr = TFTPc_GetRxBlkNbr();                           /* Get rx'd pkt's blk nbr.                              */

    if (rx_blk_nbr == TFTPc_TxPktBlkNbr) {                      /* If ACK blk nbr matches data sent (see Note #1) ...   */

        switch (TFTPc_State) {
            case TFTPc_STATE_DATA_PUT:                          /* ... and more data to read, ...                       */
                 rd_data_len = TFTPc_DataRd(p_err);             /* ... rd next blk from file                      ...   */

                 if (*p_err == TFTPc_ERR_NONE) {
                     TFTPc_TxPktBlkNbr++;                       /* ... and tx data.                                     */
                     TFTPc_TxData(TFTPc_TxPktBlkNbr, rd_data_len, p_err);
                     TFTPc_TxPktRetry = 0;

                     if (rd_data_len < TFTPc_DATA_BLOCK_SIZE) {
                         TFTPc_State = TFTPc_STATE_DATA_PUT_WAIT_LAST_ACK;
                     }

                 } else {                                       /* Err rd'ing data to file.                             */
                     TFTPc_TxErr((CPU_INT16U ) TFTP_ERR_CODE_NOT_DEF,
                                 (CPU_CHAR  *) TFTPc_ERR_MSG_RD_ERR,
                                 (TFTPc_ERR *)&err);
                 }
                 break;


            case TFTPc_STATE_DATA_PUT_WAIT_LAST_ACK:            /* If waiting for last ack, ...                         */
                 TFTPc_State = TFTPc_STATE_TRANSFER_COMPLETE;   /* ... transfer completed.                              */
                 break;


            default:
                *p_err = TFTPc_ERR_INVALID_STATE;
                 break;
        }
    }
}


/*
*********************************************************************************************************
*                                         TFTPc_GetRxBlkNbr()
*
* Description : Extract the block number from the received TFTP packet.
*
* Argument(s) : none.
*
* Return(s)   : Received block number.
*
* Caller(s)   : TFTPc_StateDataPut().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT16U  TFTPc_GetRxBlkNbr (void)
{
    CPU_INT16U  blk_nbr;


    blk_nbr = NET_UTIL_VAL_GET_NET_16(&TFTPc_RxPktBuf[TFTP_PKT_OFFSET_BLK_NBR]);

    return (blk_nbr);
}


/*
*********************************************************************************************************
*                                        TFTPc_FileOpenMode()
*
* Description : Open the specified file for read or write.
*
* Argument(s) : filename        Name of the file to open.
*
*               file_access     File access :
*
*                                   TFTPc_FILE_OPEN_RD      Open for reading.
*                                   TFTPc_FILE_OPEN_WR      Open for writing.
*
* Return(s)   : Pointer to a file handle for the opened file, if NO error.
*
*               Pointer to NULL,                              otherwise.
*
* Caller(s)   : TFTPc_Get(),
*               TFTPc_Put().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  *TFTPc_FileOpenMode (CPU_CHAR           *p_filename,
                                   TFTPc_FILE_ACCESS   file_access)
{
    void  *pfile;


    pfile = (void *)0;
    switch (file_access) {
        case TFTPc_FILE_OPEN_RD:
             pfile = NetFS_FileOpen(p_filename,
                                    NET_FS_FILE_MODE_OPEN,
                                    NET_FS_FILE_ACCESS_RD);
             break;


        case TFTPc_FILE_OPEN_WR:
             pfile = NetFS_FileOpen(p_filename,
                                    NET_FS_FILE_MODE_CREATE,
                                    NET_FS_FILE_ACCESS_WR);
             break;


        default:
             break;
    }

    return (pfile);
}


/*
*********************************************************************************************************
*                                           TFTPc_DataWr()
*
* Description : Write data to the file system.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               TFTPc_ERR_NONE      No error.
*                               TFTPc_ERR_FILE_WR   Error writing to file.
*
* Return(s)   : Number of octets written to file.
*
* Caller(s)   : TFTPc_StateDataGet().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT16U  TFTPc_DataWr (TFTPc_ERR  *p_err)
{
    CPU_SIZE_T  rx_data_len;
    CPU_SIZE_T  wr_data_len;


    rx_data_len = TFTPc_RxPktLen - TFTP_PKT_SIZE_OPCODE - TFTP_PKT_SIZE_BLK_NBR;
    wr_data_len = 0;

    if (rx_data_len > 0) {
       (void)NetFS_FileWr((void       *) TFTPc_FileHandle,
                          (void       *)&TFTPc_RxPktBuf[TFTP_PKT_OFFSET_DATA],
                          (CPU_SIZE_T  ) rx_data_len,
                          (CPU_SIZE_T *)&wr_data_len);
    }

    if (wr_data_len != rx_data_len) {
       *p_err = TFTPc_ERR_FILE_WR;
    } else {
       *p_err = TFTPc_ERR_NONE;
    }

    return ((CPU_INT16U)wr_data_len);
}


/*
*********************************************************************************************************
*                                           TFTPc_DataRd()
*
* Description : Read data from the file system.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               TFTPc_ERR_NONE      No error.
*                               TFTPc_ERR_FILE_RD   Error reading file.
*
* Return(s)   : Number of octets read from file.
*
* Caller(s)   : TFTPc_StateDataPut().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT16U  TFTPc_DataRd (TFTPc_ERR  *p_err)
{
    CPU_SIZE_T   rd_data_len;
    CPU_BOOLEAN  err;


   *p_err = TFTPc_ERR_NONE;
                                                                /* Rd data from file.                                   */
    err  = NetFS_FileRd((void       *) TFTPc_FileHandle,
                        (void       *)&TFTPc_TxPktBuf[TFTP_PKT_OFFSET_DATA],
                        (CPU_SIZE_T  ) TFTPc_DATA_BLOCK_SIZE,
                        (CPU_SIZE_T *)&rd_data_len);

    if (rd_data_len == 0) {                                     /* If NO data rd                   ...                  */
        if (err == DEF_FAIL) {                                  /* ... and err occurred (NOT EOF), ...                  */
           *p_err = TFTPc_ERR_FILE_RD;                          /* ... rtn err.                                         */
        }
    }

    return ((CPU_INT16U)rd_data_len);
}


/*
*********************************************************************************************************
*                                            TFTPc_RxPkt()
*
* Description : Receive TFTP packet.
*
* Argument(s) : sock_id         Socket descriptor/handle identifier of socket to receive data.
*
*               p_pkt           Pointer to packet to receive.
*
*               pkt_len         Length of  packet to receive (in octets).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               TFTPc_ERR_NONE          Packet successfully transmitted.
*                               TFTPc_ERR_RX_TIMEOUT    Receive timeout.
*                               TFTPc_ERR_RX            Error receiving packet.
*
* Return(s)   : Number of positive data octets received, if NO errors.
*
*               NET_SOCK_BSD_RTN_CODE_CONN_CLOSED,       if socket connection closed.
*
*               NET_SOCK_BSD_ERR_RX,                     otherwise.
*
* Caller(s)   : TFTPc_Processing().
*
* Note(s)     : (1) #### When returning from the receive function, we should make sure the received
*                   packet comes from the host we are connected to.
*
*               (2) #### Transitory errors (NET_ERR_RX) should probably trigger another attempt to
*                   transmit the packet, instead of returning an error right away.
*********************************************************************************************************
*/

static  NET_SOCK_RTN_CODE  TFTPc_RxPkt (NET_SOCK_ID         sock_id,
                                        void               *p_pkt,
                                        CPU_INT16U          pkt_len,
                                        TFTPc_ERR          *p_err)
{
#ifdef  NET_IPv4_MODULE_EN
    NET_SOCK_ADDR_IPv4  *p_addrv4;
    NET_SOCK_ADDR_IPv4  *p_serverv4;
#endif
#ifdef  NET_IPv6_MODULE_EN
    NET_SOCK_ADDR_IPv6  *p_addrv6;
    NET_SOCK_ADDR_IPv6  *p_serverv6;
#endif
    NET_SOCK_RTN_CODE  rtn_code;
    NET_SOCK_ADDR      server_sock_addr_ip;
    NET_SOCK_ADDR_LEN  server_sock_addr_ip_len;
    NET_ERR            err;

                                                                /* --------------- RX PKT THROUGH SOCK ---------------- */
                                                                /* See Note #1.                                         */
    server_sock_addr_ip_len = sizeof(server_sock_addr_ip);
    rtn_code                = NetSock_RxDataFrom((NET_SOCK_ID        ) sock_id,
                                                 (void              *) p_pkt,
                                                 (CPU_INT16U         ) pkt_len,
                                                 (CPU_INT16S         ) NET_SOCK_FLAG_NONE,
                                                 (NET_SOCK_ADDR     *)&server_sock_addr_ip,
                                                 (NET_SOCK_ADDR_LEN *)&server_sock_addr_ip_len,
                                                 (void              *) 0,
                                                 (CPU_INT08U         ) 0,
                                                 (CPU_INT08U        *) 0,
                                                 (NET_ERR           *)&err);
    switch (err) {
        case NET_SOCK_ERR_NONE:
            *p_err = TFTPc_ERR_NONE;
             if (TFTPc_TID_Set != DEF_YES) {                    /* If terminal ID NOT set, ...                          */
                                                                /* ... change server port to last rx'd one.             */
                 switch(TFTPc_SockAddr.AddrFamily) {
#ifdef  NET_IPv4_MODULE_EN
                     case NET_SOCK_ADDR_FAMILY_IP_V4:
                          p_addrv4       = (NET_SOCK_ADDR_IPv4 *)&TFTPc_SockAddr;
                          p_serverv4     = (NET_SOCK_ADDR_IPv4 *)&server_sock_addr_ip;
                          p_addrv4->Port =  p_serverv4->Port;
                          break;
#endif
#ifdef  NET_IPv6_MODULE_EN
                     case NET_SOCK_ADDR_FAMILY_IP_V6:
                          p_addrv6       = (NET_SOCK_ADDR_IPv6 *)&TFTPc_SockAddr;
                          p_serverv6     = (NET_SOCK_ADDR_IPv6 *)&server_sock_addr_ip;
                          p_addrv6->Port =  p_serverv6->Port;
                          break;
#endif

                     default:
                          return (TFTPc_ERR_INVALID_PROTO_FAMILY);
                 }

                 TFTPc_TID_Set       = DEF_YES;
             }
             break;

        case NET_SOCK_ERR_RX_Q_EMPTY:
            *p_err = TFTPc_ERR_RX_TIMEOUT;
             break;

        case NET_ERR_RX:                                        /* See Note #2.                                         */
        default:
            *p_err = TFTPc_ERR_RX;
             break;
    }

    return (rtn_code);
}


/*
*********************************************************************************************************
*                                            TFTPc_TxReq()
*
* Description : Transmit TFTP request packet.
*
* Argument(s) : req_opcode  Opcode for this request :
*
*                               TFTP_OPCODE_RRQ         Read  request.
*                               TFTP_OPCODE_WRQ         Write request.
*
*               p_filename  Name of the file to transfer.
*
*               mode        TFTP mode :
*
*                               TFTPc_MODE_NETASCII     ASCII  mode.
*                               TFTPc_MODE_OCTET        Binary mode.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               TFTPc_ERR_NONE              Request packet successfully transmitted.
*                               TFTPc_ERR_NULL_PTR          Argument 'filename'   passed a  NULL pointer.
*                               TFTPc_ERR_INVALID_OPCODE    Argument 'req_opcode' passed an invalid opcode.
*                               TFTPc_ERR_INVALID_MODE      Argument 'mode'       passed an invalid mode.
*
*                                                           --------- RETURNED BY TFTPc_TxPkt() : ----------
*                               TFTPc_ERR_TX                Error transmitting packet.
*
* Return(s)   : none.
*
* Caller(s)   : TFTPc_Get(),
*               TFTPc_Put().
*
* Note(s)     : (1) RFC #1350, section 1 'Purpose' states that "the mail mode is obsolete and should not
*                   be implemented or used".
*********************************************************************************************************
*/

static  void  TFTPc_TxReq (CPU_INT16U   req_opcode,
                           CPU_CHAR    *p_filename,
                           TFTPc_MODE   mode,
                           TFTPc_ERR   *p_err)
{
    CPU_CHAR           *pmode_str;
    CPU_INT16U          filename_len;
    CPU_INT16U          mode_len;
    CPU_INT16U          wr_pkt_ix;
    NET_SOCK_ADDR_LEN   sock_addr_size;


                                                                /* ------------------ VALIDATE ARGS ------------------- */
    if (p_filename == (CPU_CHAR *)0) {                          /* If filename is NULL, ...                             */
       *p_err = TFTPc_ERR_NULL_PTR;                             /* ... rtn err.                                         */
        return;
    }

    if ((req_opcode != TFTP_OPCODE_RRQ) &&                      /* If opcode not RRQ or WRQ, ...                        */
        (req_opcode != TFTP_OPCODE_WRQ)) {
        *p_err = TFTPc_ERR_INVALID_OPCODE;                      /* ... rtn err.                                         */
         return;
    }

    switch (mode) {
        case TFTPc_MODE_NETASCII:
             pmode_str = TFTP_MODE_NETASCII_STR;
             break;


        case TFTPc_MODE_OCTET:
             pmode_str = TFTP_MODE_BINARY_STR;
             break;


        case TFTPc_MODE_MAIL:                                   /* See Note #1.                                         */
        default:
            *p_err = TFTPc_ERR_INVALID_MODE;
             return;
    }



                                                                /* -------------------- CREATE PKT -------------------- */
                                                                /* Wr opcode.                                           */
    NET_UTIL_VAL_SET_NET_16(&TFTPc_TxPktBuf[TFTP_PKT_OFFSET_OPCODE],
                             req_opcode);

                                                                /* Copy filename.                                       */
    Str_Copy((CPU_CHAR *)&TFTPc_TxPktBuf[TFTP_PKT_OFFSET_FILENAME],
             (CPU_CHAR *) p_filename);

                                                                /* Wr mode.                                             */
    filename_len = Str_Len(p_filename);
    wr_pkt_ix    = TFTP_PKT_OFFSET_FILENAME +
                   filename_len             +
                   TFTP_PKT_SIZE_NULL;

    Str_Copy((CPU_CHAR *)&TFTPc_TxPktBuf[wr_pkt_ix],
             (CPU_CHAR *) pmode_str);

    mode_len       = Str_Len(pmode_str);

                                                                /* Get total pkt size.                                  */
    TFTPc_TxPktLen = wr_pkt_ix +
                     mode_len  +
                     TFTP_PKT_SIZE_NULL;


                                                                 /* --------------------- TX PKT ---------------------- */
    sock_addr_size = sizeof(NET_SOCK_ADDR);
   (void)TFTPc_TxPkt((NET_SOCK_ID      ) TFTPc_SockID,
                     (void            *)&TFTPc_TxPktBuf[0],
                     (CPU_INT16U       ) TFTPc_TxPktLen,
                     (NET_SOCK_ADDR   *)&TFTPc_SockAddr,
                     (NET_SOCK_ADDR_LEN) sock_addr_size,
                     (TFTPc_ERR       *) p_err);
}


/*
*********************************************************************************************************
*                                           TFTPc_TxData()
*
* Description : Transmit TFTP data packet.
*
* Argument(s) : blk_nbr     Block number for data packet.
*
*               data_len    Length of data portion of packet (in octets).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               TFTPc_ERR_NONE      Data packet successfully transmitted.
*
*                                                   ------------- RETURNED BY TFTPc_TxPkt() : --------------
*                               TFTPc_ERR_TX        Error transmitting packet.
*
* Return(s)   : none.
*
* Caller(s)   : TFTPc_DataRd().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  TFTPc_TxData (TFTPc_BLK_NBR   blk_nbr,
                            CPU_INT16U      data_len,
                            TFTPc_ERR      *p_err)
{
    NET_SOCK_ADDR_LEN  sock_addr_size;


                                                                /* -------------------- CREATE PKT -------------------- */
                                                                /* Wr opcode.                                           */
    NET_UTIL_VAL_SET_NET_16(&TFTPc_TxPktBuf[TFTP_PKT_OFFSET_OPCODE],
                             TFTP_OPCODE_DATA);

                                                                /* Wr blk nbr.                                          */
    NET_UTIL_VAL_SET_NET_16(&TFTPc_TxPktBuf[TFTP_PKT_OFFSET_BLK_NBR],
                             blk_nbr);

                                                                /* Get total pkt size.                                  */
    TFTPc_TxPktLen = TFTP_PKT_SIZE_OPCODE  +
                     TFTP_PKT_SIZE_BLK_NBR +
                     data_len;

                                                                 /* --------------------- TX PKT ---------------------- */
    sock_addr_size = sizeof(NET_SOCK_ADDR);
   (void)TFTPc_TxPkt((NET_SOCK_ID      ) TFTPc_SockID,
                     (void            *)&TFTPc_TxPktBuf[0],
                     (CPU_INT16U       ) TFTPc_TxPktLen,
                     (NET_SOCK_ADDR   *)&TFTPc_SockAddr,
                     (NET_SOCK_ADDR_LEN) sock_addr_size,
                     (TFTPc_ERR       *) p_err);
}


/*
*********************************************************************************************************
*                                            TFTPc_TxAck()
*
* Description : Transmit TFTP acknowledge packet.
*
* Argument(s) : blk_nbr     Block number to acknowledge.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               TFTPc_ERR_NONE      Acknowledge packet successfully transmitted.
*
*                                                   ------------- RETURNED BY TFTPc_TxPkt() : --------------
*                               TFTPc_ERR_TX        Error transmitting packet.
*
* Return(s)   : none.
*
* Caller(s)   : TFTPc_DataWr().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  TFTPc_TxAck (TFTPc_BLK_NBR   blk_nbr,
                           TFTPc_ERR      *p_err)
{
    NET_SOCK_ADDR_LEN  sock_addr_size;


    NET_UTIL_VAL_SET_NET_16(&TFTPc_TxPktBuf[TFTP_PKT_OFFSET_OPCODE],
                             TFTP_OPCODE_ACK);

    NET_UTIL_VAL_SET_NET_16(&TFTPc_TxPktBuf[TFTP_PKT_OFFSET_BLK_NBR],
                             blk_nbr);

    TFTPc_TxPktLen = TFTP_PKT_SIZE_OPCODE + TFTP_PKT_SIZE_BLK_NBR;


                                                                 /* --------------------- TX PKT ---------------------- */
    sock_addr_size = sizeof(NET_SOCK_ADDR);
   (void)TFTPc_TxPkt((NET_SOCK_ID      ) TFTPc_SockID,
                     (void            *)&TFTPc_TxPktBuf[0],
                     (CPU_INT16U       ) TFTPc_TxPktLen,
                     (NET_SOCK_ADDR   *)&TFTPc_SockAddr,
                     (NET_SOCK_ADDR_LEN) sock_addr_size,
                     (TFTPc_ERR       *) p_err);
}


/*
*********************************************************************************************************
*                                            TFTPc_TxErr()
*
* Description : Transmit TFTP error packet.
*
* Argument(s) : err_code    Code indicating the nature of the error.
*
*               p_err_msg   String associated with error (terminated by NULL character).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               TFTPc_ERR_NONE      Error packet successfully transmitted.
*
*                                                   ------------- RETURNED BY TFTPc_TxPkt() : --------------
*                               TFTPc_ERR_TX        Error transmitting packet.
*
* Return(s)   : none.
*
* Caller(s)   : TFTPc_StateDataGet(),
*               TFTPc_StateDataPut().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  TFTPc_TxErr (CPU_INT16U   err_code,
                           CPU_CHAR    *p_err_msg,
                           TFTPc_ERR   *p_err)
{
    CPU_INT16U         err_msg_len;
    NET_SOCK_ADDR_LEN  sock_addr_size;


    NET_UTIL_VAL_SET_NET_16(&TFTPc_TxPktBuf[TFTP_PKT_OFFSET_OPCODE],
                             TFTP_OPCODE_ERR);

    NET_UTIL_VAL_SET_NET_16(&TFTPc_TxPktBuf[TFTP_PKT_OFFSET_ERR_CODE],
                             err_code);

                                                                /* Copy err msg into tx pkt.                            */
    if (p_err_msg != (CPU_CHAR *)0) {
        Str_Copy((CPU_CHAR *)&TFTPc_TxPktBuf[TFTP_PKT_OFFSET_ERR_MSG],
                 (CPU_CHAR *) p_err_msg);

        err_msg_len = Str_Len(p_err_msg);

    } else {
        TFTPc_TxPktBuf[TFTP_PKT_OFFSET_ERR_MSG] = (CPU_CHAR)0;
        err_msg_len = 0;
    }

    TFTPc_TxPktLen = TFTP_PKT_SIZE_OPCODE   +
                     TFTP_PKT_SIZE_ERR_CODE +
                     err_msg_len            +
                     TFTP_PKT_SIZE_NULL;

                                                                 /* --------------------- TX PKT ---------------------- */
    sock_addr_size = sizeof(NET_SOCK_ADDR);
   (void)TFTPc_TxPkt((NET_SOCK_ID      ) TFTPc_SockID,
                     (void            *)&TFTPc_TxPktBuf[0],
                     (CPU_INT16U       ) TFTPc_TxPktLen,
                     (NET_SOCK_ADDR   *)&TFTPc_SockAddr,
                     (NET_SOCK_ADDR_LEN) sock_addr_size,
                     (TFTPc_ERR       *) p_err);
}


/*
*********************************************************************************************************
*                                            TFTPc_TxPkt()
*
* Description : Transmit TFTP packet.
*
* Argument(s) : sock_id         Socket descriptor/handle identifier of socket to transmit data.
*
*               p_pkt           Pointer to packet to transmit.
*
*               pkt_len         Length of  packet to transmit (in octets).
*
*               p_addr_remote   Pointer to destination address buffer.
*
*               addr_len        Length of  destination address buffer (in octets).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               TFTPc_ERR_NONE      Packet successfully transmitted.
*                               TFTPc_ERR_TX        Error transmitting packet.
*
* Return(s)   : Number of positive data octets transmitted, if NO error.
*
*               NET_SOCK_BSD_RTN_CODE_CONN_CLOSED,          if socket connection closed.
*
*               NET_SOCK_BSD_ERR_TX,                        otherwise .
*
* Caller(s)   : TFTPc_TxReq(),
*               TFTPc_TxData(),
*               TFTPc_TxAck(),
*               TFTPc_TxErr().
*
* Note(s)     : (1) #### Transitory errors (NET_ERR_TX) should probably trigger another attempt to
*                   transmit the packet, instead of returning an error right away.
*********************************************************************************************************
*/

static  NET_SOCK_RTN_CODE  TFTPc_TxPkt (NET_SOCK_ID         sock_id,
                                        void               *p_pkt,
                                        CPU_INT16U          pkt_len,
                                        NET_SOCK_ADDR      *p_addr_remote,
                                        NET_SOCK_ADDR_LEN   addr_len,
                                        TFTPc_ERR          *p_err)
{
    NET_SOCK_RTN_CODE  rtn_code;
    NET_ERR            err;

                                                                /* --------------- TX PKT THROUGH SOCK ---------------- */
    rtn_code = NetSock_TxDataTo((NET_SOCK_ID      ) sock_id,
                                (void            *) p_pkt,
                                (CPU_INT16U       ) pkt_len,
                                (CPU_INT16S       ) NET_SOCK_FLAG_NONE,
                                (NET_SOCK_ADDR   *) p_addr_remote,
                                (NET_SOCK_ADDR_LEN) addr_len,
                                (NET_ERR         *)&err);
    switch (err) {
        case NET_SOCK_ERR_NONE:
            *p_err = TFTPc_ERR_NONE;
             break;

        case NET_ERR_TX:                                        /* See Note #1.                                         */
        default:
            *p_err = TFTPc_ERR_TX;
             break;
    }

    return (rtn_code);
}


/*
*********************************************************************************************************
*                                          TFTPc_Terminate()
*
* Description : (1) Terminate the current file transfer process.
*
*                   (a) Set TFTP client state to 'COMPLETE'
*                   (b) Close opened file.
*
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : TFTPc_Get(),
*               TFTPc_Put(),
*               TFTPc_Processing().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  TFTPc_Terminate (void)
{
    NET_ERR  err;


    if (TFTPc_SockID != NET_SOCK_ID_NONE) {                     /* Close sock.                                          */
        NetSock_Close(TFTPc_SockID, &err);
        TFTPc_SockID = NET_SOCK_ID_NONE;
    }

    if (TFTPc_FileHandle != (void *)0) {                        /* Close file.                                          */
        NetFS_FileClose(TFTPc_FileHandle);
        TFTPc_FileHandle  = (void *)0;
    }
}
