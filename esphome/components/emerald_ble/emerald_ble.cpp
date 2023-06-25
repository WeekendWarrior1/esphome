#include "emerald_ble.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/time.h"

#ifdef USE_ESP32

namespace esphome {
namespace emerald_ble {

static const char *const TAG = "emerald_ble";

void Emerald::dump_config() {
  ESP_LOGCONFIG(TAG, "EMERALD");
  LOG_SENSOR(" ", "Battery", this->battery_);
  LOG_SENSOR(" ", "Power", this->power_sensor_);
  LOG_SENSOR(" ", "Daily Energy", this->daily_energy_sensor_);
  LOG_SENSOR(" ", "Total Energy", this->energy_sensor_);
  ESP_LOGD(TAG, "pulses_per_kwh_: %f", this->pulses_per_kwh_);
  ESP_LOGD(TAG, "pulse_multiplier_: %f", this->pulse_multiplier_);
}

// void Emerald::setup() { this->authenticated_ = false; }

std::string Emerald::pkt_to_hex_(const uint8_t *data, uint16_t len) {
  char buf[64];
  memset(buf, 0, 64);
  for (int i = 0; i < len; i++)
    sprintf(&buf[i * 2], "%02x", data[i]);
  std::string ret = buf;
  return ret;
}

void Emerald::decode_(const uint8_t *data, uint16_t length) {
  ESP_LOGD(TAG, "DEC(%d): 0x%s", length, this->pkt_to_hex_(data, length).c_str());
}

void Emerald::parse_battery_(const uint8_t *data, uint16_t length) {
  ESP_LOGD(TAG, "Battery: DEC(%d): 0x%s", length, this->pkt_to_hex_(data, length).c_str());
  if (length == 1) {
    this->battery_->publish_state(data[0]);
  }
}

uint32_t Emerald::parse_command_header_(const uint8_t *data) {
    uint32_t command_header = 0;
    for (int i = 0;  i < 5; i++) {
        command_header += (data[i] << (8*(4-i)));
    }
    return command_header;
}

uint32_t Emerald::decode_emerald_date_(const uint8_t *data) {
    uint32_t command_date_bin = 0;
    for (int i = 5;  i < 9; i++) {
        command_date_bin += (data[i] << (8*(8-i)));
    }
    // // (6 bits)year + (4 bits)month + (5 bits)days + (5 bits)hours(locale adjusted) + (6 bits)minutes + (6 bits)seconds
    // uint16_t year = 2000 + (commandDateBin >> 26);  // need to add 2000 to get the correct year
    // uint8_t month = ((commandDateBin >> 22) & 0b1111);  // month number between 1 - 12
    // uint8_t days = ((commandDateBin >> 17) & 0b11111); // 1-31
    // uint8_t hours = ((commandDateBin >> 12) & 0b11111); // 0-23
    // uint8_t minutes = ((commandDateBin >> 6) & 0b111111); // 0 -59
    // uint8_t seconds = commandDateBin & 0b111111; // 0 -59
    return command_date_bin;
}

void Emerald::decode_emerald_packet_(const uint8_t *data, uint16_t length) {
  ESP_LOGD(TAG, "DEC(%d): 0x%s", length, this->pkt_to_hex_(data, length).c_str());
  if (length >= 5) {
    uint32_t command_header = this->parse_command_header_(data);
    switch(command_header) {
      case RETURN30S_POWER_CONSUMPTION_CMD: {
        if (length != 11) {
          //return
        }

        uint16_t pulses_within_interval = data[9] << 8;
        pulses_within_interval += + data[10];

        float avg_watts_within_interval = pulses_within_interval * this->pulse_multiplier_;

        ESP_LOGI(TAG, "Timestamp: , Pulses: %d, Average Watts within interval: %f W", pulses_within_interval,
                avg_watts_within_interval);

        if (this->power_sensor_ != nullptr) {
          this->power_sensor_->publish_state(avg_watts_within_interval);
        }

        if (this->energy_sensor_ != nullptr) {
          total_pulses_ += pulses_within_interval;
          float energy = total_pulses_ / this->pulses_per_kwh_;
          this->energy_sensor_->publish_state(energy);
        }

        if (this->daily_energy_sensor_ != nullptr) {
          // even if new day, publish last measurement window before resetting
          this->daily_pulses_ += pulses_within_interval;
          float energy = this->daily_pulses_ / this->pulses_per_kwh_;
          this->daily_energy_sensor_->publish_state(energy);


          // if esphome device has a valid time component set up, use that (preferred)
          // else, use the emerald measurement timestamps
#ifdef USE_TIME
          auto *time_ = *this->time_;
          ESPTime date_of_measurement = time_->now();
          // ESPTime date_of_measurement = this->time_->now();
          if (date_of_measurement.is_valid()) {
            if (this->day_of_last_measurement_ == 0) { this->day_of_last_measurement_ = date_of_measurement.day_of_year; }
            else if (this->day_of_last_measurement_ != date_of_measurement.day_of_year) {
              this->daily_pulses_ = 0;
              this->day_of_last_measurement_ = date_of_measurement.day_of_year;
            }
          } else {
            // if !date_of_measurement.is_valid(), user may have a bare "time:" in their yaml without a specific platform selected, so fallback to date of emerald measurement
#else
            // avoid using ESPTime here so we don't need a time component in the config
            uint32_t command_date_bin = this->decode_emerald_date_(data);
            uint8_t day_of_measurement = ((command_date_bin >> 17) & 0b11111); // 1-31
            if (this->day_of_last_measurement_ == 0) { this->day_of_last_measurement_ = day_of_measurement; }
            else if (this->day_of_last_measurement_ != day_of_measurement) {
              this->daily_pulses_ = 0;
              this->day_of_last_measurement_ = day_of_measurement;
            }
#endif
#ifdef USE_TIME
          }
#endif
        }

        break;
      }
      case RETURN_UPDATED_POWER_CMD: {
        break;
      }
      case RETURN_EVERY30S_POWER_CONSUMPTION_CMD: {
        break;
      }
      case RETURN_IMPULSE_CMD: {
        break;
      }
      case RETURN_PAIRING_CODE_CMD: {
        break;
      }
      case RETURN_DEVICE_TIME_CMD: {
        break;
      }
    }
  } else {
    // is this possible? failure?
  }
}

void Emerald::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                   esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      ESP_LOGD(TAG, "[%s] ESP_GATTC_READ_CHAR_EVT (Received READ)", this->parent_->address_str().c_str());
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Error reading char at handle %d, status=%d", param->read.handle, param->read.status);
        break;
      }

