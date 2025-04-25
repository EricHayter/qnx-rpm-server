/**
 * @file ProcessHistory.hpp
 * @brief Process history tracking for the QNX Remote Process Monitor
 *
 * This file defines the ProcessHistory class, which provides functionality for
 * recording and managing historical data about processes. This includes
 * metrics like CPU and memory usage over time, allowing for trend analysis
 * and visualization in the monitoring interface.
 */

#pragma once

#include "ProcessControl.hpp"
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <sys/types.h>
#include <vector>

namespace qnx {
struct ProcessHistoryEntry {
  double cpu_usage;
  long memory_usage;
  time_t timestamp;
};

class ProcessHistory {
public:
  /**
   * @brief Get the singleton instance of ProcessHistory.
   * @return Reference to the singleton instance.
   */
  static ProcessHistory &getInstance();

  // Delete copy/move constructors and assignment operators
  ProcessHistory(const ProcessHistory &) = delete;
  ProcessHistory &operator=(const ProcessHistory &) = delete;
  ProcessHistory(ProcessHistory &&) = delete;
  ProcessHistory &operator=(ProcessHistory &&) = delete;

  /**
   * @brief Add a new history entry for a specific process.
   * @param pid The process ID.
   * @param cpu_usage Current CPU usage.
   * @param memory_usage Current memory usage.
   */
  void addEntry(pid_t pid, double cpu_usage, long memory_usage);

  /**
   * @brief Retrieve historical entries for a specific process.
   * @param pid The process ID.
   * @return A vector containing the requested historical entries, or an empty
   * vector if none exist.
   */
  std::vector<ProcessHistoryEntry> getHistory(pid_t pid) const;

  /**
   * @brief Retrieve historical entries for all processes.
   * @return A map containing historical entries for all processes.
   */
  std::map<pid_t, std::vector<ProcessHistoryEntry>> getAllHistory() const;

  /**
   * @brief Clear all historical data for a specific process
   * @param pid The process ID to clear history for
   */
  void clearProcessHistory(pid_t pid);

  /**
   * @brief Clear all historical data for all processes
   */
  void clearAllHistory();

private:
  ProcessHistory() = default;
  ~ProcessHistory() = default;

  mutable std::mutex mutex_;
  std::map<pid_t, std::deque<ProcessHistoryEntry>> history_data_;
  size_t max_entries_per_process_ = 100;
  size_t max_tracked_processes_ = 1000;
};
} // namespace qnx