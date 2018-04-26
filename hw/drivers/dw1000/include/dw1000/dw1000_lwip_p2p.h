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

#ifndef _DW1000_LWIP_P2P_H_
#define _DW1000_LWIP_P2P_H_

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <hal/hal_spi.h>
#include <dw1000/dw1000_regs.h>
#include <dw1000/dw1000_dev.h>
#include <dw1000/dw1000_ftypes.h>

/*
 * LWIP p2p modes
 */
typedef enum _dw1000_lwip_p2p_modes_t{
    LWIP_P2P_BLOCKING,
    LWIP_P2P_NONBLOCKING
}dw1000_lwip_p2p_modes_t;

/*
 * LWIP p2p config structure
 */
typedef struct _dw1000_lwip_p2p_config_t{
    uint16_t postprocess:1;
}dw1000_lwip_p2p_config_t;

/*
 * LWIP p2p status Structure
 */
typedef struct _dw1000_lwip_p2p_status_t{
    uint16_t selfmalloc:1;
    uint16_t initialized:1;
    uint16_t valid:1;
    uint16_t start_tx_error:1;
    uint16_t rx_timeout_error:1;
    uint16_t rx_error:1;
    uint16_t request_timeout_error:1;
    uint16_t timer_enabled:1;
    uint16_t start_rng_req:1;
    uint16_t rng_req_rsp:1;
}dw1000_lwip_p2p_status_t;

/*
 * Structure to be used for storing p2p value b/w
 * node1 and node 2
 */
typedef struct _node_ranges_t{
    uint16_t node1_short_addr;
    uint16_t node2_short_addr;
    float range_val;
}node_ranges_t;

/*
 * Lwip p2p instance Structure
 */
typedef struct _dw1000_lwip_p2p_instance_t{
    struct _dw1000_dev_instance_t * parent;
    dw1000_lwip_p2p_status_t status;
    dw1000_lwip_p2p_config_t config;
    uint8_t idx;
    uint16_t nnodes;
    node_ranges_t node_ranges[];
}dw1000_lwip_p2p_instance_t;

/*
 * Structure of frame to be used for lwip p2p service
 */
typedef struct _lwip_twr_frame_t{
    uint8_t code;
    uint16_t master_addr;   // node0_address
    /* The two nodes between which lwip will initiate p2p. */
    uint16_t node1_addr;
    uint16_t node2_addr;
    float range;
}lwip_twr_frame_t;

/**
 * [dw1000_lwip_p2p_init description]
 * Function to initialize and allocate memory for lwip p2p service.
 * @param  inst   [Device instance]
 * @param  nnodes [Number of nodes]
 * @return        [Return lwip p2p instance]
 */
dw1000_lwip_p2p_instance_t * dw1000_lwip_p2p_init(dw1000_dev_instance_t * inst, uint16_t nnodes);

/**
 * [dw1000_lwip_p2p_set_frames description]
 * Set the lwip frame/payload to be sent for initializing p2p of two nodes.
 * @param inst [Device instance]
 * @param twr  [Prepared frame/payload]
 */
void dw1000_lwip_p2p_set_frames(dw1000_dev_instance_t * inst, lwip_twr_frame_t twr[]);

/**
 * [dw1000_lwip_p2p_free description]
 * Free the memory allocated for lwip p2p service/instance
 * @param inst [Device instance]
 */
void dw1000_lwip_p2p_free(dw1000_lwip_p2p_instance_t * inst);

/**
 * [dw1000_lwip_p2p_set_callbacks description]
 * Function for setting the callbacks
 * @param inst                 [Device instance]
 * @param dw1000_lwip_p2p_cb [Callback function]
 */
void dw1000_lwip_p2p_set_callbacks(dw1000_dev_instance_t * inst,dw1000_dev_cb_t dw1000_lwip_p2p_cb,
                                    dw1000_dev_cb_t tx_complete_cb, dw1000_dev_cb_t rx_complete_cb, 
                                    dw1000_dev_cb_t rx_timeout_cb, dw1000_dev_cb_t rx_error_cb);

/**
 * [dw1000_lwip_p2p_set_postprocess description]
 * Set the post-process callback function
 * @param inst                     [Device instance]
 * @param lwip_p2p_postprocess [Callback function]
 */
void dw1000_lwip_p2p_set_postprocess(dw1000_dev_instance_t * inst, os_event_fn * lwip_p2p_postprocess);

/**
 * [dw1000_lwip_p2p_start description]
 * Start the lwip p2p service
 * @param inst [Device instance]
 */
void dw1000_lwip_p2p_start(dw1000_dev_instance_t * inst);

/**
 * [dw1000_lwip_p2p_stop description]
 * Stop the lwip p2p service
 * @param inst [Device instance]
 */
void dw1000_lwip_p2p_stop(dw1000_dev_instance_t * inst);


#ifdef __cplusplus
}
#endif
#endif /* _DW1000_LWIP_p2p_H_ */
