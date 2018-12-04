/*
 *  Copyright (c) 2018, Nagoya University
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name of Autoware nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ********************/

// headers in local directory
#include "object_map_utils.hpp"
#include "costmap_generator.h"


// Constructor
CostmapGenerator::CostmapGenerator() :
  use_wayarea_(true),
  use_waypoint_(true),
  use_objects_(false),
  has_subscribed_wayarea_(false),
  private_nh_("~"),
  SENSOR_POINTS_COSTMAP_LAYER_("sensor_points"),
  OBJECTS_COSTMAP_LAYER_("objects"),
  VECTORMAP_COSTMAP_LAYER_("vectormap"),
  WAYPOINTS_COSTMAP_LAYER_("waypoints"),
  COMBINED_COSTMAP_LAYER_("costmap")
{
  // const std::string SENSOR_POINTS_COSTMAP_LAYER_ = "sensor_points_cost";
}

CostmapGenerator::~CostmapGenerator() {}

void CostmapGenerator::init()
{
  //TODO sample sompo rosbga has weird velodyne coordinate. so be carefult about default grid length and position
  private_nh_.param<std::string>("velodyne_frame", velodyne_frame_, "velodyne");
  private_nh_.param<std::string>("map_frame", map_frame_, "map");
  private_nh_.param<double>("grid_min_value", grid_min_value_, 0.0);
  private_nh_.param<double>("grid_max_value", grid_max_value_, 3.0);
  private_nh_.param<double>("grid_resolution", grid_resolution_, 0.2);
  private_nh_.param<double>("grid_length_x", grid_length_x_, 30);
  private_nh_.param<double>("grid_length_y", grid_length_y_, 50);
  private_nh_.param<double>("grid_position_x", grid_position_x_, 0);
  private_nh_.param<double>("grid_position_x", grid_position_y_, 20);
  private_nh_.param<double>("maximum_sensor_points_height_thres", maximum_sensor_points_height_thres_, 0.3);

  initGridmap();
}

void CostmapGenerator::run()
{
  pub_costmap_ = nh_.advertise<grid_map_msgs::GridMap>("/semantics/costmap", 1);
  pub_sensor_points_cost_cloud_ = nh_.advertise<sensor_msgs::PointCloud2>("/semantics/costmap_generator/sensor_points_cost_cloud", 1);
  pub_vectormap_cost_cloud_ = nh_.advertise<sensor_msgs::PointCloud2>("/semantics/costmap_generator/vectormap_cost_cloud", 1);
  pub_combined_cost_cloud_ = nh_.advertise<sensor_msgs::PointCloud2>("/semantics/costmap_generator/combined_cost_cloud", 1);

  if(use_waypoint_)
  {
    sub_waypoint_ = nh_.subscribe("/based_waypoints", 1, &CostmapGenerator::waypointsCallback, this);
  }

  if(!use_objects_)
  {
    sub_points_ = nh_.subscribe("/points_no_ground", 1, &CostmapGenerator::sensorPointsCallback, this);
  }
  // else
  // {
  //   registerSyncedSubscriber();
  // }
}

// void CostmapGenerator::syncedCallback(const sensor_msgs::PointCloud2::ConstPtr& in_sensor_points,
//                                 const autoware_msgs::DetectedObjectArray::ConstPtr& in_objects)
// {
//   // if(checkSubscripton())
//   // generateSensorPointsCostmap()
//   // generateObjectsCostmap()
//   // generateVectormapCostmap()
//   // generateWaypointCostmap()
//   // generateCombinedCostmap()
//   // pub(costmap)
// }

void CostmapGenerator::objectsCallback(const autoware_msgs::DetectedObjectArray::ConstPtr& in_objects)
{
  costmap_ = generateObjectsCostmap(in_objects);
  costmap_ = generateVectormapCostmap();
  costmap_ = generateCombinedCostmap();

  publishRosMsg(costmap_);
}

void CostmapGenerator::sensorPointsCallback(const sensor_msgs::PointCloud2::ConstPtr& in_sensor_points_msg)
{
  // if(checkSubscripton())
  costmap_ = generateSensorPointsCostmap(in_sensor_points_msg);
  costmap_ = generateVectormapCostmap();
  costmap_ = generateCombinedCostmap();

  publishRosMsg(costmap_);
}

void CostmapGenerator::waypointsCallback(const autoware_msgs::LaneArray::ConstPtr& in_waypoint)
{
  // costmap_ = generateWaypointsCostmap();
  // costmap_ = generateCombinedCostmap();
  // publishRosMsg(costmap_);

}

// void CostmapGenerator::registerSyncedSubscriber()
// {
//   sub_sync_points_ptr_.reset(
//     new message_filters::Subscriber<sensor_msgs::PointCloud2>(nh_, "/points_no_ground", 1));
//   sub_sync_objects_ptr_.reset(
//     new message_filters::Subscriber<autoware_msgs::DetectedObjectArray>(nh_, "/detection/lidar_tracker/objects", 1));
//   sync_ptr_.reset(
//     new message_filters::TimeSynchronizer<sensor_msgs::PointCloud2,
//     autoware_msgs::DetectedObjectArray>
//     (*sub_sync_points_ptr_, *sub_sync_objects_ptr_, 10));
//
//   sync_ptr_->registerCallback(boost::bind(&CostmapGenerator::syncedCallback, this, _1, _2));
// }

