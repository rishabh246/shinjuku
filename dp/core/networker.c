/*
 * Copyright 2018-19 Board of Trustees of Stanford University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * networker.c - networking core functionality
 *
 * A single core is responsible for receiving all network packets in the
 * system and forwading them to the dispatcher.
 */
#include <stdio.h>

#include <ix/vm.h>
#include <ix/cfg.h>
#include <ix/log.h>
#include <ix/mbuf.h>
#include <ix/dispatch.h>
#include <ix/ethqueue.h>
#include <ix/transmit.h>

#include <asm/chksum.h>

#include <net/ip.h>
#include <net/udp.h>
#include <net/ethernet.h>

#include <ix/leveldb.h>
#include <time.h>
#include "helpers.h"

uint64_t TEST_START_TIME = 0;
uint64_t TOTAL_PACKETS = 0; 
bool TEST_STARTED = false;

/**
 * do_networking - implements networking core's functionality
 */
void do_networking(void)
{
	log_info("Do networking started \n");
	int i, num_recv;
	while (1)
	{
		eth_process_poll();
		num_recv = eth_process_recv();
		if (num_recv == 0)
			continue;
		while (networker_pointers.cnt != 0)
			;
		for (i = 0; i < networker_pointers.free_cnt; i++)
		{
			mbuf_free(networker_pointers.pkts[i]);
		}
		networker_pointers.free_cnt = 0;
		for (i = 0; i < num_recv; i++)
		{
			networker_pointers.pkts[i] = recv_mbufs[i];
			networker_pointers.types[i] = (uint8_t)recv_type[i];
		}
		networker_pointers.cnt = num_recv;
	}
}

/* Support for fake networking */

#define MAX_LIVE_REQS 128

// Basic mempool. TODO: Optimize
volatile struct mbuf fake_pkts[MAX_LIVE_REQS];
int live_reqs[MAX_LIVE_REQS] = {0};
int cursor = 0;
struct mempool_datastore custom_payload_datastore;

int create_custom_payload_datastore()
{
    return mempool_create_datastore(&custom_payload_datastore, 128000,
                                    sizeof(struct custom_payload), 1,
                                    MEMPOOL_DEFAULT_CHUNKSIZE,
                                    "custom_payload");
}

void do_work_gen(void)
{
	int t;
	log_info("Creating mempool for custom payloads\n");
	create_custom_payload_datastore();
	
	log_info("Generating fake works\n");
	srand(time(NULL));
	long work_counter = 0;

	TEST_START_TIME = get_us();
	TEST_STARTED = true;
	log_info("Test started %u, %d\n", TEST_START_TIME, TEST_STARTED);

	while (1)
	{
		while (networker_pointers.cnt != 0);

		for (t = 0; t < networker_pointers.free_cnt; t++)
		{
			live_reqs[t] = 0;
		}
		networker_pointers.free_cnt = 0;
		for (t = 0; t < ETH_RX_MAX_BATCH; t++)
		{
			// struct mbuf *temp = gen_fake_reqs();
			struct mbuf *temp;
			temp = mbuf_alloc_local();

			// db_key *key = malloc(sizeof(db_key));
			// (*key) = "my_key";

			struct custom_payload *
				req = mbuf_mtod(temp, struct custom_payload *);

			req->id = t + 1;
			req->ms = (rand() % 2) ? 1 : 100;

			// log_info("work generated %d\n", work_counter++);
			usleep(10);

			if (!temp)
			{
				break; // no more packets to receive
			}
			networker_pointers.pkts[t] = temp;
			networker_pointers.types[t] = 0; // For now, only 1 port/type
		}
		networker_pointers.cnt = t;
	}
}