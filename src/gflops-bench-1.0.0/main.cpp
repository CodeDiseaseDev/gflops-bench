#include <fstream>
#include <iostream>
#include <string.h>
#include <thread>
#include <immintrin.h>

#ifdef _WIN32
#include <cpuid.h>
#include <windows.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#include <sched.h>
#endif


#include "ArgParser.h"

ArgParser parser;

using Counter = std::atomic<uint64_t>;

struct AlignedValue {
  alignas(64) Counter value {0};
};

struct cpu_info {
  std::string name;
  int thread_count;
};

AlignedValue *values = nullptr;
size_t thread_count = 1;

std::atomic<bool> running = true;
int duration_s = 2;

bool brutal_mode = false;

bool vector_mode = false;


bool is_root() {
#ifdef _WIN32
  return true; // Windows: allow brutal mode unconditionally
#else
  return geteuid() == 0;
#endif
}

void set_thread_priority() {
#ifdef _WIN32
  HANDLE hThread = GetCurrentThread();
  if (!SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST)) {
    fprintf(stderr, "SetThreadPriority failed: %lu\n", GetLastError());
  }
#else
  sched_param sch_params;
  sch_params.sched_priority = 1;
  if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sch_params) != 0) {
    perror("pthread_setschedparam");
  }
#endif
}

void set_affinity(int i) {
#ifdef _WIN32
  DWORD_PTR mask = (1ull << i);
  SetThreadAffinityMask(GetCurrentThread(), mask);
#else
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(i, &cpuset);
  pthread_setaffinity_np(pthread_self(),
                         sizeof(cpu_set_t), &cpuset);
#endif
}



void vector_thread_entry_method(Counter* x, int i) {
  uint64_t local_x = 0;

  alignas(32) float a[8] = {1,2,3,4,5,6,7,8};
  alignas(32) float b[8] = {2,3,4,5,6,7,8,9};
  alignas(32) float c[8];

  while (running) {
    __m256 va = _mm256_load_ps(a);
    __m256 vb = _mm256_load_ps(b);
    __m256 vc = _mm256_mul_ps(va, vb);
    vc = _mm256_add_ps(vc, va);
    _mm256_store_ps(c, vc);

    local_x += 16;
  }

  (*x) += local_x;
}

void int_thread_entry_method(Counter* x, int i) {


  while (running) {
    (*x)++;
  }
}

void thread_entry(Counter *x, int i) {
  if (brutal_mode) {
    set_thread_priority();
    set_affinity(i);
  }



  if (vector_mode) {
    vector_thread_entry_method(x, i);
  }
  else {
    int_thread_entry_method(x, i);
  }
}

cpu_info get_cpu_info() {
  cpu_info info;
#ifdef _WIN32
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  info.thread_count = sysinfo.dwNumberOfProcessors;

  if (info.thread_count <= 0) {
    info.thread_count = std::thread::hardware_concurrency();
  }

  unsigned int eax, ebx, ecx, edx;
  char brand[0x40] = {0};

  for (unsigned int i = 0; i < 3; i++) {
    __get_cpuid(0x80000002 + i, &eax, &ebx, &ecx, &edx);
    memcpy(brand + i * 16 + 0,  &eax, 4);
    memcpy(brand + i * 16 + 4,  &ebx, 4);
    memcpy(brand + i * 16 + 8,  &ecx, 4);
    memcpy(brand + i * 16 + 12, &edx, 4);
  }

  info.name = brand[0] ? brand : "Unknown CPU";
  info.name.erase(0, info.name.find_first_not_of(' '));
  info.name.erase(info.name.find_last_not_of(' ') + 1);
#else
  info.thread_count = 0;
  std::ifstream cpuinfo("/proc/cpuinfo");
  std::string line;
  while (std::getline(cpuinfo, line)) {
    if (line.find("model name") != std::string::npos) {
      auto pos = line.find(':');
      if (pos != std::string::npos) {
        info.name = line.substr(pos + 2);
      }
    } else if (line.find("siblings") != std::string::npos) {
      auto pos = line.find(':');
      if (pos != std::string::npos) {
        info.thread_count = std::stoi(line.substr(pos + 2));
      }
    }
  }
#endif
  return info;
}


void init_arr() {
  values = new AlignedValue[thread_count];
  for (size_t i = 0; i < thread_count; i++) {
    values[i].value.store(0);
  }
}


void countdown(const int duration) {
  for (int i = 0; i < duration; i++) {
    printf("\rPerforming multi-core test. %is Left", duration - i);
    fflush(stdout);

    std::this_thread::sleep_for(
      std::chrono::seconds(1));
  }
  printf("\r");
}

void register_parameters() {
  parser.register_parameter({"--help", "-h", false});

  parser.register_parameter({"--threads", "-t", true});
  parser.register_parameter({"--duration", "-d", true});

  parser.register_parameter({"--brutal-mode", "-b", false});

  parser.register_parameter({"--vector", "-v", false});
}

int main(int argc, char** argv) {
  // set a higher process priority
  // if (setpriority(PRIO_PROCESS, 0, -40) == -1) {
  //   perror("setpriority");
  // }

  register_parameters();

  cpu_info cpu_info = get_cpu_info();
  printf("** %s **\n\n", cpu_info.name.c_str());

  thread_count = cpu_info.thread_count;

  parser.parse(argc, argv);

  if (parser.get("-h") || parser.get("--help")) {
    printf("\nUsage: %s [options]\n", argv[0]);
    printf("Options:\n");
    printf("\t-t | --threads <number of threads>\n");
    printf("\t-d | --duration <number of seconds>\n");
    printf("\t-b | --brutal  (Hits the CPU harder; requires root on Linux)\n");
    printf("\t-v | --vector  (Does vector calculations instead of addition)\n");
    printf("\t-h | --help\n\n");

    printf("Example: %s -d 15 -b -v\n\n", argv[0]);

    return 0;
  }


  brutal_mode = parser.get("--brutal-mode") || parser.get("-b");
  vector_mode = parser.get("--vector") || parser.get("-v");

  if (brutal_mode && !is_root()) {
    printf("Error: Brutal mode requires root privileges.\n\n");
    return 1;
  }


  const std::optional<std::string> threads_arg = (parser.get("--threads") ?
    parser.get("--threads") : parser.get("-t"));

  if (threads_arg.has_value()) {
    thread_count = std::stoi(threads_arg.value());
  }

  const std::optional<std::string> duration = (parser.get("--duration") ?
    parser.get("--duration") : parser.get("-d"));

  if (duration.has_value()) {
    duration_s = std::stoi(duration.value());
  }

  init_arr();



  const size_t t_count = thread_count;

  std::thread threads[thread_count];

  for (int i = 0; i < thread_count; i++) {
    threads[i] = std::thread(
      thread_entry, &values[i].value, i);

  }


  countdown(duration_s);
  running = false;

  unsigned long long total = 0;

  // printf("==================================\n");
  for (int i = 0; i < thread_count; i++) {
    unsigned long long value = values[i].value.load();
    unsigned long long normal = value / duration_s;

    total += normal;

    // printf("%2d: %llu %is\n", i, normal, duration_s);
    threads[i].join();
  }

  const unsigned long long avg = total / thread_count;

  const char* measurement = vector_mode ? "GFLOP/s" : "GOP/s";

  printf("Multi-threading Score (%i): %0.3f %s\n", thread_count, total / 1000000000.0f, measurement);
  printf("Average Single Thread Score: %0.3f %s\n", avg / 1000000000.0f, measurement);

  printf("\n");

  return 0;
}
