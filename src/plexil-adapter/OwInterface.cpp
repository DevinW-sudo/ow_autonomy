// The Notices and Disclaimers for Ocean Worlds Autonomy Testbed for Exploration
// Research and Simulation can be found in README.md in the root directory of
// this repository.

// ow_autonomy
#include "OwInterface.h"
#include "subscriber.h"
#include "joint_support.h"

// OW - other
#include <ow_lander/DigCircular.h>
#include <ow_lander/DigLinear.h>
#include <ow_lander/GuardedMove.h>
#include <ow_lander/PublishTrajectory.h>

// ROS
#include <std_msgs/Float64.h>
#include <std_msgs/Empty.h>
#include <sensor_msgs/Image.h>

// C++
#include <set>
#include <map>
#include <thread>
#include <functional>
using std::set;
using std::map;
using std::thread;
using std::ref;

// C
#include <cmath>  // for M_PI and fabs


//////////////////// Utilities ////////////////////////

// Degree/Radian
const double D2R = M_PI / 180.0 ;
const double R2D = 180.0 / M_PI ;

const double DegreeTolerance = 0.2;    // made up, degees
const double VelocityTolerance = 0.01; // made up, unitless

static bool within_tolerance (double val1, double val2, double tolerance)
{
  return fabs (val1 - val2) <= tolerance;
}


//////////////////// Lander Operation Support ////////////////////////

const double PanTiltTimeout = 5.0; // seconds, made up

// Lander operation names.
// In some cases, these must match those used in PLEXIL and/or ow_lander
const string Op_GuardedMove       = "GuardedMove";
const string Op_GuardedMoveAction = "GuardedMoveAction";
const string Op_DigCircular       = "DigCircular";
const string Op_DigLinear         = "DigLinear";
const string Op_PublishTrajectory = "PublishTrajectory";
const string Op_PanAntenna        = "PanAntenna";
const string Op_TiltAntenna       = "TiltAntenna";

// NOTE: The following map *should* be thread-safe, according to C++11 docs and
// in particular because map entries are never added or deleted, and the code
// insures that each entry can be read/written by only one thread.  (The map
// itself can be read/written by multiple threads concurrently).

static map<string, bool> Running
{
  { Op_GuardedMove, false },
  { Op_GuardedMoveAction, false },
  { Op_DigCircular, false },
  { Op_DigLinear, false },
  { Op_PublishTrajectory, false },
  { Op_PanAntenna, false },
  { Op_TiltAntenna, false }
};

static bool is_lander_operation (const string& name)
{
  return Running.find (name) != Running.end();
}

static bool mark_operation_running (const string& name)
{
  if (Running.at (name)) {
    ROS_WARN ("%s already running, ignoring duplicate request.", name.c_str());
    return false;
  }

  Running.at (name) = true;
  publish ("Running", true, name);
  return true;
}

static void mark_operation_finished (const string& name)
{
  if (! Running.at (name)) {
    ROS_WARN ("%s was not running. Should never happen.", name.c_str());
  }
  Running.at (name) = false;
  publish ("Running", false, name);
  publish ("Finished", true, name);
}


//////////////////// Fault Support ////////////////////////

// NOTE: the design goal is to map each lander operation to the set of faults
// that should be monitored while it is running.  This direct inspection of ROS
// parameters is just a simple first cut (stub really) for actual fault
// detection which would look at telemetry.

const map<string, string> AntennaFaults
{
  // Param name -> human-readable
  { "/faults/ant_pan_encoder_failure", "Antenna Pan Encoder" },
  { "/faults/ant_tilt_encoder_failure", "Antenna Tilt Encoder" },
  { "/faults/ant_pan_torque_sensor_failure", "Antenna Pan Torque Sensor" },
  { "/faults/ant_tilt_torque_sensor_failure", "Antenna Tilt Torque Sensor" }
};

