
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/canbus/canbus.h"

#include <map>

namespace esphome {
namespace toptronic {

enum TypeName {
    U8,
    U16,
    U32,
    S8,
    S16,
    S32,
    S64,
};

enum SensorType {
    SENSOR,
    TEXTSENSOR,
};

uint32_t build_can_id(uint8_t message_id, uint8_t priority, uint8_t device_type, uint8_t device_id);
std::vector<uint8_t> build_get_request(uint8_t function_group, uint8_t function_number, uint32_t datapoint);
std::vector<uint8_t> build_set_request(uint8_t function_group, uint8_t function_number, uint32_t datapoint, std::vector<uint8_t> value);

class TopTronicSensorBase {
   public:
    void set_device_type(uint8_t device_type) { device_type_ = device_type; }
    void set_device_addr(uint8_t device_addr) { device_addr_ = device_addr; }

    void set_function_group(uint8_t function_group) { function_group_ = function_group; }
    void set_function_number(uint8_t function_number) { function_number_ = function_number; }
    void set_datapoint(uint16_t datapoint) { datapoint_ = datapoint; }

    uint32_t get_id();
    uint32_t get_device_id();
    std::vector<uint8_t> get_request_data();

    virtual SensorType sensor_type();

   protected:
    uint8_t device_type_;
    uint8_t device_addr_;

    uint8_t function_group_;
    uint8_t function_number_;
    uint16_t datapoint_;
};

class TopTronicSensor: public sensor::Sensor, public TopTronicSensorBase {
   public:
    void set_type(TypeName type) { type_ = type; }

    float parse_value(std::vector<uint8_t> value);
    SensorType sensor_type() override { return SENSOR; };

   protected:
    TypeName type_;
};

class TopTronicTextSensor: public text_sensor::TextSensor, public TopTronicSensorBase {
   public:

    std::string parse_value(std::vector<uint8_t> value);
    SensorType sensor_type() override { return TEXTSENSOR; };

    void add_option(int16_t value, std::string text) {
        toText_[value] = text;
        toValue_[text] = value;
    }

   protected:
    std::map<int16_t, std::string> toText_;
    std::map<std::string, int16_t> toValue_;
};

class TopTronicDevice {
   public:
    std::map<uint32_t, TopTronicSensorBase*> sensors;
};

class TopTronic : public Component {
   public:
    explicit TopTronic(canbus::Canbus *canbus): canbus_(canbus){};

    void add_sensor(TopTronicSensorBase *sensor);
    void parse_frame(std::vector<uint8_t> data, uint32_t can_id, bool remote_transmission_request);
    void get_sensors();

    void setup() override;
    void loop() override;
    void dump_config() override;

   protected:
    TopTronicDevice* get_or_create_device(uint32_t can_id);

    canbus::Canbus *canbus_;
    std::map<uint32_t, TopTronicDevice*> devices_;
};

}  // namespace toptronic
}  // namespace esphome
