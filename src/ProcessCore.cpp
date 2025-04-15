/**
 * @file ProcessCore.cpp
 * @brief Implementation of the Process Management Core for QNX Remote Process Monitor
 *
 * This file implements the core functionality for process monitoring and management
 * in the QNX environment. It provides methods for collecting process information,
 * tracking resource usage, and managing process priorities.
 *
 * The implementation uses QNX-specific APIs where possible, with fallbacks for
 * non-QNX systems when appropriate. It interacts with the /proc filesystem to
 * gather detailed information about running processes.
 */

#include "ProcessCore.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <system_error>
#include <thread>
#include <fstream>
#include <cstring>
#include <map>
#include <sys/neutrino.h> // QNX specific header
#include <sys/procfs.h>   // For procfs_status structures

namespace qnx
{
    namespace core
    {

        ProcessInfo::ProcessInfo()
            : pid_(0), group_id_(0), memory_usage_(0), cpu_usage_(0.0), priority_(0), policy_(0), num_threads_(0), runtime_(0), start_time_(std::chrono::system_clock::now()), state_(0) {}

        /**
         * @brief Get the singleton instance of the ProcessCore class
         *
         * This method implements the singleton pattern to ensure only one instance
         * of ProcessCore exists throughout the application lifetime. It is thread-safe
         * due to the static local variable initialization guarantees of C++11.
         *
         * @return Reference to the singleton ProcessCore instance
         */
        ProcessCore &ProcessCore::getInstance()
        {
            static ProcessCore instance;
            return instance;
        }

        /**
         * @brief Initialize the process core module
         *
         * Prepares the ProcessCore for operation by clearing any existing process list
         * and resetting the last update timestamp. This method must be called before
         * using other ProcessCore functionality.
         *
         * @return true if initialization was successful, false otherwise
         */
        bool ProcessCore::init()
        {
            try
            {
                process_list_.clear();
                last_update_time_ = std::chrono::system_clock::now();
                return true;
            }
            catch (const std::exception &e)
            {
                std::cerr << "Failed to initialize ProcessCore: " << e.what() << std::endl;
                return false;
            }
        }

        /**
         * @brief Release resources used by the process core module
         *
         * Cleans up resources by clearing the process list. This method should be
         * called when the ProcessCore is no longer needed, typically during
         * application shutdown.
         */
        void ProcessCore::shutdown()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            process_list_.clear();
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
        int ProcessCore::collectInfo()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            process_list_.clear();

