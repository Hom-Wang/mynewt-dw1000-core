/*
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
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <os/os.h>
#include <hal/hal_spi.h>
#include <hal/hal_gpio.h>
#include "bsp/bsp.h"


#include <dw1000/dw1000_regs.h>
#include <dw1000/dw1000_dev.h>
#include <dw1000/dw1000_hal.h>
#include <dw1000/dw1000_mac.h>
#include <dw1000/dw1000_phy.h>
#include <dw1000/dw1000_ftypes.h>

#if MYNEWT_VAL(DW1000_LWIP_P2P)
#include <dw1000/dw1000_lwip.h>
#include <dw1000/dw1000_lwip_p2p.h>
#include <lwip/netif.h>

//static uint16_t node_addr[] = {0x6002, 0x6003};

static void lwip_p2p_postprocess(struct os_event * ev);
static void lwip_p2p_complete_cb(dw1000_dev_instance_t * inst);
static struct os_callout lwip_p2p_callout_timer;
static struct os_callout lwip_p2p_callout_postprocess;

static void rx_complete_cb(dw1000_dev_instance_t * inst);
static void tx_complete_cb(dw1000_dev_instance_t * inst);
static void rx_timeout_cb(dw1000_dev_instance_t * inst);
static void rx_error_cb(dw1000_dev_instance_t * inst);

static void
lwip_p2p_timer_ev_cb(struct os_event *ev) {
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);

    //dw1000_lwip_p2p_request(inst, <dst_address>, <LWIP_p2p_CODE>);
    os_callout_reset(&lwip_p2p_callout_timer, OS_TICKS_PER_SEC/32 );
}

static void
lwip_p2p_timer_init(dw1000_dev_instance_t *inst) {
    os_callout_init(&lwip_p2p_callout_timer, os_eventq_dflt_get(), lwip_p2p_timer_ev_cb, (void *) inst);
    os_callout_reset(&lwip_p2p_callout_timer, OS_TICKS_PER_SEC/100);
    dw1000_lwip_p2p_instance_t * lwip_p2p = inst->lwip_p2p;
    lwip_p2p->status.timer_enabled = true;
}

static void lwip_p2p_complete_cb(dw1000_dev_instance_t *inst){
    dw1000_lwip_p2p_instance_t *lwip_p2p = inst->lwip_p2p;

    if(lwip_p2p->config.postprocess)
        os_eventq_put(os_eventq_dflt_get(), &lwip_p2p_callout_postprocess.c_ev);
}

static void lwip_p2p_postprocess(struct os_event * ev){
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);

    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *)ev->ev_arg;
    //dw1000_lwip_p2p_instance_t * lwip_p2p = inst->lwip_p2p;
    //dw1000_lwip_instance_t * lwip = inst->lwip;
    //dw1000_lwip_rng_instance_t * lwip_rng = inst->lwip_rng;
    char * data_buf = (char *)malloc(80);
    assert(data_buf);
    for (int i = 0; i < 80; ++i)
        *(data_buf+i) = *(inst->lwip->data_buf[0]+i+4);

    inst->lwip->netif->input((struct pbuf *)data_buf, inst->lwip->netif);
    return;

    /* TODO : Implement postprocess code here */

    /* CODE */

}

dw1000_lwip_p2p_instance_t *
dw1000_lwip_p2p_init(dw1000_dev_instance_t * inst, uint16_t nnodes){
    assert(inst);

    if (inst->lwip_p2p == NULL ) {
        inst->lwip_p2p = (dw1000_lwip_p2p_instance_t *) malloc(sizeof(dw1000_lwip_p2p_instance_t) + ((nnodes-1)*(nnodes-2)/2) * sizeof(node_ranges_t));
        assert(inst->lwip_p2p);
        memset(inst->lwip_p2p, 0, sizeof(dw1000_lwip_p2p_instance_t) + ((nnodes-1)*(nnodes-2)/2) * sizeof(node_ranges_t));
        inst->lwip_p2p->status.selfmalloc = 1;
        inst->lwip_p2p->nnodes = nnodes;
    }else{
        assert(inst->lwip_p2p->nnodes == nnodes);
    }
    inst->lwip_p2p->parent = inst;
    inst->lwip_p2p->config = (dw1000_lwip_p2p_config_t){
        .postprocess = false,
    };

    dw1000_lwip_p2p_set_callbacks(inst, lwip_p2p_complete_cb, tx_complete_cb, rx_complete_cb, rx_timeout_cb, rx_error_cb);
    dw1000_lwip_p2p_set_postprocess(inst, &lwip_p2p_postprocess);
    inst->lwip_p2p->status.initialized = 1;
    return inst->lwip_p2p;
}

