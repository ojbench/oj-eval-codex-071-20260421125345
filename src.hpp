#pragma once

#include <vector>
#include <queue>
#include <map>
#include <utility>
#include <cstddef>
#include <algorithm>

namespace oj {

// The OJ provides Task/Description/Launch/Saving/Cancel/Policy and PublicInformation.
// We do not include or redefine them here; just use the types.

// Helper to distribute a total sum across items while respecting per-item bounds.
static inline void distribute_sum(std::vector<std::size_t>& arr,
                                  std::size_t per_min, std::size_t per_max,
                                  std::size_t sum_min, std::size_t sum_max) {
  std::fill(arr.begin(), arr.end(), per_min);
  const std::size_t n = arr.size();
  unsigned __int128 base_sum = static_cast<unsigned __int128>(per_min) * n;
  if (base_sum > sum_max) return; // infeasible; leave as per_min
  unsigned __int128 need = 0;
  if (base_sum < sum_min) need = sum_min - base_sum;
  for (std::size_t i = 0; i < n && need > 0; ++i) {
    std::size_t can_add = per_max - arr[i];
    unsigned __int128 add = std::min<unsigned __int128>(can_add, need);
    arr[i] += static_cast<std::size_t>(add);
    need -= add;
  }
}

auto generate_tasks(const Description &desc) -> std::vector<Task> {
  std::vector<Task> tasks(desc.task_count);

  // Execution times: satisfy per-task and total ranges.
  std::vector<std::size_t> exec(desc.task_count);
  distribute_sum(exec,
                 desc.execution_time_single.min, desc.execution_time_single.max,
                 desc.execution_time_sum.min,   desc.execution_time_sum.max);

  // Priorities: satisfy per-task and total ranges.
  std::vector<std::size_t> pris(desc.task_count);
  distribute_sum(pris,
                 desc.priority_single.min, desc.priority_single.max,
                 desc.priority_sum.min,    desc.priority_sum.max);

  // Launch times: simple staggering to spread arrivals a bit.
  for (std::size_t i = 0; i < desc.task_count; ++i) {
    std::size_t launch = static_cast<std::size_t>(i % 8);
    std::size_t ddl = desc.deadline_time.max;
    if (ddl < desc.deadline_time.min) ddl = desc.deadline_time.min;
    tasks[i] = Task{launch, ddl, exec[i], static_cast<priority_t>(pris[i])};
  }
  return tasks;
}

// ---------------- Scheduler ----------------
// Simple FIFO 1-CPU-per-task scheduler.
static std::queue<std::pair<std::size_t, Task>> g_queue;
static task_id_t g_next_task_id = 0;
static std::map<time_t, std::vector<task_id_t>> g_save_at;
static cpu_id_t g_free_cpu = PublicInformation::kCPUCount;

auto schedule_tasks(time_t time, std::vector<Task> list, const Description & /*desc*/) -> std::vector<Policy> {
  std::vector<Policy> actions;

  // Enqueue new arrivals with assigned IDs.
  for (std::size_t i = 0; i < list.size(); ++i) {
    g_queue.emplace(g_next_task_id + i, list[i]);
  }

  // Process any saves scheduled for this time.
  if (auto it = g_save_at.find(time); it != g_save_at.end()) {
    for (auto id : it->second) actions.emplace_back(Saving{id});
  }

  // CPUs saved from jobs that finished saving at (time - kSaving) become free now.
  if (time >= PublicInformation::kSaving) {
    auto it2 = g_save_at.find(time - PublicInformation::kSaving);
    if (it2 != g_save_at.end()) g_free_cpu += static_cast<cpu_id_t>(it2->second.size());
  }

  // Launch as many as we have free CPUs (1 CPU per task).
  while (!g_queue.empty() && g_free_cpu > 0) {
    auto [tid, t] = g_queue.front();
    g_queue.pop();
    actions.emplace_back(Launch{1, tid});
    const time_t save_time = time + PublicInformation::kStartUp + t.execution_time;
    g_save_at[save_time].push_back(tid);
    --g_free_cpu;
  }

  g_next_task_id += list.size();
  return actions;
}

} // namespace oj
