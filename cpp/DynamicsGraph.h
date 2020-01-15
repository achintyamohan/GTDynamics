/**
 * @file  DynamicsGraphBuilder.h
 * @brief robot arm motion planner using nonlinear factor graph
 * @Author:
 */
#pragma once

#include <IntegrationFactor.h>
#include <JointLimitFactor.h>
#include <JsonSaver.h>
#include <OptimizerSetting.h>
#include <PoseFactor.h>
#include <ToolPoseFactor.h>
#include <TorqueFactor.h>
#include <TwistAccelFactor.h>
#include <TwistFactor.h>
#include <WrenchEquivalenceFactor.h>
#include <WrenchFactors.h>
#include <WrenchPlanarFactor.h>
#include <cmath>
#include <gtsam/linear/NoiseModel.h>
#include <iostream>
#include <utils.h>

#include <RobotJoint.h>
#include <RobotLink.h>
#include <UniversalRobot.h>

#include <boost/optional.hpp>
#include <gtsam/base/numericalDerivative.h>
#include <gtsam/inference/LabeledSymbol.h>
#include <gtsam/nonlinear/DoglegOptimizer.h>
#include <gtsam/nonlinear/GaussNewtonOptimizer.h>
#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/Values.h>
#include <gtsam/slam/PriorFactor.h>

#include <fstream>
#include <iostream>

namespace robot {

/* Shorthand for F_i_j_t, for wrenches at j-th joint on the i-th link at time t.
 */
gtsam::LabeledSymbol WrenchKey(int i, int j, int t) {
  return gtsam::LabeledSymbol('F', i * 16 + j,
                              t); // a hack here for a key with 3 numbers
}

/* Shorthand for T_j_t, for torque on the j-th joint at time t. */
gtsam::LabeledSymbol TorqueKey(int j, int t) {
  return gtsam::LabeledSymbol('T', j, t);
}

/* Shorthand for p_i_t, for COM pose on the i-th link at time t. */
gtsam::LabeledSymbol PoseKey(int i, int t) {
  return gtsam::LabeledSymbol('p', i, t);
}

/* Shorthand for V_i_t, for 6D link twist vector on the i-th link. */
gtsam::LabeledSymbol TwistKey(int i, int t) {
  return gtsam::LabeledSymbol('V', i, t);
}

/* Shorthand for A_i_t, for twist accelerations on the i-th link at time t. */
gtsam::LabeledSymbol TwistAccelKey(int i, int t) {
  return gtsam::LabeledSymbol('A', i, t);
}

/* Shorthand for q_j_t, for j-th joint angle at time t. */
gtsam::LabeledSymbol JointAngleKey(int j, int t) {
  return gtsam::LabeledSymbol('q', j, t);
}

/* Shorthand for v_j_t, for j-th joint velocity at time t. */
gtsam::LabeledSymbol JointVelKey(int j, int t) {
  return gtsam::LabeledSymbol('v', j, t);
}

/* Shorthand for a_j_t, for j-th joint acceleration at time t. */
gtsam::LabeledSymbol JointAccelKey(int j, int t) {
  return gtsam::LabeledSymbol('a', j, t);
}

/* Shorthand for t_k, for duration for timestep dt_k. */
gtsam::LabeledSymbol TimeKey(int k) { return gtsam::LabeledSymbol('t', 0, k); }

/**
 * DynamicsGraphBuilder is a class which builds a factor graph to do kinodynamic
 * motion planning
 */
class DynamicsGraphBuilder {
private:
  manipulator::OptimizerSetting opt_;

public:
  /**
   * Constructor
   */
  explicit DynamicsGraphBuilder() {
    opt_ = manipulator::OptimizerSetting();
    // set all dynamics related factors to be constrained
    opt_.bp_cost_model = gtsam::noiseModel::Constrained::All(6);
    opt_.bv_cost_model = gtsam::noiseModel::Constrained::All(6);
    opt_.ba_cost_model = gtsam::noiseModel::Constrained::All(6);
    opt_.p_cost_model = gtsam::noiseModel::Constrained::All(6);
    opt_.v_cost_model = gtsam::noiseModel::Constrained::All(6);
    opt_.a_cost_model = gtsam::noiseModel::Constrained::All(6);
    opt_.f_cost_model = gtsam::noiseModel::Constrained::All(6);
    opt_.t_cost_model = gtsam::noiseModel::Constrained::All(1);
    opt_.tf_cost_model = gtsam::noiseModel::Constrained::All(6);
    opt_.q_cost_model = gtsam::noiseModel::Constrained::All(1);
    opt_.qv_cost_model = gtsam::noiseModel::Constrained::All(1);
    opt_.setJointLimitCostModel(1e-3);

    opt_.setLM();
  }
  ~DynamicsGraphBuilder() {}