      // time_read_char_handle_
      if (param->read.handle == this->time_read_char_handle_) {
        ESP_LOGD(TAG, "Recieved time read event");
        this->decode_emerald_packet_(param->read.value, param->read.value_len);
        break;
      }

      // // firmware
      // if (param->read.handle == this->firmware_char_handle_) {
      //   ESP_LOGD(TAG, "Recieved firmware read event");
      //   this->decode_(param->read.value, param->read.value_len);
      //   break;
      // }

      break;
    }

    case ESP_GATTC_WRITE_CHAR_EVT: {
      ESP_LOGD(TAG, "[%s] ESP_GATTC_WRITE_CHAR_EVT (Write confirmed)", this->parent_->address_str().c_str());
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Error writing value to char at handle %d, status=%d", param->write.handle, param->write.status);
        break;
      }

      // ESP_LOGE(TAG, "[%s] Seemed to miss any handle matches, what is the handel?: %d",
      //          this->parent_->address_str().c_str(), param->write.handle);
      break;
    }  // ESP_GATTC_WRITE_CHAR_EVT

    case ESP_GATTC_NOTIFY_EVT: {
      ESP_LOGD(TAG, "[%s] Received Notification", this->parent_->address_str().c_str());


      // time_read_char_handle_
      if (param->notify.handle == this->time_read_char_handle_) {
        ESP_LOGD(TAG, "Recieved time read notification");
        this->decode_emerald_packet_(param->notify.value, param->notify.value_len);
        break;
      }

      // battery
      if (param->notify.handle == this->battery_char_handle_) {
        ESP_LOGD(TAG, "Recieved battery notify event");
        this->parse_battery_(param->notify.value, param->notify.value_len);
        break;
      }

      // // battery
      // if (param->notify.handle == this->measurement_char_handle_) {
      //   ESP_LOGD(TAG, "Recieved measurement notify event");
      //   this->parse_measurement_(param->notify.value, param->notify.value_len);
      //   break;
      // }
      break;  // registerForNotify
    }
    default:
      break;
  }
}

void Emerald::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    // This event is sent once authentication has completed
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
      if (param->ble_security.auth_cmpl.success) {
        auto status = esp_ble_gattc_register_for_notify(this->parent_->gattc_if, this->parent_->remote_bda,
                                                            this->time_read_char_handle_);
        if (status) {
          ESP_LOGW(TAG, "[%s] esp_ble_gattc_register_for_notify failed, status=%d",
                    this->parent_->address_str().c_str(), status);
        }

        // uint8_t set_auto_upload[] = {0x00, 0x01, 0x02, 0x0b, 0x01, 0x01};
        ESP_LOGI(TAG, "[%s] Writing auto upload code to Emerald", this->parent_->address_str().c_str());
        auto write_status = esp_ble_gattc_write_char(this->parent()->gattc_if, this->parent()->conn_id,
                                               this->time_write_size_char_handle_, sizeof(setAutoUploadStatusCmd),
                                               setAutoUploadStatusCmd, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
        if (write_status) {
          ESP_LOGW(TAG, "Error sending write request for pairing_code, status=%d", write_status);
        }

        // read battery
        auto read_battery_status = esp_ble_gattc_read_char(this->parent()->gattc_if, this->parent()->conn_id,
                                                            this->battery_char_handle_, ESP_GATT_AUTH_REQ_NONE);
        if (read_battery_status) {
          ESP_LOGW(TAG, "Error sending read request for battery, status=%d", read_battery_status);
        }
        // Enable notifications for battery
        auto notify_battery_status = esp_ble_gattc_register_for_notify(
            this->parent_->gattc_if, this->parent_->remote_bda, this->battery_char_handle_);
        if (notify_battery_status) {
          ESP_LOGW(TAG, "[%s] esp_ble_gattc_register_for_notify failed, status=%d",
                    this->parent_->address_str().c_str(), notify_battery_status);
        }
      }
      break;
    }
    case ESP_GAP_BLE_PASSKEY_REQ_EVT: { /* passkey request event */
      ESP_LOGE(TAG, "ESP_GAP_BLE_PASSKEY_REQ_EVT, onPassKeyRequest %x", event);
      esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, this->pairing_code_);
      break;
    }
    default:
      break;
  }
}

}  // namespace emerald_ble
}  // namespace esphome

#endif
