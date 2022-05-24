/* ----------------------------------------------------------------------------
 * GTDynamics Copyright 2020, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file  ConstraintManifold.cpp
 * @brief Constraint manifold implementations.
 * @author: Yetong Zhang
 */

#include <gtdynamics/optimizer/ConstraintManifold.h>
#include <gtdynamics/optimizer/AugmentedLagrangianOptimizer.h>
#include <gtdynamics/optimizer/PenaltyMethodOptimizer.h>

#include <gtsam/linear/GaussianBayesNet.h>
#include <gtsam/linear/VectorValues.h>
#include <gtsam/nonlinear/LinearContainerFactor.h>

namespace gtsam {

/* ************************************************************************* */
void ConstraintManifold::compute_values(const gtsam::Values& values,
                                        const bool retract_init) {
  values_ = gtsam::Values();
  base_dim_ = 0;
  for (const gtsam::Key& key : cc_->keys) {
    const auto& value = values.at(key);
    base_dim_ += value.dim();
    values_.insert(key, value);
  }
  constraint_dim_ = 0;
  for (const auto& constraint : cc_->constraints) {
    constraint_dim_ += constraint->dim();
  }
  if (base_dim_ > constraint_dim_) {
    dim_ = base_dim_ - constraint_dim_;
  } else {
    dim_ = 0;
  }
  if (retract_init) {
    values_ = retract_constraints(values_);
  }
}

/* ************************************************************************* */
Values ConstraintManifold::retract_constraints(const Values& values) const {
  if (params_->retract_type == Params::RetractType::UOPT) {
    return retract_uopt(values);
  } else if (params_->retract_type == Params::RetractType::PROJ) {
    return retract_proj(values);
  } else if (params_->retract_type == Params::RetractType::PARTIAL_PROJ) {
    return retract_p_proj(values);
  }
  std::cerr << "Unrecognized retraction type.\n";
  return Values();
}

/* ************************************************************************* */
void ConstraintManifold::compute_basis() {
  if (params_->basis_type == Params::BasisType::KERNEL) {
    compute_basis_kernel();
  } else if (params_->basis_type == Params::BasisType::SPECIFY_VARIABLES) {
    compute_basis_specify_variables();
  }
}

/* ************************************************************************* */
const gtsam::Value& ConstraintManifold::recover(const gtsam::Key key,
                                                ChartJacobian H) const {
  if (H) {
    // choose the corresponding rows in basis
    *H = basis_.middleRows(var_location_.at(key), var_dim_.at(key));
  }
  return values_.at(key);
}

/* ************************************************************************* */
ConstraintManifold ConstraintManifold::retract(const gtsam::Vector& xi,
                                               ChartJacobian H1,
                                               ChartJacobian H2) const {
  // Compute delta for each variable and perform update.
  gtsam::Vector x_xi = basis_ * xi;
  gtsam::VectorValues delta;
  for (const Key& key : cc_->keys) {
    delta.insert(key, x_xi.middleRows(var_location_.at(key), var_dim_.at(key)));
  }
  gtsam::Values new_values = values_.retract(delta);

  // Temporarily set jacobian as 0 since they are not used for optimization.
  if (H1) H1->setZero();
  if (H2) H2->setZero();

  // Satisfy the constraints in the connected component.
  return createWithNewValues(new_values, true);
}

/* ************************************************************************* */
gtsam::Vector ConstraintManifold::localCoordinates(const ConstraintManifold& g,
                                                   ChartJacobian H1,
                                                   ChartJacobian H2) const {
  Eigen::MatrixXd basis_pinv =
      basis_.completeOrthogonalDecomposition().pseudoInverse();
  gtsam::VectorValues delta = values_.localCoordinates(g.values_);
  gtsam::Vector xi_base = Vector::Zero(base_dim_);
  for (const auto& it : delta) {
    const Key& key = it.first;
    xi_base.middleRows(var_location_.at(key), var_dim_.at(key)) = it.second;
  }
  gtsam::Vector xi = basis_pinv * xi_base;

  // Temporarily set jacobian as 0 since they are not used for optimization.
  if (H1) H1->setZero();
  if (H2) H2->setZero();

  return xi;
}

/* ************************************************************************* */
void ConstraintManifold::print(const std::string& s) const {
  std::cout << (s.empty() ? s : s + " ") << "ConstraintManifold" << std::endl;
  values_.print();
}

/* ************************************************************************* */
bool ConstraintManifold::equals(const ConstraintManifold& other,
                                double tol) const {
  return values_.equals(other.values_, tol);
}


/* ************************************************************************* */
gtsam::Values ConstraintManifold::retract_uopt(
    const gtsam::Values& values) const {
  // return values;
  gtsam::Values init_values_cc;
  for (const Key& key : cc_->keys) {
    init_values_cc.insert(key, values.at(key));
  }

  // TODO: avoid copy-paste of graph, avoid constructing optimzier everytime
  gtsam::LevenbergMarquardtOptimizer optimizer(cc_->merit_graph, init_values_cc,
                                               params_->lm_params);
  return optimizer.optimize();
}

/* ************************************************************************* */
gtsam::Values ConstraintManifold::retract_proj(
    const gtsam::Values& values) const {
  NonlinearFactorGraph prior_graph;
  Values init_values_cc;
  for (const Key& key : cc_->keys) {
    init_values_cc.insert(key, values.at(key));
    size_t dim = values.at(key).dim();
    auto linear_factor = boost::make_shared<JacobianFactor>(
        key, Matrix::Identity(dim, dim), Vector::Zero(dim),
        noiseModel::Unit::Create(dim));
    Values linearization_point;
    linearization_point.insert(key, values.at(key));
    prior_graph.emplace_shared<LinearContainerFactor>(linear_factor,
                                                      linearization_point);
  }
  // gtdynamics::AugmentedLagrangianParameters al_params(params_->lm_params);
  // gtdynamics::AugmentedLagrangianOptimizer optimizer(al_params);
  gtdynamics::PenaltyMethodParameters al_params(params_->lm_params);
  gtdynamics::PenaltyMethodOptimizer optimizer(al_params);

  return optimizer.optimize(prior_graph, cc_->constraints, init_values_cc);
}

/* ************************************************************************* */
gtsam::Values ConstraintManifold::retract_p_proj(
    const gtsam::Values& values) const {
  Values init_values_cc;
  for (const Key& key : cc_->keys) {
    init_values_cc.insert(key, values.at(key));
  }
  NonlinearFactorGraph graph = cc_->merit_graph;
  for (const Key& key : basis_keys_) {
    size_t dim = values.at(key).dim();
    auto linear_factor = boost::make_shared<JacobianFactor>(
        key, Matrix::Identity(dim, dim) * 1e6, Vector::Zero(dim),
        noiseModel::Unit::Create(dim));
    Values linearization_point;
    linearization_point.insert(key, values.at(key));
    graph.emplace_shared<LinearContainerFactor>(linear_factor,
                                                linearization_point);
  }
  gtsam::LevenbergMarquardtOptimizer optimizer(graph, init_values_cc,
                                               params_->lm_params);
  return optimizer.optimize();
  // auto result = 
  // if (graph.error(result) > 1e-4) {
  //   std::cout << "cannot retract exactly tproj\n";
  // }
  // return result;
}

/* ************************************************************************* */
void ConstraintManifold::compute_basis_kernel() {
  auto linear_graph = cc_->merit_graph.linearize(values_);
  JacobianFactor combined(*linear_graph);
  auto augmented = combined.augmentedJacobian();
  Matrix A = augmented.leftCols(augmented.cols() - 1);  // m x n
  // Vector b = augmented.col(augmented.cols() - 1);
  Eigen::FullPivLU<Eigen::MatrixXd> lu(A);
  basis_ = lu.kernel();  // n x n-m
  size_t position = 0;
  for (const Key& key : combined.keys()) {
    size_t var_dim = values_.at(key).dim();
    var_dim_[key] = var_dim;
    var_location_[key] = position;
    position += var_dim;
  }
}

/* ************************************************************************* */
void ConstraintManifold::compute_basis_specify_variables() {
  size_t basis_dim = 0;
  for (const Key& key : basis_keys_) {
    basis_dim += values_.at(key).dim();
  }
  if (basis_dim != dim()) {
    std::cerr << "specified basis have wrong dimensions\n";
  }

  auto linear_graph = cc_->merit_graph.linearize(values_);
  auto full_ordering =
      Ordering::ColamdConstrainedLast(*linear_graph, basis_keys_);
  Ordering ordering = full_ordering;
  for (size_t i = 0; i < basis_keys_.size(); i++) {
    ordering.pop_back();
  }
  auto elim_result = linear_graph->eliminatePartialSequential(ordering);
  auto bayes_net = elim_result.first;

  size_t position = 0;
  for (const Key& key : full_ordering) {
    size_t var_dim = values_.at(key).dim();
    var_dim_[key] = var_dim;
    var_location_[key] = position;
    position += var_dim;
  }

  basis_ = Matrix::Zero(position, dim());

  size_t col_idx = 0;
  for (const Key& basis_key : basis_keys_) {
    auto dim = values_.at(basis_key).dim();
    for (size_t dim_idx = 0; dim_idx < dim; dim_idx++) {
      // construct basis values
      VectorValues sol_missing;
      for (const Key& key : basis_keys_) {
        Vector vec = Vector::Zero(values_.at(key).dim());
        if (key == basis_key) {
          vec(dim_idx) = 1;
        }
        sol_missing.insert(key, vec);
      }

      VectorValues result = bayes_net->optimize(sol_missing);

      for (const Key& key : full_ordering) {
        basis_.block(var_location_.at(key), col_idx, var_dim_.at(key), 1) =
            result.at(key);
      }
      col_idx += 1;
    }
  }
}


}  // namespace gtsam
