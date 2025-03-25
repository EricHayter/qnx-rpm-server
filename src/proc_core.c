/**
 * @file proc_core.c
 * @brief Implementation of the Process Management Core Module
 *
 * This file implements the process management functionality defined in proc_core.h.
 * It provides mechanisms for collecting, storing, and displaying information about
 * processes running on the system. The implementation includes both QNX-specific
 * code (when compiled for QNX) and a simplified fallback for non-QNX systems.
 */

#include <stdio.h>     /* For standard I/O functions */
#include <stdlib.h>    /* For standard library functions */
#include <unistd.h>    /* For POSIX API functions */
#include <string.h>    /* For string manipulation functions */
#include <sys/types.h> /* For system types like pid_t */
#include <dirent.h>    /* For directory operations */
#include <fcntl.h>     /* For file control operations */
#include <errno.h>     /* For error handling */
#include <pthread.h>   /* For thread synchronization */
#include <time.h>      /* For time-related functions */
#include <ctype.h>     /* For character classification functions like isspace() */
#include "proc_core.h"
#include "proc_group.h"

/* Static data */
/**
 * @brief Array of process information structures
 *
 * This static array stores information about all processes in the system.
 * The array is populated by proc_collect_info().
 */
static proc_info_t proc_list[MAX_PROCS];

/**
 * @brief Current number of processes
 *
 * Tracks how many entries in the proc_list array are valid.
 */
static int proc_count = 0;

/**
 * @brief Mutex for thread-safe access to process data
 *
 * Ensures that multiple threads can safely access and modify the proc_list array.
 */
static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Tracks the last time we collected process data
 *
 * Used to calculate CPU usage percentages between collection intervals.
 */
static struct timespec last_update_time = {0, 0};

/**
 * @brief Lock the process data mutex
 *
 * Acquires exclusive access to the process data structures.
 * This should be called before accessing process data from external modules.
 *
 * @return 0 on success, non-zero on failure
 */
int proc_core_mutex_lock(void) {
    return pthread_mutex_lock(&data_mutex);
}

/**
 * @brief Unlock the process data mutex
 *
 * Releases exclusive access to the process data structures.
 * This should be called after accessing process data from external modules.
 *
 * @return 0 on success, non-zero on failure
 */
int proc_core_mutex_unlock(void) {
    return pthread_mutex_unlock(&data_mutex);
}

/**
 * @brief Initialize the process core module
 *
 * Currently, this function doesn't need to perform any special initialization,
 * but it's included for API completeness and potential future extensions.
 *
 * @return 0 on success, non-zero on failure
 */
int proc_core_init(void) {
    /* Nothing special to initialize for now */
    return 0;
}

/**
 * @brief Clean up resources used by the process core module
 *
 * Destroys the mutex used for thread synchronization.
 */
void proc_core_shutdown(void) {
    pthread_mutex_destroy(&data_mutex);
}

#ifdef __QNXNTO__
/**
 * @brief Collect information about all processes (QNX-specific implementation)
 *
 * This function uses QNX's procfs API to gather detailed information about
 * each process, including memory usage, CPU time, and other statistics.
 *
 * @return Number of processes found, or negative value on error
 */
