/* ----------------------------------------------------------------------------
 * GTDynamics Copyright 2020, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file  Joint.h
 * @brief Abstract representation of a robot joint.
 * @Author: Frank Dellaert, Mandy Xie, Alejandro Escontrela, Yetong Zhang
 */

#ifndef GTDYNAMICS_UNIVERSAL_ROBOT_JOINT_H_
#define GTDYNAMICS_UNIVERSAL_ROBOT_JOINT_H_

#include <gtsam/linear/GaussianFactorGraph.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "gtdynamics/dynamics/OptimizerSetting.h"
#include "gtdynamics/universal_robot/Link.h"
#include "gtdynamics/universal_robot/RobotTypes.h"

namespace gtdynamics {

// TODO(aescontrela): Make toString method to display joint info.

/* Shorthand for q_j_t, for j-th joint angle at time t. */
inline gtsam::LabeledSymbol JointAngleKey(int j, int t) {
  return gtsam::LabeledSymbol('q', j, t);
}

/* Shorthand for v_j_t, for j-th joint velocity at time t. */
inline gtsam::LabeledSymbol JointVelKey(int j, int t) {
  return gtsam::LabeledSymbol('v', j, t);
}

/* Shorthand for a_j_t, for j-th joint acceleration at time t. */
inline gtsam::LabeledSymbol JointAccelKey(int j, int t) {
  return gtsam::LabeledSymbol('a', j, t);
}

/* Shorthand for T_j_t, for torque on the j-th joint at time t. */
inline gtsam::LabeledSymbol TorqueKey(int j, int t) {
  return gtsam::LabeledSymbol('T', j, t);
}

/**
 * Joint is the base class for a joint connecting two Link objects.
 */
class Joint : public std::enable_shared_from_this<Joint> {
 public:
  /** joint effort types
   * Actuated: motor powered
   * Unactuated: not powered, free to move, exert zero torque
   * Impedance: with spring resistance
   */
  enum JointEffortType { Actuated, Unactuated, Impedance };

  /**
   * JointParams contains all parameters to construct a joint
   */
  struct Params {
    std::string name;                    // name of the joint
    char joint_type;                     // type of joint
    Joint::JointEffortType effort_type;  // joint effor type
    LinkSharedPtr parent_link;           // shared pointer to parent link
    LinkSharedPtr child_link;            // shared pointer to child link
    gtsam::Vector3 axis;                 // joint axis expressed in joint frame
    gtsam::Pose3 wTj;                    // joint pose expressed in world frame
    double joint_lower_limit;
    double joint_upper_limit;
    double joint_limit_threshold;
  };

 protected:
  // This joint's name, as described in the URDF file.
  std::string name_;

  // ID reference to gtsam::LabeledSymbol.
  int id_ = -1;

  LinkSharedPtr parent_link_;
  LinkSharedPtr child_link_;

  // Joint frame defined in world frame.
  gtsam::Pose3 wTj_;
  // Rest transform to parent link CoM frame from joint frame.
  gtsam::Pose3 jTpcom_;
  // Rest transform to child link CoM frame from joint frame.
  gtsam::Pose3 jTccom_;
  // Rest transform to parent link com frame from child link com frame at rest.
  gtsam::Pose3 pMccom_;

  /// Transform from the world frame to the joint frame.
  const gtsam::Pose3 &wTj() const { return wTj_; }

  /// Transform from the joint frame to the parent's center of mass.
  const gtsam::Pose3 &jTpcom() const { return jTpcom_; }

  /// Transform from the joint frame to the child's center of mass.
  const gtsam::Pose3 &jTccom() const { return jTccom_; }

  /// Abstract method. Return transform of child link com frame w.r.t parent
  /// link com frame
  gtsam::Pose3 pMcCom(boost::optional<double> q = boost::none);

  /// Abstract method. Return transform of parent link com frame w.r.t child
  /// link com frame
  gtsam::Pose3 cMpCom(boost::optional<double> q = boost::none);

