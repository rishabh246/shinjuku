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
#define DB_NO_KEY                   100000


// Schedule Methods
#define METHOD_PI       0
#define METHOD_YIELD    1
#define METHOD_NONE     2
#define METHOD_CONCORD  3

// Different workload mixes 
#define BENCHMARK_TYPE       	        1 

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