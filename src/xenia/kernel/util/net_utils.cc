/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/util/net_utils.h"

#include <cstring>
#include <limits>
#include <random>

#include "xenia/base/logging.h"
#include "xenia/base/string_util.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/util/shim_utils.h"

namespace xe {
namespace kernel {

MacAddress::MacAddress(const uint8_t* macaddress) {
  for (uint8_t i = 0; i < MacAddressSize; i++) {
    mac_address_[i] = macaddress[i];
  }
}

MacAddress::MacAddress(std::string macaddress) {
  const uint64_t mac =
      xe::byte_swap(string_util::from_string<uint64_t>(macaddress, true)) >>
      0x10;
  memcpy(mac_address_, &mac, MacAddressSize);
}

MacAddress::MacAddress(uint64_t macaddress) {
  xe::be<uint64_t> be_macaddress = (macaddress << 0x10);
  memcpy(mac_address_, &be_macaddress, MacAddressSize);
}

MacAddress::~MacAddress() {}

std::vector<uint8_t> MacAddress::to_array() const {
  std::vector<uint8_t> result = {};
  result.resize(MacAddressSize);
  memcpy(result.data(), mac_address_, MacAddressSize);
  return result;
}

uint64_t MacAddress::to_uint64() const {
  return string_util::from_string<uint64_t>(to_string(), true);
}

std::string MacAddress::to_string() const {
  std::string result = fmt::format(
      "{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}", mac_address_[0], mac_address_[1],
      mac_address_[2], mac_address_[3], mac_address_[4], mac_address_[5]);
  return result;
}

std::string MacAddress::to_printable_form() const {
  std::string mac =
      fmt::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}", mac_address_[0],
                  mac_address_[1], mac_address_[2], mac_address_[3],
                  mac_address_[4], mac_address_[5]);
  return mac;
}

std::string ip_to_string(const ::in_addr& addr) {
  char ip_str[INET_ADDRSTRLEN] = {};
  const char* result = inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN);
  return ip_str;
}

std::string ip_to_string(const ::sockaddr_in& sockaddr) {
  char ip_str[INET_ADDRSTRLEN] = {};
  const char* result =
      inet_ntop(AF_INET, &sockaddr.sin_addr, ip_str, INET_ADDRSTRLEN);
  return ip_str;
}

::sockaddr_in ip_to_sockaddr(std::string ip_str) {
  ::sockaddr_in addr{};
  int32_t result = inet_pton(AF_INET, ip_str.c_str(), &addr.sin_addr);
  return addr;
}

::in_addr ip_to_in_addr(std::string ip_str) {
  ::in_addr addr{};
  int32_t result = inet_pton(AF_INET, ip_str.c_str(), &addr.s_addr);
  return addr;
}

uint64_t GetMachineId(const uint64_t mac_address) {
  const uint64_t machine_id_mask = 0xFA00000000000000ULL;
  return machine_id_mask | mac_address;
}

uint64_t GetLocalMachineId(const MacAddress mac_address) {
  return GetMachineId(mac_address.to_uint64());
}

MacAddress GetConsoleMacAddress() {
  uint8_t mac_address[MacAddress::MacAddressSize] = {};

  if (!kernel_state() || !kernel_state()->xconfig()) {
    return MacAddress(mac_address);
  }

  kernel_state()->xconfig()->ReadSetting(
      XCONFIG_SECURED_CATEGORY, XCONFIG_SECURED_MAC_ADDRESS, &mac_address);

  return MacAddress(mac_address);
}

}  // namespace kernel
}  // namespace xe
