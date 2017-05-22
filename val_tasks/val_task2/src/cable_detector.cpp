#include "val_task2/cable_detector.h"
#include <visualization_msgs/Marker.h>
#include "val_common/val_common_names.h"
#include <pcl/common/centroid.h>
#include <thread>

//#define DISABLE_DRAWINGS true
#define DISABLE_TRACKBAR true

CableDetector::CableDetector(ros::NodeHandle nh) : nh_(nh), ms_sensor_(nh_), organizedCloud_(new src_perception::StereoPointCloudColor)
{
    ms_sensor_.giveQMatrix(qMatrix_);
    marker_pub_ = nh_.advertise<visualization_msgs::MarkerArray>("detected_cable",1);
}

void CableDetector::showImage(cv::Mat image, std::string caption)
{
#ifdef DISABLE_DRAWINGS
    return;
#endif
    cv::namedWindow( caption, cv::WINDOW_AUTOSIZE );
    cv::imshow( caption, image);
    cv::waitKey(3);
}

void CableDetector::colorSegment(cv::Mat &imgHSV, cv::Mat &outImg)
{
    cv::inRange(imgHSV,cv::Scalar(hsv_[0], hsv_[2], hsv_[4]), cv::Scalar(hsv_[1], hsv_[3], hsv_[5]), outImg);
#ifdef DISABLE_TRACKBAR
    return;
#endif
    setTrackbar();

    cv::imshow("binary thresholding", outImg);
    cv::imshow("HSV thresholding", imgHSV);
    cv::waitKey(3);
}

size_t CableDetector::findMaxContour(const std::vector<std::vector<cv::Point> >& contours)
{
    int largest_area = 0;
    int largest_contour_index = 0;
    std::vector<std::vector<cv::Point>> hull(contours.size());

    for( size_t i = 0; i< contours.size(); i++ )
    {
        cv::convexHull(contours[i], hull[i], false);
        //  Find the area of contour
        double area = cv::contourArea( hull[i]);

        if(area > largest_area)
        {
            largest_area = area;
            // Store the index of largest contour
            largest_contour_index = i;
        }
    }

    return largest_contour_index;
}

bool CableDetector::getCableLocation(geometry_msgs::Point& cableLoc)
{
    bool foundCable = false;
    src_perception::StereoPointCloudColor::Ptr organizedCloud(new src_perception::StereoPointCloudColor);
    src_perception::PointCloudHelper::generateOrganizedRGBDCloud(current_disparity_, current_image_, qMatrix_, organizedCloud);
    tf::TransformListener listener;
    geometry_msgs::PointStamped geom_point;
    geometry_msgs::PointStamped geom_point1;
    geometry_msgs::PointStamped geom_point2;
    std::vector<std::vector<cv::Point> > contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::Mat imgHSV, outImg;
    std::vector<cv::Point> points;

    cv::cvtColor(current_image_, imgHSV, cv::COLOR_BGR2HSV);
    colorSegment(imgHSV, outImg);

    // Find contours
    cv::findContours(outImg, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, cv::Point(0, 0));

    Eigen::Vector4f cloudCentroid;
    if(!contours.empty()) //avoid seg fault at runtime by checking that the contours exist
    {
        foundCable = true;
        points = getOrientation(contours[findMaxContour(contours)], outImg);
        //ROS_INFO_STREAM(point << std::endl);
        double theta = (1/3600.0) * 3.14159265359;
        double r = std::sqrt(std::pow(points[1].x - points[0].x , 2) + std::pow(points[1].y - points[0].y , 2)) + 5.0;
        pcl::PointCloud<pcl::PointXYZRGB> currentDetectionCloud;

        for (size_t k = 0; k < 360; k++ )
        {
            for (size_t l = 0; l < 10; l++ )
            {

                pcl::PointXYZRGB temp_pclPoint;
                try
                {
                    temp_pclPoint = organizedCloud->at(int(points[1].x + r * std::cos(k * l * theta)), int(points[1].y + r * std::sin(k * l * theta)));
                }
                catch (const std::out_of_range& ex){
                    ROS_ERROR("%s",ex.what());
                    return false;
                }

                if (temp_pclPoint.z > -2.0 && temp_pclPoint.z < 2.0 )
                {
                    currentDetectionCloud.push_back(pcl::PointXYZRGB(temp_pclPoint));
                }
            }
        }
        //  Calculating the Centroid of the handle Point cloud
        pcl::compute3DCentroid(currentDetectionCloud, cloudCentroid);
    }

    if( foundCable )
    {
        pcl::PointXYZRGB temp_pclPoint1 = organizedCloud->at(points[0].x, points[0].y);
        pcl::PointXYZRGB temp_pclPoint2 = organizedCloud->at(points[1].x, points[1].y);

        geom_point1.point.x = temp_pclPoint1.x;
        geom_point1.point.y = temp_pclPoint1.y;
        geom_point1.point.z = temp_pclPoint1.z;
        geom_point1.header.frame_id = VAL_COMMON_NAMES::LEFT_CAMERA_OPTICAL_FRAME_TF;

        geom_point2.point.x = temp_pclPoint2.x;
        geom_point2.point.y = temp_pclPoint2.y;
        geom_point2.point.z = temp_pclPoint2.z;
        geom_point2.header.frame_id = VAL_COMMON_NAMES::LEFT_CAMERA_OPTICAL_FRAME_TF;

        geom_point.point.x = cloudCentroid(0);
        geom_point.point.y = cloudCentroid(1);
        geom_point.point.z = cloudCentroid(2);
        geom_point.header.frame_id = VAL_COMMON_NAMES::LEFT_CAMERA_OPTICAL_FRAME_TF;
        try
        {
            listener.waitForTransform(VAL_COMMON_NAMES::WORLD_TF, VAL_COMMON_NAMES::LEFT_CAMERA_OPTICAL_FRAME_TF, ros::Time(0), ros::Duration(3.0));
            listener.transformPoint(VAL_COMMON_NAMES::WORLD_TF, geom_point, geom_point);
            listener.transformPoint(VAL_COMMON_NAMES::WORLD_TF, geom_point1, geom_point1);
            listener.transformPoint(VAL_COMMON_NAMES::WORLD_TF, geom_point2, geom_point2);
        }
        catch (tf::TransformException ex){
            ROS_ERROR("%s",ex.what());
            return false;
        }
    }
    cableLoc.x = double (geom_point.point.x);
    cableLoc.y = double (geom_point.point.y);
    cableLoc.z = double (geom_point.point.z);

    visualize_point(geom_point1.point, 0.0, 0.0, 1.0);
    //visualize_point(geom_point2.point, 0.0, 1.0, 0.0);
    visualize_point(geom_point.point, 0.7, 0.5, 0.0);

    marker_pub_.publish(markers_);

    return foundCable;
}

