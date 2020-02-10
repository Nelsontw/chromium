// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_controller.h"

#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_test_utils.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/system_clock/system_clock_client.h"
#include "chromeos/settings/timezone_settings.h"
#include "components/arc/mojom/app.mojom.h"
#include "components/arc/test/fake_app_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

namespace chromeos {
namespace app_time {

namespace {

constexpr char kStartTime[] = "1 Jan 2020 00:00:00 GMT";
constexpr base::TimeDelta kDay = base::TimeDelta::FromHours(24);
constexpr base::TimeDelta kSixHours = base::TimeDelta::FromHours(6);
constexpr base::TimeDelta kOneHour = base::TimeDelta::FromHours(1);
constexpr base::TimeDelta kZeroTime = base::TimeDelta::FromSeconds(0);
constexpr char kApp1Name[] = "App1";
constexpr char kApp2Name[] = "App2";
const chromeos::app_time::AppId kApp1(apps::mojom::AppType::kArc, "1");
const chromeos::app_time::AppId kApp2(apps::mojom::AppType::kArc, "2");

}  // namespace

class AppTimeControllerTest : public testing::Test {
 protected:
  AppTimeControllerTest() = default;
  AppTimeControllerTest(const AppTimeControllerTest&) = delete;
  AppTimeControllerTest& operator=(const AppTimeControllerTest&) = delete;
  ~AppTimeControllerTest() override = default;

  void SetUp() override;
  void TearDown() override;

  void EnablePerAppTimeLimits();

  void CreateActivityForApp(const AppId& app_id,
                            base::TimeDelta active_time,
                            base::TimeDelta time_limit);

  void SimulateInstallArcApp(const AppId& app_id, const std::string& app_name);
  bool HasNotificationFor(
      const std::string& app_name,
      chromeos::app_time::AppNotification notification) const;

  AppTimeController::TestApi* test_api() { return test_api_.get(); }
  AppTimeController* controller() { return controller_.get(); }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  SystemClockClient::TestInterface* system_clock_client_test() {
    return SystemClockClient::Get()->GetTestInterface();
  }

  NotificationDisplayServiceTester& notification_tester() {
    return notification_tester_;
  }

  apps::AppServiceTest& app_service_test() { return app_service_test_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  NotificationDisplayServiceTester notification_tester_{&profile_};
  apps::AppServiceTest app_service_test_;
  ArcAppTest arc_test_;

  std::unique_ptr<AppTimeController> controller_;
  std::unique_ptr<AppTimeController::TestApi> test_api_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

void AppTimeControllerTest::SetUp() {
  SystemClockClient::InitializeFake();
  testing::Test::SetUp();

  // The tests are going to start at local midnight on January 1.
  base::Time time;
  ASSERT_TRUE(base::Time::FromString(kStartTime, &time));
  base::Time local_midnight = time.LocalMidnight();
  base::TimeDelta forward_by = local_midnight - base::Time::Now();
  task_environment_.FastForwardBy(forward_by);

  app_service_test_.SetUp(&profile_);
  arc_test_.SetUp(&profile_);
  arc_test_.app_instance()->set_icon_response_type(
      arc::FakeAppInstance::IconResponseType::ICON_RESPONSE_SKIP);
  task_environment_.RunUntilIdle();

  controller_ = std::make_unique<AppTimeController>(&profile_);
  test_api_ = std::make_unique<AppTimeController::TestApi>(controller_.get());
  SimulateInstallArcApp(kApp1, kApp1Name);
  SimulateInstallArcApp(kApp2, kApp2Name);
}

void AppTimeControllerTest::TearDown() {
  test_api_.reset();
  controller_.reset();
  arc_test_.TearDown();
  SystemClockClient::Shutdown();
  testing::Test::TearDown();
}

void AppTimeControllerTest::EnablePerAppTimeLimits() {
  scoped_feature_list_.InitAndEnableFeature(features::kPerAppTimeLimits);
}

void AppTimeControllerTest::CreateActivityForApp(const AppId& app_id,
                                                 base::TimeDelta time_active,
                                                 base::TimeDelta time_limit) {
  AppActivityRegistry* registry = controller_->app_registry();
  const AppLimit limit(AppRestriction::kTimeLimit, time_limit,
                       base::Time::Now());
  registry->SetAppLimit(app_id, limit);
  task_environment_.RunUntilIdle();

  // AppActivityRegistry uses |window| to uniquely identify between different
  // instances of the same active application. Since this test is just trying to
  // mock one instance of an application, using nullptr is good enough.
  registry->OnAppActive(app_id, /* window */ nullptr, base::Time::Now());
  task_environment_.FastForwardBy(time_active);
  if (time_active < time_limit) {
    registry->OnAppInactive(app_id, /* window */ nullptr, base::Time::Now());
  }
}

void AppTimeControllerTest::SimulateInstallArcApp(const AppId& app_id,
                                                  const std::string& app_name) {
  std::string package_name = app_id.app_id();
  arc_test_.AddPackage(CreateArcAppPackage(package_name)->Clone());
  const arc::mojom::AppInfo app = CreateArcAppInfo(package_name, app_name);
  arc_test_.app_instance()->SendPackageAppListRefreshed(package_name, {app});
  task_environment_.RunUntilIdle();
  return;
}

bool AppTimeControllerTest::HasNotificationFor(
    const std::string& app_name,
    chromeos::app_time::AppNotification notification) const {
  std::string notification_id;
  switch (notification) {
    case chromeos::app_time::AppNotification::kFiveMinutes:
    case chromeos::app_time::AppNotification::kOneMinute:
      notification_id = "time-limit-reaching-id-";
      break;
    case chromeos::app_time::AppNotification::kTimeLimitChanged:
      notification_id = "time-limit-updated-id-";
      break;
    default:
      NOTREACHED();
      break;
  }

  notification_id = base::StrCat({notification_id, app_name});

  base::Optional<message_center::Notification> message_center_notification =
      notification_tester_.GetNotification(notification_id);
  return message_center_notification.has_value();
}

TEST_F(AppTimeControllerTest, EnableFeature) {
  EnablePerAppTimeLimits();
  EXPECT_TRUE(AppTimeController::ArePerAppTimeLimitsEnabled());
}

TEST_F(AppTimeControllerTest, GetNextResetTime) {
  base::Time start_time = base::Time::Now();

  base::Time next_reset_time = test_api()->GetNextResetTime();
  base::Time local_midnight = next_reset_time.LocalMidnight();
  EXPECT_EQ(kSixHours, next_reset_time - local_midnight);

  EXPECT_TRUE(next_reset_time >= start_time);
  EXPECT_TRUE(next_reset_time <= start_time + kDay);
}

TEST_F(AppTimeControllerTest, ResetTimeReached) {
  base::Time start_time = base::Time::Now();

  // Assert that we start at midnight.
  ASSERT_EQ(start_time, start_time.LocalMidnight());

  // This App will not reach its time limit. Advances time by 1 hour.
  CreateActivityForApp(kApp1, kOneHour, kOneHour * 2);

  // This app will reach its time limit. Advances time by 1 hour.
  CreateActivityForApp(kApp2, kOneHour, kOneHour / 2);

  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp1), kOneHour);
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp2), kOneHour / 2);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp1),
            AppState::kAvailable);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp2),
            AppState::kLimitReached);

  // The default reset time is 6 hours after local midnight. Fast forward by 4
  // hours to reach it. FastForwardBy triggers the reset timer.
  task_environment().FastForwardBy(base::TimeDelta::FromHours(4));

  // Make sure that there is no activity
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp1), kZeroTime);
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp2), kZeroTime);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp1),
            AppState::kAvailable);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp2),
            AppState::kAvailable);
}

