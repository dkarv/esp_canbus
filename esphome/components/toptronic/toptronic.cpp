#include "toptronic.h"
#include "esphome/core/log.h"

#include <sstream>
#include <string>
namespace esphome {
namespace toptronic {

static const char *const TAG = "tt";

uint32_t TopTronicSensorBase::get_device_id() {
    return (device_type_ << 8) | device_addr_;
}

uint32_t build_can_id(uint8_t message_id, uint8_t priority, uint8_t device_type, uint8_t device_id) {
    return (message_id << 24) | (priority << 16) | (device_type << 8) | device_id;
}

std::vector<uint8_t> build_get_request(uint8_t function_group, uint8_t function_number, uint32_t datapoint) {
    return {
        0x01,   // message length
        0x40,   // GET_REQUEST = 0x40
        function_group,
        function_number,
        (uint8_t)(datapoint >> 8),
        (uint8_t)(datapoint),
    };
}

std::vector<uint8_t> build_set_request(uint8_t function_group, uint8_t function_number, uint32_t datapoint, std::vector<uint8_t> value) {
    std::vector<uint8_t> data = {
        0x01,   // message length
        0x46,   // SET_REQUEST = 0x46
        function_group,
        function_number,
        (uint8_t)(datapoint >> 8),
        (uint8_t)(datapoint),
    };

    for (int i = 0; i < value.size(); i++) {
        data.push_back(value[i]);
    }
    
    return data;
}

uint32_t TopTronicSensorBase::get_id() {
    return function_group_
        + (function_number_ << 8)
        + (datapoint_ << 16);
}

std::vector<uint8_t> TopTronicSensorBase::get_request_data() {
    return build_get_request(function_group_, function_number_, datapoint_);
}

template <typename T> 
T bytesToInt(std::vector<uint8_t> value) {
    T a = 0;
    for(int i = 0; i < value.size(); i++) {
        a += value[i] << 8*i;
    }
    return a;
}

float TopTronicSensor::parse_value(std::vector<uint8_t> value) {
    float res = 0.0f;
    switch(type_) {
        case U8:
            {
                res = (float)bytesToInt<uint8_t>(value);
                break;
            }
        case U16:
            {
                res = (float)bytesToInt<uint16_t>(value);
                break;
            }
        case U32:
            {
                res = (float)bytesToInt<uint32_t>(value);
                break;
            }
        case S8:
            {
                res = (float)bytesToInt<int8_t>(value);
                break;
            }
        case S16:
            {
                res = (float)bytesToInt<int16_t>(value);
                break;
            }
        case S32:
            {
                res = (float)bytesToInt<int32_t>(value);
                break;
            }
        case S64:
            {
                res = (float)bytesToInt<int64_t>(value);
                break;
            }
            
            
    }
    return res;
}

std::string TopTronicTextSensor::parse_value(std::vector<uint8_t> value) {
    int16_t intValue = bytesToInt<int16_t>(value);
    return toText_[intValue];
}

TopTronicDevice* TopTronic::get_or_create_device(uint32_t device_id) {
    if (devices_.count(device_id) <= 0) {
        devices_[device_id] = new TopTronicDevice();
    }
    return devices_[device_id];
}

void TopTronic::add_sensor(TopTronicSensorBase *sensor) {
    TopTronicDevice *device = get_or_create_device(sensor->get_device_id());
    device->sensors[sensor->get_id()] = sensor;
}

void TopTronic::get_sensors() {
    for (const auto &d : devices_) {
        auto device = d.second;
        uint8_t message_id = 30;
        for (const auto &s : device->sensors) {
            auto sensor = s.second;
            uint32_t can_id = build_can_id(message_id, 200, 50, 50);
            canbus_->send_data(can_id, true, sensor->get_request_data());
            message_id += 1;
        }
    }
}

void TopTronic::setup() {
    get_sensors();
}

void TopTronic::loop() {

}

void TopTronic::dump_config() {

}

void TopTronic::parse_frame(std::vector<uint8_t> data, uint32_t can_id, bool remote_transmission_request) {
    // check if operation is of type RESPONSE
    ESP_LOGD(TAG, "FRAME_START");
    ESP_LOGD(TAG, "can_id: 0x%X", can_id);
    
    if (data[1] != 0x42) {
        ESP_LOGD(TAG, "ignoring: no response frame");
        return;
    }

    uint32_t device_id = can_id & 0xFFFF;

    if (devices_.count(device_id) <= 0) {
        ESP_LOGD(TAG, "ignoring: unknown device 0x%X", device_id);
        return;
    }
    TopTronicDevice *device = devices_[device_id];   

    // check if sensor exists for the received value
    uint32_t datapoint = data[5] + (data[4] << 8);
    uint32_t id = data[2]
        + (data[3] << 8)
        + (datapoint << 16);

    if (device->sensors.count(id) <= 0) {
        ESP_LOGD(TAG, "ignoring: unknown sensor 0x%X", id);
        return;
    }
    TopTronicSensorBase *sensorBase = device->sensors[id];

    if (sensorBase->sensor_type() == SENSOR) {
        TopTronicSensor *sensor = (TopTronicSensor*)sensorBase;
        float value = sensor->parse_value(std::vector<uint8_t>(data.begin() + 6, data.end()));
        ESP_LOGD(TAG, "publish %f", value);
        sensor->publish_state(value);
    } else if (sensorBase->sensor_type() == TEXTSENSOR) {
        TopTronicTextSensor *sensor = (TopTronicTextSensor*)sensorBase;
        std::string value = sensor->parse_value(std::vector<uint8_t>(data.begin() + 6, data.end()));
        sensor->publish_state(value);
    }
}

}  // namespace toptronic
}  // namespace esphome