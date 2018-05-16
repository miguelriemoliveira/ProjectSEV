//Includes
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
#include <pcl/filters/passthrough.h>
#include <pcl/filters/statistical_outlier_removal.h>

#include <pcl/keypoints/iss_3d.h>
#include <pcl/keypoints/sift_keypoint.h>
#include <pcl/registration/correspondence_estimation.h>
#include <pcl/registration/correspondence_rejection.h>
#include <pcl/registration/correspondence_rejection_surface_normal.h>
#include <pcl/registration/registration.h>
#include <pcl/registration/sample_consensus_prerejective.h>
#include <pcl/registration/transformation_estimation.h>
#include <pcl/registration/transformation_estimation_svd.h>
#include <pcl/registration/transformation_validation.h>
#include <pcl/features/fpfh.h>
#include <pcl/features/pfh.h>
#include <pcl/sample_consensus/ransac.h>
#include <pcl/registration/correspondence_rejection_sample_consensus.h>
#include <pcl/registration/correspondence_rejection_one_to_one.h>
#include <pcl/registration/transformation_estimation_svd.h>
#include <pcl/registration/transformation_estimation_svd_scale.h>
#include <pcl/registration/icp.h>
#include <pcl/registration/icp_nl.h>

//Definitions
typedef pcl::PointXYZRGB PointT;
typedef pcl::PFHSignature125 algorithm;

//Global vars
std::string filename  = "/tmp/output.pcd";
pcl::PointCloud<PointT>::Ptr accumulated_cloud (new pcl::PointCloud<PointT>());
pcl::PointCloud<PointT>::Ptr backup_compare (new pcl::PointCloud<PointT>()); // compare features from last cloud
//pcl::PointCloud<PointT>::Ptr second;

tf::TransformListener *p_listener;
boost::shared_ptr<ros::Publisher> pub;

bool use_icp = false; // Here to try simple icp instead of whole feature process

pcl::PointCloud<PointT>::Ptr filter_color(pcl::PointCloud<PointT>::Ptr cloud_in){

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

  return cloud_in;

}

void calculate_pointnormals(pcl::PointCloud<PointT>::Ptr cloud_in, int k, pcl::PointCloud<pcl::PointNormal>::Ptr cloud_pointnormals){

  ROS_INFO("Extraindo coordenadas...");
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_xyz_ptr (new pcl::PointCloud<pcl::PointXYZ>() ); // Gather only pose so can compute normals
  cloud_xyz_ptr->resize(cloud_in->points.size()); // allocate memory space
  for(int i=0; i < cloud_in->points.size(); i++){
    cloud_xyz_ptr->points[i].x = cloud_in->points[i].x;
    cloud_xyz_ptr->points[i].y = cloud_in->points[i].y;
    cloud_xyz_ptr->points[i].z = cloud_in->points[i].z;
  }
  ROS_INFO("Calculando normais.......");
  pcl::PointCloud<pcl::Normal>::Ptr cloud_normals (new pcl::PointCloud<pcl::Normal>);
  pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
  pcl::search::KdTree<pcl::PointXYZ>::Ptr tree_n(new pcl::search::KdTree<pcl::PointXYZ>());

  ne.setInputCloud(cloud_xyz_ptr);
  ne.setSearchMethod(tree_n);
//  ne.setRadiusSearch(radius);
  ne.setKSearch(k);
  ne.compute(*cloud_normals);
  ROS_INFO("Retornando nuvem com normais calculadas!");

  cloud_pointnormals->resize(cloud_normals->points.size());
  pcl::concatenateFields(*cloud_xyz_ptr, *cloud_normals, *cloud_pointnormals);
}

void calculate_normals(pcl::PointCloud<PointT>::Ptr cloud_in, int k, pcl::PointCloud<pcl::Normal>::Ptr cloud_normals){

//  ROS_INFO("Extraindo coordenadas...");
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_xyz_ptr (new pcl::PointCloud<pcl::PointXYZ>()); // Gather only pose so can compute normals
  cloud_xyz_ptr->resize(cloud_in->points.size()); // allocate memory space
  for(int i=0; i < cloud_in->points.size(); i++){
    cloud_xyz_ptr->points[i].x = cloud_in->points[i].x;
    cloud_xyz_ptr->points[i].y = cloud_in->points[i].y;
    cloud_xyz_ptr->points[i].z = cloud_in->points[i].z;
  }
//  ROS_INFO("Calculando normais.......");
  pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
  pcl::search::KdTree<pcl::PointXYZ>::Ptr tree_n(new pcl::search::KdTree<pcl::PointXYZ>());

  ne.setInputCloud(cloud_xyz_ptr);
  ne.setSearchMethod(tree_n);
//  ne.setRadiusSearch(radius);
  ne.setKSearch(k);
  ne.compute(*cloud_normals);
}

