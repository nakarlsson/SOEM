/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

/** \file
 * \brief
 * Headerfile for ethercatvoescope.c
 */

#ifndef _ethercatvoescope_
#define _ethercatvoescope_

#ifdef __cplusplus
extern "C"
{
#endif

#define VOE_SCOPETYPE           0x10

#define SCOPE_BUFSIZE (MBXSIZE - sizeof(_MBXh) - VOE_SCOPEMAXCHANNELS - 12)

int ecx_scopeinit(ecx_contextt *context, uint8 group);
int ecx_scopeclose(ecx_contextt *context, uint8 group);
int ecx_scopeenableslave(ecx_contextt *context, uint16 slave);
int ecx_scopedisableslave(ecx_contextt *context, uint16 slave);

#ifdef __cplusplus
}
#endif

#endif
