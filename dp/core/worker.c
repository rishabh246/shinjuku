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
 * worker.c - Worker core functionality
 *
 * Poll dispatcher CPU to get request to execute. The request is in the form
 * of ucontext_t. If interrupted, swap to main context and poll for next
 * request.
 */

#include <ucontext.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <stdio.h>
#include <time.h>

#include <sys/types.h>
#include <sys/resource.h>

// Added for leveldb
#include <ix/leveldb.h>
#include <ix/hijack.h>
#include <leveldb/c.h>

#include <ix/cpu.h>
#include <ix/log.h>
#include <ix/mbuf.h>
#include <asm/cpu.h>
#include <ix/context.h>
#include <ix/dispatch.h>
#include <ix/transmit.h>

#include <dune.h>

#include <net/ip.h>
#include <net/udp.h>
#include <net/ethernet.h>

#include "helpers.h"
#include "benchmark.h"
#include <sys/stat.h>
#include <fcntl.h>
#include "looplib.h"
#include "concord.h"

bool PREEMPT_NOW = false;

// ---- Added for tests ----
extern uint64_t TEST_TOTAL_PACKETS_COUNTER;
extern uint64_t TEST_RCVD_SMALL_PACKETS;
extern uint64_t TEST_RCVD_BIG_PACKETS;
extern uint64_t TEST_START_TIME;
extern uint64_t TEST_END_TIME;
extern bool TEST_FINISHED;

// Added for leveldb support
extern leveldb_t *db;
// extern leveldb_iterator_t *iter;
extern leveldb_options_t *options;
extern leveldb_readoptions_t *roptions;
extern leveldb_writeoptions_t *woptions;

#define PREEMPT_VECTOR 0xf2

// Local Variables
uint64_t JOB_STARTED_AT = 0;
uint64_t total_scheduled = 0;

// For debugging purposes
// uint64_t posted_interrupts[1024 * 1024] = {0};
// uint64_t interrupt_iterator = 0;

// uint64_t yeilds [1024 * 1024] = {0};
// uint64_t yield_iterator = 0;

__thread ucontext_t uctx_main;
__thread ucontext_t *cont;
__thread int cpu_nr_;
__thread volatile uint8_t finished;

// Added for leveldb
extern uint8_t flag;

DEFINE_PERCPU(struct mempool, response_pool __attribute__((aligned(64))));

extern int getcontext_fast(ucontext_t *ucp);
extern int swapcontext_fast(ucontext_t *ouctx, ucontext_t *uctx);
extern int swapcontext_fast_to_control(ucontext_t *ouctx, ucontext_t *uctx);
extern int swapcontext_very_fast(ucontext_t *ouctx, ucontext_t *uctx);

extern void dune_apic_eoi();
extern int dune_register_intr_handler(int vector, dune_intr_cb cb);

struct response
{
    uint64_t runNs;
    uint64_t genNs;
};

struct request
{
    uint64_t runNs;
    uint64_t genNs;
};

/**
 * myresponse_init - allocates global response datastore
 */
int myresponse_init(void)
{
    return mempool_create_datastore(&response_datastore, 128000,
                                    sizeof(struct myresponse), 1,
                                    MEMPOOL_DEFAULT_CHUNKSIZE,
                                    "response");
}

/**
 * response_init - allocates global response datastore
 */
int response_init(void)
{
    return myresponse_init();
    // return mempool_create_datastore(&response_datastore, 128000,
    //                                 sizeof(struct response), 1,
    //                                 MEMPOOL_DEFAULT_CHUNKSIZE,
    //                                 "response");
}

/**
 * response_init_cpu - allocates per cpu response mempools
 */
int response_init_cpu(void)
{
    struct mempool *m = &percpu_get(response_pool);
    return mempool_create(m, &response_datastore, MEMPOOL_SANITY_PERCPU,
                          percpu_get(cpu_id));
}

static void test_handler(struct dune_tf *tf)
{
    asm volatile("cli" :::);

    #if SCHEDULE_METHOD == METHOD_YIELD
    log_err("Interrupt fired \n");
    #endif

    dune_apic_eoi();
    
    swapcontext_fast_to_control(cont, &uctx_main);
}

