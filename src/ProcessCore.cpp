/**
 * @file ProcessCore.cpp
 * @brief Implementation of the Process Management Core for QNX Remote Process
 * Monitor
 *
 * This file implements the core functionality for process monitoring and
 * management in the QNX environment. It provides methods for collecting process
 * information, tracking resource usage, and managing process priorities.
 *
 * The implementation uses QNX-specific APIs where possible, with fallbacks for
 * non-QNX systems when appropriate. It interacts with the /proc filesystem to
 * gather detailed information about running processes.
 */

#include "ProcessCore.hpp"
#include "ProcessControl.hpp"
#include <chrono> // Added for time points and durations
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional> // for std::optional
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map> // Added for tracking CPU times
#include <unordered_set>
#include <vector>

// POSIX headers for open/close and error reporting
#include <cerrno>   // for errno
#include <cstring>  // for strerror()
#include <fcntl.h>  // for O_RDONLY
#include <unistd.h> // for open(), close()

#ifdef __QNXNTO__
#include <devctl.h>        // devctl(), DCMD_PROC_*
#include <sys/dcmd_proc.h> // Added for DCMD_PROC_* definitions
#include <sys/neutrino.h>  // QNX-specific
#include <sys/procfs.h>    // procfs_status, debug_process_t, debug_thread_t
#include <sys/syspage.h>
// Explicitly declare POSIX functions sometimes hidden in C++ on QNX
extern "C" {
int open(const char *pathname, int flags, ...);
int close(int fd);
}
#endif

namespace qnx {
/**
 * @brief Constructor: Initializes the last update time.
 */
ProcessCore::ProcessCore()
    : last_update_time_(std::chrono::steady_clock::now()) {}

/**
 * @brief Get the singleton instance of the ProcessCore class
 *
 * This method implements the singleton pattern to ensure only one instance
 * of ProcessCore exists throughout the application lifetime. It is thread-safe
 * due to the static local variable initialization guarantees of C++11.
 *
 * @return Reference to the singleton ProcessCore instance
 */
ProcessCore &ProcessCore::getInstance() {
  static ProcessCore instance;
  return instance;
}

/**
 * @brief Collect information about all running processes in the system
 *
 * This method traverses the /proc filesystem to gather information about
 * all currently running processes. For each valid process directory found,
 * it attempts to read detailed process information and adds it to the
 * internal process list.
 *
 * Thread-safe through mutex locking of the process list.
 *
 * @return The number of processes collected, or -1 if an error occurred
 */
std::optional<int> ProcessCore::collectInfo() {
  std::lock_guard<std::mutex> lock(mutex_);
  process_list_.clear();
  std::unordered_set<pid_t> current_pids;

  // --- CPU Calculation Setup ---
  auto now = std::chrono::steady_clock::now();
  auto elapsed_time = now - last_update_time_;
  // Avoid division by zero or tiny intervals
  if (elapsed_time < std::chrono::milliseconds{1}) {
    elapsed_time = std::chrono::milliseconds{1};
  }
  // --- End CPU Calculation Setup ---

  try {
    const std::filesystem::path proc_path("/proc");
    if (!std::filesystem::exists(proc_path)) {
      throw std::runtime_error("Proc filesystem not found");
    }

    for (const auto &entry : std::filesystem::directory_iterator(proc_path)) {
      if (!entry.is_directory())
        continue;

      const std::string &name = entry.path().filename().string();
      if (name.empty() || !std::isdigit(name[0]))
        continue;

      try {
        pid_t pid = std::stoi(name);
        ProcessInfo info;
        current_pids.insert(pid);

        // Pass elapsed time for CPU calculation
        if (readProcessInfo(pid, info, elapsed_time)) {
          process_list_.push_back(std::move(info));
        }
      } catch (const std::exception &e) {
        std::cerr << "Error processing PID " << name << ": " << e.what()
                  << std::endl;
        continue;
      }
    }
    // --- Prune old PIDs from CPU tracking maps ---
    for (auto it = last_sutimes_.begin(); it != last_sutimes_.end();
         /* no increment */) {
      if (current_pids.find(it->first) == current_pids.end()) {
        it = last_sutimes_.erase(it); // Erase and get next iterator
      } else {
        ++it;
      }
    }
    // --- End Pruning ---

    // Update last global update time for next cycle's CPU calculation
    last_update_time_ = now;

    return std::make_optional(static_cast<int>(process_list_.size()));
  } catch (const std::exception &e) {
    std::cerr << "Error collecting process information: " << e.what()
              << std::endl;
    return std::nullopt;
  }
}

/**
 * @brief Get the count of currently tracked processes
 *
 * Returns the number of ProcessInfo objects currently stored in the internal
 * process list. This method is thread-safe and locks the process list while
 * counting.
 *
 * @return The number of processes in the internal process list
 */
size_t ProcessCore::getCount() const noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  return process_list_.size();
}

