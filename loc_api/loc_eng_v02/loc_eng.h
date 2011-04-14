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

#ifndef LOC_ENG_H
#define LOC_ENG_H

// Uncomment to keep all LOG messages (LOGD, LOGI, LOGV, etc.)
// #define LOG_NDEBUG 0

#include <stdbool.h>
#include "loc_api_v02_client.h"
#include <loc_eng_xtra.h>
#include <loc_eng_cfg.h>

#ifdef LOC_UTIL_TARGET_OFF_TARGET

#include "gps.h"

#else

#include <hardware/gps.h>

#endif //LOC_UTIL_TARGET_OFF_TARGET

#include <loc_eng_ni.h>

// The system sees GPS engine turns off after inactive for this period of time
#define GPS_AUTO_OFF_TIME         2  /* secs */
//To signify that when requesting a data connection HAL need not specify whether CDMA or UMTS
#define DONT_CARE                 0

typedef enum {
   LOC_MUTE_SESS_NONE,
   LOC_MUTE_SESS_WAIT,
   LOC_MUTE_SESS_IN_SESSION
}loc_mute_session_e_type;

#define DEFERRED_ACTION_EVENT               (0x01)
#define DEFERRED_ACTION_DELETE_AIDING       (0x02)
#define DEFERRED_ACTION_AGPS_STATUS         (0x04)
#define DEFERRED_ACTION_AGPS_DATA_SUCCESS   (0x08)
#define DEFERRED_ACTION_AGPS_DATA_CLOSED    (0x10)
#define DEFERRED_ACTION_AGPS_DATA_FAILED    (0x20)
#define DEFERRED_ACTION_QUIT                (0x40)

typedef struct
{
  GpsPositionMode mode;
  GpsPositionRecurrence recurrence;
  uint32_t min_interval ;
  uint32_t preferred_accuracy ;
  uint32_t preferred_time;
}loc_eng_fix_criteria_s_type;


// Module data
typedef struct
{
   locClientHandleType            client_handle;
   gps_location_callback          location_cb;
   gps_status_callback            status_cb;
   gps_sv_status_callback         sv_status_cb;

   gps_nmea_callback              nmea_cb;
   gps_ni_notify_callback         ni_notify_cb;
   gps_acquire_wakelock           acquire_wakelock_cb;
   gps_release_wakelock           release_wakelock_cb;

   //stored fix criteria
   loc_eng_fix_criteria_s_type    fix_criteria;

   loc_eng_xtra_data_s_type       xtra_module_data;

   pthread_mutex_t                deferred_stop_mutex;
   // data from loc_event_cb
   int32_t                        loc_event_id;
   locClientEventIndUnionType     loc_event_payload;

   bool                           client_opened;

   // set to true when the client calls loc_eng_start and
   // set to false when the client call loc_eng_stop
   // indicates client state before fix_session_status
   // is set through the loc api event
   bool                           navigating;

   // GPS engine status
   GpsStatusValue                 engine_status;
   // GPS
   GpsStatusValue                 fix_session_status;

   // Aiding data information to be deleted, aiding data can only be deleted when GPS engine is off
   GpsAidingData                  aiding_data_for_deletion;

    // Data variables used by deferred action thread
   pthread_t                      deferred_action_thread;

   // Mutex used by deferred action thread
   pthread_mutex_t                deferred_action_mutex;
   // Condition variable used by deferred action thread
   pthread_cond_t                 deferred_action_cond;
   // flags for pending events for deferred action thread
   int                             deferred_action_flags;
   // For muting session broadcast
   pthread_mutex_t                mute_session_lock;
   loc_mute_session_e_type        mute_session_state;

} loc_eng_data_s_type;

extern loc_eng_data_s_type loc_eng_data;

extern void loc_eng_mute_one_session();

#endif // LOC_ENG_H