void concord_func()
{
    asm volatile("nop");
}

/**
 * generic_work - generic function acting as placeholder for application-level
 *                work
 * @msw: the top 32-bits of the pointer containing the data
 * @lsw: the bottom 32 bits of the pointer containing the data
 */
static void generic_work(uint32_t msw, uint32_t lsw, uint32_t msw_id,
                         uint32_t lsw_id)
{
    asm volatile("sti" ::
                     :);

    struct ip_tuple *id = (struct ip_tuple *)((uint64_t)msw_id << 32 | lsw_id);
    void *data = (void *)((uint64_t)msw << 32 | lsw);
    int ret;

    struct request *req = (struct request *)data;

    // Added for leveldb
    // leveldb_readoptions_t *readoptions = leveldb_readoptions_create();
    // leveldb_iterator_t *iter = leveldb_create_iterator(db, readoptions);
    // for (leveldb_iter_seek_to_first(iter); leveldb_iter_valid(iter); leveldb_iter_next(iter))
    // {
    //     char *retr_key;
    //     size_t klen;
    //     retr_key = leveldb_iter_key(iter, &klen);
    //     if (req->runNs > 0)
    //         break;
    // }
    // leveldb_iter_destroy(iter);
    // leveldb_readoptions_destroy(readoptions);

    // uint64_t i = 0;
    // do
    // {
    //     asm volatile("nop");
    //     i++;
    // } while (i / 0.233 < req->runNs);

    asm volatile("cli" ::
                     :);

    struct response *resp = mempool_alloc(&percpu_get(response_pool));
    if (!resp)
    {
        log_warn("Cannot allocate response buffer\n");
        finished = true;
        swapcontext_very_fast(cont, &uctx_main);
    }

    resp->genNs = req->genNs;
    resp->runNs = req->runNs;
    struct ip_tuple new_id = {
        .src_ip = id->dst_ip,
        .dst_ip = id->src_ip,
        .src_port = id->dst_port,
        .dst_port = id->src_port};

    ret = udp_send((void *)resp, sizeof(struct response), &new_id,
                   (uint64_t)resp);
    // ret = udp_send_one((void *)resp, sizeof(struct response), &new_id,
    //             (uint64_t) resp);

    if (ret)
        log_warn("udp_send failed with error %d\n", ret);

    finished = true;
    swapcontext_very_fast(cont, &uctx_main);
}

static inline void parse_packet(struct mbuf *pkt, void **data_ptr,
                                struct ip_tuple **id_ptr)
{
    log_info("new packet \n");
    // Quickly parse packet without doing checks
    struct eth_hdr *ethhdr = mbuf_mtod(pkt, struct eth_hdr *);
    struct ip_hdr *iphdr = mbuf_nextd(ethhdr, struct ip_hdr *);
    int hdrlen = iphdr->header_len * sizeof(uint32_t);
    struct udp_hdr *udphdr = mbuf_nextd_off(iphdr, struct udp_hdr *,
                                            hdrlen);
    // Get data and udp header
    (*data_ptr) = mbuf_nextd(udphdr, void *);
    uint16_t len = ntoh16(udphdr->len);

    if (unlikely(!mbuf_enough_space(pkt, udphdr, len)))
    {
        log_warn("worker: not enough space in mbuf\n");
        (*data_ptr) = NULL;
        return;
    }

    (*id_ptr) = mbuf_mtod(pkt, struct ip_tuple *);
    (*id_ptr)->src_ip = ntoh32(iphdr->src_addr.addr);
    (*id_ptr)->dst_ip = ntoh32(iphdr->dst_addr.addr);
    (*id_ptr)->src_port = ntoh16(udphdr->src_port);
    (*id_ptr)->dst_port = ntoh16(udphdr->dst_port);
    pkt->done = (void *)0xDEADBEEF;
}

static inline void init_worker(void)
{
    cpu_nr_ = percpu_get(cpu_nr) - 2;
    worker_responses[cpu_nr_].flag = PROCESSED;

    dune_register_intr_handler(PREEMPT_VECTOR, test_handler);

    eth_process_reclaim();
    asm volatile("cli" ::
                     :);
}

