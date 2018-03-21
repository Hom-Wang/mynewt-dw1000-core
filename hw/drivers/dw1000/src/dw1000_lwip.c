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

#include <dw1000/dw1000_phy.h>
#include "sysinit/sysinit.h"


#include <lwip/pbuf.h>
#include <lwip/netif.h>
#include <netif/lowpan6.h>
#include <lwip/ethip6.h>


// TODOs::This file is a place holder for the lwip project

static void rx_complete_cb(dw1000_dev_instance_t * inst);
static void tx_complete_cb(dw1000_dev_instance_t * inst);
static void rx_timeout_cb(dw1000_dev_instance_t * inst);
static void rx_error_cb(dw1000_dev_instance_t * inst);


static 
dwt_config_t tx_mac_config = {
	.chan = 5,                          // Channel number. 
	.prf = DWT_PRF_64M,                 // Pulse repetition frequency. 
	.txPreambLength = DWT_PLEN_256,     // Preamble length. Used in TX only. 
	.rxPAC = DWT_PAC8,                  // Preamble acquisition chunk size. Used in RX only. 
	.txCode = 8,                        // TX preamble code. Used in TX only. 
	.rxCode = 9,                        // RX preamble code. Used in RX only. 
	.nsSFD = 0,                         // 0 to use standard SFD, 1 to use non-standard SFD. 
	.dataRate = DWT_BR_6M8,             // Data rate. 
	.phrMode = DWT_PHRMODE_STD,         // PHY header mode. 
	.sfdTO = (512 + 1 + 8 - 8)          // SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. 
};

static 
dwt_config_t rx_mac_config = {
	.chan = 5,                          // Channel number. 
	.prf = DWT_PRF_64M,                 // Pulse repetition frequency. 
	.txPreambLength = DWT_PLEN_512,     // Preamble length. Used in TX only. 
	.rxPAC = DWT_PAC8,                  // Preamble acquisition chunk size. Used in RX only. 
	.txCode = 9,                        // TX preamble code. Used in TX only. 
	.rxCode = 8,                        // RX preamble code. Used in RX only. 
	.nsSFD = 0,                         // 0 to use standard SFD, 1 to use non-standard SFD. 
	.dataRate = DWT_BR_6M8,             // Data rate. 
	.phrMode = DWT_PHRMODE_STD,         // PHY header mode. 
	.sfdTO = (512 + 1 + 8 - 8)          // SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. 
};

static 
dw1000_phy_txrf_config_t txrf_config = { 
	.PGdly = TC_PGDELAY_CH5,
	.BOOSTNORM = dw1000_power_value(DW1000_txrf_config_9db, 5),
	.BOOSTP500 = dw1000_power_value(DW1000_txrf_config_9db, 5),
	.BOOSTP250 = dw1000_power_value(DW1000_txrf_config_9db, 5),
	.BOOSTP125 = dw1000_power_value(DW1000_txrf_config_9db, 5)
};


static 
dw1000_lwip_config_t lwip_config = {
	.poll_resp_delay = 0x4800,
	.resp_timeout = 0xF000,
	.uwbtime_to_systime = 0
};


dw1000_dev_status_t
dw1000_lwip_config(dw1000_dev_instance_t * inst, dw1000_lwip_config_t * config){

	assert(inst);
	assert(config);

	inst->lwip->config = config;
	return inst->status;
}


