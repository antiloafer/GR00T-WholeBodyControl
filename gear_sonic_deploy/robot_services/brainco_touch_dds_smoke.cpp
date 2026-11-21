#include "dds/Subscription.h"

#include <unitree/robot/channel/channel_factory.hpp>
#include <unitree/idl/go2/MotorStates_.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

using TouchMessage = unitree_go::msg::dds_::MotorStates_;

int main(int argc, char **argv) {
  const std::string interface = argc > 1 ? argv[1] : "eth0";
  unitree::robot::ChannelFactory::Instance()->Init(0, interface);

  unitree::robot::SubscriptionBase<TouchMessage> left("rt/brainco/left/touch");
  unitree::robot::SubscriptionBase<TouchMessage> right("rt/brainco/right/touch");
  left.set_timeout_ms(500);
  right.set_timeout_ms(500);

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  TouchMessage left_msg;
  TouchMessage right_msg;
  while (std::chrono::steady_clock::now() < deadline) {
    if (left.getFreshMessage(left_msg) && right.getFreshMessage(right_msg)) {
      if (left_msg.states().size() != 5 || right_msg.states().size() != 5) {
        std::cerr << "unexpected touch vector size\n";
        return 1;
      }
      std::cout << "left_status=";
      for (int i = 0; i < 5; ++i) {
        std::cout << (i ? "," : "") << left_msg.states()[i].lost();
      }
      std::cout << " right_status=";
      for (int i = 0; i < 5; ++i) {
        std::cout << (i ? "," : "") << right_msg.states()[i].lost();
      }
      std::cout << "\nBRAINCO_TOUCH_DDS_PASS\n";
      return 0;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  std::cerr << "timed out waiting for both touch topics\n";
  return 1;
}
