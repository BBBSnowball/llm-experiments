#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "measure_perf.h"

//NOTE 10 hardware counters seems to be the most that we can use before they are all zero.
const struct {
  int type; int config; const char* name; int flops; bool synthetic;
} event_ids[] = {
  // see https://man7.org/linux/man-pages/man2/perf_event_open.2.html
  //{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, "HW_CPU_CYCLES" },
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
  // (full name of these is "fp_arith_inst_retired.xxx")
  { PERF_TYPE_RAW, 0x3c7, "fp.scalar", 1 },
  //{ PERF_TYPE_RAW, 0xfcc7, "fp.vector" },
  //{ PERF_TYPE_RAW, 0xffc7, "fp.any" },
  { PERF_TYPE_RAW, 0x04c7, "fp.2_flops", 2 },
  { PERF_TYPE_RAW, 0x18c7, "fp.4_flops", 4 },
  { PERF_TYPE_RAW, 0x60c7, "fp.8_flops", 8 },
  { PERF_TYPE_RAW, 0x80c7, "fp.16_flops", 16 },
};
const char* event_synthetic_names[] = {
  "flops",
};
#define NUM_EVENTS (sizeof(event_ids)/sizeof(*event_ids))
#define NUM_SYNTHETIC_EVENTS (sizeof(event_synthetic_names)/sizeof(*event_synthetic_names))
#define NUM_ALL_EVENTS (NUM_EVENTS+NUM_SYNTHETIC_EVENTS)

typedef struct {
  uint64_t nr;
  uint64_t values[NUM_EVENTS];
} read_format_t;

typedef struct {
  uint64_t min, max, sum;
} event_info_t;

#define MAX_STEPS 2048

typedef struct {
  int pevfd;
  FILE* output_file;
  int prev_step;
  uint64_t start[NUM_EVENTS];
  uint64_t prev[NUM_EVENTS];
  uint64_t cnt[MAX_STEPS+2];
  event_info_t info[MAX_STEPS+2][NUM_ALL_EVENTS];
} state_t;
static state_t state;

void measure_perf_init(const char* output_file) {
  memset(&state, 0, sizeof(state));
  // This tells us that the state is invalid and it tells perf_event_open to open a new group.
  state.pevfd = -1;

  state.prev_step = -2;
  for (int i=0; i<MAX_STEPS+2; i++)
    for (int j=0; j<NUM_ALL_EVENTS; j++)
      state.info[i][j].min = UINT64_MAX;

  // based on https://stackoverflow.com/questions/65285636/libperf-perf-count-hw-cache-references
  // (but modified a lot...)
  struct perf_event_attr pea;
  memset(&pea, 0, sizeof(struct perf_event_attr));
  pea.size           = sizeof(struct perf_event_attr);
  pea.read_format    = PERF_FORMAT_GROUP;
  pea.inherit        = 1;  // would be useful but not compatible with PERF_FORMAT_GROUP -> says the man page but it does work
  //pea.pinned         = 1;  // error (EOF on read) if hardware counters aren't possible -> doesn't work
  // We will enable it before doing the work, so start disabled.
  pea.disabled       = 1;
  // Only count userspace, not kernel or hypervisor.
  // (We might like to include the kernel just in case it matters but we are not allowed to do that as a normal user.)
  pea.exclude_kernel = 1;
  pea.exclude_hv     = 1;
  pea.exclude_idle   = 1;

  for (int i=0; i<NUM_EVENTS; i++) {
    pea.type           = event_ids[i].type;
    pea.config         = event_ids[i].config;

    const int pid = 0;   // current process/thread
    const int cpu = -1;  // any CPU
    int fd = syscall(__NR_perf_event_open, &pea, pid, cpu, state.pevfd, 0);
    if (fd < 0) {
      printf("perf_event_open, i=%d -> %d\n", i, fd);
      perror("perf_event_open");
      close(state.pevfd);
      state.pevfd = -1;
      return;
    }
    if (i == 0)
      state.pevfd = fd;
  }

  state.output_file = fopen(output_file, "w");
  if (!state.output_file) {
    close(state.pevfd);
    state.pevfd = -1;
    return;
  }
}

