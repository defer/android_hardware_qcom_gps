/* Copyright (c) 2009,2011 Code Aurora Forum. All rights reserved.
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

#define LOG_NDDEBUG 0
#define LOG_TAG "libloc"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>

#include <loc_eng.h>

#include "log_util.h"

// comment this out to enable logging
// #undef LOGD
// #define LOGD(...) {}

/*=============================================================================
 *
 *                             DATA DECLARATION
 *
 *============================================================================*/

const GpsNiInterface sLocEngNiInterface =
{
   sizeof(GpsNiInterface),
   loc_eng_ni_init,
   loc_eng_ni_respond,
};

boolean loc_eng_ni_data_init = FALSE;
loc_eng_ni_data_s_type loc_eng_ni_data;

extern loc_eng_data_s_type loc_eng_data;
/* User response callback waiting conditional variable */
pthread_cond_t             user_cb_arrived_cond = PTHREAD_COND_INITIALIZER;

/* User callback waiting data block, protected by user_cb_data_mutex */
pthread_mutex_t            user_cb_data_mutex   = PTHREAD_MUTEX_INITIALIZER;

/*=============================================================================
 *
 *                             FUNCTION DECLARATIONS
 *
 *============================================================================*/
static void* loc_ni_thread_proc(void *threadid);
/*===========================================================================

FUNCTION respond_from_enum

DESCRIPTION
   Returns the name of the response

RETURN VALUE
   response name string

===========================================================================*/
static const char* respond_from_enum(GpsUserResponseType resp)
{
   switch (resp)
   {
   case GPS_NI_RESPONSE_ACCEPT:
      return "accept";
   case GPS_NI_RESPONSE_DENY:
      return "deny";
   case GPS_NI_RESPONSE_NORESP:
      return "no response";
   default:
      return NULL;
   }
}

/*===========================================================================

FUNCTION loc_ni_respond

DESCRIPTION
   Displays the NI request and awaits user input. If a previous request is
   in session, the new one is handled using sys.ni_default_response (if exists);
   otherwise, it is denied.

DEPENDENCY
   Do not lock the data by mutex loc_ni_lock

RETURN VALUE
   none

===========================================================================*/
static void loc_ni_respond
(
      GpsUserResponseType resp,
      void *request_pass_back
)
{
    LOC_LOGD("Sending NI response: %s\n", respond_from_enum(resp));
    loc_eng_msg_inform_ni_response *msg(new loc_eng_msg_inform_ni_response(resp, request_pass_back));
    loc_eng_msgsnd( loc_eng_data.deferred_q, &msg);
}

/*===========================================================================

FUNCTION loc_ni_request_handler

DESCRIPTION
   Displays the NI request and awaits user input. If a previous request is
   in session, it is ignored.

RETURN VALUE
   none

===========================================================================*/
void loc_ni_request_handler(const GpsNiNotification &notif, const void* passThrough)
{

   /* If busy, use default or deny */
   if (NULL != loc_eng_ni_data.loc_ni_request)
   {
       /* XXX Consider sending a NO RESPONSE reply or queue the request */
       LOC_LOGW("loc_ni_request_handler, notification in progress, new NI request ignored, type: %d",
                notif.ni_type);
       if (NULL != passThrough) {
           free((void*)passThrough);
       }
   }
   else {
      pthread_mutex_lock(&loc_eng_ni_data.loc_ni_lock);

      /* Save request */
      loc_eng_ni_data.loc_ni_request = (void*)passThrough;

      loc_eng_ni_data.current_notif_id = notif.notification_id;

      if (notif.notify_flags == GPS_NI_PRIVACY_OVERRIDE)
      {
          loc_eng_mute_one_session();
      }

      /* Log requestor ID and text for debugging */
      LOC_LOGI("Notification: notif_type: %d, timeout: %d, default_resp: %d", notif.ni_type, notif.timeout, notif.default_response);
      LOC_LOGI("              requestor_id: %s (encoding: %d)", notif.requestor_id, notif.requestor_id_encoding);
      LOC_LOGI("              text: %s text (encoding: %d)", notif.text, notif.text_encoding);
      LOC_LOGI("              notification id %d, notify flags %u", notif.notification_id, notif.notify_flags);
      if (notif.extras[0])
      {
         LOC_LOGI("              extras: %s", notif.extras);
      }

      /* For robustness, spawn a thread at this point to timeout to clear up the notification status, even though
       * the OEM layer in java does not do so.
       **/
      loc_eng_ni_data.response_time_left = 5 + (notif.timeout != 0 ? notif.timeout : LOC_NI_NO_RESPONSE_TIME);
      LOC_LOGI("Automatically sends 'no response' in %d seconds (to clear status)\n", loc_eng_ni_data.response_time_left);

      /* @todo may required when android framework issue is fixed
       * loc_eng_ni_data.callbacks_ref->create_thread_cb("loc_api_ni", loc_ni_thread_proc, NULL);
       */

      int rc = 0;
      rc = pthread_create(&loc_eng_ni_data.loc_ni_thread, NULL, loc_ni_thread_proc, NULL);
      if (rc)
      {
         LOC_LOGE("Loc NI thread is not created.\n");
      }
      rc = pthread_detach(loc_eng_ni_data.loc_ni_thread);
      if (rc)
      {
         LOC_LOGE("Loc NI thread is not detached.\n");
      }
      pthread_mutex_unlock(&loc_eng_ni_data.loc_ni_lock);

      /* Notify callback */
      if (loc_eng_data.ni_notify_cb != NULL)
      {
            CALLBACK_LOG_CALLFLOW("ni_notify_cb");
            loc_eng_data.ni_notify_cb((GpsNiNotification*)&notif);
      }
   }
}