const map<string, string> ArmFaults
{
  // Param name -> human-readable
  { "/faults/shou_yaw_encoder_failure", "Shoulder Yaw Encoder" },
  { "/faults/shou_pitch_encoder_failure", "Shoulder Pitch Encoder" },
  { "/faults/shou_pitch_torque_sensor_failure", "Shoulder Pitch Torque Sensor" },
  { "/faults/prox_pitch_encoder_failure", "Proximal Pitch Encoder" },
  { "/faults/prox_pitch_torque_sensor_failure", "Proximal Pitch Torque Sensor" },
  { "/faults/dist_pitch_encoder_failure", "Distal Pitch Encoder" },
  { "/faults/dist_pitch_torque_sensor_failure", "Distal Pitch Torque Sensor" },
  { "/faults/hand_yaw_encoder_failure", "Hand Yaw Encoder" },
  { "/faults/hand_yaw_torque_sensor_failure", "Hand Yaw Torque Sensor" },
  { "/faults/scoop_yaw_encoder_failure", "Scoop Yaw Encoder" },
  { "/faults/scoop_yaw_torque_sensor_failure", "Scoop Yaw Torque Sensor" }
};

const map<string, map<string, string> > Faults
{
  // Map each lander operation to its relevant fault set.
  { Op_GuardedMove, ArmFaults },
  { Op_GuardedMoveAction, ArmFaults },
  { Op_DigCircular, ArmFaults },
  { Op_DigLinear, ArmFaults },
  { Op_PublishTrajectory, ArmFaults },
  { Op_PanAntenna, AntennaFaults },
  { Op_TiltAntenna, AntennaFaults }
};

static bool faulty (const string& fault)
{
  bool val;
  ros::param::get (fault, val);
  return val;
}

static void monitor_for_faults (const string& opname)
{
  using namespace std::chrono_literals;
  while (Running.at (opname)) {
    ROS_DEBUG ("Monitoring for faults in %s", opname.c_str());
    for (auto fault : Faults.at (opname)) {
      if (faulty (fault.first)) {
        ROS_WARN("Fault in %s: %s failure.",
                 opname.c_str(), fault.second.c_str());
      }
    }
    std::this_thread::sleep_for (1s);
  }
}


/////////////////// ROS Service support //////////////////////

template<class Service>
static void service_call (ros::ServiceClient client, Service srv, string name)
{
  // NOTE: arguments are copies because this function is called in a thread that
  // outlives its caller.  Assumes that service is not already running; this is
  // checked upstream.

  thread fault_thread (monitor_for_faults, name);
  if (client.call (srv)) { // blocks
    ROS_INFO("%s returned: %d, %s", name.c_str(), srv.response.success,
             srv.response.message.c_str());  // make DEBUG later
  }
  else {
    ROS_ERROR("Failed to call service %s", name.c_str());
  }
  mark_operation_finished (name);
  fault_thread.join();
}

static bool check_service_client (ros::ServiceClient& client)
{
  if (! client.exists()) {
    ROS_ERROR("Service client does not exist!");
    return false;
  }

  if (! client.isValid()) {
    ROS_ERROR("Service client is invalid!");
    return false;
  }

  return true;
}


/////////////////////////// Joint/Torque Support ///////////////////////////////

static set<string> JointsAtHardTorqueLimit { };
static set<string> JointsAtSoftTorqueLimit { };

static map<string, Joint> JointMap {
  // ROS JointStates message name -> type
  { "j_shou_yaw", Joint::shoulder_yaw },
  { "j_shou_pitch", Joint::shoulder_pitch },
  { "j_prox_pitch", Joint::proximal_pitch },
  { "j_dist_pitch", Joint::distal_pitch },
  { "j_hand_yaw", Joint::hand_yaw },
  { "j_scoop_yaw", Joint::scoop_yaw },
  { "j_ant_pan", Joint::antenna_pan },
  { "j_ant_tilt", Joint::antenna_tilt }
};

static map<Joint, JointProperties> JointPropMap {
  // NOTE: Torque limits are made up, and there may be a better place for these
  // later.  Assuming that only magnitude matters.

  { Joint::shoulder_yaw,   { "j_shou_yaw", "ShoulderYaw", 60, 80 }},
  { Joint::shoulder_pitch, { "j_shou_pitch", "ShoulderPitch", 60, 80 }},
  { Joint::proximal_pitch, { "j_prox_pitch", "ProximalPitch", 60, 80 }},
  { Joint::distal_pitch,   { "j_dist_pitch", "DistalPitch", 60, 80 }},
  { Joint::hand_yaw,       { "j_hand_yaw", "HandYaw", 60, 80 }},
  { Joint::scoop_yaw,      { "j_scoop_yaw", "ScoopYaw", 60, 80 }},
  { Joint::antenna_pan,    { "j_ant_pan", "AntennaPan", 30, 30 }},
  { Joint::antenna_tilt,   { "j_ant_tilt", "AntennaTilt", 30, 30 }}
};