/**
 * @brief Get the list of all currently tracked processes
 *
 * Returns a reference to the internal list of ProcessInfo objects. The list
 * contains information about all processes that were found during the last
 * call to collectInfo().
 *
 * @note This method does not refresh the process list. Call collectInfo() first
 * to get the latest information.
 *
 * @return Const reference to the internal vector of ProcessInfo objects
 */
const std::vector<ProcessInfo> &ProcessCore::getProcessList() const noexcept {
  return process_list_;
}

/**
 * @brief Find a specific process by its PID
 *
 * Searches the internal process list for a process with the specified PID and
 * returns its information if found. The method is thread-safe through mutex
 * locking.
 *
 * @param pid The process ID to search for
 * @return An optional containing the ProcessInfo if found, or empty if not
 * found
 */
std::optional<ProcessInfo>
ProcessCore::getProcessById(pid_t pid) const noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it =
      std::find_if(process_list_.begin(), process_list_.end(),
                   [pid](const ProcessInfo &info) { return info.pid == pid; });

  if (it != process_list_.end()) {
    return std::make_optional(*it);
  }
  return {};
}

/**
 * @brief Change the priority and scheduling policy of a process
 *
 * Attempts to modify the specified process's scheduling priority and policy.
 * This operation requires appropriate privileges to succeed.
 *
 * @param pid The process ID to modify
 * @param priority The new priority value to set
 * @param policy The new scheduling policy to set
 * @return true if the priority was successfully adjusted, false otherwise
 */
bool ProcessCore::adjustPriority(pid_t pid, int priority, int policy) {
#ifdef __QNXNTO__
  struct sched_param param;
  param.sched_priority = priority;

  if (sched_setscheduler(pid, policy, &param) == -1) {
    std::error_code ec(errno, std::system_category());
    std::cerr << "Failed to adjust priority for PID " << pid << ": "
              << ec.message() << std::endl;
    return false;
  }
  return true;
#else
  std::cerr << "Priority adjustment not supported on non-QNX systems"
            << std::endl;
  return false;
#endif
}

/**
 * @brief Display process information in a formatted table
 *
 * Prints a summary of all tracked processes to standard output in a
 * formatted table. This is useful for debugging and console output.
 * The output includes PID, name, memory usage, CPU usage, thread count,
 * priority, and policy information.
 */
void ProcessCore::displayInfo() const {
  std::lock_guard<std::mutex> lock(mutex_);

  // Print header
  std::cout << std::setw(8) << "PID" << std::setw(20) << "Name" << std::setw(12)
            << "Memory(KB)" << std::setw(10) << "CPU%" << std::setw(8)
            << "Threads" << std::setw(10) << "Priority" << std::setw(15)
            << "Policy" << std::endl;
  std::cout << std::string(83, '-') << std::endl;

  // Print process information
  for (const auto &proc : process_list_) {
    std::cout << std::setw(8) << proc.pid << std::setw(20) << proc.name
              << std::setw(12) << proc.memory_usage / 1024 << std::setw(10)
              << std::fixed << std::setprecision(1) << proc.cpu_usage
              << std::setw(8) << proc.num_threads << std::setw(10)
              << proc.priority << std::setw(15) << proc.policy << std::endl;
  }
}

