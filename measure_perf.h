#ifndef MEASUREPERF_H
#define MEASUREPERF_H

void measure_perf_init(const char* output_file);
void measure_perf_start();
void measure_perf_step(int step);
void measure_perf_end();
void measure_perf_write();

#endif