int proc_collect_info(void) {
    DIR *dir;
    struct dirent *entry;
    char path[MAX_PATH_LEN];
    char buffer[4096];
    int count = 0;
    procfs_status status;
    int fd;
    
    /* Lock the mutex to ensure thread safety */
    pthread_mutex_lock(&data_mutex);
    
    /* Update the last_update_time for CPU calculations */
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    
    /* On first run, initialize last_update_time */
    if (last_update_time.tv_sec == 0 && last_update_time.tv_nsec == 0) {
        last_update_time = current_time;
    }
    
    /* Open the /proc directory */
    dir = opendir("/proc");
    if (!dir) {
        perror("Failed to open /proc directory");
        pthread_mutex_unlock(&data_mutex);
        return -1;
    }
    
    /* Iterate through all entries in the /proc directory */
    while ((entry = readdir(dir)) != NULL && count < MAX_PROCS) {
        /* Skip non-numeric entries (not PIDs) */
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9')
            continue;
            
        /* Convert the directory name to a PID */
        pid_t pid = atoi(entry->d_name);
        
        /* Store the process ID */
        proc_list[count].pid = pid;
        
        /* Try to get the process name from /proc/PID/exefile */
        snprintf(path, sizeof(path), "/proc/%d/exefile", pid);
        fd = open(path, O_RDONLY);
        if (fd >= 0) {
            ssize_t len = read(fd, buffer, sizeof(buffer) - 1);
            if (len > 0) {
                buffer[len] = '\0';
                /* Extract just the filename from the path */
                char *name = strrchr(buffer, '/');
                if (name) {
                    strncpy(proc_list[count].name, name + 1, MAX_NAME_LEN - 1);
                } else {
                    strncpy(proc_list[count].name, buffer, MAX_NAME_LEN - 1);
                }
                proc_list[count].name[MAX_NAME_LEN - 1] = '\0';
            } else {
                /* Fallback to PID as name */
                strncpy(proc_list[count].name, entry->d_name, MAX_NAME_LEN - 1);
                proc_list[count].name[MAX_NAME_LEN - 1] = '\0';
            }
            close(fd);
        } else {
            /* Fallback to PID as name */
            strncpy(proc_list[count].name, entry->d_name, MAX_NAME_LEN - 1);
            proc_list[count].name[MAX_NAME_LEN - 1] = '\0';
        }
        
        /* Set a default memory value if all else fails */
        proc_list[count].memory_usage = 1024; /* 1 MB as a fallback */
        
        /* Try to get memory info from /proc/PID/vmstat first (most accurate) */
        snprintf(path, sizeof(path), "/proc/%d/vmstat", pid);
        fd = open(path, O_RDONLY);
        if (fd >= 0) {
            ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                
                /* Look for as_stats.anon_rsv which shows private memory reservation */
                char *mem_info = strstr(buffer, "as_stats.anon_rsv=");
                if (mem_info) {
                    /* Parse the memory value - format is typically like:
                     * as_stats.anon_rsv=0xa5 (660.000kB)
                     */
                    char *kb_start = strstr(mem_info, "(");
                    if (kb_start) {
                        double kb_value = 0;
                        if (sscanf(kb_start + 1, "%lf", &kb_value) == 1) {
                            proc_list[count].memory_usage = (unsigned long)kb_value;
                        }
                    }
                }
            }
            close(fd);
        }
        
        /* If vmstat didn't give us memory info, try pmap as a fallback */
        if (proc_list[count].memory_usage == 1024) {
            snprintf(path, sizeof(path), "/proc/%d/pmap", pid);
            fd = open(path, O_RDONLY);
            if (fd >= 0) {
                unsigned long total_reserved = 0;
                ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';
                    
                    /* Process each line of pmap output */
                    char *line = buffer;
                    char *next_line;
                    
                    /* Skip header line */
                    line = strchr(line, '\n');
                    if (line) line++;
                    
                    /* Process each mapping */
                    while (line && *line) {
                        next_line = strchr(line, '\n');
                        if (next_line) *next_line = '\0';
                        
                        /* Parse a pmap entry - format is CSV with fields:
                         * vaddr,size,flags,prot,maxprot,dev,ino,offset,rsv,...
                         */
                        char *fields[15] = {0};
                        int field_count = 0;
                        char *field = line;
                        char *next_field;
                        
                        while (field && field_count < 15) {
                            next_field = strchr(field, ',');
                            if (next_field) *next_field = '\0';
                            
                            fields[field_count++] = field;
                            
                            if (next_field) {
                                field = next_field + 1;
                            } else {
                                break;
                            }
                        }
                        
                        /* We need at least 9 fields to get to the rsv field */
                        if (field_count >= 9) {
                            /* Field 1 is size, field 8 is rsv */
                            unsigned long rsv_value = 0;
                            if (sscanf(fields[8], "0x%lx", &rsv_value) == 1) {
                                /* Add this reservation to the total */
                                total_reserved += rsv_value;
                            }
                        }
                        
                        /* Move to next line */
                        if (next_line) {
                            *next_line = '\n';
                            line = next_line + 1;
                        } else {
                            line = NULL;
                        }
                    }
                    
                    /* If we found reservations, use them (convert to KB) */
                    if (total_reserved > 0) {
                        proc_list[count].memory_usage = total_reserved / 1024;
                    }
                }
                close(fd);
            }
        }
        
        /* Open the process status file */
        snprintf(path, sizeof(path), "/proc/%d/as", pid);
        fd = open(path, O_RDONLY);
        if (fd >= 0) {
            /* Get process status */
            if (devctl(fd, DCMD_PROC_STATUS, &status, sizeof(status), NULL) == EOK) {
                /* Get thread count, priority, policy, and state directly from status */
                proc_list[count].num_threads = 1;  /* Default to 1 */
                proc_list[count].priority = status.priority;
                proc_list[count].policy = status.policy;
                proc_list[count].state = status.state;
                
                /* Get process start time */
                proc_list[count].start_time = status.start_time;
            }
            
            /* Get CPU usage */
            clockid_t clock_id;
            struct timespec ts;
            if (clock_getcpuclockid(pid, &clock_id) == 0) {
                if (clock_gettime(clock_id, &ts) == 0) {
                    /* Calculate CPU time in seconds */
                    double cpu_time = (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
                    
                    /* Store current time and calculate delta time if we have a previous value */
                    static struct timespec last_cpu_time[MAX_PROCS] = {0};
                    static pid_t last_pid[MAX_PROCS] = {0};
                    int idx = -1;
                    
                    /* Look for this PID in our tracking array */
                    for (int i = 0; i < MAX_PROCS; i++) {
                        if (last_pid[i] == pid) {
                            idx = i;
                            break;
                        } else if (last_pid[i] == 0 && idx == -1) {
                            /* Found empty slot if we don't have a match yet */
                            idx = i;
                        }
                    }
                    
                    if (idx >= 0) {
                        /* Calculate CPU percentage based on delta time */
                        if (last_pid[idx] == pid) {
                            double prev_time = (double)last_cpu_time[idx].tv_sec + 
                                               (double)last_cpu_time[idx].tv_nsec / 1000000000.0;
                            
                            /* Delta time in seconds */
                            double delta_time = cpu_time - prev_time;
                            
                            /* Convert to percentage (0-100 range) based on time since last update */
                            struct timespec current_time;
                            clock_gettime(CLOCK_MONOTONIC, &current_time);
                            double elapsed = (current_time.tv_sec - last_update_time.tv_sec) + 
                                            ((current_time.tv_nsec - last_update_time.tv_nsec) / 1000000000.0);
                            
                            if (elapsed > 0) {
                                /* CPU percentage = (CPU time delta / elapsed real time) * 100 */
                                proc_list[count].cpu_usage = (delta_time / elapsed) * 100.0;
                                
                                /* Cap at 100% */
                                if (proc_list[count].cpu_usage > 100.0) {
                                    proc_list[count].cpu_usage = 100.0;
                                }
                            } else {
                                proc_list[count].cpu_usage = 0.0;
                            }
                        } else {
                            /* First time seeing this process, can't calculate percentage yet */
                            proc_list[count].cpu_usage = 0.0;
                        }
                        
                        /* Save current values for next time */
                        last_pid[idx] = pid;
                        last_cpu_time[idx] = ts;
                    } else {
                        /* Should never happen but just in case */
                        proc_list[count].cpu_usage = 0.0;
                    }
                }
            }
            
            close(fd);
            
            /* Get group ID by calling into the process group module */
            proc_list[count].group_id = 0; /* Default group */
            proc_group_find_process(pid, &proc_list[count].group_id);
            
            count++;
        }
    }
    
    /* Clean up and update the process count */
    closedir(dir);
    proc_count = count;
    
    /* Update the last_update_time for next CPU calculation */
    last_update_time = current_time;
    
    pthread_mutex_unlock(&data_mutex);
    
    /* Always return a positive count to avoid the warning */
    return count > 0 ? count : 0;
}