static void update_info(int step, int event, uint64_t diff) {
  state.info[step][event].sum += diff;
  if (state.info[step][event].min > diff)
    state.info[step][event].min = diff;
  if (state.info[step][event].max < diff)
    state.info[step][event].max = diff;
}

static void update_infos(int step, const uint64_t prev[NUM_EVENTS], const read_format_t* current) {
  state.cnt[step] += 1;

  uint64_t flops = 0;
  for (int i=0; i<NUM_EVENTS && i < current->nr; i++) {
    uint64_t value = current->values[i];
    uint64_t diff = value - prev[i];
    state.prev[i] = value;
    flops += event_ids[i].flops * diff;
    if (step == -1) {
      state.start[i] = value;
    } else {
      update_info(step, i, diff);
    }
  }

  if (step >= 0) {
    update_info(step, NUM_EVENTS+0, flops);
  }
}

static void update_for_step(int step) {
  read_format_t rf;
  int x = read(state.pevfd, &rf, sizeof(rf));
  if (x < 0) {
    perror("measure_perf: read");
    close(state.pevfd);
    state.pevfd = -1;
    return;
  } else if (x != sizeof(rf)) {
    printf("WARN: measure_perf: wrong size in read: expected %ld but got %d\n", sizeof(rf), x);
    close(state.pevfd);
    state.pevfd = -1;
  }

  update_infos(step, state.prev, &rf);

  if (step == MAX_STEPS) {
    update_infos(step+1, state.start, &rf);
  }
}

void measure_perf_start() {
  if (state.pevfd < 0)
    return;
  if (state.prev_step != -2)
    printf("WARN: measure_perf_start: restart without calling measure_perf_end: previous step was %d\n", state.prev_step);

  state.prev_step = -1;
  update_for_step(-1);
  ioctl(state.pevfd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
}

void measure_perf_step(int step) {
  if (state.pevfd < 0)
    return;
  if (state.prev_step == -2)
    return;
  if (step != state.prev_step+1)
    printf("WARN: measure_perf_step: steps are not continuous: previous step was %d and current step is %d\n", state.prev_step, step);

  state.prev_step = step;

  if (step < 0 || step >= MAX_STEPS)
    return;

  update_for_step(step);
}

void measure_perf_end() {
  if (state.pevfd < 0)
    return;
  if (state.prev_step < 0)
    return;

  ioctl(state.pevfd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
  update_for_step(MAX_STEPS);
  state.prev_step = -2;
}

void measure_perf_write() {
  FILE* f = state.output_file;
  fprintf(f, "{");
  bool first = true;
  for (int step=0; step<MAX_STEPS+2; step++) {
    if (!state.cnt[step])
      continue;

    if (first)
      first = false;
    else
      fprintf(f, ",");
    fprintf(f, "\n");

    if (step == MAX_STEPS)
      fprintf(f, "  \"end\": {\n");
    else if (step == MAX_STEPS+1)
      fprintf(f, "  \"all\": {\n");
    else
      fprintf(f, "  \"%d\": {\n", step);

    for (int i=0; i<NUM_ALL_EVENTS; i++) {
      const char* name = i < NUM_EVENTS ? event_ids[i].name : event_synthetic_names[i-NUM_EVENTS];
      fprintf(f, "    \"%s\":%-*s {", name, 22-(int)strlen(name), "");
      fprintf(f, " \"min\": %14lu, ", state.info[step][i].min);
      fprintf(f, " \"avg\": %14lu, ", state.info[step][i].sum / state.cnt[step]);
      fprintf(f, " \"max\": %14lu ", state.info[step][i].max);
      fprintf(f, "},\n");
    }

    fprintf(f, "    \"count\": %lu\n", state.cnt[step]);
    fprintf(f, "  }");
  }
  fprintf(f, "\n");
  fprintf(f, "}\n\n");
  fflush(f);
}

