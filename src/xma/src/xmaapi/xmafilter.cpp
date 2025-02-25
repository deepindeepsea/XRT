/*
 * Copyright (C) 2018, Xilinx Inc - All rights reserved
 * Xilinx SDAccel Media Accelerator API
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "lib/xmaapi.h"
#include "lib/xmahw_hal.h"
//#include "lib/xmares.h"
#include "xmaplugin.h"

#define XMA_FILTER_MOD "xmafilter"

extern XmaSingleton *g_xma_singleton;

XmaFilterSession*
xma_filter_session_create(XmaFilterProperties *filter_props)
{
    XmaFilterSession *filter_session = (XmaFilterSession*) malloc(sizeof(XmaFilterSession));
    if (filter_session == NULL) {
        return NULL;
    }
	//XmaResources xma_shm_cfg = g_xma_singleton->shm_res_cfg;
    //XmaKernelRes kern_res;

    xma_logmsg(XMA_DEBUG_LOG, XMA_FILTER_MOD, "%s()\n", __func__);
    /*Sarab: Remove xma_res stuff
	if (!xma_shm_cfg) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "No reference to xma res database\n");
        free(filter_session);
		return NULL;
    }
    */

    // Load the xmaplugin library as it is a dependency for all plugins
    void *xmahandle = dlopen("libxmaplugin.so",
                             RTLD_LAZY | RTLD_GLOBAL);
    if (!xmahandle)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "Failed to open plugin xmaplugin.so. Error msg: %s\n",
                   dlerror());
        return NULL;
    }
    void *handle = dlopen(filter_props->plugin_lib, RTLD_NOW);
    if (!handle)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
            "Failed to open plugin %s\n Error msg: %s\n",
            filter_props->plugin_lib, dlerror());
        return NULL;
    }

    XmaFilterPlugin *plg =
        (XmaFilterPlugin*)dlsym(handle, "filter_plugin");
    char *error;
    if ((error = dlerror()) != NULL)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
            "Failed to get filterer_plugin from %s\n Error msg: %s\n",
            filter_props->plugin_lib, dlerror());
        return NULL;
    }


    memset(filter_session, 0, sizeof(XmaFilterSession));
    // init session data
    filter_session->props = *filter_props;
    filter_session->base.channel_id = filter_props->channel_id;
    filter_session->base.session_type = XMA_FILTER;
    filter_session->filter_plugin = plg;

    /*Sarab: Remove xma_res stuff
    // Just assume this is an ABR filter for now and that the FPGA
    // has been downloaded.  This is accomplished by getting the
    // first device (dev_handle, base_addr, ddr_bank) and making a
    // XmaHwSession out of it.  Later this needs to be done by searching
    // for an available resource.
	/--* JPM TODO default to exclusive access.  Ensure multiple threads
	   can access this device if in-use pid = requesting thread pid *--/
    rc = xma_res_alloc_filter_kernel(xma_shm_cfg,
                                     filter_props->hwfilter_type,
                                     filter_props->hwvendor_string,
		                             &filter_session->base, false);
    if (rc) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "Failed to allocate free filter kernel. Return code %d\n", rc);
        free(filter_session);
        return NULL;
    }

    kern_res = filter_session->base.kern_res;

    dev_handle = xma_res_dev_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_FILTER_MOD,"dev_handle = %d\n", dev_handle);
    if (dev_handle < 0) {
        free(filter_session);
        return NULL;
    }

    kern_handle = xma_res_kern_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_FILTER_MOD,"kern_handle = %d\n", kern_handle);
    if (kern_handle < 0) {
        free(filter_session);
        return NULL;
    }

    filter_handle = xma_res_plugin_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_FILTER_MOD,"filter_handle = %d\n",
               filter_handle);
    if (filter_handle < 0) 
    {
        free(filter_session);
        return NULL;
    }
    */

    bool expected = false;
    bool desired = true;
    while (!(g_xma_singleton->locked).compare_exchange_weak(expected, desired)) {
        expected = false;
    }
    //Singleton lock acquired

   //Sarab: TODO Fix device index, CU index & session->xx_plugin assigned above
    int rc, dev_index, cu_index;
    dev_index = filter_props->dev_index;
    cu_index = filter_props->cu_index;
    //filter_handle = filter_props->cu_index;

    g_xma_singleton->num_filters++;

    XmaHwCfg *hwcfg = &g_xma_singleton->hwcfg;
    XmaHwHAL *hal = (XmaHwHAL*)hwcfg->devices[dev_index].handle;
    filter_session->base.hw_session.dev_handle = hal->dev_handle;

    //For execbo:
    filter_session->base.hw_session.kernel_info = &hwcfg->devices[dev_index].kernels[cu_index];

    filter_session->base.hw_session.dev_index = hal->dev_index;

    // Assume it is the first filter plugin for now
    //filter_session->filter_plugin = &g_xma_singleton->filtercfg[filter_handle];

    // Allocate the private data
    filter_session->base.plugin_data =
        calloc(filter_session->filter_plugin->plugin_data_size, sizeof(uint8_t));

    /*Sarab: Remove xma_connect stuff
    XmaEndpoint *end_pt = (XmaEndpoint*) malloc(sizeof(XmaEndpoint));
    end_pt->session = &filter_session->base;
    end_pt->dev_id = dev_handle;
    end_pt->format = filter_props->output.format;
    end_pt->bits_per_pixel = filter_props->output.bits_per_pixel;
    end_pt->width = filter_props->output.width;
    end_pt->height = filter_props->output.height;
    filter_session->conn_send_handle =
        xma_connect_alloc(end_pt, XMA_CONNECT_SENDER);

    // TODO: fix to use allocate handle making sure that
    //       we don't connect to ourselves
    filter_session->conn_recv_handle = -1;
    */

    filter_session->base.session_id = g_xma_singleton->num_filters;
    filter_session->base.session_signature = (void*)(((uint64_t)filter_session->base.hw_session.kernel_info) | ((uint64_t)filter_session->base.hw_session.dev_handle));

    //Release singleton lock
    g_xma_singleton->locked = false;

    // Call the plugins initialization function with this session data
    //Sarab: Check plugin compatibility to XMA
    int32_t xma_main_ver = -1;
    int32_t xma_sub_ver = -1;
    rc = filter_session->filter_plugin->xma_version(&xma_main_ver, & xma_sub_ver);
    //Sarab: TODO. Check version match. Stop here for now
    //Sarab: Remove it later on
    return NULL;

    rc = filter_session->filter_plugin->init(filter_session);
    if (rc) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "Initalization of filter plugin failed. Return code %d\n",
                   rc);
        free(filter_session->base.plugin_data);
        //xma_connect_free(filter_session->conn_send_handle, XMA_CONNECT_SENDER);
        free(filter_session);
        return NULL;
    }

    return filter_session;
}

