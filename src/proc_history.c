/**
 * @file proc_history.c
 * @brief Process History Management Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "proc_history.h"

static proc_history_t *process_histories = NULL;
static int history_count = 0;
static pthread_mutex_t history_mutex = PTHREAD_MUTEX_INITIALIZER;

int proc_history_init(void) {
    pthread_mutex_lock(&history_mutex);
    
    if (process_histories == NULL) {
        process_histories = calloc(MAX_TRACKED_PROCESSES, sizeof(proc_history_t));
        if (process_histories == NULL) {
            pthread_mutex_unlock(&history_mutex);
            return -1;
        }
        history_count = 0;
    }
    
    pthread_mutex_unlock(&history_mutex);
    return 0;
}

void proc_history_shutdown(void) {
    pthread_mutex_lock(&history_mutex);
    
    if (process_histories != NULL) {
        free(process_histories);
        process_histories = NULL;
        history_count = 0;
    }
    
    pthread_mutex_unlock(&history_mutex);
}

static proc_history_t* find_or_create_history(pid_t pid) {
    // First, try to find existing history
    for (int i = 0; i < history_count; i++) {
        if (process_histories[i].pid == pid) {
            return &process_histories[i];
        }
    }
    
    // If not found and we have space, create new entry
    if (history_count < MAX_TRACKED_PROCESSES) {
        proc_history_t *history = &process_histories[history_count];
        history->pid = pid;
        history->entry_count = 0;
        history->next_entry = 0;
        history_count++;
        return history;
    }
    
    // No space available
    return NULL;
}

int proc_history_add_entry(pid_t pid, double cpu_usage, unsigned long memory_usage) {
    pthread_mutex_lock(&history_mutex);
    
    proc_history_t *history = find_or_create_history(pid);
    if (history == NULL) {
        pthread_mutex_unlock(&history_mutex);
        return -1;
    }
    
    // Add new entry
    proc_history_entry_t *entry = &history->entries[history->next_entry];
    entry->cpu_usage = cpu_usage;
    entry->memory_usage = memory_usage;
    entry->timestamp = time(NULL);
    
    // Update counters
    if (history->entry_count < MAX_HISTORY_ENTRIES) {
        history->entry_count++;
    }
    history->next_entry = (history->next_entry + 1) % MAX_HISTORY_ENTRIES;
    
    pthread_mutex_unlock(&history_mutex);
    return 0;
}

int proc_history_get_entries(pid_t pid, proc_history_entry_t *entries, int max_entries) {
    pthread_mutex_lock(&history_mutex);
    
    // Find process history
    proc_history_t *history = NULL;
    for (int i = 0; i < history_count; i++) {
        if (process_histories[i].pid == pid) {
            history = &process_histories[i];
            break;
        }
    }
    
    if (history == NULL || history->entry_count == 0) {
        pthread_mutex_unlock(&history_mutex);
        return 0;
    }
    
    // Calculate how many entries to return
    int entries_to_return = (max_entries < history->entry_count) ? 
                           max_entries : history->entry_count;
    
    // Copy entries in chronological order
    int start = (history->next_entry - history->entry_count + MAX_HISTORY_ENTRIES) % MAX_HISTORY_ENTRIES;
    for (int i = 0; i < entries_to_return; i++) {
        int idx = (start + i) % MAX_HISTORY_ENTRIES;
        entries[i] = history->entries[idx];
    }
    
    pthread_mutex_unlock(&history_mutex);
    return entries_to_return;
}

void proc_history_clear_process(pid_t pid) {
    pthread_mutex_lock(&history_mutex);
    
    for (int i = 0; i < history_count; i++) {
        if (process_histories[i].pid == pid) {
            // If it's not the last entry, move the last entry to this position
            if (i < history_count - 1) {
                process_histories[i] = process_histories[history_count - 1];
            }
            history_count--;
            break;
        }
    }
    
    pthread_mutex_unlock(&history_mutex);
} 