#include "measure_perf.h"
#include <time.h>

int main() {
  measure_perf_init("measure_perf_test.txt");

  for (int i=0; i<20; i++) {
    measure_perf_start();

    volatile float a = 2.0;
    volatile float b = 3.0;
    volatile float c = a + b;
    measure_perf_step(0);

    volatile float d = a*a + b*b + c;
    measure_perf_step(1);

    struct timespec t = { 0, 10*1024*1024 };
    (void)nanosleep(&t, NULL);
    measure_perf_step(2);

    measure_perf_end();

    if (i == 2)
      measure_perf_write();
  }

  measure_perf_write();

  return 0;
}

