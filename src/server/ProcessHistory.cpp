/**
 * @file ProcessHistory.cpp
 * @brief Implementation of process history tracking functionality for QNX
 * Remote Process Monitor
 *
 * This file implements the ProcessHistory class, which manages historical data
 * about process resource usage over time. It maintains a circular buffer of
 * metrics like CPU and memory usage for each tracked process, allowing for
 * trend analysis and visualization in the monitoring interface.
 *
 * The implementation provides thread-safe operations for adding, retrieving,
 * and managing history data with configurable limits on the number of processes
 * tracked and the amount of history stored per process.
 */

#include "ProcessHistory.hpp"
#include <ctime>

namespace qnx {
/**
 * @brief Get the singleton instance of the ProcessHistory class
 *
 * This method implements the singleton pattern to ensure only one instance
 * of ProcessHistory exists throughout the application lifetime. It is
 * thread-safe due to the static local variable initialization guarantees of
 * C++11.
 *
 * @return Reference to the singleton ProcessHistory instance
 */
ProcessHistory &ProcessHistory::getInstance() {
  static ProcessHistory instance;
  return instance;
}

/**
 * @brief Add a new history entry for a specific process
 *
 * Records a new CPU and memory usage data point for the specified process.
 * The entry is timestamped with the current system time. If the process is
 * not already being tracked and the maximum number of tracked processes has
 * been reached, the new entry will be ignored.
 *
 * If the number of entries for a process exceeds max_entries_per_process_,
 * the oldest entry will be discarded to maintain the size limit.
 *
 * Thread-safe through mutex locking of the history data.
 *
 * @param pid The process ID to add history for
 * @param cpu_usage The current CPU usage percentage
 * @param memory_usage The current memory usage in bytes
 */
void ProcessHistory::addEntry(pid_t pid, double cpu_usage, long memory_usage) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (history_data_.find(pid) == history_data_.end() &&
      history_data_.size() >= max_tracked_processes_) {
    return;
  }

  ProcessHistoryEntry entry;
  entry.cpu_usage = cpu_usage;
  entry.memory_usage = memory_usage;
  entry.timestamp = std::time(nullptr);

  auto &history_deque = history_data_[pid];
  history_deque.push_back(entry);

  if (history_deque.size() > max_entries_per_process_) {
    history_deque.pop_front();
  }
}

/**
 * @brief Retrieve historical entries for a specific process
 *
 * Returns a vector containing the most recent history entries for the specified
 * process. The vector is ordered with the most recent entry first. If the
 * process is not being tracked or has no history, an empty vector is returned.
 *
 * Thread-safe through mutex locking of the history data.
 *
 * @param pid The process ID to retrieve history for
 * @param count The maximum number of entries to retrieve
 * @return A vector of HistoryEntry objects containing the requested history
 */
std::vector<ProcessHistoryEntry> ProcessHistory::getHistory(pid_t pid) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = history_data_.find(pid);
  if (it != history_data_.end()) {
    return std::vector<ProcessHistoryEntry>(it->second.begin(),
                                            it->second.end());
  }
  return {};
}

/**
 * @brief Clear all historical data for a specific process
 *
 * Removes all history entries for the specified process. If the process
 * is not being tracked, this method has no effect.
 *
 * Thread-safe through mutex locking of the history data.
 *
 * @param pid The process ID to clear history for
 */
void ProcessHistory::clearProcessHistory(pid_t pid) {
  std::lock_guard<std::mutex> lock(mutex_);
  history_data_.erase(pid);
}

/**
 * @brief Clear all historical data for all processes
 *
 * Removes all history entries for all tracked processes, effectively
 * resetting the history module to its initial state.
 *
 * Thread-safe through mutex locking of the history data.
 */
void ProcessHistory::clearAllHistory() {
  std::lock_guard<std::mutex> lock(mutex_);
  history_data_.clear();
}

/**
 * @brief Retrieve all historical data for all processes
 *
 * Returns a map containing all historical entries for all tracked processes.
 * The map is keyed by process ID, and the value is a vector of
 * ProcessHistoryEntry objects.
 *
 * Thread-safe through mutex locking of the history data.
 *
 * @return A map of process ID to vector of ProcessHistoryEntry objects
 */
std::map<pid_t, std::vector<ProcessHistoryEntry>>
ProcessHistory::getAllHistory() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::map<pid_t, std::vector<ProcessHistoryEntry>> result;
  for (const auto &pair : history_data_) {
    result.emplace(pair.first, std::vector<ProcessHistoryEntry>(
                                   pair.second.begin(), pair.second.end()));
  }
  return result;
}
} // namespace qnx