// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_manager.h"

#if defined(ENABLE_GPU)

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/sys_info.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/common/gpu/gpu_memory_tracking.h"
#include "gpu/command_buffer/service/gpu_switches.h"

namespace content {
namespace {

const int kDelayedScheduleManageTimeoutMs = 67;

bool IsInSameContextShareGroupAsAnyOf(
    const GpuCommandBufferStubBase* stub,
    const std::vector<GpuCommandBufferStubBase*>& stubs) {
  for (std::vector<GpuCommandBufferStubBase*>::const_iterator it =
      stubs.begin(); it != stubs.end(); ++it) {
    if (stub->IsInSameContextShareGroup(**it))
      return true;
  }
  return false;
}

void AssignMemoryAllocations(
    const std::vector<GpuCommandBufferStubBase*>& stubs,
    const GpuMemoryAllocation& allocation) {
  for (std::vector<GpuCommandBufferStubBase*>::const_iterator it =
          stubs.begin();
      it != stubs.end();
      ++it) {
    (*it)->SetMemoryAllocation(allocation);
  }
}

}

GpuMemoryManager::GpuMemoryManager(GpuMemoryManagerClient* client,
        size_t max_surfaces_with_frontbuffer_soft_limit)
    : client_(client),
      manage_immediate_scheduled_(false),
      max_surfaces_with_frontbuffer_soft_limit_(
          max_surfaces_with_frontbuffer_soft_limit),
      bytes_available_gpu_memory_(0),
      bytes_available_gpu_memory_overridden_(false),
      bytes_allocated_current_(0),
      bytes_allocated_historical_max_(0),
      window_count_has_been_received_(false),
      window_count_(0)
{
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceGpuMemAvailableMb)) {
    base::StringToSizeT(
      command_line->GetSwitchValueASCII(switches::kForceGpuMemAvailableMb),
      &bytes_available_gpu_memory_);
    bytes_available_gpu_memory_ *= 1024 * 1024;
    bytes_available_gpu_memory_overridden_ = true;
  } else
    bytes_available_gpu_memory_ = GetDefaultAvailableGpuMemory();
}

GpuMemoryManager::~GpuMemoryManager() {
  DCHECK(tracking_groups_.empty());
}

size_t GpuMemoryManager::CalcAvailableFromViewportArea(int viewport_area) {
  // We can't query available GPU memory from the system on Android, but
  // 18X the viewport and 50% of the dalvik heap size give us a good
  // estimate of available GPU memory on a wide range of devices.
  const int kViewportMultiplier = 18;
  const unsigned int kComponentsPerPixel = 4; // GraphicsContext3D::RGBA
  const unsigned int kBytesPerComponent = 1; // sizeof(GC3Dubyte)
  size_t viewport_limit = viewport_area * kViewportMultiplier *
                                          kComponentsPerPixel *
                                          kBytesPerComponent;
#if !defined(OS_ANDROID)
  return viewport_limit;
#else
  static size_t dalvik_limit = 0;
  if (!dalvik_limit)
      dalvik_limit = (base::SysInfo::DalvikHeapSizeMB() / 2) * 1024 * 1024;
  return std::min(viewport_limit, dalvik_limit);
#endif
}

size_t GpuMemoryManager::CalcAvailableFromGpuTotal(size_t total_gpu_memory) {
  // Allow Chrome to use 75% of total GPU memory, or all-but-64MB of GPU
  // memory, whichever is less.
  return std::min(3 * total_gpu_memory / 4, total_gpu_memory - 64*1024*1024);
}

