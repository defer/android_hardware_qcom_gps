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
#include <errno.h>
#include <math.h>
#include <pthread.h>

#include <loc_eng.h>
#include <loc_eng_msg.h>
#include <loc_eng_msg_id.h>

#include "log_util.h"

// comment this out to enable logging
// #undef LOGD
// #define LOGD(...) {}

static int qct_loc_eng_xtra_init (GpsXtraCallbacks* callbacks);
static int qct_loc_eng_inject_xtra_data_proxy(char* data, int length);

const GpsXtraInterface sLocEngXTRAInterface =
{
    sizeof(GpsXtraInterface),
    qct_loc_eng_xtra_init,
    qct_loc_eng_inject_xtra_data_proxy
};

/*===========================================================================
FUNCTION    qct_loc_eng_xtra_init

DESCRIPTION
   Initialize XTRA module.

DEPENDENCIES
   N/A

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
static int qct_loc_eng_xtra_init (GpsXtraCallbacks* callbacks)
{
   loc_eng_xtra_data_s_type *xtra_module_data_ptr;

   xtra_module_data_ptr = &loc_eng_data.xtra_module_data;
   xtra_module_data_ptr->download_request_cb = callbacks->download_request_cb;

   return 0;
}

/*===========================================================================
FUNCTION    qct_loc_eng_inject_xtra_data_proxy

DESCRIPTION
   Injects XTRA file into the engine but buffers the data if engine is busy.

DEPENDENCIES
   N/A

RETURN VALUE
   0: success
   >0: failure

SIDE EFFECTS
   N/A

===========================================================================*/
static int qct_loc_eng_inject_xtra_data_proxy(char* data, int length)
{
    loc_eng_msg_inject_xtra_data *msg(new loc_eng_msg_inject_xtra_data(data, length));
    loc_eng_msg_sender(msg);

    return 0;
}
