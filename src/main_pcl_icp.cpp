#include "ros/ros.h"
#include "ros/package.h"
#include <rosbag/bag.h>
#include <rosbag/view.h>
#include "sensor_msgs/PointCloud2.h"
#include "sensor_msgs/Image.h"
#include "nav_msgs/Odometry.h"
#include <boost/foreach.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <sensor_msgs/image_encodings.h>
//#include "cv_bridge/cv_bridge.h"
#define foreach BOOST_FOREACH

#include "loam/loam_wrapper.h"
#include "loam/transformMaintenance.h"
#include "loam/laserMapping.h"
#include "loam/scanRegistration.h"
#include "loam/laserOdometry.h"

#include "KITTI_utils/KITTI.h"

#include "KITTI_utils/toMATLAB.h"

#include <pcl/registration/registration.h>
#include <pcl/registration/transformation_estimation_point_to_plane.h>

struct data
{
    nav_msgs::OdometryPtr * ptr_od;
    sensor_msgs::PointCloud2Ptr * ptr_pc;
    sensor_msgs::ImagePtr * ptr_img;
};

int main( int argc, char** argv )
{
    ros::init(argc, argv, "KITTI");
    int offset = 0;
    int maxId = 4000;
    KITTI kitti(0,maxId,offset);
    loam_wrapper loam;


    std::vector<Eigen::Matrix4d> T_result;
    std::vector<Eigen::Matrix4d> T_result_delta;
    T_result.push_back(Eigen::Matrix4d::Identity());
    Eigen::Matrix4d T_offset = kitti.getGroundTruthByID(offset);
    Eigen::Matrix4d T_all_od = kitti.getGroundTruthByID(offset);
    Eigen::Matrix4d T_all_map = kitti.getGroundTruthByID(offset);
    Eigen::Matrix4d T_diff_od_map;


    Eigen::Matrix4d T_Velo_to_Cam = kitti.getVelo_to_cam_T();
    for (int i=offset; i<maxId-1;i++)
    {

        pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_source_filtered (new pcl::PointCloud<pcl::PointXYZ>);
        pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_target_filtered (new pcl::PointCloud<pcl::PointXYZ>);
        // Get Input Clouds
        pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_source(new pcl::PointCloud<pcl::PointXYZ>());
        kitti.getOneVel(cloud_source_filtered,i);
        pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_target(new pcl::PointCloud<pcl::PointXYZ>());
        kitti.getOneVel(cloud_target_filtered,i+1);


        pcl::VoxelGrid<pcl::PointXYZ> downSizeFilter;
        downSizeFilter.setInputCloud(cloud_source_filtered);
        downSizeFilter.setLeafSize(0.1f, 0.1f, 0.1f);
        downSizeFilter.filter(*cloud_source);

        pcl::VoxelGrid<pcl::PointXYZ> downSizeFilter2;
        downSizeFilter2.setInputCloud(cloud_target_filtered);
        downSizeFilter2.setLeafSize(0.1f, 0.1f, 0.1f);
        downSizeFilter2.filter(*cloud_target);

        // Transform into Cam coordinate system
        pcl::transformPointCloud(*cloud_source, *cloud_source, T_Velo_to_Cam);
        pcl::transformPointCloud(*cloud_target, *cloud_target, T_Velo_to_Cam);

        // transform according to ground truth
        Eigen::Matrix4d T_1 = kitti.getGroundTruthByID(i);
        Eigen::Matrix4d T_2 = kitti.getGroundTruthByID(i+1);
        Eigen::Matrix4d T_diff = T_1.inverse() * T_2;
        pcl::transformPointCloud (*cloud_target, *cloud_target, T_diff);



        loam.publishFirst(cloud_source);
        loam.publishSecond(cloud_target);








        pcl::PointCloud<pcl::PointNormal>::Ptr src(new pcl::PointCloud<pcl::PointNormal>);
        pcl::copyPointCloud(*cloud_source, *src);
        pcl::PointCloud<pcl::PointNormal>::Ptr tgt(new pcl::PointCloud<pcl::PointNormal>);
        pcl::copyPointCloud(*cloud_target, *tgt);

        pcl::NormalEstimation<pcl::PointNormal, pcl::PointNormal> norm_est;
        norm_est.setSearchMethod (pcl::search::KdTree<pcl::PointNormal>::Ptr (new pcl::search::KdTree<pcl::PointNormal>));
        norm_est.setKSearch (10);
        norm_est.setInputCloud (tgt);
        norm_est.compute (*tgt);

        pcl::IterativeClosestPoint<pcl::PointNormal, pcl::PointNormal> icp;
        typedef pcl::registration::TransformationEstimationPointToPlane<pcl::PointNormal, pcl::PointNormal> PointToPlane;
        boost::shared_ptr<PointToPlane> point_to_plane(new PointToPlane);
        icp.setTransformationEstimation(point_to_plane);
        icp.setInputCloud(src);
        icp.setInputTarget(tgt);
        icp.setRANSACOutlierRejectionThreshold(0.05);
        icp.setRANSACIterations(100);
        icp.setMaximumIterations(1000);
        icp.setTransformationEpsilon(1e-3);
        pcl::PointCloud<pcl::PointNormal> output;
        icp.align(output);








        // Obtain the transformation that aligned cloud_source to cloud_source_registered
        Eigen::Matrix4d T_back= icp.getFinalTransformation().cast<double>();




        T_all_map = T_back * T_all_map * T_diff;

        Eigen::Matrix4d T_gt = kitti.getGroundTruthDeltaByID(i);


        T_result_delta.push_back(T_back);

        T_all_od = T_offset*loam.T_total_od;

        std::cout << "[INFO]: i=" << i << std::endl;
        std::cout << "T_back:" << T_back << std::endl;
        std::cout << "T_all_od:" << T_all_od << std::endl;
        std::cout << "T_all_map:" << T_all_map << std::endl;
        std::cout << "T_gt:" << kitti.getGroundTruthByID(i) << std::endl;
        std::cout << "T_diff_od_map:" << T_diff_od_map << std::endl;

        T_result.push_back(T_all_map);


        loam.publishFirst(cloud_source);
        pcl::transformPointCloud (*cloud_target, *cloud_target, T_back);
        loam.publishSecond(cloud_target);






    }

    kitti.plotDeltaPoses(T_result,0);
    kitti.eval(T_result);

    return 0;
}



