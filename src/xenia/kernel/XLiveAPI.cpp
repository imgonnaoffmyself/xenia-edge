// clang-format off
#include <cstdio>
#include <cstring>

#include "xenia/base/platform.h"

#ifdef XE_PLATFORM_WIN32
// NOTE: must be included after platform.h (which includes windows.h) to avoid
// conflicts between winsock.h and winsock2.h.
#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
#include <winsock2.h>  // NOLINT(build/include_order)
#endif

#include "third_party/libcurl/include/curl/curl.h"
// clang-format on

#include "xenia/kernel/XLiveAPI.h"
#include "xenia/base/cvar.h"
#include "xenia/base/logging.h"
#include "xenia/base/string_util.h"
#include "xenia/emulator.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/util/net_utils.h"
#include "xenia/kernel/util/shim_utils.h"
#include "xenia/kernel/xboxkrnl/xboxkrnl_error.h"

DEFINE_string(api_address, "", "Xenia Web Services API address", "Live");
DEFINE_string(api_list, "", "Comma delimited list of API URLs", "Live");
DEFINE_bool(log_mask_ips, true, "Do not include P2P IPs inside the log",
            "Live");
DEFINE_int32(network_mode, 0,
             "Network mode types: 0 - Offline, 1 - Systemlink, 2 - Xbox Live.",
             "Live");
DEFINE_bool(bind_interface, false,
            "Useful for network Tunnels/VPNs e.g. XLink Kai.", "Live");
DEFINE_bool(xhttp, false, "Toggles XHTTP.", "Live");
DEFINE_bool(stub_xlivebase, true, "Return success for all unimplemented XLiveBase calls.", "Live");
DEFINE_bool(upnp, true, "Automatically port forward using UPnP", "Live");
DEFINE_string(upnp_root, "", "UPnP Root Device", "Live");
DEFINE_int32(discord_presence_user_index, 0, "User profile index used for Discord rich presence [0, 3].", "Live");
DEFINE_string(friends_xuids, "", "Comma delimited list of XUIDs. (Max 100)", "Live");
DEFINE_string(network_interface, "", "Network Interface Identifier", "Live");
DEFINE_bool(xstorage_backend, false, "Request XStorage content from backend and fallback locally", "Live");
DEFINE_bool(xstorage_user_data_backend, false, "Store user data on backend (not recommended), otherwise fallback locally", "Live");

namespace xe {
namespace kernel {

namespace {

std::string JoinStrings(const std::vector<std::string>& parts,
                        const std::string& delim) {
  std::string result;
  for (size_t i = 0; i < parts.size(); i++) {
    if (i) {
      result += delim;
    }
    result += parts[i];
  }
  return result;
}

std::vector<std::string> SplitString(const std::string& s, char delim) {
  std::vector<std::string> result;
  std::string current;
  for (char c : s) {
    if (c == delim) {
      if (!current.empty()) {
        result.push_back(current);
        current.clear();
      }
    } else {
      current += c;
    }
  }
  if (!current.empty()) {
    result.push_back(current);
  }
  return result;
}

// Performs a libcurl request with the given HTTP method. |data| is the request
// body (may be empty). |chunk| receives the response body when non-null.
// Returns true if the request completed and the HTTP code indicates success.
bool CurlRequest(CURL* curl_handle, const std::string& method,
                 const std::string& endpoint, const std::string& data,
                 response_data* chunk, const uint32_t timeout) {
  if (chunk) {
    chunk->response = nullptr;
    chunk->size = 0;
    chunk->http_code = 0;
  }

  if (!curl_handle) {
    XELOGE("XLiveAPI: Cannot initialize CURL");
    return false;
  }

  XELOGI("XLiveAPI:{}: Endpoint: {}", method, endpoint);

  curl_slist* headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "Accept: application/json");
  headers = curl_slist_append(headers, "charset: utf-8");
  headers = curl_slist_append(headers, "User-Agent: xenia");

  if (timeout > 0) {
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, timeout);
  }

