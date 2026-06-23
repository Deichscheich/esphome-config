#include "trip_logger.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <sys/time.h>
#include <HTTPClient.h>
#include <WiFi.h>

namespace esphome {
namespace trip_logger {

static const char *TAG = "trip_logger";

void TripLogger::setup() {
  ESP_LOGI(TAG, "TripLogger boot. State: %d", (int)state_);

  if (is_moving_) {
    start_trip();
  } else {
    enter_upload();
  }
}

void TripLogger::loop() {
  uint32_t now = millis();

  switch (state_) {
    // ── TRACKING: log GPS points at interval ──────────────────
    case TRACKING: {
      if (now - last_log_ >= log_interval_ * 1000) {
        log_point();
        last_log_ = now;
      }

      float speed = gps_speed_->has_state() ? gps_speed_->get_raw_value() : 0.0f;
      if (speed < 0.5f) {
        if (stopped_since_ == 0) stopped_since_ = now;
        if ((now - stopped_since_) / 1000 >= stop_timeout_) {
          ESP_LOGI(TAG, "Stop timeout reached, ending trip");
          end_trip();
          enter_upload();
        }
      } else {
        stopped_since_ = 0;
      }
      break;
    }

    // ── UPLOADING: connect WiFi, drain pending trip files ─────
    case UPLOADING: {
      // Try to connect to home WiFi
      if (WiFi.status() != WL_CONNECTED) {
        if (!connect_wifi()) {
          // Can't reach home WiFi — give up, go back to sleep
          // Trip files stay on SD for next attempt
          ESP_LOGW(TAG, "Home WiFi not reachable, sleeping with %d pending files",
                   pending_file_count());
          state_ = SLEEPING;
          App.schedule_deep_sleep();
          break;
        }
      }

      // WiFi connected — process all trip files
      File root = SD.open("/");
      if (!root) {
        ESP_LOGE(TAG, "Cannot open SD root");
        state_ = SLEEPING;
        App.schedule_deep_sleep();
        break;
      }

      bool all_clear = true;
      while (true) {
        File entry = root.openNextFile();
        if (!entry) break;

        std::string name = entry.name();
        entry.close();

        if (name.rfind("trip_", 0) != 0 || name.rfind(".csv") == std::string::npos)
          continue;

        TripUploadResult result = upload_trip(name);
        switch (result) {
          case UPLOAD_OK:
            SD.remove(("/" + name).c_str());
            ESP_LOGI(TAG, "Uploaded & deleted: %s", name.c_str());
            break;
          case UPLOAD_PARTIAL:
            // Some points failed — keep file, retry next wake
            all_clear = false;
            ESP_LOGW(TAG, "Partial upload: %s, keeping for retry", name.c_str());
            break;
          case UPLOAD_FAIL:
            // Server unreachable or error — abort upload loop entirely
            all_clear = false;
            ESP_LOGE(TAG, "Upload failed: %s, aborting", name.c_str());
            root.close();
            state_ = SLEEPING;
            App.schedule_deep_sleep();
            return;
        }
      }
      root.close();

      // Disconnect WiFi to save power before sleeping
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);

      if (all_clear) {
        ESP_LOGI(TAG, "All trips uploaded, SD clean");
      }
      state_ = SLEEPING;
      App.schedule_deep_sleep();
      break;
    }

    default:
      break;
  }
}

// ──────────────────────────────────────────────────────────────
//  WiFi CONNECT
// ──────────────────────────────────────────────────────────────

bool TripLogger::connect_wifi() {
  // Quick scan: is the home SSID even visible?
  if (!is_home_wifi()) return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid_.c_str(), wifi_password_.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < 15000) {
    delay(200);
  }

  if (WiFi.status() != WL_CONNECTED) {
    ESP_LOGW(TAG, "WiFi connect timed out");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return false;
  }

  ESP_LOGI(TAG, "WiFi connected: %s", WiFi.localIP().toString().c_str());
  return true;
}

bool TripLogger::is_home_wifi() {
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == wifi_ssid_.c_str()) {
      WiFi.scanDelete();
      return true;
    }
  }
  WiFi.scanDelete();
  return false;
}

// ──────────────────────────────────────────────────────────────
//  UPLOAD SINGLE TRIP FILE → TRACCAR (OsmAnd Protocol)
// ──────────────────────────────────────────────────────────────

