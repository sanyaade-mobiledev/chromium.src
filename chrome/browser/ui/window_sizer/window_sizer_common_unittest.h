// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WINDOW_SIZER_WINDOW_SIZER_COMMON_UNITTEST_H_
#define CHROME_BROWSER_UI_WINDOW_SIZER_WINDOW_SIZER_COMMON_UNITTEST_H_

#include <vector>

#include "base/logging.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/test/base/test_browser_window.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "ui/gfx/rect.h"

// Some standard monitor sizes (no task bar).
static const gfx::Rect tentwentyfour(0, 0, 1024, 768);
static const gfx::Rect twelveeighty(0, 0, 1280, 1024);
static const gfx::Rect sixteenhundred(0, 0, 1600, 1200);
static const gfx::Rect sixteeneighty(0, 0, 1680, 1050);
static const gfx::Rect nineteentwenty(0, 0, 1920, 1200);

// Represents a 1024x768 monitor that is not the primary monitor, arranged to
// the immediate left of the primary 1024x768 monitor.
static const gfx::Rect left_nonprimary(-1024, 0, 1024, 768);

// Represents a 1024x768 monitor that is not the primary monitor, arranged to
// the immediate right of the primary 1024x768 monitor.
static const gfx::Rect right_nonprimary(1024, 0, 1024, 768);

// Represents a 1024x768 monitor that is not the primary monitor, arranged to
// the immediate top of the primary 1024x768 monitor.
static const gfx::Rect top_nonprimary(0, -768, 1024, 768);

// Represents a 1024x768 monitor that is not the primary monitor, arranged to
// the immediate bottom of the primary 1024x768 monitor.
static const gfx::Rect bottom_nonprimary(0, 768, 1024, 768);

// The work area for 1024x768 monitors with different taskbar orientations.
static const gfx::Rect taskbar_bottom_work_area(0, 0, 1024, 734);
static const gfx::Rect taskbar_top_work_area(0, 34, 1024, 734);
static const gfx::Rect taskbar_left_work_area(107, 0, 917, 768);
static const gfx::Rect taskbar_right_work_area(0, 0, 917, 768);

extern int kWindowTilePixels;

// Testing implementation of WindowSizer::MonitorInfoProvider that we can use
// to fake various monitor layouts and sizes.
class TestMonitorInfoProvider : public MonitorInfoProvider {
 public:
  TestMonitorInfoProvider();
  virtual ~TestMonitorInfoProvider();

  void AddMonitor(const gfx::Rect& bounds, const gfx::Rect& work_area);

  // Overridden from WindowSizer::MonitorInfoProvider:
  virtual gfx::Rect GetPrimaryDisplayWorkArea() const OVERRIDE;

  virtual gfx::Rect GetPrimaryDisplayBounds() const OVERRIDE;

  virtual gfx::Rect GetMonitorWorkAreaMatching(
      const gfx::Rect& match_rect) const OVERRIDE;

 private:
  size_t GetMonitorIndexMatchingBounds(const gfx::Rect& match_rect) const;

  std::vector<gfx::Rect> monitor_bounds_;
  std::vector<gfx::Rect> work_areas_;

  DISALLOW_COPY_AND_ASSIGN(TestMonitorInfoProvider);
};

// Testing implementation of WindowSizer::StateProvider that we use to fake
// persistent storage and existing windows.
class TestStateProvider : public WindowSizer::StateProvider {
 public:
  TestStateProvider();
  virtual ~TestStateProvider() {};

  void SetPersistentState(const gfx::Rect& bounds,
                          const gfx::Rect& work_area,
                          ui::WindowShowState show_state,
                          bool has_persistent_data);
  void SetLastActiveState(const gfx::Rect& bounds,
                          ui::WindowShowState show_state,
                          bool has_last_active_data);

  // Overridden from WindowSizer::StateProvider:
  virtual bool GetPersistentState(
      gfx::Rect* bounds,
      gfx::Rect* saved_work_area,
      ui::WindowShowState* show_state) const OVERRIDE;
  virtual bool GetLastActiveWindowState(
      gfx::Rect* bounds,
      ui::WindowShowState* show_state) const OVERRIDE;

 private:
  gfx::Rect persistent_bounds_;
  gfx::Rect persistent_work_area_;
  bool has_persistent_data_;
  ui::WindowShowState persistent_show_state_;

  gfx::Rect last_active_bounds_;
  bool has_last_active_data_;
  ui::WindowShowState last_active_show_state_;

  DISALLOW_COPY_AND_ASSIGN(TestStateProvider);
};

// Several convenience functions which allow to set up a state for
// window sizer test operations with a single call.

enum Source { DEFAULT, LAST_ACTIVE, PERSISTED, BOTH };

// Set up the window bounds, monitor bounds, show states and more to get the
// resulting |out_bounds| and |out_show_state| from the WindowSizer.
// |source| specifies which type of data gets set for the test: Either the
// last active window, the persisted value which was stored earlier, both or
// none. For all these states the |bounds| and |work_area| get used, for the
// show states either |show_state_persisted| or |show_state_last| will be used.
void GetWindowBoundsAndShowState(const gfx::Rect& monitor1_bounds,
                                 const gfx::Rect& monitor1_work_area,
                                 const gfx::Rect& monitor2_bounds,
                                 const gfx::Rect& bounds,
                                 const gfx::Rect& work_area,
                                 ui::WindowShowState show_state_persisted,
                                 ui::WindowShowState show_state_last,
                                 Source source,
                                 const Browser* browser,
                                 const gfx::Rect& passed_in,
                                 gfx::Rect* out_bounds,
                                 ui::WindowShowState* out_show_state);

// Set up the window bounds, monitor bounds, and work area to get the
// resulting |out_bounds| from the WindowSizer.
// |source| specifies which type of data gets set for the test: Either the
// last active window, the persisted value which was stored earlier, both or
// none. For all these states the |bounds| and |work_area| get used, for the
// show states either |show_state_persisted| or |show_state_last| will be used.
void GetWindowBounds(const gfx::Rect& monitor1_bounds,
                     const gfx::Rect& monitor1_work_area,
                     const gfx::Rect& monitor2_bounds,
                     const gfx::Rect& bounds,
                     const gfx::Rect& work_area,
                     Source source,
                     const Browser* browser,
                     const gfx::Rect& passed_in,
                     gfx::Rect* out_bounds);

// Set up the various show states and get the resulting show state from
// the WindowSizer.
ui::WindowShowState GetWindowShowState(
    ui::WindowShowState show_state_persisted,
    ui::WindowShowState show_state_last,
    Source source,
    const Browser* browser);

#endif  // CHROME_BROWSER_UI_WINDOW_SIZER_WINDOW_SIZER_COMMON_UNITTEST_H_
