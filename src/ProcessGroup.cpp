#include "ProcessGroup.hpp"
#include "ProcessControl.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>

namespace qnx
{
    namespace process
    {

        ProcessGroup &ProcessGroup::getInstance()
        {
            static ProcessGroup instance;
            return instance;
        }

        bool ProcessGroup::init()
        {
            try
            {
                std::lock_guard<std::mutex> lock(mutex_);
                groups_.clear();
                process_group_map_.clear();
                next_group_id_ = 1;
                return true;
            }
            catch (const std::exception &e)
            {
                std::cerr << "Failed to initialize ProcessGroup: " << e.what() << std::endl;
                return false;
            }
        }

        void ProcessGroup::shutdown()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            groups_.clear();
            process_group_map_.clear();
        }

        int ProcessGroup::createGroup(const std::string &name, int priority, const std::string &description)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            int group_id = next_group_id_++;
            groups_[group_id] = Group(group_id, name, priority, description);

            return group_id;
        }

        bool ProcessGroup::deleteGroup(int group_id)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = groups_.find(group_id);
            if (it == groups_.end())
            {
                return false;
            }

            // Remove all processes from this group in the process_group_map
            for (pid_t pid : it->second.processes)
            {
                process_group_map_.erase(pid);
            }

            // Remove the group
            return groups_.erase(group_id) > 0;
        }

        bool ProcessGroup::renameGroup(int group_id, const std::string &new_name)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = groups_.find(group_id);
            if (it == groups_.end())
            {
                return false;
            }

            it->second.name = new_name;
            return true;
        }

        bool ProcessGroup::addProcessToGroup(pid_t pid, int group_id)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = groups_.find(group_id);
            if (it == groups_.end())
            {
                return false;
            }

            // Check if process exists
            if (!utils::ProcessControl::exists(pid))
            {
                return false;
            }

            // Remove process from previous group if any
            auto prev_group_it = process_group_map_.find(pid);
            if (prev_group_it != process_group_map_.end())
            {
                int prev_group_id = prev_group_it->second;
                if (prev_group_id != group_id && groups_.find(prev_group_id) != groups_.end())
                {
                    groups_[prev_group_id].processes.erase(pid);
                }
            }

            // Add to new group
            it->second.processes.insert(pid);
            process_group_map_[pid] = group_id;

            return true;
        }

        bool ProcessGroup::removeProcessFromGroup(pid_t pid, int group_id)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = groups_.find(group_id);
            if (it == groups_.end())
            {
                return false;
            }

            auto &processes = it->second.processes;
            auto erase_count = processes.erase(pid);

            if (erase_count > 0)
            {
                process_group_map_.erase(pid);
                return true;
            }

            return false;
        }

        int ProcessGroup::getProcessGroup(pid_t pid) const
        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = process_group_map_.find(pid);
            if (it != process_group_map_.end())
            {
                return it->second;
            }

            return -1;
        }

        std::set<pid_t> ProcessGroup::getProcessesInGroup(int group_id) const
        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = groups_.find(group_id);
            if (it != groups_.end())
            {
                return it->second.processes;
            }

            return std::set<pid_t>();
        }

        const std::map<int, Group> &ProcessGroup::getAllGroups() const
        {
            return groups_;
        }

        void ProcessGroup::updateGroupStats()
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Reset all group stats
            for (auto &group_pair : groups_)
            {
                Group &group = group_pair.second;
                group.total_cpu_usage = 0.0;
                group.total_memory_usage = 0;

                // Remove any processes that no longer exist
                std::set<pid_t> to_remove;
                for (pid_t pid : group.processes)
                {
                    if (!utils::ProcessControl::exists(pid))
                    {
                        to_remove.insert(pid);
                        process_group_map_.erase(pid);
                    }
                    else
                    {
                        // Add current process stats to group totals
                        auto proc_info = utils::ProcessControl::getProcessInfo(pid);
                        if (proc_info)
                        {
                            group.total_cpu_usage += proc_info->cpu_usage;
                            group.total_memory_usage += proc_info->memory_usage;
                        }
                    }
                }

                // Remove non-existent processes
                for (pid_t pid : to_remove)
                {
                    group.processes.erase(pid);
                }
            }
        }

        void ProcessGroup::displayGroups() const
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Print header
            std::cout << std::setw(8) << "Group ID"
                      << std::setw(20) << "Name"
                      << std::setw(12) << "Processes"
                      << std::setw(12) << "CPU%"
                      << std::setw(12) << "Memory(MB)"
                      << std::setw(10) << "Priority"
                      << std::setw(20) << "Description"
                      << std::endl;
            std::cout << std::string(94, '-') << std::endl;

            // Print group information
            for (const auto &pair : groups_)
            {
                const auto &group = pair.second;
                std::cout << std::setw(8) << group.id
                          << std::setw(20) << group.name
                          << std::setw(12) << group.processes.size()
                          << std::setw(12) << std::fixed << std::setprecision(1) << group.total_cpu_usage
                          << std::setw(12) << group.total_memory_usage / (1024 * 1024)
                          << std::setw(10) << group.priority
                          << std::setw(20) << group.description
                          << std::endl;
            }
        }

    } // namespace process
} // namespace qnx