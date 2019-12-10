// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_WINDOW_ARC_TRACKER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_WINDOW_ARC_TRACKER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/arc_util.h"

namespace aura {
class window;
}

class AppServiceAppWindowLauncherController;
class Profile;

// AppServiceAppWindowArcTracker observes the ArcAppListPrefs to handle ARC app
// window special cases, e.g. task id, closing ARC app windows, etc.
//
// TODO(crbug.com/1011235):
// 1. Add ActiveUserChanged to handle the user switch case.
// 2. Add PlatStore launch handling
class AppServiceAppWindowArcTracker : public ArcAppListPrefs::Observer {
 public:
  explicit AppServiceAppWindowArcTracker(
      AppServiceAppWindowLauncherController* app_service_controller);
  ~AppServiceAppWindowArcTracker() override;

  AppServiceAppWindowArcTracker(const AppServiceAppWindowArcTracker&) = delete;
  AppServiceAppWindowArcTracker& operator=(
      const AppServiceAppWindowArcTracker&) = delete;

  // Invoked by controller to notify |window| visibility is changing.
  void OnWindowVisibilityChanging(aura::Window* window);

  // ArcAppListPrefs::Observer:
  void OnTaskCreated(int task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent) override;
  void OnTaskDescriptionUpdated(
      int32_t task_id,
      const std::string& label,
      const std::vector<uint8_t>& icon_png_data) override;
  void OnTaskDestroyed(int task_id) override;
  void OnTaskSetActive(int32_t task_id) override;

  // Attaches controller and sets window's property when |window| is an ARC
  // window and has the related task id.
  void AttachControllerToWindow(aura::Window* window);

  // Adds the app window to |arc_window_candidates_|.
  void AddCandidateWindow(aura::Window* window);
  // Removes the app window from |arc_window_candidates_|.
  void RemoveCandidateWindow(aura::Window* window);

  ash::ShelfID GetShelfId(int task_id) const;

 private:
  class ArcAppWindowInfo;

  using TaskIdToArcAppWindowInfo =
      std::map<int, std::unique_ptr<ArcAppWindowInfo>>;

  // Checks |arc_window_candidates_| and attaches controller when they
  // are ARC app windows and have task id.
  void CheckAndAttachControllers();

  Profile* const observed_profile_;
  AppServiceAppWindowLauncherController* const app_service_controller_;

  TaskIdToArcAppWindowInfo task_id_to_arc_app_window_info_;

  // ARC app task id could be created after the window initialized.
  // |arc_window_candidates_| is used to record those initialized ARC app
  // windows, which haven't been assigned a task id. When a task id is created,
  // the windows in |arc_window_candidates_| will be checked and attach the task
  // id. Once the window is assigned a task id, the window is removed from
  // |arc_window_candidates_|.
  std::set<aura::Window*> arc_window_candidates_;

  int active_task_id_ = arc::kNoTaskId;
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_WINDOW_ARC_TRACKER_H_