#else
/**
 * @brief Collect information about all processes (non-QNX implementation)
 *
 * This is a simplified implementation for non-QNX systems that creates
 * a single dummy process entry for testing purposes.
 *
 * @return Number of processes found (always 1 in this implementation)
 */
int proc_collect_info(void) {
    /* Lock the mutex to ensure thread safety */
    pthread_mutex_lock(&data_mutex);
    
    /* Create a single dummy process entry for the current process */
    proc_list[0].pid = getpid();
    strcpy(proc_list[0].name, "proc-monitor");
    proc_list[0].group_id = 0;
    proc_list[0].memory_usage = 1024 * 1024; /* 1MB */
    proc_list[0].cpu_usage = 0.5;
    proc_list[0].priority = 10;
    proc_list[0].policy = 0;
    proc_list[0].num_threads = 1;
    proc_list[0].runtime = 0;
    proc_list[0].start_time = time(NULL);
    proc_list[0].state = 0;
    
    /* Set the process count to 1 */
    proc_count = 1;
    pthread_mutex_unlock(&data_mutex);
    
    return proc_count;
}
#endif

/**
 * @brief Get the current number of processes
 *
 * @return Number of processes currently tracked
 */
int proc_get_count(void) {
    return proc_count;
}

/**
 * @brief Get the list of processes
 *
 * @return Pointer to the internal array of proc_info_t structures
 */
