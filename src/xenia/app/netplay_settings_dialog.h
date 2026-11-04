#ifndef XENIA_APP_NETPLAY_SETTINGS_DIALOG_H_
#define XENIA_APP_NETPLAY_SETTINGS_DIALOG_H_

#include <string>

#include "xenia/ui/imgui_dialog.h"
#include "xenia/ui/imgui_drawer.h"

namespace xe {
namespace ui {

// Forward declaration in case imgui_drawer.h hasn't been fully processed yet.
class ImGuiDrawer;

}  // namespace ui

namespace app {

// User-visible configuration dialog for xenia-webservices online play. Lets the
// user set the web service API address and the network mode used by the XNet
// stack. Backed by cvars so values persist in the emulator config.
class NetplaySettingsDialog final : public ui::ImGuiDialog {
 public:
  explicit NetplaySettingsDialog(ui::ImGuiDrawer* imgui_drawer)
      : ui::ImGuiDialog(imgui_drawer) {}

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  char api_address_buf_[256] = {};
  char api_list_buf_[256] = {};
  char friends_xuids_buf_[256] = {};
  char network_interface_buf_[64] = {};
  char upnp_root_buf_[256] = {};
  int selected_network_mode_ = 0;  // 0 = Offline, 1 = Systemlink, 2 = XboxLive
  int discord_presence_user_index_ = 0;
  bool xhttp_ = false;
  bool bind_interface_ = false;
  bool stub_xlivebase_ = true;
  bool upnp_ = true;
  bool log_mask_ips_ = true;
  bool xstorage_backend_ = false;
  bool xstorage_user_data_backend_ = false;
  bool initialized_ = false;

  // Async web service call results
  std::string find_player_result_;
  std::string session_list_result_;
  std::string session_details_result_;
  std::string delete_session_result_;
};

}  // namespace app
}  // namespace xe

#endif  // XENIA_APP_NETPLAY_SETTINGS_DIALOG_H_