/**
 * @brief Read detailed information for a specific process by PID.
 *
 * This method aggregates information by calling helper functions to read
 * memory usage and status details (including data needed for CPU calculation).
 * It then calculates the CPU usage based on the change in `sutime` over the
 * elapsed time since the last update.
 *
 * @param pid The process ID to read information for.
 * @param info Reference to a ProcessInfo struct to populate.
 * @param elapsed_time The time elapsed since the last global process list
 * update.
 * @return true if process information was successfully read, false otherwise.
 */
bool ProcessCore::readProcessInfo(pid_t pid, ProcessInfo &info,
                                  std::chrono::duration<double> elapsed_time) {
#ifdef __QNXNTO__
  // Get status info (including parent_pid, num_threads, priority, policy,
  // state, sutime)
  std::optional<uint64_t> current_sutime_opt = readProcessStatus(pid, info);

  if (current_sutime_opt) {
    // Get the process name separately
    if (auto path_opt = getProcessExecutablePath(pid)) {
      info.name = *path_opt;
    } else {
      // Fallback or use cmdline?
      info.name = getCommandLine(pid); // Try cmdline if path fails
      if (info.name.empty()) {
        info.name = "N/A"; // Default if both fail
      } else {
        // Often cmdline has args, take first part
        size_t first_space = info.name.find(' ');
        if (first_space != std::string::npos) {
          info.name = info.name.substr(0, first_space);
        }
      }
    }

    // Read memory info only if status read was successful
    if (!readProcessMemory(pid, info)) {
      // Handle memory read failure? Log? Set memory to 0?
      info.memory_usage = 0;
    }

    // --- Calculate CPU Usage ---
    uint64_t current_sutime = *current_sutime_opt;
    // Get num_cpu directly from the main syspage structure
    int num_cpus =
        _syspage_ptr->num_cpu; // Get number of CPUs directly from syspage
    double elapsed_nanos =
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed_time)
            .count();

    auto it = last_sutimes_.find(pid);
    if (it != last_sutimes_.end()) // Check if we have previous data
    {
      uint64_t last_sutime = it->second;
      uint64_t sutime_delta =
          (current_sutime >= last_sutime)
              ? (current_sutime - last_sutime)
              : current_sutime; // Handle potential wraparound or reset? For now
                                // assume monotonic increase.

      if (elapsed_nanos > 0 && num_cpus > 0) {
        // CPU% = (change in process time / change in wall time) / num_cpus *
        // 100
        info.cpu_usage =
            (static_cast<double>(sutime_delta) / elapsed_nanos) * 100.0;
        // Optional: Cap at num_cpus * 100% if needed, though unlikely with
        // sutime info.cpu_usage = std::min(info.cpu_usage,
        // static_cast<double>(num_cpus * 100.0));
      } else {
        info.cpu_usage = 0.0; // Avoid division by zero
      }
    } else {
      info.cpu_usage =
          0.0; // First time seeing this process, cannot calculate delta
    }
    // Update the last known sutime for this process
    last_sutimes_[pid] = current_sutime;
    // --- End CPU Calculation ---

    return true; // Successfully read status and calculated CPU
  } else {
    // Status read failed, cannot proceed
    info.name = "N/A"; // Set default name on status failure
    info.cpu_usage = 0.0;
    info.memory_usage = 0;
    // Ensure PID is removed from tracking if status fails
    last_sutimes_.erase(pid);
    return false;
  }

#else
  // Non-QNX fallback (if needed)
  info.name = "N/A";
  info.cpu_usage = 0.0; // Cannot calculate CPU on non-QNX
  info.memory_usage = 0;
  // Basic info population might go here if supported
  return false; // Indicate incomplete information
#endif
}

/**
 * @brief Read memory usage information for a specific process
 *
 * Retrieves memory usage information from the /proc filesystem for the
 * specified process. First attempts to use the address space file (as),
 * then falls back to the vmstat file if necessary.
 *
 * @param pid The process ID to read memory information for
 * @param info The ProcessInfo object to update with memory usage data
 * @return true if memory information was successfully read, false otherwise
 */
