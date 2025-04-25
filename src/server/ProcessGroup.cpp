#include "server/ProcessGroup.hpp"
#include "server/ProcessControl.hpp"
#include "server/ProcessCore.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>

namespace qnx {
ProcessGroup &qnx::ProcessGroup::getInstance() {
  static ProcessGroup instance;
  return instance;
}

int qnx::ProcessGroup::createGroup(std::string_view name, int priority,
                                   std::string_view description) {
  std::lock_guard<std::mutex> lock(mutex_);

  int group_id = next_group_id_++;
  groups_[group_id] = Group(group_id, name, priority, description);

  return group_id;
}

bool qnx::ProcessGroup::deleteGroup(int group_id) {
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

bool qnx::ProcessGroup::renameGroup(int group_id, std::string_view new_name) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    return false;
  }

  it->second.name = new_name;
  return true;
}

bool qnx::ProcessGroup::addProcessToGroup(pid_t pid, int group_id) {
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

bool qnx::ProcessGroup::removeProcessFromGroup(pid_t pid, int group_id) {
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

int qnx::ProcessGroup::getProcessGroupId(pid_t pid) const {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = process_group_map_.find(pid);
  if (it != process_group_map_.end()) {
    return it->second;
  }

  return -1;
}

std::set<pid_t> qnx::ProcessGroup::getProcessesInGroup(int group_id) const {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = groups_.find(group_id);
  if (it != groups_.end()) {
    return it->second.processes;
  }

  return std::set<pid_t>();
}

void qnx::ProcessGroup::updateGroupStats() {
  std::lock_guard<std::mutex> lock(mutex_);

  // Reset all group stats
  for (auto &group_pair : groups_) {
    Group &group = group_pair.second;
    group.total_cpu_usage = 0.0;
    group.total_memory_usage = 0;
    group.num_processes = 0;

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
        auto proc_info_opt = ProcessCore::getInstance().getProcessById(pid);
        if (proc_info_opt) {
          const auto &proc_info = *proc_info_opt;
          group.total_memory_usage += proc_info.memory_usage;
          group.total_cpu_usage += proc_info.cpu_usage;
          group.num_processes++;
        }
      }
    }

    // Remove dead processes
    for (pid_t pid : to_remove) {
      group.processes.erase(pid);
    }
  }
}

void qnx::ProcessGroup::displayGroups() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "--- Process Groups ---" << std::endl;
  for (const auto &pair : groups_) {
    const Group &group = pair.second;
    std::cout << "Group ID: " << group.id << ", Name: " << group.name
              << ", Priority: " << group.priority
              << ", Desc: " << group.description
              << ", Processes: " << group.num_processes
              << ", CPU: " << std::fixed << std::setprecision(1)
              << group.total_cpu_usage << "%"
              << ", Memory: " << group.total_memory_usage / 1024
              << " KB" // Assuming KB
              << std::endl;
    // Optionally print PIDs in the group
    // std::cout << "  PIDs: ";
    // for(pid_t pid : group.processes) {
    //     std::cout << pid << " ";
    // }
    // std::cout << std::endl;
  }
  std::cout << "----------------------" << std::endl;
}

void qnx::ProcessGroup::prioritizeGroup(int group_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = groups_.find(group_id);
  if (it == groups_.end()) {
    std::cerr << "Error: Cannot prioritize non-existent group " << group_id
              << std::endl;
    return;
  }

  // Example: Set a higher priority (lower number is higher prio in POSIX/QNX)
  // This is a simplified example; real implementation might involve
  // iterating through processes and calling ProcessCore::adjustPriority
  // with appropriate logic based on group membership or policy.
  int new_base_priority = 10; // Example high priority
  int policy = SCHED_RR;      // Example policy

  // Silence unused variable warnings for placeholder implementation
  (void)new_base_priority;
  (void)policy;

  std::cout << "Prioritizing group " << group_id << " (processes: ";
  for (pid_t pid : it->second.processes) {
    std::cout << pid << " ";
    // Note: Adjusting priority needs careful consideration of permissions and
    // policy ProcessCore::getInstance().adjustPriority(pid, new_base_priority,
    // policy);
  }
  std::cout << ")" << std::endl;
  // You might update the group's priority field as well if needed
  // it->second.priority = some_value_indicating_prioritization;
}
} // namespace qnx