pcl::PointCloud<pcl::PointWithScale> calculate_sift(pcl::PointCloud<pcl::PointNormal>::Ptr normals){

  // Parameters for sift computation
  const float min_scale = 0.01f;
  const int n_octaves = 3;
  const int n_scales_per_octave = 4;
  const float min_contrast = 0.001f;
  ROS_INFO("Comecando calculo SIFT....");
  // Estimate the sift interest points using normals values from xyz as the Intensity variants
  pcl::SIFTKeypoint<pcl::PointNormal, pcl::PointWithScale> sift;
  pcl::PointCloud<pcl::PointWithScale> result;
  pcl::search::KdTree<pcl::PointNormal>::Ptr tree(new pcl::search::KdTree<pcl::PointNormal> ());
  sift.setSearchMethod(tree);
  sift.setScales(min_scale, n_octaves, n_scales_per_octave);
  sift.setMinimumContrast(min_contrast);
  sift.setInputCloud(normals);
  sift.compute(result);

//  ROS_INFO("SIFT calculado com %d pontos!", result.points.size());

  return result;

}

void calculate_sift_from_rgb(pcl::PointCloud<PointT>::Ptr cloud_in, pcl::PointCloud<pcl::PointWithScale>::Ptr kp){
  // Parameters for sift computation
  const float min_scale = 0.1f;
  const int n_octaves = 3;
  const int n_scales_per_octave = 4;
  const float min_contrast = 0.01f;
  ROS_INFO("Comecando calculo SIFT....");
  // Estimate the sift interest points using normals values from xyz as the Intensity variants
  pcl::SIFTKeypoint<PointT, pcl::PointWithScale> sift;
  pcl::PointCloud<pcl::PointWithScale> result;
  pcl::search::KdTree<PointT>::Ptr tree(new pcl::search::KdTree<PointT> ());
  sift.setSearchMethod(tree);
  sift.setScales(min_scale, n_octaves, n_scales_per_octave);
  sift.setMinimumContrast(min_contrast);
  sift.setInputCloud(cloud_in);
  sift.compute(result);

  *kp = result;

  ROS_INFO("Pontos do descritor: [%d]", kp->points.size());
}

pcl::PointCloud<pcl::PointXYZ>::Ptr calculate_keypoints(pcl::PointCloud<PointT>::Ptr cloud_in){

  pcl::search::KdTree<pcl::PointXYZ>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZ>());
  pcl::ISSKeypoint3D<pcl::PointXYZ, pcl::PointXYZ> iss;
  pcl::PointCloud<pcl::PointXYZ>::Ptr target_keypoints(new pcl::PointCloud<pcl::PointXYZ>());

  iss.setSearchMethod(tree);
  //iss.setSalientRadius(10 * model_resolution_1);
  //iss.setNonMaxRadius(8 * model_resolution_1);
  iss.setThreshold21(0.2);
  iss.setThreshold32(0.2);
  iss.setMinNeighbors(10);
  iss.setNumberOfThreads(10);

  pcl::PointCloud<pcl::PointXYZ>::Ptr temp(new pcl::PointCloud<pcl::PointXYZ>());
  temp->resize(cloud_in->points.size());
  for(int i=0; i<temp->size(); i++){
    temp->points[i].x = cloud_in->points[i].x;
    temp->points[i].y = cloud_in->points[i].y;
    temp->points[i].z = cloud_in->points[i].z;
  }

  iss.setInputCloud(temp);
  iss.compute((*target_keypoints));

  return target_keypoints;

}

