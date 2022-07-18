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

volatile struct mbuf *gen_fake_reqs(void)
{
	struct mbuf *ret = NULL;

	for (int x = 0; x < MAX_LIVE_REQS; cursor = (cursor + 1) & (MAX_LIVE_REQS - 1), x++)
	{
		printf("cursor %d \n", cursor);
		if (live_reqs[cursor])
		{
			continue;
		}
		live_reqs[cursor] = 1;
		ret = &(fake_pkts[cursor]);

		printf("ret %d\n", *ret);
		break;
	}

	printf("Sent fake req\n");
	return ret;
}

void do_work_gen(void)
{
	int ikl;
	printf("Generating fake works\n");
	srand(time(NULL));

	
	while (networker_pointers.cnt != 0)
		;
	for (ikl = 0; ikl < networker_pointers.free_cnt; ikl++)
	{
		live_reqs[ikl] = 0;
	}
	networker_pointers.free_cnt = 0;
	for (ikl = 0; ikl < 7; ikl++)
	{
		// struct mbuf *temp = gen_fake_reqs();
		struct mbuf * temp;
		temp = mbuf_alloc_local();

		// db_key *key = malloc(sizeof(db_key));
		// (*key) = "my_key";

		asm volatile("cli":::);

		struct custom_payload * payload = malloc(sizeof(custom_payload));

		asm volatile("sti":::);

		payload->id = ikl+1;

		payload->ms = (rand() % 2) ? 1 : 100;

		struct db_req *req;

		req = mbuf_mtod(temp, struct db_req *);

		req->type = CUSTOM;
		req->params = payload;

		log_info("work generated with id %d, ms: %d\n", payload->id, payload->ms);
		usleep(100);

		// req->type = GET;
		// req->params = key;

		if (!temp)
		{
			break; // no more packets to receive
		}
		networker_pointers.pkts[ikl] = temp;
		networker_pointers.types[ikl] = 0; // For now, only 1 port/type
	}
	networker_pointers.cnt = ikl;
	
}