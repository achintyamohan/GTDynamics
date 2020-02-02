/* ----------------------------------------------------------------------------
 * GTDynamics Copyright 2020, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file UniversalRobot.h
 * @brief Robot structure.
 * @Author: Frank Dellaert, Mandy Xie, and Alejandro Escontrela
 */

#pragma once

#include <RobotJoint.h>
#include <RobotLink.h>
#include <RobotTypes.h>

#include <sdf/parser_urdf.hh>

#include <map>

#include <string>
#include <utility>
#include <vector>

#include <boost/optional.hpp>

namespace robot {

/** Construct all RobotLink and RobotJoint objects from an input
 * urdf::ModelInterfaceSharedPtr. Keyword arguments: urdf_ptr         -- a
 * shared pointer to a urdf::ModelInterface object. joint_params     -- a vector
 * contanining optional params for joints.
 *
 */

typedef std::map<std::string, robot::RobotLinkSharedPtr> LinkMap;
typedef std::map<std::string, robot::RobotJointSharedPtr> JointMap;
typedef std::pair<LinkMap, JointMap> LinkJointPair;

/** Construct all RobotLink and RobotJoint objects from an input
 sdf::ElementPtr.
 * Keyword arguments:
 *    sdf_ptr          -- a shared pointer to a sdf::ElementPtr containing the
        robot model.
 *    joint_params     -- a vector contanining optional params for joints.
 *
 */
LinkJointPair extract_structure_from_sdf(
    const sdf::Model sdf,
    const boost::optional<std::vector<robot::RobotJointParams>> joint_params =
        boost::none);

/** Construct all RobotLink and RobotJoint objects from an input urdf or sdf
 * file. Keyword arguments: file_path    -- absolute path to the urdf or sdf
 * file containing the robot description. joint_params -- a vector containing
 * optional params for joints.
 */
LinkJointPair extract_structure_from_file(
    const std::string file_path, const std::string model_name,
    const boost::optional<std::vector<robot::RobotJointParams>> joint_params =
        boost::none);

/**
 * UniversalRobot is used to create a representation of a robot's
 * inertial/dynamic properties from a URDF/SDF file. The resulting object
 * provides getters for the robot's various joints and links, which can then
 * be fed into an optimization pipeline.
 */
class UniversalRobot {
 private:

  // For quicker/easier access to links and joints.
  LinkMap name_to_link_;
  JointMap name_to_joint_;

 public:
  /**
   * Construct a robot structure using a URDF model interface.
   * Keyword Arguments:
   *  robot_links_and_joints    -- LinkJointPair containing links and joints.
   *
   */
  explicit UniversalRobot(LinkJointPair links_and_joints);

  /** Construct a robot structure directly from a urdf or sdf file.
   *
   * Keyword Arguments:
   *  file_path -- path to the file.
   */
  explicit UniversalRobot(const std::string file_path,
                          std::string model_name = "");

  /// Return this robot's links.
  std::vector<RobotLinkSharedPtr> links() const;

  /// Return this robot's joints.
  std::vector<RobotJointSharedPtr> joints() const;

  /// remove specified link from the robot
  void removeLink(RobotLinkSharedPtr link);

  /// remove specified joint from the robot
  void removeJoint(RobotJointSharedPtr joint);

  /// Return the link corresponding to the input string.
  RobotLinkSharedPtr getLinkByName(std::string name) const;

  /// Return the joint corresponding to the input string.
  RobotJointSharedPtr getJointByName(std::string name) const;

  /// Return number of *moving* links.
  int numLinks() const;

  /// Return number of joints.
  int numJoints() const;

  // print links and joints of the robot, for debug purposes
  void printRobot() const;
};
}  // namespace robot
