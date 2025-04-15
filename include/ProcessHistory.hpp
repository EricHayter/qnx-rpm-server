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

#include <vector>
#include <deque>       // Include deque header
#include <list>        // Keep list for now, or remove if definitely unused elsewhere
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <optional>
#include <cstddef>     // For size_t
#include <sys/types.h> // For pid_t

namespace qnx
{
    namespace history
    {

        /**
         * @struct HistoryEntry
         * @brief Represents a single data point in a process's history.
         */
        struct HistoryEntry
        {
            double cpu_usage = 0.0;
            size_t memory_usage = 0;
            std::chrono::system_clock::time_point timestamp;

            HistoryEntry() : timestamp(std::chrono::system_clock::now()) {}
            HistoryEntry(double cpu, size_t mem)
                : cpu_usage(cpu), memory_usage(mem), timestamp(std::chrono::system_clock::now()) {}
        };

        /**
         * @class ProcessHistory
         * @brief Manages historical resource usage data for multiple processes.
         */
        class ProcessHistory
        {
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
             * @brief Initialize the history module.
             * @param max_entries_per_process Maximum number of historical entries per process.
             * @param max_tracked_processes Maximum number of processes to track history for.
             * @return true on success, false otherwise.
             */
            bool init(size_t max_entries_per_process = 60, size_t max_tracked_processes = 100);

            /**
             * @brief Shutdown the history module and clear data.
             */
            void shutdown();

            /**
             * @brief Add a new history entry for a specific process.
             * @param pid The process ID.
             * @param cpu_usage The current CPU usage.
             * @param memory_usage The current memory usage (in bytes).
             */
            void addEntry(pid_t pid, double cpu_usage, size_t memory_usage);

            /**
             * @brief Retrieve historical entries for a specific process.
             * @param pid The process ID.
             * @param count The maximum number of entries to retrieve (most recent first).
             * @return A vector containing the requested historical entries, or an empty vector if none exist.
             */
            std::vector<HistoryEntry> getEntries(pid_t pid, size_t count) const;

            /**
             * @brief Clear all historical data for a specific process.
             * @param pid The process ID.
             */
            void clearProcessHistory(pid_t pid);

            /**
             * @brief Clear all historical data for all processes.
             */
            void clearAllHistory();

        private:
            ProcessHistory() = default;
            ~ProcessHistory() = default;

            // Use std::deque instead of std::list
            using ProcessData = std::deque<HistoryEntry>;
            std::unordered_map<pid_t, ProcessData> history_data_;
            mutable std::mutex mutex_;
            size_t max_entries_per_process_ = 60;
            size_t max_tracked_processes_ = 100;
        };

    } // namespace history
} // namespace qnx