static map<Joint, JointTelemetry> JointTelemetryMap { };

static void handle_overtorque (Joint joint, double effort)
{
  // For now, torque is just effort (Newton-meter), and overtorque is specific
  // to the joint.

  string joint_name = JointPropMap[joint].plexilName;

  if (fabs(effort) >= JointPropMap[joint].hardTorqueLimit) {
    JointsAtHardTorqueLimit.insert (joint_name);
  }
  else if (fabs(effort) >= JointPropMap[joint].softTorqueLimit) {
    JointsAtSoftTorqueLimit.insert(joint_name);
  }
  else {
    JointsAtHardTorqueLimit.erase (joint_name);
    JointsAtSoftTorqueLimit.erase (joint_name);
  }
}

static void handle_joint_fault (Joint joint, int joint_index,
                                const sensor_msgs::JointState::ConstPtr& msg)
{
  // NOTE: For now, the only fault is overtorque.
  handle_overtorque (joint, msg->effort[joint_index]);
}

void OwInterface::jointStatesCallback
(const sensor_msgs::JointState::ConstPtr& msg)
{
  // Publish all joint information for visibility to PLEXIL and handle any
  // joint-related faults.

  for (int i = 0; i < JointMap.size(); i++) {
    string ros_name = msg->name[i];
    if (JointMap.find (ros_name) != JointMap.end()) {
      Joint joint = JointMap[ros_name];
      double position = msg->position[i];
      double velocity = msg->velocity[i];
      double effort = msg->effort[i];
      if (joint == Joint::antenna_pan) {
        managePanTilt (Op_PanAntenna, position, velocity, m_currentPan,
                       m_goalPan, m_panStart);
      }
      else if (joint == Joint::antenna_tilt) {
        managePanTilt (Op_TiltAntenna, position, velocity, m_currentTilt,
                       m_goalTilt, m_tiltStart);
      }
      JointTelemetryMap[joint] = JointTelemetry (position, velocity, effort);
      string plexil_name = JointPropMap[joint].plexilName;
      publish (plexil_name + "Position", position);
      publish (plexil_name + "Velocity", velocity);
      publish (plexil_name + "Effort", effort);
      handle_joint_fault (joint, i, msg);
    }
    else ROS_ERROR("jointStatesCallback: unsupported joint %s",
                   ros_name.c_str());
  }
}

void OwInterface::managePanTilt (const string& opname,
                                 double position, double velocity,
                                 double current, double goal,
                                 const ros::Time& start)
{
  // We are only concerned when there is a pan/tilt in progress.
  if (! operationRunning (opname)) return;

  // Antenna states of interest,
  bool reached = within_tolerance (current, goal, DegreeTolerance);
  bool expired = ros::Time::now() > start + ros::Duration (PanTiltTimeout);

  if (reached || expired) {
    mark_operation_finished (opname);
    if (expired) ROS_ERROR("%s timed out", opname.c_str());
    if (! reached) {
      ROS_ERROR("%s failed. Ended at %f degrees, goal was %f.",
                opname.c_str(), current, goal);
    }
  }
}


////////////////////////////// Image Support ///////////////////////////////////

static bool   ImageReceived       = false;

void OwInterface::panCallback
(const control_msgs::JointControllerState::ConstPtr& msg)
{
  m_currentPan = msg->set_point * R2D;
  publish ("PanDegrees", m_currentPan);
}

void OwInterface::tiltCallback
(const control_msgs::JointControllerState::ConstPtr& msg)
{
  m_currentTilt = msg->set_point * R2D;
  publish ("TiltDegrees", m_currentTilt);
}

static void camera_callback (const sensor_msgs::Image::ConstPtr& msg)
{
  // Assuming that receipt of this message is success itself.
  ImageReceived = true;
  publish ("ImageReceived", ImageReceived);
}


//////////////////// GuardedMove Action support ////////////////////////////////

