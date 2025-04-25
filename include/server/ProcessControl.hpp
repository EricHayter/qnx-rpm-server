#pragma once

#include <optional>
#include <string>
#include <sys/types.h>
#include <vector>

#ifdef __QNXNTO__
#include <unistd.h> // Required for pid_t
#else
typedef int pid_t;
#endif

namespace qnx {
// Structure to hold basic process information for control operations
struct BasicProcessInfo {
  pid_t pid;
  pid_t parentPid;
  int state; // Process state (e.g., STATE_RUNNING)
  uid_t uid;
  gid_t gid;
  int priority;
  int threads;      // Number of threads
  long memoryUsage; // Memory usage (e.g., VmSize)
  double cpu_usage; // CPU usage (needs calculation)
  std::string commandLine;
  std::optional<std::string> workingDirectory; // Can be optional
};

// Function declarations (now directly under qnx)
bool sendSignal(pid_t pid, int signal);
bool suspend(pid_t pid);
bool resume(pid_t pid);
bool terminate(pid_t pid);
bool exists(pid_t pid);
std::optional<pid_t> getParentPid(pid_t pid);
std::vector<pid_t> getChildProcesses(pid_t pid);
std::string getCommandLine(pid_t pid);
std::optional<std::string> getWorkingDirectory(pid_t pid);
std::optional<std::string> getProcessExecutablePath(pid_t pid);
} // namespace qnx