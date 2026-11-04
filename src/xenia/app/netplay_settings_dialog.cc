#include "xenia/app/netplay_settings_dialog.h"

#include <cstring>
#include <string>

#include "third_party/imgui/imgui.h"
#include "xenia/base/cvar.h"
#include "xenia/base/logging.h"
#include "xenia/base/string_util.h"
#include "xenia/kernel/XLiveAPI.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/util/net_utils.h"
#include "xenia/kernel/util/shim_utils.h"

DECLARE_string(api_address);
DECLARE_string(api_list);
DECLARE_bool(bind_interface);
DECLARE_bool(log_mask_ips);
DECLARE_int32(network_mode);
DECLARE_bool(stub_xlivebase);
DECLARE_bool(upnp);
DECLARE_string(upnp_root);
DECLARE_int32(discord_presence_user_index);
DECLARE_string(friends_xuids);
DECLARE_string(network_interface);
DECLARE_bool(xstorage_backend);
DECLARE_bool(xstorage_user_data_backend);
DECLARE_bool(xhttp);

namespace xe {
namespace app {

namespace {

constexpr int kApiAddressBufSize = 256;
constexpr int kFriendsXuidsBufSize = 256;
constexpr int kNetworkInterfaceBufSize = 64;
constexpr int kFindPlayerIpBufSize = 64;

const char* kNetworkModes[] = {"Offline", "Systemlink", "Xbox Live"};
constexpr int kNetworkModeCount = 3;

// Minimal JSON value extraction: finds "key":"value" or "key":number and
// returns the value string. Returns false if not found.
bool JsonGetString(const std::string& json, const std::string& key,
                   std::string* out) {
  std::string search = "\"" + key + "\":\"";
  size_t pos = json.find(search);
  if (pos == std::string::npos) {
    return false;
  }
  size_t start = pos + search.size();
  size_t end = json.find('"', start);
  if (end == std::string::npos) {
    return false;
  }
  *out = json.substr(start, end - start);
  return true;
}

bool JsonGetNumber(const std::string& json, const std::string& key,
                   int64_t* out) {
  std::string search = "\"" + key + "\":";
  size_t pos = json.find(search);
  if (pos == std::string::npos) {
    return false;
  }
  size_t start = pos + search.size();
  size_t end = json.find_first_of(",} \n", start);
  if (end == std::string::npos) {
    end = json.size();
  }
  try {
    *out = std::stoll(json.substr(start, end - start));
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace

void NetplaySettingsDialog::OnDraw(ImGuiIO& io) {
  if (!initialized_) {
    xe::string_util::copy_truncating(
        api_address_buf_, cvars::api_address.c_str(), kApiAddressBufSize);
    xe::string_util::copy_truncating(
        api_list_buf_, cvars::api_list.c_str(), kApiAddressBufSize);
    xe::string_util::copy_truncating(
        friends_xuids_buf_, cvars::friends_xuids.c_str(), kFriendsXuidsBufSize);
    xe::string_util::copy_truncating(
        network_interface_buf_, cvars::network_interface.c_str(),
        kNetworkInterfaceBufSize);
    xe::string_util::copy_truncating(
        upnp_root_buf_, cvars::upnp_root.c_str(), kApiAddressBufSize);
    selected_network_mode_ = cvars::network_mode;
    if (selected_network_mode_ < 0 ||
        selected_network_mode_ >= kNetworkModeCount) {
      selected_network_mode_ = 0;
    }
    xhttp_ = cvars::xhttp;
    bind_interface_ = cvars::bind_interface;
    stub_xlivebase_ = cvars::stub_xlivebase;
    upnp_ = cvars::upnp;
    log_mask_ips_ = cvars::log_mask_ips;
    discord_presence_user_index_ = cvars::discord_presence_user_index;
    xstorage_backend_ = cvars::xstorage_backend;
    xstorage_user_data_backend_ = cvars::xstorage_user_data_backend;
    initialized_ = true;
  }

  auto* xbl_api = xe::kernel::kernel_state()->GetXboxLiveAPI();

  ImGui::Text("Xenia Web Services (Online Play)");
  ImGui::Separator();

  // API address
  ImGui::Text("API Address");
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  if (ImGui::InputText("##api_address", api_address_buf_, kApiAddressBufSize,
                       ImGuiInputTextFlags_EnterReturnsTrue)) {
    cvars::api_address = api_address_buf_;
  }
  ImGui::SameLine();
  if (ImGui::Button("Default")) {
    cvars::api_address = xbl_api->GetDefaultPublicServer();
    xe::string_util::copy_truncating(
        api_address_buf_, cvars::api_address.c_str(), kApiAddressBufSize);
  }
  ImGui::TextDisabled("Xenia Server Address e.g. IP:PORT");

  // API list
  ImGui::Spacing();
  ImGui::Text("API List");
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  if (ImGui::InputText("##api_list", api_list_buf_, kApiAddressBufSize,
                       ImGuiInputTextFlags_EnterReturnsTrue)) {
    cvars::api_list = api_list_buf_;
  }
  ImGui::SameLine();
  ImGui::TextDisabled("Comma delimited list URL1, URL2 (Max 10)");

  // Network mode
  ImGui::Spacing();
  ImGui::Text("Network Mode");
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  if (ImGui::Combo("##network_mode", &selected_network_mode_, kNetworkModes,
                   kNetworkModeCount)) {
    xbl_api->SelectNetworkMode(static_cast<uint32_t>(selected_network_mode_));
  }
  ImGui::SameLine();
  ImGui::TextDisabled("(Offline / LAN / XboxLive webservices)");

  // Network interface
  ImGui::Spacing();
  ImGui::Text("Network Interface");
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  if (ImGui::InputText("##network_interface", network_interface_buf_,
                       kNetworkInterfaceBufSize,
                       ImGuiInputTextFlags_EnterReturnsTrue)) {
    cvars::network_interface = network_interface_buf_;
  }
  ImGui::SameLine();
  ImGui::TextDisabled("Useful for network Tunnels/VPNs e.g. XLink Kai.");

  // Bind interface
  ImGui::Spacing();
  ImGui::Checkbox("Bind Interface", &bind_interface_);
  cvars::bind_interface = bind_interface_;
  ImGui::SameLine();
  ImGui::TextDisabled("Useful for network Tunnels/VPNs e.g. XLink Kai.");

  // UPnP
  ImGui::Spacing();
  ImGui::Checkbox("UPnP", &upnp_);
  cvars::upnp = upnp_;
  ImGui::SameLine();
  ImGui::TextDisabled("Automatically port forward using UPnP");

  // UPnP root
  ImGui::Spacing();
  ImGui::Text("UPnP Root");
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  if (ImGui::InputText("##upnp_root", upnp_root_buf_, kApiAddressBufSize,
                       ImGuiInputTextFlags_EnterReturnsTrue)) {
    cvars::upnp_root = upnp_root_buf_;
  }
  ImGui::SameLine();
  ImGui::TextDisabled("UPnP Root Device");

  // Stub XLiveBase
  ImGui::Spacing();
  ImGui::Checkbox("Stub XLiveBase", &stub_xlivebase_);
  cvars::stub_xlivebase = stub_xlivebase_;
  ImGui::SameLine();
  ImGui::TextDisabled("Return success for all unimplemented XLiveBase calls.");

  // XHttp
  ImGui::Spacing();
  ImGui::Checkbox("Enable XHttp (HTTP stubs)", &xhttp_);
  xbl_api->SetXHttp(xhttp_);
  cvars::xhttp = xhttp_;

  // Log mask IPs
  ImGui::Spacing();
  ImGui::Checkbox("Log Mask IPs", &log_mask_ips_);
  cvars::log_mask_ips = log_mask_ips_;
  ImGui::SameLine();
  ImGui::TextDisabled("Do not include P2P IPs inside the log");

  // Friends XUIDs
  ImGui::Spacing();
  ImGui::Text("Friends XUIDs");
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  if (ImGui::InputText("##friends_xuids", friends_xuids_buf_,
                       kFriendsXuidsBufSize,
                       ImGuiInputTextFlags_EnterReturnsTrue)) {
    cvars::friends_xuids = friends_xuids_buf_;
  }
  ImGui::SameLine();
  ImGui::TextDisabled("Comma delimited list of XUIDs. (Max 100)");

  // Discord presence user index
  ImGui::Spacing();
  ImGui::Text("Discord Presence User Index");
  ImGui::SetNextItemWidth(100.0f);
  if (ImGui::InputInt("##discord_presence_user_index",
                      &discord_presence_user_index_)) {
    if (discord_presence_user_index_ < 0)
      discord_presence_user_index_ = 0;
    if (discord_presence_user_index_ > 3)
      discord_presence_user_index_ = 3;
    cvars::discord_presence_user_index = discord_presence_user_index_;
  }
  ImGui::SameLine();
  ImGui::TextDisabled("User profile index used for Discord rich presence [0, 3].");

  // XStorage backend
  ImGui::Spacing();
  ImGui::Checkbox("XStorage Backend", &xstorage_backend_);
  cvars::xstorage_backend = xstorage_backend_;
  ImGui::SameLine();
  ImGui::TextDisabled("Request XStorage content from backend and fallback locally.");

  // XStorage user data backend
  ImGui::Spacing();
  ImGui::Checkbox("XStorage User Data Backend",
                  &xstorage_user_data_backend_);
  cvars::xstorage_user_data_backend = xstorage_user_data_backend_;
  ImGui::SameLine();
  ImGui::TextDisabled("Store user data on backend (not recommended).");

  ImGui::Spacing();
  ImGui::Separator();

  // Online status
  ImGui::Text("Online Status");
  ImGui::SameLine();
  if (xbl_api) {
    bool connected = xbl_api->IsConnectedToServer();
    ImGui::TextColored(connected ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
                       connected ? "Connected to server" : "Not connected");
  } else {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "XLiveAPI not available");
  }

  ImGui::Spacing();
  ImGui::Separator();

  // --- Session browse / join / find player ---
  ImGui::Text("Session Management");
  ImGui::Separator();

  // Online IP (resolved via whoami)
  if (xbl_api) {
    std::string online_ip = xbl_api->OnlineIP_str();
    ImGui::Text("Online IP: %s", online_ip.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(resolved via whoami)");
  }

  // Find Player by IP
  ImGui::Spacing();
  ImGui::Text("Find Player");
  ImGui::SetNextItemWidth(200.0f);
  static char find_player_ip_buf[kFindPlayerIpBufSize] = {};
  ImGui::InputText("##find_player_ip", find_player_ip_buf, kFindPlayerIpBufSize);
  ImGui::SameLine();
  if (ImGui::Button("Find Player")) {
    if (xbl_api && strlen(find_player_ip_buf) > 0) {
      auto result = xbl_api->FindPlayer(find_player_ip_buf);
      find_player_result_.clear();
      if (result.response && result.size > 0) {
        find_player_result_.assign(result.response, result.size);
      } else {
        find_player_result_ = "No player found or request failed.";
      }
      free(result.response);
    }
  }
  if (!find_player_result_.empty()) {
    ImGui::TextWrapped("Result: %s", find_player_result_.c_str());
  }

  // Browse Sessions
  ImGui::Spacing();
  if (ImGui::Button("Browse Sessions")) {
    if (xbl_api) {
      uint32_t title_id = xe::kernel::kernel_state()->title_id();
      auto result = xbl_api->GetTitleSessions(title_id);
      session_list_result_.clear();
      if (result.response && result.size > 0) {
        session_list_result_.assign(result.response, result.size);
      } else {
        session_list_result_ = "No sessions found or request failed.";
      }
      free(result.response);
    }
  }
  ImGui::SameLine();
  ImGui::TextDisabled("Search for active sessions on the web service");

  if (!session_list_result_.empty()) {
    ImGui::TextWrapped("Sessions: %s", session_list_result_.c_str());
  }

  // Session Details
  ImGui::Spacing();
  ImGui::Text("Session Details");
  static char session_id_buf[32] = {};
  ImGui::SetNextItemWidth(200.0f);
  ImGui::InputText("##session_id", session_id_buf, IM_ARRAYSIZE(session_id_buf));
  ImGui::SameLine();
  if (ImGui::Button("Get Details")) {
    if (xbl_api && strlen(session_id_buf) > 0) {
      uint64_t session_id = 0;
      bool session_id_valid = true;
      try {
        session_id = std::stoull(session_id_buf);
      } catch (...) {
        session_details_result_ = "Invalid session ID.";
        session_id_valid = false;
      }
      if (session_id_valid) {
        auto result = xbl_api->SessionDetails(session_id);
        session_details_result_.clear();
        if (result.response && result.size > 0) {
          session_details_result_.assign(result.response, result.size);
        } else {
          session_details_result_ = "Session not found or request failed.";
        }
        free(result.response);
      }
    }
  }
  if (!session_details_result_.empty()) {
    ImGui::TextWrapped("Details: %s", session_details_result_.c_str());
  }

  // Delete Session
  ImGui::Spacing();
  static char delete_session_id_buf[32] = {};
  ImGui::SetNextItemWidth(200.0f);
  ImGui::InputText("##delete_session_id", delete_session_id_buf,
                   IM_ARRAYSIZE(delete_session_id_buf));
  ImGui::SameLine();
  if (ImGui::Button("Delete Session")) {
    if (xbl_api && strlen(delete_session_id_buf) > 0) {
      uint64_t session_id = 0;
      bool session_id_valid = true;
      try {
        session_id = std::stoull(delete_session_id_buf);
      } catch (...) {
        delete_session_result_ = "Invalid session ID.";
        session_id_valid = false;
      }
      if (session_id_valid) {
        bool ok = xbl_api->DeleteSession(session_id);
        delete_session_result_ =
            ok ? "Session deleted successfully." : "Failed to delete session.";
      }
    }
  }
  if (!delete_session_result_.empty()) {
    ImGui::TextWrapped("%s", delete_session_result_.c_str());
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::TextWrapped(
      "Set the API Address to your xenia-webservices server URL "
      "(e.g. https://xenia-netplay-....herokuapp.com), choose a network "
      "mode, then launch a title to use online play via web services.");

  if (ImGui::Button("Close")) {
    Close();
  }
}

}  // namespace app
}  // namespace xe
