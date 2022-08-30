/* ---- BENCHMARK Parameters ---- */
// Benchmark Types:
// 1 -> %50 1us, %50 10us
// 2 -> %95	0.5us, %5 500us

// With Schedule:
// 0 -> Posted Interrupt (Default)
// 1 -> Yield
// 2 -> None (Cause Hol-block)
/* ----------------------------- */

#define BENCHMARK_NO_PACKETS          1000
#define BENCHMARK_TYPE       	        1
#define BENCHMARK_STOP_AT_PACKET      1000
#define BENCHMARK_STOP_AT_NS        1000000 * 20 
#define SCHEDULE_METHOD          METHOD_YIELD
#define DB_NO_KEY                   100000


// Schedule Methods
#define METHOD_PI       0
#define METHOD_YIELD    1
#define METHOD_NONE     2

// Define Packet Sizes
#if BENCHMARK_TYPE == 1
#define BENCHMARK_SMALL_PKT_NS 1  * 1000
#define BENCHMARK_BIG_PKT_NS   10 * 1000
#elif
#define BENCHMARK_SMALL_PKT_NS 0.5 * 1000
#define BENCHMARK_BIG_PKT      500 * 1000
#endif