const proc_info_t *proc_get_list(void) {
    return proc_list;
}

/**
 * @brief Display information about all processes to the console
 *
 * Formats and prints a table showing details of all processes, including
 * their PID, name, group ID, memory usage, CPU usage, priority, and thread count.
 */
void proc_display_info(void) {
    int i;

    /* Lock the mutex to ensure thread safety */
    pthread_mutex_lock(&data_mutex);

    /* Print the table header */
    printf("\n--- Process Information (Total: %d) ---\n", proc_count);
    printf("%-8s %-20s %-10s %-10s %-8s %-10s %-8s\n", 
           "PID", "Name", "Group", "Memory(KB)", "CPU%", "Priority", "Threads");
    printf("-------------------------------------------------------------------------\n");
    
    /* Print information for each process */
    for (i = 0; i < proc_count; i++) {
        printf("%-8d %-20s %-10d %-10lu %-8.2f %-10u %-8d\n", 
               proc_list[i].pid, 
               proc_list[i].name, 
               proc_list[i].group_id,
               proc_list[i].memory_usage, /* Already in KB from proc_collect_info */
               proc_list[i].cpu_usage,
               proc_list[i].priority,
               proc_list[i].num_threads);
    }
    
    /* Unlock the mutex */
    pthread_mutex_unlock(&data_mutex);
}

/**
 * @brief Adjust the priority and scheduling policy of a process
 *
 * This is a placeholder implementation that doesn't actually change
 * process priorities. In a full QNX implementation, it would use the
 * SchedSet() function to modify scheduling parameters.
 *
 * @param pid The process ID to modify (unused)
 * @param priority The new priority value to set (unused)
 * @param policy The new scheduling policy to set (unused)
 * @return Always returns -1 (not implemented)
 */
int proc_adjust_priority(pid_t pid, int priority, int policy) {
#ifdef __QNXNTO__
    /* In a full QNX implementation, we would use SchedSet() */
    printf("Priority adjustment not implemented in this version\n");
#else
    printf("Priority adjustment not available on non-QNX systems\n");
#endif
    return -1;
} 