// At present, this is a prototypical action using a dummy server in this
// directory, GuardedMoveServer.

static void guarded_move_done_cb
(const actionlib::SimpleClientGoalState& state,
 const ow_autonomy::GuardedMoveResultConstPtr& result)
{
  ROS_INFO ("GuardedMove done callback: finished in state [%s]",
            state.toString().c_str());
  ROS_INFO("GuardedMove done callback: result (%f, %f, %f)",
           result->final_x, result->final_y, result->final_z);
}

static void guarded_move_active_cb ()
{
  ROS_INFO ("GuardedMove active callback - goal active!");
}

static void guarded_move_feedback_cb
(const ow_autonomy::GuardedMoveFeedbackConstPtr& feedback)
{
  ROS_INFO ("GuardedMove feedback callback: (%f, %f, %f)",
            feedback->current_x, feedback->current_y, feedback->current_z);
}


/////////////////////////// OwInterface members ////////////////////////////////

OwInterface* OwInterface::m_instance = nullptr;

OwInterface* OwInterface::instance ()
{
  // Very simple singleton
  if (m_instance == nullptr) m_instance = new OwInterface();
  return m_instance;
}

OwInterface::OwInterface ()
  : m_genericNodeHandle (nullptr),
    m_antennaTiltPublisher (nullptr),
    m_antennaPanPublisher (nullptr),
    m_leftImageTriggerPublisher (nullptr),
    m_antennaTiltSubscriber (nullptr),
    m_antennaPanSubscriber (nullptr),
    m_jointStatesSubscriber (nullptr),
    m_cameraSubscriber (nullptr),
    m_guardedMoveClient ("GuardedMove", true),
    m_currentPan (0), m_currentTilt (0),
    m_goalPan (0), m_goalTilt (0)
    // m_panStart, m_tiltStart left uninitialized
{
  ROS_INFO ("Waiting for action servers...");
  m_guardedMoveClient.waitForServer();
  ROS_INFO ("Action servers available.");
}

OwInterface::~OwInterface ()
{
  if (m_genericNodeHandle) delete m_genericNodeHandle;
  if (m_antennaTiltPublisher) delete m_antennaTiltPublisher;
  if (m_leftImageTriggerPublisher) delete m_leftImageTriggerPublisher;
  if (m_antennaTiltSubscriber) delete m_antennaTiltSubscriber;
  if (m_antennaPanSubscriber) delete m_antennaPanSubscriber;
  if (m_jointStatesSubscriber) delete m_jointStatesSubscriber;
  if (m_cameraSubscriber) delete m_cameraSubscriber;
  if (m_instance) delete m_instance;
}

void OwInterface::initialize()
{
  static bool initialized = false;

  if (not initialized) {
    m_genericNodeHandle = new ros::NodeHandle();

    // Initialize publishers.  Queue size is a guess at adequacy.  For now,
    // latching in lieu of waiting for publishers.

    const int qsize = 3;
    const bool latch = true;
    m_antennaTiltPublisher = new ros::Publisher
      (m_genericNodeHandle->advertise<std_msgs::Float64>
       ("/ant_tilt_position_controller/command", qsize, latch));
    m_antennaPanPublisher = new ros::Publisher
      (m_genericNodeHandle->advertise<std_msgs::Float64>
       ("/ant_pan_position_controller/command", qsize, latch));
    m_leftImageTriggerPublisher = new ros::Publisher
      (m_genericNodeHandle->advertise<std_msgs::Empty>
       ("/StereoCamera/left/image_trigger", qsize, latch));

    // Initialize subscribers

    m_antennaTiltSubscriber = new ros::Subscriber
      (m_genericNodeHandle ->
       subscribe("/ant_tilt_position_controller/state", qsize,
                 &OwInterface::tiltCallback, this));
    m_antennaPanSubscriber = new ros::Subscriber
      (m_genericNodeHandle ->
       subscribe("/ant_pan_position_controller/state", qsize,
                 &OwInterface::panCallback, this));
    m_jointStatesSubscriber = new ros::Subscriber
      (m_genericNodeHandle ->
       subscribe("/joint_states", qsize,
                 &OwInterface::jointStatesCallback, this));
    m_cameraSubscriber = new ros::Subscriber
      (m_genericNodeHandle ->
       subscribe("/StereoCamera/left/image_raw", qsize, camera_callback));
  }
}

