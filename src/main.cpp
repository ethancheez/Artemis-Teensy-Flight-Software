#include <Arduino.h>
#include <vector>
#include <artemis_channels.h>

struct thread_struct
{
  int thread_id;
  const char *thread_name;
};

std::vector<struct thread_struct> thread_list;

void setup()
{
  Serial.begin(115200);

  // Threads
  // thread_list.push_back({threads.addThread(Artemis::Teensy::Channels::rfm23_channel), "rfm23 thread"});
  thread_list.push_back({threads.addThread(Artemis::Teensy::Channels::accelerometer_gyroscope_channel), "accelerometer gyroscope thread"});
  thread_list.push_back({threads.addThread(Artemis::Teensy::Channels::magnetometer_channel), "magnetomter thread"});
  thread_list.push_back({threads.addThread(Artemis::Teensy::Channels::temperature_channel), "temperature thread"});
  thread_list.push_back({threads.addThread(Artemis::Teensy::Channels::current_channel), "current thread"});
  thread_list.push_back({threads.addThread(Artemis::Teensy::Channels::pdu_channel), "pdu thread"});
}
void loop()
{
}
