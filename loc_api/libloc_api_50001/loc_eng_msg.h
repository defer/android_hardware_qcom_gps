/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#ifndef LOC_ENG_MSG_H
#define LOC_ENG_MSG_H

// #ifdef __cplusplus
// extern "C" {
// #endif /* __cplusplus */

#include <stdlib.h>
#include <rpc/rpc.h>
#include "loc_api_rpc_glue.h"
#include "loc_eng.h"
#include "loc_eng_msg_id.h"

struct loc_eng_msg_quit {
    unsigned int msgsz;
    int msgid;
};

struct loc_eng_msg_loc_event {
    unsigned int msgsz;
    int msgid;
    rpc_loc_client_handle_type client_handle;
    rpc_loc_event_mask_type loc_event;
    struct rpc_loc_event_payload_u_type loc_event_payload;
};

struct loc_eng_msg_modem_restart {
    unsigned int msgsz;
    int msgid;
};

struct loc_eng_msg_start_fix {
    unsigned int msgsz;
    int msgid;
};

struct loc_eng_msg_stop_fix {
    unsigned int msgsz;
    int msgid;
};

struct loc_eng_msg_delete_aiding_data {
    unsigned int msgsz;
    int msgid;
    GpsAidingData type;
};

struct loc_eng_msg_update_network_availability {
    unsigned int msgsz;
    int msgid;
    int available;
    char apn_name[101];
};

struct loc_eng_msg_inject_xtra_data {
    unsigned int msgsz;
    int msgid;
    int xtra_data_len;
    char * xtra_data_for_injection;
};

struct loc_eng_msg_ioctl{
    unsigned int msgsz;
    int msgid;
    rpc_loc_client_handle_type    client_handle;
    rpc_loc_ioctl_e_type          ioctl_type;
    rpc_loc_ioctl_data_u_type     ioctl_data;
    uint32                        timeout_msec;
    rpc_loc_ioctl_callback_s_type *cb_data_ptr;
};

struct loc_eng_msg_mute_session{
    unsigned int msgsz;
    int msgid;
};

struct loc_eng_msg_agps_open_status {
    unsigned int msgsz;
    int msgid;
    AGpsBearerType bearerType;
    char apn_name[101];
};

struct loc_eng_msg_agps_close_status {
    unsigned int msgsz;
    int msgid;
};

struct loc_eng_msg_agps_failed {
    unsigned int msgsz;
    int msgid;
};

/* purpose of this union is to know the maximum memory size needed for msg */
union loc_eng_msg {
    struct msgbuf msg;

    struct loc_eng_msg_quit msg_quit;

    struct loc_eng_msg_loc_event loc_event;
    struct loc_eng_msg_modem_restart modem_restart;

    struct loc_eng_msg_start_fix start_fix;
    struct loc_eng_msg_stop_fix stop_fix;
    struct loc_eng_msg_delete_aiding_data delete_aiding_data;
    struct loc_eng_msg_inject_xtra_data inject_xtra_data;

    struct loc_eng_msg_update_network_availability update_network_availability;

    struct loc_eng_msg_ioctl ioctl;
    struct loc_eng_msg_mute_session mute_session;

    struct loc_eng_msg_agps_open_status agps_open_status;
    struct loc_eng_msg_agps_close_status agps_close_status;
    struct loc_eng_msg_agps_failed agps_failed;
};

int loc_eng_msgget(int * p_req_msgq);
int loc_eng_msgremove(int req_msgq);
int loc_eng_msgsnd(int msgqid, void * msgp, unsigned int msgsz);
int loc_eng_msgrcv(int msgqid, void * msgp, unsigned int msgsz);
int loc_eng_msgflush(int msgqid);
int loc_eng_msgunblock(int msgqid);

// #ifdef __cplusplus
// }
// #endif /* __cplusplus */

#endif /* LOC_ENG_MSG_H */
