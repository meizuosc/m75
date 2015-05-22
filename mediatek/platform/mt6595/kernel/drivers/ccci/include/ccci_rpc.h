/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccci_rpc.h
 *
 * Project:
 * --------
 *   YuSu
 *
 * Description:
 * ------------
 *   
 *
 * Author:
 * -------
 *   
 *
 ****************************************************************************/

#ifndef __CCCI_RPC_H__
#define __CCCI_RPC_H__

#include <crypto_engine_export.h>
#include <sec_error.h>

#define CCCI_SED_LEN_BYTES   16 
typedef struct {unsigned char sed[CCCI_SED_LEN_BYTES]; }sed_t;
#define SED_INITIALIZER { {[0 ... CCCI_SED_LEN_BYTES-1]=0}}
/*******************************************************************************
 * Define marco or constant.
 *******************************************************************************/
#define IPC_RPC_EXCEPT_MAX_RETRY     7
#define IPC_RPC_MAX_RETRY            0xFFFF
#define IPC_RPC_REQ_BUFFER_NUM       2 /* support 2 concurrently request*/
#define IPC_RPC_MAX_ARG_NUM          6 /* parameter number */
#define IPC_RPC_MAX_BUF_SIZE         2048 

#define IPC_RPC_USE_DEFAULT_INDEX    -1
#define IPC_RPC_API_RESP_ID          0xFFFF0000
#define IPC_RPC_INC_BUF_INDEX(x)     (x = (x + 1) % IPC_RPC_REQ_BUFFER_NUM)

/*******************************************************************************
 * Define data structure.
 *******************************************************************************/
typedef enum
{
    IPC_RPC_CPSVC_SECURE_ALGO_OP = 0x2001,
}RPC_OP_ID;

typedef struct
{
   unsigned int len;
   void *buf;
}RPC_PKT;

typedef struct
{
    unsigned int     op_id;
    unsigned char    buf[IPC_RPC_MAX_BUF_SIZE];
}RPC_BUF;
#define CCCI_RPC_SMEM_SIZE (sizeof(RPC_BUF) * IPC_RPC_REQ_BUFFER_NUM)

#define FS_NO_ERROR										 0
#define FS_ERROR_RESERVED								-1
#define	FS_PARAM_ERROR									-2
extern int ccci_rpc_init(void);
extern void ccci_rpc_exit(void);


#endif // 