void OwInterface::digCircularDemo()
{
  if (! mark_operation_running (Op_DigCircular)) return;

  ros::NodeHandle nhandle ("planning");

  ros::ServiceClient client =
    // NOTE: typo is deliberate
    nhandle.serviceClient<ow_lander::DigCircular>("arm/dig_circular");

  if (check_service_client (client)) {
    ow_lander::DigCircular srv;
    srv.request.use_defaults = true;
    srv.request.x = 0.0;
    srv.request.y = 0.0;
    srv.request.depth = 0.0;
    srv.request.radial = false;
    srv.request.ground_position = 0.0;
    srv.request.delete_prev_traj = false;
    thread service_thread (service_call<ow_lander::DigCircular>,
                           client, srv, Op_DigCircular);
    service_thread.detach();
  }
}

void OwInterface::guardedMoveActionDemo()
{
  guardedMoveAction();
}

void OwInterface::guardedMoveAction (double x,
                                     double y,
                                     double z,
                                     double direction_x,
                                     double direction_y,
                                     double direction_z,
                                     double search_distance,
                                     bool delete_prev_traj)
{
  if (! mark_operation_running (Op_GuardedMoveAction)) return;

  thread action_thread (&OwInterface::guardedMoveActionAux, this,
                        x, y, z,
                        direction_x, direction_y, direction_z,
                        search_distance, delete_prev_traj);
  action_thread.detach();
}

void OwInterface::guardedMoveActionAux (double x,
                                        double y,
                                        double z,
                                        double direction_x,
                                        double direction_y,
                                        double direction_z,
                                        double search_distance,
                                        bool delete_prev_traj)
{
  ow_autonomy::GuardedMoveGoal goal;
  goal.use_defaults = false;
  goal.delete_prev_traj = delete_prev_traj;
  goal.x = x;
  goal.y = y;
  goal.z = z;
  goal.direction_x = direction_x;
  goal.direction_y = direction_y;
  goal.direction_z = direction_z;
  goal.search_distance = search_distance;

  thread fault_thread (monitor_for_faults, Op_GuardedMoveAction);
  m_guardedMoveClient.sendGoal (goal,
                                guarded_move_done_cb,
                                guarded_move_active_cb,
                                guarded_move_feedback_cb);

  // Wait for the action to return
  bool finished_before_timeout =
    m_guardedMoveClient.waitForResult (ros::Duration (30.0));

  if (finished_before_timeout) {
    actionlib::SimpleClientGoalState state = m_guardedMoveClient.getState();
    ROS_INFO("GuardedMove action finished: %s", state.toString().c_str());
    ow_autonomy::GuardedMoveResultConstPtr result =
      m_guardedMoveClient.getResult();
    ROS_INFO("GuardedMove action result: (%f, %f, %f)",
             result->final_x, result->final_y, result->final_z);
  }
  else {
    ROS_INFO("GuardedMove action did not finish before the time out.");
  }

  mark_operation_finished (Op_GuardedMoveAction);
  fault_thread.join();
}

void OwInterface::guardedMoveDemo()
{
  guardedMove();
}

void OwInterface::guardedMove (double x, double y, double z,
                               double direction_x,
                               double direction_y,
                               double direction_z,
                               double search_distance,
                               bool delete_prev_traj)
{
  if (! mark_operation_running (Op_GuardedMove)) return;

  ros::NodeHandle nhandle ("planning");

  ros::ServiceClient client =
    nhandle.serviceClient<ow_lander::GuardedMove>("arm/guarded_move");

  if (check_service_client (client)) {
    ow_lander::GuardedMove srv;
    srv.request.use_defaults = false;
    srv.request.x = x;
    srv.request.y = y;
    srv.request.z = z;
    srv.request.direction_x = direction_x;
    srv.request.direction_y = direction_y;
    srv.request.direction_z = direction_z;
    srv.request.search_distance = search_distance;
    srv.request.delete_prev_traj = delete_prev_traj;
    thread service_thread (service_call<ow_lander::GuardedMove>,
                           client, srv, Op_GuardedMove);
    service_thread.detach();
  }
}

