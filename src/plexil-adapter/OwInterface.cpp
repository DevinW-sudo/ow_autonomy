// __BEGIN_LICENSE__
// Copyright (c) 2018-2020, United States Government as represented by the
// Administrator of the National Aeronautics and Space Administration. All
// rights reserved.
// __END_LICENSE__

// OW
#include "OwInterface.h"
#include "subscriber.h"
#include <ow_lander/StartPlanning.h>
#include <ow_lander/MoveGuarded.h>
#include <ow_lander/PublishTrajectory.h>

// ROS
#include <std_msgs/Float64.h>
#include <std_msgs/Empty.h>
#include <sensor_msgs/JointState.h>

// C
#include <math.h>  // for M_PI

// Degree/Radian
const double D2R = M_PI / 180.0 ;
const double R2D = 180.0 / M_PI ;

// Lander state cache, simple start for now -- may need to refactor later.
// Individual variables for now -- may want to employ a container if this gets
// big.

double CurrentTilt = 0.0;
double CurrentPan = 0.0;

OwInterface* OwInterface::m_instance = nullptr;

OwInterface* OwInterface::instance ()
{
  // Very simple singleton
  if (m_instance == nullptr) m_instance = new OwInterface();
  return m_instance;
}

// Subscription callbacks

static void pan_callback
(const control_msgs::JointControllerState::ConstPtr& msg)
{
  CurrentPan = msg->set_point * R2D;
  publish ("PanDegrees", CurrentPan);
}

static void tilt_callback
(const control_msgs::JointControllerState::ConstPtr& msg)
{
  CurrentTilt = msg->set_point * R2D;
  publish ("TiltDegrees", CurrentTilt);
}



OwInterface::OwInterface ()
  : m_genericNodeHandle (nullptr),
    m_antennaTiltPublisher (nullptr),
    m_antennaPanPublisher (nullptr),
    m_leftImageTriggerPublisher (nullptr),
    m_antennaTiltSubscriber (nullptr),
    m_antennaPanSubscriber (nullptr)
{ }

OwInterface::~OwInterface ()
{
  if (m_genericNodeHandle) delete m_genericNodeHandle;
  if (m_antennaTiltPublisher) delete m_antennaTiltPublisher;
  if (m_leftImageTriggerPublisher) delete m_leftImageTriggerPublisher;
  if (m_antennaTiltSubscriber) delete m_antennaTiltSubscriber;
  if (m_antennaPanSubscriber) delete m_antennaPanSubscriber;
  if (m_instance) delete m_instance;
}

void OwInterface::initialize()
{
  // Hack?  Does this function need to be idempotent?
  static bool initialized = false;

  if (not initialized) {
    m_genericNodeHandle = new ros::NodeHandle();
    m_antennaTiltPublisher = new ros::Publisher
      (m_genericNodeHandle->advertise<std_msgs::Float64>
       ("/ant_tilt_position_controller/command", 1));
    m_antennaPanPublisher = new ros::Publisher
      (m_genericNodeHandle->advertise<std_msgs::Float64>
       ("/ant_pan_position_controller/command", 1));
    m_leftImageTriggerPublisher = new ros::Publisher
      (m_genericNodeHandle->advertise<std_msgs::Empty>
       ("/StereoCamera/left/image_trigger", 1));
    m_antennaTiltSubscriber = new ros::Subscriber
      (m_genericNodeHandle ->
       subscribe("/ant_tilt_position_controller/state", 1000, tilt_callback));
    m_antennaPanSubscriber = new ros::Subscriber
      (m_genericNodeHandle ->
       subscribe("/ant_pan_position_controller/state", 1000, pan_callback));
    initialized = true;
  }
}

