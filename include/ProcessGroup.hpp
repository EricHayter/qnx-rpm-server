/**
 * @file ProcessGroup.hpp
 * @brief Process grouping functionality for the QNX Remote Process Monitor
 *
 * This file defines the ProcessGroup class, which provides functionality for
 * organizing processes into logical groups. This allows for better organization,
 * collective management, and reporting on sets of related processes.
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <sys/types.h>

namespace qnx
{
    // Removed nested process namespace

    // ProcessInfo is now directly in qnx namespace, forward declaration might not be needed
    // if ProcessControl.hpp is included, but safer to keep it if used before include.
    struct ProcessInfo;

    /**
     * @struct Group
     * @brief Represents a logical grouping of processes
     *
     * A Group contains metadata about a collection of processes that are
     * logically related (e.g., system processes, user applications, etc.).
     */
    struct Group
    {
        Group() = default;            // default constructor for container use
        int id;                       ///< Unique identifier for the group
        std::string name;             ///< Display name for the group
        int priority;                 ///< Display priority (lower values appear first)
        std::string description;      ///< Optional description of the group's purpose
        std::set<pid_t> processes;    ///< Set of process IDs belonging to this group
        double total_cpu_usage = 0.0; ///< Sum of CPU usage of all processes in the group
        long total_memory_usage = 0;  ///< Sum of memory usage of all processes in the group

        /**
         * @brief Constructor with required fields
         *
         * @param group_id Unique identifier for the group
         * @param group_name Display name for the group
         * @param group_priority Display priority (lower values appear first)
         * @param group_desc Optional description of the group's purpose
         */
        Group(int group_id, std::string_view group_name, int group_priority, std::string_view group_desc = "")
            : id(group_id), name(group_name), priority(group_priority), description(group_desc) {}
    };

    /**
     * @class ProcessGroup
     * @brief Manages logical groupings of processes
     *
     * This singleton class provides functionality for creating, managing,
     * and querying groups of processes. It allows processes to be organized
     * into logical collections for easier management and monitoring.
     */
    class ProcessGroup
    {
    public:
        /**
         * @brief Get the singleton instance of ProcessGroup
         *
         * @return Reference to the singleton instance
         */
        static ProcessGroup &getInstance();

        // Delete copy and move constructors/operators
        ProcessGroup(const ProcessGroup &) = delete;
        ProcessGroup &operator=(const ProcessGroup &) = delete;
        ProcessGroup(ProcessGroup &&) = delete;
        ProcessGroup &operator=(ProcessGroup &&) = delete;

        /**
         * @brief Create a new process group
         *
         * @param name Display name for the group
         * @param priority Display priority (lower values appear first)
         * @param description Optional description of the group's purpose
         * @return The ID of the newly created group, or -1 on failure
         */
        int createGroup(std::string_view name, int priority, std::string_view description = "");

        /**
         * @brief Delete an existing process group
         *
         * @param group_id ID of the group to delete
         * @return true if the group was successfully deleted, false otherwise
         */
        bool deleteGroup(int group_id);

        /**
         * @brief Rename an existing process group
         *
         * @param group_id ID of the group to rename
         * @param new_name New name for the group
         * @return true if the group was successfully renamed, false otherwise
         */
        bool renameGroup(int group_id, std::string_view new_name);

        /**
         * @brief Add a process to a group
         *
         * @param pid Process ID to add
         * @param group_id Group ID to add the process to
         * @return true if the process was successfully added, false otherwise
         */
        bool addProcessToGroup(pid_t pid, int group_id);

        /**
         * @brief Remove a process from a group
         *
         * @param pid Process ID to remove
         * @param group_id Group ID to remove the process from
         * @return true if the process was successfully removed, false otherwise
         */
        bool removeProcessFromGroup(pid_t pid, int group_id);

        /**
         * @brief Get the group ID a process belongs to
         *
         * @param pid Process ID to query
         * @return Group ID the process belongs to, or -1 if not in any group
         */
        int getProcessGroupId(pid_t pid) const;

        /**
         * @brief Get all processes in a group
         *
         * @param group_id Group ID to query
         * @return Set of process IDs belonging to the group
         */
        std::set<pid_t> getProcessesInGroup(int group_id) const;

        /**
         * @brief Get all defined group IDs
         * @return A vector of group IDs
         */
        std::vector<int> getGroupIds() const; // snapshot of current group IDs

        /**
         * @brief Update group statistics
         *
         * Recalculates CPU and memory usage totals for all groups
         * based on current process information.
         */
        void updateGroupStats();

        /**
         * @brief Display group information to console
         *
         * Debug utility to print information about all defined groups
         * and their member processes.
         */
        void displayGroups() const;

        /**
         * @brief Prioritize a group
         *
         * @param group_id ID of the group to prioritize
         */
        void prioritizeGroup(int group_id);

    private:
        /**
         * @brief Private constructor to enforce singleton pattern
         */
        ProcessGroup() = default;

        /**
         * @brief Private destructor
         */
        ~ProcessGroup() = default;

        /**
         * @brief Next available group ID
         */
        int next_group_id_ = 1;

        /**
         * @brief Map of group IDs to Group objects
         */
        std::map<int, Group> groups_;

        /**
         * @brief Map of process IDs to their group IDs
         */
        std::map<pid_t, int> process_group_map_;

        /**
         * @brief Mutex for thread-safe access to group data
         */
        mutable std::mutex mutex_;
    };

    // Inline definition of getGroupIds() inside namespace for proper scoping
    inline std::vector<int> ProcessGroup::getGroupIds() const
    {
        std::lock_guard<std::mutex> guard(mutex_);
        std::vector<int> ids;
        ids.reserve(groups_.size());
        for (auto const &entry : groups_)
        {
            ids.push_back(entry.first);
        }
        return ids;
    }
} // namespace qnx