void calculate_descriptors(pcl::PointCloud<PointT>::Ptr cloud_in, pcl::PointCloud<pcl::Normal>::Ptr normals, pcl::PointCloud<pcl::PointWithScale>::Ptr kp, int k, pcl::PointCloud<algorithm>::Ptr descriptors){

  pcl::PFHEstimation<PointT, pcl::Normal, algorithm> pfh_est;

  pcl::search::KdTree<PointT>::Ptr tree (new pcl::search::KdTree<PointT> ());
  pfh_est.setSearchMethod(tree);
  pfh_est.setKSearch(k);

//  ROS_INFO("Extraindo features...");
  pcl::PointCloud<PointT>::Ptr kp_rgb(new pcl::PointCloud<PointT>());
  // Copy data to retain the keypoints
  pcl::copyPointCloud(*kp, *kp_rgb);
  // we set the surface to be searched as the whole cloud, but only search at the kp defines
  pfh_est.setSearchSurface(cloud_in);
  pfh_est.setInputNormals(normals);
  pfh_est.setInputCloud(kp_rgb);
  // Compute results
  pfh_est.compute(*descriptors);

}

Eigen::Matrix4f tf_from_correspondences_between_ptclouds(pcl::PointCloud<algorithm>::Ptr src,
                                                         pcl::PointCloud<algorithm>::Ptr tgt,
                                                         pcl::PointCloud<pcl::PointWithScale>::Ptr src_kp,
                                                         pcl::PointCloud<pcl::PointWithScale>::Ptr tgt_kp)
{
  // Calculating correspondences
  pcl::registration::CorrespondenceEstimation<algorithm, algorithm> est;
  pcl::CorrespondencesPtr cor(new pcl::Correspondences());

  est.setInputSource(src);
  est.setInputTarget(tgt);
  est.determineCorrespondences(*cor);
  ROS_INFO("Obtendo melhor transformacao...");
  // Reject duplicates
  pcl::CorrespondencesPtr cor_no_dup (new pcl::Correspondences());
  pcl::registration::CorrespondenceRejectorOneToOne rej;

  rej.setInputCorrespondences(cor);
  rej.getCorrespondences(*cor_no_dup);

  // Rejecting outliers via ransac algorithm
  pcl::registration::CorrespondenceRejectorSampleConsensus<pcl::PointWithScale> corr_rej_sac;
  pcl::CorrespondencesPtr cor_ransac(new pcl::Correspondences());
  corr_rej_sac.setInputSource(src_kp);
  corr_rej_sac.setInputTarget(tgt_kp);
  corr_rej_sac.setInlierThreshold(1.5);
  corr_rej_sac.setMaximumIterations(100);
  corr_rej_sac.setRefineModel(false);
  corr_rej_sac.setInputCorrespondences(cor_no_dup);
  corr_rej_sac.getCorrespondences(*cor_ransac);
  Eigen::Matrix4f transf;
//  transf = corr_rej_sac.getBestTransformation();

  pcl::registration::TransformationEstimationSVD<pcl::PointWithScale, pcl::PointWithScale> trans_est;
  trans_est.estimateRigidTransformation(*src_kp, *tgt_kp, *cor_ransac, transf);

  ROS_INFO("Melhor transformacao obtida...");
  return transf;

}

pcl::PointCloud<pcl::PointXYZ>::Ptr simplify_cloud_xyz(pcl::PointCloud<PointT>::Ptr cloud_in){

  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_out (new pcl::PointCloud<pcl::PointXYZ>());
  cloud_out->resize(cloud_in->size());
  for(int i=0; i<cloud_in->points.size(); i++){
    cloud_out->points[i].x = cloud_in->points[i].x;
    cloud_out->points[i].y = cloud_in->points[i].y;
    cloud_out->points[i].z = cloud_in->points[i].z;
  }
  return cloud_out;

}

void passthrough(pcl::PointCloud<PointT>::Ptr in, std::string field, float min, float max){
  pcl::PassThrough<PointT> ps;
  ps.setInputCloud(in);
  ps.setFilterFieldName(field);
  ps.setFilterLimits(min, max);

  ps.filter(*in);
}

void remove_outlier(pcl::PointCloud<PointT>::Ptr in, float mean, float deviation){
  pcl::StatisticalOutlierRemoval<PointT> sor;
  sor.setInputCloud(in);
  sor.setMeanK(mean);
  sor.setStddevMulThresh(deviation);
  sor.filter(*in);
}