  /** return nonlinear factor graph of all dynamics factors
  * Keyword arguments:
     robot                      -- the robot
     t                          -- time step
     gravity                    -- gravity in world frame
     plannar_axis               -- the axis of the plane, used only in case of a
  planar robot
   */
  gtsam::NonlinearFactorGraph dynamicsFactorGraph(
      const UniversalRobot &robot, const int t,
      const boost::optional<gtsam::Vector3> &gravity = boost::none,
      const boost::optional<gtsam::Vector3> &plannar_axis = boost::none) const {
    using namespace gtsam;
    NonlinearFactorGraph graph;

    // add factors corresponding to links
    for (auto &&link : robot.links()) {
      int i = link->getID();
      if (link->isFixed()) {
        graph.add(PriorFactor<Pose3>(PoseKey(i, t), link->getFixedPose(),
                                     noiseModel::Constrained::All(6)));
        graph.add(PriorFactor<Vector6>(TwistKey(i, t), Vector6::Zero(),
                                       noiseModel::Constrained::All(6)));
        graph.add(PriorFactor<Vector6>(TwistAccelKey(i, t), Vector6::Zero(),
                                       noiseModel::Constrained::All(6)));
      } else {
        const auto &connected_joints = link->getJoints();
        if (connected_joints.size() == 0) {
          graph.add(WrenchFactor0(TwistKey(i, t), TwistAccelKey(i, t),
                                  PoseKey(i, t), opt_.f_cost_model,
                                  link->inertiaMatrix(), gravity));
        } else if (connected_joints.size() == 1) {
          graph.add(WrenchFactor1(TwistKey(i, t), TwistAccelKey(i, t),
                                  WrenchKey(i, connected_joints[0]->getID(), t),
                                  PoseKey(i, t), opt_.f_cost_model,
                                  link->inertiaMatrix(), gravity));
        } else if (connected_joints.size() == 2) {
          graph.add(WrenchFactor2(TwistKey(i, t), TwistAccelKey(i, t),
                                  WrenchKey(i, connected_joints[0]->getID(), t),
                                  WrenchKey(i, connected_joints[1]->getID(), t),
                                  PoseKey(i, t), opt_.f_cost_model,
                                  link->inertiaMatrix(), gravity));
        } else if (connected_joints.size() == 3) {
          graph.add(WrenchFactor3(TwistKey(i, t), TwistAccelKey(i, t),
                                  WrenchKey(i, connected_joints[0]->getID(), t),
                                  WrenchKey(i, connected_joints[1]->getID(), t),
                                  WrenchKey(i, connected_joints[2]->getID(), t),
                                  PoseKey(i, t), opt_.f_cost_model,
                                  link->inertiaMatrix(), gravity));
        } else if (connected_joints.size() == 4) {
          graph.add(WrenchFactor4(TwistKey(i, t), TwistAccelKey(i, t),
                                  WrenchKey(i, connected_joints[0]->getID(), t),
                                  WrenchKey(i, connected_joints[1]->getID(), t),
                                  WrenchKey(i, connected_joints[2]->getID(), t),
                                  WrenchKey(i, connected_joints[3]->getID(), t),
                                  PoseKey(i, t), opt_.f_cost_model,
                                  link->inertiaMatrix(), gravity));
        } else {
          throw std::runtime_error("Wrench factor not defined");
        }
      }
    }

    // add factors corresponding to joints
    for (auto &&joint : robot.joints()) {
      const auto &link_1 = joint->parentLink();
      const auto &link_2 = joint->childLink().lock();
      int i1 = link_1->getID();
      int i2 = link_2->getID(); // cannot use methods for a weak ptr?
      int j = joint->getID();
      // add pose factor
      graph.add(manipulator::PoseFactor(PoseKey(i1, t), PoseKey(i2, t),
                                        JointAngleKey(j, t), opt_.p_cost_model,
                                        joint->McpCom(), joint->screwAxis()));

      // add twist factor
      graph.add(manipulator::TwistFactor(TwistKey(i1, t), TwistKey(i2, t),
                                         JointAngleKey(j, t), JointVelKey(j, t),
                                         opt_.v_cost_model, joint->McpCom(),
                                         joint->screwAxis()));

      // add twist acceleration factor
      graph.add(manipulator::TwistAccelFactor(
          TwistKey(i2, t), TwistAccelKey(i1, t), TwistAccelKey(i2, t),
          JointAngleKey(j, t), JointVelKey(j, t), JointAccelKey(j, t),
          opt_.a_cost_model, joint->McpCom(), joint->screwAxis()));

      // add wrench equivalence factor
      if (!link_1->isFixed() && !link_2->isFixed()) {
        graph.add(WrenchEquivalenceFactor(
            WrenchKey(i1, j, t), WrenchKey(i2, j, t), JointAngleKey(j, t),
            opt_.f_cost_model, joint->McpCom(), joint->screwAxis()));
      }

      // add torque factor
      graph.add(manipulator::TorqueFactor(WrenchKey(i2, j, t), TorqueKey(j, t),
                                          opt_.t_cost_model,
                                          joint->screwAxis()));

      // add planar wrench factor
      if (plannar_axis) {
        graph.add(WrenchPlanarFactor(WrenchKey(i2, j, t),
                                     gtsam::noiseModel::Constrained::All(3),
                                     *plannar_axis));
      }
    }
    return graph;
  }

