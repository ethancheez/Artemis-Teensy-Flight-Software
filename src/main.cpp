#include <vector>
#include <Arduino.h>
#include <USBHost_t36.h>
#include <support/configCosmosKernel.h>
#include <pdu.h>
#include "channels/artemis_channels.h"
#include "artemis_devices.h"
#include "artemisbeacons.h"
#include "tests/packet_tests.h"

// For setting Teensy Clock Frequency (only for Teensy 4.0 and 4.1)
#if defined(__IMXRT1062__)
extern "C" uint32_t set_arm_clock(uint32_t frequency);
#endif

namespace
{
  using namespace Artemis::Teensy;
  Artemis::Devices devices;
  PacketComm packet;
  USBHost usb;
  elapsedMillis sensortimer;
  elapsedMillis uptime;
  int32_t iretn = 0;
}

void setup()
{
  Serial.begin(115200);

#if defined(__IMXRT1062__)
  set_arm_clock(450000000); // Allowed Frequencies (MHz): 24, 150, 396, 450, 528, 600
#endif

  usb.begin();
  pinMode(RPI_ENABLE, OUTPUT);
  pinMode(UART6_TX, INPUT);
  delay(3000);

  iretn = devices.setup_magnetometer();
  iretn = devices.setup_imu();

  devices.setup_current();
  devices.setup_gps();

  threads.setSliceMillis(10);

  // Threads
  thread_list.push_back({threads.addThread(Channels::rfm23_channel, 9000), Channels::Channel_ID::RFM23_CHANNEL});
  thread_list.push_back({threads.addThread(Channels::pdu_channel, 9000), Channels::Channel_ID::PDU_CHANNEL});

// Only uncomment these when testing and you want to force the RPi to turn on  
  // thread_list.push_back({threads.addThread(Channels::rpi_channel, 9000), Channels::Channel_ID::RPI_CHANNEL});
  // pinMode(RPI_ENABLE, HIGH);

  threads.delay(5000);
  Serial.println("Teensy Flight Software Setup Complete");
}

