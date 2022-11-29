/* ---- BENCHMARK Parameters ---- */
// Benchmark Types:
// 1 -> %50 1us, %50 10us
// 2 -> %95	0.5us, %5 500us

// With Schedule:
// 0 -> Posted Interrupt (Default)
// 1 -> Yield
// 2 -> None (Cause Hol-block)
/* ----------------------------- */

#define BENCHMARK_STOP_AT_PACKET     1000000000000
#define BENCHMARK_DURATION_US        1000000 * 10 
#define SCHEDULE_METHOD              METHOD_CONCORD
#define DB_NUM_KEYS                  15000

// Set to -1 to run in infinite loop
#define BENCHMARK_CREATE_NO_PACKET   -1

// Schedule Methods
#define METHOD_PI       0
#define METHOD_YIELD    1
#define METHOD_NONE     2
#define METHOD_CONCORD  3

// Debug Methods
#define LATENCY_DEBUG   1

// Dispatcher do work
#define DISPATCHER_DO_WORK 1

// If 0, runs leveldb. If 1 runs simpleloop
#define RUN_UBENCH      0  

#if RUN_UBENCH == 1
    // Different workload mixes 
    #define BENCHMARK_TYPE    1 
    #if BENCHMARK_TYPE == 0                      // 100% 100us.
    #define BENCHMARK_SMALL_PKT_SPIN   62   
    #define BENCHMARK_SMALL_PKT_NS     1000
    #define BENCHMARK_LARGE_PKT_SPIN   6200  
    #define BENCHMARK_LARGE_PKT_NS     100000
    #define MU                         0.01
    #elif  BENCHMARK_TYPE == 1                  // 50% 1us, 50% 100us
    #define BENCHMARK_SMALL_PKT_SPIN   62   
    #define BENCHMARK_SMALL_PKT_NS     1000
    #define BENCHMARK_LARGE_PKT_SPIN   6200  
    #define BENCHMARK_LARGE_PKT_NS     100000
    #define MU                         0.0198               
    #elif  BENCHMARK_TYPE == 2                  // 99.5% 0.5us, 0.5% 500us
    #define BENCHMARK_SMALL_PKT_SPIN   27 
    #define BENCHMARK_SMALL_PKT_NS     500
    #define BENCHMARK_LARGE_PKT_SPIN   30000 
    #define BENCHMARK_LARGE_PKT_NS     500000
    #define MU                         0.333611
    #endif
#else
    #define BENCHMARK_TYPE 1 // Always. We only work with a 50-50 split
    #define BENCHMARK_SMALL_PKT_SPIN   27 
    #define BENCHMARK_SMALL_PKT_NS     1500  // Random get costs 1.5us
    #define BENCHMARK_LARGE_PKT_SPIN   30000 
    #define BENCHMARK_LARGE_PKT_NS     644000 // Scan of 15k keys costs ~644us
    #define MU                         0.0030511
#endif