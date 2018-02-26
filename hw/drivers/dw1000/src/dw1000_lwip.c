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

dw1000_dev_status_t
dw1000_lwip_config(dw1000_dev_instance_t * inst, dw1000_lwip_config_t * config){

    assert(inst);
    assert(config);

    inst->lwip->config = config;
    return inst->status;
}

dw1000_lwip_instance_t * 
dw1000_lwip_init(dw1000_dev_instance_t * inst, dw1000_lwip_config_t * config, struct netif * netif){

    dw1000_lwip_instance_t * lwip = inst->lwip;
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

    assert(netif);
    inst->lwip->netif = netif;

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
dw1000_lwip_set_callbacks(dw1000_lwip_instance_t * inst, dw1000_dev_cb_t tx_complete_cb, dw1000_dev_cb_t rx_complete_cb,  dw1000_dev_cb_t rx_timeout_cb,  dw1000_dev_cb_t rx_error_cb)
{
    inst->dev->tx_complete_cb = tx_complete_cb;
    inst->dev->rx_complete_cb = rx_complete_cb;
    inst->dev->rx_timeout_cb = rx_timeout_cb;
    inst->dev->rx_error_cb = rx_error_cb;
}

inline void
dw1000_lwip_set_frames(dw1000_dev_instance_t * inst, test_frame_t * tx_frame){
    if (tx_frame != NULL)
	inst->lwip->tx_frame = tx_frame;
}

dw1000_dev_status_t 
dw1000_lwip_send(dw1000_dev_instance_t * inst, struct pbuf *p, dw1000_lwip_modes_t code){

    struct netif * lwip_netif;


    /* Semaphore lock for multi-threaded applications */
    os_error_t err = os_sem_pend(&inst->lwip->sem, OS_TIMEOUT_NEVER);
    assert(err == OS_OK);

    inst->lwip->tx_frame->code = code;

    test_frame_t * tx_frame = inst->lwip->tx_frame;
    //test_frame_t * twr = inst->lwip->tx_frame;

    dw1000_lwip_config_t * config = inst->lwip->config;

    printf("<LT> SENT %s : %d\n", __func__, __LINE__);

    tx_frame->seq_num++;
    tx_frame->code = code;

    lwip_netif = inst->lwip->netif;

    printf("NETIF : %lu\n", (long unsigned int)lwip_netif);
//    lwip_netif->output_ip6;
#if 0
    dw1000_write_tx(inst, (uint8_t *)tx_frame, 0, sizeof(test_frame_t));
    dw1000_write_tx_fctrl(inst, sizeof(test_frame_t), 0, false);     
#endif
    dw1000_write_tx(inst, (uint8_t *)p, 0, sizeof(struct pbuf));
    dw1000_set_wait4resp(inst, true);    
    dw1000_set_rx_timeout(inst, config->resp_timeout); 
    dw1000_set_rx_timeout(inst, 1000);
    dw1000_start_tx(inst);

    tx_frame->data += 1;
    if (tx_frame->data > 5000 )
	tx_frame->data = 0;

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
    //uint16_t data,dst_address;
    struct pbuf *p ;
    p = (struct pbuf *)malloc((sizeof(struct pbuf)));

    dw1000_read_rx(inst, (uint8_t *) p, 0, 1024);

    #if 0
    if (inst->fctrl == 0xC5){ 
        dw1000_read_rx(inst, (uint8_t *) &data, offsetof(test_frame_t,data), sizeof(uint16_t));
        dw1000_read_rx(inst, (uint8_t *) &dst_address, offsetof(test_frame_t,dst_address), sizeof(uint16_t));    
    }else{
        return;
    }

    if (dst_address != inst->my_short_address){
        return;
    }
    printf("\n--Received--\n\tData : %d\n", data);
    #endif

    lowpan6_input( p, inst->lwip->netif);

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
