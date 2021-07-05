/* ----------------------------------------------------------------------------
 * GTDynamics Copyright 2020, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file  PreintegratedContactFactors.h
 * @brief Preintegrated contact factors as defined in Hartley18icra.
 * @author Varun Agrawal
 */

#pragma once

#include <gtsam/base/Matrix.h>
#include <gtsam/base/OptionalJacobian.h>
#include <gtsam/base/Vector.h>
#include <gtsam/geometry/Pose3.h>
#include <gtsam/navigation/ImuFactor.h>
#include <gtsam/nonlinear/NonlinearFactor.h>

#include <memory>
#include <string>

#include "gtdynamics/universal_robot/JointTyped.h"
#include "gtdynamics/universal_robot/Link.h"

namespace gtdynamics {

/**
 * Class to perform preintegration of contact measurements for point foot model.
 */
class PreintegratedPointContactMeasurements {
  gtsam::Matrix3 preintMeasCov_;

  ///< The covariance of the discrete contact noise, aka Σvd in the paper
  gtsam::Matrix3 vdCov_;

 public:
  PreintegratedPointContactMeasurements() {}

  /**
   * @brief Construct a new Preintegrated Point Contact Measurements object.
   *
   * @param discreteVelocityCovariance The covariance matrix for the discrete
   * velocity of the contact frame.
   */
  PreintegratedPointContactMeasurements(
      const gtsam::Matrix3 &discreteVelocityCovariance) {
    preintMeasCov_.setZero();
    vdCov_ = discreteVelocityCovariance;
  }

  /// Virtual destructor for serialization
  ~PreintegratedPointContactMeasurements() {}

  /**
   * @brief Propagate measurement for the first step, i.e. when k = i.
   *
   * @param base_k The pose of the current base frame.
   * @param contact_k The pose of the current contact frame. Taken from the
   * forward kinematics.
   * @param dt The time between the previous and current step.
   */
  void initialize(const gtsam::Pose3 &base_k, const gtsam::Pose3 &contact_k,
                  double dt) {
    gtsam::Matrix3 B =
        base_k.rotation().transpose() * contact_k.rotation().matrix() * dt;
    preintMeasCov_ = preintMeasCov_ + (B * vdCov_ * B.transpose());
  }

  /**
   * @brief Add a single slip/noise measurement to the preintegration.
   *
   * @param contact_k The pose of the current contact frame obtained via forward
   * kinematics.
   * @param deltaRik The rotation delta obtained from the IMU preintegration.
   * @param dt Time interval between this and the last IMU measurement.
   */
  void integrateMeasurement(const gtsam::Pose3 &contact_k,
                            const gtsam::Rot3 &deltaRik, const double dt) {
    gtsam::Matrix3 B = (deltaRik * contact_k.rotation()).matrix() * dt;
    preintMeasCov_ = preintMeasCov_ + (B * vdCov_ * B.transpose());
  }

  gtsam::Matrix3 preintMeasCov() const { return preintMeasCov_; }
};

/**
 * The Preintegrated Contact Factor for point foot measurements as defined in
 * Hartley18icra.
 */
class PreintegratedPointContactFactor
    : public gtsam::NoiseModelFactor4<gtsam::Pose3, gtsam::Pose3, gtsam::Pose3,
                                      gtsam::Pose3> {
 private:
  using This = PreintegratedPointContactFactor;
  using Base = gtsam::NoiseModelFactor4<gtsam::Pose3, gtsam::Pose3,
                                        gtsam::Pose3, gtsam::Pose3>;

 public:
  /**
   * Constructor
   *
   * @param wTbi_key Key for base link pose in world frame at initial time of
   contact.
   * @param wTci_key Key for contact pose in world frame at initial time of
   contact.
   * @param wTbi_key Key for base link pose in world frame at final time of
   contact.
   * @param wTci_key Key for contact pose in world frame at final time of
   contact.
   * @param pcm Preintegrated point contact measurements which captures the
   measurement covariance for the point foot model.
   *
   */
  PreintegratedPointContactFactor(
      gtsam::Key wTbi_key, gtsam::Key wTci_key, gtsam::Key wTbj_key,
      gtsam::Key wTcj_key, const PreintegratedPointContactMeasurements &pcm)
      : Base(gtsam::noiseModel::Gaussian::Covariance(pcm.preintMeasCov()),
             wTbi_key, wTci_key, wTbj_key, wTcj_key) {}

  virtual ~PreintegratedPointContactFactor() {}

  /**
   * @brief
   *
   * @param wTb_i current body link CoM pose
   * @param wTc_i current contact pose
   * @param wTb_j next body link CoM pose
   * @param wTc_i next contact pose
   */
  gtsam::Vector evaluateError(
      const gtsam::Pose3 &wTb_i, const gtsam::Pose3 &wTc_i,
      const gtsam::Pose3 &wTb_j, const gtsam::Pose3 &wTc_j,
      boost::optional<gtsam::Matrix &> H_wTb_i = boost::none,
      boost::optional<gtsam::Matrix &> H_wTc_i = boost::none,
      boost::optional<gtsam::Matrix &> H_wTb_j = boost::none,
      boost::optional<gtsam::Matrix &> H_wTc_j = boost::none) const override {
    // For Rot3, translation == inverse due to orthogonality
    gtsam::Vector3 error = wTb_i.rotation().inverse() *
                           (wTc_j.translation() - wTc_i.translation());

    // Please refer to the supplementary material for the Jacobian calculations.
    // https://arxiv.org/src/1712.05873v2/anc/icra-supplementary-material.pdf
    if (H_wTb_i) {
      gtsam::Matrix36 H;
      H << gtsam::SO3::Hat(error), gtsam::Z_3x3;
      *H_wTb_i = H;
    }
    if (H_wTc_i) {
      gtsam::Matrix36 H;
      H << gtsam::Z_3x3, -gtsam::I_3x3;
      *H_wTc_i = H;
    }
    if (H_wTb_j) {
      *H_wTb_j = gtsam::Z_6x6;
    }
    if (H_wTc_j) {
      gtsam::Matrix36 H;
      H << gtsam::Z_3x3, wTb_i.rotation().inverse() * wTb_j.rotation();
      *H_wTc_j = H;
    }
    return error;
  }

  //// @return a deep copy of this factor
  gtsam::NonlinearFactor::shared_ptr clone() const override {
    return boost::static_pointer_cast<gtsam::NonlinearFactor>(
        gtsam::NonlinearFactor::shared_ptr(new This(*this)));
  }

  /// print contents
  void print(const std::string &s = "",
             const gtsam::KeyFormatter &keyFormatter =
                 gtsam::DefaultKeyFormatter) const override {
    std::cout << (s.empty() ? s : s + " ")
              << "Preintegrated Point Contact Factor" << std::endl;
    Base::print("", keyFormatter);
  }

 private:
  /// Serialization function
  friend class boost::serialization::access;
  template <class ARCHIVE>
  void serialize(ARCHIVE &ar, const unsigned int version) {  // NOLINT
    ar &boost::serialization::make_nvp(
        "NoiseModelFactor4", boost::serialization::base_object<Base>(*this));
  }
};

/**
 * Class to perform preintegration of contact measurements for rigid foot model.
 */
class PreintegratedRigidContactMeasurements {
  gtsam::Matrix6 preintMeasCov_;
  gtsam::Matrix3 wCov_, vCov_;
  double deltaT_;

