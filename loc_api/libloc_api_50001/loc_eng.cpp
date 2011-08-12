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
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>         /* struct sockaddr_in */
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <time.h>
#include <dlfcn.h>

#include "LocApiAdapter.h"

#include <cutils/properties.h>
#include <cutils/sched_policy.h>
#include <utils/SystemClock.h>
#include <utils/Log.h>
#include <string.h>

#include <loc_eng.h>
#include <loc_eng_dmn_conn.h>
#include <loc_eng_msg.h>
#include <loc_eng_msg_id.h>

#include "ulp.h"

#include "log_util.h"
#include "loc_eng_log.h"

#define DEBUG_NI_REQUEST_EMU 0

#define TENMSDELAY   (10000)
#define SUCCESS TRUE
#define FAILURE FALSE

typedef void (*voidFuncPtr)(void);

static int loc_eng_reinit();
// Function declarations for sLocEngInterface
static int  loc_eng_init(GpsCallbacks* callbacks);
static int  loc_eng_start();
static int  loc_eng_stop();
static int  loc_eng_set_position_mode(GpsPositionMode mode, GpsPositionRecurrence recurrence,
            uint32_t min_interval, uint32_t preferred_accuracy, uint32_t preferred_time);
static void loc_eng_cleanup();
static int  loc_eng_inject_time(GpsUtcTime time, int64_t timeReference, int uncertainty);
static int loc_eng_inject_location(double latitude, double longitude, float accuracy);
static void loc_eng_delete_aiding_data(GpsAidingData f);
static const void* loc_eng_get_extension(const char* name);

// 2nd half of init(), singled out for
// modem restart to use.
static void loc_eng_agps_reinit();
// Function declarations for sLocEngAGpsInterface
static void loc_eng_agps_init(AGpsCallbacks* callbacks);
static int loc_eng_atl_open(const char* apn, AGpsBearerType bearerType);
static int loc_eng_atl_closed();
static int loc_eng_atl_open_failed();
static int loc_eng_set_server(AGpsType type, const char *hostname, int port);
static int loc_eng_set_server_proxy(AGpsType type, const char *hostname, int port);

// Internal functions
static int loc_eng_deinit();
static void loc_inform_gps_status(GpsStatusValue status);
static void loc_eng_report_status(GpsStatusValue status);
static void loc_eng_process_conn_request(int connHandle, AGpsType agps_type);

static void loc_eng_deferred_action_thread(void* arg);
static void loc_eng_process_atl_action(int conn_handle,
                                       AGpsStatusValue status, AGpsType agps_type);
static void loc_eng_delete_aiding_data_action(GpsAidingData delete_bits);
/* Helper functions to manage the state machine for each ATL session*/
static boolean check_if_any_connection(loc_eng_atl_session_state_e_type conn_state,int session_index);
static boolean check_if_all_connection(loc_eng_atl_session_state_e_type conn_state,int session_index);
static int loc_eng_get_index(int active_conn_handle);
static void loc_eng_atl_close_status(int is_succ);
static void loc_eng_handle_engine_down() ;
static void loc_eng_handle_engine_up() ;
static void loc_eng_send_engine_restart_msg(int msgid, voidFuncPtr setupLocked);

static void loc_eng_agps_ril_init( AGpsRilCallbacks* callbacks );
static void loc_eng_agps_ril_set_ref_location(const AGpsRefLocation *agps_reflocation, size_t sz_struct);
static void loc_eng_agps_ril_set_set_id(AGpsSetIDType type, const char* setid);
static void loc_eng_agps_ril_ni_message(uint8_t *msg, size_t len);
static void loc_eng_agps_ril_update_network_state(int connected, int type, int roaming, const char* extra_info);
static void loc_eng_agps_ril_update_network_availability(int avaiable, const char* apn);
static bool loc_eng_inject_raw_command(char* command, int length);
static int loc_eng_update_criteria(UlpLocationCriteria criteria);
static void loc_eng_msg_sender(void* msg);

// Defines the GpsInterface in gps.h
static const GpsInterface sLocEngInterface =
{
   sizeof(GpsInterface),
   loc_eng_init,
   loc_eng_start,
   loc_eng_stop,
   loc_eng_cleanup,
   loc_eng_inject_time,
   loc_eng_inject_location,
   loc_eng_delete_aiding_data,
   loc_eng_set_position_mode,
   loc_eng_get_extension,
   loc_eng_update_criteria
};

static const AGpsInterface sLocEngAGpsInterface =
{
   sizeof(AGpsInterface),
   loc_eng_agps_init,
   loc_eng_atl_open,
   loc_eng_atl_closed,
   loc_eng_atl_open_failed,
   loc_eng_set_server_proxy
};

static const AGpsRilInterface sLocEngAGpsRilInterface =
{
   sizeof(AGpsRilInterface),
   loc_eng_agps_ril_init,
   loc_eng_agps_ril_set_ref_location,
   loc_eng_agps_ril_set_set_id,
   loc_eng_agps_ril_ni_message,
   loc_eng_agps_ril_update_network_state,
   loc_eng_agps_ril_update_network_availability
};

static const InjectRawCmdInterface sLocEngInjectRawCmdInterface =
{
   sizeof(InjectRawCmdInterface),
   loc_eng_inject_raw_command
};

// Global data structure for location engine
loc_eng_data_s_type loc_eng_data;
loc_eng_msg_position_mode position_mode;

int loc_eng_inited = 0; /* not initialized */
char extra_data[100];

// Address buffers, for addressing setting before init
int    supl_host_set = 0;
char   supl_host_buf[101];
int    supl_port_buf;
int    c2k_host_set = 0;
char   c2k_host_buf[101];
int    c2k_port_buf;

// Logging Improvements
const char *boolStr[]={"False","True"};

// ULP integration
static const ulpInterface* locEngUlpInf = NULL;
static int loc_eng_ulp_init() ;

/*********************************************************************
 * Initialization checking macros
 *********************************************************************/
#define INIT_CHECK_RET(x, c) \
  if (!loc_eng_inited) \
  { \
     /* Not intialized, abort */ \
     LOC_LOGE("%s: GPS not initialized.", x); \
      EXIT_LOG(%s,x); \
     return c; \
  }
#define INIT_CHECK(x) INIT_CHECK_RET(x, -1)
#define INIT_CHECK_VOID(x) INIT_CHECK_RET(x, )

/*===========================================================================
FUNCTION    gps_get_hardware_interface

DESCRIPTION
   Returns the GPS hardware interaface based on LOC API
   if GPS is enabled.

DEPENDENCIES
   None

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
const GpsInterface* gps_get_hardware_interface ()
{
    ENTRY_LOG_CALLFLOW();
    const GpsInterface* ret_val;

    char propBuf[PROPERTY_VALUE_MAX];

    // check to see if GPS should be disabled
    property_get("gps.disable", propBuf, "");
    if (propBuf[0] == '1')
    {
        LOC_LOGD("gps_get_interface returning NULL because gps.disable=1\n");
        ret_val = NULL;
    } else {
        ret_val = &sLocEngInterface;
    }

    EXIT_LOG(%p, ret_val);
    return ret_val;
}

static void loc_eng_msg_sender(void* msg)
{
    loc_eng_msgsnd(loc_eng_data.deferred_q, msg);
}

/*===========================================================================
FUNCTION    loc_eng_init

DESCRIPTION
   Initialize the location engine, this include setting up global datas
   and registers location engien with loc api service.

DEPENDENCIES
   None

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_init(GpsCallbacks* callbacks)
{
   ENTRY_LOG_CALLFLOW();
   int ret_val = -1;

   if (!loc_eng_inited) {
       loc_read_gps_conf();
       callbacks->set_capabilities_cb(gps_conf.CAPABILITIES);

       // Save callbacks
       memset(&loc_eng_data, 0, sizeof (loc_eng_data_s_type));
       loc_eng_data.location_cb  = callbacks->location_cb;
       loc_eng_data.sv_status_cb = callbacks->sv_status_cb;
       loc_eng_data.status_cb    = callbacks->status_cb;
       loc_eng_data.nmea_cb      = callbacks->nmea_cb;
       loc_eng_data.acquire_wakelock_cb = callbacks->acquire_wakelock_cb;
       loc_eng_data.release_wakelock_cb = callbacks->release_wakelock_cb;

       // Loc engine module data initialization
       loc_eng_data.engine_status = GPS_STATUS_NONE;
       loc_eng_data.fix_session_status = GPS_STATUS_NONE;

       loc_eng_data.deferred_action_flags = 0;
       // Mute session
       loc_eng_data.mute_session_state = LOC_MUTE_SESS_NONE;

       loc_eng_data.aiding_data_for_deletion = 0;

       // Create threads (if not yet created)
       loc_eng_msgget( &loc_eng_data.deferred_q);
       loc_eng_data.deferred_action_thread = callbacks->create_thread_cb("loc_api",loc_eng_deferred_action_thread, NULL);
       loc_eng_ulp_init();
#ifdef FEATURE_GNSS_BIT_API
       {
           char baseband[PROPERTY_VALUE_MAX];
           property_get("ro.baseband", baseband, "msm");
           if ((strcmp(baseband,"svlte2a") == 0))
           {
               loc_eng_dmn_conn_loc_api_server_launch(callbacks->create_thread_cb, NULL, NULL);
           }
       }
#endif /* FEATURE_GNSS_BIT_API */

       // XTRA module data initialization
       loc_eng_data.xtra_module_data.download_request_cb = NULL;

       LOC_API_ADAPTER_EVENT_MASK_T event =
           LOC_API_ADAPTER_BIT_PARSED_POSITION_REPORT |
           LOC_API_ADAPTER_BIT_SATELLITE_REPORT |
           LOC_API_ADAPTER_BIT_LOCATION_SERVER_REQUEST |
           LOC_API_ADAPTER_BIT_ASSISTANCE_DATA_REQUEST |
           LOC_API_ADAPTER_BIT_IOCTL_REPORT |
           LOC_API_ADAPTER_BIT_STATUS_REPORT |
           LOC_API_ADAPTER_BIT_NMEA_1HZ_REPORT |
           LOC_API_ADAPTER_BIT_NI_NOTIFY_VERIFY_REQUEST;
       LocEng locEngHandle(event, loc_eng_data.acquire_wakelock_cb,
                           loc_eng_data.release_wakelock_cb, loc_eng_msg_sender);
       loc_eng_data.client_handle = getLocApiAdapter(locEngHandle);

       // call reinit to send initialization messages
       loc_eng_reinit();

       loc_eng_inited = 1;
       ret_val = 0;
   }

   EXIT_LOG(%d, ret_val);
   return ret_val;
}


