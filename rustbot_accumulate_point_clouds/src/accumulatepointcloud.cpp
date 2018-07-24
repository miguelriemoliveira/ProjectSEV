//Includes
#define PCL_NO_PRECOMPILE
#include <cmath>
#include <ros/ros.h>
#include <tf/transform_listener.h>
#include <tf_conversions/tf_eigen.h>
#include <tf/tf.h>

#include <sensor_msgs/PointCloud2.h>
#include <nav_msgs/Odometry.h>

#include <mavros/mavros.h>
#include <mavros_msgs/Mavlink.h>
#include <mavros_msgs/mavlink_convert.h>
#include <mavlink/config.h>
#include <mavros/utils.h>
#include <mavros/mavros.h>
#include <mavros_msgs/mavlink_convert.h>
#include <mavros_msgs/VFR_HUD.h>
#include <mavros_msgs/GlobalPositionTarget.h>

#include <dynamixel_workbench_toolbox/dynamixel_multi_driver.h>
#include <dynamixel_workbench_msgs/DynamixelStateList.h>
#include <dynamixel_workbench_msgs/DynamixelState.h>

#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl_ros/transforms.h>
#include <pcl/filters/conditional_removal.h>
#include <pcl/filters/statistical_outlier_removal.h>
#include <pcl/filters/passthrough.h>

#include <Eigen/Geometry>
#include <Eigen/Dense>
#include <Eigen/Core>

#include <message_filters/subscriber.h>
#include <message_filters/time_synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>

/// Namespaces
using namespace pcl;
using namespace std;
using namespace message_filters;
using namespace nav_msgs;
using namespace mavros_msgs;
using namespace dynamixel_workbench_msgs;

/// Definitions
typedef PointXYZRGB PointTT;
typedef sync_policies::ApproximateTime<sensor_msgs::PointCloud2, Odometry, VFR_HUD, GlobalPositionTarget, Odometry> syncPolicy;
//typedef sync_policies::ApproximateTime<DynamixelState, DynamixelState> syncPolicyDyn;

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

