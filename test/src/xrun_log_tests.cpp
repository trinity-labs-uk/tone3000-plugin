#include "XRunLogTracker.h"

#include <gtest/gtest.h>

TEST(XRunLogTrackerTest, LogsIncreasesAndPeriodicZeroCountWhileOpen) {
  XRunLogTracker tracker;
  int device = 0;

  auto baseline = tracker.observe(&device, 0, 0, 1000);
  ASSERT_TRUE(baseline);
  EXPECT_EQ(baseline->reason, XRunLogTracker::Reason::baseline);
  EXPECT_FALSE(tracker.observe(&device, 0, 0, 5000));

  auto increase = tracker.observe(&device, 2, 3, 6000);
  ASSERT_TRUE(increase);
  EXPECT_EQ(increase->reason, XRunLogTracker::Reason::increase);
  EXPECT_EQ(increase->deviceDelta, 2);
  EXPECT_EQ(increase->managerDelta, 3);
  EXPECT_EQ(increase->intervalMs, 5000u);
  EXPECT_FALSE(tracker.observe(&device, 2, 3, 65000));

  auto heartbeat = tracker.observe(&device, 2, 3, 66000);
  ASSERT_TRUE(heartbeat);
  EXPECT_EQ(heartbeat->reason, XRunLogTracker::Reason::heartbeat);
  EXPECT_EQ(heartbeat->deviceDelta, 0);
  EXPECT_EQ(heartbeat->managerDelta, 0);
  EXPECT_EQ(heartbeat->intervalMs, 60000u);

  auto summary = tracker.observe(&device, 2, 3, 69000, true);
  ASSERT_TRUE(summary);
  EXPECT_EQ(summary->reason, XRunLogTracker::Reason::summary);
  EXPECT_EQ(summary->deviceDelta, 0);
  EXPECT_EQ(summary->intervalMs, 3000u);
}

TEST(XRunLogTrackerTest, ReportsJuceOnlyXrunsWhenBackendCountUnavailable) {
  XRunLogTracker tracker;
  int device = 0;
  ASSERT_TRUE(tracker.observe(&device, -1, 0, 0));
  auto increase = tracker.observe(&device, -1, 1, 5000);
  ASSERT_TRUE(increase);
  EXPECT_EQ(increase->reason, XRunLogTracker::Reason::increase);
  EXPECT_EQ(increase->deviceTotal, -1);
  EXPECT_EQ(increase->deviceDelta, 0);
  EXPECT_EQ(increase->managerDelta, 1);
}

TEST(XRunLogTrackerTest, ReopenAndCounterResetStartFreshBaseline) {
  XRunLogTracker tracker;
  int oldDevice = 0, newDevice = 0;
  ASSERT_TRUE(tracker.observe(&oldDevice, 4, 5, 0));
  auto reopened = tracker.observe(&newDevice, 0, 0, 5000);
  ASSERT_TRUE(reopened);
  EXPECT_EQ(reopened->reason, XRunLogTracker::Reason::baseline);
  EXPECT_EQ(reopened->deviceDelta, 0);
  ASSERT_TRUE(tracker.observe(&newDevice, 2, 2, 10000));
  auto reset = tracker.observe(&newDevice, 0, 0, 15000);
  ASSERT_TRUE(reset);
  EXPECT_EQ(reset->reason, XRunLogTracker::Reason::baseline);
  EXPECT_FALSE(tracker.observe(nullptr, 0, 0, 20000));
  EXPECT_EQ(tracker.observe(&newDevice, 0, 0, 25000)->reason,
            XRunLogTracker::Reason::baseline);
}

TEST(XRunLogTrackerTest, DeviceIncreaseSurvivesJuceEstimateReset) {
  XRunLogTracker tracker;
  int device = 0;
  ASSERT_TRUE(tracker.observe(&device, 3, 8, 0));
  auto increase = tracker.observe(&device, 4, 5, 5000);
  ASSERT_TRUE(increase);
  EXPECT_EQ(increase->reason, XRunLogTracker::Reason::increase);
  EXPECT_EQ(increase->deviceDelta, 1);
  EXPECT_EQ(increase->managerDelta, 0);
  auto next = tracker.observe(&device, 4, 6, 10000);
  ASSERT_TRUE(next);
  EXPECT_EQ(next->managerDelta, 1);
}
