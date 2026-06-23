#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sd_mmc_card/sd_mmc_card.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"
#include <FS.h>
#include <SD.h>
#include <HTTPClient.h>
#include <WiFi.h>

namespace esphome {
namespace trip_logger {

struct GpsPoint {
  double lat, lon, alt, speed, bearing;
  uint32_t timestamp;
};

class TripLogger : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void on_motion(float magnitude);

  // Setters
  void set_sd_card(sd_mmc_card::SdMmc *sd) { sd_ = sd; }
  void set_gps_lat(sensor::Sensor *s) { gps_lat_ = s; }
  void set_gps_lon(sensor::Sensor *s) { gps_lon_ = s; }
  void set_gps_speed(sensor::Sensor *s) { gps_speed_ = s; }
  void set_gps_alt(sensor::Sensor *s) { gps_alt_ = s; }
  void set_gps_course(sensor::Sensor *s) { gps_course_ = s; }
  void set_motion_threshold(float t) { motion_threshold_ = t; }
  void set_stop_timeout(uint32_t t) { stop_timeout_ = t; }
  void set_log_interval(uint32_t t) { log_interval_ = t; }
  void set_wifi_ssid(const std::string &s) { wifi_ssid_ = s; }
  void set_wifi_password(const std::string &s) { wifi_password_ = s; }
  void set_upload_url(const std::string &s) { upload_url_ = s; }
  void set_device_id(const std::string &s) { device_id_ = s; }

 protected:
  enum State { SLEEPING, CHECKING, TRACKING, STOPPED, UPLOADING };

  State state_{SLEEPING};
  sd_mmc_card::SdMmc *sd_{nullptr};
  sensor::Sensor *gps_lat_{nullptr}, *gps_lon_{nullptr}, *gps_speed_{nullptr};
  sensor::Sensor *gps_alt_{nullptr}, *gps_course_{nullptr};

  float motion_threshold_{0.6f};
  uint32_t stop_timeout_{120};
  uint32_t log_interval_{3};
  std::string wifi_ssid_, wifi_password_, upload_url_, device_id_;

  // Tracking state
  uint32_t last_log_{0}, stopped_since_{0};
  char trip_filename_[48]{};
  std::vector<GpsPoint> buffer_;
  bool is_moving_{false};

  void start_trip();
  void end_trip();
  void log_point();
  void flush_buffer();
  void enter_upload();
  bool upload_trip(const std::string &filename);
  bool is_home_wifi();
};

}  // namespace trip_logger
}  // namespace esphome