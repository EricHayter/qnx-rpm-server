/**
 * @file ProcessHistory.cpp
 * @brief Implementation of process history tracking functionality for QNX Remote Process Monitor
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
#include <iostream>
#include <algorithm> // For std::min

namespace qnx
{
    namespace history
    {

        /**
         * @brief Get the singleton instance of the ProcessHistory class
         *
         * This method implements the singleton pattern to ensure only one instance
         * of ProcessHistory exists throughout the application lifetime. It is thread-safe
         * due to the static local variable initialization guarantees of C++11.
         *
         * @return Reference to the singleton ProcessHistory instance
         */
        ProcessHistory &ProcessHistory::getInstance()
        {
            static ProcessHistory instance;
            return instance;
        }

        /**
         * @brief Initialize the process history module
         *
         * Prepares the ProcessHistory for operation by clearing any existing history data
         * and setting the maximum limits for entries per process and total tracked processes.
         * This method must be called before using other ProcessHistory functionality.
         *
         * @param max_entries_per_process Maximum number of history entries to store per process
         * @param max_tracked_processes Maximum number of processes to track simultaneously
         * @return true if initialization was successful, false otherwise
         */
        bool ProcessHistory::init(size_t max_entries_per_process, size_t max_tracked_processes)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            try
            {
                history_data_.clear();
                max_entries_per_process_ = max_entries_per_process;
                max_tracked_processes_ = max_tracked_processes;
                history_data_.reserve(max_tracked_processes_); // Pre-allocate map buckets
                return true;
            }
            catch (const std::exception &e)
            {
                std::cerr << "Failed to initialize ProcessHistory: " << e.what() << std::endl;
                return false;
            }
        }

        /**
         * @brief Release resources used by the process history module
         *
         * Cleans up resources by clearing all history data. This method should be
         * called when the ProcessHistory is no longer needed, typically during
         * application shutdown.
         */
        void ProcessHistory::shutdown()
        {
            clearAllHistory();
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
         * @param cpu_usage The current CPU usage percentage (0-100)
         * @param memory_usage The current memory usage in bytes
         */
        void ProcessHistory::addEntry(pid_t pid, double cpu_usage, size_t memory_usage)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = history_data_.find(pid);

            // If process not tracked and we've reached the limit, do nothing
            if (it == history_data_.end() && history_data_.size() >= max_tracked_processes_)
            {
                // Optionally, implement a strategy to remove the oldest tracked process
                // or the process with the least recent activity.
                // For now, we simply don't add the new process.
                // std::cerr << "Max tracked processes reached, not adding history for PID " << pid << std::endl;
                return;
            }

            // Find or create the list for the process
            ProcessData &data_list = history_data_[pid];

            // Add the new entry to the front (most recent)
            data_list.emplace_front(cpu_usage, memory_usage);

            // Trim the list if it exceeds the maximum size
            if (data_list.size() > max_entries_per_process_)
            {
                data_list.pop_back(); // Remove the oldest entry
            }
        }

        /**
         * @brief Retrieve historical entries for a specific process
         *
         * Returns a vector containing the most recent history entries for the specified process.
         * The vector is ordered with the most recent entry first. If the process is not being
         * tracked or has no history, an empty vector is returned.
         *
         * Thread-safe through mutex locking of the history data.
         *
         * @param pid The process ID to retrieve history for
         * @param count The maximum number of entries to retrieve
         * @return A vector of HistoryEntry objects containing the requested history
         */
        std::vector<HistoryEntry> ProcessHistory::getEntries(pid_t pid, size_t count) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::vector<HistoryEntry> result;

            auto it = history_data_.find(pid);
            if (it != history_data_.end())
            {
                const ProcessData &data_list = it->second;
                size_t num_to_copy = std::min(count, data_list.size());
                result.reserve(num_to_copy);

                // Copy the most recent 'num_to_copy' entries
                auto list_it = data_list.begin();
                for (size_t i = 0; i < num_to_copy; ++i, ++list_it)
                {
                    result.push_back(*list_it);
                }
            }

            return result;
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
        void ProcessHistory::clearProcessHistory(pid_t pid)
        {
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
        void ProcessHistory::clearAllHistory()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            history_data_.clear();
        }

    } // namespace history
} // namespace qnx