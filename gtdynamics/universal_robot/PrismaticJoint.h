/* ----------------------------------------------------------------------------
 * GTDynamics Copyright 2020, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file  PrismaticJoint.h
 * @brief Representation of PrismaticJoint that inherits from ScrewJointBase
 * @author Frank Dellaert
 * @author Mandy Xie
 * @author Alejandro Escontrela
 * @author Yetong Zhang
 * @author Stephanie McCormick
 * @author Gerry Chen
 */

#ifndef GTDYNAMICS_UNIVERSAL_ROBOT_PRISMATICJOINT_H_
#define GTDYNAMICS_UNIVERSAL_ROBOT_PRISMATICJOINT_H_

#include "gtdynamics/universal_robot/ScrewJointBase.h"

namespace gtdynamics {

/**
 * @class PrismaticJoint is an implementation of the ScrewJointBase class
 *  which represents a prismatic joint and contains all necessary factor
 *  construction methods.
 */
class PrismaticJoint : public ScrewJointBase {
 protected:
  /// Returns the screw axis in the joint frame given the joint axis
  gtsam::Vector6 getScrewAxis(gtsam::Vector3 axis) {
    gtsam::Vector6 screw_axis;
    screw_axis << 0, 0, 0, axis;
    return screw_axis;
  }

 public:
  /**
   * @brief Create PrismaticJoint from a sdf::Joint instance.
   *
   * @param[in] sdf_joint                  sdf::Joint object.
   * @param[in] joint_effort_type          Joint effort type.
   * @param[in] springCoefficient          Spring coefficient.
   * @param[in] jointLimitThreshold        Joint angle limit threshold.
   * @param[in] velocityLimitThreshold     Joint velocity limit threshold.
   * @param[in] accelerationLimit          Joint acceleration limit.
   * @param[in] accelerationLimitThreshold Joint Acceleration limit threshold.
   * @param[in] torqueLimitThreshold       Joint torque limit threshold.
   * @param[in] parent_link                Shared pointer to the parent Link.
   * @param[in] child_link                 Shared pointer to the child Link.
   */
  PrismaticJoint(const sdf::Joint &sdf_joint,
                JointEffortType joint_effort_type,
                double springCoefficient, double jointLimitThreshold,
                double velocityLimitThreshold, double accelerationLimit,
                double accelerationLimitThreshold, double torqueLimitThreshold,
                LinkSharedPtr parent_link, LinkSharedPtr child_link)
      : ScrewJointBase(sdf_joint,
                       getScrewAxis(getSdfAxis(sdf_joint)),
                       joint_effort_type, springCoefficient,
                       jointLimitThreshold, velocityLimitThreshold,
                       accelerationLimit, accelerationLimitThreshold,
                       torqueLimitThreshold, parent_link, child_link) {}

  /** 
   * @brief Create PrismaticJoint using sdf::Joint instance and joint parameters. 
   * 
   * @param[in] sdf_joint                  sdf::Joint object.
   * @param[in] parameters                 Joint::Params struct
   * @param[in] parent_link                Shared pointer to the parent Link.
   * @param[in] child_link                 Shared pointer to the child Link.
  */
  PrismaticJoint(const sdf::Joint &sdf_joint,
                 const gtdynamics::JointParams &parameters,
                 LinkSharedPtr parent_link, LinkSharedPtr child_link)
      : PrismaticJoint(
          sdf_joint,
          parameters.jointEffortType, parameters.springCoefficient,
          parameters.jointLimitThreshold, parameters.velocityLimitThreshold,
          parameters.accelerationLimit, parameters.accelerationLimitThreshold,
          parameters.torqueLimitThreshold, parent_link, child_link) {}

  /** 
   * @brief Create PrismaticJoint using JointParams and screw axes.
   * 
   * @param[in] params        Joint::Params struct
   * @param[in] axis          joint axis expressed in joint frame
  */
  PrismaticJoint(const Params &params, gtsam::Vector3 axis)
      : ScrewJointBase(params,
                       axis,
                       getScrewAxis(axis)) {}

  /// Return jointType for use in reconstructing robot from Parameters.
  JointType jointType() const { return JointType::Prismatic; }
};

}  // namespace gtdynamics

#endif  // GTDYNAMICS_UNIVERSAL_ROBOT_PRISMATICJOINT_H_