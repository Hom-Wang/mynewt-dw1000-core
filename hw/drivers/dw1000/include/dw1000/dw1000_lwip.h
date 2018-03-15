/**
 * Copyright 2018, Decawave Limited, All Rights Reserved
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef _DW1000_LWIP_H_
#define _DW1000_LWIP_H_

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <hal/hal_spi.h>
#include <dw1000/dw1000_regs.h>
#include <dw1000/dw1000_dev.h>
#include <dw1000/dw1000_ftypes.h>
#include <lwip/pbuf.h>

typedef struct _dw1000_lwip_config_t{
   uint16_t poll_resp_delay;    // Delay between frames, in UWB microseconds.
   uint16_t resp_timeout;       // Receive response timeout, in UWB microseconds.
   uint32_t uwbtime_to_systime;
}dw1000_lwip_config_t;

typedef enum _dw1000_lwip_modes_t{
    LWIP_BLOCKING,
    LWIP_NONBLOCKING
}dw1000_lwip_modes_t;

typedef struct _dw1000_lwip_status_t{
    uint16_t selfmalloc:1;
    uint16_t initialized:1;
}dw1000_lwip_status_t;

typedef struct _dw1000_lwip_instance_t{
    struct _dw1000_dev_instance_t * dev;
    struct os_sem sem;
    test_frame_t * tx_frame;
    test_frame_t * rx_frame;
    dw1000_lwip_config_t * config;
    dw1000_lwip_status_t status;
    struct netif * netif;
}dw1000_lwip_instance_t;

dw1000_lwip_config_t * dw1000_config(dw1000_dev_instance_t * inst);

dw1000_lwip_instance_t * dw1000_lwip_init(dw1000_dev_instance_t * inst, dw1000_lwip_config_t * config, bool ping_status);

void dw1000_lwip_free(dw1000_lwip_instance_t * inst);

#if 0
void dw1000_lwip_set_callbacks(dw1000_lwip_instance_t * inst, dw1000_dev_cb_t lwip_tx_complete_cb, dw1000_dev_cb_t lwip_rx_complete_cb,  dw1000_dev_cb_t lwip_timeout_cb,  dw1000_dev_cb_t lwip_error_cb);
#else
void dw1000_lwip_set_callbacks(dw1000_dev_instance_t * inst, dw1000_dev_cb_t lwip_tx_complete_cb, dw1000_dev_cb_t lwip_rx_complete_cb,  dw1000_dev_cb_t lwip_timeout_cb,  dw1000_dev_cb_t lwip_error_cb);
#endif

dw1000_lwip_status_t dw1000_lwip_write(dw1000_lwip_instance_t * inst, dw1000_lwip_config_t * config, dw1000_lwip_modes_t mode);
void dw1000_lwip_set_frames(dw1000_dev_instance_t * inst, test_frame_t *tx_frame);
dw1000_dev_status_t dw1000_lwip_send(dw1000_dev_instance_t * inst, uint16_t dst_address, dw1000_lwip_modes_t code);

dw1000_dev_status_t dw1000_lwip_ping_send(dw1000_dev_instance_t * inst, struct pbuf *p, dw1000_lwip_modes_t code);

#ifdef __cplusplus
}
#endif
#endif /* _DW1000_LWIP_H_ */
