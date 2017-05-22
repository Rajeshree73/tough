#include <val_task2/solar_panel_grasp.h>
#include <stdlib.h>
#include <stdio.h>


solar_panel_handle_grabber::solar_panel_handle_grabber(ros::NodeHandle n):nh_(n), armTraj_(nh_), gripper_(nh_)
{
    current_state_ = RobotStateInformer::getRobotStateInformer(nh_);
    leftHandOrientation_.header.frame_id = VAL_COMMON_NAMES::PELVIS_TF;
    /* Top Grip */

    leftHandOrientation_.quaternion.x = 0.089;
    leftHandOrientation_.quaternion.y = 0.680;
    leftHandOrientation_.quaternion.z = -0.519;
    leftHandOrientation_.quaternion.w = 0.510;

    /* Top Grip */
    rightHandOrientation_.header.frame_id = VAL_COMMON_NAMES::PELVIS_TF;
    rightHandOrientation_.quaternion.x = -0.175;
    rightHandOrientation_.quaternion.y = 0.714;
    rightHandOrientation_.quaternion.z = 0.488;
    rightHandOrientation_.quaternion.w = 0.471;

    // cartesian planners for the arm
    left_arm_planner_ = new cartesianPlanner("leftPalm", VAL_COMMON_NAMES::WORLD_TF);
    right_arm_planner_ = new cartesianPlanner("rightPalm", VAL_COMMON_NAMES::WORLD_TF);
    wholebody_controller_ = new wholebodyManipulation(nh_);
}

solar_panel_handle_grabber::~solar_panel_handle_grabber()
{

}


geometry_msgs::QuaternionStamped solar_panel_handle_grabber::leftHandOrientation() const
{
    return leftHandOrientation_;
}

void solar_panel_handle_grabber::setLeftHandOrientation(const geometry_msgs::QuaternionStamped &leftHandOrientation)
{
    leftHandOrientation_ = leftHandOrientation;
}
geometry_msgs::QuaternionStamped solar_panel_handle_grabber::rightHandOrientation() const
{
    return rightHandOrientation_;
}

void solar_panel_handle_grabber::setRightHandOrientation(const geometry_msgs::QuaternionStamped &rightHandOrientation)
{
    rightHandOrientation_ = rightHandOrientation;
}


void solar_panel_handle_grabber::grasp_handles(const armSide side, const geometry_msgs::Pose &goal, float executionTime)
{
    const std::vector<float>* seed;
    if(side == armSide::LEFT){
        seed = &leftShoulderSeed_;
    }
    else {
        seed = &rightShoulderSeed_;
    }


    ROS_INFO("opening grippers");
    gripper_.openGripper(side);

    //move shoulder roll outwards
    ROS_INFO("Setting shoulder roll");
    std::vector< std::vector<float> > armData;
    armData.push_back(*seed);

    armTraj_.moveArmJoints(side, armData, executionTime);
    ros::Duration(executionTime*2).sleep();


    //move arm to given point with known orientation and higher z
    geometry_msgs::Pose finalGoal, intermGoal;
    geometry_msgs::Point finalPoint, intermPoint;

    current_state_->transformPose(goal,intermGoal, VAL_COMMON_NAMES::WORLD_TF, VAL_COMMON_NAMES::PELVIS_TF);
    intermGoal.position.z += 0.1;

    //transform that point back to world frame
    current_state_->transformPose(intermGoal, intermGoal, VAL_COMMON_NAMES::PELVIS_TF, VAL_COMMON_NAMES::WORLD_TF);

    ROS_INFO("Moving at an intermidate point before goal");
    armTraj_.moveArmInTaskSpace(side, intermGoal, executionTime*2);
    ros::Duration(executionTime*2).sleep();

    //move arm to final position with known orientation

    current_state_->transformPose(goal,finalGoal, VAL_COMMON_NAMES::WORLD_TF, VAL_COMMON_NAMES::PELVIS_TF);
    finalGoal.position.x -= 0.04; // this is to compensate for the distance between palm frame and center of palm

    //transform that point back to world frame
    current_state_->transformPose(finalGoal, finalGoal, VAL_COMMON_NAMES::PELVIS_TF, VAL_COMMON_NAMES::WORLD_TF);

    ROS_INFO("Moving towards goal");

    std::vector<geometry_msgs::Pose> waypoints;

    waypoints.push_back(finalGoal);
    ROS_INFO_STREAM(finalGoal);
    moveit_msgs::RobotTrajectory traj;
    if(side == armSide::LEFT)
    {
        left_arm_planner_->getTrajFromCartPoints(waypoints, traj, false);
    }
    else
    {
        right_arm_planner_->getTrajFromCartPoints(waypoints, traj, false);
    }
    ROS_INFO("Calculated Traj");
    wholebody_controller_->compileMsg(side, traj.joint_trajectory);

    ros::Duration(executionTime).sleep();
    ROS_INFO("Closing grippers");
    gripper_.closeGripper(side);
    ros::Duration(0.3).sleep();
}