static inline void handle_new_packet(void)
{
    int ret;
    void *data;
    struct ip_tuple *id;
    struct mbuf *pkt = (struct mbuf *)dispatcher_requests[cpu_nr_].mbuf;
    parse_packet(pkt, &data, &id);

    log_info("parse packet");

    if (data)
    {
        uint32_t msw = ((uint64_t)data & 0xFFFFFFFF00000000) >> 32;
        uint32_t lsw = (uint64_t)data & 0x00000000FFFFFFFF;
        uint32_t msw_id = ((uint64_t)id & 0xFFFFFFFF00000000) >> 32;
        uint32_t lsw_id = (uint64_t)id & 0x00000000FFFFFFFF;
        cont = dispatcher_requests[cpu_nr_].rnbl;
        getcontext_fast(cont);
        set_context_link(cont, &uctx_main);
        makecontext(cont, (void (*)(void))generic_work, 4, msw, lsw,
                    msw_id, lsw_id);
        finished = false;
        ret = swapcontext_very_fast(&uctx_main, cont);
        if (ret)
        {
            log_err("Failed to do swap into new context\n");
            exit(-1);
        }
    }
    else
    {
        log_info("OOPS No Data\n");
        finished = true;
    }
}

static void simple_generic_work(long ns, int id)
{
    uint64_t i = 0, to_run = 0;
    float us = (float)ns / 12;

    to_run = us / 3.3;

    asm volatile("sti" ::
                     :);
    do
    {
        asm volatile("nop");
        i++;
        if (unlikely(get_ns() - JOB_STARTED_AT > 4900))
        {
            asm volatile("nop");

#if SCHEDULE_METHOD == METHOD_YIELD
            swapcontext_fast_to_control(cont, &uctx_main);
#endif
        }
    } while (to_run > i);
    asm volatile("cli" ::
                     :);

    TEST_TOTAL_PACKETS_COUNTER += 1;

    if (TEST_TOTAL_PACKETS_COUNTER == BENCHMARK_STOP_AT_PACKET)
    {
        TEST_END_TIME = get_us();
        TEST_FINISHED = true;
    }

    if (ns == BENCHMARK_SMALL_PKT_NS)
        TEST_RCVD_SMALL_PACKETS += 1;
    else
        TEST_RCVD_BIG_PACKETS += 1;

    finished = true;
    swapcontext_very_fast(cont, &uctx_main);
}

static void do_db_generic_work(struct db_req *db_pkg, uint64_t _start_time)
{
    // Set interrupt flag
    asm volatile("sti" ::
                     :);
    uint64_t start_time = _start_time;
    DB_REQ_TYPE type = db_pkg->type;
    uint64_t iter_cnt = 0;

    switch (db_pkg->type)
    {
    case (DB_PUT):
    {
        char *db_err = NULL;

        PRE_PROTECTCALL;
        leveldb_put(db, woptions,
                    db_pkg->key, KEYSIZE,
                    db_pkg->val, VALSIZE,
                    &db_err);
        POST_PROTECTCALL;

        break;
    }

    case (DB_GET):
    {
        char *db_err = NULL;
        int read_len;

        PRE_PROTECTCALL;
        char *returned_value = leveldb_get(db, roptions,
                                "key1", 4,
                                &read_len, &db_err);
        POST_PROTECTCALL;


        break;
    }
    case (DB_DELETE):
    {
        int k = 0;

        while (k < 50000)
        {
            asm volatile("nop");
            asm volatile("nop");
            asm volatile("nop");
            asm volatile("nop");
            k++;
        }

        break;
    }
    case (DB_ITERATOR):
    {
        libloop(100000);


        break;
    }

    case (DB_SEEK):
    {
        PRE_PROTECTCALL;
        leveldb_iterator_t *iter = leveldb_create_iterator(db, roptions);
        POST_PROTECTCALL;

        // swapcontext_fast_to_control(cont, &uctx_main);

        PRE_PROTECTCALL;
        leveldb_iter_seek(iter,"mykey",5);
        POST_PROTECTCALL;

        break;
    }
    default:
        break;
    }

    // printf("Type of request %d, handle time (ns): %d\n", db_pkg->type, get_ns()-start_time);

    // fake_network_send(db_pkg->type, start_time - get_ns(), "finish", 7);
    asm volatile("cli" ::
                     :);

    TEST_TOTAL_PACKETS_COUNTER += 1;

    if (TEST_TOTAL_PACKETS_COUNTER == BENCHMARK_STOP_AT_PACKET)
    {
        TEST_END_TIME = get_us();
        TEST_FINISHED = true;
    }

    if (type == DB_GET || type == DB_PUT){
        TEST_RCVD_SMALL_PACKETS += 1;
    }
    else
    {
        TEST_RCVD_BIG_PACKETS += 1;
    }

    // printf("%llu\n", iter_cnt);
    finished = true;
    swapcontext_very_fast(cont, &uctx_main);
}