std::vector<cv::Point> CableDetector::getOrientation(const std::vector<cv::Point> &contourPts, cv::Mat &img)
{
    int sz = static_cast<int>(contourPts.size());
    cv::Mat data_pts = cv::Mat(sz, 2, CV_64FC1);

    for (int i = 0; i < data_pts.rows; ++i)
    {
        data_pts.at<double>(i, 0) = contourPts[i].x;
        data_pts.at<double>(i, 1) = contourPts[i].y;
    }

    //Perform PCA analysis
    cv::PCA pca_analysis(data_pts, cv::Mat(), CV_PCA_DATA_AS_ROW);

    //Store the center of the object
    cv::Point cntr = cv::Point(static_cast<int>(pca_analysis.mean.at<double>(0, 0)),
    static_cast<int>(pca_analysis.mean.at<double>(0, 1)));

    //Store the eigenvalues and eigenvectors
    std::vector<cv::Point2d> eigen_vecs(2);
    std::vector<double> eigen_val(2);

    for (int i = 0; i < 2; ++i)
    {
        eigen_vecs[i] = cv::Point2d(pca_analysis.eigenvectors.at<double>(i, 0),
        pca_analysis.eigenvectors.at<double>(i, 1));
        eigen_val[i] = pca_analysis.eigenvalues.at<double>(0, i);
    }
    //ROS_INFO_STREAM(cntr << std::endl);
    // Draw the principal components
    cv::circle(current_image_, cntr, 3, cv::Scalar(255, 0, 255), 2);
    cv::Point p1 = cntr + 0.3 * cv::Point(static_cast<int>(eigen_vecs[0].x * eigen_val[0]), static_cast<int>(eigen_vecs[0].y * eigen_val[0]));
    cv::Point p2 = cntr + 1.0 * cv::Point(static_cast<int>(eigen_vecs[1].x * eigen_val[1]), static_cast<int>(eigen_vecs[1].y * eigen_val[1]));

    //VISUALIZATION - Uncomment this line
    drawAxis(current_image_, cntr, p1, cv::Scalar(0, 255, 0), 1);
    drawAxis(current_image_, cntr, p2, cv::Scalar(255, 255, 0), 1);
    //ROS_INFO_STREAM(p2 << std::endl);
    std::vector<cv::Point> points(2);
    points[0] = cntr;
    points[1] = p1;
    showImage(current_image_);
    return points;
}