/// Global vars
PointCloud<PointTT>::Ptr accumulated_cloud;
tf::TransformListener *p_listener;
boost::shared_ptr<ros::Publisher> pub;
boost::shared_ptr<ros::Publisher> pub_termica;
// Motors
float pan_front, pan_current , pwm2pan ; // YAW   [PWM, PWM, RAD/PWM]
float tilt_hor , tilt_current, pwm2tilt; // PITCH [PWM, PWM, RAD/PWM]
bool  first_read_pan, first_read_tilt;
// Mavros
bool   first_read_mavros;
double angle_offset_yaw; // [RAD]
double east_offset, north_offset; // [m]

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void filter_color(PointCloud<PointTT>::Ptr cloud_in){

  // Try to clear white and blue points from sky, and green ones from the grass, or something close to it
  int rMax = 210;
  int rMin = 0;
  int gMax = 210;
  int gMin = 0;
  int bMax = 210;
  int bMin = 0;

  ConditionAnd<PointTT>::Ptr color_cond (new ConditionAnd<PointTT> ());
  color_cond->addComparison (PackedRGBComparison<PointTT>::Ptr (new PackedRGBComparison<PointTT> ("r", ComparisonOps::LT, rMax)));
  color_cond->addComparison (PackedRGBComparison<PointTT>::Ptr (new PackedRGBComparison<PointTT> ("r", ComparisonOps::GT, rMin)));
  color_cond->addComparison (PackedRGBComparison<PointTT>::Ptr (new PackedRGBComparison<PointTT> ("g", ComparisonOps::LT, gMax)));
  color_cond->addComparison (PackedRGBComparison<PointTT>::Ptr (new PackedRGBComparison<PointTT> ("g", ComparisonOps::GT, gMin)));
  color_cond->addComparison (PackedRGBComparison<PointTT>::Ptr (new PackedRGBComparison<PointTT> ("b", ComparisonOps::LT, bMax)));
  color_cond->addComparison (PackedRGBComparison<PointTT>::Ptr (new PackedRGBComparison<PointTT> ("b", ComparisonOps::GT, bMin)));

  // build the filter
  ConditionalRemoval<PointTT> condrem (color_cond);
  condrem.setInputCloud (cloud_in);
  condrem.setKeepOrganized(true);

  // apply filter
  condrem.filter (*cloud_in);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void passthrough(PointCloud<PointTT>::Ptr in, std::string field, float min, float max){
  PassThrough<PointTT> ps;
  ps.setInputCloud(in);
  ps.setFilterFieldName(field);
  ps.setFilterLimits(min, max);

  ps.filter(*in);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void remove_outlier(PointCloud<PointTT>::Ptr in, float mean, float deviation){
  StatisticalOutlierRemoval<PointTT> sor;
  sor.setInputCloud(in);
  sor.setMeanK(mean);
  sor.setStddevMulThresh(deviation);
  sor.filter(*in);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void filter_grid(PointCloud<PointTT>::Ptr in, float lf){
  VoxelGrid<PointTT> grid;
  grid.setInputCloud(in);
  grid.setLeafSize(lf, lf, lf);
  grid.filter(*in);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void cloud_open_target2(const sensor_msgs::PointCloud2ConstPtr& msg_ptc, const OdometryConstPtr& msg_odo,
                        const VFR_HUDConstPtr& msg_ang,
                        const GlobalPositionTargetConstPtr& mag_enu,
                        const OdometryConstPtr& msg_dyn){
  // Declare variables
  PointCloud<PointTT>::Ptr cloud (new PointCloud<PointTT>());
  PointCloud<PointTT>::Ptr cloud_transformed (new PointCloud<PointTT>());
  sensor_msgs::PointCloud2 msg_out;

  PointCloud<PointTT> cloud_acumulada_termica;
  sensor_msgs::PointCloud2 termica_out;

  // Convert the ros message to pcl point cloud
  fromROSMsg (*msg_ptc, *cloud);

  // Remove any NAN points in the cloud
  vector<int> indicesNAN;
  removeNaNFromPointCloud(*cloud, *cloud, indicesNAN);
  // Voxel Grid
  float lf = 0.12f;
  filter_grid(cloud, lf);
  // Filter with passthrough filter -> region to see
  passthrough(cloud, "z",   1, 20);
  passthrough(cloud, "x", -10, 10);
  passthrough(cloud, "y", -10, 10);
  // Filter for color
  filter_color(cloud);
  // Remove outiliers
  remove_outlier(cloud, 15, 0.5);

  /// Obter a odometria da mensagem
  // Rotacao
  Eigen::Quaternion<double> q;
  q.x() = (double)msg_odo->pose.pose.orientation.x;
  q.y() = (double)msg_odo->pose.pose.orientation.y;
  q.z() = (double)msg_odo->pose.pose.orientation.z;
  q.w() = (double)msg_odo->pose.pose.orientation.w;
  // Translacao
  Eigen::Vector3d offset(msg_odo->pose.pose.position.x, msg_odo->pose.pose.position.y, msg_odo->pose.pose.position.z);
  // Print para averiguar
  if(true){
    Eigen::Matrix<double, 3, 1> euler;
    euler = q.toRotationMatrix().eulerAngles(0, 1, 2);
    cout << "Roll: " << RAD2DEG(euler[0]) << "\tPitch: " << RAD2DEG(euler[1]) << "\tYaw: " << RAD2DEG(euler[2]) << endl;
    cout << "X   : " << offset(0)         << "\tY    : " << offset(1)         << "\tZ  : " << offset(2)         << endl;
  }

  /// Obter pose das mensagens
  if(first_read_mavros){

  } else {

  }

  // Transformar a nuvem
  transformPointCloud<PointTT>(*cloud, *cloud_transformed, offset, q);

  // Accumulate the point cloud using the += operator
//  ROS_INFO("Size of cloud_transformed = %ld", cloud_transformed->points.size());
  (*accumulated_cloud) += (*cloud_transformed);

//  ROS_INFO("Size of accumulated_cloud = %ld", accumulated_cloud->points.size());

//  // Separando as point clouds em visual e térmica
//  int nPontos = int(accumulated_cloud->points.size());
//  cloud_acumulada_termica.points.resize (nPontos);
//  for(int i = 0; i < int(accumulated_cloud->points.size()); i++)
//  {

//      cloud_acumulada_termica.points[i].x = accumulated_cloud->points[i].x;
//      cloud_acumulada_termica.points[i].y = accumulated_cloud->points[i].y;
//      cloud_acumulada_termica.points[i].z = accumulated_cloud->points[i].z;

//      cloud_acumulada_termica.points[i].r = accumulated_cloud->points[i].l;
//      cloud_acumulada_termica.points[i].g = accumulated_cloud->points[i].o;
//      cloud_acumulada_termica.points[i].b = accumulated_cloud->points[i].p;\

//      if(cloud_acumulada_termica.points[i].r != -1)
//      {
//          cloud_acumulada_termica.points[i].r = accumulated_cloud->points[i].l;
//          cloud_acumulada_termica.points[i].g = accumulated_cloud->points[i].o;
//          cloud_acumulada_termica.points[i].b = accumulated_cloud->points[i].p;\
//      }
//      else
//      {
//          cloud_acumulada_termica.points[i].x = nan("");
//          cloud_acumulada_termica.points[i].y = nan("");
//          cloud_acumulada_termica.points[i].z = nan("");
//          cloud_acumulada_termica.points[i].r = nan("");
//          cloud_acumulada_termica.points[i].g = nan("");
//          cloud_acumulada_termica.points[i].b = nan("");
//      }

//  }

//  // Limpar point cloud termica de elementos NAN
//  vector<int> indicesNAN2;
//  removeNaNFromPointCloud(cloud_acumulada_termica, cloud_acumulada_termica, indicesNAN2);

  // Convert the pcl point cloud to ros msg and publish
  toROSMsg(*accumulated_cloud, msg_out);
  msg_out.header.stamp = msg_ptc->header.stamp;
  pub->publish(msg_out);

  // Publicando point cloud térmica
  toROSMsg (cloud_acumulada_termica, termica_out);
  termica_out.header.stamp = msg_ptc->header.stamp;
  termica_out.header.frame_id = ros::names::remap("/odom");
  pub_termica->publish(termica_out);

  cloud.reset();
  cloud_transformed.reset();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void pan_cb(const DynamixelStateConstPtr& msg_pan){
  if(first_read_pan){
    pan_front   = msg_pan->present_position;
    pan_current = msg_pan->present_position;
    first_read_pan = false;
  } else {
    pan_current = msg_pan->present_position;
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void tilt_cb(const DynamixelStateConstPtr& msg_tilt){
  if(first_read_tilt){
    tilt_hor     = msg_tilt->present_position;
    tilt_current = msg_tilt->present_position;
    first_read_tilt = false;
  } else {
    tilt_current = msg_tilt->present_position;
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main (int argc, char** argv)
{
  // Initialize ROS
  ros::init (argc, argv, "accumulatepointcloud");
  ros::NodeHandle nh;

  // It will be the first reading for motors when the subscribers start
  first_read_pan = true; first_read_tilt = true;
  // First time too for all position data from mavros
  first_read_mavros = true;

  //Initialize accumulated cloud variable
  accumulated_cloud = (PointCloud<PointTT>::Ptr) new PointCloud<PointTT>;
  accumulated_cloud->header.frame_id = ros::names::remap("/odom");

  //Initialize the point cloud publisher
  pub = (boost::shared_ptr<ros::Publisher>) new ros::Publisher;
  *pub = nh.advertise<sensor_msgs::PointCloud2>("/accumulated_point_cloud", 1);
  pub_termica = (boost::shared_ptr<ros::Publisher>) new ros::Publisher;
  *pub_termica = nh.advertise<sensor_msgs::PointCloud2>("/accumulated_termica", 1);

  // Subscribers para estados do motor
  ros::Subscriber sub_dynpan  = nh.subscribe("/multi_port/pan_state" , 100, pan_cb );
  ros::Subscriber sub_dyntilt = nh.subscribe("/multi_port/tilt_state", 100, tilt_cb);
//  message_filters::Subscriber<DynamixelState> subpan(nh, "/stereo/points2"          , 100);
//  message_filters::Subscriber<DynamixelState>                 subtil(nh, "/stereo_odometer/odometry", 100);
//  Synchronizer<syncPolicyDyn> syncdyn(syncPolicyDyn(100), subpan, subtil);
//  syncdyn.registerCallback(boost::bind(&dynamixel_state, _1, _2));

  // Subscriber para a nuvem instantanea e odometria
  message_filters::Subscriber<sensor_msgs::PointCloud2> subptc(nh, "/stereo/points2"          , 100);
  message_filters::Subscriber<Odometry>                 subodo(nh, "/stereo_odometer/odometry", 100);
  message_filters::Subscriber<VFR_HUD>                  subang(nh, "/dados_mavros"            , 100);
  message_filters::Subscriber<GlobalPositionTarget>     subenu(nh, "/dados_mavros"            , 100);
  message_filters::Subscriber<Odometry>                 subdyn(nh, "/dados_dynamixel"         , 100);
  // Sincroniza as leituras dos topicos (sensores e imagem a principio) em um so callback
  Synchronizer<syncPolicy> sync(syncPolicy(100), subptc, subodo, subang, subenu, subdyn);
  sync.registerCallback(boost::bind(&cloud_open_target2, _1, _2, _3, _4, _5));

  //Loop infinitly
  ros::spin();

  return 0;

}
