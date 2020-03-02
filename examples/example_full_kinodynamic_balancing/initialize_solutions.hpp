/* ----------------------------------------------------------------------------
 * GTDynamics Copyright 2020, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file  initialize_solutions.hpp
 * @brief Various initialization techniques for the trajectory optimization.
 * @Author: Alejandro Escontrela
 */

#ifndef EXAMPLES_EXAMPLE_FULL_KINODYNAMIC_BALANCING_INITIALIZE_SOLUTIONS_HPP_
#define EXAMPLES_EXAMPLE_FULL_KINODYNAMIC_BALANCING_INITIALIZE_SOLUTIONS_HPP_

#include <gtdynamics/dynamics/DynamicsGraph.h>
#include <gtdynamics/factors/MinTorqueFactor.h>
#include <gtdynamics/factors/PoseGoalFactor.h>
#include <gtdynamics/universal_robot/Robot.h>
#include <gtsam/base/Value.h>
#include <gtsam/base/Vector.h>

#include <string>
#include <utility>
#include <vector>

/** @fn Initialize solution via linear interpolation of initial and final pose.
 *
 * @param[in] robot           A gtdynamics::Robot object.
 * @param[in] link_name       The name of the link whose pose to interpolate.
 * @param[in] wTl_i           The initial pose of the link.
 * @param[in] wTl_f           The final pose of the link.
 * @param[in] T_i             Time at which to start interpolation.
 * @param[in] T_f             Time at which to end interpolation.
 * @param[in] dt              The duration of a single timestep.
 * @param[in] contact_points  The duration of a single timestep.
 * @return Initial solution stored in gtsam::Values object.
 */
gtsam::Values initialize_solution_interpolation(
    const gtdynamics::Robot& robot, const std::string& link_name,
    const gtsam::Pose3& wTl_i, const gtsam::Pose3& wTl_f, const double& T_i,
    const double& T_f, const double& dt,
    const boost::optional<std::vector<gtdynamics::ContactPoint>>&
        contact_points = boost::none) {
  gtsam::Values init_vals;

  int n_steps_init = static_cast<int>(std::ceil(T_i / dt));
  int n_steps_final = static_cast<int>(std::ceil(T_f / dt));
  gtsam::Point3 wPl_i = wTl_i.translation(), wPl_f = wTl_f.translation();
  gtsam::Rot3 wRl_i = wTl_i.rotation(), wRl_f = wTl_f.rotation();

  // Initialize joint angles and velocities to 0.
  gtdynamics::Robot::JointValues jangles, jvels;
  for (auto&& joint : robot.joints()) {
    jangles.insert(std::make_pair(joint->name(), 0.0));
    jvels.insert(std::make_pair(joint->name(), 0.0));
  }

  gtsam::Vector zero_twist = gtsam::Vector6::Zero(),
                zero_accel = gtsam::Vector6::Zero(),
                zero_wrench = gtsam::Vector6::Zero(),
                zero_torque = gtsam::Vector1::Zero(),
                zero_q = gtsam::Vector1::Zero(),
                zero_v = gtsam::Vector1::Zero(),
                zero_a = gtsam::Vector1::Zero();

  double t_elapsed = T_i;
  for (int t = n_steps_init; t < n_steps_final; t++) {
    double s = (t_elapsed - T_i) / (T_f - T_i);

    // Compute interpolated pose for link.
    gtsam::Point3 wPl_t = (1 - s) * wPl_i + s * wPl_f;
    gtsam::Rot3 wRl_t = wRl_i.slerp(s, wRl_f);
    gtsam::Pose3 wTl_t = gtsam::Pose3(wRl_t, wPl_t);

    // std::cout << "\t Pose: [" << wPl_t << " | " << wRl_t.rpy() << "]" <<
    // std::endl;

    // Compute forward dynamics to obtain remaining link poses.
    auto fk_results = robot.forwardKinematics(jangles, jvels, link_name, wTl_t);
    for (auto&& pose_result : fk_results.first)
      init_vals.insert(gtdynamics::PoseKey(
                           robot.getLinkByName(pose_result.first)->getID(), t),
                       pose_result.second);

    // Initialize link dynamics to 0.
    for (auto&& link : robot.links()) {
      init_vals.insert(gtdynamics::TwistKey(link->getID(), t), zero_twist);
      init_vals.insert(gtdynamics::TwistAccelKey(link->getID(), t), zero_accel);
    }

    // Initialize joint kinematics/dynamics to 0.
    for (auto&& joint : robot.joints()) {
      int j = joint->getID();
      init_vals.insert(
          gtdynamics::WrenchKey(joint->parentLink()->getID(), j, t),
          zero_wrench);
      init_vals.insert(gtdynamics::WrenchKey(joint->childLink()->getID(), j, t),
                       zero_wrench);
      init_vals.insert(gtdynamics::TorqueKey(j, t), zero_torque[0]);
      init_vals.insert(gtdynamics::JointAngleKey(j, t), zero_q[0]);
      init_vals.insert(gtdynamics::JointVelKey(j, t), zero_v[0]);
      init_vals.insert(gtdynamics::JointAccelKey(j, t), zero_a[0]);
    }

    // Initialize contacts to 0.
    if (contact_points) {
      for (auto&& contact_point : *contact_points) {
        int link_id = -1;
        for (auto& link : robot.links()) {
          if (link->name() == contact_point.name) link_id = link->getID();
        }
        if (link_id == -1) throw std::runtime_error("Link not found.");
        init_vals.insert(
            gtdynamics::ContactWrenchKey(link_id, contact_point.contact_id, t),
            zero_wrench);
      }
    }
    t_elapsed += dt;
  }

  return init_vals;
}

/** @fn Iteratively solve for the robot kinematics with contacts
 *
 *
 *
 *
 *
 */
gtsam::Values initialize_solution_kinematics(
    const gtdynamics::Robot& robot, const std::string& link_name,
    const gtsam::Pose3& wTl_i, const gtsam::Pose3& wTl_f, const double& T_i,
    const double& T_f, const double& dt,
    const boost::optional<std::vector<gtdynamics::ContactPoint>>&
        contact_points = boost::none) {
  gtsam::Values init_vals;

  throw std::runtime_error("Not yet implemented.");

  return init_vals;
}

#endif  // EXAMPLES_EXAMPLE_FULL_KINODYNAMIC_BALANCING_INITIALIZE_SOLUTIONS_HPP_
