/**
 * @file  testDynamicsGraph.cpp
 * @brief test forward and inverse dynamics factor graph
 * @Author: Yetong Zhang
 */

#include <DynamicsGraph.h>
#include <UniversalRobot.h>
#include <gtsam/geometry/Point3.h>
#include <gtsam/inference/Key.h>
#include <gtsam/inference/LabeledSymbol.h>
#include <gtsam/slam/PriorFactor.h>
#include <utils.h>
#include <JsonSaver.h>

#include <gtsam/base/numericalDerivative.h>
#include <gtsam/inference/Symbol.h>
#include <gtsam/nonlinear/GaussNewtonOptimizer.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/Values.h>

#include <CppUnitLite/TestHarness.h>
#include <gtsam/base/Testable.h>
#include <gtsam/base/TestableAssertions.h>

#include <iostream>
#include <fstream>

using namespace std;
using namespace robot;
using namespace gtsam;

int DEBUG_SIMPLE_OPTIMIZATION_EXAMPLE = 1;
int DEBUG_FOUR_BAR_LINKAGE_ILS_EXAMPLE = 0;

// print the info of links and joints of the robot
void print_robot(UniversalRobot& this_robot) {
  for (const auto& link: this_robot.links()) {
    cout<<link->name() << ":\n";
    cout<<"\tlink pose: " << link->getLinkPose().rotation().rpy().transpose() << ", " << link->getLinkPose().translation() << "\n";
    cout<<"\tcom pose: " << link->getComPose().rotation().rpy().transpose() << ", " << link->getComPose().translation() << "\n";
  }

  for (const auto& joint: this_robot.joints()) {
    cout << joint->name() << ":\n";
    cout<<"\tparent: " << joint->parentLink()->name() << "\tchild: " << joint->childLink().lock()->name() << "\n";
    cout<<"\tscrew axis: " << joint->screwAxis().transpose() << "\n";
    cout<<"\tpMc: " << joint->pMc().rotation().rpy().transpose() << ", " << joint->pMc().translation() << "\n";
    cout<<"\tpMc_com: " << joint->pMcCom().rotation().rpy().transpose() << ", " << joint->pMcCom().translation() << "\n";
  }
}

// print the factors of the factor graph
void print_graph(const NonlinearFactorGraph& graph) {
  for (auto& factor: graph) {
    for (auto& key: factor->keys()) {
      auto symb = LabeledSymbol(key);
      cout << symb.chr() << int(symb.label()) << "_" << symb.index() << "\t";
    }
    cout << "\n";
  }
}

// using radial location to locate the variables
gtsam::Vector3 radial_location(double r, int i) {
  if (i==1) {
    return (Vector(3) << r, 0, 0).finished();
  }
  else if (i==2) {
    return (Vector(3) << 0, -r, 0).finished();
  }
  else if (i==3) {
    return (Vector(3) << -r, 0, 0).finished();
  }
  else if (i==4) {
    return (Vector(3) << 0, r, 0).finished();
  }
  return (Vector(3) << 0, 0, 0).finished();
}

// using radial location to locate the variables
gtsam::Vector3 corner_location(double r, int j) {
  r = r * 0.7;
  if (j==1) {
    return (Vector(3) << r, -r, 0).finished();
  }
  else if (j==2) {
    return (Vector(3) << -r, -r, 0).finished();
  }
  else if (j==3) {
    return (Vector(3) << -r, r, 0).finished();
  }
  else if (j==4) {
    return (Vector(3) << r, r, 0).finished();
  }
  return (Vector(3) << 0, 0, 0).finished();
}

// print the values
void print_values(const Values& result) {
      for (auto& key: result.keys()) {
      auto symb = LabeledSymbol(key);
      cout << symb.chr() << int(symb.label()) << "_" << symb.index() << " ";
      result.at(key).print();
      cout << "\n";
    }
}

