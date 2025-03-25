/**
 * @file proc_group.c
 * @brief Implementation of the Process Group Management Module
 *
 * This file implements the process group management functionality defined in proc_group.h.
 * It provides a simplified implementation of process grouping, which in a full QNX system
 * would interface with the Adaptive Partitioning Scheduler.
 */

#include "proc_group.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include "proc_core.h"  /* Include this to access process data */

/**
 * @brief Maximum number of process groups supported
 */
#define MAX_GROUPS 16

/* Static data */
/**
 * @brief Array of process groups
 *
 * This static array stores information about all process groups in the system.
 * The array is populated by proc_group_collect_info().
 */
static process_group_t group_list[MAX_GROUPS];

/**
 * @brief Current number of process groups
 *
 * Tracks how many entries in the group_list array are valid.
 */
static int group_count = 0;

/**
 * @brief Mutex for thread-safe access to group data
 *
 * Ensures that multiple threads can safely access and modify the group_list array.
 */
static pthread_mutex_t group_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Initialize process groups
 *
 * This function sets up the predefined process groups:
 * 1. System - For system processes (highest priority)
 * 2. User - For user applications (medium priority)
 * 3. Background - For background tasks (lowest priority)
 *
 * @return 0 on success, -1 on error
 */
int proc_group_init(void) {
    /* Lock the mutex to ensure thread safety */
    pthread_mutex_lock(&group_mutex);
    
    /* Initialize with a system group */
    group_list[0].id = 0;
    strcpy(group_list[0].name, "System");
    group_list[0].priority = 10;
    group_list[0].cpu_usage = 0.0;
    group_list[0].memory_usage = 0;
    group_list[0].process_count = 0;
    
    /* Add a user group */
    group_list[1].id = 1;
    strcpy(group_list[1].name, "User");
    group_list[1].priority = 5;
    group_list[1].cpu_usage = 0.0;
    group_list[1].memory_usage = 0;
    group_list[1].process_count = 0;
    
    /* Add a background group */
    group_list[2].id = 2;
    strcpy(group_list[2].name, "Background");
    group_list[2].priority = 2;
    group_list[2].cpu_usage = 0.0;
    group_list[2].memory_usage = 0;
    group_list[2].process_count = 0;
    
    /* Set the total number of groups */
    group_count = 3;
    
    /* Unlock the mutex */
    pthread_mutex_unlock(&group_mutex);
    
    return 0;
}

/**
 * @brief Clean up resources used by the process group module
 *
 * Destroys the mutex used for thread synchronization.
 */
void proc_group_shutdown(void) {
    pthread_mutex_destroy(&group_mutex);
}

/**
 * @brief Collect information about all process groups and update statistics
 *
 * This function recalculates cumulative information for process groups, such as
 * memory usage, CPU usage, and priority. It uses the process list built by the
 * proc_collect_info function.
 *
 * @return Number of process groups, or negative value on error
 */
int proc_group_collect_info(void) {
    int i, j;
    const proc_info_t *proc_list;
    int proc_count;
    
    /* Get the current process list and count */
    proc_list = proc_get_list();
    proc_count = proc_get_count();
    
    /* Lock the mutex to ensure thread safety */
    pthread_mutex_lock(&group_mutex);
    
    /* Initialize statistics for all groups */
    for (i = 0; i < group_count; i++) {
        group_list[i].memory_usage = 0;
        group_list[i].cpu_usage = 0.0;
        group_list[i].process_count = 0;
    }
    
    /* Calculate group statistics based on process information */
    if (proc_list != NULL && proc_count > 0) {
        for (i = 0; i < proc_count; i++) {
            int group_id = proc_list[i].group_id;
            
            /* Find the group in our list */
            for (j = 0; j < group_count; j++) {
                if (group_list[j].id == group_id) {
                    /* Update statistics for this group */
                    group_list[j].memory_usage += proc_list[i].memory_usage;
                    group_list[j].cpu_usage += proc_list[i].cpu_usage;
                    group_list[j].process_count++;
                    break;
                }
            }
        }
    }
    
    pthread_mutex_unlock(&group_mutex);
    
    /* Always return a positive count to avoid the warning */
    return group_count > 0 ? group_count : 0;
}

