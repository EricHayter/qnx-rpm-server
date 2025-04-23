#pragma once

#include <optional>
#include <string>
#include <sys/types.h>
#include <vector>

namespace qnx {
// Structure to hold basic process information for control operations
struct BasicProcessInfo {
  double cpu_usage;
  long memory_usage;
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
std::optional<BasicProcessInfo> getProcessInfo(pid_t pid);
} // namespace qnx