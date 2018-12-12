/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Servo over EtherCAT (SoE) Module.
 */

#include <stdio.h>
#include <string.h>
#include "osal.h"
#include "oshw.h"
#include "ethercattype.h"
#include "ethercatbase.h"
#include "ethercatmain.h"
#include "ethercatvoescope.h"

/** VoE Scope mailbox structure */
PACKED_BEGIN
typedef struct PACKED
{
   ec_mbxheadert    MbxHeader;
   uint8_t          voe_scopetype;
   uint8_t          channels;
   uint8_t          datatype[VOE_SCOPEMAXCHANNELS];
   uint16_t         lines;
   uint64_t         timestamp;
   uint8_t          data[];
} ec_VoEscopet;
PACKED_END

OSAL_THREAD_HANDLE ecx_scopethreadh;

int ecx_scopeinit(ecx_contextt *context, uint8 group)
{
    if(!context->grouplist[group].scopevar)
    {
        ec_scopet *scopevar;
        if((scopevar = osal_malloc(sizeof(ec_scopet))))
        {
            context->grouplist[group].scopevar = scopevar;
            scopevar->scopeslavecnt = 0;
        }
        return 1;
    }
    return 0;
}

int ecx_scopeclose(ecx_contextt *context, uint8 group)
{
    if(context->grouplist[group].scopevar)
    {
        osal_free(context->grouplist[group].scopevar);
        context->grouplist[group].scopevar = NULL;
        return 1;
    }
    return 0;
}

int ecx_scopeenableslave(ecx_contextt *context, uint16 slave)
{
    ec_slavet *slaveitem = &context->slavelist[slave];
    uint8 group = slaveitem->group;
    ec_scopet *scopevar;
    scopevar = context->grouplist[group].scopevar;
    if(slaveitem->coembxin)
    {
        if(scopevar->scopeslavecnt < EC_MAXSCOPESLAVE)
            scopevar->scopeslavecnt++;
        scopevar->scopeslave[scopevar->scopeslavecnt - 1] = slave;
        slaveitem->voembxinfull = FALSE;
        slaveitem->voembxoverrun = 0;
      //  slaveitem->voembxin = (uint8 *)scopevar->mbxin[scopevar->scopeslavecnt -1];
        slaveitem->mbxhandlerstate = ECT_MBXH_CYCLIC;
        return 1;
    }
    return 0;   
}

int ecx_scopedisableslave(ecx_contextt *context, uint16 slave)
{
    ec_slavet *slaveitem = &context->slavelist[slave];
    if(slaveitem->voembxin)
    {
        slaveitem->voembxin = NULL;
        return 1;
    }
    return 0;
}

int ecx_scopembxhandler(ecx_contextt *context)
{
    ec_slavet *slaveitem = &context->slavelist[0];
    if(slaveitem->voembxin)
    {
        slaveitem->voembxin = NULL;
        return 1;
    }
    return 0;
}
/*
OSAL_THREAD_FUNC ecx_scope_thread(void *param)
{
   ecx_mapt_t *maptp;
   maptp = param;
}

int ecx_scopeinit(void)
{
    osal_thread_create_rt(&ecx_scopethreadh, 128000, &ecx_scope_thread, &(ecx_mapt[thrn]));
    return 1;
}
*/
