// The Notices and Disclaimers for Ocean Worlds Autonomy Testbed for Exploration
// Research and Simulation can be found in README.md in the root directory of
// this repository.

// OW autonomy ROS node.

// ROS
#include <ros/ros.h>
#include <ros/package.h>

// OW
#include "OwExecutive.h"
#include "OwInterface.h"

int main(int argc, char* argv[])
{
  // Initializations

  ros::init(argc, argv, "autonomy_node");

  if (! OwExecutive::instance()->initialize()) {
    ROS_ERROR("Could not initialize OW executive, shutting down.");
    return 1;
  }

  OwInterface::instance()->initialize();

  // Run the specified plan

  if (argc == 2) {
    ROS_INFO ("Running plan %s", argv[1]);
    OwExecutive::instance()->runPlan (argv[1]); // asynchronous
  }
  else {
    ROS_ERROR("autonomy_node got %i args, expected 2", argc);
    return 1;
  }

  // ROS Loop (runs concurrently with plan).  Note that once this loop starts,
  // this function (and node) is terminated with an interrupt.  I believe this
  // is the ROS convention.

  ros::Rate rate(1); // 1 Hz seems appropriate, for now.
  while (ros::ok()) {
    ros::spinOnce();
    rate.sleep();
  }

  return 0;  // We never actually get here!
}
