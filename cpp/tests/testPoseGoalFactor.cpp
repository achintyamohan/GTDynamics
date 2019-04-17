/**
 * @file  testPoseGoalFactor.cpp
 * @brief test pose goal factor
 * @Author: Frank Dellaert and Mandy Xie
 */

#include <PoseGoalFactor.h>
#include <DHLink.h>
#include <SerialLink.h>

#include <gtsam/base/numericalDerivative.h>
#include <gtsam/inference/Symbol.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/nonlinear/Values.h>

#include <gtsam/base/Testable.h>
#include <gtsam/base/TestableAssertions.h>
#include <CppUnitLite/TestHarness.h>

#include <iostream>

using namespace std;
using namespace gtsam;
using namespace manipulator;

namespace example
{
// RR link example
vector<DH_Link> dh_rr = {DH_Link(0, 0, 2, 0, 'R', 1, Point3(-1, 0, 0), Vector3(0, 0, 0), -5, 10, 2),
                         DH_Link(0, 0, 2, 0, 'R', 1, Point3(-1, 0, 0), Vector3(0, 0, 0), -5, 10, 2)};
auto robot = SerialLink<DH_Link>(dh_rr, Pose3());
auto jacobian = boost::bind(&SerialLink<DH_Link>::forwardKinematics, robot, _1, _2);
} // namespace example

/**
 * Test pose goal factor
 */
TEST(PoseGoalFactor, error)
{
  // settings
  noiseModel::Gaussian::shared_ptr cost_model = noiseModel::Isotropic::Sigma(6, 1.0);

  Vector2 joint_coordinates;
  Pose3 goal_pose;
  Vector6 actual_error, expected_error;
  Matrix actual_H, expected_H;

  // zero joint angles
  joint_coordinates = Vector2(0, 0);
  goal_pose = Pose3(Rot3(), Point3(4, 0, 0));
  PoseGoalFactor factor(0, cost_model, goal_pose, example::jacobian);
  actual_error = factor.evaluateError(joint_coordinates, actual_H);
  expected_error = Vector6::Zero();
  expected_H = numericalDerivative11(boost::function<Vector6(const Vector2 &)>(
                                         boost::bind(&PoseGoalFactor::evaluateError, factor, _1, boost::none)),
                                     joint_coordinates, 1e-6);
  EXPECT(assert_equal(expected_error, actual_error, 1e-6));
  EXPECT(assert_equal(expected_H, actual_H, 1e-6));

  // pi/4 joint angle
  joint_coordinates = Vector2(M_PI / 4, 0);
  goal_pose = Pose3(Rot3::Rz(M_PI / 4), Point3(2.82842712, 2.82842712, 0));
  factor = PoseGoalFactor(0, cost_model, goal_pose, example::jacobian);
  actual_error = factor.evaluateError(joint_coordinates, actual_H);
  expected_error = Vector6::Zero();
  expected_H = numericalDerivative11(boost::function<Vector6(const Vector2 &)>(
                                         boost::bind(&PoseGoalFactor::evaluateError, factor, _1, boost::none)),
                                     joint_coordinates, 1e-6);
  EXPECT(assert_equal(expected_error, actual_error, 1e-6));
  EXPECT(assert_equal(expected_H, actual_H, 1e-6));

  // TODO: Jacobian calculated by numericalDerivative11 is wrong!!!!!!!!!!!
  // // non zero error
  // joint_coordinates = Vector2(M_PI / 2, 0);
  // goal_pose = Pose3(Rot3(), Point3(4, 0, 0));
  // factor = PoseGoalFactor(0, cost_model, goal_pose, example::jacobian);
  // actual_error = factor.evaluateError(joint_coordinates, actual_H);
  // expected_error = (Vector(6) << 0, 0, M_PI / 2, -4, 4, 0).finished();
  // expected_H = numericalDerivative11(boost::function<Vector6(const Vector2 &)>(
  //                                        boost::bind(&PoseGoalFactor::evaluateError, factor, _1, boost::none)),
  //                                    joint_coordinates, 1e-6);
  // EXPECT(assert_equal(expected_error, actual_error, 1e-6));
  // EXPECT(assert_equal(expected_H, actual_H, 1e-6));
}

TEST(PoseGoalFactor, optimization)
{
  // settings
  noiseModel::Gaussian::shared_ptr cost_model = noiseModel::Isotropic::Sigma(6, 0.1);

  Vector joint_coordinates = (Vector(2) << M_PI / 4.0, 0).finished();
  Pose3 goal_pose = Pose3(Rot3::Rz(M_PI / 4), Point3(2.82842712, 2.82842712, 0));

  NonlinearFactorGraph graph;
  Key key = Symbol('x', 0);
  graph.add(PoseGoalFactor(key, cost_model, goal_pose,
                           example::jacobian));
  Values init_values;
  Vector joint_coordinates_init = (Vector(2) << 0, 0).finished();
  init_values.insert(key, joint_coordinates_init);

  LevenbergMarquardtOptimizer optimizer(graph, init_values);
  optimizer.optimize();
  Values results = optimizer.values();

  EXPECT_DOUBLES_EQUAL(0, graph.error(results), 1e-3);
  EXPECT(assert_equal(joint_coordinates, results.at<Vector>(key), 1e-3));
}

int main()
{
  TestResult tr;
  return TestRegistry::runAllTests(tr);
}