static int loc_eng_reinit()
{
    ENTRY_LOG();
    int ret_val;

    loc_eng_data.client_handle->reinit();

    LOC_LOGD("loc_eng_init created client, id = %p\n", loc_eng_data.client_handle);

    loc_eng_msg_suple_version *supl_msg(new loc_eng_msg_suple_version(gps_conf.SUPL_VER));
    loc_eng_msgsnd( loc_eng_data.deferred_q, &supl_msg);

    loc_eng_msg_sensor_control_config *sensor_control_config_msg(new loc_eng_msg_sensor_control_config(gps_conf.SENSOR_USAGE));
    loc_eng_msgsnd(loc_eng_data.deferred_q, &sensor_control_config_msg);

    /* Make sure this is specified by the user in the gps.conf file */
    if(gps_conf.GYRO_BIAS_RANDOM_WALK_VALID)
    {
        loc_eng_msg_sensor_properties *sensor_properties_msg(new loc_eng_msg_sensor_properties(gps_conf.GYRO_BIAS_RANDOM_WALK));
        loc_eng_msgsnd(loc_eng_data.deferred_q, &sensor_properties_msg);
    }

    loc_eng_msg_sensor_perf_control_config *sensor_perf_control_conf_msg(new loc_eng_msg_sensor_perf_control_config(gps_conf.SENSOR_CONTROL_MODE,
                                                                                                                    gps_conf.SENSOR_ACCEL_SAMPLES_PER_BATCH,
                                                                                                                    gps_conf.SENSOR_ACCEL_BATCHES_PER_SEC,
                                                                                                                    gps_conf.SENSOR_GYRO_SAMPLES_PER_BATCH,
                                                                                                                    gps_conf.SENSOR_GYRO_BATCHES_PER_SEC
                                                                                                                    ));
    loc_eng_msgsnd(loc_eng_data.deferred_q, &sensor_perf_control_conf_msg);

    ret_val = 0;

    EXIT_LOG(%d, ret_val);
    return ret_val;
}

/*===========================================================================
FUNCTION    loc_eng_deinit

DESCRIPTION
   De-initialize the location engine. This includes stopping fixes and
   closing the client.

DEPENDENCIES
   None

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_deinit()
{
    ENTRY_LOG();
    int ret_val;

    if (loc_eng_data.navigating)
    {
        LOC_LOGD("loc_eng_init: fix not stopped. stop it now.");
        loc_eng_stop();
        loc_eng_data.navigating = FALSE;
    }

    if (loc_eng_data.deferred_action_thread)
    {
        loc_eng_msg *msg(new loc_eng_msg(LOC_ENG_MSG_QUIT));
        loc_eng_msgsnd( loc_eng_data.deferred_q, &msg);

        void* ignoredValue;
        pthread_join(loc_eng_data.deferred_action_thread, &ignoredValue);
        loc_eng_msgremove( loc_eng_data.deferred_q);
        loc_eng_data.deferred_action_thread = NULL;
    }

    // De-initialize ulp
    if (locEngUlpInf != NULL)
    {
        locEngUlpInf->destroy ();
        locEngUlpInf = NULL;
    }

    if (loc_eng_data.client_handle != NULL)
    {
        LOC_LOGD("loc_eng_init: client opened. close it now.");
        delete loc_eng_data.client_handle;
        loc_eng_data.client_handle = NULL;
    }

#ifdef FEATURE_GNSS_BIT_API
    {
        char baseband[PROPERTY_VALUE_MAX];
        property_get("ro.baseband", baseband, "msm");
        if ((strcmp(baseband,"svlte2a") == 0))
        {
            loc_eng_dmn_conn_loc_api_server_unblock();
            loc_eng_dmn_conn_loc_api_server_join();
        }
    }
#endif /* FEATURE_GNSS_BIT_API */

    /* @todo destroy resource by loc_api_glue_init() */

    loc_eng_inited = 0;
    ret_val = 0;

    EXIT_LOG(%d, ret_val);
    return ret_val;
}

/*===========================================================================
FUNCTION    loc_eng_cleanup

DESCRIPTION
   Cleans location engine. The location client handle will be released.

DEPENDENCIES
   None

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
// fully shutting down the GPS is temporarily disabled to avoid intermittent BP crash
#define DISABLE_CLEANUP 1

static void loc_eng_cleanup()
{
    ENTRY_LOG_CALLFLOW();
    INIT_CHECK_VOID("loc_eng_cleanup");

#if DISABLE_CLEANUP
    LOC_LOGD("cleanup disabled");
#else
    // clean up
    loc_eng_deinit();
#endif

    EXIT_LOG(%s, VOID_RET);
}


/*===========================================================================
FUNCTION    loc_eng_start

DESCRIPTION
   Starts the tracking session

DEPENDENCIES
   None

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_start()
{
   ENTRY_LOG_CALLFLOW();
   INIT_CHECK("loc_eng_start");

   loc_eng_msg *msg(new loc_eng_msg(LOC_ENG_MSG_START_FIX));
   loc_eng_msgsnd( loc_eng_data.deferred_q, &msg);
   int ret_val = 0;

   EXIT_LOG(%d, ret_val);
   return ret_val;
}

static int loc_eng_start_handler()
{
   ENTRY_LOG();
   int ret_val;

   if (NULL == locEngUlpInf ||
       locEngUlpInf->start_fix () == 1) {
       ret_val = loc_eng_data.client_handle->startFix();
   }

   if (ret_val == LOC_API_ADAPTER_ERR_SUCCESS ||
       ret_val == LOC_API_ADAPTER_ERR_ENGINE_DOWN)
   {
       loc_eng_data.navigating = TRUE;
   }

   EXIT_LOG(%d, ret_val);
   return ret_val;
}

/*===========================================================================
FUNCTION    loc_eng_stop

DESCRIPTION
   Stops the tracking session

DEPENDENCIES
   None

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_stop()
{
    ENTRY_LOG_CALLFLOW();
    INIT_CHECK("loc_eng_stop");

    loc_eng_msg *msg(new loc_eng_msg(LOC_ENG_MSG_STOP_FIX));
    loc_eng_msgsnd(loc_eng_data.deferred_q, &msg);

    EXIT_LOG(%d, 0);
    return 0;
}

static int loc_eng_stop_handler()
{
   ENTRY_LOG();
   int ret_val;

   // Stops the ULP
   if (locEngUlpInf != NULL)
   {
      locEngUlpInf->stop_fix ();
   }

   ret_val = loc_eng_data.client_handle->stopFix();
   if (ret_val == LOC_API_ADAPTER_ERR_SUCCESS &&
       loc_eng_data.fix_session_status != GPS_STATUS_SESSION_BEGIN)
   {
      loc_inform_gps_status(GPS_STATUS_SESSION_END);
   }
   loc_eng_data.navigating = FALSE;

    EXIT_LOG(%d, ret_val);
    return ret_val;
}

/*===========================================================================
FUNCTION    loc_eng_mute_one_session

DESCRIPTION
   Mutes one session

DEPENDENCIES
   None

RETURN VALUE
   0: Success

SIDE EFFECTS
   N/A

===========================================================================*/
void loc_eng_mute_one_session()
{
    ENTRY_LOG();
    loc_eng_data.mute_session_state = LOC_MUTE_SESS_WAIT;
    EXIT_LOG(%s, VOID_RET);
}

