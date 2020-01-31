/**
 * @file  testWrenchFactors.cpp
 * @brief test wrench factors
 * @Author: Yetong Zhang
 */

#include <CppUnitLite/TestHarness.h>
#include <RobotModels.h>
#include <WrenchFactors.h>
#include <gtsam/base/Testable.h>
#include <gtsam/base/TestableAssertions.h>
#include <gtsam/base/numericalDerivative.h>
#include <gtsam/inference/Symbol.h>
#include <gtsam/nonlinear/GaussNewtonOptimizer.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/Values.h>
#include <gtsam/nonlinear/factorTesting.h>

#include <iostream>

using namespace std;
using namespace gtsam;
using namespace manipulator;
using namespace robot;

namespace example {
// R link example

using namespace simple_urdf_zero_inertia;

auto inertia = my_robot.links()[0] -> inertiaMatrix();

noiseModel::Gaussian::shared_ptr cost_model =
    noiseModel::Gaussian::Covariance(I_6x6);
Key twist_key = Symbol('V', 1), twist_accel_key = Symbol('T', 1),
    wrench_1_key = Symbol('W', 1), wrench_2_key = Symbol('W', 2),
    wrench_3_key = Symbol('W', 3), wrench_4_key = Symbol('W', 4),
    qKey = Symbol('q', 1), pKey = Symbol('p', 1);
}  // namespace example

// Test wrench factor for stationary case with gravity
TEST(WrenchFactor2, error_1) {
  cout << "Inertia Matrix:" << endl << example::inertia << endl;
  // Create all factors
  Vector3 gravity;
  gravity << 0, -9.8, 0;

  WrenchFactor2 factor(example::twist_key, example::twist_accel_key,
                       example::wrench_1_key, example::wrench_2_key,
                       example::pKey, example::cost_model, example::inertia,
                       gravity);
  Vector twist, twist_accel, wrench_1, wrench_2;
  twist = (Vector(6) << 0, 0, 0, 0, 0, 0).finished();
  twist_accel = (Vector(6) << 0, 0, 0, 0, 0, 0).finished();
  wrench_1 = (Vector(6) << 0, 0, -1, 0, 4.9, 0).finished();
  wrench_2 = (Vector(6) << 0, 0, 1, 0, 4.9, 0).finished();
  Pose3 pose = Pose3(Rot3(), Point3(1, 0, 0));
  Vector6 actual_errors, expected_errors;

  actual_errors =
      factor.evaluateError(twist, twist_accel, wrench_1, wrench_2, pose);
  expected_errors << 0, 0, 0, 0, 0, 0;
  EXPECT(assert_equal(expected_errors, actual_errors, 1e-6));
  // Make sure linearization is correct
  Values values;
  values.insert(example::twist_key, twist);
  values.insert(example::twist_accel_key, twist_accel);
  values.insert(example::wrench_1_key, wrench_1);
  values.insert(example::wrench_2_key, wrench_2);
  values.insert(example::pKey, pose);
  double diffDelta = 1e-7;
  EXPECT_CORRECT_FACTOR_JACOBIANS(factor, values, diffDelta, 1e-3);
}

// Test wrench factor for stationary case with gravity
TEST(WrenchFactor3, error_1) {
  // Create all factors
  Vector3 gravity;
  gravity << 0, -9.8, 0;

  WrenchFactor3 factor(example::twist_key, example::twist_accel_key,
                       example::wrench_1_key, example::wrench_2_key,
                       example::wrench_3_key, example::pKey,
                       example::cost_model, example::inertia, gravity);
  Vector twist, twist_accel, wrench_1, wrench_2, wrench_3;
  twist = (Vector(6) << 0, 0, 0, 0, 0, 0).finished();
  twist_accel = (Vector(6) << 0, 0, 0, 0, 0, 0).finished();
  wrench_1 = (Vector(6) << 0, 0, 0, 0, 1, 0).finished();
  wrench_2 = (Vector(6) << 0, 0, 0, 0, 2, 0).finished();
  wrench_3 = (Vector(6) << 0, 0, 0, 0, 6.8, 0).finished();
  Pose3 pose = Pose3(Rot3(), Point3(1, 0, 0));
  Vector6 actual_errors, expected_errors;

  actual_errors = factor.evaluateError(twist, twist_accel, wrench_1, wrench_2,
                                       wrench_3, pose);
  expected_errors << 0, 0, 0, 0, 0, 0;
  EXPECT(assert_equal(expected_errors, actual_errors, 1e-6));
  // Make sure linearization is correct
  Values values;
  values.insert(example::twist_key, twist);
  values.insert(example::twist_accel_key, twist_accel);
  values.insert(example::wrench_1_key, wrench_1);
  values.insert(example::wrench_2_key, wrench_2);
  values.insert(example::wrench_3_key, wrench_3);
  values.insert(example::pKey, pose);
  double diffDelta = 1e-7;
  EXPECT_CORRECT_FACTOR_JACOBIANS(factor, values, diffDelta, 1e-3);
}

