#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include "process.h"

#include <filesystem>
#include <chrono>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <boost/asio/ssl/context.hpp>

#include <Simple-Web-Server/server_http.hpp>
#include <Simple-Web-Server/server_https.hpp>
#include <boost/asio/ssl/context_base.hpp>

#include "config.h"
#include "crypto.h"
#include "httpcommon.h"
#include "main.h"
#include "network.h"
#include "nvhttp.h"
#include "platform/common.h"
#include "rtsp.h"
#include "utility.h"
#include "uuid.h"
#include "sync.h"

namespace http {
using namespace std::literals;
namespace fs = std::filesystem;
namespace pt = boost::property_tree;

int reload_user_creds(const std::string &file);
bool user_creds_exist(const std::string &file);

std::string unique_id;
net::net_e origin_pin_allowed;
net::net_e origin_web_ui_allowed;

static util::sync_t<server_status_t> server_status;

int init() {
  bool clean_slate      = config::sunshine.flags[config::flag::FRESH_STATE];
  origin_pin_allowed    = net::from_enum_string(config::nvhttp.origin_pin_allowed);
  origin_web_ui_allowed = net::from_enum_string(config::nvhttp.origin_web_ui_allowed);

  if(clean_slate) {
    unique_id           = util::uuid_t::generate().string();
    auto dir            = std::filesystem::temp_directory_path() / "Sushine"sv;
    config::nvhttp.cert = (dir / ("cert-"s + unique_id)).string();
    config::nvhttp.pkey = (dir / ("pkey-"s + unique_id)).string();
  }

  if(!fs::exists(config::nvhttp.pkey) || !fs::exists(config::nvhttp.cert)) {
    if(create_creds(config::nvhttp.pkey, config::nvhttp.cert)) {
      return -1;
    }
  }
  if(user_creds_exist(config::sunshine.credentials_file)) {
    if(reload_user_creds(config::sunshine.credentials_file)) return -1;
  } else {
    BOOST_LOG(info) << "Open the Web UI to set your new username and password and getting started";
  }

  auto lg = server_status.lock();
  server_status->session_last_active_ts = std::chrono::steady_clock::now();

  return 0;
}

int save_user_creds(const std::string &file, const std::string &username, const std::string &password, bool run_our_mouth) {
  pt::ptree outputTree;

  if(fs::exists(file)) {
    try {
      pt::read_json(file, outputTree);
    }
    catch(std::exception &e) {
      BOOST_LOG(error) << "Couldn't read user credentials: "sv << e.what();
      return -1;
    }
  }

  auto salt = crypto::rand_alphabet(16);
  outputTree.put("username", username);
  outputTree.put("salt", salt);
  outputTree.put("password", util::hex(crypto::hash(password + salt)).to_string());
  try {
    pt::write_json(file, outputTree);
  }
  catch(std::exception &e) {
    BOOST_LOG(error) << "generating user credentials: "sv << e.what();
    return -1;
  }

  BOOST_LOG(info) << "New credentials have been created"sv;
  return 0;
}

bool user_creds_exist(const std::string &file) {
  if(!fs::exists(file)) {
    return false;
  }

  pt::ptree inputTree;
  try {
    pt::read_json(file, inputTree);
    return inputTree.find("username") != inputTree.not_found() &&
           inputTree.find("password") != inputTree.not_found() &&
           inputTree.find("salt") != inputTree.not_found();
  }
  catch(std::exception &e) {
    BOOST_LOG(error) << "validating user credentials: "sv << e.what();
  }

  return false;
}

int reload_user_creds(const std::string &file) {
  pt::ptree inputTree;
  try {
    pt::read_json(file, inputTree);
    config::sunshine.username = inputTree.get<std::string>("username");
    config::sunshine.password = inputTree.get<std::string>("password");
    config::sunshine.salt     = inputTree.get<std::string>("salt");
  }
  catch(std::exception &e) {
    BOOST_LOG(error) << "loading user credentials: "sv << e.what();
    return -1;
  }
  return 0;
}

int create_creds(const std::string &pkey, const std::string &cert) {
  fs::path pkey_path = pkey;
  fs::path cert_path = cert;

  auto creds = crypto::gen_creds("Sunshine Gamestream Host"sv, 2048);

  auto pkey_dir = pkey_path;
  auto cert_dir = cert_path;
  pkey_dir.remove_filename();
  cert_dir.remove_filename();

  std::error_code err_code {};
  fs::create_directories(pkey_dir, err_code);
  if(err_code) {
    BOOST_LOG(error) << "Couldn't create directory ["sv << pkey_dir << "] :"sv << err_code.message();
    return -1;
  }

  fs::create_directories(cert_dir, err_code);
  if(err_code) {
    BOOST_LOG(error) << "Couldn't create directory ["sv << cert_dir << "] :"sv << err_code.message();
    return -1;
  }

  if(write_file(pkey.c_str(), creds.pkey)) {
    BOOST_LOG(error) << "Couldn't open ["sv << config::nvhttp.pkey << ']';
    return -1;
  }

  if(write_file(cert.c_str(), creds.x509)) {
    BOOST_LOG(error) << "Couldn't open ["sv << config::nvhttp.cert << ']';
    return -1;
  }

  fs::permissions(pkey_path,
    fs::perms::owner_read | fs::perms::owner_write,
    fs::perm_options::replace, err_code);

  if(err_code) {
    BOOST_LOG(error) << "Couldn't change permissions of ["sv << config::nvhttp.pkey << "] :"sv << err_code.message();
    return -1;
  }

  fs::permissions(cert_path,
    fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read | fs::perms::owner_write,
    fs::perm_options::replace, err_code);

  if(err_code) {
    BOOST_LOG(error) << "Couldn't change permissions of ["sv << config::nvhttp.cert << "] :"sv << err_code.message();
    return -1;
  }

  return 0;
}

void set_gamepad_device_path(const std::string& device_path) {
  auto lg = server_status.lock();
  server_status->gamepad_device_path = device_path;
}

void set_process_started() {
  auto lg = server_status.lock();
  server_status->process_started = true;
}

void set_process_stopped() {
  auto lg = server_status.lock();
  server_status->process_started = false;
}

void set_session_started() {
  auto lg = server_status.lock();
  server_status->session_num += 1;
  server_status->session_last_active_ts = std::chrono::steady_clock::now();
}

void set_session_active() {
  auto lg = server_status.lock();
  server_status->session_last_active_ts = std::chrono::steady_clock::now();
}

void set_session_stopped() {
  auto lg = server_status.lock();
  server_status->session_num -= 1;
}

server_status_t get_server_status() {
  auto lg = server_status.lock();
  auto ret = *server_status;
  return ret;
}

} // namespace http