  curl_easy_setopt(curl_handle, CURLOPT_URL, endpoint.c_str());
  curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, method.c_str());
  curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);

  if (!data.empty() &&
      (method == "POST" || method == "PUT" || method == "PATCH")) {
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, data.size());
  }

  if (chunk) {
    curl_easy_setopt(
        curl_handle, CURLOPT_WRITEFUNCTION,
        [](void* data_ptr, size_t size, size_t nmemb, void* clientp) -> size_t {
          size_t realsize = size * nmemb;
          struct response_data* mem = (struct response_data*)clientp;
          char* ptr = (char*)realloc(mem->response, mem->size + realsize + 1);
          if (ptr == NULL) {
            return 0;
          }
          mem->response = ptr;
          memcpy(&(mem->response[mem->size]), data_ptr, realsize);
          mem->size += realsize;
          mem->response[mem->size] = 0;
          return realsize;
        });
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)chunk);
  }

  CURLcode result = curl_easy_perform(curl_handle);

  if (result != CURLE_OK) {
    XELOGE("XLiveAPI:{}: CURL Error Code: {}", method,
           static_cast<uint32_t>(result));
    curl_easy_cleanup(curl_handle);
    curl_slist_free_all(headers);
    return false;
  }

  long http_code = 0;
  result = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
  if (chunk) {
    chunk->http_code = static_cast<uint64_t>(http_code);
  }

  curl_easy_cleanup(curl_handle);
  curl_slist_free_all(headers);

  bool ok = result == CURLE_OK &&
            (http_code == 200 || http_code == 201 || http_code == 204);

  if (!ok) {
    XELOGE("XLiveAPI:{}: Failed! HTTP Error Code: {}", method, http_code);
  }

  return ok;
}

}  // namespace

XLiveAPI::XLiveAPI() {
  if (cvars::network_mode == 0) {
    initialized_ = InitState::Failed;
  }
}

XLiveAPI::~XLiveAPI() {}

void XLiveAPI::Init() {
  if (GetInitState() != InitState::Pending) {
    return;
  }

  response_data chunk = {};
  const std::string endpoint = BuildEndpoint("");
  bool ok = Get(endpoint, &chunk, 5000);

  if (ok) {
    initialized_ = InitState::Success;
    cvars::network_mode = 2;
    StartWhoamiAsync();
  } else {
    initialized_ = InitState::Failed;
    cvars::network_mode = 1;
  }

  XELOGI("XLiveAPI: init state={}, network_mode={}",
         static_cast<int>(initialized_), cvars::network_mode);
}

void XLiveAPI::SetNetworkMode(uint32_t mode) const {
  cvars::network_mode = static_cast<int32_t>(mode);
}

bool XLiveAPI::SelectNetworkMode(uint32_t mode) {
  if (mode == 0) {
    cvars::network_mode = 0;
    initialized_ = InitState::Failed;
    return true;
  }

  if (mode == 2) {
    Init();
  } else {
    cvars::network_mode = 1;
  }

  return cvars::network_mode == static_cast<int32_t>(mode);
}

void XLiveAPI::SetXHttp(bool state) const { cvars::xhttp = state; }

std::string XLiveAPI::GetApiAddress() {
  if (cvars::api_address.empty()) {
    cvars::api_address = kDefaultLocalServer;
  }
  if (cvars::api_address.back() != '/') {
    cvars::api_address.push_back('/');
  }
  return cvars::api_address;
}

std::string XLiveAPI::BuildEndpoint(std::string endpoint) {
  return GetApiAddress() + endpoint;
}

void XLiveAPI::AddAPIAddress(std::string address) const {
  if (address.back() != '/') {
    address.push_back('/');
  }

  std::vector<std::string> api_addresses = ParseAPIList();

  auto it = std::find(api_addresses.begin(), api_addresses.end(), address);
  if (it == api_addresses.end()) {
    api_addresses.push_back(address);
    cvars::api_list = JoinStrings(api_addresses, ",");
  }
}

void XLiveAPI::RemoveAPIAddress(std::string address) const {
  std::vector<std::string> api_addresses = ParseAPIList();

  auto it = std::find(api_addresses.begin(), api_addresses.end(), address);
  if (it != api_addresses.end()) {
    api_addresses.erase(it);
    cvars::api_list = JoinStrings(api_addresses, ",");
  }
}

std::vector<std::string> XLiveAPI::ParseAPIList() const {
  if (cvars::api_list.empty()) {
    cvars::api_list = std::string(kDefaultPublicServer) + ",";
  }

  return SplitString(cvars::api_list, ',');
}

bool XLiveAPI::Get(const std::string endpoint, response_data* chunk,
                   const uint32_t timeout) {
  CURL* curl_handle = curl_easy_init();
  return CurlRequest(curl_handle, "GET", endpoint, "", chunk, timeout);
}

bool XLiveAPI::Post(const std::string endpoint, const std::string& data,
                    response_data* chunk, const uint32_t timeout) {
  CURL* curl_handle = curl_easy_init();
  return CurlRequest(curl_handle, "POST", endpoint, data, chunk, timeout);
}

bool XLiveAPI::Del(const std::string endpoint, response_data* chunk,
                   const uint32_t timeout) {
  CURL* curl_handle = curl_easy_init();
  return CurlRequest(curl_handle, "DELETE", endpoint, "", chunk, timeout);
}

void XLiveAPI::WaitForWhoami() {
  if (whoami_result_.valid()) {
    online_ip_ = whoami_result_.get();
  }
}

std::string XLiveAPI::OnlineIP_str() {
  WaitForWhoami();
  return ip_to_string(online_ip_);
}