void GpuMemoryManager::UpdateAvailableGpuMemory(
   std::vector<GpuCommandBufferStubBase*>& stubs) {
  // If the amount of video memory to use was specified at the command
  // line, never change it.
  if (bytes_available_gpu_memory_overridden_)
    return;

#if defined(OS_ANDROID)
  // On Android we use the surface size, so this finds the largest visible
  // surface size instead of lowest gpu's limit.
  int max_surface_area = 0;
  for (std::vector<GpuCommandBufferStubBase*>::iterator it = stubs.begin();
      it != stubs.end(); ++it) {
    GpuCommandBufferStubBase* stub = *it;
    gfx::Size surface_size = stub->GetSurfaceSize();
    max_surface_area = std::max(max_surface_area, surface_size.width() *
                                                  surface_size.height());
  }
  bytes_available_gpu_memory_ = CalcAvailableFromViewportArea(max_surface_area);
#else
  // We do not have a reliable concept of multiple GPUs existing in
  // a system, so just be safe and go with the minimum encountered.
  size_t bytes_min = 0;
  for (std::vector<GpuCommandBufferStubBase*>::iterator it = stubs.begin();
      it != stubs.end(); ++it) {
    GpuCommandBufferStubBase* stub = *it;
    size_t bytes = 0;
    if (stub->GetTotalGpuMemory(&bytes)) {
      if (!bytes_min || bytes < bytes_min)
        bytes_min = bytes;
    }
  }
  if (!bytes_min)
    return;

  bytes_available_gpu_memory_ = CalcAvailableFromGpuTotal(bytes_min);

#endif

  // And never go below the default allocation
  bytes_available_gpu_memory_ = std::max(bytes_available_gpu_memory_,
                                         GetDefaultAvailableGpuMemory());

  // And never go above the maximum.
  bytes_available_gpu_memory_ = std::min(bytes_available_gpu_memory_,
                                         GetMaximumTotalGpuMemory());
}

bool GpuMemoryManager::StubWithSurfaceComparator::operator()(
    GpuCommandBufferStubBase* lhs,
    GpuCommandBufferStubBase* rhs) {
  DCHECK(lhs->memory_manager_state().has_surface &&
         rhs->memory_manager_state().has_surface);
  const GpuCommandBufferStubBase::MemoryManagerState& lhs_mms =
      lhs->memory_manager_state();
  const GpuCommandBufferStubBase::MemoryManagerState& rhs_mms =
      rhs->memory_manager_state();
  if (lhs_mms.visible)
    return !rhs_mms.visible || (lhs_mms.last_used_time >
                                rhs_mms.last_used_time);
  else
    return !rhs_mms.visible && (lhs_mms.last_used_time >
                                rhs_mms.last_used_time);
};

void GpuMemoryManager::ScheduleManage(bool immediate) {
  if (manage_immediate_scheduled_)
    return;
  if (immediate) {
    MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GpuMemoryManager::Manage, AsWeakPtr()));
    manage_immediate_scheduled_ = true;
    if (!delayed_manage_callback_.IsCancelled())
      delayed_manage_callback_.Cancel();
  } else {
    if (!delayed_manage_callback_.IsCancelled())
      return;
    delayed_manage_callback_.Reset(base::Bind(&GpuMemoryManager::Manage,
                                              AsWeakPtr()));
    MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      delayed_manage_callback_.callback(),
      base::TimeDelta::FromMilliseconds(kDelayedScheduleManageTimeoutMs));
  }
}

void GpuMemoryManager::TrackMemoryAllocatedChange(size_t old_size,
                                                  size_t new_size) {
  if (new_size < old_size) {
    size_t delta = old_size - new_size;
    DCHECK(bytes_allocated_current_ >= delta);
    bytes_allocated_current_ -= delta;
  } else {
    size_t delta = new_size - old_size;
    bytes_allocated_current_ += delta;
    if (bytes_allocated_current_ > bytes_allocated_historical_max_) {
      bytes_allocated_historical_max_ = bytes_allocated_current_;
    }
  }
  if (new_size != old_size) {
    TRACE_COUNTER1("gpu",
                   "GpuMemoryUsage",
                   bytes_allocated_current_);
  }
}

void GpuMemoryManager::AddTrackingGroup(
    GpuMemoryTrackingGroup* tracking_group) {
  tracking_groups_.insert(tracking_group);
}

void GpuMemoryManager::RemoveTrackingGroup(
    GpuMemoryTrackingGroup* tracking_group) {
  tracking_groups_.erase(tracking_group);
}

