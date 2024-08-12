// based on https://stackoverflow.com/questions/65285636/libperf-perf-count-hw-cache-references

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>

struct read_format {
  uint64_t nr;
  struct {
    uint64_t value;
    uint64_t id;
  } values[ 2 ];
};

struct {
  int pevfd;
  int id_cache_refs;
  int id_cache_misses;
} perf_ids[8];

static void pevnt_init(int cpu_id) {
  struct perf_event_attr pea;
  int                    fd1 = -1;
  int                    fd2 = -1;
  int                    tid = syscall(__NR_gettid);

  memset(&pea, 0, sizeof(struct perf_event_attr));
  pea.type           = PERF_TYPE_HARDWARE;
  pea.size           = sizeof(struct perf_event_attr);
  pea.config         = PERF_COUNT_HW_CACHE_REFERENCES;
  pea.disabled       = 1;
  pea.pinned         = 1;
  pea.exclude_kernel = 1;
  pea.exclude_hv     = 1;
  pea.exclude_idle   = 1;
  pea.read_format    = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

  fd1 = syscall(__NR_perf_event_open, &pea, tid, cpu_id, -1, 0);
  if (fd1 != -1) {
    ioctl(fd1, PERF_EVENT_IOC_ID, &perf_ids[ cpu_id].id_cache_refs);

    memset(&pea, 0, sizeof(struct perf_event_attr));
    pea.type           = PERF_TYPE_HARDWARE;
    pea.size           = sizeof(struct perf_event_attr);
    pea.config         = PERF_COUNT_HW_CACHE_MISSES;
    pea.disabled       = 1;
    pea.exclude_kernel = 1;
    pea.exclude_hv     = 1;
    pea.exclude_idle   = 1;
    pea.read_format    = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

    fd2 = syscall(__NR_perf_event_open, &pea, tid, cpu_id, fd1, 0);
    if (fd2 != -1) {
      ioctl(fd2, PERF_EVENT_IOC_ID, &perf_ids[ cpu_id].id_cache_misses);
    }
  }
  printf("perf_event_open :: %6d :: %2d :: %5d :: %5d\n", tid, cpu_id, fd1, fd2); 

  perf_ids[ cpu_id ].pevfd = fd1;
}

int min(int a, int b) { return (a <= b ? a : b); }

static int num_procs;

int main() {
  num_procs = min(get_nprocs(), sizeof(perf_ids)/sizeof(*perf_ids));
  for (int i=0; i<num_procs; i++)
    pevnt_init(i);
  for (int i=0; i<num_procs; i++)
    ioctl(perf_ids[i].pevfd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);

  static volatile char buf[100*1024*1024];
  memset((void*)buf, 42, sizeof(buf));
  char sum = 0;
  for (int i=0; i<sizeof(buf); i++)
    sum += buf[i];
  printf("sum: %d\r\n", sum);

  for (int i=0; i<num_procs; i++)
    ioctl(perf_ids[i].pevfd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
  for (int i=0; i<num_procs; i++) {
    struct read_format rf;
    int x = read(perf_ids[i].pevfd, &rf, sizeof(rf));
    if (x < 0)
      perror("ERROR: read failed");
    printf("CPU %d: status=%d, nr=%ld, value1=%10ld, id1=%ld, value2=%10ld, id2=%ld\n",
        i, x, rf.nr, rf.values[0].value, rf.values[0].id, rf.values[1].value, rf.values[2].id);
  }

  return 0;
}

