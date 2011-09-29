/* Copyright (c) 2011 Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __LOG_UTIL_H__
#define __LOG_UTIL_H__

#ifdef __cplusplus
extern "C"
{
#endif
/*=============================================================================
 *
 *                         LOC LOGGER TYPE DECLARATION
 *
 *============================================================================*/
/* LOC LOGGER */
typedef struct loc_logger_s
{
  unsigned long  DEBUG_LEVEL;
  unsigned long  TIMESTAMP;
} loc_logger_s_type;

/*=============================================================================
 *
 *                               EXTERNAL DATA
 *
 *============================================================================*/
extern loc_logger_s_type loc_logger;

/*=============================================================================
 *
 *                        MODULE EXPORTED FUNCTIONS
 *
 *============================================================================*/
extern void loc_logger_init(unsigned long debug, unsigned long timestamp);
extern char* get_timestamp(char* str, unsigned long buf_size);


#include <utils/Log.h>

#ifndef DEBUG_DMN_LOC_API

/* LOGGING MACROS */
#define LOC_LOGE(...) \
if (loc_logger.DEBUG_LEVEL >= 1) { LOGE("E/"__VA_ARGS__); }

#define LOC_LOGW(...) \
if (loc_logger.DEBUG_LEVEL >= 2) { LOGE("W/"__VA_ARGS__); }

#define LOC_LOGI(...) \
if (loc_logger.DEBUG_LEVEL >= 3) { LOGE("I/"__VA_ARGS__); }

#define LOC_LOGD(...) \
if (loc_logger.DEBUG_LEVEL >= 4) { LOGE("D/"__VA_ARGS__); }

#define LOC_LOGV(...) \
if (loc_logger.DEBUG_LEVEL >= 5) { LOGE("V/"__VA_ARGS__); }

#else /* DEBUG_DMN_LOC_API */

#define LOC_LOGE(...) LOGE("E/"__VA_ARGS__)

#define LOC_LOGW(...) LOGW("W/"__VA_ARGS__)

#define LOC_LOGI(...) LOGI("I/"__VA_ARGS__)

#define LOC_LOGD(...) LOGD("D/"__VA_ARGS__)

#define LOC_LOGV(...) LOGV("V/"__VA_ARGS__)

#endif /* DEBUG_DMN_LOC_API */

/*=============================================================================
 *
 *                          LOGGING IMPROVEMENT MACROS
 *
 *============================================================================*/
extern const char *boolStr[];
#define VOID_RET "None"
#define FROM_AFW   "===>"
#define TO_MODEM   "--->"
#define FROM_MODEM "<---"
#define TO_AFW     "<==="

#define ENTRY_LOG()                                                                                                    \
do { if (loc_logger.TIMESTAMP) {                                                                                       \
             char time_stamp[32];                                                                                      \
             LOC_LOGV("[%s] %s called, line %d", get_timestamp(time_stamp, sizeof(time_stamp)), __func__, __LINE__);   \
        } else    {                                                                                                    \
          LOC_LOGV("%s called, line %d", __func__, __LINE__);                                                          \
          }                                                                                                            \
    } while (0)

#define EXIT_LOG(SPECIFIER, RETVAL)                                                                                    \
do { if (loc_logger.TIMESTAMP) {                                                                                       \
             char time_stamp[32];                                                                                      \
             LOC_LOGV("[%s] %s finished, line %d", get_timestamp(time_stamp, sizeof(time_stamp)), __func__, __LINE__); \
        } else {                                                                                                       \
          LOC_LOGV("%s finished, line %d, returned " #SPECIFIER, __func__, __LINE__, RETVAL);                          \
          }                                                                                                            \
    } while (0)

// Used for logging callflow from Android Framework
#define ENTRY_LOG_CALLFLOW()                                                                                                 \
do { if (loc_logger.TIMESTAMP) {                                                                                             \
             char time_stamp[32];                                                                                            \
             LOC_LOGI("[%s] %s %s line %d", get_timestamp(time_stamp, sizeof(time_stamp)), FROM_AFW, __func__, __LINE__);    \
        } else {                                                                                                             \
          LOC_LOGI("%s %s line %d", FROM_AFW, __func__, __LINE__);                                                           \
          }                                                                                                                  \
    } while (0)

// Used for logging callflow to Modem
#define EXIT_LOG_CALLFLOW(SPECIFIER, RETVAL)                                                                                 \
do { if (loc_logger.TIMESTAMP) {                                                                                             \
             char time_stamp[32];                                                                                            \
             LOC_LOGI("[%s] %s %s line %d", get_timestamp(time_stamp, sizeof(time_stamp)), TO_MODEM, __func__, __LINE__);    \
        } else {                                                                                                             \
          LOC_LOGI("%s %s line %d, returned" #SPECIFIER, TO_MODEM, __func__, __LINE__, RETVAL);                              \
          }                                                                                                                  \
    } while (0)

// Used for logging callflow from Modem
#define MODEM_LOG_CALLFLOW()                                                                                                 \
do { if (loc_logger.TIMESTAMP) {                                                                                             \
             char time_stamp[32];                                                                                            \
             LOC_LOGI("[%s] %s %s line %d", get_timestamp(time_stamp, sizeof(time_stamp)), FROM_MODEM, __func__, __LINE__);  \
        } else {                                                                                                             \
          LOC_LOGI("%s %s line %d", FROM_MODEM, __func__, __LINE__);                                                         \
          }                                                                                                                  \
    } while (0)

// Used for logging callflow to Android Framework
#define CALLBACK_LOG_CALLFLOW(CALLBACK_NAME)                                                                                 \
do { if (loc_logger.TIMESTAMP) {                                                                                             \
             char time_stamp[32];                                                                                            \
             LOC_LOGI("[%s] %s %s line %d", get_timestamp(time_stamp, sizeof(time_stamp)), TO_AFW, CALLBACK_NAME, __LINE__); \
        } else {                                                                                                             \
          LOC_LOGI("%s %s line %d", TO_AFW, CALLBACK_NAME, __LINE__);                                                        \
          }                                                                                                                  \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif // __LOG_UTIL_H__
