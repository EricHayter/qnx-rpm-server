#include "ProcessGroup.hpp"
#include "ProcessControl.hpp"
#include <algorithm>
#include <iomanip>
#include <iostream>

namespace qnx {
ProcessGroup &ProcessGroup::getInstance() {
  static ProcessGroup instance;
  return instance;
}

int ProcessGroup::createGroup(std::string_view name, int priority,
                              std::string_view description) {
  std::lock_guard<std::mutex> lock(mutex_);

  int group_id = next_group_id_++;
  groups_[group_id] = Group(group_id, name, priority, description);

  return group_id;
}

bool ProcessGroup::deleteGroup(int group_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return false;
  }

  // Remove all processes from this group in the process_group_map
  for (pid_t pid : it->second.processes) {
    process_group_map_.erase(pid);
  }

  // Remove the group
  return groups_.erase(group_id) > 0;
}

bool ProcessGroup::renameGroup(int group_id, std::string_view new_name) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return false;
  }

  it->second.name = new_name;
  return true;
}

bool ProcessGroup::addProcessToGroup(pid_t pid, int group_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return false;
  }

  // Check if process exists using the correct namespace
  if (!qnx::exists(pid)) {
    return false;
  }

  // Remove process from previous group if any
  auto prev_group_it = process_group_map_.find(pid);
  if (prev_group_it != process_group_map_.end()) {
    int prev_group_id = prev_group_it->second;
    if (prev_group_id != group_id &&
        groups_.find(prev_group_id) != groups_.end()) {
      groups_[prev_group_id].processes.erase(pid);
    }
  }

  // Add to new group
  it->second.processes.insert(pid);
  process_group_map_[pid] = group_id;

  return true;
}

bool ProcessGroup::removeProcessFromGroup(pid_t pid, int group_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return false;
  }

  auto &processes = it->second.processes;
  auto erase_count = processes.erase(pid);

  if (erase_count > 0) {
    process_group_map_.erase(pid);
    return true;
  }

  return false;
}

int ProcessGroup::getProcessGroupId(pid_t pid) const {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = process_group_map_.find(pid);
  if (it != process_group_map_.end()) {
    return it->second;
  }

  return -1;
}

std::set<pid_t> ProcessGroup::getProcessesInGroup(int group_id) const {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = groups_.find(group_id);
  if (it != groups_.end()) {
    return it->second.processes;
  }

  return std::set<pid_t>();
}

void ProcessGroup::updateGroupStats() {
  std::lock_guard<std::mutex> lock(mutex_);

  // Reset all group stats
  for (auto &group_pair : groups_) {
    Group &group = group_pair.second;
    group.total_cpu_usage = 0.0;
    group.total_memory_usage = 0;

    // Remove any processes that no longer exist
    std::set<pid_t> to_remove;
    for (pid_t pid : group.processes) {
      // Use correct namespace
      if (!qnx::exists(pid)) {
        to_remove.insert(pid);
        process_group_map_.erase(pid);
      } else {
        // Add current process stats to group totals
        // Use correct namespace
        auto proc_info = qnx::getProcessInfo(pid);
        if (proc_info) {
          group.total_cpu_usage += proc_info->cpu_usage;
          group.total_memory_usage += proc_info->memory_usage;
        }
      }
    }

    // Remove dead processes
    for (pid_t pid : to_remove) {
      group.processes.erase(pid);
    }
  }
}
} // namespace qnx