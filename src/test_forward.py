#!/usr/bin/env python
"""
Test forward dynamics using factor graphs.
Author: Frank Dellaert and Mandy Xie
"""

# pylint: disable=C0103, E1101, E0401

from __future__ import print_function

import math
import unittest

import numpy as np
# from gtsam import Point3, Pose3, Rot3
from gtsam import *                                      
from utils import *

def helper_calculate_twist_i(i_T_i_minus_1, joint_vel_i, twist_i_mius_1, screw_axis_i):
    twist_i = np.dot(i_T_i_minus_1.AdjointMap(), twist_i_mius_1) + screw_axis_i*joint_vel_i
    return twist_i


def forward_traditional_way_R():
    # Calculate joint accelerations for R manipulator with traditional method
    return vector(0)


def forward_factor_graph_way_R():
    # Setup factor graph
    # Optimize it
    # return joint accelerations

    # number of revolute joint
    num = 1

    twist_i_mius_1 = vector(0., 0., 0., 0., 0., 0.)
    # configuration of link 0 frame in space frame 0
    pose3_0 = Pose3(Rot3(np.identity(3)), Point3(0,0,0))
    # configuration of link 1 frame in space frame 0
    pose3_1 = Pose3(Rot3(np.identity(3)), Point3(1,0,0))
    # configuration of end effector frame in space frame 0
    pose3_2 = Pose3(Rot3(np.identity(3)), Point3(2,0,0))
    link_config = np.array([pose3_0, pose3_1, pose3_2])

    # joint velocity of link 1
    joint_vel = np.array([0.0, 1.0])

    # screw axis for joint 0 expressed in link frame 0
    screw_axis_0 = unit_twist(vector(0,0,0), vector(0,0,0))
    # screw axis for joint 1 expressed in link frame 1
    screw_axis_1 = unit_twist(vector(0,0,1), vector(-1,0,0))
    screw_axis = np.array([screw_axis_0, screw_axis_1])

    # inertial matrix of link i expressed in link frame i
    I0 = np.zeros((3,3))
    I1 = np.diag([0, 1/6., 1/6.])
    I = np.array([I0, I1])
    # mass of link i
    m0 = 0
    m1 = 1
    m = np.array([m0, m1])

    # Gaussian Factor Graph
    gfg = GaussianFactorGraph()

    for i in range(1, num+1):

        # configuration of link frame i-1 relative to link frame i for joint i angle 0 
        i_T_i_minus_1 = compose(link_config[i].inverse(), link_config[i-1])
        twist_i = helper_calculate_twist_i(i_T_i_minus_1, joint_vel[i], twist_i_mius_1, screw_axis[i])

        ## factor 1
        # LHS of acceleration equation 
        J_twist_accel_i = np.identity(6)
        J_twist_accel_i_mius_1 = -i_T_i_minus_1.AdjointMap()
        J_joint_accel_i = -np.array([[screw_axis[i][0]],
                                     [screw_axis[i][1]],
                                     [screw_axis[i][2]],
                                     [screw_axis[i][3]],
                                     [screw_axis[i][4]],
                                     [screw_axis[i][5]]])
        # RHS of acceleration equation 
        b_accel = np.dot(adtwist(twist_i), screw_axis[i]*joint_vel[i])

        key_twist_accel_i_minus_1 = 1 + 5*(i - 1)
        key_twist_accel_i = 2 + 5*(i - 1)
        key_joint_accel_i = 3 + 5*(i - 1)

        sigmas = np.zeros(6)
        model = noiseModel_Diagonal.Sigmas(sigmas)
        gfg.add(key_twist_accel_i_minus_1, J_twist_accel_i_mius_1, 
                                 key_twist_accel_i, J_twist_accel_i,
                                 key_joint_accel_i, J_joint_accel_i,
                                 b_accel, model)
        
        # configuration of link frame i relative to link frame i+1 for joint i+1 angle 0 
        i_plus_1_T_i = compose(link_config[i+1].inverse(), link_config[i])

        ## factor 2
        # LHS of wrench equation
        J_wrench_i = np.identity(6)
        J_wrench_i_plus_1 = -i_plus_1_T_i.AdjointMap().transpose()
        J_twist_accel_i = -genaral_mass_matrix(I[i], m[i])
        # RHS of wrench equation
        b_wrench = -np.dot(np.dot(adtwist(twist_i).transpose(), genaral_mass_matrix(I[i], m[i])), twist_i)

        key_wrench_i = 4 + 5*(i - 1)
        key_wrench_i_plus_1 = 5 + 5*(i - 1)

        sigmas = np.zeros(6)
        model = noiseModel_Diagonal.Sigmas(sigmas)
        gfg.add(key_wrench_i, J_wrench_i, 
                key_wrench_i_plus_1, J_wrench_i_plus_1,
                key_twist_accel_i, J_twist_accel_i,
                b_wrench, model)

        ## factor 3
        # LHS of torque equation
        J_wrench_i = np.array([screw_axis[i]])
        # RHS of torque equation
        b_torque = np.array([0])
        model = noiseModel_Diagonal.Sigmas(np.array([0.0]))
        gfg.add(key_wrench_i, J_wrench_i, b_torque, model)

        if i == 1:
            ## factor 4, prior factor, link 0 twist acceleration = 0
            # LHS 
            J_twist_accel_i_mius_1 = np.identity(6)
            # RHS
            b_twist_accel = np.zeros(6)
            sigmas = np.zeros(6)
            model = noiseModel_Diagonal.Sigmas(sigmas)
            gfg.add(key_twist_accel_i_minus_1, J_twist_accel_i_mius_1, b_twist_accel, model)

        if i == num:
            ## factor 5, prior factor, end effector wrench = 0
            # LHS 
            J_wrench_i_plus_1 = np.identity(6)
            # RHS
            b_wrench = np.zeros(6)
            sigmas = np.zeros(6)
            model = noiseModel_Diagonal.Sigmas(sigmas)
            gfg.add(key_wrench_i_plus_1, J_wrench_i_plus_1, b_wrench, model)

        twist_i_mius_1 = twist_i
        results = gfg.optimize()
        print(results)
    return vector(0)

class TestForwardDynamics(unittest.TestCase):
    """Unit tests for R manipulator class."""

    def gtsamAssertEquals(self, actual, expected, tol=1e-2):
        """Helper function that prints out actual and expected if not equal."""
        # TODO: make a unittest.TestCase class hat has this in GTSAM
        equal = actual.equals(expected, tol)
        if not equal:
            raise self.failureException(
                "Values are not equal:\n{}!={}".format(actual, expected))

    def setUp(self):
        """setup."""
        pass

    def test_R_forward_dynamics(self):
        """Try a simple R robot."""
        expected_joint_accels = forward_traditional_way_R()
        # Call a function with appropriate arguments to co compute them 
        actual_joint_accels = forward_factor_graph_way_R()
        np.testing.assert_array_almost_equal(actual_joint_accels, expected_joint_accels)


if __name__ == "__main__":
    unittest.main()
