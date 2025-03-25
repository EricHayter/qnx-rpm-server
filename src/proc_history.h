/**
 * @file proc_history.h
 * @brief Process History Management Module
 */

#ifndef PROC_HISTORY_H
#define PROC_HISTORY_H

#include <time.h>
#include "proc_core.h"

#define MAX_HISTORY_ENTRIES 60  // Store up to 60 entries per process
#define MAX_TRACKED_PROCESSES 100  // Track up to 100 processes

/**
 * @struct proc_history_entry_t
 * @brief Structure representing a historical process data entry
 */
typedef struct {
    double cpu_usage;
    unsigned long memory_usage;
    time_t timestamp;
} proc_history_entry_t;

/**
 * @struct proc_history_t
 * @brief Structure representing historical data for a process
 */
typedef struct {
    pid_t pid;
    proc_history_entry_t entries[MAX_HISTORY_ENTRIES];
    int entry_count;
    int next_entry;  // Circular buffer index
} proc_history_t;

/**
 * Initialize the process history module
 * @return 0 on success, -1 on failure
 */
int proc_history_init(void);

/**
 * Clean up the process history module
 */
void proc_history_shutdown(void);

/**
 * Add a new history entry for a process
 * @param pid Process ID
 * @param cpu_usage Current CPU usage
 * @param memory_usage Current memory usage
 * @return 0 on success, -1 on failure
 */
int proc_history_add_entry(pid_t pid, double cpu_usage, unsigned long memory_usage);

/**
 * Get historical data for a process
 * @param pid Process ID
 * @param entries Array to store history entries
 * @param max_entries Maximum number of entries to retrieve
 * @return Number of entries retrieved, or -1 on error
 */
int proc_history_get_entries(pid_t pid, proc_history_entry_t *entries, int max_entries);

/**
 * Clear history for a specific process
 * @param pid Process ID
 */
void proc_history_clear_process(pid_t pid);

#endif /* PROC_HISTORY_H */ 