  /** return integration factors on joint angles and velocities from time step t
  to t+1
  * Keyword arguments:
     robot                      -- the robot
     t                          -- time step
     dt                         -- duration of each timestep
   */
  gtsam::NonlinearFactorGraph integrationFactors(const UniversalRobot &robot,
                                                 const int t, const double dt);

  /** return soft integration factors on joint angles and velocities from time
  step t to t+1
  * Keyword arguments:
     robot                      -- the robot
     t                          -- time step
   */
  gtsam::NonlinearFactorGraph
  softIntegrationFactors(const UniversalRobot &robot, const int t);

  /** return joint factors to limit angle, velocity, acceleration, and torque
  * Keyword arguments:
     robot                      -- the robot
     t                          -- time step
   */
  gtsam::NonlinearFactorGraph jointLimitFactors(const UniversalRobot &robot,
                                                const int t) const;

  /** return joint factors to limit angle, velocity, acceleration, and torque
  * Keyword arguments:
     robot                      -- the robot
     t                          -- time step
     joint_angles               -- joint angles specified in order of joints
     joint_vels                 -- joint velocites specified in order of joints
     torques                    -- joint torques specified in order of joints
   */
  gtsam::NonlinearFactorGraph
  forwardDynamicsPriors(const UniversalRobot &robot, const int t,
                        const gtsam::Vector &joint_angles,
                        const gtsam::Vector &joint_vels,
                        const gtsam::Vector &torques) const;

  /** return the joint accelerations
  * Keyword arguments:
     robot                      -- the robot
     t                          -- time step
   */
  static gtsam::Vector jointAccels(const UniversalRobot &robot,
                                   const gtsam::Values &result, const int t);

  static gtsam::Vector jointVels(const UniversalRobot &robot,
                                   const gtsam::Values &result, const int t);

  static gtsam::Vector jointAngles(const UniversalRobot &robot,
                                   const gtsam::Values &result, const int t);

  /** return zero values for all variables for initial value of optimization
  * Keyword arguments:
     robot                      -- the robot
     t                          -- time step
   */
  static gtsam::Values zeroValues(const UniversalRobot &robot, const int t);

  // print the factors of the factor graph
  static void print_graph(const gtsam::NonlinearFactorGraph &graph);

  // print the values
  static void print_values(const gtsam::Values &values);

  /** save factor graph in json format for visualization
  * Keyword arguments:
     file_path                  -- path of the json file to store the graph
     graph                      -- factor graph
     values                     -- values of variables in factor graph
     robot                      -- the robot
     t                          -- time step
     radial                     -- option to display in radial format
   */
  static void saveGraph(const std::string &file_path,
                        const gtsam::NonlinearFactorGraph &graph,
                        const gtsam::Values &values,
                        const UniversalRobot &robot, const int t,
                        bool radial = false);
};

} // namespace robot
