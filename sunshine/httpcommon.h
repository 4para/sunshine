#include "network.h"
#include "thread_safe.h"

namespace http {

int init();
int create_creds(const std::string &pkey, const std::string &cert);
int save_user_creds(
  const std::string &file,
  const std::string &username,
  const std::string &password,
  bool run_our_mouth = false);

int reload_user_creds(const std::string &file);
extern std::string unique_id;
extern net::net_e origin_pin_allowed;
extern net::net_e origin_web_ui_allowed;

struct server_status_t {
  std::string gamepad_device_path = "";
  int session_num = 0;
  std::chrono::steady_clock::time_point session_last_active_ts;
  bool process_started = false;
};

void set_gamepad_device_path(const std::string& device_path);

void set_process_started();

void set_process_stopped();

void set_session_started();

void set_session_active();

void set_session_stopped();

server_status_t get_server_status();

} // namespace http