/*===========================================================================
FUNCTION    loc_eng_set_position_mode

DESCRIPTION
   Sets the mode and fix frequency for the tracking session.

DEPENDENCIES
   None

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
static int  loc_eng_set_position_mode(GpsPositionMode mode,
                                      GpsPositionRecurrence recurrence,
                                      uint32_t min_interval,
                                      uint32_t preferred_accuracy,
                                      uint32_t preferred_time)
{
    ENTRY_LOG_CALLFLOW();
    loc_eng_msg_position_mode *msg(new loc_eng_msg_position_mode(mode, recurrence, min_interval,
                                                                 preferred_accuracy, preferred_time));
    loc_eng_msgsnd( loc_eng_data.deferred_q, &msg);
    EXIT_LOG(%d, 0);
    return 0;
}

/*===========================================================================
FUNCTION    loc_eng_inject_time

DESCRIPTION
   This is used by Java native function to do time injection.

DEPENDENCIES
   None

RETURN VALUE
   0

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_inject_time(GpsUtcTime time, int64_t timeReference, int uncertainty)
{
    ENTRY_LOG_CALLFLOW();
    loc_eng_msg_set_time *msg(new loc_eng_msg_set_time(time, timeReference, uncertainty));
    loc_eng_msgsnd( loc_eng_data.deferred_q, &msg);
    EXIT_LOG(%d, 0);
    return 0;
}


/*===========================================================================
FUNCTION    loc_eng_inject_location

DESCRIPTION
   This is used by Java native function to do location injection.

DEPENDENCIES
   None

RETURN VALUE
   0          : Successful
   error code : Failure

SIDE EFFECTS
   N/A
===========================================================================*/
static int loc_eng_inject_location(double latitude, double longitude, float accuracy)
{
    ENTRY_LOG_CALLFLOW();
    loc_eng_msg_inject_location *msg(new loc_eng_msg_inject_location(latitude, longitude, accuracy));
    loc_eng_msgsnd( loc_eng_data.deferred_q, &msg);
    EXIT_LOG(%d, 0);
    return 0;
}


/*===========================================================================
FUNCTION    loc_eng_delete_aiding_data

DESCRIPTION
   This is used by Java native function to delete the aiding data. The function
   updates the global variable for the aiding data to be deleted. If the GPS
   engine is off, the aiding data will be deleted. Otherwise, the actual action
   will happen when gps engine is turned off.

DEPENDENCIES
   Assumes the aiding data type specified in GpsAidingData matches with
   LOC API specification.

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_delete_aiding_data(GpsAidingData f)
{
    ENTRY_LOG_CALLFLOW();
    INIT_CHECK_VOID("loc_eng_delete_aiding_data");

    loc_eng_msg_delete_aiding_data *msg(new loc_eng_msg_delete_aiding_data(f));
    loc_eng_msgsnd( loc_eng_data.deferred_q, &msg);

    EXIT_LOG(%s, VOID_RET);
}

/*===========================================================================
FUNCTION    loc_eng_get_extension

DESCRIPTION
   Get the gps extension to support XTRA.

DEPENDENCIES
   N/A

RETURN VALUE
   The GPS extension interface.

SIDE EFFECTS
   N/A

===========================================================================*/
static const void* loc_eng_get_extension(const char* name)
{
    ENTRY_LOG_CALLFLOW();
    const void* ret_val = NULL;

   if (strcmp(name, GPS_XTRA_INTERFACE) == 0)
   {
      ret_val = &sLocEngXTRAInterface;
   }

   else if (strcmp(name, AGPS_INTERFACE) == 0)
   {
      ret_val = &sLocEngAGpsInterface;
   }

   else if (strcmp(name, GPS_NI_INTERFACE) == 0)
   {
      ret_val = &sLocEngNiInterface;
   }

   else if (strcmp(name, AGPS_RIL_INTERFACE) == 0)
   {
      ret_val = &sLocEngAGpsRilInterface;
   }
   else if (strcmp(name, ULP_RAW_CMD_INTERFACE) == 0)
   {
      ret_val = &sLocEngInjectRawCmdInterface;
   }
   else
   {
      LOC_LOGE ("get_extension: Invalid interface passed in\n");
   }

    EXIT_LOG(%p, ret_val);
    return ret_val;
}