sockaddr_in XLiveAPI::whoami(const std::string& endpoint) {
  sockaddr_in addr = {};
  response_data chunk = {};
  if (!Get(endpoint, &chunk, 5000)) {
    return addr;
  }

  // Minimal JSON parsing: look for "ip" and "port" fields in the response.
  std::string body(chunk.response ? chunk.response : "", chunk.size);
  free(chunk.response);

  // Find "ip":"x.x.x.x"
  size_t ip_pos = body.find("\"ip\"");
  if (ip_pos != std::string::npos) {
    size_t colon = body.find(':', ip_pos);
    size_t quote = body.find('"', colon + 1);
    size_t end_quote = body.find('"', quote + 1);
    if (quote != std::string::npos && end_quote != std::string::npos) {
      std::string ip_str = body.substr(quote + 1, end_quote - quote - 1);
      addr = ip_to_sockaddr(ip_str);
    }
  }

  // Find "port":36000
  size_t port_pos = body.find("\"port\"");
  if (port_pos != std::string::npos) {
    size_t colon = body.find(':', port_pos);
    if (colon != std::string::npos) {
      std::string port_str = body.substr(colon + 1);
      // Trim to first non-digit
      size_t end = port_str.find_first_of(",} \n");
      if (end != std::string::npos) {
        port_str = port_str.substr(0, end);
      }
      try {
        addr.sin_port =
            xe::byte_swap(static_cast<uint16_t>(std::stoi(port_str)));
      } catch (...) {
        addr.sin_port = xe::byte_swap(static_cast<uint16_t>(36000));
      }
    }
  }

  return addr;
}

void XLiveAPI::StartWhoamiAsync() {
  whoami_result_ = std::async(
      std::launch::async, [this]() { return whoami(BuildEndpoint("whoami")); });
}

bool XLiveAPI::RegisterPlayer(const uint64_t xuid) {
  // Minimal player registration payload.
  std::string json = "{\"xuid\":" + std::to_string(xuid) + "}";
  response_data chunk = {};
  return Post(BuildEndpoint("players"), json, &chunk, 5000);
}

response_data XLiveAPI::FindPlayer(std::string ip) {
  response_data chunk = {};
  std::string endpoint = BuildEndpoint("players/find?ip=" + ip);
  Get(endpoint, &chunk, 5000);
  return chunk;
}

namespace {

std::string FormatSessionSearchEndpoint(const std::string& api_address,
                                        uint32_t title_id) {
  char title_buf[9];
  snprintf(title_buf, sizeof(title_buf), "%08X", title_id);
  return api_address + "title/" + std::string(title_buf) + "/sessions/search";
}

std::string FormatSessionCreateEndpoint(const std::string& api_address,
                                        uint32_t title_id) {
  char title_buf[9];
  snprintf(title_buf, sizeof(title_buf), "%08X", title_id);
  return api_address + "title/" + std::string(title_buf) + "/sessions";
}

std::string FormatSessionEndpoint(const std::string& api_address,
                                  uint32_t title_id, uint64_t session_id,
                                  const char* suffix = "") {
  char title_buf[9];
  char session_buf[17];
  snprintf(title_buf, sizeof(title_buf), "%08X", title_id);
  snprintf(session_buf, sizeof(session_buf), "%016llx",
           static_cast<unsigned long long>(session_id));
  std::string endpoint = api_address + "title/" + std::string(title_buf) +
                         "/sessions/" + std::string(session_buf);
  if (suffix && suffix[0] != '\0') {
    endpoint += suffix;
  }
  return endpoint;
}

}  // namespace

response_data XLiveAPI::GetTitleSessions(uint32_t title_id) {
  response_data chunk = {};
  std::string endpoint = FormatSessionSearchEndpoint(GetApiAddress(), title_id);
  Get(endpoint, &chunk, 5000);
  return chunk;
}

bool XLiveAPI::XSessionCreate(uint64_t sessionId,
                              const std::string& session_json) {
  std::string endpoint =
      FormatSessionCreateEndpoint(GetApiAddress(), kernel_state()->title_id());
  response_data chunk = {};
  return Post(endpoint, session_json, &chunk, 5000);
}

bool XLiveAPI::DeleteSession(uint64_t sessionId) {
  std::string endpoint = FormatSessionEndpoint(
      GetApiAddress(), kernel_state()->title_id(), sessionId);
  response_data chunk = {};
  return Del(endpoint, &chunk, 5000);
}

response_data XLiveAPI::SessionDetails(uint64_t sessionId) {
  response_data chunk = {};
  std::string endpoint = FormatSessionEndpoint(
      GetApiAddress(), kernel_state()->title_id(), sessionId, "/details");
  Get(endpoint, &chunk, 5000);
  return chunk;
}

}  // namespace kernel
}  // namespace xe