void
dw1000_lwip_p2p_free(dw1000_lwip_p2p_instance_t * inst){
    assert(inst);
    if (inst->status.selfmalloc)
        free(inst);
    else
        inst->status.initialized = 0;
}

void dw1000_lwip_p2p_set_callbacks(dw1000_dev_instance_t * inst, dw1000_dev_cb_t lwip_range_cb, 
                                    dw1000_dev_cb_t tx_complete_cb, dw1000_dev_cb_t rx_complete_cb, 
                                    dw1000_dev_cb_t rx_timeout_cb, dw1000_dev_cb_t rx_error_cb){

    inst->lwip_p2p_complete_cb = lwip_range_cb;
    inst->tx_complete_cb = tx_complete_cb;
    inst->rx_complete_cb = rx_complete_cb;
    inst->rx_timeout_cb = rx_timeout_cb;
    inst->rx_error_cb = rx_error_cb;
}

void
dw1000_lwip_p2p_set_postprocess(dw1000_dev_instance_t * inst, os_event_fn * lwip_p2p_postprocess){
    os_callout_init(&lwip_p2p_callout_postprocess, os_eventq_dflt_get(), lwip_p2p_postprocess, (void *) inst);
    dw1000_lwip_p2p_instance_t * lwip_p2p = inst->lwip_p2p;
    lwip_p2p->config.postprocess = true;
}


void
dw1000_lwip_p2p_start(dw1000_dev_instance_t * inst){
    // Initialise frame timestamp to current time
    dw1000_lwip_p2p_instance_t * lwip_p2p = inst->lwip_p2p;
    lwip_p2p->idx = 0x0;
    lwip_p2p->status.valid = false;
    lwip_p2p_timer_init(inst);
}

void
dw1000_lwip_p2p_stop(dw1000_dev_instance_t * inst){
    os_callout_stop(&lwip_p2p_callout_timer);
}

static void 
rx_complete_cb(dw1000_dev_instance_t * inst){

    printf("%s\n", __func__);
    uint16_t buf_idx = (inst->lwip->buf_idx++) % inst->lwip->nframes;
    char *data_buf = inst->lwip->data_buf[buf_idx];

    dw1000_read_rx(inst, (uint8_t *) data_buf, 0, inst->lwip->buf_len);
    #if 0
    for (int i = 0; i < 4; ++i)
        printf("[%c] ", (char)(*(data_buf+i)));
    printf(" \n");
    #endif

    if((*(data_buf+0) == 'L') && (*(data_buf+1) == 'W') && (*(data_buf+2) == 'I') && (*(data_buf+3) == 'P')){
        inst->lwip_p2p->status.start_rng_req = 1;
        inst->lwip_p2p_complete_cb(inst);
        dw1000_set_rx_timeout(inst, 0);
        dw1000_start_rx(inst);
        return;
    }

    os_error_t err = os_sem_release(&inst->lwip->data_sem);
    assert(err == OS_OK);

}

static void 
tx_complete_cb(dw1000_dev_instance_t * inst){

    printf("%s\n", __func__);
    #if 0
    if(inst->lwip_p2p->status.start_rng_req == 1){
        dw1000_set_rx_timeout(inst, 0xFFFF);
        dw1000_start_rx(inst);
    }
    #endif
    os_error_t err = os_sem_release(&inst->lwip->sem);
    assert(err == OS_OK);
}


static void 
rx_timeout_cb(dw1000_dev_instance_t * inst){

    printf("%s\n", __func__);
    os_error_t err = os_sem_release(&inst->lwip->data_sem);
    assert(err == OS_OK);

    inst->lwip->status.rx_timeout_error = 1;
}


void
rx_error_cb(dw1000_dev_instance_t * inst){

    printf("%s\n", __func__);
    os_error_t err = os_sem_release(&inst->lwip->data_sem);
    assert(err == OS_OK);

    inst->lwip->status.rx_error = 1;
}

#endif //MYNEWT_VAL(DW1000_LWIP_p2p)