  /// Check if the link is a child link, throw an error if link is not
  /// connected to this joint.
  bool isChildLink(const LinkSharedPtr link) const {
    LinkSharedPtr link_ptr = link;
    if (link_ptr != child_link_ && link_ptr != parent_link_)
      throw std::runtime_error("link " + link_ptr->name() +
                               " is not connected to this joint " + name_);
    return link_ptr == child_link_;
  }

 public:
  Joint() {}

  /**
   * @brief Constructor to create Joint from a sdf::Joint instance.
   *
   * @param[in] sdf_joint    sdf::Joint object to derive joint attributes from.
   * @param[in] parent_link  Shared pointer to the parent Link.
   * @param[in] child_link   Shared pointer to the child Link.
   */
  Joint(const sdf::Joint &sdf_joint, LinkSharedPtr parent_link,
        LinkSharedPtr child_link)
      : name_(sdf_joint.Name()),
        parent_link_(parent_link),
        child_link_(child_link) {
    if (sdf_joint.PoseFrame() == "" ||
        sdf_joint.PoseFrame() == child_link->name()) {
      if (sdf_joint.Pose() == ignition::math::Pose3d())
        wTj_ = child_link->wTl();
      else
        wTj_ = child_link->wTl() * parse_ignition_pose(sdf_joint.Pose());
    } else if (sdf_joint.PoseFrame() == parent_link->name()) {
      if (sdf_joint.Pose() == ignition::math::Pose3d())
        wTj_ = parent_link->wTl();
      else
        wTj_ = parent_link->wTl() * parse_ignition_pose(sdf_joint.Pose());
    } else if (sdf_joint.PoseFrame() == "world") {
      wTj_ = parse_ignition_pose(sdf_joint.Pose());
    } else {
      // TODO(gchen328): get pose frame from name. Need sdf::Model to do that
      // though.
      throw std::runtime_error("joint pose frames other than world, parent, or "
                               "child not yet supported");
    }

    jTpcom_ = wTj_.inverse() * parent_link_->wTcom();
    jTccom_ = wTj_.inverse() * child_link_->wTcom();
    pMccom_ = parent_link_->wTcom().inverse() * child_link_->wTcom();
  }

  /**
   * @brief Constructor to create Joint from gtdynamics::JointParams instance.
   *
   * @param[in] params  gtdynamics::JointParams object.
   */
  explicit Joint(const Params &params)
      : name_(params.name),
        parent_link_(params.parent_link),
        child_link_(params.child_link),
        wTj_(params.wTj) {
    jTpcom_ = wTj_.inverse() * parent_link_->wTcom();
    jTccom_ = wTj_.inverse() * child_link_->wTcom();
    pMccom_ = parent_link_->wTcom().inverse() * child_link_->wTcom();
  }

  /**
   * @brief Default destructor.
   */
  virtual ~Joint() = default;

  /// Return a shared ptr to this joint.
  JointSharedPtr getSharedPtr() { return shared_from_this(); }

  /// Set the joint's ID to track reference to gtsam::LabeledSymbol.
  void setID(unsigned char id) { id_ = id; }

  /// Get the joint's ID to track reference to gtsam::LabeledSymbol.
  int getID() const {
    if (id_ == -1)
      throw std::runtime_error(
          "Calling getID on a link whose ID has not been set");
    return id_;
  }

  /// Return joint name.
  std::string name() const { return name_; }

  /// Return the connected link other than the one provided.
  LinkSharedPtr otherLink(const LinkSharedPtr link) const {
    return isChildLink(link) ? parent_link_ : child_link_;
  }

  /// Return the links connected to this joint.
  std::vector<LinkSharedPtr> links() const {
    return std::vector<LinkSharedPtr>{parent_link_, child_link_};
  }

  /// Return a shared ptr to the parent link.
  LinkSharedPtr parentLink() { return parent_link_; }

  /// Return a shared ptr to the child link.
  LinkSharedPtr childLink() { return child_link_; }

  /**
   * \defgroup AbstractMethods Abstract methods for the joint class.
   * @{
   */

