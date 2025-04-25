#ifndef QNX_PROCESS_CORE_HPP
#define QNX_PROCESS_CORE_HPP

/**
 * @file ProcessCore.hpp
 * @brief Process Management Core Module for QNX Process Monitor
 *
 * This header defines the interface for the process management core module,
 * which provides functionality for monitoring and managing processes in the
 * system.
 */

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// POSIX headers
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// QNX-specific headers
#ifdef __QNXNTO__
#include <sys/dcmd_proc.h>
#include <sys/netmgr.h>
#include <sys/neutrino.h>
#include <sys/procfs.h>
#include <sys/sched.h>
#endif

namespace qnx {

/**
 * @class ProcessInfo
 * @brief Struct representing process information
 */
struct ProcessInfo {
  pid_t pid;
  pid_t parent_pid;
  std::string name;
  int group_id;
  uint64_t memory_usage;
  double cpu_usage;
  int priority;
  int policy;
  int num_threads;
  int state;
};

/**
 * @class ProcessCore
 * @brief Core class for process management functionality
 */
class ProcessCore {
public:
  static ProcessCore &getInstance();

  // Delete copy constructor and assignment operator
  ProcessCore(const ProcessCore &) = delete;
  ProcessCore &operator=(const ProcessCore &) = delete;

  // Construction and destruction handle setup/teardown automatically

  // Process information collection
  std::optional<int> collectInfo();

  // Process information retrieval
  size_t getCount() const noexcept;
  const std::vector<ProcessInfo> &getProcessList() const noexcept;
  std::optional<ProcessInfo> getProcessById(pid_t pid) const noexcept;

  // Process control
  bool adjustPriority(pid_t pid, int priority, int policy);

  // Display
  void displayInfo() const;

private:
  ProcessCore();
  ~ProcessCore() = default;

  // Helper methods
  bool readProcessInfo(pid_t pid, ProcessInfo &info,
                       std::chrono::duration<double> elapsed_time);
  bool readProcessMemory(pid_t pid, ProcessInfo &info);
  std::optional<uint64_t> readProcessStatus(pid_t pid, ProcessInfo &info);

  std::vector<ProcessInfo> process_list_;
  mutable std::mutex mutex_;
  std::chrono::steady_clock::time_point last_update_time_;
  std::unordered_map<pid_t, uint64_t> last_sutimes_;
};

} // namespace qnx

#endif /* QNX_PROCESS_CORE_HPP */
