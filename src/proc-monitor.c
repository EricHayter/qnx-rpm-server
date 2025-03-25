#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/neutrino.h>
#include "proc_core.h"
#include "proc_group.h"

// Global flag for graceful shutdown
static volatile int shutdown_flag = 0;

// Default update interval in seconds
#define DEFAULT_UPDATE_INTERVAL 2

// Function to get process CPU time
static void get_process_cpu_time(pid_t pid, struct timespec *ts) {
    int clock_id;
    
    // Get the CPU-time clock ID for the specified process
    if (clock_getcpuclockid(pid, &clock_id) != 0) {
        fprintf(stderr, "Warning: Failed to get CPU clock for PID %d\n", pid);
        ts->tv_sec = 0;
        ts->tv_nsec = 0;
        return;
    }
    
    // Get the CPU time for the process
    if (clock_gettime(clock_id, ts) != 0) {
        fprintf(stderr, "Warning: Failed to get CPU time for PID %d\n", pid);
        ts->tv_sec = 0;
        ts->tv_nsec = 0;
    }
}

// Signal handler for graceful shutdown
static void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\nShutdown signal received. Cleaning up...\n");
        shutdown_flag = 1;
    }
}

// Display usage information
static void display_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("Options:\n");
    printf("  -i INTERVAL  Update interval in seconds (default: %d)\n", DEFAULT_UPDATE_INTERVAL);
    printf("  -h          Display this help message\n");
}

// Function to display time in a human-readable format
static void display_time(const struct timespec *ts) {
    if (ts->tv_sec > 0) {
        printf("%lds ", ts->tv_sec);
    }
    printf("%ldms", ts->tv_nsec / 1000000);
}

int main(int argc, char *argv[]) {
    int opt;
    int update_interval = DEFAULT_UPDATE_INTERVAL;
    struct timespec last_update, current_time, elapsed;
    
    // Parse command line arguments
    while ((opt = getopt(argc, argv, "hi:")) != -1) {
        switch (opt) {
            case 'i':
                update_interval = atoi(optarg);
                if (update_interval < 1) {
                    fprintf(stderr, "Error: Update interval must be at least 1 second\n");
                    return 1;
                }
                break;
            case 'h':
                display_usage(argv[0]);
                return 0;
            default:
                display_usage(argv[0]);
                return 1;
        }
    }

    // Set up signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Initialize process monitoring
    if (proc_core_init() != 0) {
        fprintf(stderr, "Failed to initialize process monitoring\n");
        return 1;
    }

    // Initialize process groups
    if (proc_group_init() != 0) {
        fprintf(stderr, "Failed to initialize process groups\n");
        proc_core_shutdown();
        return 1;
    }

    printf("QNX Process Monitor started. Update interval: %d second(s)\n", update_interval);
    printf("Press Ctrl+C to exit\n\n");

    // Get initial time
    clock_gettime(CLOCK_MONOTONIC, &last_update);

    // Main monitoring loop
    while (!shutdown_flag) {
        // Lock process data for thread safety
        proc_core_mutex_lock();

        // Get current time
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        
        // Calculate elapsed time
        elapsed.tv_sec = current_time.tv_sec - last_update.tv_sec;
        elapsed.tv_nsec = current_time.tv_nsec - last_update.tv_nsec;
        if (elapsed.tv_nsec < 0) {
            elapsed.tv_sec--;
            elapsed.tv_nsec += 1000000000L;
        }

        // Collect process information
        if (proc_collect_info() != 0) {
            fprintf(stderr, "Warning: Failed to collect process information\n");
        }

        // Collect process group information
        if (proc_group_collect_info() != 0) {
            fprintf(stderr, "Warning: Failed to collect process group information\n");
        }

        // Display timing information
        printf("\nUpdate Time: ");
        display_time(&elapsed);
        printf("\n");

        // Display process and group information
        proc_display_info();
        proc_group_display_info();

        // Update last update time
        last_update = current_time;

        // Unlock process data
        proc_core_mutex_unlock();

        // Wait for next update using nanosleep for better precision
        struct timespec sleep_time = {
            .tv_sec = update_interval,
            .tv_nsec = 0
        };
        nanosleep(&sleep_time, NULL);
    }

    // Cleanup
    printf("Shutting down...\n");
    proc_group_shutdown();
    proc_core_shutdown();

    return 0;
}