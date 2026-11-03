#ifndef XENIA_KERNEL_XLIVEAPI_H_
#define XENIA_KERNEL_XLIVEAPI_H_

#include <cstdint>
#include <future>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "xenia/base/byte_order.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/util/net_utils.h"

namespace xe {
namespace kernel {

class XLiveAPI {
 public:
  XLiveAPI();
  ~XLiveAPI();

  enum class InitState { Success, Failed, Pending };

  InitState GetInitState() const { return initialized_; }
  bool IsConnectedToServer() const {
    return initialized_ == InitState::Success;
  }

  void Init();
  void SetNetworkMode(uint32_t mode) const;
  bool SelectNetworkMode(uint32_t mode);
  void SetXHttp(bool state) const;

  std::string GetApiAddress();
  static std::string BuildEndpoint(std::string endpoint);

  void AddAPIAddress(std::string address) const;
  void RemoveAPIAddress(std::string address) const;
  std::vector<std::string> ParseAPIList() const;

  std::string GetDefaultLocalServer() const { return kDefaultLocalServer; }
  std::string GetDefaultPublicServer() const { return kDefaultPublicServer; }

  // HTTP helpers
  static bool Get(const std::string endpoint, response_data* chunk,
                  const uint32_t timeout = 0);
  static bool Post(const std::string endpoint, const std::string& data,
                   response_data* chunk = nullptr, const uint32_t timeout = 0);
  static bool Del(const std::string endpoint, response_data* chunk = nullptr,
                  const uint32_t timeout = 0);

  // whoami / online IP
  static sockaddr_in whoami(const std::string& endpoint);
  std::string OnlineIP_str() const;
  sockaddr_in OnlineIP() const { return online_ip_; }
  void StartWhoamiAsync();

  // Player management
  bool RegisterPlayer(const uint64_t xuid);
  response_data FindPlayer(std::string ip);

  // Session management via web services
  response_data GetTitleSessions(uint32_t title_id = 0);
  bool XSessionCreate(uint64_t sessionId, const std::string& session_json);
  bool DeleteSession(uint64_t sessionId);
  response_data SessionDetails(uint64_t sessionId);

  uint16_t GetPlayerPort() const { return 36000; }
  uint32_t GetSystemlinkID() const { return systemlink_id_; }
  void SetSystemlinkID(uint32_t id) { systemlink_id_ = id; }

  static inline std::mutex xnaddr_cache_mutex;
  static inline std::map<uint32_t, uint64_t> session_id_cache;
  static inline std::map<uint32_t, MacAddress> mac_address_cache;

 private:
  static constexpr const char* kDefaultLocalServer = "192.168.0.1:36000/";
  static constexpr const char* kDefaultPublicServer =
      "https://xenia-netplay-2a0298c0e3f4.herokuapp.com/";

  InitState initialized_ = InitState::Pending;
  uint32_t systemlink_id_ = 0;
  sockaddr_in online_ip_ = {};
  std::future<sockaddr_in> whoami_result_;
};

}  // namespace kernel
}  // namespace xe

#endif  // XENIA_KERNEL_XLIVEAPI_H_
