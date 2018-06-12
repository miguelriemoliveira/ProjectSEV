//Includes
#define PCL_NO_PRECOMPILE
#include <cmath>
#include <ros/ros.h>
#include <tf/transform_listener.h>
#include <tf_conversions/tf_eigen.h>
#include <sensor_msgs/PointCloud2.h>

#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/io/ply_io.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl_ros/transforms.h>
#include <pcl/filters/conditional_removal.h>
#include <pcl/filters/statistical_outlier_removal.h>
#include <pcl/filters/passthrough.h>

//Definitions
//typedef pcl::PointXYZRGB PointT;
typedef pcl::PointXYZRGB PointTT;

struct PointT
{
  PCL_ADD_POINT4D;                  // preferred way of adding a XYZ+padding
  PCL_ADD_RGB;
  float l;
  float o;
  float p;
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW   // make sure our new allocators are aligned
} EIGEN_ALIGN16;                    // enforce SSE padding for correct memory alignment

POINT_CLOUD_REGISTER_POINT_STRUCT  (PointT,           // here we assume a XYZ + "test" (as fields)
                                   (float, x, x)
                                   (float, y, y)
                                   (float, z, z)
                                   (float, rgb, rgb)
                                   //(float, g, g)
                                   //(float, b, b)
                                   (float, l, l)
                                   (float, o, o)
                                   (float, p, p)
)

//typedef pcl::PointNormal PS;

//Global vars
std::string filename  = "/tmp/output.pcd";
//std::string filename2 = "/tmp/output.ply";
//std::string filename3 = "/tmp/output_plus_normals.ply";
pcl::PointCloud<PointT>::Ptr accumulated_cloud;
tf::TransformListener *p_listener;
boost::shared_ptr<ros::Publisher> pub;
//boost::shared_ptr<ros::Publisher> pub_visual;
boost::shared_ptr<ros::Publisher> pub_termica;

