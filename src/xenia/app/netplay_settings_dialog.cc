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

DECLARE_bool(xhttp);

DECLARE_int32(network_mode);

namespace xe {
namespace app {

namespace {

constexpr int kApiAddressBufSize = 256;

const char* kNetworkModes[] = {"Offline", "Systemlink", "Xbox Live"};
constexpr int kNetworkModeCount = 3;

}  // namespace

void NetplaySettingsDialog::OnDraw(ImGuiIO& io) {
  if (!initialized_) {
    xe::string_util::copy_truncating(
        api_address_buf_, cvars::api_address.c_str(), kApiAddressBufSize);
    selected_network_mode_ = cvars::network_mode;
    if (selected_network_mode_ < 0 ||
        selected_network_mode_ >= kNetworkModeCount) {
      selected_network_mode_ = 0;
    }
    xhttp_ = cvars::xhttp;
    initialized_ = true;
  }

  auto* xbl_api = kernel_state()->GetXboxLiveAPI();

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

  // XHttp
  ImGui::Spacing();
  ImGui::Checkbox("Enable XHttp (HTTP stubs)", &xhttp_);
  xbl_api->SetXHttp(xhttp_);
  cvars::xhttp = xhttp_;

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