void OwInterface::publishTrajectoryDemo()
{
  if (! mark_operation_running (Op_PublishTrajectory)) return;

  ros::NodeHandle nhandle ("planning");

  ros::ServiceClient client =
    nhandle.serviceClient<ow_lander::PublishTrajectory>("arm/publish_trajectory");

  if (check_service_client (client)) {
    ow_lander::PublishTrajectory srv;
    srv.request.use_latest = true;
    srv.request.trajectory_filename = "";
    thread service_thread (service_call<ow_lander::PublishTrajectory>,
                           client, srv, Op_PublishTrajectory);
    service_thread.detach();
  }
}

static bool antenna_op (const string& opname,
                        double degrees,
                        ros::Publisher* pub)
{
  if (! mark_operation_running (opname)) {
    return false;
  }

  std_msgs::Float64 radians;
  radians.data = degrees * D2R;
  ROS_INFO ("Starting %s: %f degrees (%f radians)", opname.c_str(),
            degrees, radians.data);
  thread fault_thread (monitor_for_faults, opname);
  fault_thread.detach();
  pub->publish (radians);
  return true;
}

bool OwInterface::tiltAntenna (double degrees)
{
  m_goalTilt = degrees;
  m_tiltStart = ros::Time::now();
  return antenna_op (Op_TiltAntenna, degrees, m_antennaTiltPublisher);
}

bool OwInterface::panAntenna (double degrees)
{
  m_goalPan = degrees;
  m_panStart = ros::Time::now();
  return antenna_op (Op_PanAntenna, degrees, m_antennaPanPublisher);
}

void OwInterface::takePicture ()
{
  std_msgs::Empty msg;
  ImageReceived = false;
  publish ("ImageReceived", ImageReceived);
  m_leftImageTriggerPublisher->publish (msg);
}

void OwInterface::digLinear (double x, double y,
                             double depth, double length, double ground_position,
                             double width, double pitch, double yaw,
                             double dumpx, double dumpy, double dumpz)
{
  if (! mark_operation_running (Op_DigLinear)) return;

  ros::NodeHandle nhandle ("planning");

  ros::ServiceClient client =
    nhandle.serviceClient<ow_lander::DigLinear>("arm/dig_linear");

  if (check_service_client (client)) {
    ow_lander::DigLinear srv;
    srv.request.use_defaults = false;
    srv.request.x = x;
    srv.request.y = y;
    srv.request.depth = depth;
    srv.request.length = length;
    srv.request.ground_position = ground_position;
    srv.request.delete_prev_traj = false;
    thread service_thread (service_call<ow_lander::DigLinear>,
                           client, srv, Op_DigLinear);
    service_thread.detach();
  }
}

double OwInterface::getTilt () const
{
  return m_currentTilt;
}

double OwInterface::getPanDegrees () const
{
  return m_currentPan;
}

double OwInterface::getPanVelocity () const
{
  return JointTelemetryMap[Joint::antenna_pan].velocity;
}

double OwInterface::getTiltVelocity () const
{
  return JointTelemetryMap[Joint::antenna_tilt].velocity;
}

bool OwInterface::imageReceived () const
{
  return ImageReceived;
}

bool OwInterface::operationRunning (const string& name) const
{
  // Note: check in caller guarantees 'at' to return a valid value.
  return Running.at (name);
}

bool OwInterface::operationFinished (const string& name) const
{
  return !operationRunning (name);
}

bool OwInterface::running (const string& name) const
{
  if (is_lander_operation (name)) return operationRunning (name);

  ROS_ERROR("OwInterface::running: unsupported operation: %s", name.c_str());
  return false;
}

bool OwInterface::finished (const string& name) const
{
  if (is_lander_operation (name)) return operationFinished (name);

  ROS_ERROR("OwInterface::finished: unsupported operation: %s", name.c_str());
  return false;
}

bool OwInterface::hardTorqueLimitReached (const std::string& joint_name) const
{
  return (JointsAtHardTorqueLimit.find (joint_name) !=
          JointsAtHardTorqueLimit.end());
}

bool OwInterface::softTorqueLimitReached (const std::string& joint_name) const
{
  return (JointsAtSoftTorqueLimit.find (joint_name) !=
          JointsAtSoftTorqueLimit.end());
}
