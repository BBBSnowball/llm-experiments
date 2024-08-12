// based on https://stackoverflow.com/questions/65285636/libperf-perf-count-hw-cache-references

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>

//NOTE 10 hardware counters seems to be the most that we can use before they are all zero.
const struct { int type; int config; const char* name; int flops; } event_ids[] = {
  // see https://man7.org/linux/man-pages/man2/perf_event_open.2.html
  { PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, "HW_CPU_CYCLES" },
  { PERF_TYPE_HARDWARE, PERF_COUNT_HW_REF_CPU_CYCLES, "HW_REF_CPU_CYCLES" },
  { PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, "HW_INSTRUCTIONS" },
  { PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES, "HW_CACHE_REFERENCES" },
  { PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES, "HW_CACHE_MISSES" },
  { PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS, "HW_BRANCH_INSTRUCTIONS" },
  //{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND },
  //{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND },
  { PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS, "SW_CPU_MIGRATIONS" },
  { PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN, "SW_PAGE_FAULTS_MIN" },
  { PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ, "SW_PAGE_FAULTS_MAJ" },
  //{ PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_READ<<8) | (PERF_COUNT_HW_CACHE_RESULT_MISS<<16), "HW_CACHE_MISS_READ" },
  //{ PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_PREFETCH<<8) | (PERF_COUNT_HW_CACHE_RESULT_MISS<<16) },
  // see `perf list --details -v`, use `event | (umask<<8)`
  { PERF_TYPE_RAW, 0x3c7, "fp_arith_inst_retired.scalar", 1 },
  //{ PERF_TYPE_RAW, 0xfcc7, "fp_arith_inst_retired.vector" },
  //{ PERF_TYPE_RAW, 0xffc7, "fp_arith_inst_retired.any" },
  { PERF_TYPE_RAW, 0x04c7, "fp_arith_inst_retired.2_flops", 2 },
  { PERF_TYPE_RAW, 0x18c7, "fp_arith_inst_retired.4_flops", 4 },
  { PERF_TYPE_RAW, 0x60c7, "fp_arith_inst_retired.8_flops", 8 },
  //{ PERF_TYPE_RAW, 0x80c7, "fp_arith_inst_retired.16_flops", 16 },
};
#define NUM_EVENTS (sizeof(event_ids)/sizeof(*event_ids))

struct read_format {
  uint64_t nr;
  struct {
    uint64_t value;
    uint64_t id;
  } values[NUM_EVENTS];
};

struct {
  int pevfd;
  int ids[NUM_EVENTS];
} perf_ids;

static void pevnt_init() {
  struct perf_event_attr pea;
  int                    fd1 = -1;
  int                    fd2 = -1;
  int                    tid = syscall(__NR_gettid);
  int cpu_id = -1;

  for (int i=0; i<NUM_EVENTS; i++) {
    memset(&pea, 0, sizeof(struct perf_event_attr));
    pea.type           = event_ids[i].type;
    pea.size           = sizeof(struct perf_event_attr);
    pea.config         = event_ids[i].config;
    pea.disabled       = 1;
    //pea.pinned         = 1;
    pea.exclude_kernel = 1;
    pea.exclude_hv     = 1;
    pea.exclude_idle   = 1;
    pea.read_format    = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

    int fd = syscall(__NR_perf_event_open, &pea, tid, cpu_id, fd1, 0);
    if (fd < 0) {
      printf("perf_event_open, i=%d -> %d\n", i, fd);
      perror("perf_event_open");
      exit(1);
    }
    if (i == 0)
      perf_ids.pevfd = fd1 = fd;
    ioctl(fd, PERF_EVENT_IOC_ID, &perf_ids.ids[i]);
  }
}

int main() {
  pevnt_init();
  ioctl(perf_ids.pevfd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);

  static volatile char buf[100*1024*1024*2];
  memset((void*)buf, 42, sizeof(buf));
  char sum = 0;
  for (int i=0; i<sizeof(buf); i++)
    sum += buf[i];
  printf("sum: %d\r\n", sum);
  volatile float a = 42.0;
  volatile float b = 23.0;
  volatile float c = a+b*b;

  //ioctl(perf_ids.pevfd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
  struct read_format rf, rf2;
  int x = read(perf_ids.pevfd, &rf, sizeof(rf));
  int x2 = read(perf_ids.pevfd, &rf2, sizeof(rf2));
  if (x < 0)
    perror("ERROR: read failed");
  printf("status=%d, nr=%ld\n", x, rf.nr);
  uint64_t flops = 0;
  for (int i=0; i<NUM_EVENTS; i++) {
    printf("  %10ld  %s\n",
        rf.values[i].value, rf.values[i].id == perf_ids.ids[i] ? event_ids[i].name : "");
    if (event_ids[i].flops)
      flops += event_ids[i].flops * rf.values[i].value;
  }
  if (x2 < 0)
    perror("ERROR: read failed");
  printf("status=%d, nr=%ld\n", x2, rf2.nr);
  uint64_t flops2 = 0;
  for (int i=0; i<NUM_EVENTS; i++) {
    printf("  %10lu, +%10lu, %s\n",
        rf2.values[i].value, rf2.values[i].value-rf.values[i].value, rf2.values[i].id == perf_ids.ids[i] ? event_ids[i].name : "");
    if (event_ids[i].flops)
      flops2 += event_ids[i].flops * rf2.values[i].value;
  }
  printf("  %10lu, +%10lu, flops\n",
      flops2, flops2-flops);

  return 0;
}

