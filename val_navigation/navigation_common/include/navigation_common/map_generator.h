#ifndef MAP_GENERATOR_H
#define MAP_GENERATOR_H

#define MAP_RESOLUTION 0.05
#define MAP_HEIGHT     100/MAP_RESOLUTION
#define MAP_WIDTH      100/MAP_RESOLUTION
#define MAP_X_OFFSET   -50.0
#define MAP_Y_OFFSET   -50.0

#include <ros/ros.h>
#include <nav_msgs/OccupancyGrid.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/point_cloud2_iterator.h>
#include "val_controllers/robot_state.h"
#include <mutex>


enum CELL_STATUS{
    FREE = 0,
    VISITED = 50,
    BLOCKED = 90,
    OCCUPIED = 100
};

class MapGenerator{

public:
    MapGenerator(ros::NodeHandle &n);
    ~MapGenerator();

    static void trimTo2DecimalPlaces(float &x, float &y);
    static size_t getIndex(float x, float y);

private:
    std::mutex mtx;

    void resetMap(const std_msgs::Empty &msg);
    void clearCurrentPoseCB(const std_msgs::Empty &msg);
    void convertToOccupancyGrid(const sensor_msgs::PointCloud2Ptr msg);
    void updatePointsToBlock(const sensor_msgs::PointCloud2Ptr msg);
    void timerCallback(const ros::TimerEvent& e);

    ros::NodeHandle nh_;
    ros::Subscriber pointcloudSub_;
    ros::Subscriber resetMapSub_;
    ros::Subscriber clearCurrentPoseSub_;
    ros::Subscriber blockMapSub_;
    ros::Publisher  mapPub_;
    ros::Publisher  visitedMapPub_;
    nav_msgs::OccupancyGrid occGrid_;
    nav_msgs::OccupancyGrid visitedOccGrid_;
    sensor_msgs::PointCloud2 pointsToBlock_;
    ros::Timer timer_;
    RobotStateInformer* currentState_;

};

#endif // MAP_GENERATOR_H