/*===========================================================================
FUNCTION    loc_inform_gps_state

DESCRIPTION
   Informs the GPS Provider about the GPS status

DEPENDENCIES
   None

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_inform_gps_status(GpsStatusValue status)
{
    ENTRY_LOG();

    static GpsStatusValue last_status = GPS_STATUS_NONE;

    GpsStatus gs = { sizeof(gs),status };


    if (loc_eng_data.status_cb)
    {
        LOC_LOGD("loc_inform_gps_status, status: %s", loc_get_gps_status_name(status));
        CALLBACK_LOG_CALLFLOW("status_cb");
        loc_eng_data.status_cb(&gs);

        // Restore session begin if needed
        if (status == GPS_STATUS_ENGINE_ON && last_status == GPS_STATUS_SESSION_BEGIN)
        {
            GpsStatus gs_sess_begin = { sizeof(gs_sess_begin),GPS_STATUS_SESSION_BEGIN };
            LOC_LOGD("loc_inform_gps_status, status: GPS_STATUS_SESSION_BEGIN");
            CALLBACK_LOG_CALLFLOW("status_cb");
            loc_eng_data.status_cb(&gs_sess_begin);
        }
    }

    last_status = status;

    EXIT_LOG(%s, VOID_RET);
}

/*===========================================================================
FUNCTION    loc_eng_agps_reinit

DESCRIPTION
   2nd half of loc_eng_agps_init(), singled out for modem restart to use.

DEPENDENCIES
   NONE

RETURN VALUE
   0

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_agps_reinit()
{
    ENTRY_LOG();
    // Data connection for AGPS
    loc_eng_data.data_connection_bearer = AGPS_APN_BEARER_INVALID;
    int i=0;
    for(i=0;i <= MAX_NUM_ATL_CONNECTIONS; i++ )
    {
        loc_eng_data.atl_conn_info[i].active = FALSE;
        loc_eng_data.atl_conn_info[i].conn_state = LOC_CONN_IDLE;
        loc_eng_data.atl_conn_info[i].conn_handle = INVALID_ATL_CONNECTION_HANDLE; //since connection handles can be 0
        loc_eng_data.atl_conn_info[i].agps_type = AGPS_TYPE_INVALID;
    }

    // Set server addresses which came before init
    if (supl_host_set)
    {
        loc_eng_set_server(AGPS_TYPE_SUPL, supl_host_buf, supl_port_buf);
    }

    if (c2k_host_set)
    {
        loc_eng_set_server(AGPS_TYPE_C2K, c2k_host_buf, c2k_port_buf);
    }
    EXIT_LOG(%p, VOID_RET);
}
/*===========================================================================
FUNCTION    loc_eng_agps_init

DESCRIPTION
   Initialize the AGps interface.

DEPENDENCIES
   NONE

RETURN VALUE
   0

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_agps_init(AGpsCallbacks* callbacks)
{
    ENTRY_LOG_CALLFLOW();
    loc_eng_data.agps_status_cb = callbacks->status_cb;

    loc_eng_agps_reinit();
    EXIT_LOG(%p, VOID_RET);
}

static void loc_eng_atl_open_status(int is_succ, const char* apn)
{
    ENTRY_LOG();
    //Go through all the active connection states to determine which command to send to LOC MW and the
    //state machine updates that need to be done
    for (int i=0;i< MAX_NUM_ATL_CONNECTIONS;i++)
    {
        LOC_LOGD("loc_eng_atl_open_status, is_active = %d, handle = %d, state = %d, bearer: %d\n",
                 loc_eng_data.atl_conn_info[i].active,
                 (int) loc_eng_data.atl_conn_info[i].conn_handle,
                 loc_eng_data.atl_conn_info[i].conn_state,
                 loc_eng_data.data_connection_bearer);

        if ((loc_eng_data.atl_conn_info[i].active == TRUE) &&
            (loc_eng_data.atl_conn_info[i].conn_state == LOC_CONN_OPEN_REQ))
        {
            //update the session states
            loc_eng_data.atl_conn_info[i].conn_state = is_succ ? LOC_CONN_OPEN : LOC_CONN_IDLE;

            loc_eng_data.client_handle->atlOpenStatus(loc_eng_data.atl_conn_info[i].conn_handle,
                                                      is_succ,
                                                      (char*)apn,
                                                      loc_eng_data.data_connection_bearer,
                                                      loc_eng_data.atl_conn_info[i].agps_type);
        }
    }
    EXIT_LOG(%s, VOID_RET);
}
/*===========================================================================
FUNCTION    loc_eng_atl_close_status

DESCRIPTION
   This function makes an IOCTL call to return data call close status to modem

DEPENDENCIES

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_atl_close_status(int is_succ)
{
    ENTRY_LOG();

    for (int i=0;i< MAX_NUM_ATL_CONNECTIONS;i++)
    {
        LOC_LOGD("loc_eng_atl_close_status, is_active = %d, handle = %d, state = %d, bearer: %d\n",
                 loc_eng_data.atl_conn_info[i].active, (int) loc_eng_data.atl_conn_info[i].conn_handle,
                 loc_eng_data.atl_conn_info[i].conn_state,
                 loc_eng_data.data_connection_bearer);

        if ( loc_eng_data.atl_conn_info[i].active == TRUE &&
             ((loc_eng_data.atl_conn_info[i].conn_state == LOC_CONN_CLOSE_REQ )||
              (loc_eng_data.atl_conn_info[i].conn_state == LOC_CONN_IDLE)))
        {

            //update the session states
            loc_eng_data.atl_conn_info[i].conn_state = LOC_CONN_IDLE;
            loc_eng_data.atl_conn_info[i].active = FALSE;
            loc_eng_data.atl_conn_info[i].conn_handle = INVALID_ATL_CONNECTION_HANDLE;
            loc_eng_data.atl_conn_info[i].agps_type = AGPS_TYPE_INVALID;
        }
    }

    EXIT_LOG(%s, VOID_RET);
}

/*===========================================================================
FUNCTION    loc_eng_atl_open

DESCRIPTION
   This function is called when on-demand data connection opening is successful.
It should inform ARM 9 about the data open result.

DEPENDENCIES
   NONE

RETURN VALUE
   0

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_atl_open(const char* apn, AGpsBearerType bearerType)
{
    ENTRY_LOG_CALLFLOW();
    INIT_CHECK("loc_eng_atl_open");

    if (apn == NULL)
    {
        LOC_LOGE("APN Name NULL\n");
        return 0;
    }

    LOC_LOGD("loc_eng_atl_open APN name = [%s]", apn);
#ifdef FEATURE_GNSS_BIT_API
    loc_eng_dmn_conn_loc_api_server_data_conn(1);
#endif
    int apn_len = smaller_of(strlen (apn), MAX_APN_LEN);
    loc_eng_msg_atl_open_status *msg(new loc_eng_msg_atl_open_status(apn, apn_len, bearerType));
    loc_eng_msgsnd( loc_eng_data.deferred_q, &msg);

    EXIT_LOG(%d, 0);
    return 0;
}

/*===========================================================================
FUNCTION    loc_eng_atl_closed

DESCRIPTION
   This function is called when on-demand data connection closing is done.
It should inform ARM 9 about the data close result.

DEPENDENCIES
   NONE

RETURN VALUE
   0

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_atl_closed()
{
    ENTRY_LOG_CALLFLOW();
    INIT_CHECK("loc_eng_atl_closed");

#ifdef FEATURE_GNSS_BIT_API
    loc_eng_dmn_conn_loc_api_server_data_conn(0);
#endif
    loc_eng_msg *msg(new loc_eng_msg(LOC_ENG_MSG_ATL_CLOSE_STATUS));
    loc_eng_msgsnd( loc_eng_data.deferred_q, &msg);

    EXIT_LOG(%d, 0);
    return 0;
}

/*===========================================================================
FUNCTION    loc_eng_atl_open_failed

DESCRIPTION
   This function is called when on-demand data connection opening has failed.
It should inform ARM 9 about the data open result.

DEPENDENCIES
   NONE

RETURN VALUE
   0

SIDE EFFECTS
   N/A

===========================================================================*/
int loc_eng_atl_open_failed()
{
    ENTRY_LOG_CALLFLOW();
    INIT_CHECK("loc_eng_atl_open_failed");

#ifdef FEATURE_GNSS_BIT_API
    loc_eng_dmn_conn_loc_api_server_data_conn(-1);
#endif
    loc_eng_msg *msg(new loc_eng_msg(LOC_ENG_MSG_ATL_OPEN_FAILED));
    loc_eng_msgsnd( loc_eng_data.deferred_q, &msg);
    EXIT_LOG(%d, 0);
    return 0;
}

/*===========================================================================

FUNCTION resolve_in_addr

DESCRIPTION
   Translates a hostname to in_addr struct

DEPENDENCIES
   n/a

RETURN VALUE
   TRUE if successful

SIDE EFFECTS
   n/a

===========================================================================*/
static boolean resolve_in_addr(const char *host_addr, struct in_addr *in_addr_ptr)
{
    ENTRY_LOG();
    boolean ret_val = TRUE;

    struct hostent             *hp;
    hp = gethostbyname(host_addr);
    if (hp != NULL) /* DNS OK */
    {
        memcpy(in_addr_ptr, hp->h_addr_list[0], hp->h_length);
    }
    else
    {
        /* Try IP representation */
        if (inet_aton(host_addr, in_addr_ptr) == 0)
        {
            /* IP not valid */
            LOC_LOGE("DNS query on '%s' failed\n", host_addr);
            ret_val = FALSE;
        }
    }

    EXIT_LOG(%s, boolStr[ret_val!=0]);
    return ret_val;
}

