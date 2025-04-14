/**
 * @file ProcessGroup.hpp
 * @brief Process grouping functionality for the QNX Remote Process Monitor
 *
 * This file defines the ProcessGroup class, which provides functionality for
 * organizing processes into logical groups. This allows for better organization,
 * collective management, and reporting on sets of related processes.
 */

#ifndef QNX_PROCESS_GROUP_HPP
#define QNX_PROCESS_GROUP_HPP

#include <string>
#include <unordered_map>
#include <map>
#include <vector>
#include <mutex>
#include <set>
#include <memory>
#include <atomic>
#include "ProcessCore.hpp"

namespace qnx
{
    namespace process
    {
        /**
         * @brief Forward declaration of ProcessInfo class
         */
        class ProcessInfo;

        /**
         * @struct Group
         * @brief Represents a logical grouping of processes
         *
         * A Group contains metadata about a collection of processes that are
         * logically related (e.g., system processes, user applications, etc.).
         */
        struct Group
        {
            int id;                    ///< Unique identifier for the group
            std::string name;          ///< Display name for the group
            std::string description;   ///< Optional description of the group's purpose
            int priority;              ///< Display priority (lower values appear first)
            double total_cpu_usage;    ///< Sum of CPU usage of all processes in the group
            size_t total_memory_usage; ///< Sum of memory usage of all processes in the group
            std::set<pid_t> processes; ///< Set of process IDs belonging to this group

            /**
             * @brief Default constructor
             */
            Group() : id(0), name(""), description(""), priority(0),
                      total_cpu_usage(0.0), total_memory_usage(0) {}

            /**
             * @brief Constructor with required fields
             *
             * @param group_id Unique identifier for the group
             * @param group_name Display name for the group
             * @param group_priority Display priority (lower values appear first)
             * @param group_desc Optional description of the group's purpose
             */
            Group(int group_id, const std::string &group_name, int group_priority, const std::string &group_desc = "")
                : id(group_id), name(group_name), description(group_desc), priority(group_priority),
                  total_cpu_usage(0.0), total_memory_usage(0) {}
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
             * @brief Initialize the process grouping system
             *
             * @return true on successful initialization, false otherwise
             */
            bool init();

            /**
             * @brief Shut down the process grouping system
             */
            void shutdown();

            /**
             * @brief Create a new process group
             *
             * @param name Display name for the group
             * @param priority Display priority (lower values appear first)
             * @param description Optional description of the group's purpose
             * @return The ID of the newly created group, or -1 on failure
             */
            int createGroup(const std::string &name, int priority, const std::string &description = "");

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
            bool renameGroup(int group_id, const std::string &new_name);

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
             * @brief Get the group a process belongs to
             *
             * @param pid Process ID to query
             * @return Group ID the process belongs to, or -1 if not in any group
             */
            int getProcessGroup(pid_t pid) const;

            /**
             * @brief Get all processes in a group
             *
             * @param group_id Group ID to query
             * @return Set of process IDs belonging to the group
             */
            std::set<pid_t> getProcessesInGroup(int group_id) const;

            /**
             * @brief Get all defined groups
             *
             * @return Map of group IDs to Group objects
             */
            const std::map<int, Group> &getAllGroups() const;

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
            std::atomic<int> next_group_id_{1};

            /**
             * @brief Map of group IDs to Group objects
             */
            std::map<int, Group> groups_;

            /**
             * @brief Map of process IDs to their group IDs
             */
            std::unordered_map<pid_t, int> process_group_map_;

            /**
             * @brief Mutex for thread-safe access to group data
             */
            mutable std::mutex mutex_;
        };

    } // namespace process
} // namespace qnx

#endif // QNX_PROCESS_GROUP_HPP