void cloud_open_target(const sensor_msgs::PointCloud2ConstPtr& msg)
{
  //declare variables
  pcl::PointCloud<PointT>::Ptr cloud (new pcl::PointCloud<PointT>());
  pcl::PointCloud<PointT>::Ptr cloud_transformed (new pcl::PointCloud<PointT>());
  pcl::PointCloud<PointT>::Ptr cloud_adjust (new pcl::PointCloud<PointT>());
  pcl::PointCloud<PointT>::Ptr tmp_cloud (new pcl::PointCloud<PointT>());
  pcl::VoxelGrid<PointT> grid;
  sensor_msgs::PointCloud2 msg_out;

  //Convert the ros message to pcl point cloud
  pcl::fromROSMsg (*msg, *cloud);

  //Remove any NAN points in the cloud
  std::vector<int> indicesNAN;
  pcl::removeNaNFromPointCloud(*cloud, *cloud, indicesNAN);

  // Filter for outliers
  remove_outlier(cloud, 30, 0.1);

  // Filter for color
//  cloud = filter_color(cloud);

  // Filter for region - PassThrough
//  passthrough(cloud, "z",  0, 30);
//  passthrough(cloud, "x", -3,  3);
//  passthrough(cloud, "y", -4,  4);

  //Get the transform, return if cannot get it
//  ros::Time tic = ros::Time::now();
  ros::Time t = msg->header.stamp;
//  tf::StampedTransform trans;
//  try
//  {
//    p_listener->waitForTransform(ros::names::remap("/map"), msg->header.frame_id, t, ros::Duration(3.0));
//    p_listener->lookupTransform(ros::names::remap("/map"), msg->header.frame_id, t, trans);
//  }
//  catch (tf::TransformException& ex){
//    ROS_ERROR("%s",ex.what());
//    //ros::Duration(1.0).sleep();
//    ROS_WARN("Cannot accumulate");
//    return;
//  }
//  ROS_INFO("Collected transforms (%0.3f secs)", (ros::Time::now() - tic).toSec());


//  Transform point cloud using the transform obtained
//  Eigen::Affine3d eigen_trf;
//  tf::transformTFToEigen (trans, eigen_trf);
//  pcl::transformPointCloud<PointT>(*cloud, *cloud_transformed, eigen_trf);
//  Eigen::Matrix4f temp_trans = eigen_trf.cast<float>().matrix();

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /// From here on we have the pipeline to accumulate from cloud registrarion using features obtained, so we have better result ///
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  if(accumulated_cloud->points.size() < 3)
  { // Here we have no cloud yet, first round
//    (*accumulated_cloud) += (*cloud_transformed);
//    backup_compare = cloud_transformed;
    *tmp_cloud = *cloud;
    grid.setInputCloud(tmp_cloud);
    grid.setLeafSize(0.2f, 0.2f, 0.2f);
    grid.filter(*tmp_cloud);
    (*accumulated_cloud) = (*tmp_cloud);
    *backup_compare = *cloud;
  } else { // Now the actual pipeline
    if(use_icp){ // Try to use icp
      // Calculate transform after alignment
      pcl::IterativeClosestPointNonLinear<PointT, PointT> icp;
      icp.setMaximumIterations(20); // So it doesnt take too long
      icp.setMaxCorrespondenceDistance(4); // So it doesnt go looking all around
      icp.setRANSACOutlierRejectionThreshold(10);
      icp.setTransformationEpsilon(1e-5);
      icp.setInputSource(cloud);
      icp.setInputTarget(backup_compare);
      // Final result, passing eigen_trf as first guess, which seems to be actually close enough
      pcl::PointCloud<PointT> cloud_aligned;
      icp.align(cloud_aligned);
      Eigen::Matrix4f transf_icp = icp.getFinalTransformation();
      // Transform the source cloud so we add afterwards and renew backup
      if(icp.hasConverged()){
        ROS_INFO("!!!!!!!!!! ALINHOU E CONVERGIU !!!!!!!!!!");
        pcl::transformPointCloud<PointT>(*cloud, *cloud_adjust, transf_icp);
      } else {
        cloud_adjust = backup_compare;
      }
      (*accumulated_cloud) += (*cloud_adjust);
      *backup_compare = *cloud_adjust;
    } else {
      // Normals
      pcl::PointCloud<pcl::Normal>::Ptr src_nor(new pcl::PointCloud<pcl::Normal>());
      pcl::PointCloud<pcl::Normal>::Ptr tgt_nor(new pcl::PointCloud<pcl::Normal>());
      calculate_normals(cloud, 4, src_nor);
      calculate_normals(backup_compare, 4, tgt_nor);
      // Keypoints
      pcl::PointCloud<pcl::PointWithScale>::Ptr src_kp_sift (new pcl::PointCloud<pcl::PointWithScale>());
      pcl::PointCloud<pcl::PointWithScale>::Ptr tgt_kp_sift (new pcl::PointCloud<pcl::PointWithScale>());
      calculate_sift_from_rgb(cloud, src_kp_sift);
      calculate_sift_from_rgb(backup_compare, tgt_kp_sift);
      // Features (descriptors)
//      ROS_INFO("!!!!!!!!!! Comecando a tirar features !!!!!!!!!!");
      pcl::PointCloud<algorithm>::Ptr src_features (new pcl::PointCloud<algorithm>());
      pcl::PointCloud<algorithm>::Ptr tgt_features (new pcl::PointCloud<algorithm>());
      calculate_descriptors(cloud, src_nor, src_kp_sift, 20, src_features);
      calculate_descriptors(backup_compare, tgt_nor, tgt_kp_sift, 20, tgt_features);
//      ROS_INFO("!!!!!!!!!! Tirou features. Obtendo transformacao !!!!!!!!!!");
      // Compare features and transform cloud
      Eigen::Matrix4f transf = tf_from_correspondences_between_ptclouds(src_features, tgt_features, src_kp_sift, tgt_kp_sift);
//      // Try to use first estimation in ICP algorithm
//      pcl::IterativeClosestPoint<PointT, PointT> icp;
//      icp.setMaximumIterations(20); // So it doesnt take too long
//      icp.setMaxCorrespondenceDistance(4); // So it doesnt go looking all around
//      icp.setRANSACOutlierRejectionThreshold(10);
//      icp.setTransformationEpsilon(1e-5);
//      icp.setInputSource(cloud);
//      icp.setInputTarget(backup_compare);
//      // Final result, passing eigen_trf as first guess, which seems to be actually close enough
//      pcl::PointCloud<PointT> cloud_aligned;
//      icp.align(cloud_aligned, transf);
//      Eigen::Matrix4f transf_icp = icp.getFinalTransformation();
      // Transform cloud, accumulate it and renew backup
      pcl::transformPointCloud<PointT>(*cloud, *cloud_adjust, transf);
      *tmp_cloud = *cloud_adjust;
      grid.setInputCloud(tmp_cloud);
      grid.setLeafSize(0.2f, 0.2f, 0.2f);
      grid.filter(*tmp_cloud);
      (*accumulated_cloud) += (*tmp_cloud);
      *backup_compare = *cloud_adjust;
//      for(int i=0; i<accumulated_cloud->points.size(); i++){
//        accumulated_cloud->points[i].r = 255;
//        accumulated_cloud->points[i].g = 255;
//        accumulated_cloud->points[i].b = 255;
//      }
//      ROS_INFO("!!!!!!!!!! Chamar nova iteracao !!!!!!!!!!");
    }
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //Filter the whole accumulated cloud with voxel grid
//  *tmp_cloud = *accumulated_cloud;
//  grid.setInputCloud(tmp_cloud);
//  grid.setLeafSize(0.05f, 0.05f, 0.05f);
//  grid.filter(*accumulated_cloud);
  //Accumulate the point cloud using the += operator
  ROS_INFO("Size of cloud_transformed = %ld", cloud->points.size());
//  ROS_INFO("Size of accumulated_cloud = %ld", accumulated_cloud->points.size());
//  (*accumulated_cloud) += (*cloud_transformed);

  //Conver the pcl point cloud to ros msg and publish
  pcl::toROSMsg (*accumulated_cloud, msg_out);
//  pcl::toROSMsg (*accumulated_cloud, msg_out);
  msg_out.header.stamp = t;
  pub->publish(msg_out);

  cloud.reset();
  tmp_cloud.reset();
  cloud_transformed.reset();
  cloud_adjust.reset();
}


int main (int argc, char** argv)
{

  // Initialize ROS
  ros::init (argc, argv, "accumulatepointcloud");
  ros::NodeHandle nh;

  //initialize the transform listenerand wait a bit
//  p_listener = (tf::TransformListener*) new tf::TransformListener;
//  ros::Duration(2).sleep();

  //Initialize accumulated cloud variable
  accumulated_cloud = (pcl::PointCloud<PointT>::Ptr) new pcl::PointCloud<PointT>();
  accumulated_cloud->header.frame_id = ros::names::remap("/map");

  //Initialize the point cloud publisher
  pub = (boost::shared_ptr<ros::Publisher>) new ros::Publisher;
  *pub = nh.advertise<sensor_msgs::PointCloud2>("/accumulated_point_cloud", 100);

  // Create a ROS subscriber for the input point cloud
  ros::Subscriber sub_target = nh.subscribe ("input", 500, cloud_open_target);

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