/*===========================================================================
FUNCTION    loc_eng_set_server

DESCRIPTION
   This is used to set the default AGPS server. Server address is obtained
   from gps.conf.

DEPENDENCIES
   NONE

RETURN VALUE
   0

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_set_server(AGpsType type, const char* hostname, int port)
{
    ENTRY_LOG();
    int ret = 0;

    if (AGPS_TYPE_C2K == type) {
        struct in_addr addr;
        if (!resolve_in_addr(hostname, &addr))
        {
            LOC_LOGE("loc_eng_set_server, hostname %s cannot be resolved.\n", hostname);
            ret = -2;
        } else {
            unsigned int ip = htonl(addr.s_addr);
            loc_eng_msg_set_server_ipv4 *msg(new loc_eng_msg_set_server_ipv4(ip, port));
            loc_eng_msgsnd( loc_eng_data.deferred_q, &msg);
        }
    } else if (AGPS_TYPE_SUPL == type) {
        char url[MAX_URL_LEN];
        unsigned int len = snprintf(url, sizeof(url), "%s:%u", hostname, (unsigned) port);

        if (sizeof(url) > len) {
            loc_eng_msg_set_server_url *msg(new loc_eng_msg_set_server_url(url, len));
            loc_eng_msgsnd( loc_eng_data.deferred_q, &msg);
        }
    }
    EXIT_LOG(%d, ret);
    return ret;
}

/*===========================================================================
FUNCTION    loc_eng_set_server_proxy

DESCRIPTION
   If loc_eng_set_server is called before loc_eng_init, it doesn't work. This
   proxy buffers server settings and calls loc_eng_set_server when the client is
   open.

DEPENDENCIES
   NONE

RETURN VALUE
   0

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_set_server_proxy(AGpsType type, const char* hostname, int port)
{
    ENTRY_LOG_CALLFLOW();
    int ret_val;

    if (loc_eng_inited)
    {
        loc_eng_set_server(type, hostname, port);
    } else {
        LOC_LOGW("set_server called before init. save the address, type: %d, hostname: %s, port: %d",
                 (int) type, hostname, port);
        switch (type)
        {
        case AGPS_TYPE_SUPL:
            strlcpy(supl_host_buf, hostname, sizeof supl_host_buf);
            supl_port_buf = port;
            supl_host_set = 1;
            break;
        case AGPS_TYPE_C2K:
            strlcpy(c2k_host_buf, hostname, sizeof c2k_host_buf);
            c2k_port_buf = port;
            c2k_host_set = 1;
            break;
        default:
            LOC_LOGE("loc_eng_set_server_proxy, unknown server type = %d", (int) type);
        }
    }
    ret_val = 0;

    EXIT_LOG(%d, ret_val);
    return ret_val;
}

// Below stub functions are members of sLocEngAGpsRilInterface
static void loc_eng_agps_ril_init( AGpsRilCallbacks* callbacks ) {}
static void loc_eng_agps_ril_set_ref_location(const AGpsRefLocation *agps_reflocation, size_t sz_struct) {}
static void loc_eng_agps_ril_set_set_id(AGpsSetIDType type, const char* setid) {}
static void loc_eng_agps_ril_ni_message(uint8_t *msg, size_t len) {}
static void loc_eng_agps_ril_update_network_state(int connected, int type, int roaming, const char* extra_info) {}

/*===========================================================================
FUNCTION    loc_eng_agps_ril_update_network_availability

DESCRIPTION
   Sets data call allow vs disallow flag to modem
   This is the only member of sLocEngAGpsRilInterface implemented.

DEPENDENCIES
   None

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_agps_ril_update_network_availability(int available, const char* apn)
{
    ENTRY_LOG_CALLFLOW();
    if (apn != NULL)
    {
        LOC_LOGD("loc_eng_agps_ril_update_network_availability: APN Name = [%s]\n", apn);
        int apn_len = smaller_of(strlen (apn), MAX_APN_LEN);
        loc_eng_msg_set_data_enable *msg(new loc_eng_msg_set_data_enable(apn, apn_len, available));
        loc_eng_msgsnd( loc_eng_data.deferred_q, &msg);
    }
    EXIT_LOG(%s, VOID_RET);
}

/*===========================================================================
FUNCTION    loc_eng_delete_aiding_data_action

DESCRIPTION
   This is used to remove the aiding data when GPS engine is off.

DEPENDENCIES
   Assumes the aiding data type specified in GpsAidingData matches with
   LOC API specification.

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_delete_aiding_data_action(GpsAidingData bits)
{
    ENTRY_LOG();
    loc_eng_data.client_handle->deleteAidingData(bits);
    EXIT_LOG(%s, VOID_RET);
}

/*===========================================================================
FUNCTION    loc_eng_report_status

DESCRIPTION
   Reports GPS engine state to Java layer.

DEPENDENCIES
   N/A

RETURN VALUE
   N/A

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_report_status (GpsStatusValue status)
{
    ENTRY_LOG();
    // Switch from WAIT to MUTE, for "engine on" or "session begin" event
    if (status == GPS_STATUS_SESSION_BEGIN || status == GPS_STATUS_ENGINE_ON)
    {
        if (loc_eng_data.mute_session_state == LOC_MUTE_SESS_WAIT)
        {
            LOC_LOGD("loc_eng_report_status: mute_session_state changed from WAIT to IN SESSION");
            loc_eng_data.mute_session_state = LOC_MUTE_SESS_IN_SESSION;
        }
    }

    // Switch off MUTE session
    if (loc_eng_data.mute_session_state == LOC_MUTE_SESS_IN_SESSION &&
        (status == GPS_STATUS_SESSION_END || status == GPS_STATUS_ENGINE_OFF))
    {
        LOC_LOGD("loc_eng_report_status: mute_session_state changed from IN SESSION to NONE");
        loc_eng_data.mute_session_state = LOC_MUTE_SESS_NONE;
    }

    // Session End is not reported during Android navigating state
    if (status != GPS_STATUS_NONE &&
        !(status == GPS_STATUS_SESSION_END && loc_eng_data.navigating) &&
        !(status == GPS_STATUS_SESSION_BEGIN && !loc_eng_data.navigating))
    {
        if (loc_eng_data.mute_session_state != LOC_MUTE_SESS_IN_SESSION)
        {
            // Inform GpsLocationProvider about mNavigating status
            loc_inform_gps_status(status);
        }
        else {
            LOC_LOGD("loc_eng_report_status: muting the status report.");
        }
    }

    // Only keeps ENGINE ON/OFF in engine_status
    if (status == GPS_STATUS_ENGINE_ON || status == GPS_STATUS_ENGINE_OFF)
    {
        loc_eng_data.engine_status = status;
    }

    // Only keeps SESSION BEGIN/END in fix_session_status
    if (status == GPS_STATUS_SESSION_BEGIN || status == GPS_STATUS_SESSION_END)
    {
        loc_eng_data.fix_session_status = status;
    }
    EXIT_LOG(%s, VOID_RET);
}

/*===========================================================================
FUNCTION    loc_eng_report_agps_status

DESCRIPTION
   This functions calls the native callback function for GpsLocationProvider
to update AGPS status. The expected behavior from GpsLocationProvider is the following.

   For status GPS_REQUEST_AGPS_DATA_CONN, GpsLocationProvider will inform the open
   status of the data connection if it is already open, or try to bring up a data
   connection when it is not.

   For status GPS_RELEASE_AGPS_DATA_CONN, GpsLocationProvider will try to bring down
   the data connection when it is open. (use this carefully)

   Currently, no actions are taken for other status, such as GPS_AGPS_DATA_CONNECTED,
   GPS_AGPS_DATA_CONN_DONE or GPS_AGPS_DATA_CONN_FAILED.

DEPENDENCIES
   None

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_report_agps_status(AGpsType type,
                                       AGpsStatusValue status,
                                       unsigned long ipv4_addr,
                                       unsigned char * ipv6_addr)
{
    ENTRY_LOG();
    int ret = 1;

   AGpsStatus agpsStatus;
   if (loc_eng_data.agps_status_cb == NULL)
   {
       LOC_LOGE("loc_eng_report_agps_status, callback not initialized.\n");
       ret = 0;
   } else {
       LOC_LOGD("loc_eng_report_agps_status, type = %d, status = %d, ipv4_addr = %d\n",
                (int) type, (int) status,  (int) ipv4_addr);

       agpsStatus.size      = sizeof(agpsStatus);
       agpsStatus.type      = (AGPS_TYPE_INVALID == type) ? AGPS_TYPE_SUPL : type;
       agpsStatus.status    = status;
       agpsStatus.ipv4_addr = ipv4_addr;
       if (ipv6_addr != NULL) {
           memcpy(agpsStatus.ipv6_addr, ipv6_addr, 16);
       } else {
           memset(agpsStatus.ipv6_addr, 0, 16);
       }
       switch (status)
       {
       case GPS_REQUEST_AGPS_DATA_CONN:
           CALLBACK_LOG_CALLFLOW("agps_status_cb");
           loc_eng_data.agps_status_cb(&agpsStatus);
           break;
       case GPS_RELEASE_AGPS_DATA_CONN:
           // This will not close always-on connection. Comment out if it does.
           CALLBACK_LOG_CALLFLOW("agps_status_cb");
           loc_eng_data.agps_status_cb(&agpsStatus);
           break;
       }
   }

    EXIT_LOG(%s, ret);
    return ret;
}


/*===========================================================================
FUNCTION    loc_eng_process_atl_action

DESCRIPTION
   This is used to inform the location engine of the processing status for
   data connection open/close request.

DEPENDENCIES
   None

RETURN VALUE
   NONE

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_process_atl_action(int conn_handle,
                                       AGpsStatusValue status, AGpsType agps_type)

{
    ENTRY_LOG();
    boolean                             ret_val;

    LOC_LOGD("loc_eng_process_atl_action,handle = 0x%lx; status = %d; agps_type = %d\n", (long) conn_handle, status, agps_type);

    //Check if the incoming connection handle already has a atl state which exists and get associated session index
    int session_index = 0;
    session_index = loc_eng_get_index(conn_handle);
    if (session_index == MAX_NUM_ATL_CONNECTIONS)
    {
        //An error has occured and so print out an error message and return. End the call flow
        LOC_LOGE("loc_eng_process_conn_request- session index error %d\n",
                 session_index);
    } else {
        LOC_LOGD("loc_eng_process_atl_action.session_index = %x, active_session_state = %x ,"
                 "active_session_handle = %x session_active %d\n", session_index,
                 loc_eng_data.atl_conn_info[session_index].conn_state,
                 (int) loc_eng_data.atl_conn_info[session_index].conn_handle,
                 loc_eng_data.atl_conn_info[session_index].active);
        //ATL data connection open request from modem
        if(status == GPS_REQUEST_AGPS_DATA_CONN )
        {
            //Go into Open request state only if current state is in IDLE
            if(loc_eng_data.atl_conn_info[session_index].conn_state == LOC_CONN_IDLE)
            {
                loc_eng_data.atl_conn_info[session_index].conn_state = LOC_CONN_OPEN_REQ;
                loc_eng_data.atl_conn_info[session_index].conn_handle = conn_handle;
                loc_eng_data.atl_conn_info[session_index].agps_type = agps_type;
                if (check_if_any_connection(LOC_CONN_OPEN, session_index))
                {
                    //PPP connection has already been opened for some other handle. So simply acknowledge the modem.
                    loc_eng_atl_open_status(SUCCESS, NULL);
                }else if (check_if_any_connection(LOC_CONN_OPEN_REQ, session_index))
                {
                    //When COnnectivity Manger acknowledges the reqest all ATL requests will be acked
                    LOC_LOGD("PPP Open has been requested already for some other handle /n");
                }else
                {//Request connectivity manager to bringup a data connection
                    loc_eng_report_agps_status(agps_type,status,INADDR_NONE,NULL);
                }
            }
            else if(loc_eng_data.atl_conn_info[session_index].conn_state == LOC_CONN_OPEN)
            {
                //PPP connection has already been opened for this handle. So simply acknowledge the modem.
                loc_eng_data.atl_conn_info[session_index].conn_state = LOC_CONN_OPEN_REQ;
                loc_eng_atl_open_status(SUCCESS, NULL);

            }else if(loc_eng_data.atl_conn_info[session_index].conn_state == LOC_CONN_CLOSE_REQ)
            {
                //In this case the open request has come in for a handle which is already in
                //CLOSE_REQ.  Here we ack the modem a failure for this request as it came in
                //even before the modem got the ack for the precedding close request for this handle.
                LOC_LOGE("ATL Open req came in for handle %d when in CLOSE_REQ state",
                         (int) loc_eng_data.atl_conn_info[session_index].conn_handle);
                loc_eng_data.atl_conn_info[session_index].conn_state = LOC_CONN_OPEN_REQ;
                loc_eng_atl_open_status(FAILURE, NULL);
            }else
            {//In this case the open request has come in for a handle which is already in OPEN_REQ.
                //Here ack to the modem request will be sent when we get an equivalent ack from
                //the Connectivity Manager.
                LOC_LOGD("ATL Open req came in for handle %d when in OPEN_REQ state",
                         (int) loc_eng_data.atl_conn_info[session_index].conn_handle);
            }

        }else if (status == GPS_RELEASE_AGPS_DATA_CONN)
        {
            if(loc_eng_data.atl_conn_info[session_index].conn_state == LOC_CONN_OPEN)
            {
                loc_eng_data.atl_conn_info[session_index].conn_state = LOC_CONN_CLOSE_REQ;
                if(check_if_all_connection(LOC_CONN_IDLE,session_index))
                {
                    loc_eng_report_agps_status(agps_type,status,INADDR_NONE,NULL);
                }else
                {
                    //Simply acknowledge to the modem that the connection is closed even through we dont pass
                    //that message to the connectivity manager
                    loc_eng_atl_close_status(SUCCESS);
                }
            }else if(loc_eng_data.atl_conn_info[session_index].conn_state == LOC_CONN_IDLE)
            {
                loc_eng_data.atl_conn_info[session_index].conn_state = LOC_CONN_CLOSE_REQ;
                //The connection is already closed previously so simply acknowledge the modem.
                loc_eng_atl_close_status(SUCCESS);
            }else if(loc_eng_data.atl_conn_info[session_index].conn_state == LOC_CONN_OPEN_REQ)
            {
                //In this case the close request has come in for a handle which is already in
                //OPEN_REQ.  In this case we ack the modem a failure for this request as it came in
                //even before the modem got the ack for the precedding open request for this handle.
                LOC_LOGE("ATL Close req came in for handle %d when in OPEN_REQ state",
                         (int) loc_eng_data.atl_conn_info[session_index].conn_handle);
                loc_eng_data.atl_conn_info[session_index].conn_state = LOC_CONN_CLOSE_REQ;
                loc_eng_atl_close_status(FAILURE);
            }else
            {//In this case the close request has come in for a handle which is already in CLOSE_REQ.
                //In this case an ack to the modem request will be sent when we get an equivalent ack
                //from the Connectivity Manager.
                LOC_LOGD("ATL Open req came in for handle %d when in CLOSE_REQ state",
                         (int) loc_eng_data.atl_conn_info[session_index].conn_handle);
            }
        }
    }

    EXIT_LOG(%s, VOID_RET);
}

/*===========================================================================
FUNCTION loc_eng_handle_engine_down
         loc_eng_handle_engine_up

DESCRIPTION
   Calls this function when it is detected that modem restart is happening.
   Either we detected the modem is down or received modem up event.
   This must be called from the deferred thread to avoid race condition.

DEPENDENCIES
   None

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
void loc_eng_handle_engine_down()
{
    ENTRY_LOG();
    loc_ni_reset_on_engine_restart();
    loc_eng_report_status(GPS_STATUS_ENGINE_OFF);
    EXIT_LOG(%s, VOID_RET);
}

void loc_eng_handle_engine_up()
{
    ENTRY_LOG();
    loc_eng_reinit();

    if (loc_eng_data.agps_status_cb != NULL) {
        loc_eng_agps_reinit();
    }

    loc_eng_report_status(GPS_STATUS_ENGINE_ON);

    // modem is back up.  If we crashed in the middle of navigating, we restart.
    if (loc_eng_data.navigating) {
        loc_eng_data.client_handle->setPositionMode(
            position_mode.pMode,
            position_mode.pRecurrence,
            position_mode.minInterval,
            position_mode.preferredAccuracy,
            position_mode.preferredTime);
        // not mutex protected, assuming fw won't call start twice without a
        // stop call in between.
        loc_eng_start_handler();
    }
    EXIT_LOG(%s, VOID_RET);
}

/*===========================================================================
FUNCTION loc_eng_deferred_action_thread

DESCRIPTION
   Main routine for the thread to execute loc_eng commands.

DEPENDENCIES
   None

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_deferred_action_thread(void* arg)
{
    ENTRY_LOG();
    loc_eng_msg *msg;
    static int cnt = 0;

    // make sure we do not run in background scheduling group
    set_sched_policy(gettid(), SP_FOREGROUND);

    while (1)
    {
        LOC_LOGD("%s:%d] %d listening ...\n", __func__, __LINE__, cnt++);

        loc_eng_data.release_wakelock_cb();
        // we are only sending / receiving msg pointers
        int length = loc_eng_msgrcv(loc_eng_data.deferred_q, (void **) &msg);
        loc_eng_data.acquire_wakelock_cb();
        if (length <= 0) {
            LOC_LOGE("%s:%d] fail receiving msg\n", __func__, __LINE__);
            return;
        }

        LOC_LOGD("%s:%d] received msg_id = %s\n",
                 __func__, __LINE__, loc_get_msg_name(msg->msgid));

        switch(msg->msgid) {
        case LOC_ENG_MSG_QUIT:
            /* processed before the switch statement */
            break;

        case LOC_ENG_MSG_REQUEST_NI:
        {
            loc_eng_msg_request_ni *niMsg = (loc_eng_msg_request_ni*)msg;
            loc_ni_request_handler(niMsg->notify, niMsg->passThroughData);
        }
            break;

        case LOC_ENG_MSG_INFORM_NI_RESPONSE:
        {
            loc_eng_msg_inform_ni_response *nrMsg = (loc_eng_msg_inform_ni_response*)msg;
            loc_eng_data.client_handle->informNiResponse(nrMsg->response, nrMsg->passThroughData);
        }
            break;

        case LOC_ENG_MSG_START_FIX:
            loc_eng_start_handler();
            break;

        case LOC_ENG_MSG_STOP_FIX:
            if (loc_eng_data.agps_request_pending)
            {
                loc_eng_data.stop_request_pending = true;
                LOC_LOGD("loc_eng_stop - deferring stop until AGPS data call is finished\n");
            } else {
                loc_eng_stop_handler();
            }
            break;

        case LOC_ENG_MSG_SET_POSITION_MODE:
        {
            loc_eng_msg_position_mode *pmMsg = (loc_eng_msg_position_mode*)msg;
            loc_eng_data.client_handle->setPositionMode(pmMsg->pMode, pmMsg->pRecurrence,
                                                        pmMsg->minInterval,pmMsg->preferredAccuracy,
                                                        pmMsg->preferredTime);
            memcpy((void*)&position_mode, (void*)pmMsg, sizeof(*pmMsg));
        }
            break;

        case LOC_ENG_MSG_SET_TIME:
        {
            loc_eng_msg_set_time *tMsg = (loc_eng_msg_set_time*)msg;
            loc_eng_data.client_handle->setTime(tMsg->time, tMsg->timeReference,
                                                tMsg->uncertainty);
        }
            break;

        case LOC_ENG_MSG_INJECT_LOCATION:
        {
            loc_eng_msg_inject_location *ilMsg = (loc_eng_msg_inject_location*) msg;
            loc_eng_data.client_handle->injectPosition(ilMsg->latitude, ilMsg->longitude,
                                                       ilMsg->accuracy);
        }
            break;

        case LOC_ENG_MSG_SET_SERVER_IPV4:
        {
            loc_eng_msg_set_server_ipv4 *ssiMsg = (loc_eng_msg_set_server_ipv4*)msg;
            loc_eng_data.client_handle->setServer(ssiMsg->nl_addr, ssiMsg->port);
        }
            break;

        case LOC_ENG_MSG_SET_SERVER_URL:
        {
            loc_eng_msg_set_server_url *ssuMsg = (loc_eng_msg_set_server_url*)msg;
            loc_eng_data.client_handle->setServer(ssuMsg->url, ssuMsg->len);
        }
            break;

        case LOC_ENG_MSG_SUPL_VERSION:
        {
            loc_eng_msg_suple_version *svMsg = (loc_eng_msg_suple_version*)msg;
            loc_eng_data.client_handle->setSUPLVersion(svMsg->supl_version);
        }
            break;

        case LOC_ENG_MSG_SET_SENSOR_CONTROL_CONFIG:
        {
            loc_eng_msg_sensor_control_config *sccMsg = (loc_eng_msg_sensor_control_config*)msg;
            loc_eng_data.client_handle->setSensorControlConfig(sccMsg->sensorsDisabled);
        }
            break;

        case LOC_ENG_MSG_SET_SENSOR_PROPERTIES:
        {
            loc_eng_msg_sensor_properties *spMsg = (loc_eng_msg_sensor_properties*)msg;
            loc_eng_data.client_handle->setSensorProperties(spMsg->gyroBiasVarianceRandomWalk);
        }
            break;

        case LOC_ENG_MSG_SET_SENSOR_PERF_CONTROL_CONFIG:
        {
            loc_eng_msg_sensor_perf_control_config *spccMsg = (loc_eng_msg_sensor_perf_control_config*)msg;
            loc_eng_data.client_handle->setSensorPerfControlConfig(spccMsg->controlMode, spccMsg->accelSamplesPerBatch, spccMsg->accelBatchesPerSec,
                                                                   spccMsg->gyroSamplesPerBatch, spccMsg->gyroBatchesPerSec);
        }
            break;

        case LOC_ENG_MSG_REPORT_POSITION:
            if (loc_eng_data.location_cb != NULL &&
               loc_eng_data.mute_session_state != LOC_MUTE_SESS_IN_SESSION)
            {
                loc_eng_msg_report_position *rpMsg = (loc_eng_msg_report_position*)msg;
                // only if
                // 1. this is NOT a intermediate fix; or
                // 2. if it intermediate..
                //  2.1 we accepte intermediate; and
                //  2.2 it is NOT the case that
                //   2.2.1 there is inaccuracy; and
                //   2.2.2 we care about inaccuracy; and
                //   2.2.3 the inaccuracy exceeds our tolerance
                if (!rpMsg->intermediate ||
                    (gps_conf.INTERMEDIATE_POS &&
                     !((rpMsg->location.flags & GPS_LOCATION_HAS_ACCURACY) &&
                       (gps_conf.ACCURACY_THRES != 0) &&
                       (rpMsg->location.accuracy > gps_conf.ACCURACY_THRES)))) {
                    CALLBACK_LOG_CALLFLOW("location_cb");
                    loc_eng_data.location_cb((GpsLocation*)&(rpMsg->location));
                }
            }

            break;

        case LOC_ENG_MSG_REPORT_SV:
            if (loc_eng_data.sv_status_cb != NULL &&
               loc_eng_data.mute_session_state != LOC_MUTE_SESS_IN_SESSION)
            {
                loc_eng_msg_report_sv *rsMsg = (loc_eng_msg_report_sv*)msg;
                CALLBACK_LOG_CALLFLOW("sv_status_cb");
                loc_eng_data.sv_status_cb((GpsSvStatus*)&(rsMsg->svStatus));
            }
            break;

        case LOC_ENG_MSG_REPORT_STATUS:
            loc_eng_report_status(((loc_eng_msg_report_status*)msg)->status);
            break;

        case LOC_ENG_MSG_REPORT_NMEA:
            if (NULL != loc_eng_data.nmea_cb) {
                loc_eng_msg_report_nmea* nmMsg = (loc_eng_msg_report_nmea*)msg;
                CALLBACK_LOG_CALLFLOW("nmea_cb");
             struct timeval tv;
                gettimeofday(&tv, (struct timezone *) NULL);
                int64_t now = tv.tv_sec * 1000LL + tv.tv_usec / 1000;
                loc_eng_data.nmea_cb(now, nmMsg->nmea, nmMsg->length);
            }
            break;

        case LOC_ENG_MSG_REQUEST_ATL:
        {
            loc_eng_msg_request_atl* arqMsg = (loc_eng_msg_request_atl*)msg;
            loc_eng_process_atl_action(arqMsg->handle, GPS_REQUEST_AGPS_DATA_CONN, arqMsg->type);
        }
            break;

        case LOC_ENG_MSG_RELEASE_ATL:
        {
            loc_eng_msg_release_atl* arlMsg = (loc_eng_msg_release_atl*)msg;
            loc_eng_process_atl_action(arlMsg->handle, GPS_RELEASE_AGPS_DATA_CONN, AGPS_TYPE_ANY);
        }
            break;

        case LOC_ENG_MSG_REQUEST_XTRA_DATA:
            if (loc_eng_data.xtra_module_data.download_request_cb != NULL)
            {
                loc_eng_data.xtra_module_data.download_request_cb();
            }
            break;

        case LOC_ENG_MSG_REQUEST_TIME:
            break;

        case LOC_ENG_MSG_REQUEST_POSITION:
            break;

        case LOC_ENG_MSG_DELETE_AIDING_DATA:
            loc_eng_data.aiding_data_for_deletion |= ((loc_eng_msg_delete_aiding_data*)msg)->type;
            break;

        case LOC_ENG_MSG_ENABLE_DATA:
        {
            loc_eng_msg_set_data_enable *unaMsg = (loc_eng_msg_set_data_enable*)msg;
            loc_eng_data.client_handle->enableData(unaMsg->enable);
            loc_eng_data.client_handle->setAPN(unaMsg->apn, unaMsg->length);
        }
            break;

        case LOC_ENG_MSG_INJECT_XTRA_DATA:
        {
            loc_eng_msg_inject_xtra_data *xdMsg = (loc_eng_msg_inject_xtra_data*)msg;
            loc_eng_data.client_handle->setXtraData(xdMsg->data, xdMsg->length);
        }
            break;

        case LOC_ENG_MSG_ATL_OPEN_STATUS:
        {
            loc_eng_msg_atl_open_status *aosMsg = (loc_eng_msg_atl_open_status*)msg;
            loc_eng_data.data_connection_bearer = aosMsg->bearerType;
            loc_eng_data.client_handle->setAPN(aosMsg->apn, aosMsg->length);
            loc_eng_atl_open_status(SUCCESS, aosMsg->apn);
        }
            break;

        case LOC_ENG_MSG_ATL_CLOSE_STATUS:
        {
            loc_eng_atl_close_status(SUCCESS);
            loc_eng_data.data_connection_bearer = AGPS_APN_BEARER_INVALID;
        }
            break;

        case LOC_ENG_MSG_ATL_OPEN_FAILED:
            if(loc_eng_data.data_connection_bearer == AGPS_APN_BEARER_INVALID)
            {
                loc_eng_atl_open_status(FAILURE, NULL);
            } else {
                loc_eng_atl_close_status(FAILURE);
            }
            loc_eng_data.data_connection_bearer = AGPS_APN_BEARER_INVALID;
            break;

        case LOC_ENG_MSG_ENGINE_DOWN:
            loc_eng_handle_engine_down();
            break;

        case LOC_ENG_MSG_ENGINE_UP:
            loc_eng_handle_engine_up();
            break;

        default:
            LOC_LOGE("unsupported msgid = %d\n", msg->msgid);
            break;
        }

        if ( (msg->msgid == LOC_ENG_MSG_ATL_OPEN_FAILED)  |
             (msg->msgid == LOC_ENG_MSG_ATL_CLOSE_STATUS)  |
             (msg->msgid == LOC_ENG_MSG_ATL_OPEN_STATUS) )
        {
            loc_eng_data.agps_request_pending = false;
            if (loc_eng_data.stop_request_pending) {
                loc_eng_stop_handler();
                loc_eng_data.stop_request_pending = false;
            }
        }
        loc_eng_data.stop_request_pending = false;

        if (loc_eng_data.engine_status != GPS_STATUS_ENGINE_ON &&
            loc_eng_data.aiding_data_for_deletion != 0)
        {
            loc_eng_data.client_handle->deleteAidingData(loc_eng_data.aiding_data_for_deletion);
            loc_eng_data.aiding_data_for_deletion = 0;
        }

        delete msg;
    }

    loc_eng_data.release_wakelock_cb();
    loc_eng_data.deferred_action_thread = 0;

    EXIT_LOG(%s, VOID_RET);
}

