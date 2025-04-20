#ifndef QNX_PROCESS_CORE_HPP
#define QNX_PROCESS_CORE_HPP

/**
 * @file ProcessCore.hpp
 * @brief Process Management Core Module for QNX Process Monitor
 *
 * This header defines the interface for the process management core module,
 * which provides functionality for monitoring and managing processes in the system.
 */

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <system_error>
#include <unordered_set>
#include <optional>

// POSIX headers
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

// QNX-specific headers
#ifdef __QNXNTO__
#include <sys/neutrino.h>
#include <sys/netmgr.h>
#include <sys/procfs.h>
#include <sys/sched.h>
#include <sys/dcmd_proc.h>
#endif

namespace qnx
{

    /**
     * @class ProcessInfo
     * @brief Struct representing process information
     */
    struct ProcessInfo
    {
        pid_t pid;
        std::string name;
        int group_id;
        size_t memory_usage;
        double cpu_usage;
        int priority;
        int policy;
        int num_threads;
        std::chrono::milliseconds runtime_ms;
        std::chrono::system_clock::time_point start_time{ std::chrono::system_clock::now() };
        int state;
    };

    /**
     * @class ProcessCore
     * @brief Core class for process management functionality
     */
    class ProcessCore
    {
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
        ProcessCore() = default;
        ~ProcessCore() = default;

        // Helper methods
        bool readProcessInfo(pid_t pid, ProcessInfo &info);
        bool readProcessMemory(pid_t pid, ProcessInfo &info);
        bool readProcessCpu(pid_t pid, ProcessInfo &info);
        bool readProcessStatus(pid_t pid, ProcessInfo &info);

        std::vector<ProcessInfo> process_list_;
        mutable std::mutex mutex_;
        std::chrono::system_clock::time_point last_update_time_;
    };

} // namespace qnx

#endif /* QNX_PROCESS_CORE_HPP */
