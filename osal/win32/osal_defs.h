/*
 * Licensed under the GNU General Public License version 2 with exceptions. See
 * LICENSE file in the project root for full license information
 */

#ifndef _osal_defs_
#define _osal_defs_

#ifdef __cplusplus
extern "C"
{
#endif

#include <winsock2.h> /* Must include before windows.h to avoid issues with winsock.h */
#include <windows.h>

#define OS_MBOX

#define OS_WAIT_FOREVER INFINITE

#ifndef PACKED
#define PACKED_BEGIN __pragma(pack(push, 1))
#define PACKED
#define PACKED_END __pragma(pack(pop))
#endif

#define OSAL_THREAD_HANDLE HANDLE
#define OSAL_THREAD_FUNC void
#define OSAL_THREAD_FUNC_RT void


typedef struct os_mbox
{
   CONDITION_VARIABLE condition;
   CRITICAL_SECTION lock;
   size_t r;
   size_t w;
   size_t count;
   size_t size;
   void * msg[];
} os_mbox_t;

#ifdef __cplusplus
}
#endif

#endif
