#include "subscriber.hh"

using std::string;

// The subscribers.  Their naming convention is:
//   Subscribe<value-type><param-type>...

static SubscribeBool SubscriberBool = NULL;
static SubscribeString SubscriberString = NULL;
static SubscribeBoolString SubscriberBoolString = NULL;

void setSubscriber (SubscribeBool s) { SubscriberBool = s; }
void setSubscriber (SubscribeString s) { SubscriberString = s; }
void setSubscriber (SubscribeBoolString s) { SubscriberBoolString = s; }

// The overloaded publish function, one for each value/parameter combination
// found in this application.

void publish (const string& state_name, bool val)
{
  SubscriberBool (state_name, val);
}

void publish (const string& state_name, const string& val)
{
  SubscriberString (state_name, val);
}

void publish (const std::string& state_name, bool val, const std::string& arg)
{
  SubscriberBoolString (state_name, val, arg);
}
