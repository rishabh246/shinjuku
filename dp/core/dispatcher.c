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
 * dispatcher.c - dispatcher core functionality
 *
 * A single core is responsible for receiving network packets from the network
 * core and dispatching these packets or contexts to the worker cores.
 */

#include <stdio.h>
#include <ix/cfg.h>
#include <ix/context.h>
#include <ix/dispatch.h>

#include <ix/networker.h>
#include <net/ip.h>
#include <net/udp.h>

#include "helpers.h"
#include "benchmark.h"

// ---- Added for tests ----
extern uint64_t total_scheduled;
extern bool TEST_STARTED;
extern bool IS_FIRST_PACKET;
extern bool INIT_FINISHED;

uint64_t TEST_START_TIME;
uint64_t TEST_END_TIME;
uint64_t TEST_RCVD_SMALL_PACKETS;
uint64_t TEST_RCVD_BIG_PACKETS;
uint64_t TEST_TOTAL_PACKETS_COUNTER; 
bool 	 TEST_FINISHED = false;


extern void dune_apic_send_posted_ipi(uint8_t vector, uint32_t dest_core);
extern void yield_handler(void);

#define PREEMPT_VECTOR 0xf2
#define PREEMPTION_DELAY 5000
#define CPU_FREQ_GHZ 3.3

extern int concord_preempt_now;

static void preempt_check_init(int num_workers)
{
	int i;
	for (i = 0; i < num_workers; i++){
		preempt_check[i].check = false;
		preempt_check[i].timestamp = MAX_UINT64;
	}
}

static void requests_init(int num_workers) {
	int i;
	for (i=0; i < num_workers; i++){
		for(uint8_t j = 0; j < JBSQ_LEN; j++)
		dispatcher_requests[i].requests[j].flag = WAITING;
	}
}

static inline void handle_finished(uint8_t i, uint8_t active_req)
{
	if (worker_responses[i].responses[active_req].mbuf == NULL)
		log_warn("No mbuf was returned from worker\n");

	context_free(worker_responses[i].responses[active_req].rnbl);
	mbuf_enqueue(&mqueue, (struct mbuf *)worker_responses[i].responses[active_req].mbuf);
	worker_responses[i].responses[active_req].flag = PROCESSED;
}

static inline void handle_preempted(uint8_t i, uint8_t active_req)
{
	void *rnbl, *mbuf;
	uint8_t type, category;
	uint64_t timestamp, runned_for;

	rnbl = worker_responses[i].responses[active_req].rnbl;
	mbuf = worker_responses[i].responses[active_req].mbuf;
	category = worker_responses[i].responses[active_req].category;
	type = worker_responses[i].responses[active_req].type;
	timestamp = worker_responses[i].responses[active_req].timestamp;
	tskq_enqueue_tail(&tskq[type], rnbl, mbuf, type, category, timestamp);
	worker_responses[i].responses[active_req].flag = PROCESSED;
}

static inline void dispatch_request(uint8_t i, uint8_t active_req, uint64_t cur_time)
{
	void *rnbl, *mbuf;
	uint8_t type, category;
	uint64_t timestamp;

	if (smart_tskq_dequeue(tskq, &rnbl, &mbuf, &type,
						   &category, &timestamp, cur_time))
		return;

	worker_responses[i].responses[active_req].flag = RUNNING;
	dispatcher_requests[i].requests[active_req].rnbl = rnbl;
	dispatcher_requests[i].requests[active_req].mbuf = mbuf;
	dispatcher_requests[i].requests[active_req].type = type;
	dispatcher_requests[i].requests[active_req].category = category;
	dispatcher_requests[i].requests[active_req].timestamp = timestamp;
	dispatcher_requests[i].requests[active_req].flag = ACTIVE;
}