 public:
  PreintegratedRigidContactMeasurements() {}

  /**
   * @brief Construct a new Preintegrated Point Contact Measurements object.
   *
   * @param discreteVelocityCovariance The covariance matrix for the discrete
   * velocity of the contact frame.
   */
  PreintegratedRigidContactMeasurements(
      const gtsam::Matrix3 &angularVelocityCovariance,
      const gtsam::Matrix3 &linearVelocityCovariance) {
    preintMeasCov_.setZero();
    wCov_ = angularVelocityCovariance;
    vCov_ = linearVelocityCovariance;
  }

  /// Virtual destructor for serialization
  ~PreintegratedRigidContactMeasurements() {}

  /**
   * @brief Integrate a new measurement.
   *
   * @param dt Time interval between this and the last IMU measurement.
   */
  void integrateMeasurement(double dt) {
    preintMeasCov_ << wCov_, gtsam::Z_3x3, gtsam::Z_3x3, vCov_;
    deltaT_ += dt;
    preintMeasCov_ *= deltaT_;
  }

  gtsam::Matrix6 preintMeasCov() const { return preintMeasCov_; }
};

/**
 * The Preintegrated Contact Factor for rigid foot measurements as defined in
 * Hartley18icra.
 */
class PreintegratedRigidContactFactor
    : public gtsam::BetweenFactor<gtsam::Pose3> {
 private:
  using This = PreintegratedRigidContactFactor;
  using Base = gtsam::BetweenFactor<gtsam::Pose3>;

 public:
  /**
   * Constructor
   *
   * @param wTci_key Key for contact pose in world frame at initial time of
   contact.
   * @param wTci_key Key for contact pose in world frame at final time of
   contact.
   * @param pcm Preintegrated rigid contact measurements object which captures
   the measurement covariance for the rigid foot model.
   *
   */
  PreintegratedRigidContactFactor(
      gtsam::Key wTci_key, gtsam::Key wTcj_key,
      const PreintegratedRigidContactMeasurements &pcm)
      : Base(gtsam::noiseModel::Gaussian::Covariance(pcm.preintMeasCov()),
             wTbi_key, wTci_key, wTbj_key, wTcj_key) {}

  virtual ~PreintegratedRigidContactFactor() {}

  //// @return a deep copy of this factor
  gtsam::NonlinearFactor::shared_ptr clone() const override {
    return boost::static_pointer_cast<gtsam::NonlinearFactor>(
        gtsam::NonlinearFactor::shared_ptr(new This(*this)));
  }

  //TODO(Varun) Verify jacobians as per supplementary material.

  /// print contents
  void print(const std::string &s = "",
             const gtsam::KeyFormatter &keyFormatter =
                 gtsam::DefaultKeyFormatter) const override {
    std::cout << (s.empty() ? s : s + " ")
              << "Preintegrated Rigid Contact Factor" << std::endl;
    Base::print("", keyFormatter);
  }

 private:
  /// Serialization function
  friend class boost::serialization::access;
  template <class ARCHIVE>
  void serialize(ARCHIVE &ar, const unsigned int version) {  // NOLINT
    ar &boost::serialization::make_nvp(
        "BetweenFactor", boost::serialization::base_object<Base>(*this));
  }
};

}  // namespace gtdynamics