// Test forward dynamics with gravity
TEST(FD_factor_graph, optimization) {

  // Load the robot from urdf file
  UniversalRobot simple_robot = UniversalRobot("../../../urdfs/test/four_bar_linkage_pure.urdf");
  print_robot(simple_robot);

  Vector twists = Vector6::Zero(), accels = Vector6::Zero(),
      wrenches = Vector6::Zero();
  Vector q = Vector::Zero(simple_robot.numJoints());
  Vector v = Vector::Zero(simple_robot.numJoints());
  Vector a = Vector::Zero(simple_robot.numJoints());
  Vector torque = Vector::Zero(simple_robot.numJoints());
  Vector3 gravity = (Vector(3) << 0, 0, 0).finished();
  Vector3 planar_axis = (Vector(3) << 1, 0, 0).finished();

  // build the dynamics factor graph
  auto graph_builder = DynamicsGraphBuilder();
  NonlinearFactorGraph graph = graph_builder.dynamicsFactorGraph(simple_robot, 0, gravity, planar_axis);

  // specify known values
  for (auto link: simple_robot.links()) {
    int i = link -> getID();
    graph.add(PriorFactor<Pose3>(PoseKey(i, 0), link -> getComPose(), noiseModel::Constrained::All(6)));
    graph.add(PriorFactor<Vector6>(TwistKey(i, 0), Vector6::Zero(), noiseModel::Constrained::All(6)));
  }
  for (auto joint: simple_robot.joints()) {
    int j = joint -> getID();
    graph.add(PriorFactor<double>(JointAngleKey(j, 0), 0, noiseModel::Constrained::All(1)));
    graph.add(PriorFactor<double>(JointVelKey(j, 0), 0, noiseModel::Constrained::All(1)));
    if ((j==1) || (j==3)) {
      graph.add(PriorFactor<double>(TorqueKey(j, 0), 1, noiseModel::Constrained::All(1)));
    }
    else {
      graph.add(PriorFactor<double>(TorqueKey(j, 0), 0, noiseModel::Constrained::All(1)));
    }
  }

  // set initial values
  Values init_values;
  for (auto link: simple_robot.links()) {
    int i = link -> getID();
    init_values.insert(PoseKey(i, 0), link -> getComPose());
    init_values.insert(TwistKey(i, 0), twists);
    init_values.insert(TwistAccelKey(i, 0), accels);
  }
  for (auto joint: simple_robot.joints()) {
    int j = joint -> getID();
    init_values.insert(WrenchKey(joint->parentLink()->getID(), j, 0), wrenches);
    init_values.insert(WrenchKey(joint->childLink().lock()->getID(), j, 0), wrenches);
    Vector torque0 = Vector::Zero(1);
    // torque0 << 1;
    init_values.insert(TorqueKey(j, 0), torque0[0]);
    init_values.insert(JointAngleKey(j, 0), q[0]);
    init_values.insert(JointVelKey(j, 0), v[0]);
    init_values.insert(JointAccelKey(j, 0), a[0]);
  }
  print_values(init_values);

  // graph.print("", MultiRobotKeyFormatter);
  if(DEBUG_SIMPLE_OPTIMIZATION_EXAMPLE) {
    print_graph(graph);
  }

  GaussNewtonOptimizer optimizer(graph, init_values);
  optimizer.optimize();
  Values result = optimizer.values();


  for (auto& key: result.keys()) {
    auto symb = LabeledSymbol(key);
    cout << symb.chr() << int(symb.label()) << "_" << symb.index() << " ";
    result.at(key).print();
    cout << "\n";
  }

  // set the factor graph display locations
  int t = 0;
  JsonSaver::LocationType locations;
  // for (auto link: simple_robot.links()) {
  //   int i = link -> getID();
  //   locations[PoseKey(i, t)] = (Vector(3) << i, 0, 0).finished();
  //   locations[TwistKey(i, t)] = (Vector(3) << i, 1, 0).finished();
  //   locations[TwistAccelKey(i, t)] = (Vector(3) << i, 2, 0).finished();
  // }

  // for (auto joint: simple_robot.joints()) {
  //   int j = joint -> getID();
  //   locations[JointAngleKey(j, t)] = (Vector(3) << j + 0.5 , 0.5, 0).finished();
  //   locations[JointVelKey(j, t)] = (Vector(3) << j + 0.5 , 1.5, 0).finished();
  //   locations[JointAccelKey(j, t)] = (Vector(3) << j + 0.5 , 2.5, 0).finished();
  //   int i1 = joint -> parentLink()  ->getID();
  //   int i2 = joint -> childLink().lock()->getID(); // cannot use methods for a weak ptr?
  //   locations[WrenchKey(i1, j, t)] = (Vector(3) << j + 0.25 , 3.5, 0).finished();
  //   locations[WrenchKey(i2, j, t)] = (Vector(3) << j + 0.75 , 3.5, 0).finished();
  //   locations[TorqueKey(j, t)] = (Vector(3) << j + 0.5 , 4.5, 0).finished();
  // }

  for (auto link: simple_robot.links()) {
    int i = link -> getID();
    locations[PoseKey(i, t)] = radial_location(5, i);
    locations[TwistKey(i, t)] = radial_location(4, i);
    locations[TwistAccelKey(i, t)] = radial_location(3, i);
  }

  for (auto joint: simple_robot.joints()) {
    int j = joint -> getID();
    locations[JointAngleKey(j, t)] = corner_location(5, j);
    locations[JointVelKey(j, t)] = corner_location(4, j);
    locations[JointAccelKey(j, t)] = corner_location(3, j);
    // int i1 = joint -> parentLink()  ->getID();
    // int i2 = joint -> childLink().lock()->getID(); // cannot use methods for a weak ptr?
    // locations[WrenchKey(i1, j, t)] = (Vector(3) << j + 0.25 , 3.5, 0).finished();
    // locations[WrenchKey(i2, j, t)] = (Vector(3) << j + 0.75 , 3.5, 0).finished();
    locations[TorqueKey(j, t)] = corner_location(1, j);
  }

  cout << "error: " << graph.error(result) << "\n";
  ofstream json_file;
  json_file.open ("../../../visualization/factor_graph.json");
  JsonSaver::SaveFactorGraph(graph, json_file, result, locations);
  json_file.close();
}



int main() {
  TestResult tr;
  return TestRegistry::runAllTests(tr);
}