static inline void preempt_worker(uint8_t i, uint64_t cur_time)
{
	if (preempt_check[i].check && (((cur_time - preempt_check[i].timestamp) / CPU_FREQ_GHZ) > PREEMPTION_DELAY))
	{
		// Avoid preempting more times.
		preempt_check[i].check = false;
		dune_apic_send_posted_ipi(PREEMPT_VECTOR, CFG.cpu[i + 2]);
	}
}

static inline void concord_preempt_worker(uint8_t i, uint64_t cur_time)
{
	if (preempt_check[i].check && (((cur_time - preempt_check[i].timestamp) / CPU_FREQ_GHZ) > PREEMPTION_DELAY))
	{
		// Avoid preempting more times.
		concord_preempt_now = 1;
		preempt_check[i].check = false;
	}
}

static inline void handle_worker(uint8_t active_req, uint8_t i, uint64_t cur_time)
{
	#if (SCHEDULE_METHOD == METHOD_PI)
	preempt_worker(i, cur_time);
	#endif
	#if (SCHEDULE_METHOD == METHOD_CONCORD)
	concord_preempt_worker(i, cur_time);
	#endif

	if (worker_responses[i].responses[active_req].flag != RUNNING)
	{
		if (worker_responses[i].responses[active_req].flag == FINISHED)
		{
			handle_finished(i, active_req);
		}
		else if (worker_responses[i].responses[active_req].flag == PREEMPTED)
		{
			handle_preempted(i, active_req);
		}
		dispatch_request(i, active_req, cur_time);
	} 
}

static inline void handle_networker(uint64_t cur_time)
{
	int i, ret;
	uint8_t type;
	ucontext_t *cont;

	if (networker_pointers.cnt != 0)
	{
		for (i = 0; i < networker_pointers.cnt; i++)
		{
			if(unlikely(networker_pointers.pkts[i] == NULL))
			{
				continue;
			}

			ret = context_alloc(&cont);
			if (unlikely(ret))
			{
				log_warn("Cannot allocate context\n");
				mbuf_enqueue(&mqueue, (struct mbuf *)networker_pointers.pkts[i]);
				continue;
			}
			type = networker_pointers.types[i];
			tskq_enqueue_tail(&tskq[type], cont,
							  (void *)networker_pointers.pkts[i],
							  type, PACKET, cur_time);
		}

		for (i = 0; i < ETH_RX_MAX_BATCH; i++)
		{
			struct mbuf *buf = mbuf_dequeue(&mqueue);
			if (!buf)
				break;
			networker_pointers.pkts[i] = buf;
			networker_pointers.free_cnt++;
		}
		networker_pointers.cnt = 0;
	}
}

/**
 * do_dispatching - implements dispatcher core's main loop
 */
void do_dispatching(int num_cpus)
{
	uint8_t i,j;
	uint64_t cur_time;

	while (!INIT_FINISHED);
	
	preempt_check_init(num_cpus - 2);
	requests_init(num_cpus-2);
	bool flag = true;

	while (1)
	{
		if (flag && TEST_STARTED && IS_FIRST_PACKET && (TEST_FINISHED || ((get_us() - TEST_START_TIME) > 10000000 * 20 )))
		{
			printf("\n\n ----------- Benchmark FINISHED ----------- \n");
			printf("Benchmark - Total number of packets %d \n", TEST_TOTAL_PACKETS_COUNTER);
			printf("Benchmark - %d big, %d small packets\n", TEST_RCVD_BIG_PACKETS, TEST_RCVD_SMALL_PACKETS);
			printf("Benchmark - Time ellapsed: %llu\n", TEST_END_TIME - TEST_START_TIME);
			// printf("Benchmark - Total scheduled times: %d\n", total_scheduled);
			flag = false;
		}

		for(i = 0; i < JBSQ_LEN; i++){
			cur_time = rdtsc();
			for (j = 0; j < num_cpus - 2; j++){
				handle_worker(i, j, cur_time);
			}
			handle_networker(cur_time);
		}
		
	}
}