            try
            {
                const std::filesystem::path proc_path("/proc");

                if (!std::filesystem::exists(proc_path))
                {
                    throw std::runtime_error("Proc filesystem not found");
                }

                for (const auto &entry : std::filesystem::directory_iterator(proc_path))
                {
                    if (!entry.is_directory())
                        continue;

                    const std::string &name = entry.path().filename().string();
                    if (name.empty() || !std::isdigit(name[0]))
                        continue;

                    try
                    {
                        pid_t pid = std::stoi(name);
                        ProcessInfo info;

                        if (readProcessInfo(pid, info))
                        {
                            process_list_.push_back(std::move(info));
                        }
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << "Error processing PID " << name << ": " << e.what() << std::endl;
                        continue;
                    }
                }

                return static_cast<int>(process_list_.size());
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error collecting process information: " << e.what() << std::endl;
                return -1;
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
        size_t ProcessCore::getCount() const noexcept
        {
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
        const std::vector<ProcessInfo> &ProcessCore::getProcessList() const noexcept
        {
            return process_list_;
        }

        /**
         * @brief Find a specific process by its PID
         *
         * Searches the internal process list for a process with the specified PID and
         * returns its information if found. The method is thread-safe through mutex locking.
         *
         * @param pid The process ID to search for
         * @return An optional containing the ProcessInfo if found, or empty if not found
         */
        std::optional<ProcessInfo> ProcessCore::getProcessById(pid_t pid) const noexcept
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = std::find_if(process_list_.begin(), process_list_.end(),
                                   [pid](const ProcessInfo &info)
                                   { return info.getPid() == pid; });

            if (it != process_list_.end())
            {
                return *it;
            }
            return std::nullopt;
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
        bool ProcessCore::adjustPriority(pid_t pid, int priority, int policy)
        {
#ifdef __QNXNTO__
            struct sched_param param;
            param.sched_priority = priority;

            if (sched_setscheduler(pid, policy, &param) == -1)
            {
                std::error_code ec(errno, std::system_category());
                std::cerr << "Failed to adjust priority for PID " << pid << ": " << ec.message() << std::endl;
                return false;
            }
            return true;
#else
            std::cerr << "Priority adjustment not supported on non-QNX systems" << std::endl;
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
        void ProcessCore::displayInfo() const
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Print header
            std::cout << std::setw(8) << "PID"
                      << std::setw(20) << "Name"
                      << std::setw(12) << "Memory(KB)"
                      << std::setw(10) << "CPU%"
                      << std::setw(8) << "Threads"
                      << std::setw(10) << "Priority"
                      << std::setw(15) << "Policy"
                      << std::endl;
            std::cout << std::string(83, '-') << std::endl;

            // Print process information
            for (const auto &proc : process_list_)
            {
                std::cout << std::setw(8) << proc.getPid()
                          << std::setw(20) << proc.getName()
                          << std::setw(12) << proc.getMemoryUsage() / 1024
                          << std::setw(10) << std::fixed << std::setprecision(1) << proc.getCpuUsage()
                          << std::setw(8) << proc.getNumThreads()
                          << std::setw(10) << proc.getPriority()
                          << std::setw(15) << proc.getPolicy()
                          << std::endl;
            }
        }

        /**
         * @brief Read detailed information about a specific process
         *
         * This method gathers comprehensive information about a process with the given PID.
         * It reads process status, memory usage, and CPU usage information from various
         * files in the /proc filesystem.
         *
         * @param pid The process ID to read information for
         * @param info The ProcessInfo object to populate with the collected data
         * @return true if information was successfully read, false otherwise
         */
        bool ProcessCore::readProcessInfo(pid_t pid, ProcessInfo &info)
        {
            info.setPid(pid);

            if (!readProcessStatus(pid, info))
            {
                return false;
            }

            readProcessMemory(pid, info);
            readProcessCpu(pid, info);

            return true;
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
        bool ProcessCore::readProcessMemory(pid_t pid, ProcessInfo &info)
        {
#ifdef __QNXNTO__
            // Open the as (address space) file to get memory information
            std::stringstream path;
            path << "/proc/" << pid << "/as";

            std::ifstream as(path.str().c_str());
            if (!as)
            {
                return false;
            }

            debug_aspace_t aspace;
            if (as.read(reinterpret_cast<char *>(&aspace), sizeof(aspace)))
            {
                // Use the Resident Set Size (RSS) as memory usage
                info.setMemoryUsage(aspace.rss / 1024); // Convert to KB
                return true;
            }

            // Fall back to vmstat
            path.str("");
            path << "/proc/" << pid << "/vmstat";

            std::ifstream vmstat(path.str().c_str());
            if (!vmstat)
            {
                return false;
            }

            std::string line;
            uint64_t memory_usage = 0;

            while (std::getline(vmstat, line))
            {
                if (line.find("private") != std::string::npos)
                {
                    std::stringstream ss(line);
                    std::string key;
                    uint64_t value;

                    ss >> key >> value;
                    memory_usage += value;
                }
            }

            info.setMemoryUsage(memory_usage / 1024); // Convert to KB
            return true;
#else
            // Non-QNX implementation
            return false;
#endif
        }

        /**
         * @brief Read CPU usage information for a specific process
         *
         * Calculates CPU usage by reading process status information and comparing
         * with previous measurements. This method tracks CPU time changes over real
         * time to derive a percentage of CPU usage.
         *
         * @param pid The process ID to read CPU information for
         * @param info The ProcessInfo object to update with CPU usage data
         * @return true if CPU information was successfully read, false otherwise
         */
        bool ProcessCore::readProcessCpu(pid_t pid, ProcessInfo &info)
        {
#ifdef __QNXNTO__
            static std::unordered_map<pid_t, std::chrono::system_clock::time_point> last_cpu_times;

            std::stringstream path;
            path << "/proc/" << pid << "/status";

            std::ifstream status(path.str());
            if (!status)
            {
                return false;
            }

            procfs_status pstatus;
            if (status.read(reinterpret_cast<char *>(&pstatus), sizeof(pstatus)))
            {
                auto now = std::chrono::system_clock::now();
                auto last_time = last_cpu_times[pid];

                if (last_time != std::chrono::system_clock::time_point())
                {
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time);
                    double cpu_ms = static_cast<double>(pstatus.sutime) / 1000000.0;
                    info.setCpuUsage((cpu_ms / duration.count()) * 100.0);
                }

                last_cpu_times[pid] = now;
                return true;
            }
#endif
            return false;
        }

        /**
         * @brief Read general status information for a specific process
         *
         * Gathers process name, thread count, group ID, priority, policy, and state
         * information from various files in the /proc filesystem. Sets these values
         * in the provided ProcessInfo object.
         *
         * @param pid The process ID to read status information for
         * @param info The ProcessInfo object to update with status data
         * @return true if status information was successfully read, false otherwise
         */
        bool ProcessCore::readProcessStatus(pid_t pid, ProcessInfo &info)
        {
            std::stringstream path;
            path << "/proc/" << pid << "/exefile";

            std::error_code ec;
            std::filesystem::path exe_path(path.str());

            if (std::filesystem::exists(exe_path, ec))
            {
                info.setName(exe_path.filename().string());
            }
            else
            {
                info.setName(std::to_string(pid));
            }

#ifdef __QNXNTO__
            path.str("");
            path << "/proc/" << pid << "/info";

            std::ifstream info_file(path.str());
            if (info_file)
            {
                debug_process_t pinfo;
                if (info_file.read(reinterpret_cast<char *>(&pinfo), sizeof(pinfo)))
                {
                    info.setNumThreads(pinfo.num_threads);
                    info.setGroupId(pinfo.pid);
                }
            }

            path.str("");
            path << "/proc/" << pid << "/status";

            std::ifstream status(path.str());
            if (status)
            {
                procfs_status pstatus;
                if (status.read(reinterpret_cast<char *>(&pstatus), sizeof(pstatus)))
                {
                    info.setPriority(pstatus.priority);
                    info.setPolicy(pstatus.policy);
                    info.setState(pstatus.state);
                    return true;
                }
            }
#endif
            return false;
        }

    } // namespace core
} // namespace qnx