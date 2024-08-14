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

//NOTE 10 hardware counters seems to be the most that we can use before they are all zero.
const struct { int type; int config; const char* name; int flops; } event_ids[] = {
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
  { PERF_TYPE_RAW, 0x3c7, "fp_arith_inst_retired.scalar", 1 },
  //{ PERF_TYPE_RAW, 0xfcc7, "fp_arith_inst_retired.vector" },
  //{ PERF_TYPE_RAW, 0xffc7, "fp_arith_inst_retired.any" },
  { PERF_TYPE_RAW, 0x04c7, "fp_arith_inst_retired.2_flops", 2 },
  { PERF_TYPE_RAW, 0x18c7, "fp_arith_inst_retired.4_flops", 4 },
  { PERF_TYPE_RAW, 0x60c7, "fp_arith_inst_retired.8_flops", 8 },
  { PERF_TYPE_RAW, 0x80c7, "fp_arith_inst_retired.16_flops", 16 },
};
#define NUM_EVENTS (sizeof(event_ids)/sizeof(*event_ids))

struct read_format {
  uint64_t nr;
  uint64_t values[NUM_EVENTS];
};

int pevfd;

// based on https://stackoverflow.com/questions/65285636/libperf-perf-count-hw-cache-references
// (but modified a lot...)
static void pevnt_init() {
  struct perf_event_attr pea;
  int fd_group = -1;

  for (int i=0; i<NUM_EVENTS; i++) {
    memset(&pea, 0, sizeof(struct perf_event_attr));
    pea.size           = sizeof(struct perf_event_attr);
    pea.type           = event_ids[i].type;
    pea.config         = event_ids[i].config;
    pea.read_format    = PERF_FORMAT_GROUP;
    pea.inherit        = 1;  // would be useful but not compatible with PERF_FORMAT_GROUP -> says the man page but it does work
    //pea.pinned         = 1;  // error (EOF on read) if hardware counters aren't possible -> doesn't work
    // We will enable it before doing the work, so start disabled.
    pea.disabled       = 1;
    // Only count userspace, not kernel or hypervisor.
    // (We might like to include the kernel just in case it matters but we are not allowed to do that as a normal user.)
    const bool withKernel = false;
    pea.exclude_kernel = withKernel ? 0 : 1;
    pea.exclude_hv     = withKernel ? 0 : 1;
    pea.exclude_idle   = 1;

    const int pid = 0;   // current process/thread
    const int cpu = -1;  // any CPU
    int fd = syscall(__NR_perf_event_open, &pea, pid, cpu, fd_group, 0);
    if (fd < 0) {
      printf("perf_event_open, i=%d -> %d\n", i, fd);
      perror("perf_event_open");
      exit(1);
    }
    if (i == 0)
      pevfd = fd_group = fd;
  }
}

static void prepare();
static void work();
static void cleanup();

void print(const struct read_format* current, const struct read_format* prev) {
  uint64_t flops = 0;
  uint64_t flops2 = 0;
  printf("counters:\n");
  for (int i=0; i<NUM_EVENTS && i < current->nr; i++) {
    printf("  %12lu, +%12lu, %s\n",
        current->values[i], prev ? current->values[i] - prev->values[i] : 0, event_ids[i].name);
    if (event_ids[i].flops) {
      if (prev)
        flops += event_ids[i].flops * prev->values[i];
      flops2 += event_ids[i].flops * current->values[i];
    }
  }
  printf("  %12lu, +%12lu, flops\n",
      flops2, prev ? flops2-flops : 0);
}

