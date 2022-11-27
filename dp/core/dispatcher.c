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
extern volatile bool TEST_STARTED;
extern volatile bool IS_FIRST_PACKET;
extern volatile bool INIT_FINISHED;
void print_stats(void);

volatile uint64_t TEST_START_TIME;
volatile uint64_t TEST_END_TIME;
volatile uint64_t TEST_RCVD_SMALL_PACKETS;
volatile uint64_t TEST_RCVD_BIG_PACKETS;
volatile uint64_t TEST_TOTAL_PACKETS_COUNTER; 
volatile bool 	 TEST_FINISHED = false;

extern void dune_apic_send_posted_ipi(uint8_t vector, uint32_t dest_core);
extern void yield_handler(void);

#define PREEMPT_VECTOR 0xf2
#define PREEMPTION_DELAY 5000
#define CPU_FREQ_GHZ 3.3

uint16_t num_workers = 0;
volatile int * cpu_preempt_points [MAX_WORKERS] = {NULL};

static void preempt_check_init()
{
	int i;
	for (i = 0; i < num_workers; i++){
		preempt_check[i].check = false;
		preempt_check[i].timestamp = MAX_UINT64;
	}
}

static void dispatch_states_init()
{
	int i;
	for (i = 0; i < num_workers; i++){
		dispatch_states[i].next_push = 0;
		dispatch_states[i].next_pop = 0;
		dispatch_states[i].occupancy = 0;
	}
}

static void requests_init() {
	int i;
	for (i=0; i < num_workers; i++){
		for(uint8_t j = 0; j < JBSQ_LEN; j++)
		dispatcher_requests[i].requests[j].flag = INACTIVE;
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

static inline int get_idle_core(){
	uint8_t min_occupancy = JBSQ_LEN;
	int idle = -1;
	for (int i = 0; i < num_workers; i++){
		if(!dispatch_states[i].occupancy)
			return i;
		else if (dispatch_states[i].occupancy < min_occupancy){
			min_occupancy = dispatch_states[i].occupancy;
			idle = i;
		}
	}
	return idle;
}
static inline void dispatch_requests(uint64_t cur_time)
{
	while(1){
		int idle = get_idle_core();
		if(idle == -1)
			return;

		void *rnbl, *mbuf;
		uint8_t type, category;
		uint64_t timestamp;

		if (smart_tskq_dequeue(tskq, &rnbl, &mbuf, &type,
							&category, &timestamp, cur_time))
			return;
		
		uint8_t active_req = dispatch_states[idle].next_push;
		dispatcher_requests[idle].requests[active_req].rnbl = rnbl;
		dispatcher_requests[idle].requests[active_req].mbuf = mbuf;
		dispatcher_requests[idle].requests[active_req].type = type;
		dispatcher_requests[idle].requests[active_req].category = category;
		dispatcher_requests[idle].requests[active_req].timestamp = timestamp;
		dispatcher_requests[idle].requests[active_req].flag = READY;
		jbsq_get_next(&(dispatch_states[idle].next_push));
		dispatch_states[idle].occupancy++;

	}
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
		*(cpu_preempt_points[i]) = 1;
		preempt_check[i].check = false;
	}
}

static inline void handle_worker(uint8_t i, uint64_t cur_time)
{
	#if (SCHEDULE_METHOD == METHOD_PI)
	preempt_worker(i, cur_time);
	#endif
	#if (SCHEDULE_METHOD == METHOD_CONCORD)
	concord_preempt_worker(i, cur_time);
	#endif

	if (dispatcher_requests[i].requests[dispatch_states[i].next_pop].flag != READY)
	{
		if (worker_responses[i].responses[dispatch_states[i].next_pop].flag == FINISHED)
		{
			handle_finished(i, dispatch_states[i].next_pop);
			jbsq_get_next(&(dispatch_states[i].next_pop));
			dispatch_states[i].occupancy--;
		}
		else if (worker_responses[i].responses[dispatch_states[i].next_pop].flag == PREEMPTED)
		{
			handle_preempted(i, dispatch_states[i].next_pop);
			jbsq_get_next(&(dispatch_states[i].next_pop));
			dispatch_states[i].occupancy--;
		}
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
	uint8_t i;
	uint64_t cur_time;

	while (!INIT_FINISHED);
	
	num_workers = num_cpus-2;
	preempt_check_init();
	dispatch_states_init();
	requests_init();
	bool flag = true;

	while (1)
	{
		if (flag && TEST_STARTED && IS_FIRST_PACKET && (TEST_FINISHED || ((get_us() - TEST_START_TIME) > BENCHMARK_DURATION_US )))
		{
			log_info("\n\n ----------- Benchmark FINISHED ----------- \n");
			log_info("Benchmark - Total number of packets %d \n", TEST_TOTAL_PACKETS_COUNTER);
			log_info("Benchmark - %d big, %d small packets\n", TEST_RCVD_BIG_PACKETS, TEST_RCVD_SMALL_PACKETS);
			log_info("Benchmark - Time elapsed (us): %llu\n", get_us() - TEST_START_TIME);
			print_stats();
			log_info("Dispatcher exiting\n");
			flag = false;
			break;
		}

		cur_time = rdtsc();
		for (i = 0; i < num_cpus - 2; i++){
			handle_worker(i, cur_time);
		}
		handle_networker(cur_time);
		dispatch_requests(cur_time);
	}
}