bool ProcessCore::readProcessMemory(pid_t pid, ProcessInfo &info) {
#ifdef __QNXNTO__
  // Open the as (address space) file to get memory information
  std::stringstream path;
  path << "/proc/" << pid << "/as";

  std::ifstream as(path.str().c_str());
  if (!as) {
    // Minor error, might not have permission, don't log verbosely unless
    // debugging std::cerr << "Failed to open /proc/" << pid << "/as: " <<
    // std::system_category().message(errno) << std::endl;
    return false;
  }

  debug_aspace_t aspace;
  if (as.read(reinterpret_cast<char *>(&aspace), sizeof(aspace))) {
    // Use the Resident Set Size (RSS) as memory usage
    info.memory_usage = aspace.rss / 1024; // Convert to KB
    return true;
  }

  // Fall back to vmstat
  path.str("");
  path << "/proc/" << pid << "/vmstat";

  std::ifstream vmstat(path.str().c_str());
  if (!vmstat) {
    // std::cerr << "Failed to open /proc/" << pid << "/vmstat: " <<
    // std::system_category().message(errno) << std::endl;
    return false;
  }

  std::string line;
  uint64_t memory_usage = 0;

  while (std::getline(vmstat, line)) {
    if (line.find("private") != std::string::npos) {
      std::stringstream ss(line);
      std::string key;
      uint64_t value;

      ss >> key >> value;
      memory_usage += value;
    }
  }

  info.memory_usage = memory_usage / 1024; // Convert to KB
  return true; // Assume success if file opened, even if no "private" found
#else
  return false;
#endif
}

/**
 * @brief Read process status information using devctl.
 *
 * Retrieves process metadata like parent PID, name, state, priority, policy,
 * thread count, and the total system + user time (`sutime`) using QNX's devctl
 * interface on /proc/<pid>/ctl.
 *
 * @param pid The process ID.
 * @param info Reference to ProcessInfo struct to populate.
 * @return std::optional<uint64_t> containing the sutime if successful,
 * std::nullopt otherwise.
 */
std::optional<uint64_t> ProcessCore::readProcessStatus(pid_t pid,
                                                       ProcessInfo &info) {
#ifdef __QNXNTO__
  std::string ctl_path_str = "/proc/" + std::to_string(pid) + "/ctl";
  int fd = open(ctl_path_str.c_str(), O_RDONLY);
  if (fd == -1) {
    // Process might have terminated between listing and opening
    // std::cerr << "Failed to open " << ctl_path_str << ": " << strerror(errno)
    // << std::endl;
    return std::nullopt;
  }

  uint64_t sutime = 0; // Variable to store sutime

  // Get general process info (path, parent PID, etc.)
  debug_process_t pinfo = {0}; // Important to zero-initialize
  if (devctl(fd, DCMD_PROC_INFO, &pinfo, sizeof(pinfo), nullptr) == EOK) {
    info.pid = pid;
    info.parent_pid = pinfo.parent;
    info.num_threads = pinfo.num_threads;

    // Get status of the first thread (TID 1) for priority, policy, state,
    // sutime
    procfs_status tinfo = {0}; // This is debug_thread_t in QNX 8.0
    tinfo.tid = 1;             // Get info for thread 1
    if (devctl(fd, DCMD_PROC_TIDSTATUS, &tinfo, sizeof(tinfo), nullptr) ==
        EOK) {
      info.priority = tinfo.priority;
      info.policy = tinfo.policy;
      info.state = tinfo.state; // Store the raw state code
      sutime = tinfo.sutime;    // Store the sutime

      close(fd);
      return std::make_optional(sutime); // Success
    } else {
      // std::cerr << "Failed devctl DCMD_PROC_TIDSTATUS for PID " << pid << ":
      // " << strerror(errno) << std::endl;
    }
  } else {
    // std::cerr << "Failed devctl DCMD_PROC_INFO for PID " << pid << ": " <<
    // strerror(errno) << std::endl;
  }

  // If we reach here, something failed
  close(fd);
  return std::nullopt;
#else
  // Non-QNX implementation placeholder
  info.pid = pid info.parent_pid = -1;
  info.num_threads = 0;
  return std::nullopt; // Cannot get sutime
#endif
}
} // namespace qnx
