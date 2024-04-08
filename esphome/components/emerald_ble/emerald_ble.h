#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/defines.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

#ifdef USE_ESP32

#include <esp_gattc_api.h>

namespace esphome {
namespace emerald_ble {

namespace espbt = esphome::esp32_ble_tracker;

static const espbt::ESPBTUUID EMERALD_SERVICE_TIME_UUID =
    espbt::ESPBTUUID::from_raw("00001910-0000-1000-8000-00805f9b34fb");
static const espbt::ESPBTUUID EMERALD_CHARACTERISTIC_TIME_READ_UUID =
    espbt::ESPBTUUID::from_raw("00002b10-0000-1000-8000-00805f9b34fb");  // indicate, notify, read, write
static const espbt::ESPBTUUID EMERALD_CHARACTERISTIC_TIME_WRITE_UUID =
    espbt::ESPBTUUID::from_raw("00002b11-0000-1000-8000-00805f9b34fb");  // indicate, notify, read, write

static const espbt::ESPBTUUID EMERALD_BATTERY_SERVICE_UUID = espbt::ESPBTUUID::from_uint16(0x180F);
static const espbt::ESPBTUUID EMERALD_BATTERY_CHARACTERISTIC_UUID = espbt::ESPBTUUID::from_uint16(0x2A19);

// static std::string getImpulseCmd =                              "0001010500";
// static std::string getPairingCodeCmd =                          "0001030100";
// static std::string getEvery30sPowerConsumptionCmd =             "0001020306";
// static std::string getDeviceTimeCmd =                           "0001010200";
// static std::string getUpdatedPowerCmd =                         "0001020100";
// static std::string getEvery30sPowerConsumptionCmdWitninHours =  "0001021308";

// static std::string setImpulseCmd =          "0001010402";
// static std::string startGettingHistoryCmd = "0001020400";
// static std::string endGettingHistoryCmd =   "0001020600";
// static std::string setDeviceTimeCmd =       "0001010104";
// static std::string resetCmd =               "0001010a00";
// static std::string setAutoUploadStatusCmd = "0001020b01";
// static const uint8_t setAutoUploadStatusCmd = "0001020b01";
//enabled
static uint8_t setAutoUploadStatusCmd[] = {0x00,0x01,0x02,0x0b,0x01,0x01};

static const uint32_t RETURN30S_POWER_CONSUMPTION_CMD =      0x0001020a06;
static const uint32_t RETURN_UPDATED_POWER_CMD =             0x0001020204;
static const uint32_t RETURN_EVERY30S_POWER_CONSUMPTION_CMD = 0x000102050e;
static const uint32_t RETURN_IMPULSE_CMD =                  0x0001010602;
static const uint32_t RETURN_PAIRING_CODE_CMD =              0x0001030206;
static const uint32_t RETURN_DEVICE_TIME_CMD =               0x0001010304;

static const uint8_t standard_update_interval = 30;    // seconds
static const float kw_to_w_conversion = 1000.0;    // conversion ratio


class Emerald : public esphome::ble_client::BLEClientNode, public Component {
  // class Emerald : public esphome::ble_client::BLEClientNode, public PollingComponent {
 public:
  // void setup() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_battery(sensor::Sensor *battery) { battery_ = battery; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }
  void set_energy_sensor(sensor::Sensor *energy_sensor) { energy_sensor_ = energy_sensor; }
  void set_daily_energy_sensor(sensor::Sensor *daily_energy_sensor) { daily_energy_sensor_ = daily_energy_sensor; }
#ifdef USE_TIME
  void set_time(time::RealTimeClock *time) { this->time_ = time; }
#endif
  void set_pulses_per_kwh(uint16_t pulses_per_kwh) {
    pulses_per_kwh_ = pulses_per_kwh;
    pulse_multiplier_ = (standard_update_interval / (pulses_per_kwh / kw_to_w_conversion));
  }
  void set_pairing_code(uint32_t pairing_code) { pairing_code_ = pairing_code; }

 protected:
  std::string pkt_to_hex_(const uint8_t *data, uint16_t len);
  void decode_(const uint8_t *data, uint16_t length);
  void parse_battery_(const uint8_t *data, uint16_t length);
  void parse_measurement_(const uint8_t *data, uint16_t length);
  uint32_t parse_command_header_(const uint8_t *data);
  uint32_t decode_emerald_date_(const uint8_t *data);
  void decode_emerald_packet_(const uint8_t *data, uint16_t length);

  sensor::Sensor *battery_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *energy_sensor_{nullptr};
  sensor::Sensor *daily_energy_sensor_{nullptr};
#ifdef USE_TIME
  optional<time::RealTimeClock *> time_{};
#endif
  uint8_t day_of_last_measurement_{0};

  uint32_t pairing_code_;
  float pulses_per_kwh_;
  float pulse_multiplier_;
  uint64_t daily_pulses_{0};
  uint64_t total_pulses_{0};

  uint16_t time_read_char_handle_ = 0x15;
  uint16_t time_write_size_char_handle_ = 0x19;

  uint16_t battery_char_handle_ = 0x1d;
  // uint16_t firmware_char_handle_;
};

}  // namespace emerald_ble
}  // namespace esphome

#endif