/**
 * @brief Get the current number of process groups
 *
 * @return Number of process groups currently tracked
 */
int proc_group_get_count(void) {
    return group_count;
}

/**
 * @brief Get the list of process groups
 *
 * @return Pointer to the internal array of process_group_t structures
 */
const process_group_t *proc_group_get_list(void) {
    return group_list;
}

/**
 * @brief Display information about all process groups to the console
 *
 * Formats and prints a table showing details of all process groups, including
 * their ID, name, priority, CPU usage, and memory usage.
 */
void proc_group_display_info(void) {
    int i;
    
    /* Lock the mutex to ensure thread safety */
    pthread_mutex_lock(&group_mutex);
    
    /* Print the table header */
    printf("\n--- Process Group Information (Total: %d) ---\n", group_count);
    printf("%-5s %-20s %-10s %-10s %-10s\n", 
           "ID", "Name", "Priority", "CPU%", "Memory(KB)");
    printf("--------------------------------------------------\n");
    
    /* Print information for each group */
    for (i = 0; i < group_count; i++) {
        printf("%-5d %-20s %-10u %-10.2f %-10lu\n", 
               group_list[i].id, 
               group_list[i].name, 
               group_list[i].priority,
               group_list[i].cpu_usage,
               group_list[i].memory_usage);  /* Already in KB */
    }
    
    /* Unlock the mutex */
    pthread_mutex_unlock(&group_mutex);
}

/**
 * @brief Adjust the priority of a specified process group
 *
 * Changes the priority level of a process group identified by its ID.
 * Valid priority values range from 1 to 63, with higher values indicating
 * higher priority.
 *
 * @param group_id The ID of the group to modify
 * @param priority The new priority value to set (1-63, higher = more important)
 * @return 0 on success, non-zero on failure
 */
int proc_group_adjust_priority(int group_id, int priority) {
    int i;
    int result = -1;
    
    /* Validate the priority value */
    if (priority < 1 || priority > 63) {
        fprintf(stderr, "Invalid priority value (must be 1-63)\n");
        return -1;
    }
    
    /* Lock the mutex to ensure thread safety */
    pthread_mutex_lock(&group_mutex);
    
    /* Find the group with the specified ID */
    for (i = 0; i < group_count; i++) {
        if (group_list[i].id == group_id) {
            /* Update the priority */
            group_list[i].priority = priority;
            printf("Group %d priority adjusted to %d\n", group_id, priority);
            result = 0;
            break;
        }
    }
    
    /* Report if the group wasn't found */
    if (result != 0) {
        fprintf(stderr, "Group ID %d not found\n", group_id);
    }
    
    /* Unlock the mutex */
    pthread_mutex_unlock(&group_mutex);
    
    return result;
}

/**
 * @brief Determine which process group a process belongs to
 *
 * Maps a process ID to its corresponding group ID.
 * In this simplified implementation, assignment is based on PID ranges:
 * - PIDs <= 100: System group (ID 0)
 * - PIDs 101-1000: User group (ID 1)
 * - PIDs > 1000: Background group (ID 2)
 *
 * @param pid The process ID to look up
 * @param group_id Pointer to store the resulting group ID
 * @return 0 on success, non-zero on failure
 */
int proc_group_find_process(pid_t pid, int *group_id) {
    /* Validate the group_id pointer */
    if (group_id == NULL) {
        return -1;
    }
    
    /* Assign group based on PID range */
    *group_id = 0;  /* Default to System group */
    
    /* Assign processes with PID > 100 to User group */
    if (pid > 100) {
        *group_id = 1;
    }
    
    /* Assign processes with PID > 1000 to Background group */
    if (pid > 1000) {
        *group_id = 2;
    }
    
    return 0;
} 