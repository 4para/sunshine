#ifndef SUNSHINE_CONFIG_H
#define SUNSHINE_CONFIG_H

#include <bitset>
#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace config {
struct video_t {
  // ffmpeg params
  int qp; // higher == more compression and less quality

  int hevc_mode;

  int min_threads; // Minimum number of threads/slices for CPU encoding
  struct {
    std::string preset;
    std::string tune;
  } sw;

  struct {
    std::optional<int> preset;
    std::optional<int> rc;
    int coder;
  } nv;

  struct {
    std::optional<int> quality;
    std::optional<int> rc;
    int coder;
  } amd;

  std::string encoder;
  std::string adapter_name;
  std::string output_name;
};

struct audio_t {
  std::string sink;
  std::string virtual_sink;
};

struct stream_t {
  std::chrono::milliseconds ping_timeout;

  std::string file_apps;

  int fec_percentage;

  // max unique instances of video and audio streams
  int channels;
};

struct nvhttp_t {
  // Could be any of the following values:
  // pc|lan|wan
  std::string origin_pin_allowed;
  std::string origin_web_ui_allowed;

  std::string pkey; // must be 2048 bits
  std::string cert; // must be signed with a key of 2048 bits

  std::string sunshine_name;

  std::string file_state;

  std::string external_ip;
  std::vector<std::string> resolutions;
  std::vector<int> fps;
};

struct input_t {
  std::unordered_map<int, int> keybindings;

  std::chrono::milliseconds back_button_timeout;
  std::chrono::milliseconds key_repeat_delay;
  std::chrono::duration<double> key_repeat_period;

  std::string gamepad;
  std::string gamepad_device_name;
  bool enable_mouse_passthrough = true;
  bool enable_keyboard_passthrough = true;
  bool display_cursor_by_default = true;
};

struct remote_port_t {
  int remote_rtsp;
  int remote_stream_video;
  int remote_stream_audio;
  int remote_stream_control;
};

namespace flag {
enum flag_e : std::size_t {
  PIN_STDIN = 0,              // Read PIN from stdin instead of http
  FRESH_STATE,                // Do not load or save state
  FORCE_VIDEO_HEADER_REPLACE, // force replacing headers inside video data
  UPNP,                       // Try Universal Plug 'n Play
  CONST_PIN,                  // Use "universal" pin
  FLAG_SIZE
};
}

struct sunshine_t {
  int min_log_level;
  std::bitset<flag::FLAG_SIZE> flags;
  std::string credentials_file;

  std::string username;
  std::string password;
  std::string salt;

  std::string config_file;

  struct cmd_t {
    std::string name;
    int argc;
    char **argv;
  } cmd;

  std::uint16_t port;
};

extern video_t video;
extern audio_t audio;
extern stream_t stream;
extern nvhttp_t nvhttp;
extern input_t input;
extern sunshine_t sunshine;
extern remote_port_t remote_port;

int parse(int argc, char *argv[]);
std::unordered_map<std::string, std::string> parse_config(const std::string_view &file_content);

uint16_t map_remote_port(int configured, int mapped_port);

} // namespace config
#endif