void OwInterface::startPlanningDemo()
{
  ros::NodeHandle nhandle ("planning");

  ros::ServiceClient client =
    // NOTE: typo is deliberate
    nhandle.serviceClient<ow_lander::StartPlanning>("start_plannning_session");

  if (! client.exists()) {
    ROS_ERROR("Service client does not exist!");
  }
  else if (! client.isValid()) {
    ROS_ERROR("Service client is invalid!");
  }
  else {
    ow_lander::StartPlanning srv;
    srv.request.use_defaults = true;
    srv.request.trench_x = 0.0;
    srv.request.trench_y = 0.0;
    srv.request.trench_d = 0.0;
    srv.request.delete_prev_traj = false;

    if (client.call(srv)) {
      ROS_INFO("StartPlanning returned: %d, %s",
               srv.response.success,
               srv.response.message.c_str());
    }
    else {
      ROS_ERROR("Failed to call service StartPlanning");
    }
  }
}


void OwInterface::moveGuardedDemo()
{
  ros::NodeHandle nhandle ("planning");

  ros::ServiceClient client =
    nhandle.serviceClient<ow_lander::MoveGuarded>("start_move_guarded");

  if (! client.exists()) {
    ROS_ERROR("Service client does not exist!");
  }
  else if (! client.isValid()) {
    ROS_ERROR("Service client is invalid!");
  }
  else {
    ow_lander::MoveGuarded srv;
    srv.request.use_defaults = true;
    srv.request.target_x = 0.0;
    srv.request.target_y = 0.0;
    srv.request.target_z = 0.0;
    srv.request.surface_normal_x = 0.0;
    srv.request.surface_normal_y = 0.0;
    srv.request.surface_normal_z = 0.0;
    srv.request.offset_distance = 0.0;
    srv.request.overdrive_distance = 0.0;
    srv.request.retract = false;

    if (client.call(srv)) {
      ROS_INFO("MoveGuarded returned: %d, %s",
               srv.response.success,
               srv.response.message.c_str());
    }
    else {
      ROS_ERROR("Failed to call service MoveGuarded");
    }
  }
}

void OwInterface::publishTrajectoryDemo()
{
  ros::NodeHandle nhandle ("planning");

  ros::ServiceClient client =
    nhandle.serviceClient<ow_lander::PublishTrajectory>("publish_trajectory");

  if (! client.exists()) {
    ROS_ERROR("Service client does not exist!");
  }
  else if (! client.isValid()) {
    ROS_ERROR("Service client is invalid!");
  }
  else {
    ow_lander::PublishTrajectory srv;
    srv.request.use_latest = true;
    srv.request.trajectory_filename = "ow_lander_trajectory.txt";
    if (client.call(srv)) {
      ROS_INFO("PublishTrajectory returned: %d, %s",
               srv.response.success,
               srv.response.message.c_str());
    }
    else {
      ROS_ERROR("Failed to call service PublishTrajectory");
    }
  }
}

void OwInterface::checkSubscribers (const ros::Publisher* pub) const
{
  if (pub->getNumSubscribers() == 0) {
    ROS_WARN("No subscribers for topic %s", pub->getTopic().c_str());
  }
}

void OwInterface::tiltAntenna (double arg)
{
  std_msgs::Float64 msg;
  msg.data = arg * D2R;
  checkSubscribers (m_antennaTiltPublisher);
  ROS_INFO("Tilting to %f degrees (%f radians)", arg, msg.data);
  m_antennaTiltPublisher->publish (msg);
}

void OwInterface::panAntenna (double arg)
{
  std_msgs::Float64 msg;
  msg.data = arg * D2R;
  checkSubscribers (m_antennaPanPublisher);
  ROS_INFO("Panning to %f degrees (%f radians)", arg, msg.data);
  m_antennaPanPublisher->publish (msg);
}

void OwInterface::takePicture ()
{
  std_msgs::Empty msg;
  checkSubscribers (m_leftImageTriggerPublisher);
  m_leftImageTriggerPublisher->publish (msg);
}

void OwInterface::digTrench (double x, double y, double z,
                             double depth, double length, double width,
                             double pitch, double yaw,
                             double dumpx, double dumpy, double dumpz)
{
  ROS_WARN("digTrench is unimplemented!");
}


// State

double OwInterface::getTilt () const
{
  return CurrentTilt;
}

double OwInterface::getPan () const
{
  return CurrentPan;
}