/*===========================================================================
FUNCTION loc_eng_if_wakeup

DESCRIPTION
   For loc_eng_dmn_conn_handler.c to call to bring up or down
   network interface

DEPENDENCIES
   None

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
void loc_eng_if_wakeup(int if_req, unsigned is_supl, unsigned long ipv4_addr, unsigned char * ipv6_addr)
{
    ENTRY_LOG();

    AGpsType agps_type = is_supl? AGPS_TYPE_SUPL : AGPS_TYPE_ANY;  // No C2k?
    AGpsStatusValue status = if_req ? GPS_REQUEST_AGPS_DATA_CONN : GPS_RELEASE_AGPS_DATA_CONN;
    int tries = 3;

    while (tries > 0 && (0 == loc_eng_report_agps_status(agps_type,
                                                         status,
                                                         ipv4_addr,
                                                         ipv6_addr))) {
        tries--;
        LOC_LOGD("loc_eng_if_wakeup loc_eng not initialized, sleep for 1 second, %d more tries", tries);
        sleep(1);
    }

    if (0 == tries) {
#ifdef FEATURE_GNSS_BIT_API
        loc_eng_dmn_conn_loc_api_server_data_conn(-1);
#endif
    }

    EXIT_LOG(%s, VOID_RET);
}

// for gps.c
extern "C" const GpsInterface* get_gps_interface()
{
    return &sLocEngInterface;
}
/*===========================================================================
FUNCTION loc_eng_get_index

DESCRIPTION
   Function which is used to determine the index to access the session
   information associated with that patciular connection handle

DEPENDENCIES
   None

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_get_index(int active_conn_handle)
{
    ENTRY_LOG();
    int ret_val;

   int i = 0;
   //search through all the active sessions to determine the correct session index
    for (i=0;i < MAX_NUM_ATL_CONNECTIONS;i++)
    {
      LOC_LOGD("In loc_eng_get_index Index: %d Active: %d ConnHandle: %d\n", i, loc_eng_data.atl_conn_info[i].active,
               (int) loc_eng_data.atl_conn_info[i].conn_handle);
      if((loc_eng_data.atl_conn_info[i].active == TRUE) &&
         (loc_eng_data.atl_conn_info[i].conn_handle == active_conn_handle)) {
         ret_val = i;
            goto exit;
        }
    }
   //If we come here there is no exiting record for this connection handle and so we create a new one
    for (i=0;i < MAX_NUM_ATL_CONNECTIONS;i++)
    {
      if(loc_eng_data.atl_conn_info[i].active == FALSE)
      {
         //set session status to active
         loc_eng_data.atl_conn_info[i].active = TRUE;
         loc_eng_data.atl_conn_info[i].conn_handle = active_conn_handle;
         LOC_LOGD("In loc_eng_get_index Index: %d Active: %d ConnHandle: %d \n", i, loc_eng_data.atl_conn_info[i].active,
               (int) loc_eng_data.atl_conn_info[i].conn_handle);
         ret_val = i;
         goto exit;
      }
    }
   //If we reach this point an error has occurred
   ret_val = MAX_NUM_ATL_CONNECTIONS;

exit:
    EXIT_LOG(%d, ret_val);
    return ret_val;
}

/*===========================================================================
FUNCTION check_if_any_connection

DESCRIPTION
   Function which is used to determine if any of the other sessions have the state
   passed in

DEPENDENCIES
   None

RETURN VALUE
   TRUE/FALSE

SIDE EFFECTS
   N/A

===========================================================================*/
static boolean check_if_any_connection(loc_eng_atl_session_state_e_type conn_state,int session_index)
{
    ENTRY_LOG();
    boolean ret_val = FALSE;

    int i=0;
    for(i=0;i < MAX_NUM_ATL_CONNECTIONS; i++)
    {
        if(i == session_index)
            continue; //skip this record as we want to check all others

        if(loc_eng_data.atl_conn_info[i].active == TRUE && loc_eng_data.atl_conn_info[i].conn_state == conn_state) {
            ret_val = TRUE;
            break;
        }
    }

    EXIT_LOG(%s, boolStr[ret_val!=0]);
    return ret_val;
}

