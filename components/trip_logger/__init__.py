import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sd_mmc_card
from esphome.components.sensor import Sensor
from esphome.const import CONF_ID
from esphome.core import ID

DEPENDENCIES = ["sd_mmc_card", "gps", "wifi"]

trip_logger_ns = cg.esphome_ns.namespace("trip_logger")
TripLogger = trip_logger_ns.class_("TripLogger", cg.Component)

CONF_SD_CARD_ID = "sd_card_id"
CONF_GPS_LAT_ID = "gps_lat_id"
CONF_GPS_LON_ID = "gps_lon_id"
CONF_GPS_SPEED_ID = "gps_speed_id"
CONF_GPS_ALT_ID = "gps_alt_id"
CONF_GPS_COURSE_ID = "gps_course_id"
CONF_MOTION_THRESHOLD = "motion_threshold"
CONF_STOP_TIMEOUT = "stop_timeout"
CONF_LOG_INTERVAL = "log_interval"
CONF_WIFI_SSID = "wifi_ssid"
CONF_WIFI_PASSWORD = "wifi_password"
CONF_UPLOAD_URL = "upload_url"
CONF_DEVICE_ID = "device_id"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(TripLogger),
    cv.Required(CONF_SD_CARD_ID): cv.use_id(sd_mmc_card.SdMmc),
    cv.Required(CONF_GPS_LAT_ID): cv.use_id(Sensor),
    cv.Required(CONF_GPS_LON_ID): cv.use_id(Sensor),
    cv.Required(CONF_GPS_SPEED_ID): cv.use_id(Sensor),
    cv.Required(CONF_GPS_ALT_ID): cv.use_id(Sensor),
    cv.Required(CONF_GPS_COURSE_ID): cv.use_id(Sensor),
    cv.Optional(CONF_MOTION_THRESHOLD, default=0.6): cv.float_,
    cv.Optional(CONF_STOP_TIMEOUT, default="120s"): cv.positive_time_period_seconds,
    cv.Optional(CONF_LOG_INTERVAL, default="3s"): cv.positive_time_period_seconds,
    cv.Required(CONF_WIFI_SSID): cv.string,
    cv.Required(CONF_WIFI_PASSWORD): cv.string,
    cv.Required(CONF_UPLOAD_URL): cv.string,
    cv.Required(CONF_DEVICE_ID): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    sd = await cg.get_variable(config[CONF_SD_CARD_ID])
    cg.add(var.set_sd_card(sd))

    lat = await cg.get_variable(config[CONF_GPS_LAT_ID])
    cg.add(var.set_gps_lat(lat))
    # ... repeat for other sensors ...

    cg.add(var.set_motion_threshold(config[CONF_MOTION_THRESHOLD]))
    cg.add(var.set_stop_timeout(config[CONF_STOP_TIMEOUT]))
    cg.add(var.set_log_interval(config[CONF_LOG_INTERVAL]))
    cg.add(var.set_wifi_ssid(config[CONF_WIFI_SSID]))
    cg.add(var.set_wifi_password(config[CONF_WIFI_PASSWORD]))
    cg.add(var.set_upload_url(config[CONF_UPLOAD_URL]))
    cg.add(var.set_device_id(config[CONF_DEVICE_ID]))