int32_t
xma_filter_session_destroy(XmaFilterSession *session)
{
    int32_t rc;

    xma_logmsg(XMA_DEBUG_LOG, XMA_FILTER_MOD, "%s()\n", __func__);
    rc  = session->filter_plugin->close(session);
    if (rc != 0)
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "Error closing filter plugin\n");

    // Clean up the private data
    free(session->base.plugin_data);

    /*Sarab: Remove xma_connect stuff
    // Free each sender connection
    xma_connect_free(session->conn_send_handle, XMA_CONNECT_SENDER);

    // Free the receiver connection
    xma_connect_free(session->conn_recv_handle, XMA_CONNECT_RECEIVER);
    */

    /* Remove xma_res stuff free kernel/kernel-session *--/
    rc = xma_res_free_kernel(g_xma_singleton->shm_res_cfg,
                             session->base.kern_res);
    if (rc)
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "Error freeing filter session. Return code %d\n", rc);
    */
    // Free the session
    // TODO: (should also free the Hw sessions)
    free(session);

    return XMA_SUCCESS;
}

int32_t
xma_filter_session_send_frame(XmaFilterSession  *session,
                              XmaFrame          *frame)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_FILTER_MOD, "%s()\n", __func__);
    /*Sarab: Remove zerocopy stuff
    if (session->conn_send_handle != -1)
    {
        // Get the connection entry to find the receiver
        int32_t c_handle = session->conn_send_handle;
        XmaConnect *conn = &g_xma_singleton->connections[c_handle];
        XmaEndpoint *recv = conn->receiver;
        if (recv)
        {
            if (is_xma_encoder(recv->session))
            {
                XmaEncoderSession *e_ses = to_xma_encoder(recv->session);
                if (!e_ses->encoder_plugin->get_dev_input_paddr) {
                    xma_logmsg(XMA_DEBUG_LOG, XMA_FILTER_MOD,
                        "encoder plugin does not support zero copy\n");
                    goto send;
		}
                session->out_dev_addr = e_ses->encoder_plugin->get_dev_input_paddr(e_ses);
                session->zerocopy_dest = true;
            }
        }
    }
send:
    */
    return session->filter_plugin->send_frame(session, frame);
}

int32_t
xma_filter_session_recv_frame(XmaFilterSession  *session,
                              XmaFrame          *frame)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_FILTER_MOD, "%s()\n", __func__);
    return session->filter_plugin->recv_frame(session, frame);
}
