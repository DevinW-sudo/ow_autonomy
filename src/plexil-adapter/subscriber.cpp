// The Notices and Disclaimers for Ocean Worlds Autonomy Testbed for Exploration
// Research and Simulation can be found in README.md in the root directory of
// this repository.

#include "subscriber.h"

// The subscribers.  Their naming convention is:
//   Subscribe<value-type><param-type>...

// To do: templatize!

static SubscribeBool SubscriberBool = nullptr;
static SubscribeDouble SubscriberDouble = nullptr;
static SubscribeString SubscriberString = nullptr;
static SubscribeBoolString SubscriberBoolString = nullptr;

void setSubscriber (SubscribeBool s) { SubscriberBool = s; }
void setSubscriber (SubscribeDouble s) { SubscriberDouble = s; }
void setSubscriber (SubscribeString s) { SubscriberString = s; }
void setSubscriber (SubscribeBoolString s) { SubscriberBoolString = s; }

// The overloaded publish function, one for each value/parameter combination
// found in this application.

void publish (const string& state_name, bool val)
{
  SubscriberBool (state_name, val);
}

void publish (const string& state_name, double val)
{
  SubscriberDouble (state_name, val);
}

void publish (const string& state_name, const string& val)
{
  SubscriberString (state_name, val);
}

void publish (const std::string& state_name, bool val, const std::string& arg)
{
  SubscriberBoolString (state_name, val, arg);
}