dw1000_lwip_instance_t *
dw1000_lwip_init(dw1000_dev_instance_t * inst, dw1000_lwip_config_t * config){

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
dw1000_lwip_set_callbacks(  dw1000_dev_instance_t * inst,   dw1000_dev_cb_t tx_complete_cb, dw1000_dev_cb_t rx_complete_cb,  
		dw1000_dev_cb_t rx_timeout_cb, dw1000_dev_cb_t rx_error_cb)
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


dw1000_lwip_status_t 
dw1000_lwip_write(dw1000_lwip_instance_t * inst, dw1000_lwip_config_t * rng_config, dw1000_lwip_modes_t mode){

	/* Semaphore lock for multi-threaded applications */
	os_error_t err = os_sem_pend(&inst->sem, OS_TIMEOUT_NEVER);
	assert(err == OS_OK);

	// TODOs::
	return inst->status;
}


#define DATA_LEN 80
dw1000_dev_status_t
dw1000_lwip_send(dw1000_dev_instance_t * inst, struct pbuf *p, dw1000_lwip_modes_t code){

	/* Semaphore lock for multi-threaded applications */
	os_error_t err = os_sem_pend(&inst->lwip->sem, OS_TIMEOUT_NEVER);
	assert(err == OS_OK);

	dw1000_lwip_config_t * config = inst->lwip->config;

	dw1000_write_tx(inst, (uint8_t *) p, 0, DATA_LEN);
	dw1000_write_tx_fctrl(inst, DATA_LEN, 0, false);     
	dw1000_set_wait4resp(inst, true);
	dw1000_set_rx_timeout(inst, config->resp_timeout);
	inst->lwip->netif->flags = 5 ;
	dw1000_start_tx(inst);

	err = os_sem_pend(&inst->lwip->sem, 10000); // Wait for completion of transactions units os_clicks
	inst->status.request_timeout = (err == OS_TIMEOUT);
	os_sem_release(&inst->lwip->sem);

	return inst->status;
}


void
dw1000_lwip_start_receive(dw1000_dev_instance_t * inst){
	dw1000_set_rx_timeout(inst, 0);
	dw1000_start_rx(inst); 
}


static void 
rx_complete_cb(dw1000_dev_instance_t * inst){

	dw1000_lwip_start_receive(inst);
	hal_gpio_toggle(LED_1);
	printf("RX CB\n");
	#if 0
	printf("FCTRL : %d\n", inst->fctrl );
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
	uint8_t i;
	for (i=0 ; i < DATA_LEN ; ++i)
		printf(" %d \n", *(buf+i));
	#endif
	char * buf;
	buf = (char *)malloc(DATA_LEN);
	dw1000_read_rx(inst,(uint8_t *) buf, 0,DATA_LEN);
	inst->lwip->netif->input((struct pbuf *)buf, inst->lwip->netif);


	os_error_t err = os_sem_release(&inst->lwip->sem);
	assert(err == OS_OK);
}


static void 
tx_complete_cb(dw1000_dev_instance_t * inst){

	printf("TX CB\n");
	os_error_t err = os_sem_release(&inst->lwip->sem);
	assert(err == OS_OK);
}


static void 
rx_timeout_cb(dw1000_dev_instance_t * inst){

	os_error_t err = os_sem_release(&inst->lwip->sem);
	assert(err == OS_OK);
}


void 
rx_error_cb(dw1000_dev_instance_t * inst){

	dw1000_lwip_start_receive(inst);
	os_error_t err = os_sem_release(&inst->lwip->sem);
	assert(err == OS_OK);
}


void
dw1000_low_level_init( dw1000_dev_instance_t * inst, bool rx_status ){

	sysinit();
	dw1000_softreset(inst);
	dw1000_phy_init(inst, &txrf_config);
	inst->PANID = 0xDECA;
	inst->fctrl = 0xDECA; 

	dw1000_set_panid(inst,inst->PANID);
	(rx_status) ? dw1000_mac_init(inst, &rx_mac_config) : dw1000_mac_init(inst, &tx_mac_config);

	dw1000_lwip_init(inst, &lwip_config);
}


void 
dw1000_netif_config( dw1000_dev_instance_t * inst, struct netif *dw1000_netif, ip_addr_t *my_ip_addr, bool rx_status){

	dw1000_low_level_init(inst, rx_status);

	netif_add(dw1000_netif, NULL, dw1000_netif_init, ip6_input);
	IP_ADDR6_HOST(dw1000_netif->ip6_addr, 	my_ip_addr->addr[0], 
						my_ip_addr->addr[1], 
						my_ip_addr->addr[2], 
						my_ip_addr->addr[3]);
	dw1000_netif->ip6_addr_state[0] = IP6_ADDR_VALID;

	netif_set_default(dw1000_netif);
	netif_set_link_up(dw1000_netif);
	netif_set_up(dw1000_netif);

	inst->lwip->netif = netif_default;
}


err_t 
dw1000_netif_init(struct netif *dw1000_netif){

	LWIP_ASSERT("netif != NULL", (dw1000_netif != NULL));

	dw1000_netif->hostname = "twr_lwip";
	dw1000_netif->name[0] = 'D';
	dw1000_netif->name[1] = 'W';
	dw1000_netif->hwaddr_len = 2;
	dw1000_netif->input = dw1000_ll_input;
	dw1000_netif->linkoutput = dw1000_ll_output;

	return ERR_OK;
}


err_t 
dw1000_ll_output(struct netif *dw1000_netif, struct pbuf *p){

	dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
	dw1000_lwip_send(inst,p,0 );

	if (inst->status.start_rx_error)
		printf("timer_ev_cb:[start_rx_error]\n");
	if (inst->status.start_tx_error)
		printf("timer_ev_cb:[start_tx_error]\n");
	if (inst->status.rx_error)
		printf("timer_ev_cb:[rx_error]\n");
	if (inst->status.request_timeout)
		printf("timer_ev_cb:[request_timeout]\n");
	if (inst->status.rx_timeout_error)
		printf("timer_ev_cb:[rx_timeout_error]\n");

	if (inst->status.start_tx_error || inst->status.rx_error || inst->status.request_timeout ||  inst->status.rx_timeout_error){
		inst->status.start_tx_error = inst->status.rx_error = inst->status.request_timeout = inst->status.rx_timeout_error = 0;
	}

	return ERR_OK;
}


err_t 
dw1000_ll_input(struct pbuf *pt, struct netif *dw1000_netif){

	/*TODO: Integrate 6lowPAN_input(p, netif) for 
		passing on the data to higher layers
		" lowpan6_input(struct pbuf * p, struct netif *netif)"

	 */
	pt->payload = pt + 1;
	lowpan6_input(pt, dw1000_netif);

	return ERR_OK;
}

