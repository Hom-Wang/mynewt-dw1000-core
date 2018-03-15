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
#include <dw1000/dw1000_ftypes.h>
#include <dw1000/dw1000_lwip.h>

#include <lwip/pbuf.h>
#include <lwip/netif.h>
#include <netif/lowpan6.h>

// TODOs::This file is a place holder for the lwip project

static void rx_complete_cb(dw1000_dev_instance_t * inst);
static void tx_complete_cb(dw1000_dev_instance_t * inst);
static void rx_timeout_cb(dw1000_dev_instance_t * inst);
static void rx_error_cb(dw1000_dev_instance_t * inst);

static void ping_rx_complete_cb(dw1000_dev_instance_t * inst);
static void ping_tx_complete_cb(dw1000_dev_instance_t * inst);
static void ping_rx_timeout_cb(dw1000_dev_instance_t * inst);
static void ping_rx_error_cb(dw1000_dev_instance_t * inst);

dw1000_dev_status_t
dw1000_lwip_config(dw1000_dev_instance_t * inst, dw1000_lwip_config_t * config){

    assert(inst);
    assert(config);

    inst->lwip->config = config;
    return inst->status;
}

dw1000_lwip_instance_t *
dw1000_lwip_init(dw1000_dev_instance_t * inst, dw1000_lwip_config_t * config, bool ping_status){

    assert(inst);
    if (inst->lwip == NULL ){
        inst->lwip  = (dw1000_lwip_instance_t *) malloc(sizeof(dw1000_lwip_instance_t));
        assert(inst->lwip);
        memset(inst->lwip,0,sizeof(dw1000_lwip_instance_t));
        inst->lwip->status.selfmalloc = 1;
    }
    os_error_t err = os_sem_init(&inst->lwip->sem, 0x01);
    assert(err == OS_OK);

    if (config != NULL){
	inst->lwip->config = config;
	dw1000_lwip_config(inst, config);
    }

    ping_status ?
	dw1000_lwip_set_callbacks(inst, ping_tx_complete_cb, ping_rx_complete_cb, ping_rx_timeout_cb, ping_rx_error_cb) :
    	dw1000_lwip_set_callbacks(inst, tx_complete_cb, rx_complete_cb, rx_timeout_cb, rx_error_cb);
    
    inst->lwip->status.initialized = 1;

    return inst->lwip;
}


void 
dw1000_lwip_free(dw1000_lwip_instance_t * inst){

    assert(inst);  
    if (inst->status.selfmalloc)
        free(inst);
    else
        inst->status.initialized = 0;
}

void
dw1000_lwip_set_callbacks(  dw1000_dev_instance_t * inst,   dw1000_dev_cb_t tx_complete_cb, 
							    dw1000_dev_cb_t rx_complete_cb,  
							    dw1000_dev_cb_t rx_timeout_cb,  
							    dw1000_dev_cb_t rx_error_cb)
{
    inst->tx_complete_cb = tx_complete_cb;
    inst->rx_complete_cb = rx_complete_cb;
    inst->rx_timeout_cb = rx_timeout_cb;
    inst->rx_error_cb = rx_error_cb;
}

inline void
dw1000_lwip_set_frames(dw1000_dev_instance_t * inst, test_frame_t * tx_frame){
    if (tx_frame != NULL)
	inst->lwip->tx_frame = tx_frame;
}

dw1000_dev_status_t
dw1000_lwip_send(dw1000_dev_instance_t * inst, uint16_t dst_address, dw1000_lwip_modes_t code){

    /* Semaphore lock for multi-threaded applications */
    os_error_t err = os_sem_pend(&inst->lwip->sem, OS_TIMEOUT_NEVER);
    assert(err == OS_OK);

    inst->lwip->tx_frame->code = code;

    test_frame_t * tx_frame = inst->lwip->tx_frame;

    tx_frame->seq_num++;
    tx_frame->code = code;
    tx_frame->src_address = inst->my_short_address;
    tx_frame->dst_address = dst_address;

    dw1000_write_tx(inst, (uint8_t *) tx_frame, 0, sizeof(test_frame_t));
    dw1000_write_tx_fctrl(inst, sizeof(test_frame_t), 0, false);     
    dw1000_set_wait4resp(inst, false);
    dw1000_start_tx(inst);

    err = os_sem_pend(&inst->lwip->sem, 10000); // Wait for completion of transactions units os_clicks
    inst->status.request_timeout = (err == OS_TIMEOUT);
    os_sem_release(&inst->lwip->sem);

    if (inst->status.start_tx_error || inst->status.rx_error || inst->status.request_timeout ||  inst->status.rx_timeout_error){
	tx_frame->seq_num--;
    }

    return inst->status;
}