int main() {
  pevnt_init();

  prepare();

  ioctl(pevfd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);

  work();

  struct read_format rf, rf2;
  int x = read(pevfd, &rf, sizeof(rf));
  int x2 = read(pevfd, &rf2, sizeof(rf2));
  if (x < 0 || x2 < 0)
    perror("ERROR: read failed");
  else if (x == 0 || x2 == 0) {
    printf("ERROR: read() has returned EOF. We have probably asked for too many hardware counters.\n");
    return 1;
  }

  work();
  struct read_format rf3;
  int x3 = read(pevfd, &rf3, sizeof(rf3));

  ioctl(pevfd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);

  //printf("status=%d, nr=%ld\n", x, rf.nr);
  print(&rf, 0);
  print(&rf2, &rf);
  print(&rf3, &rf2);

  ioctl(pevfd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
  int child = fork();
  int child2;
  if (child > 0)
    child2 = fork();
  if (child == 0 || child2 == 0) {
    work();

    printf("in child:\n");
    struct read_format rf5;
    int x5 = read(pevfd, &rf5, sizeof(rf5));
    print(&rf5, &rf3);
    return 0;
  } else {
    waitpid(child, NULL, 0);
    waitpid(child2, NULL, 0);
  }
  ioctl(pevfd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);

  printf("in parent:\n");
  struct read_format rf4;
  int x4 = read(pevfd, &rf4, sizeof(rf4));
  print(&rf4, &rf3);

  cleanup();

  return 0;
}

#include "./llama.cpp/ggml/include/ggml.h"

static struct ggml_context * ctx;
struct ggml_tensor *x, *a, *b, *f;
struct ggml_cgraph * gf;

static void fill_tensor(struct ggml_tensor *x) {
  for (int i0=0; i0<x->ne[0]; i0++) {
    for (int i1=0; i1<x->ne[1]; i1++) {
      for (int i2=0; i2<x->ne[2]; i2++) {
        for (int i3=0; i3<x->ne[3]; i3++) {
          ggml_set_f32_nd(x, i0, i1, i2, i3,
              //i0*-37+i1*23+i2*51-3*i3);
              i0*3-i1*2-1);
        }
      }
    }
  }
}

static void prepare() {
  struct ggml_init_params params = {
    .mem_size   = 600*1024*1024,
    .mem_buffer = NULL,
  };
  ctx = ggml_init(params);

  //const int type = GGML_TYPE_F32;
  const int type = GGML_TYPE_F16;
  //const int type = GGML_TYPE_I32;

  const int ne0 = 12288;
  //const int ne0 = 1000;
  //const int ne0 = 10;

  x = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 1024, 4);

  ggml_set_param(ctx, x); // x is an input variable
  a  = ggml_new_tensor_2d(ctx, type, ne0, 1024);
  b  = ggml_new_tensor_2d(ctx, type, ne0, 4);
  ggml_set_param(ctx, a);
  ggml_set_param(ctx, b);
  struct ggml_tensor * x2 = ggml_mul_mat(ctx, a, b);
  //f  = ggml_add(ctx, ggml_mul(ctx, x, x2), x);
  //f  = ggml_mul(ctx, x, x2);
  f = x2;
  // How many FLOPS do we expect?
  // 12288*1024*4 mul+add, 1024*4 mul, 1024*4 add -> 1e8 = 100M
  // -> We get 100.9M and we expect 100.7M, which is quite close.

  fill_tensor(a);
  fill_tensor(b);
  fill_tensor(x);

  gf = ggml_new_graph(ctx);
  ggml_build_forward_expand(gf, f);
}

static void cleanup() {
}

static void work() {
  if (0) {
    static volatile char buf[100*1024*1024*2];
    memset((void*)buf, 42, sizeof(buf));
    char sum = 0;
    for (int i=0; i<sizeof(buf); i++)
      sum += buf[i];
    printf("sum: %d\r\n", sum);
  }
  if (0) {
    volatile float a = 42.0;
    volatile float b = 23.0;
    volatile float c = a+b*b;
  }

  // set the input variable and parameter values
  //ggml_set_f32(x, 2.0f);
  //ggml_set_f32(a, 3.0f);
  //ggml_set_f32(b, 4.0f);

  ggml_graph_compute_with_ctx(ctx, gf, 1);
  
  printf("f = %f\n", ggml_get_f32_1d(f, 0));
}
