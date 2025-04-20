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
     * @brief Class representing process information
     */
    class ProcessInfo
    {
    public:
        ProcessInfo();
        ~ProcessInfo() = default;

        // Getters
        pid_t getPid() const noexcept { return pid_; }
        const std::string &getName() const noexcept { return name_; }
        int getGroupId() const noexcept { return group_id_; }
        size_t getMemoryUsage() const noexcept { return memory_usage_; }
        double getCpuUsage() const noexcept { return cpu_usage_; }
        int getPriority() const noexcept { return priority_; }
        int getPolicy() const noexcept { return policy_; }
        int getNumThreads() const noexcept { return num_threads_; }
        std::chrono::milliseconds getRuntime() const noexcept { return runtime_; }
        std::chrono::system_clock::time_point getStartTime() const noexcept { return start_time_; }
        int getState() const noexcept { return state_; }

        // Setters
        void setPid(pid_t pid) noexcept { pid_ = pid; }
        void setName(const std::string &name) { name_ = name; }
        void setGroupId(int id) noexcept { group_id_ = id; }
        void setMemoryUsage(size_t usage) noexcept { memory_usage_ = usage; }
        void setCpuUsage(double usage) noexcept { cpu_usage_ = usage; }
        void setPriority(int priority) noexcept { priority_ = priority; }
        void setPolicy(int policy) noexcept { policy_ = policy; }
        void setNumThreads(int threads) noexcept { num_threads_ = threads; }
        void setRuntime(std::chrono::milliseconds runtime) noexcept { runtime_ = runtime; }
        void setStartTime(std::chrono::system_clock::time_point time) noexcept { start_time_ = time; }
        void setState(int state) noexcept { state_ = state; }

    private:
        pid_t pid_;
        std::string name_;
        int group_id_;
        size_t memory_usage_;
        double cpu_usage_;
        int priority_;
        int policy_;
        int num_threads_;
        std::chrono::milliseconds runtime_;
        std::chrono::system_clock::time_point start_time_;
        int state_;
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