float lf = 0.05;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void filter_color(pcl::PointCloud<PointT>::Ptr cloud_in){

  // Try to clear white and blue points from sky, and green ones from the grass, or something close to it
  int rMax = 200;
  int rMin = 0;
  int gMax = 120;
  int gMin = 0;
  int bMax = 120;
  int bMin = 0;

  pcl::ConditionAnd<PointT>::Ptr color_cond (new pcl::ConditionAnd<PointT> ());
  color_cond->addComparison (pcl::PackedRGBComparison<PointT>::Ptr (new pcl::PackedRGBComparison<PointT> ("r", pcl::ComparisonOps::LT, rMax)));
  color_cond->addComparison (pcl::PackedRGBComparison<PointT>::Ptr (new pcl::PackedRGBComparison<PointT> ("r", pcl::ComparisonOps::GT, rMin)));
  color_cond->addComparison (pcl::PackedRGBComparison<PointT>::Ptr (new pcl::PackedRGBComparison<PointT> ("g", pcl::ComparisonOps::LT, gMax)));
  color_cond->addComparison (pcl::PackedRGBComparison<PointT>::Ptr (new pcl::PackedRGBComparison<PointT> ("g", pcl::ComparisonOps::GT, gMin)));
  color_cond->addComparison (pcl::PackedRGBComparison<PointT>::Ptr (new pcl::PackedRGBComparison<PointT> ("b", pcl::ComparisonOps::LT, bMax)));
  color_cond->addComparison (pcl::PackedRGBComparison<PointT>::Ptr (new pcl::PackedRGBComparison<PointT> ("b", pcl::ComparisonOps::GT, bMin)));

  // build the filter
  pcl::ConditionalRemoval<PointT> condrem (color_cond);
  condrem.setInputCloud (cloud_in);
  condrem.setKeepOrganized(true);

  // apply filter
  condrem.filter (*cloud_in);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void passthrough(pcl::PointCloud<PointT>::Ptr in, std::string field, float min, float max){
  pcl::PassThrough<PointT> ps;
  ps.setInputCloud(in);
  ps.setFilterFieldName(field);
  ps.setFilterLimits(min, max);

  ps.filter(*in);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void remove_outlier(pcl::PointCloud<PointT>::Ptr in, float mean, float deviation){
  pcl::StatisticalOutlierRemoval<PointT> sor;
  sor.setInputCloud(in);
  sor.setMeanK(mean);
  sor.setStddevMulThresh(deviation);
  sor.filter(*in);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void cloud_open_target(const sensor_msgs::PointCloud2ConstPtr& msg)
{
  //declare variables
  pcl::PointCloud<PointT>::Ptr cloud;
  pcl::PointCloud<PointT>::Ptr cloud_transformed;
  pcl::VoxelGrid<PointT> grid;
  sensor_msgs::PointCloud2 msg_out;

  pcl::PointCloud<PointTT> cloud_acumulada_visual;
  pcl::PointCloud<PointTT> cloud_acumulada_termica;
  sensor_msgs::PointCloud2 visual_out;
  sensor_msgs::PointCloud2 termica_out;

  //allocate objects for pointers
  cloud = (pcl::PointCloud<PointT>::Ptr) (new pcl::PointCloud<PointT>);
  cloud_transformed = (pcl::PointCloud<PointT>::Ptr) (new pcl::PointCloud<PointT>);

  //Convert the ros message to pcl point cloud
  pcl::fromROSMsg (*msg, *cloud);

  //Remove any NAN points in the cloud
  std::vector<int> indicesNAN;
  pcl::removeNaNFromPointCloud(*cloud, *cloud, indicesNAN);
  // Filter with passthrough filter -> region to see
  passthrough(cloud, "z", 1, 35);
  passthrough(cloud, "x", -8, 8);
  passthrough(cloud, "y", -8, 8);
  // Filter for color
//  filter_color(cloud);
  // Remove outiliers
  remove_outlier(cloud, 16, 1);
  // Voxel grid
  grid.setInputCloud(cloud);
  grid.setLeafSize(lf, lf, lf);
  grid.filter(*cloud);
  //Get the transform, return if cannot get it
  ros::Time tic = ros::Time::now();
  ros::Time t = msg->header.stamp;
  tf::StampedTransform trans;
  try
  {
    p_listener->waitForTransform(ros::names::remap("/map"), msg->header.frame_id, t, ros::Duration(3.0));
    p_listener->lookupTransform(ros::names::remap("/map"), msg->header.frame_id, t, trans);
  }
  catch (tf::TransformException& ex){
    ROS_ERROR("%s",ex.what());
    //ros::Duration(1.0).sleep();
    ROS_WARN("Cannot accumulate");
    return;
  }
  ROS_INFO("Collected transforms (%0.3f secs)", (ros::Time::now() - tic).toSec());


  //Transform point cloud using the transform obtained
  Eigen::Affine3d eigen_trf;
  tf::transformTFToEigen (trans, eigen_trf);
  pcl::transformPointCloud<PointT>(*cloud, *cloud_transformed, eigen_trf);

  //Accumulate the point cloud using the += operator
  ROS_INFO("Size of cloud_transformed = %ld", cloud_transformed->points.size());
  (*accumulated_cloud) += (*cloud_transformed);

  ROS_INFO("Size of accumulated_cloud = %ld", accumulated_cloud->points.size());

  // Separando as point clouds em visual e térmica
  int nPontos = int(accumulated_cloud->points.size());
  cloud_acumulada_visual.points.resize (nPontos);
  cloud_acumulada_termica.points.resize (nPontos);
  for(int i = 0; i < int(accumulated_cloud->points.size()); i++)
  {


      cloud_acumulada_visual.points[i].x = accumulated_cloud->points[i].x;
      cloud_acumulada_visual.points[i].y = accumulated_cloud->points[i].y;
      cloud_acumulada_visual.points[i].z = accumulated_cloud->points[i].z;

      cloud_acumulada_visual.points[i].r = accumulated_cloud->points[i].r;
      cloud_acumulada_visual.points[i].g = accumulated_cloud->points[i].g;
      cloud_acumulada_visual.points[i].b = accumulated_cloud->points[i].b;

      cloud_acumulada_termica.points[i].x = accumulated_cloud->points[i].x;
      cloud_acumulada_termica.points[i].y = accumulated_cloud->points[i].y;
      cloud_acumulada_termica.points[i].z = accumulated_cloud->points[i].z;

      cloud_acumulada_termica.points[i].r = accumulated_cloud->points[i].l;
      cloud_acumulada_termica.points[i].g = accumulated_cloud->points[i].o;
      cloud_acumulada_termica.points[i].b = accumulated_cloud->points[i].p;\

      if(cloud_acumulada_termica.points[i].r != -1)
      {
          cloud_acumulada_termica.points[i].r = accumulated_cloud->points[i].l;
          cloud_acumulada_termica.points[i].g = accumulated_cloud->points[i].o;
          cloud_acumulada_termica.points[i].b = accumulated_cloud->points[i].p;\
      }
      else
      {
          cloud_acumulada_termica.points[i].x = nan("");
          cloud_acumulada_termica.points[i].y = nan("");
          cloud_acumulada_termica.points[i].z = nan("");
          cloud_acumulada_termica.points[i].r = nan("");
          cloud_acumulada_termica.points[i].g = nan("");
          cloud_acumulada_termica.points[i].b = nan("");
      }

  }

  // Limpar point cloud termica de elementos NAN
  std::vector<int> indicesNAN2;
  pcl::removeNaNFromPointCloud(cloud_acumulada_termica, cloud_acumulada_termica, indicesNAN2);

  //Conver the pcl point cloud to ros msg and publish
  pcl::toROSMsg (*accumulated_cloud, msg_out);
  msg_out.header.stamp = t;
  pub->publish(msg_out);

  // Publicando point cloud visual e térmica
//  pcl::toROSMsg (cloud_acumulada_visual, visual_out);
  pcl::toROSMsg (cloud_acumulada_termica, termica_out);
  visual_out.header.stamp = t;
  visual_out.header.frame_id = ros::names::remap("/map");
  termica_out.header.stamp = t;
  termica_out.header.frame_id = ros::names::remap("/map");
//  pub_visual->publish(visual_out);
  pub_termica->publish(termica_out);

  cloud.reset();
  cloud_transformed.reset();
}


int main (int argc, char** argv)
{

  // Initialize ROS
  ros::init (argc, argv, "accumulatepointcloud");
  ros::NodeHandle nh;

  //initialize the transform listenerand wait a bit
  p_listener = (tf::TransformListener*) new tf::TransformListener;
  ros::Duration(2).sleep();

  //Initialize accumulated cloud variable
  accumulated_cloud = (pcl::PointCloud<PointT>::Ptr) new pcl::PointCloud<PointT>;
  accumulated_cloud->header.frame_id = ros::names::remap("/map");

  //Initialize the point cloud publisher
  pub = (boost::shared_ptr<ros::Publisher>) new ros::Publisher;
  *pub = nh.advertise<sensor_msgs::PointCloud2>("/accumulated_point_cloud", 1);
//  pub_visual = (boost::shared_ptr<ros::Publisher>) new ros::Publisher;
//  *pub_visual = nh.advertise<sensor_msgs::PointCloud2>("/accumulated_visual", 1);
  pub_termica = (boost::shared_ptr<ros::Publisher>) new ros::Publisher;
  *pub_termica = nh.advertise<sensor_msgs::PointCloud2>("/accumulated_termica", 1);

  // Create a ROS subscriber for the input point cloud  -- Se inscreve na point cloud gerada no pkg termica_reconstrucao
  ros::Subscriber sub_target = nh.subscribe ("/completa_pc", 10, cloud_open_target);

  //Loop infinitly
  while (ros::ok())
  {
    // Spin
    ros::spinOnce();
  }
  //Save accumulated point cloud to a file
  printf("Saving to file %s\n", filename.c_str());
  pcl::io::savePCDFileASCII (filename, *accumulated_cloud);

  return 0;

}
