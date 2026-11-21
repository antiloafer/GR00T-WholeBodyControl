#include "stark-sdk.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: " << argv[0] << " /dev/ttyUSBn slave_id\n";
    return 2;
  }

  const auto slave_id = static_cast<uint8_t>(std::strtoul(argv[2], nullptr, 0));
  init_logging(LogLevel::LOG_LEVEL_ERROR);

  DeviceHandler *handle = modbus_open(argv[1], 460800);
  if (!handle) {
    std::cerr << "failed to open " << argv[1] << "\n";
    return 1;
  }

  // Manual Modbus initialization requires a series/type hint before any
  // query whose register layout differs between Revo generations.
  stark_set_hardware_type(handle, slave_id, STARK_HARDWARE_TYPE_REVO2_TOUCH);

  uint16_t enabled[5]{};
  if (stark_read_holding_registers(handle, slave_id, 4000, 5, enabled) != 0) {
    std::cerr << "failed to read touch enable registers\n";
    modbus_close(handle);
    return 1;
  }
  std::cout << "enabled=" << enabled[0] << ',' << enabled[1] << ','
            << enabled[2] << ',' << enabled[3] << ',' << enabled[4] << '\n';

  // Read the documented Revo2 register layout directly. SDK v2.0.3's
  // high-level raw helper still assumes the older 7/11-channel layout and
  // aborts on the Revo2 six-channel response used by this firmware.
  uint16_t processed[30]{};
  if (stark_read_input_registers(handle, slave_id, 4200, 30, processed) != 0) {
    std::cerr << "processed touch register read failed\n";
    modbus_close(handle);
    return 1;
  }

  constexpr std::array<const char *, 5> names{
      "thumb", "index", "middle", "ring", "pinky"};
  for (std::size_t i = 0; i < names.size(); ++i) {
    const uint16_t status = processed[25 + i];
    const uint32_t proximity =
        (static_cast<uint32_t>(processed[15 + i * 2]) << 16) |
        processed[16 + i * 2];
    std::cout << names[i]
              << " normal=" << processed[i * 3]
              << " tangential=" << processed[i * 3 + 1]
              << " direction=" << processed[i * 3 + 2]
              << " proximity=" << proximity
              << " sequence=" << (status >> 8)
              << " status_bits=" << (status & 0xff) << '\n';
  }

  uint16_t raw_registers[60]{};
  if (stark_read_input_registers(handle, slave_id, 4300, 60, raw_registers) != 0) {
    std::cerr << "raw touch register read failed\n";
    modbus_close(handle);
    return 1;
  }
  for (std::size_t finger = 0; finger < names.size(); ++finger) {
    std::cout << names[finger] << " raw=";
    for (std::size_t channel = 0; channel < 6; ++channel) {
      const std::size_t offset = finger * 12 + channel * 2;
      const uint32_t value =
          (static_cast<uint32_t>(raw_registers[offset]) << 16) |
          raw_registers[offset + 1];
      std::cout << (channel == 0 ? "" : ",") << value;
    }
    std::cout << '\n';
  }

  modbus_close(handle);
  return 0;
}