dw1000_lwip_status_t 
dw1000_lwip_write(dw1000_lwip_instance_t * inst, dw1000_lwip_config_t * rng_config, dw1000_lwip_modes_t mode){

    /* Semaphore lock for multi-threaded applications */
    os_error_t err = os_sem_pend(&inst->sem, OS_TIMEOUT_NEVER);
    assert(err == OS_OK);

    // TODOs::
    return inst->status;
}


static void 
rx_complete_cb(dw1000_dev_instance_t * inst){

    dw1000_lwip_instance_t * lwip = (dw1000_lwip_instance_t * ) inst->lwip;

    os_error_t err = os_sem_release(&lwip->sem);
    assert(err == OS_OK);
}

static void 
tx_complete_cb(dw1000_dev_instance_t * inst){

    dw1000_lwip_instance_t * lwip = (dw1000_lwip_instance_t * ) inst->lwip;

    os_error_t err = os_sem_release(&lwip->sem);
    assert(err == OS_OK);
}

static void 
rx_timeout_cb(dw1000_dev_instance_t * inst){

    dw1000_lwip_instance_t * lwip = (dw1000_lwip_instance_t * ) inst->lwip;

    os_error_t err = os_sem_release(&lwip->sem);
    assert(err == OS_OK);

}

static void 
rx_error_cb(dw1000_dev_instance_t * inst){

    dw1000_lwip_instance_t * lwip = (dw1000_lwip_instance_t * ) inst->lwip;
   
    os_error_t err = os_sem_release(&lwip->sem);
    assert(err == OS_OK);

}

#define DATA_LEN 50
dw1000_dev_status_t
dw1000_lwip_ping_send(dw1000_dev_instance_t * inst, struct pbuf *p, dw1000_lwip_modes_t code){

    /* Semaphore lock for multi-threaded applications */
    os_error_t err = os_sem_pend(&inst->lwip->sem, OS_TIMEOUT_NEVER);
    assert(err == OS_OK);

    dw1000_lwip_config_t * config = inst->lwip->config;

    dw1000_write_tx(inst, (uint8_t *) p, 0, DATA_LEN);
    dw1000_write_tx_fctrl(inst, DATA_LEN, 0, false);     
    dw1000_set_wait4resp(inst, true);
    dw1000_set_rx_timeout(inst, config->resp_timeout);
    dw1000_start_tx(inst);

    err = os_sem_pend(&inst->lwip->sem, 10000); // Wait for completion of transactions units os_clicks
    inst->status.request_timeout = (err == OS_TIMEOUT);
    os_sem_release(&inst->lwip->sem);

    return inst->status;
}


dw1000_dev_status_t
dw1000_lwip_resp_send(dw1000_dev_instance_t * inst, struct pbuf *p, dw1000_lwip_modes_t code){

    /* Semaphore lock for multi-threaded applications */
    os_error_t err = os_sem_pend(&inst->lwip->sem, OS_TIMEOUT_NEVER);
    assert(err == OS_OK);

    dw1000_write_tx(inst, (uint8_t *) p, 0, DATA_LEN);
    dw1000_write_tx_fctrl(inst, DATA_LEN, 0, false);     
    dw1000_set_wait4resp(inst, false);
    dw1000_start_tx(inst);

    err = os_sem_pend(&inst->lwip->sem, 10000); // Wait for completion of transactions units os_clicks
    inst->status.request_timeout = (err == OS_TIMEOUT);
    os_sem_release(&inst->lwip->sem);

    return inst->status;
}

static void 
ping_rx_complete_cb(dw1000_dev_instance_t * inst){

    hal_gpio_toggle(LED_1);

    if(inst->fctrl == 26668)
    {
	test_frame_t * tx_frame;
	tx_frame = (test_frame_t *)malloc(sizeof(test_frame_t));
	tx_frame->fctrl = 0xC5;
	tx_frame->PANID = 0xDECA;
	
	dw1000_write_tx(inst, (uint8_t *)tx_frame, 0, sizeof(test_frame_t));
	dw1000_write_tx_fctrl(inst, sizeof(test_frame_t), 0, false); 
	dw1000_set_wait4resp(inst, false);    
	dw1000_start_tx(inst);
    }

    if(inst->fctrl == 197)
    {
	struct pbuf *p;
	p=NULL;
	inst->lwip->netif->input(p, inst->lwip->netif);
    }
    os_error_t err = os_sem_release(&inst->lwip->sem);
    assert(err == OS_OK);

}

static void 
ping_tx_complete_cb(dw1000_dev_instance_t * inst){

    os_error_t err = os_sem_release(&inst->lwip->sem);
    assert(err == OS_OK);
}

static void 
ping_rx_timeout_cb(dw1000_dev_instance_t * inst){

    os_error_t err = os_sem_release(&inst->lwip->sem);
    assert(err == OS_OK);
}

static void 
ping_rx_error_cb(dw1000_dev_instance_t * inst){

    os_error_t err = os_sem_release(&inst->lwip->sem);
    assert(err == OS_OK);

}

