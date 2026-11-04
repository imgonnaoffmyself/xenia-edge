#include "xenia/app/netplay_settings_dialog.h"

#include <string>

#include "third_party/imgui/imgui.h"
#include "xenia/base/cvar.h"
#include "xenia/base/logging.h"
#include "xenia/base/string_util.h"
#include "xenia/kernel/XLiveAPI.h"
#include "xenia/kernel/kernel_state.h"
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

const char* kNetworkModes[] = {"Offline", "Systemlink", "Xbox Live"};
constexpr int kNetworkModeCount = 3;

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
  ImGui::Checkbox("XStorage User Data Backend", &xstorage_user_data_backend_);
  cvars::xstorage_user_data_backend = xstorage_user_data_backend_;
  ImGui::SameLine();
  ImGui::TextDisabled("Store user data on backend (not recommended).");

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
