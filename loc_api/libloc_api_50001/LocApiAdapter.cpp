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
#define LOG_NDDEBUG 0
#define LOG_TAG "LocApiAdapter"

#include <LocApiAdapter.h>
#include "loc_eng_msg.h"
#include "loc_log.h"

LocApiAdapter::LocApiAdapter(LocEng &locEng) :
    locEngHandle(locEng)
{
}

int LocApiAdapter::hexcode(char *hexstring, int string_size,
                        const char *data, int data_size)
{
   int i;
   for (i = 0; i < data_size; i++)
   {
      char ch = data[i];
      if (i*2 + 3 <= string_size)
      {
         snprintf(&hexstring[i*2], 3, "%02X", ch);
      }
      else {
         break;
      }
   }
   return i;
}

int LocApiAdapter::decodeAddress(char *addr_string, int string_size,
                               const char *data, int data_size)
{
    const char addr_prefix = 0x91;
    int i, idxOutput = 0;

    if (!data || !addr_string) { return 0; }

    if (data[0] != addr_prefix)
    {
        LOC_LOGW("decodeAddress: address prefix is not 0x%x but 0x%x", addr_prefix, data[0]);
        addr_string[0] = '\0';
        return 0; // prefix not correct
    }

    for (i = 1; i < data_size; i++)
    {
        unsigned char ch = data[i], low = ch & 0x0F, hi = ch >> 4;
        if (low <= 9 && idxOutput < string_size - 1) { addr_string[idxOutput++] = low + '0'; }
        if (hi <= 9 && idxOutput < string_size - 1) { addr_string[idxOutput++] = hi + '0'; }
    }

    addr_string[idxOutput] = '\0'; // Terminates the string

    return idxOutput;
}

void LocApiAdapter::reportPosition(GpsLocation &location, bool intermediate)
{
    loc_eng_msg_report_position *msg(new loc_eng_msg_report_position(location, intermediate));
    locEngHandle.sendMsge(&msg);
}

void LocApiAdapter::reportSv(GpsSvStatus &svStatus)
{
    loc_eng_msg_report_sv *msg(new loc_eng_msg_report_sv(svStatus));
    locEngHandle.sendMsge(&msg);
}

void LocApiAdapter::reportStatus(GpsStatusValue status)
{
    loc_eng_msg_report_status *msg(new loc_eng_msg_report_status(status));
    locEngHandle.sendMsge(&msg);
}

void LocApiAdapter::reportNmea(const char* nmea, int length)
{
    loc_eng_msg_report_nmea *msg(new loc_eng_msg_report_nmea(nmea, length));
    locEngHandle.sendMsge(&msg);
}

void LocApiAdapter::requestATL(int connHandle, AGpsType agps_type)
{
    loc_eng_msg_request_atl *msg(new loc_eng_msg_request_atl(connHandle, agps_type));
    locEngHandle.sendMsge(&msg);
}

void LocApiAdapter::releaseATL(int connHandle)
{
    loc_eng_msg_release_atl *msg(new loc_eng_msg_release_atl(connHandle));
    locEngHandle.sendMsge(&msg);
}

void LocApiAdapter::requestXtraData()
{
    LOC_LOGD("XTRA download request");

    loc_eng_msg *msg(new loc_eng_msg(LOC_ENG_MSG_REQUEST_XTRA_DATA));
    locEngHandle.sendMsge(&msg);
}

void LocApiAdapter::requestTime()
{
    LOC_LOGD("loc_event_cb: XTRA time download request... not supported");
    // loc_eng_msg *msg(new loc_eng_msg(LOC_ENG_MSG_REQUEST_TIME));
    // locEngHandle.sendMsge(msg, sizeof(*msg));
}

void LocApiAdapter::requestLocation()
{
    LOC_LOGD("loc_event_cb: XTRA time download request... not supported");
    // loc_eng_msg *msg(new loc_eng_msg(LOC_ENG_MSG_REQUEST_POSITION));
    // locEngHandle.sendMsge(msg, sizeof(*msg));
}

void LocApiAdapter::requestNiNotify(GpsNiNotification &notif, const void* data)
{
    notif.size = sizeof(notif);
    notif.timeout     = LOC_NI_NO_RESPONSE_TIME;

    loc_eng_msg_request_ni *msg(new loc_eng_msg_request_ni(notif, data));
    locEngHandle.sendMsge(&msg);
}

void LocApiAdapter::handleEngineDownEvent()
{
    loc_eng_msg *msg(new loc_eng_msg(LOC_ENG_MSG_ENGINE_DOWN));
    locEngHandle.sendMsge(&msg);
}

void LocApiAdapter::handleEngineUpEvent()
{
    loc_eng_msg *msg(new loc_eng_msg(LOC_ENG_MSG_ENGINE_UP));
    locEngHandle.sendMsge(&msg);
}
