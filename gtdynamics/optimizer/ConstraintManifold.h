/* ----------------------------------------------------------------------------
 * GTDynamics Copyright 2020, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file  ConstraintManifold.h
 * @brief Manifold representing variables satisfying constraints.
 * @author: Yetong Zhang
 */

#pragma once

#include <gtdynamics/optimizer/ConnectedComponent.h>

namespace gtsam {

/** Manifold representing constraint-connected component. Any element on the
 * manifold is the values of variables in CCC satisfying the constraints, e.g.,
 * {X : h(X)=0}. */
class ConstraintManifold {
 public:
  /** Parameters for constraint manifold. */
  struct Params {
    // Method to compute tangent space basis
    enum BasisType { KERNEL = 0, ELIMINATION = 1, SPECIFY_VARIABLES = 2 };
    // Method to perform retraction
    enum RetractType { UOPT = 0, PROJ = 1, PARTIAL_PROJ = 2 };

    LevenbergMarquardtParams lm_params;
    RetractType retract_type;
    BasisType basis_type;

    using shared_ptr = boost::shared_ptr<Params>;
    Params()
        : lm_params(),
          retract_type(RetractType::UOPT),
          basis_type(BasisType::KERNEL) {}
  };

 protected:
  Params::shared_ptr params_;
  ConnectedComponent::shared_ptr cc_;
  size_t base_dim_;
  size_t constraint_dim_;
  size_t dim_;
  gtsam::Values values_;
  gtsam::Matrix basis_;
  std::map<Key, size_t> var_location_;
  std::map<Key, size_t> var_dim_;
  KeyVector basis_keys_;

 public:
  enum { dimension = Eigen::Dynamic };

  typedef OptionalJacobian<Eigen::Dynamic, Eigen::Dynamic> ChartJacobian;

  /** Constructor from Connected Component.
   * @param cc      constraint-connected component
   * @param values  values of variable in the connected component
   * @param retract_init  whether to perform retract in initialization
   * @param construct_basis compute basis on initialization
   */
  ConstraintManifold(
      const ConnectedComponent::shared_ptr& cc, const gtsam::Values& values,
      const Params::shared_ptr& params = boost::make_shared<Params>(),
      const bool retract_init = true, const bool construct_basis = true,
      const boost::optional<const KeyVector&> basis_keys = boost::none)
      : params_(params), cc_(cc) {
    if (basis_keys) {
      basis_keys_ = *basis_keys;
    }
    compute_values(values, retract_init);
    if (construct_basis && dim() > 0) {
      compute_basis();
    }
  }

  /** Construct new ConstraintManifold with new values. */
  ConstraintManifold createWithNewValues(const gtsam::Values& values,
                                         bool retract_init = true) const {
    return ConstraintManifold(cc_, values, params_, true, true, basis_keys_);
  }

  /** Dimension of the constraint manifold. */
  inline size_t dim() const { return dim_; }

  /** Base values of the CCC. */
  inline const Values& values() const { return values_; }

  /** get base value with optional Jacobian. */
  const gtsam::Value& recover(const gtsam::Key key,
                              ChartJacobian H1 = boost::none) const;

  /** get base value by type with optional Jacobian. */
  template <typename ValueType>
  inline ValueType recover(const gtsam::Key key,
                           ChartJacobian H1 = boost::none) const {
    return recover(key, H1).cast<ValueType>();
  }

  /// Retraction of the constraint manifold, e.g., retraction required for gtsam
  /// manifold type.
  ConstraintManifold retract(const gtsam::Vector& xi,
                             ChartJacobian H1 = boost::none,
                             ChartJacobian H2 = boost::none) const;

  /// LocalCoordinates of the constraint manifold, e.g., localCoordinates
  /// required for gtsam manifold type.
  gtsam::Vector localCoordinates(const ConstraintManifold& g,
                                 ChartJacobian H1 = boost::none,
                                 ChartJacobian H2 = boost::none) const;

  /** Given values of variables in CCC that may violate the constraints, compute
   * the values that satisfy the constraints. */
  Values retract_constraints(const Values& values) const;

  /// print
  void print(const std::string& s = "") const;

  /// equals
  bool equals(const ConstraintManifold& other, double tol = 1e-8) const;

  /** Return the basis of the tangent space. */
  inline const gtsam::Matrix& basis() const { return basis_; }

  /** Compute the tangent space basis for the constraint manifold. */
  void compute_basis();

 protected:
  /** Set the values_ of variables in CCC and compute dimension of the
   * constraint manifold, given the initial values of the variables in CCC. */
  void compute_values(const gtsam::Values& values, const bool retract_init);

  /** Perform retraction by minimizing the constraint violation, e.g.,
   * ||h(x)||^2. */
  gtsam::Values retract_uopt(const gtsam::Values& values) const;

  /** Perform retraction by performing metric projection, e.g., minimizing
   * ||dist(x,x0)||^2  s.t. h(x)=0. */
  gtsam::Values retract_proj(const gtsam::Values& values) const;

  /** Perform retraction by minimizing the constraint violation while fixing the
   * specified variables, e.g., min ||h(x)||^2.  s.t. x_s=x0_s. */
  gtsam::Values retract_p_proj(const gtsam::Values& values) const;

  /** Compute the tangent space basis as the kernel of Dh(X). */
  void compute_basis_kernel();

  /** Compute the tangent space basis as the specified variables, the update on
   * the rest of the variables will be computed through variable elimination. */
  void compute_basis_specify_variables();
};

// Specialize ConstraintManifold traits to use a Retract/Local
template <>
struct traits<ConstraintManifold>
    : gtsam::internal::Manifold<ConstraintManifold> {};

}  // namespace gtsam