TEST_F(AppTimeControllerTest, SystemTimeChangedFastForwardByTwoDays) {
  CreateActivityForApp(kApp1, kOneHour, kOneHour * 2);
  CreateActivityForApp(kApp2, kOneHour, kOneHour / 2);

  // Advance system time with two days. TaskEnvironment::AdvanceClock doesn't
  // run the tasks that have been posted. This allows us to simulate the system
  // time changing to two days ahead without triggering the reset timer.
  task_environment().AdvanceClock(2 * kDay);

  // Since the reset timer has not been triggered the application activities are
  // instact.
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp1), kOneHour);
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp2), kOneHour / 2);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp1),
            AppState::kAvailable);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp2),
            AppState::kLimitReached);

  // Notify AppTimeController that system time has changed. This triggers reset.
  system_clock_client_test()->NotifyObserversSystemClockUpdated();

  // Make sure that there is no activity
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp1), kZeroTime);
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp2), kZeroTime);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp1),
            AppState::kAvailable);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp2),
            AppState::kAvailable);
}

TEST_F(AppTimeControllerTest, SystemTimeChangedGoingBackwards) {
  CreateActivityForApp(kApp1, kOneHour, kOneHour * 2);
  CreateActivityForApp(kApp2, kOneHour, kOneHour / 2);

  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp1), kOneHour);
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp2), kOneHour / 2);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp1),
            AppState::kAvailable);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp2),
            AppState::kLimitReached);

  // Simulate time has gone back by setting the last reset time to be in the
  // future.
  base::Time last_reset_time = test_api()->GetLastResetTime();
  test_api()->SetLastResetTime(last_reset_time + 2 * kDay);
  system_clock_client_test()->NotifyObserversSystemClockUpdated();

  // Make sure that there is no activity
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp1), kZeroTime);
  EXPECT_EQ(controller()->app_registry()->GetActiveTime(kApp2), kZeroTime);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp1),
            AppState::kAvailable);
  EXPECT_EQ(controller()->app_registry()->GetAppState(kApp2),
            AppState::kAvailable);
}

TEST_F(AppTimeControllerTest, TimeLimitNotification) {
  AppActivityRegistry* registry = controller()->app_registry();

  const AppLimit limit1(AppRestriction::kTimeLimit,
                        base::TimeDelta::FromMinutes(35), base::Time::Now());
  const AppLimit limit2(AppRestriction::kTimeLimit,
                        base::TimeDelta::FromMinutes(30), base::Time::Now());
  const std::map<AppId, AppLimit> limits{{kApp1, limit1}, {kApp2, limit2}};
  registry->UpdateAppLimits(limits);
  task_environment().RunUntilIdle();

  registry->OnAppActive(kApp1, /* window */ nullptr, base::Time::Now());
  registry->OnAppActive(kApp2, /* window */ nullptr, base::Time::Now());

  task_environment().FastForwardBy(base::TimeDelta::FromMinutes(25));

  // Expect that there is a 5 minute notification for kApp2.
  EXPECT_TRUE(HasNotificationFor(kApp2Name, AppNotification::kFiveMinutes));

  // One minute left notification will be shown and then the app will reach its
  // time limit.
  task_environment().FastForwardBy(base::TimeDelta::FromMinutes(5));

  EXPECT_TRUE(HasNotificationFor(kApp2Name, AppNotification::kOneMinute));
  EXPECT_TRUE(HasNotificationFor(kApp1Name, AppNotification::kFiveMinutes));

  task_environment().FastForwardBy(base::TimeDelta::FromMinutes(5));

  EXPECT_TRUE(HasNotificationFor(kApp1Name, AppNotification::kOneMinute));
}

}  // namespace app_time
}  // namespace chromeos