// Test wrench factor for stationary case with gravity
TEST(WrenchFactor4, error_1) {
  // Create all factors
  Vector3 gravity;
  gravity << 0, -9.8, 0;

  WrenchFactor4 factor(
      example::twist_key, example::twist_accel_key, example::wrench_1_key,
      example::wrench_2_key, example::wrench_3_key, example::wrench_4_key,
      example::pKey, example::cost_model, example::inertia, gravity);
  Vector twist, twist_accel, wrench_1, wrench_2, wrench_3, wrench_4;
  twist = (Vector(6) << 0, 0, 0, 0, 0, 0).finished();
  twist_accel = (Vector(6) << 0, 0, 0, 0, 0, 0).finished();
  wrench_1 = (Vector(6) << 0, 0, 0, 0, 1, 0).finished();
  wrench_2 = (Vector(6) << 0, 0, 0, 0, 1, 0).finished();
  wrench_3 = (Vector(6) << 0, 0, 0, 0, 1, 0).finished();
  wrench_4 = (Vector(6) << 0, 0, 0, 0, 6.8, 0).finished();
  Pose3 pose = Pose3(Rot3(), Point3(1, 0, 0));
  Vector6 actual_errors, expected_errors;

  actual_errors = factor.evaluateError(twist, twist_accel, wrench_1, wrench_2,
                                       wrench_3, wrench_4, pose);
  expected_errors << 0, 0, 0, 0, 0, 0;
  EXPECT(assert_equal(expected_errors, actual_errors, 1e-6));
  // Make sure linearization is correct
  Values values;
  values.insert(example::twist_key, twist);
  values.insert(example::twist_accel_key, twist_accel);
  values.insert(example::wrench_1_key, wrench_1);
  values.insert(example::wrench_2_key, wrench_2);
  values.insert(example::wrench_3_key, wrench_3);
  values.insert(example::wrench_4_key, wrench_4);
  values.insert(example::pKey, pose);
  double diffDelta = 1e-7;
  EXPECT_CORRECT_FACTOR_JACOBIANS(factor, values, diffDelta, 1e-3);
}

// Test wrench factor for non-zero twist case, zero joint angle
TEST(WrenchFactor2, error_2) {
  // Create all factors

  WrenchFactor2 factor(example::twist_key, example::twist_accel_key,
                       example::wrench_1_key, example::wrench_2_key,
                       example::pKey, example::cost_model, example::inertia);

  Vector6 twist, twist_accel, wrench_1, wrench_2;
  twist = (Vector(6) << 0, 0, 1, 0, 1, 0).finished();
  twist_accel = (Vector(6) << 0, 0, 1, 0, 1, 0).finished();
  wrench_1 = (Vector(6) << 0, 0, 4, -1, 2, 0).finished();
  wrench_2 = (Vector(6) << 0, 0, -4, 0, -1, 0).finished();
  Pose3 pose = Pose3(Rot3(), Point3(1, 0, 0));
  Vector6 actual_errors, expected_errors;

  actual_errors =
      factor.evaluateError(twist, twist_accel, wrench_1, wrench_2, pose);
  expected_errors << 0, 0, 0, 0, 0, 0;

  EXPECT(assert_equal(expected_errors, actual_errors, 1e-6));
  // Make sure linearization is correct
  Values values;
  values.insert(example::twist_key, twist);
  values.insert(example::twist_accel_key, twist_accel);
  values.insert(example::wrench_1_key, wrench_1);
  values.insert(example::wrench_2_key, wrench_2);
  values.insert(example::pKey, pose);
  double diffDelta = 1e-7;
  EXPECT_CORRECT_FACTOR_JACOBIANS(factor, values, diffDelta, 1e-3);
}

int main() {
  TestResult tr;
  return TestRegistry::runAllTests(tr);
}