static inline void handle_fake_new_packet(void)
{
    int ret;
    struct mbuf *pkt;
    // struct custom_payload *req;
    struct db_req *req;

    pkt = (struct mbuf *)dispatcher_requests[cpu_nr_].mbuf;

    // req = mbuf_mtod(pkt, struct custom_payload *);
    req = mbuf_mtod(pkt, struct db_req *);

    if (req == NULL)
    {
        log_info("OOPS No Data\n");
        finished = true;
        return;
    }

    cont = (struct mbuf *)dispatcher_requests[cpu_nr_].rnbl;
    getcontext_fast(cont);
    set_context_link(cont, &uctx_main);

    // makecontext(cont, (void (*)(void))simple_generic_work, 2, req->ns, req->id);
    makecontext(cont, (void (*)(void))do_db_generic_work, 2, req, get_ns());

    finished = false;
    ret = swapcontext_very_fast(&uctx_main, cont);
    if (ret)
    {
        log_err("Failed to do swap into new context\n");
        exit(-1);
    }
}

static inline void handle_context(void)
{
    int ret;
    finished = false;
    cont = dispatcher_requests[cpu_nr_].rnbl;
    set_context_link(cont, &uctx_main);
    ret = swapcontext_fast(&uctx_main, cont);
    if (ret)
    {
        log_err("Failed to swap to existing context\n");
        exit(-1);
    }
}

static inline void handle_request(void)
{
    while (dispatcher_requests[cpu_nr_].flag == WAITING)
        ;
    dispatcher_requests[cpu_nr_].flag = WAITING;
    if (dispatcher_requests[cpu_nr_].category == PACKET)
        handle_new_packet();
    else
        handle_context();
}

bool IS_FIRST_PACKET = false;

static inline void handle_fake_request(void)
{
    while (dispatcher_requests[cpu_nr_].flag == WAITING)
        ;
    dispatcher_requests[cpu_nr_].flag = WAITING;
    if (dispatcher_requests[cpu_nr_].category == PACKET)
    {
        if (unlikely(!IS_FIRST_PACKET))
        {
            TEST_START_TIME = get_us();
            IS_FIRST_PACKET = true;
        }
        handle_fake_new_packet();
    }
    else
    {
        handle_context();
    }
}

static inline void finish_request(void)
{
    worker_responses[cpu_nr_].timestamp = dispatcher_requests[cpu_nr_].timestamp;
    worker_responses[cpu_nr_].type = dispatcher_requests[cpu_nr_].type;
    worker_responses[cpu_nr_].mbuf = dispatcher_requests[cpu_nr_].mbuf;
    worker_responses[cpu_nr_].rnbl = cont;
    worker_responses[cpu_nr_].category = CONTEXT;

    if (finished)
    {
        worker_responses[cpu_nr_].flag = FINISHED;
    }
    else
    {
        worker_responses[cpu_nr_].flag = PREEMPTED;
    }
}

void do_work(void)
{
    init_worker();
    log_info("do_work: Waiting for dispatcher work\n");

    // sure about vdso
    for (size_t i = 0; i < 50; i++)
        get_ns();

    while (true)
    {
        JOB_STARTED_AT = get_ns();

#ifdef FAKE_WORK
        handle_fake_request();
        fake_eth_process_send();
#else

        eth_process_reclaim();
        eth_process_send();
        handle_request();
#endif
        finish_request();
    }
}