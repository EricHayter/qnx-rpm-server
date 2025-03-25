#ifndef PROC_GROUP_H
#define PROC_GROUP_H

/**
 * @file proc_group.h
 * @brief Process Group Management Module for QNX Process Monitor
 *
 * This header defines the interface for the process group management module,
 * which provides functionality for organizing processes into logical groups.
 * Process groups allow for better organization and management of processes
 * based on their purpose or resource requirements.
 *
 * In a full QNX implementation with Adaptive Partitioning, this would interface
 * with the QNX APS (Adaptive Partitioning Scheduler). In this implementation,
 * it provides a simplified grouping mechanism.
 */

#include <stdio.h>    /* For standard I/O functions */
#include <stdlib.h>   /* For standard library functions */
#include <pthread.h>  /* For thread synchronization primitives */
#include <sys/types.h> /* For system types like pid_t */

/**
 * @struct process_group_t
 * @brief Structure representing a process group
 *
 * Contains information about a process group, including its identifier,
 * name, priority level, and resource usage statistics.
 */
typedef struct {
    int id;                  /**< Unique identifier for the process group */
    char name[64];           /**< Human-readable name for the process group */
    unsigned int priority;   /**< Priority level for the group (higher = more important) */
    double cpu_usage;        /**< Current CPU usage as a percentage */
    unsigned long memory_usage; /**< Current memory usage in bytes */
    int process_count;       /**< Number of processes in the group */
} process_group_t;

/**
 * @brief Initialize the process group module
 *
 * Sets up any resources needed by the process group module, such as
 * mutexes for thread synchronization.
 *
 * @return 0 on success, non-zero on failure
 */
int proc_group_init(void);

/**
 * @brief Clean up resources used by the process group module
 *
 * Releases any resources allocated by proc_group_init().
 */
void proc_group_shutdown(void);

/**
 * @brief Collect information about all process groups
 *
 * Updates the internal list of process groups with current information.
 * In a full implementation, this would query the system for actual group data.
 * In this simplified version, it creates predefined groups.
 *
 * @return Number of process groups found, or negative value on error
 */
int proc_group_collect_info(void);

/**
 * @brief Get the current number of process groups
 *
 * @return Number of process groups currently tracked
 */
int proc_group_get_count(void);

/**
 * @brief Get the list of process groups
 *
 * @return Pointer to the internal array of process_group_t structures
 */
const process_group_t *proc_group_get_list(void);

/**
 * @brief Display information about all process groups to the console
 *
 * Formats and prints a table showing details of all process groups.
 */
void proc_group_display_info(void);

/**
 * @brief Adjust the priority of a specified process group
 *
 * Changes the priority level of a process group identified by its ID.
 *
 * @param group_id The ID of the group to modify
 * @param priority The new priority value to set (1-63, higher = more important)
 * @return 0 on success, non-zero on failure
 */
int proc_group_adjust_priority(int group_id, int priority);

/**
 * @brief Determine which process group a process belongs to
 *
 * Maps a process ID to its corresponding group ID.
 * In this simplified implementation, assignment is based on PID ranges.
 *
 * @param pid The process ID to look up
 * @param group_id Pointer to store the resulting group ID
 * @return 0 on success, non-zero on failure
 */
int proc_group_find_process(pid_t pid, int *group_id);

#endif /* PROC_GROUP_H */ 