/*===========================================================================

FUNCTION loc_ni_process_user_response

DESCRIPTION
   Handles user input from the UI

RETURN VALUE
   error code (0 for successful, -1 for error)

===========================================================================*/
int loc_ni_process_user_response(GpsUserResponseType userResponse)
{
   int rc=0;
   LOC_LOGD("NI response from UI: %d", userResponse);

   /* Turn of the timeout*/
   pthread_mutex_lock(&user_cb_data_mutex);
   loc_eng_ni_data.resp = userResponse;
   loc_eng_ni_data.user_response_received = TRUE;
   rc = pthread_cond_signal(&user_cb_arrived_cond);
   pthread_mutex_unlock(&user_cb_data_mutex);
   return 0;
}

/*===========================================================================

FUNCTION loc_ni_thread_proc

===========================================================================*/
static void* loc_ni_thread_proc(void *threadid)
{
   int rc = 0;          /* return code from pthread calls */

   struct timeval present_time;
   struct timespec expire_time;

   LOC_LOGD("Starting Loc NI thread...\n");
   pthread_mutex_lock(&user_cb_data_mutex);
   /* Calculate absolute expire time */
   gettimeofday(&present_time, NULL);
   expire_time.tv_sec  = present_time.tv_sec;
   expire_time.tv_nsec = present_time.tv_usec * 1000;
   expire_time.tv_sec += loc_eng_ni_data.response_time_left;
   LOC_LOGD("loc_ni_thread_proc-Time out set for abs time %ld\n", (long) expire_time.tv_sec );

   while (!loc_eng_ni_data.user_response_received)
   {
      rc = pthread_cond_timedwait(&user_cb_arrived_cond, &user_cb_data_mutex, &expire_time);
      if (rc == ETIMEDOUT)
      {
         loc_eng_ni_data.resp = GPS_NI_RESPONSE_NORESP;
         LOC_LOGD("loc_ni_thread_proc-Thread time out after valting for specified time. Ret Val %d\n",rc );
         break;
      }
   }
      if (loc_eng_ni_data.user_response_received == TRUE)
      {
         LOC_LOGD("loc_ni_thread_proc-Java layer has sent us a user response and return value from "
                  "pthread_cond_timedwait = %d\n",rc );
         loc_eng_ni_data.user_response_received = FALSE; /* Reset the user response flag for the next session*/
      }

   // adding this check to support modem restart, in which case, we need the thread
   // to exit without calling loc_ni_respond. We make sure notif_in_progress is false
   // in loc_ni_reset_on_engine_restart()
   if (NULL != loc_eng_ni_data.loc_ni_request) {
       loc_ni_respond(loc_eng_ni_data.resp, loc_eng_ni_data.loc_ni_request);
       loc_eng_ni_data.loc_ni_request = NULL;
   }
   pthread_mutex_unlock(&user_cb_data_mutex);
   pthread_mutex_lock(&loc_eng_ni_data.loc_ni_lock);
   loc_eng_ni_data.response_time_left = 0;
   loc_eng_ni_data.current_notif_id = -1;
   pthread_mutex_unlock(&loc_eng_ni_data.loc_ni_lock);
   return NULL;
}

void loc_ni_reset_on_engine_restart()
{
    // only if modem has requested but then died.
    if (NULL != loc_eng_ni_data.loc_ni_request) {
        pthread_mutex_lock(&loc_eng_ni_data.loc_ni_lock);
        free(loc_eng_ni_data.loc_ni_request);
        loc_eng_ni_data.loc_ni_request = NULL;
        // turn the flag off to make sure we do not IOCTL
        pthread_mutex_unlock(&loc_eng_ni_data.loc_ni_lock);

        pthread_mutex_lock(&user_cb_data_mutex);
        // the goal is to wake up loc_ni_thread_proc
        // and let it exit.
        loc_eng_ni_data.user_response_received = TRUE;
        pthread_cond_signal(&user_cb_arrived_cond);
        pthread_mutex_unlock(&user_cb_data_mutex);
    }
}

/*===========================================================================
FUNCTION    loc_eng_ni_init

DESCRIPTION
   This function initializes the NI interface

DEPENDENCIES
   NONE

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
void loc_eng_ni_init(GpsNiCallbacks *callbacks)
{
   LOC_LOGD("loc_eng_ni_init: entered.");

   if (!loc_eng_ni_data_init)
   {
      pthread_mutex_init(&loc_eng_ni_data.loc_ni_lock, NULL);
      loc_eng_ni_data_init = TRUE;
   }

   loc_eng_ni_data.loc_ni_request = NULL;
   loc_eng_ni_data.current_notif_id = -1;
   loc_eng_ni_data.response_time_left = 0;
   loc_eng_ni_data.user_response_received = FALSE;

   srand(time(NULL));
   loc_eng_data.ni_notify_cb = callbacks->notify_cb;
}

/*===========================================================================
FUNCTION    loc_eng_ni_respond

DESCRIPTION
   This function sends an NI respond to the modem processor

DEPENDENCIES
   NONE

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
void loc_eng_ni_respond(int notif_id, GpsUserResponseType user_response)
{
   if (notif_id == loc_eng_ni_data.current_notif_id &&
       NULL != loc_eng_ni_data.loc_ni_request)
   {
      LOC_LOGI("loc_eng_ni_respond: send user response %d for notif %d", user_response, notif_id);
      loc_ni_process_user_response(user_response);
   }
   else {
      LOC_LOGE("loc_eng_ni_respond: notif_id %d, loc_eng_ni_data.current_notif_id %d, loc_ni_request %p, mismatch or notification not in progress, response: %d",
                notif_id, loc_eng_ni_data.current_notif_id, loc_eng_ni_data.loc_ni_request, user_response);
   }
}