/*===========================================================================
FUNCTION check_if_all_connection

DESCRIPTION
   Function which is used to determine if all the other sessions have the state
   passed in

DEPENDENCIES
   None

RETURN VALUE
   TRUE/FALSE

SIDE EFFECTS
   N/A

===========================================================================*/
static boolean check_if_all_connection(loc_eng_atl_session_state_e_type conn_state,int session_index)
{
    ENTRY_LOG();
    boolean ret_val = TRUE;

    for(int i=0;i < MAX_NUM_ATL_CONNECTIONS; i++)
    {
        if(i == session_index)
            continue; //skip this record as we want to check all others

        if(loc_eng_data.atl_conn_info[i].conn_state != conn_state) {
            ret_val = FALSE;
            break;
        }
    }

    EXIT_LOG(%s, boolStr[ret_val!=0]);
    return ret_val;
}

/*===========================================================================
FUNCTION loc_eng_report_position_ulp

DESCRIPTION
   Report a ULP position
         p_ulp_pos_absolute, ULP position in absolute coordinates

DEPENDENCIES
   None

RETURN VALUE
   0: SUCCESS
   others: error

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_report_position_ulp (const GpsLocation* location_report_ptr,
                                         unsigned int   ext_data_length,
                                         unsigned char* ext_data)
{
  ENTRY_LOG();
  int ret_val = -1;

  if (ext_data_length > sizeof (extra_data))
  {
      ext_data_length = sizeof (extra_data);
  }

  memcpy(extra_data,
         ext_data,
         ext_data_length);

  if (loc_eng_inited == 1)
  {
      loc_eng_data.client_handle->reportPosition((GpsLocation&)*location_report_ptr, false);
      ret_val = 0;
  }

  EXIT_LOG(%d, ret_val);
  return ret_val;
}

/*===========================================================================
FUNCTION loc_eng_ulp_init

DESCRIPTION
   This function dynamically loads the libulp.so and calls
   its init function to start up the ulp module

DEPENDENCIES
   None

RETURN VALUE
   0: no error
   -1: errors

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_ulp_init()
{
    ENTRY_LOG();
    int ret_val;
    void *handle;
    const char *error;
    get_ulp_interface* get_ulp_inf;

    handle = dlopen ("libulp.so", RTLD_NOW);
    if (!handle)
    {
        LOGE ("%s, dlopen for libulp.so failed\n", __func__);
        ret_val = -1;
        goto exit;
    }
    dlerror();    /* Clear any existing error */

    get_ulp_inf = (get_ulp_interface*) dlsym(handle, "ulp_get_interface");
    if ((error = dlerror()) != NULL)  {
        LOGE ("%s, dlsym for ulpInterface failed, error = %s\n", __func__, error);
        ret_val = -1;
        goto exit;
    }

    locEngUlpInf = get_ulp_inf();

    // Initialize the ULP interface
    locEngUlpInf->init (loc_eng_report_position_ulp);

    ret_val = 0;
exit:
    EXIT_LOG(%d, ret_val);
    return ret_val;

}

/*===========================================================================
FUNCTION    loc_eng_inject_raw_command

DESCRIPTION
   This is used to send special test modem commands from the applications
   down into the HAL
DEPENDENCIES
   N/A

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
static bool loc_eng_inject_raw_command(char* command, int length)
{
    ENTRY_LOG_CALLFLOW();
    boolean ret_val;
    LOC_LOGD("loc_eng_send_extra_command: %s\n", command);
    ret_val = TRUE;

    EXIT_LOG(%s, boolStr[ret_val!=0]);
    return ret_val;
}
/*===========================================================================
FUNCTION    loc_eng_update_criteria

DESCRIPTION
   This is used to inform the ULP module of new unique criteria that are passed
   in by the applications
DEPENDENCIES
   N/A

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_update_criteria(UlpLocationCriteria criteria)
{
    ENTRY_LOG();
    int ret_val;
    ret_val = 0;

    EXIT_LOG(%d, ret_val);
    return ret_val;
}
