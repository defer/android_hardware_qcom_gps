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
#define LOG_NDEBUG 0
#define MAX_NUM_ATL_CONNECTIONS  2
// Define boolean type to be used by libgps on loc api module
typedef unsigned char boolean;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#include <loc_eng_xtra.h>
#include <loc_eng_ni.h>
#include <loc_cfg.h>
#include <loc_log.h>
#include <log_util.h>
#include <loc_eng_msg.h>
#include <LocApiAdapter.h>

// The data connection minimal open time
#define DATA_OPEN_MIN_TIME        1  /* sec */

// The system sees GPS engine turns off after inactive for this period of time
#define GPS_AUTO_OFF_TIME         2  /* secs */
#define SUCCESS              TRUE
#define FAILURE                 FALSE
#define INVALID_ATL_CONNECTION_HANDLE -1

#define MAX_APN_LEN 100
#define MAX_URL_LEN 256
#define smaller_of(a, b) (((a) > (b)) ? (b) : (a))

enum loc_mute_session_e_type {
   LOC_MUTE_SESS_NONE,
   LOC_MUTE_SESS_WAIT,
   LOC_MUTE_SESS_IN_SESSION
};
enum loc_eng_atl_session_state_e_type{
   LOC_CONN_IDLE,
   LOC_CONN_OPEN_REQ,
   LOC_CONN_OPEN,
   LOC_CONN_CLOSE_REQ
};
typedef struct
{
   // ATL variables
   loc_eng_atl_session_state_e_type conn_state;
   int                              conn_handle;
   AGpsType                         agps_type;
   boolean                          active;
}loc_eng_atl_info_s_type;

// Module data
typedef struct
{
   LocApiAdapter                 *client_handle;
   gps_location_callback          location_cb;
   gps_status_callback            status_cb;
   gps_sv_status_callback         sv_status_cb;
   agps_status_callback           agps_status_cb;
   gps_nmea_callback              nmea_cb;
   gps_ni_notify_callback         ni_notify_cb;
   gps_acquire_wakelock           acquire_wakelock_cb;
   gps_release_wakelock           release_wakelock_cb;
   AGpsStatusValue                agps_status;
   // used to defer stopping the GPS engine until AGPS data calls are done
   boolean                         agps_request_pending;
   boolean                         stop_request_pending;
   loc_eng_xtra_data_s_type       xtra_module_data;

   boolean                        navigating;
   AGpsBearerType                 data_connection_bearer;

   // ATL variables
   // Adequate instances of ATL variables for cases where we have simultaneous
   // connections to MPC & PDE
   loc_eng_atl_info_s_type       atl_conn_info[MAX_NUM_ATL_CONNECTIONS];
   // GPS engine status
   GpsStatusValue                 engine_status;
   GpsStatusValue                 fix_session_status;

   // Aiding data information to be deleted, aiding data can only be deleted when GPS engine is off
   GpsAidingData                  aiding_data_for_deletion;

   // Data variables used by deferred action thread
   pthread_t                      deferred_action_thread;
   void*                          deferred_q;

   // flags for pending events for deferred action thread
   unsigned int                   deferred_action_flags;
   // For muting session broadcast
   loc_mute_session_e_type        mute_session_state;
    // [0] - supl version
    // [1] - position_mode
} loc_eng_data_s_type;

extern loc_eng_data_s_type loc_eng_data;

extern void loc_eng_mute_one_session();
extern void loc_eng_msg_sender(void* msg);

#endif // LOC_ENG_H
