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
#ifndef LOC_ENG_MSG_ID_H
#define LOC_ENG_MSG_ID_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct msgbuf {
    unsigned int msgsz;
    int msgid;
};

enum {
    /* 0x 0 - 0xEF is reserved for daemon internal */
    /* 0xF0 - 0x1FF is reserved for daemon & framework communication */
    LOC_ENG_MSG_QUIT = 0x200,

    LOC_ENG_MSG_LOC_EVENT,
    LOC_ENG_MSG_MODEM_DOWN,
    LOC_ENG_MSG_MODEM_UP,

    LOC_ENG_MSG_START_FIX,
    LOC_ENG_MSG_STOP_FIX,
    LOC_ENG_MSG_INJECT_XTRA_DATA,
    LOC_ENG_MSG_DELETE_AIDING_DATA,

    LOC_ENG_MSG_UPDATE_NETWORK_AVAILABILITY,

    LOC_ENG_MSG_IOCTL,
    LOC_ENG_MSG_MUTE_SESSION,

    LOC_ENG_MSG_AGPS_DATA_OPEN_STATUS,
    LOC_ENG_MSG_AGPS_DATA_CLOSE_STATUS,
    LOC_ENG_MSG_AGPS_DATA_FAILED,
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* LOC_ENG_MSG_ID_H */
