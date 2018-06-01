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
#include <dw1000/dw1000_rng.h>

#if MYNEWT_VAL(DW1000_LWIP_P2P)
#include <dw1000/dw1000_lwip_p2p.h>

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

    os_callout_reset(&lwip_p2p_callout_timer, OS_TICKS_PER_SEC/2);
    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *)ev->ev_arg;
    
    uint8_t idx=0;

    #if 0
    dw1000_lwip_p2p_instance_t *lwip_p2p = inst->lwip_p2p;
    
    assert(lwip_p2p->lwip_p2p_buf != NULL);

    ip_addr_t ip6_tgt_addr[LWIP_IPV6_NUM_ADDRESSES];

    IP_ADDR6(ip6_tgt_addr, MYNEWT_VAL(TGT_IP6_ADDR_1), MYNEWT_VAL(TGT_IP6_ADDR_2), 
                            MYNEWT_VAL(TGT_IP6_ADDR_3), MYNEWT_VAL(TGT_IP6_ADDR_4));
    dw1000_lwip_p2p_send(lwip_p2p->p2p_pcb, lwip_p2p->lwip_p2p_buf, ip6_tgt_addr);
    raw_sendto(lwip_p2p->p2p_pcb, lwip_p2p->lwip_p2p_buf, ip6_tgt_addr);
    #endif

    dw1000_lwip_p2p_send(inst, idx);
    printf("[PS]\n");
    dw1000_lwip_start_rx(inst, 0xFFFF);
}

static void
lwip_p2p_timer_init(dw1000_dev_instance_t *inst) {

    os_callout_init(&lwip_p2p_callout_timer, os_eventq_dflt_get(), lwip_p2p_timer_ev_cb, (void *) inst);
    os_callout_reset(&lwip_p2p_callout_timer, OS_TICKS_PER_SEC/4);
    dw1000_lwip_p2p_instance_t * lwip_p2p = inst->lwip_p2p;
    lwip_p2p->status.timer_enabled = true;
}

static void 
lwip_p2p_complete_cb(dw1000_dev_instance_t *inst){

    dw1000_lwip_p2p_instance_t *lwip_p2p = inst->lwip_p2p;
    if(lwip_p2p->config.postprocess)
        os_eventq_put(os_eventq_dflt_get(), &lwip_p2p_callout_postprocess.c_ev);
}

static void 
lwip_p2p_postprocess(struct os_event * ev){

    assert(ev != NULL);
    assert(ev->ev_arg != NULL);
}

dw1000_lwip_p2p_instance_t *
dw1000_lwip_p2p_init(dw1000_dev_instance_t * inst, uint16_t nnodes, dw1000_lwip_p2p_payload_info_t payload_info[]){
                        
    assert(inst);

    if (inst->lwip_p2p == NULL ) {
        inst->lwip_p2p = (dw1000_lwip_p2p_instance_t *)malloc(sizeof(dw1000_lwip_p2p_instance_t) + 
                            nnodes * (sizeof(dw1000_lwip_p2p_payload_info_t *)));
        assert(inst->lwip_p2p);
        memset(inst->lwip_p2p, 0, sizeof(dw1000_lwip_p2p_instance_t));
        inst->lwip_p2p->status.selfmalloc = 1;
        inst->lwip_p2p->nnodes = nnodes;
    }
    else
        assert(inst->lwip_p2p->nnodes == nnodes);

    dw1000_lwip_p2p_set_frames(inst, payload_info);

    inst->lwip_p2p->parent = inst;
    inst->lwip_p2p->config = (dw1000_lwip_p2p_config_t){
        .postprocess = false,
    };

    dw1000_lwip_p2p_set_callbacks(inst, lwip_p2p_complete_cb, tx_complete_cb, rx_complete_cb, rx_timeout_cb, rx_error_cb);
    dw1000_lwip_p2p_set_postprocess(inst, &lwip_p2p_postprocess);
    inst->lwip_p2p->status.initialized = 1;
    return inst->lwip_p2p;
}

