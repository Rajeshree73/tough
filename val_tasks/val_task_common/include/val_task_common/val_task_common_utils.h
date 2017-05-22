#pragma once

#include <ros/ros.h>
#include <geometry_msgs/Pose2D.h>
#include <geometry_msgs/Pose.h>
#include <val_control/val_arm_navigation.h>

#define DISTANCE_TOLERANCE      0.02      // 2cm
#define ANGLE_TOLERANCE         0.0174533   // 1 degree
#define GOAL_DISTANCE_TOLERANCE 0.05      // 5cm
#define GOAL_ANGLE_TOLERANCE    ANGLE_TOLERANCE*3   // 3 degrees

namespace taskCommonUtils {

bool isPoseChanged(geometry_msgs::Pose2D pose_old, geometry_msgs::Pose2D pose_new);
bool isGoalReached(geometry_msgs::Pose2D pose_old, geometry_msgs::Pose2D pose_new);
bool isGoalReached(geometry_msgs::Pose pose_old, geometry_msgs::Pose2D pose_new);
void moveToWalkSafePose(ros::NodeHandle &nh);
void moveToInitPose(ros::NodeHandle &nh);
void fixHandFramePose(ros::NodeHandle nh, armSide side, geometry_msgs::Pose &poseInWorldFrame);
}