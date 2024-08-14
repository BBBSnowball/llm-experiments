#ifndef MEASUREPERF_H
#define MEASUREPERF_H

#ifdef __cplusplus
extern "C" {
#endif

void measure_perf_init(const char* output_file, bool force);
void measure_perf_start(void);
void measure_perf_step(int step);
void measure_perf_end(void);
void measure_perf_write(void);

#ifdef __cplusplus
}
#endif

#endif

