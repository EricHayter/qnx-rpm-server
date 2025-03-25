#ifndef PROC_CORE_H
#define PROC_CORE_H

/**
 * @file proc_core.h
 * @brief Process Management Core Module for QNX Process Monitor
 *
 * This header defines the interface for the process management core module,
 * which provides functionality for monitoring and managing processes in the system.
 * It includes structures for storing process information and functions for
 * collecting, retrieving, and displaying process data.
 *
 * The module is designed to work on both QNX and non-QNX systems, with
 * conditional compilation to use QNX-specific features when available.
 */

#include <stdio.h>    /* For standard I/O functions */
#include <stdlib.h>   /* For standard library functions */
#include <string.h>   /* For string manipulation functions */
#include <unistd.h>   /* For POSIX API functions */
#include <errno.h>    /* For error codes */
#include <time.h>     /* For time-related functions */
#include <sys/stat.h> /* For file status functions */
#include <fcntl.h>    /* For file control options */
#include <dirent.h>   /* For directory entry functions */
#include <pthread.h>  /* For thread synchronization primitives */

/**
 * @brief Include QNX-specific headers when compiling for QNX
 *
 * These headers provide access to QNX-specific functionality for process
 * management, scheduling, and system information.
 */
#ifdef __QNXNTO__
#include <sys/neutrino.h>  /* QNX Neutrino RTOS API */
#include <sys/netmgr.h>    /* QNX network manager API */
#include <sys/procfs.h>    /* QNX process filesystem API */
#include <sys/sched.h>     /* QNX scheduler API */
#include <sys/dcmd_proc.h> /* QNX process devctl commands */
#endif

/**
 * @brief Path to the process information directory
 *
 * On POSIX systems, process information is typically available in /proc.
 */
#define PROC_PATH "/proc"

/**
 * @brief Maximum number of processes to track
 *
 * Limits the size of the internal process list array.
 */
#define MAX_PROCS 256

/**
 * @brief Maximum length for file paths
 *
 * Used for buffers that store file paths when accessing process information.
 */
#define MAX_PATH_LEN 256

/**
 * @brief Maximum length for process names
 *
 * Limits the size of the name field in the proc_info_t structure.
 */
#define MAX_NAME_LEN 128

/**
 * @struct proc_info_t
 * @brief Structure representing process information
 *
 * Contains comprehensive information about a process, including its
 * identifier, name, resource usage, and scheduling parameters.
 */
typedef struct {
    pid_t pid;                    /**< Process ID */
    char name[MAX_NAME_LEN];      /**< Process name (executable name) */
    int group_id;                 /**< Process group ID */
    unsigned long memory_usage;   /**< Memory usage in bytes */
    double cpu_usage;             /**< CPU usage as a percentage */
    unsigned int priority;        /**< Scheduling priority */
    int policy;                   /**< Scheduling policy (e.g., SCHED_RR, SCHED_FIFO) */
    int num_threads;              /**< Number of threads in the process */
    unsigned long long runtime;   /**< Total runtime in milliseconds */
    time_t start_time;            /**< Process start time */
    int state;                    /**< Process state (running, sleeping, etc.) */
} proc_info_t;

/**
 * @brief Initialize the process core module
 *
 * Sets up any resources needed by the process core module.
 *
 * @return 0 on success, non-zero on failure
 */
int proc_core_init(void);

/**
 * @brief Clean up resources used by the process core module
 *
 * Releases any resources allocated by proc_core_init().
 */
void proc_core_shutdown(void);

/**
 * @brief Collect information about all processes
 *
 * Updates the internal list of processes with current information.
 * On QNX systems, this reads from the /proc filesystem to gather
 * detailed information. On non-QNX systems, it provides a simplified
 * implementation for testing.
 *
 * @return Number of processes found, or negative value on error
 */
int proc_collect_info(void);

/**
 * @brief Get the current number of processes
 *
 * @return Number of processes currently tracked
 */
int proc_get_count(void);

/**
 * @brief Get the list of processes
 *
 * @return Pointer to the internal array of proc_info_t structures
 */
const proc_info_t *proc_get_list(void);

/**
 * @brief Display information about all processes to the console
 *
 * Formats and prints a table showing details of all processes.
 */
void proc_display_info(void);

/**
 * @brief Adjust the priority and scheduling policy of a process
 *
 * Changes the scheduling parameters of a process identified by its PID.
 *
 * @param pid The process ID to modify
 * @param priority The new priority value to set
 * @param policy The new scheduling policy to set
 * @return 0 on success, non-zero on failure
 */
int proc_adjust_priority(pid_t pid, int priority, int policy);

/**
 * @brief Lock the process data mutex
 *
 * Acquires exclusive access to the process data structures.
 * This should be called before accessing process data from external modules.
 *
 * @return 0 on success, non-zero on failure
 */
int proc_core_mutex_lock(void);

/**
 * @brief Unlock the process data mutex
 *
 * Releases exclusive access to the process data structures.
 * This should be called after accessing process data from external modules.
 *
 * @return 0 on success, non-zero on failure
 */
int proc_core_mutex_unlock(void);

#endif /* PROC_CORE_H */ 