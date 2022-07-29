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
#include "benchmark.h"

extern bool INIT_FINISHED;
bool TEST_STARTED = false;

struct custom_payload* generate_benchmark_request(struct mbuf* temp, uint64_t t);
struct db_req* generate_db_req(DB_REQ_TYPE type, struct mbuf * temp);

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

/**
 * do_fake_networking - implements fake network logic with a given benchmark
 */
void do_fake_networking(void)
{
	uint64_t packet_counter = 0, work_counter = 0, t = 0;
	srand(time(NULL));

	TEST_STARTED = true;
	log_info("Generating fake works\n");
	log_info("Test started\n");

	while (!INIT_FINISHED);
	
	
	DB_REQ_TYPE request_types [2] = { DB_ITERATOR, DB_GET};

	while (packet_counter < BENCHMARK_NO_PACKETS + 2)
	{
		while (networker_pointers.cnt != 0);

		for (t = 0; t < networker_pointers.free_cnt; t++)
		{
			mbuf_free(networker_pointers.pkts[t]);
		}

		networker_pointers.free_cnt = 0;

		for (t = 0; t < ETH_RX_MAX_BATCH; t++)
		{
			struct mbuf* temp = mbuf_alloc_local();

			generate_db_req(request_types[rand() % 2], temp);
			// generate_benchmark_request(temp, packet_counter);
			
			// -------- Send --------
			networker_pointers.pkts[t] = temp;
			networker_pointers.types[t] = 0; 	// For now, only 1 port/type

			packet_counter += 1;
		}
		
		networker_pointers.cnt = ETH_RX_MAX_BATCH;
	}
}


struct db_req* generate_db_req(DB_REQ_TYPE type, struct mbuf * temp)
{
	struct db_req* req = mbuf_mtod(temp, struct db_req *);
	req->type = type;

	if (type == DB_GET)
	{
		strcpy(req->key, "musakey");
		strcpy(req->val, "musavalue");
	} 
	else if(type == DB_ITERATOR)
	{
		strcpy(req->key, "");
		strcpy(req->key, "");
	}
	else if(type == DB_DELETE)
	{
	}
	else if(type == DB_PUT)
	{
		strcpy(req->key, "musakey");
		strcpy(req->val, "musavalue");
	}
	else if (type == DB_SEEK)
	{
		strcpy(req->key, "");
		strcpy(req->key, "");
	}

	req->ts = get_ns();
	return req;
}

struct custom_payload* generate_benchmark_request(struct mbuf* temp, uint64_t t) 
{
	struct custom_payload *
	req = mbuf_mtod(temp, struct custom_payload *);

	req->id = t + 1;

	#if BENCHMARK_TYPE == 1
	req->ns = (rand() % 2) ? 1 * 1000 : 100 * 1000;
	#elif BENCHMARK_TYPE == 2
	req->ns = (rand() % 1000) < 995 ? 0.5 * 1000 : 500 * 1000;
	#else
	// No Benchmark Provided
	#endif

	req->timestamp = get_us();
	return req;
}