void CostmapGenerator::initGridmap()
{
  costmap_.setFrameId(velodyne_frame_);
  costmap_.setGeometry(grid_map::Length(grid_length_x_, grid_length_y_),
                  grid_resolution_,
                  grid_map::Position(grid_position_x_, grid_position_y_));

  costmap_.add(SENSOR_POINTS_COSTMAP_LAYER_, 0);
  costmap_.add(VECTORMAP_COSTMAP_LAYER_, 0);
  costmap_.add("waypoint_cost", 0);
  costmap_.add("objects_cost", 0);
  costmap_.add(COMBINED_COSTMAP_LAYER_, 0);
}

grid_map::GridMap CostmapGenerator::generateSensorPointsCostmap(const sensor_msgs::PointCloud2::ConstPtr& in_sensor_points_msg)
{
  // TODO: check tf coordinate. If assuming that make costmap in velodyne coordinate, it is not necessary
  // TODO: rename makeCostmapFromSensorPoints
  grid_map::GridMap sensor_points_costmap =
                        points2costmap_.makeSensorPointsCostmap(maximum_sensor_points_height_thres_, costmap_,
                                                                SENSOR_POINTS_COSTMAP_LAYER_, in_sensor_points_msg);
  return sensor_points_costmap;
}

grid_map::GridMap CostmapGenerator::generateObjectsCostmap(const autoware_msgs::DetectedObjectArray::ConstPtr& in_objects)
{
  // TODO: check tf coordinate. If assuming that make costmap in velodyne coordinate, it is not necessary
  grid_map::GridMap objects_costmap =
                        objects2costmap_.makeCostmapFromObjects(costmap_, OBJECTS_COSTMAP_LAYER_, in_objects);
  return objects_costmap;
}

grid_map::GridMap CostmapGenerator::generateWaypointsCostmap(const autoware_msgs::LaneArray::ConstPtr& in_waypoints)
{
  grid_map::GridMap waypoints_costmap =
                        waypoints2costmap_.makeCostmapFromWaypoints(costmap_, WAYPOINTS_COSTMAP_LAYER_, in_waypoints);
  return waypoints_costmap;
}


// only this funstion depends on object_map_utils
grid_map::GridMap CostmapGenerator::generateVectormapCostmap()
{
  grid_map::GridMap vectormap_costmap = costmap_;
  if(use_wayarea_)
  {
    if(!has_subscribed_wayarea_)
    {
      object_map::LoadRoadAreasFromVectorMap(private_nh_, area_points_);
    }
    // std::cout << area_points.empty() << std::endl;
    if(!area_points_.empty())
    {
      has_subscribed_wayarea_ = true;
      object_map::FillPolygonAreas(vectormap_costmap, area_points_, VECTORMAP_COSTMAP_LAYER_,
                        grid_max_value_, grid_min_value_, grid_min_value_,
                       grid_max_value_, velodyne_frame_, map_frame_,
                       tf_listener_);
    }
  }
  return vectormap_costmap;
}

grid_map::GridMap CostmapGenerator::generateCombinedCostmap()
{
  grid_map::GridMap combined_costmap = costmap_;
  combined_costmap[COMBINED_COSTMAP_LAYER_] = costmap_[SENSOR_POINTS_COSTMAP_LAYER_].cwiseMax(
                                                                  costmap_[VECTORMAP_COSTMAP_LAYER_]);
  return combined_costmap;
}

void CostmapGenerator::publishRosMsg(const grid_map::GridMap& costmap)
{
  sensor_msgs::PointCloud2 out_sensor_points_cost_cloud_msg;
  grid_map::GridMapRosConverter::toPointCloud(costmap, SENSOR_POINTS_COSTMAP_LAYER_, out_sensor_points_cost_cloud_msg);
  pub_sensor_points_cost_cloud_.publish(out_sensor_points_cost_cloud_msg);

  sensor_msgs::PointCloud2 out_vectormap_cost_cloud_msg;
  grid_map::GridMapRosConverter::toPointCloud(costmap, VECTORMAP_COSTMAP_LAYER_, out_vectormap_cost_cloud_msg);
  pub_vectormap_cost_cloud_.publish(out_vectormap_cost_cloud_msg);

  sensor_msgs::PointCloud2 out_combined_cost_cloud_msg;
  grid_map::GridMapRosConverter::toPointCloud(costmap, COMBINED_COSTMAP_LAYER_, out_combined_cost_cloud_msg);
  pub_combined_cost_cloud_.publish(out_combined_cost_cloud_msg);

  grid_map_msgs::GridMap out_gridmap_msg;
  grid_map::GridMapRosConverter::toMessage(costmap, out_gridmap_msg);
  pub_costmap_.publish(out_gridmap_msg);
}