  /// Abstract method: Return joint type.
  virtual char jointType() const = 0;

  /// Abstract method. Return the transform from this link com to the other link
  /// com frame
  virtual gtsam::Pose3 transformFrom(
      const LinkSharedPtr link,
      boost::optional<double> q = boost::none) const = 0;

  /// Abstract method. Return the twist of the other link given this link's
  /// twist and joint angle.
  virtual gtsam::Vector6 transformTwistFrom(
      const LinkSharedPtr link, boost::optional<double> q = boost::none,
      boost::optional<double> q_dot = boost::none,
      boost::optional<gtsam::Vector6> this_twist = boost::none) const = 0;

  /// Abstract method. Return the transform from the other link com to this link
  /// com frame
  virtual gtsam::Pose3 transformTo(
      const LinkSharedPtr link,
      boost::optional<double> q = boost::none) const = 0;

  /// Abstract method. Return the twist of this link given the other link's
  /// twist and joint angle.
  virtual gtsam::Vector6 transformTwistTo(
      const LinkSharedPtr link, boost::optional<double> q = boost::none,
      boost::optional<double> q_dot = boost::none,
      boost::optional<gtsam::Vector6> other_twist = boost::none) const = 0;

  /// Abstract method. Return joint angle factors.
  virtual gtsam::NonlinearFactorGraph qFactors(
      const int &t, const OptimizerSetting &opt) const = 0;

  /// Abstract method. Return joint vel factors.
  virtual gtsam::NonlinearFactorGraph vFactors(
      const int &t, const OptimizerSetting &opt) const = 0;

  /// Abstract method. Return joint accel factors.
  virtual gtsam::NonlinearFactorGraph aFactors(
      const int &t, const OptimizerSetting &opt) const = 0;

  /// Abstract method. Return linearized form of joint accel factors.
  virtual gtsam::GaussianFactorGraph linearAFactors(
      const int &t, const std::map<std::string, gtsam::Pose3> &poses,
      const std::map<std::string, gtsam::Vector6> &twists,
      const std::map<std::string, double> &joint_angles,
      const std::map<std::string, double> &joint_vels,
      const OptimizerSetting &opt,
      const boost::optional<gtsam::Vector3> &planar_axis =
          boost::none) const = 0;

  /// Abstract method. Return joint dynamics factors.
  virtual gtsam::NonlinearFactorGraph dynamicsFactors(
      const int &t, const OptimizerSetting &opt,
      const boost::optional<gtsam::Vector3> &planar_axis) const = 0;

  /// Abstract method. Return linearized form of joint dynamics factors.
  virtual gtsam::GaussianFactorGraph linearDynamicsFactors(
      const int &t, const std::map<std::string, gtsam::Pose3> &poses,
      const std::map<std::string, gtsam::Vector6> &twists,
      const std::map<std::string, double> &joint_angles,
      const std::map<std::string, double> &joint_vels,
      const OptimizerSetting &opt,
      const boost::optional<gtsam::Vector3> &planar_axis =
          boost::none) const = 0;

  /// Abstract method. Return joint limit factors.
  virtual gtsam::NonlinearFactorGraph jointLimitFactors(
      const int &t, const OptimizerSetting &opt) = 0;

  /**@}*/
};

struct JointParams {
  std::string name;  // Name of this joint as described in the URDF file.

  Joint::JointEffortType jointEffortType = Joint::JointEffortType::Actuated;
  double springCoefficient = 0;      // spring coefficient for Impedance joint.
  double jointLimitThreshold = 0.0;  // joint angle limit threshold.
  double velocityLimitThreshold = 0.0;  // joint velocity limit threshold.
  double accelerationLimit = 10000;     // joint acceleration limit.
  double accelerationLimitThreshold =
      0.0;                            // joint acceleration limit threshold.
  double torqueLimitThreshold = 0.0;  // joint torque limit threshold.
};

}  // namespace gtdynamics

#endif  // GTDYNAMICS_UNIVERSAL_ROBOT_JOINT_H_
