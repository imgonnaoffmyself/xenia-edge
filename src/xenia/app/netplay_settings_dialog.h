#ifndef XENIA_APP_NETPLAY_SETTINGS_DIALOG_H_
#define XENIA_APP_NETPLAY_SETTINGS_DIALOG_H_

#include <string>

#include "xenia/ui/imgui_dialog.h"

namespace xe {
namespace app {

// User-visible configuration dialog for xenia-webservices online play. Lets the
// user set the web service API address and the network mode used by the XNet
// stack. Backed by cvars so values persist in the emulator config.
class NetplaySettingsDialog final : public ui::ImGuiDialog {
 public:
  explicit NetplaySettingsDialog(ImGuiDrawer* imgui_drawer)
      : ui::ImGuiDialog(imgui_drawer) {}

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  char api_address_buf_[256] = {};
  int selected_network_mode_ = 0;  // 0 = Offline, 1 = Systemlink, 2 = XboxLive
  bool xhttp_ = false;
  bool initialized_ = false;
};

}  // namespace app
}  // namespace xe

#endif  // XENIA_APP_NETPLAY_SETTINGS_DIALOG_H_