TripUploadResult TripLogger::upload_trip(const std::string &filename) {
  std::string path = "/" + filename;
  File f = SD.open(path.c_str(), FILE_READ);
  if (!f) {
    ESP_LOGE(TAG, "Cannot open trip file: %s", filename.c_str());
    return UPLOAD_FAIL;
  }

  // Skip CSV header line
  String header = f.readStringUntil('\n');
  if (header.length() == 0) {
    f.close();
    return UPLOAD_OK; // Empty file? Delete it
  }

  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(8000);

  int uploaded = 0;
  int failed   = 0;
  bool server_unreachable = false;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() < 5) continue;

    float lat, lon, alt, speed, bearing;
    uint32_t ts;
    int parsed = sscanf(line.c_str(), "%f,%f,%f,%f,%f,%u",
                        &lat, &lon, &alt, &speed, &bearing, &ts);
    if (parsed < 2) {
      failed++;
      continue;
    }

    // Build OsmAnd protocol URL
    // Format: /?id=DEVICE_ID&lat=XX&lon=YY&speed=ZZ&bearing=BB&altitude=AA&timestamp=TT
    char url[512];
    int len = snprintf(url, sizeof(url),
                       "%s?id=%s&lat=%.6f&lon=%.6f",
                       upload_url_.c_str(), device_id_.c_str(), lat, lon);

    if (parsed >= 4) {
      len += snprintf(url + len, sizeof(url) - len, "&speed=%.1f", speed);
    }
    if (parsed >= 5) {
      len += snprintf(url + len, sizeof(url) - len, "&bearing=%.1f", bearing);
    }
    if (parsed >= 3) {
      len += snprintf(url + len, sizeof(url) - len, "&altitude=%.1f", alt);
    }
    len += snprintf(url + len, sizeof(url) - len, "&timestamp=%u", ts);

    http.begin(url);

    int code = http.GET();
    http.end();

    if (code == 200) {
      uploaded++;
    } else if (code < 0) {
      // Connection-level failure: server unreachable or DNS error
      server_unreachable = true;
      failed++;
      ESP_LOGW(TAG, "HTTP error code %d on point — server unreachable", code);
      break; // Stop pushing, something is wrong upstream
    } else {
      // HTTP error but connection worked (404, 500, etc.)
      failed++;
      ESP_LOGW(TAG, "HTTP %d on point upload", code);
    }

    // Small delay to avoid hammering the server
    delay(10);
  }

  f.close();

  if (server_unreachable) return UPLOAD_FAIL;
  if (uploaded > 0 && failed == 0) return UPLOAD_OK;
  if (uploaded > 0 && failed > 0) return UPLOAD_PARTIAL;
  // uploaded == 0 && failed > 0 → corrupted file or server issue
  return UPLOAD_FAIL;
}

// ──────────────────────────────────────────────────────────────
//  COUNT PENDING TRIP FILES ON SD
// ──────────────────────────────────────────────────────────────

int TripLogger::pending_file_count() {
  int count = 0;
  File root = SD.open("/");
  if (!root) return -1;

  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    std::string name = entry.name();
    entry.close();
    if (name.rfind("trip_", 0) == 0 && name.rfind(".csv") != std::string::npos)
      count++;
  }
  root.close();
  return count;
}

// ──────────────────────────────────────────────────────────────
//  MOTION CALLBACK (from QMI8658)
// ──────────────────────────────────────────────────────────────

void TripLogger::on_motion(float magnitude) {
  if (magnitude > motion_threshold_ && state_ == SLEEPING) {
    is_moving_ = true;
    // If we're in deep sleep, ESPHome's run_duration keeps us awake
    // Next loop() iteration transitions to TRACKING via setup() check
  }
}

// ──────────────────────────────────────────────────────────────
//  TRIP START / END / LOG
// ──────────────────────────────────────────────────────────────

void TripLogger::start_trip() {
  state_ = TRACKING;
  buffer_.clear();
  buffer_.reserve(30);
  last_log_ = 0;
  stopped_since_ = 0;

  time_t now;
  time(&now);
  struct tm *t = localtime(&now);
  snprintf(trip_filename_, sizeof(trip_filename_),
           "/trip_%04d%02d%02d_%02d%02d%02d.csv",
           t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
           t->tm_hour, t->tm_min, t->tm_sec);

  File f = SD.open(trip_filename_, FILE_WRITE);
  if (f) {
    f.println("lat,lon,altitude,speed,bearing,timestamp");
    f.close();
  }
  ESP_LOGI(TAG, "Trip started: %s", trip_filename_);
}

void TripLogger::log_point() {
  GpsPoint p;
  p.lat       = gps_lat_->has_state() ? gps_lat_->get_raw_value() : 0.0;
  p.lon       = gps_lon_->has_state() ? gps_lon_->get_raw_value() : 0.0;
  p.alt       = gps_alt_->has_state() ? gps_alt_->get_raw_value() : 0.0;
  p.speed     = gps_speed_->has_state() ? gps_speed_->get_raw_value() : 0.0;
  p.bearing   = gps_course_->has_state() ? gps_course_->get_raw_value() : 0.0;
  p.timestamp = millis();

  buffer_.push_back(p);

  if (buffer_.size() >= 30) flush_buffer();
}

void TripLogger::flush_buffer() {
  if (buffer_.empty()) return;
  File f = SD.open(trip_filename_, FILE_APPEND);
  if (!f) {
    ESP_LOGE(TAG, "flush_buffer: cannot open %s", trip_filename_);
    buffer_.clear();
    return;
  }
  for (auto &p : buffer_) {
    f.printf("%.6f,%.6f,%.1f,%.1f,%.1f,%u\n",
             p.lat, p.lon, p.alt, p.speed, p.bearing, p.timestamp);
  }
  f.close();
  buffer_.clear();
}

void TripLogger::end_trip() {
  flush_buffer();
  state_ = STOPPED;
  ESP_LOGI(TAG, "Trip ended: %s (%u points written)", trip_filename_, buffer_.size());
}

void TripLogger::enter_upload() {
  state_ = UPLOADING;
  ESP_LOGI(TAG, "Entering upload state");
}

void TripLogger::dump_config() {
  ESP_LOGCONFIG(TAG, "Trip Logger:");
  ESP_LOGCONFIG(TAG, "  Motion threshold: %.2f", motion_threshold_);
  ESP_LOGCONFIG(TAG, "  Stop timeout: %us", stop_timeout_);
  ESP_LOGCONFIG(TAG, "  Log interval: %us", log_interval_);
  ESP_LOGCONFIG(TAG, "  Upload URL: %s", upload_url_.c_str());
  ESP_LOGCONFIG(TAG, "  Device ID: %s", device_id_.c_str());
}

}  // namespace trip_logger
}  // namespace esphome