void loop()
{
#ifdef TEST_PACKETS
  send_test_packets();
  threads.delay(5000);
#endif
  pinMode(UART6_TX, INPUT);
  Serial.println(digitalRead(UART6_TX));

  if (PullQueue(packet, main_queue, main_queue_mtx))
  {
    if (packet.header.nodedest == (uint8_t)NODES::GROUND_NODE_ID)
    {
      switch (packet.header.chanout)
      {
      case Channels::Channel_ID::RFM23_CHANNEL:
        PushQueue(packet, rfm23_queue, rfm23_queue_mtx);
        break;
      default:
        break;
      }
    }
    else if (packet.header.nodedest == (uint8_t)NODES::RPI_NODE_ID)
    {
      if (!digitalRead(UART6_TX))
      {
        float curr_V = devices.current_sensors["battery_board"]->getBusVoltage_V();
        if ((curr_V >= 7.0) || 1)
        {
          Serial.println("Turning on RPi");
          digitalWrite(RPI_ENABLE, HIGH);
          thread_list.push_back({threads.addThread(Channels::rpi_channel, 9000), Channels::Channel_ID::RPI_CHANNEL});
          threads.delay(5000);
        }
        else
        {
          packet.header.type = PacketComm::TypeId::CommandEpsSwitchStatus;
          packet.header.nodeorig = (uint8_t)NODES::GROUND_NODE_ID;
          packet.header.nodedest = (uint8_t)NODES::TEENSY_NODE_ID;
          packet.data.clear();
          packet.data.push_back((uint8_t)Artemis::Teensy::PDU::PDU_SW::All);
          PushQueue(packet, pdu_queue, pdu_queue_mtx);
        }
      }
      PushQueue(packet, rpi_queue, rpi_queue_mtx);
    }
    else if (packet.header.nodedest == (uint8_t)NODES::TEENSY_NODE_ID)
    {
      switch (packet.header.type)
      {
      case PacketComm::TypeId::CommandObcPing:
      {
        packet.header.nodedest = packet.header.nodeorig;
        packet.header.nodeorig = (uint8_t)NODES::TEENSY_NODE_ID;
        packet.header.type = PacketComm::TypeId::DataObcPong;
        packet.data.resize(0);
        const char *data = "Pong";
        for (size_t i = 0; i < strlen(data); i++)
        {
          packet.data.push_back(data[i]);
        }
        switch (packet.header.chanout)
        {
        case Channels::Channel_ID::RFM23_CHANNEL:
          PushQueue(packet, rfm23_queue, rfm23_queue_mtx);
          break;
        default:
          break;
        }
        break;
      }
      case PacketComm::TypeId::CommandEpsCommunicate:
      {
        PushQueue(packet, pdu_queue, pdu_queue_mtx);
      }
      break;
      case PacketComm::TypeId::CommandEpsSwitchName:
      {
        PDU::PDU_SW switchid = (PDU::PDU_SW)packet.data[0];
        switch (switchid)
        {
        case PDU::PDU_SW::RPI:
        {
          float curr_V = devices.current_sensors["battery_board"]->getBusVoltage_V();
          if ((packet.data[1] == 1 && curr_V >= 7.0) || (packet.data[1] == 1 && packet.data[2] == 1))
          {
            digitalWrite(RPI_ENABLE, packet.data[1]);
            thread_list.push_back({threads.addThread(Channels::rpi_channel), Channels::Channel_ID::RPI_CHANNEL});
            threads.delay(5000);
          }
          else if (packet.data[1] == 0)
          {
            PushQueue(packet, rpi_queue, rpi_queue_mtx);
          }
          else
          {
            packet.header.type = PacketComm::TypeId::CommandEpsSwitchStatus;
            packet.header.nodeorig = (uint8_t)NODES::GROUND_NODE_ID;
            packet.header.nodedest = (uint8_t)NODES::TEENSY_NODE_ID;
            packet.data.clear();
            packet.data.push_back((uint8_t)Artemis::Teensy::PDU::PDU_SW::All);
            PushQueue(packet, pdu_queue, pdu_queue_mtx);
          }
          break;
        }
        default:
          PushQueue(packet, pdu_queue, pdu_queue_mtx);
          break;
        }
        break;
      }
      case PacketComm::TypeId::CommandEpsSwitchStatus:
      {
        PDU::PDU_SW switchid = (PDU::PDU_SW)packet.data[0];
        switch (switchid)
        {
        case PDU::PDU_SW::RPI:
        {
          packet.data.resize(1);
          packet.data.push_back(digitalRead(RPI_ENABLE));
          packet.header.type = PacketComm::TypeId::DataEpsResponse;
          packet.header.nodedest = packet.header.nodeorig;
          packet.header.nodeorig = (uint8_t)NODES::TEENSY_NODE_ID;
          PushQueue(packet, rfm23_queue, rfm23_queue_mtx);
        }
        break;
        default:
          PushQueue(packet, pdu_queue, pdu_queue_mtx);
          break;
        }
      }
      break;
      case PacketComm::TypeId::CommandObcSendBeacon:
      {
        devices.read_temperature(uptime);
        devices.read_current(uptime);
        devices.read_imu(uptime);
        devices.read_mag(uptime);
        devices.read_gps(uptime);

        // Get PDU Switches
        packet.header.type = PacketComm::TypeId::CommandEpsSwitchStatus;
        packet.header.nodeorig = (uint8_t)NODES::GROUND_NODE_ID;
        packet.header.nodedest = (uint8_t)NODES::TEENSY_NODE_ID;
        packet.data.clear();
        packet.data.push_back((uint8_t)Artemis::Teensy::PDU::PDU_SW::All);
        PushQueue(packet, pdu_queue, pdu_queue_mtx);
      }
      default:
        break;
      }
    }
  }

  // Periodic Telemetry
  if (sensortimer > 600000)
  {
    sensortimer = 0;
    devices.read_temperature(uptime);
    devices.read_current(uptime);
    devices.read_imu(uptime);
    devices.read_mag(uptime);
    devices.read_gps(uptime);

    // Get PDU Switches
    packet.header.type = PacketComm::TypeId::CommandEpsSwitchStatus;
    packet.header.nodeorig = (uint8_t)NODES::GROUND_NODE_ID;
    packet.header.nodedest = (uint8_t)NODES::TEENSY_NODE_ID;
    packet.data.clear();
    packet.data.push_back((uint8_t)Artemis::Teensy::PDU::PDU_SW::All);
    PushQueue(packet, pdu_queue, pdu_queue_mtx);
  }
  devices.update_gps();

  threads.delay(1000);
}