bool CableDetector::findCable(geometry_msgs::Point& cableLoc)
{
    //VISUALIZATION - include a spinOnce here to visualize the eigenvectors
    markers_.markers.clear();
    if(ms_sensor_.giveImage(current_image_))
    {
        if( ms_sensor_.giveDisparityImage(current_disparity_))
        {
            return getCableLocation(cableLoc);
        }
    }
    return 0;
}

void CableDetector::drawAxis(cv::Mat& img, cv::Point p, cv::Point q, cv::Scalar colour, const float scale)
{
    double angle;
    double hypotenuse;

    angle = std::atan2( (double) p.y - q.y, (double) p.x - q.x ); // angle in radians
    hypotenuse = std::sqrt( (double) (p.y - q.y) * (p.y - q.y) + (p.x - q.x) * (p.x - q.x));
    // double degrees = angle * 180 / CV_PI; // convert radians to degrees (0-180 range)
    // cout << "Degrees: " << abs(degrees - 180) << endl; // angle in 0-360 degrees range
    // Here we lengthen the arrow by a factor of scale

    q.x = (int) (p.x - scale * hypotenuse * std::cos(angle));
    q.y = (int) (p.y - scale * hypotenuse * std::sin(angle));
    cv::line(img, p, q, colour, 1, CV_AA);
    // create the arrow hooks
    p.x = (int) (q.x + 9 * std::cos(angle + CV_PI / 4));
    p.y = (int) (q.y + 9 * std::sin(angle + CV_PI / 4));
    cv::line(img, p, q, colour, 1, CV_AA);
    p.x = (int) (q.x + 9 * std::cos(angle - CV_PI / 4));
    p.y = (int) (q.y + 9 * std::sin(angle - CV_PI / 4));
    cv::line(img, p, q, colour, 1, CV_AA);
}

void CableDetector::setTrackbar()
{
    cv::namedWindow("binary thresholding");
    cv::createTrackbar("hl", "binary thresholding", &hsv_[0], 180);
    cv::createTrackbar("hu", "binary thresholding", &hsv_[1], 180);
    cv::createTrackbar("sl", "binary thresholding", &hsv_[2], 255);
    cv::createTrackbar("su", "binary thresholding", &hsv_[3], 255);
    cv::createTrackbar("vl", "binary thresholding", &hsv_[4], 255);
    cv::createTrackbar("vu", "binary thresholding", &hsv_[5], 255);

    cv::getTrackbarPos("hl", "binary thresholding");
    cv::getTrackbarPos("hu", "binary thresholding");
    cv::getTrackbarPos("sl", "binary thresholding");
    cv::getTrackbarPos("su", "binary thresholding");
    cv::getTrackbarPos("vl", "binary thresholding");
    cv::getTrackbarPos("vu", "binary thresholding");
}

void CableDetector::visualize_point(geometry_msgs::Point point, double r, double g, double b){

    std::cout<< "goal origin :\n"<< point << std::endl;
    static int id = 0;
    visualization_msgs::Marker marker;
    // Set the frame ID and timestamp.  See the TF tutorials for information on these.
    marker.header.frame_id = VAL_COMMON_NAMES::WORLD_TF;
    marker.header.stamp = ros::Time::now();

    // Set the namespace and id for this marker.  This serves to create a unique ID
    // Any marker sent with the same namespace and id will overwrite the old one
    marker.ns = std::to_string(frameID_);
    marker.id = id++;

    // Set the marker type.  Initially this is CUBE, and cycles between that and SPHERE, ARROW, and CYLINDER
    marker.type = visualization_msgs::Marker::CUBE;

    // Set the marker action.  Options are ADD, DELETE, and new in ROS Indigo: 3 (DELETEALL)
    marker.action = visualization_msgs::Marker::ADD;

    // Set the pose of the marker.  This is a full 6DOF pose relative to the frame/time specified in the header
    marker.pose.position = point;
    marker.pose.orientation.w = 1.0f;

    marker.scale.x = 0.01;
    marker.scale.y = 0.01;
    marker.scale.z = 0.01;
    marker.color.a = 1.0; // Don't forget to set the alpha!
    marker.color.r = r;
    marker.color.g = g;
    marker.color.b = b;

    markers_.markers.push_back(marker);
}

CableDetector::~CableDetector()
{

}