#if 0
uint8_t
dw1000_lwip_p2p_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr){

    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *)arg;
    LWIP_UNUSED_ARG(pcb);
    LWIP_UNUSED_ARG(addr);
    LWIP_ASSERT("p != NULL", p != NULL);
    inst->lwip_p2p->lwip_p2p_buf = p;
    if (pbuf_header( p, -PBUF_IP_HLEN)==0)
        inst->lwip_p2p_complete_cb(inst);
    memp_free(MEMP_PBUF_POOL,p);
    return 1;
}
#endif

void dw1000_lwip_p2p_send(dw1000_dev_instance_t * inst, uint8_t idx){

    dw1000_lwip_send(inst, idx);
}

inline void
dw1000_lwip_p2p_set_frames(dw1000_dev_instance_t * inst, dw1000_lwip_p2p_payload_info_t payload_info[]){

    uint16_t nnodes = inst->lwip_p2p->nnodes;

    for(uint16_t i=0 ; i < nnodes ; i++){
        inst->lwip_p2p->payload_info[i]  = &payload_info[i];
    }

    #if 0
    if(inst->lwip_p2p->lwip_p2p_buf == NULL){
        struct pbuf *pb = pbuf_alloc(PBUF_RAW, (u16_t)payload_size, PBUF_RAM);
        assert( pb != NULL);
        inst->lwip_p2p->lwip_p2p_buf = pb;
    }
    #endif
}

void
dw1000_lwip_p2p_free(dw1000_lwip_p2p_instance_t * inst){

    assert(inst);
    if (inst->status.selfmalloc)
        free(inst);
    else
        inst->status.initialized = 0;
}

void 
dw1000_lwip_p2p_set_callbacks(dw1000_dev_instance_t * inst, dw1000_dev_cb_t lwip_p2p_complete_cb, 
                                    dw1000_dev_cb_t tx_complete_cb, dw1000_dev_cb_t rx_complete_cb, 
                                    dw1000_dev_cb_t rx_timeout_cb, dw1000_dev_cb_t rx_error_cb){

    inst->lwip_p2p_complete_cb      = lwip_p2p_complete_cb;
    inst->lwip_p2p_tx_complete_cb   = tx_complete_cb;
    inst->lwip_p2p_rx_complete_cb   = rx_complete_cb;
    inst->lwip_p2p_rx_timeout_cb    = rx_timeout_cb;
    inst->lwip_p2p_rx_error_cb      = rx_error_cb;
}

void
dw1000_lwip_p2p_set_postprocess(dw1000_dev_instance_t * inst, os_event_fn * lwip_p2p_postprocess){

    os_callout_init(&lwip_p2p_callout_postprocess, os_eventq_dflt_get(), lwip_p2p_postprocess, (void *) inst);
    dw1000_lwip_p2p_instance_t * lwip_p2p = inst->lwip_p2p;
    lwip_p2p->config.postprocess = true;
}

void
dw1000_lwip_p2p_start(dw1000_dev_instance_t * inst){

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

    char *buf = inst->lwip_p2p->payload_info[0]->input_payload.payload_ptr;
    for (int i = 0; i < 10; ++i)
    {
        printf("0x%x\n",*(buf+i));
        /* code */
    }
    inst->lwip_p2p_complete_cb(inst);

    /* Nothing to do for now. Place holder for future
     * expansions
     */
}

static void 
tx_complete_cb(dw1000_dev_instance_t * inst){

    /* Nothing to do for now. Place holder for future
     * expansions
     */
}

static void 
rx_timeout_cb(dw1000_dev_instance_t * inst){

    inst->lwip_p2p->status.rx_timeout_error = 1;
    return;
}

void
rx_error_cb(dw1000_dev_instance_t * inst){

    inst->lwip_p2p->status.rx_error = 1;
    return;
}
#endif //MYNEWT_VAL(DW1000_LWIP_P2P)