void GpuMemoryManager::GetVideoMemoryUsageStats(
    GPUVideoMemoryUsageStats& video_memory_usage_stats) const {
  // For each context group, assign its memory usage to its PID
  video_memory_usage_stats.process_map.clear();
  for (std::set<GpuMemoryTrackingGroup*>::const_iterator i =
       tracking_groups_.begin(); i != tracking_groups_.end(); ++i) {
    const GpuMemoryTrackingGroup* tracking_group = (*i);
    video_memory_usage_stats.process_map[
        tracking_group->GetPid()].video_memory += tracking_group->GetSize();
  }

  // Assign the total across all processes in the GPU process
  video_memory_usage_stats.process_map[
      base::GetCurrentProcId()].video_memory = bytes_allocated_current_;
  video_memory_usage_stats.process_map[
      base::GetCurrentProcId()].has_duplicates = true;
}

void GpuMemoryManager::SetWindowCount(uint32 window_count) {
  bool should_schedule_manage = !window_count_has_been_received_ ||
                                (window_count != window_count_);
  window_count_has_been_received_ = true;
  window_count_ = window_count;
  if (should_schedule_manage)
    ScheduleManage(true);
}

// The current Manage algorithm simply classifies contexts (stubs) into
// "foreground", "background", or "hibernated" categories.
// For each of these three categories, there are predefined memory allocation
// limits and front/backbuffer states.
//
// Stubs may or may not have a surfaces, and the rules are different for each.
//
// The rules for categorizing contexts with a surface are:
//  1. Foreground: All visible surfaces.
//                 * Must have both front and back buffer.
//
//  2. Background: Non visible surfaces, which have not surpassed the
//                 max_surfaces_with_frontbuffer_soft_limit_ limit.
//                 * Will have only a frontbuffer.
//
//  3. Hibernated: Non visible surfaces, which have surpassed the
//                 max_surfaces_with_frontbuffer_soft_limit_ limit.
//                 * Will not have either buffer.
//
// The considerations for categorizing contexts without a surface are:
//  1. These contexts do not track {visibility,last_used_time}, so cannot
//     sort them directly.
//  2. These contexts may be used by, and thus affect, other contexts, and so
//     cannot be less visible than any affected context.
//  3. Contexts belong to share groups within which resources can be shared.
//
// As such, the rule for categorizing contexts without a surface is:
//  1. Find the most visible context-with-a-surface within each
//     context-without-a-surface's share group, and inherit its visibilty.
void GpuMemoryManager::Manage() {
  manage_immediate_scheduled_ = false;
  delayed_manage_callback_.Cancel();

  // Create stub lists by separating out the two types received from client
  std::vector<GpuCommandBufferStubBase*> stubs_with_surface;
  std::vector<GpuCommandBufferStubBase*> stubs_without_surface;
  {
    std::vector<GpuCommandBufferStubBase*> stubs;
    client_->AppendAllCommandBufferStubs(stubs);

    for (std::vector<GpuCommandBufferStubBase*>::iterator it = stubs.begin();
        it != stubs.end(); ++it) {
      GpuCommandBufferStubBase* stub = *it;
      if (!stub->memory_manager_state().
         client_has_memory_allocation_changed_callback)
        continue;
      if (stub->memory_manager_state().has_surface)
        stubs_with_surface.push_back(stub);
      else
        stubs_without_surface.push_back(stub);
    }
  }

  // Sort stubs with surface into {visibility,last_used_time} order using
  // custom comparator
  std::sort(stubs_with_surface.begin(),
            stubs_with_surface.end(),
            StubWithSurfaceComparator());
  DCHECK(std::unique(stubs_with_surface.begin(), stubs_with_surface.end()) ==
         stubs_with_surface.end());

  // Separate stubs into memory allocation sets.
  std::vector<GpuCommandBufferStubBase*> stubs_with_surface_foreground,
                                         stubs_with_surface_background,
                                         stubs_with_surface_hibernated,
                                         stubs_without_surface_foreground,
                                         stubs_without_surface_background,
                                         stubs_without_surface_hibernated;

  for (size_t i = 0; i < stubs_with_surface.size(); ++i) {
    GpuCommandBufferStubBase* stub = stubs_with_surface[i];
    DCHECK(stub->memory_manager_state().has_surface);
    if (stub->memory_manager_state().visible)
      stubs_with_surface_foreground.push_back(stub);
    else if (i < max_surfaces_with_frontbuffer_soft_limit_)
      stubs_with_surface_background.push_back(stub);
    else
      stubs_with_surface_hibernated.push_back(stub);
  }
  for (std::vector<GpuCommandBufferStubBase*>::const_iterator it =
      stubs_without_surface.begin(); it != stubs_without_surface.end(); ++it) {
    GpuCommandBufferStubBase* stub = *it;
    DCHECK(!stub->memory_manager_state().has_surface);

    // Stubs without surfaces have deduced allocation state using the state
    // of surface stubs which are in the same context share group.
    if (IsInSameContextShareGroupAsAnyOf(stub, stubs_with_surface_foreground))
      stubs_without_surface_foreground.push_back(stub);
    else if (IsInSameContextShareGroupAsAnyOf(
        stub, stubs_with_surface_background))
      stubs_without_surface_background.push_back(stub);
    else
      stubs_without_surface_hibernated.push_back(stub);
  }

  // Update the amount of GPU memory available on the system. Only use the
  // stubs that are visible, because otherwise the set of stubs we are
  // querying could become extremely large (resulting in hundreds of calls
  // to the driver).
  UpdateAvailableGpuMemory(stubs_with_surface_foreground);

  size_t bonus_allocation = 0;
  // Calculate bonus allocation by splitting remainder of global limit equally
  // after giving out the minimum to those that need it.
  size_t num_stubs_need_mem = stubs_with_surface_foreground.size() +
                              stubs_without_surface_foreground.size() +
                              stubs_without_surface_background.size();
  size_t base_allocation_size = GetMinimumTabAllocation() * num_stubs_need_mem;
  if (base_allocation_size < GetAvailableGpuMemory() &&
      !stubs_with_surface_foreground.empty())
    bonus_allocation = (GetAvailableGpuMemory() - base_allocation_size) /
                       stubs_with_surface_foreground.size();
  size_t stubs_with_surface_foreground_allocation = GetMinimumTabAllocation() +
                                                    bonus_allocation;

  // If we have received a window count message, then override the stub-based
  // scheme with a per-window scheme
  if (window_count_has_been_received_) {
    stubs_with_surface_foreground_allocation = std::max(
       stubs_with_surface_foreground_allocation,
       GetAvailableGpuMemory()/std::max(window_count_, 1u));
  }

  // Limit the memory per stub to its maximum allowed level.
  if (stubs_with_surface_foreground_allocation >= GetMaximumTabAllocation())
    stubs_with_surface_foreground_allocation = GetMaximumTabAllocation();

  // Now give out allocations to everyone.
  AssignMemoryAllocations(
      stubs_with_surface_foreground,
      GpuMemoryAllocation(stubs_with_surface_foreground_allocation,
                          GpuMemoryAllocation::kHasFrontbuffer));

  AssignMemoryAllocations(
      stubs_with_surface_background,
      GpuMemoryAllocation(stubs_with_surface_foreground_allocation,
                          GpuMemoryAllocation::kHasFrontbuffer));

  AssignMemoryAllocations(
      stubs_with_surface_hibernated,
      GpuMemoryAllocation(stubs_with_surface_foreground_allocation,
                          GpuMemoryAllocation::kHasNoFrontbuffer));

  AssignMemoryAllocations(
      stubs_without_surface_foreground,
      GpuMemoryAllocation(GetMinimumTabAllocation(),
                          GpuMemoryAllocation::kHasNoFrontbuffer));

  AssignMemoryAllocations(
      stubs_without_surface_background,
      GpuMemoryAllocation(GetMinimumTabAllocation(),
                          GpuMemoryAllocation::kHasNoFrontbuffer));

  AssignMemoryAllocations(
      stubs_without_surface_hibernated,
      GpuMemoryAllocation(0, GpuMemoryAllocation::kHasNoFrontbuffer));
}

}  // namespace content

#endif
