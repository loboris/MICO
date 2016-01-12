#include "mico.h"
#include "mico_app_define.h"
#include "Common.h"

struct  _hapCharacteristic_t acc_info_characteristics[5] =
{
  [0] = {
    .type = "23",
    .valueType = ValueType_string,
    .secureRead = true,
    .hasEvents = false,
  },
  [1] = { //public.hap.characteristic.manufacturer
    .type = "20",
    .valueType = ValueType_string,
    .hasStaticValue = true,
    .value.stringValue = MANUFACTURER,
    .secureRead = true,
    .hasEvents = false,
  },
  [2] = { //public.hap.characteristic.serial-number
    .type = "30",
    .valueType = ValueType_string,
    .hasStaticValue = true,
    .value.stringValue = SERIAL_NUMBER,
    .secureRead = true,
    .hasEvents = false,
  },  
  [3] = { //public.hap.characteristic.model
    .type = "21",
    .valueType = ValueType_string,
    .hasStaticValue = true,
    .value.stringValue = MODEL,
    .secureRead = true,
    .hasEvents = false,
  },
  [4] = { //public.hap.characteristic.identify 
    .type = "14",
    .valueType = ValueType_null,
    .hasStaticValue = true,
    .value = NULL,
    .secureRead = false,
    .secureWrite = true,
    .hasEvents = false,
  } 
};

struct  _hapCharacteristic_t lightbulb_characteristics[5] =
{
  [0] = { //public.hap.characteristic.on
    .type = "25",
    .valueType = ValueType_bool,
    .secureRead = true,
    .secureWrite = true,
    .hasEvents = true,
  },
  [1] = { //public.hap.characteristic.brightness
    .type = "8",
    .valueType = ValueType_int,
    .secureRead = true,
    .secureWrite = true,
    .hasEvents = true,
    // .hasMinimumValue = true,
    // .minimumValue = 0,
    // .hasMaximumValue = true,
    // .maximumValue = 100,
    // .hasMinimumStep = true,
    // .minimumStep = 1,
    // .unit = "percentage",
  },
  [2] = { //public.hap.characteristic.hue
    .type = "13",
    .valueType = ValueType_float,
    .secureRead = true,
    .secureWrite = true,
    .hasEvents = true,
    // .hasMinimumValue = true,
    // .minimumValue.floatValue = 0,
    // .hasMaximumValue = true,
    // .maximumValue.floatValue = 360,
    // .hasMinimumStep = true,
    // .minimumStep.floatValue = 1,
    // .unit = "arcdegrees",
  }, 
  [3] = { //public.hap.characteristic.saturation
    .type = "2F",
    .valueType = ValueType_float,
    .secureRead = true,
    .secureWrite = true,
    .hasEvents = true,
    // .hasMinimumValue = true,
    // .minimumValue.floatValue = 0,
    // .hasMaximumValue = true,
    // .maximumValue.floatValue = 100,
    // .hasMinimumStep = true,
    // .minimumStep.floatValue = 1,
    // .unit = "percentage",
  }, 
  [4] = { //public.hap.characteristic.name
    .type = "23",
    .valueType = ValueType_string,
    .hasStaticValue = true,
    .value.stringValue = "lightbulb",
    .secureRead = true,
    .secureWrite = false,
    .hasEvents = false,
  }
};

struct  _hapCharacteristic_t fan_characteristics[2] =
{
  [0] = { //public.hap.characteristic.on
    .type = "25",
    .valueType = ValueType_bool,
    .secureRead = true,
    .secureWrite = true,
    .hasEvents = true,
  },
  [1] = { //public.hap.characteristic.name
    .type = "23",
    .valueType = ValueType_string,
    .hasStaticValue = true,
    .value.stringValue = "DC motor",
    .secureRead = true,
    .secureWrite = false,
    .hasEvents = false,
  }
};

struct  _hapCharacteristic_t temp_characteristics[2] =
{
  [0] = { //public.hap.characteristic.temperature.current
    .type = "11",
    .valueType = ValueType_float,
    .secureRead = true,
    .secureWrite = false,
    .hasEvents = true,
    // .hasMinimumValue = true,
    // .minimumValue.floatValue = 0,
    // .hasMaximumValue = true,
    // .maximumValue.floatValue = 100,
    // .hasMinimumStep = true,
    // .minimumStep.floatValue = 0.1,
    // .unit = "celsius",
  },
  [1] = { //public.hap.characteristic.name
    .type = "23",
    .valueType = ValueType_string,
    .hasStaticValue = true,
    .value.stringValue = "temperature",
    .secureRead = true,
    .secureWrite = false,
    .hasEvents = false,
  }
};

struct  _hapCharacteristic_t humidity_characteristics[2] =
{
  [0] = { //public.hap.characteristic.relative-humidity.current
    .type = "10",
    .valueType = ValueType_float,
    .secureRead = true,
    .secureWrite = false,
    .hasEvents = true,
    // .hasMinimumValue = true,
    // .minimumValue.floatValue = 0,
    // .hasMaximumValue = true,
    // .maximumValue.floatValue = 100,
    // .hasMinimumStep = true,
    // .minimumStep.floatValue = 1,
    // .unit = "percentage",
  },
  [1] = { //public.hap.characteristic.name
    .type = "23",
    .valueType = ValueType_string,
    .hasStaticValue = true,
    .value.stringValue = "humidity",
    .secureRead = true,
    .secureWrite = false,
    .hasEvents = false,
  }
};

struct  _hapCharacteristic_t occupancy_characteristics[2] =
{
  [0] = { //public.hap.characteristic.relative-humidity.current
    .type = "71",
    .valueType = ValueType_int,
    .secureRead = true,
    .secureWrite = false,
    .hasEvents = true,
    // .hasMinimumValue = true,
    // .minimumValue.intValue = 0,
    // .hasMaximumValue = true,
    // .maximumValue.intValue = 1,
    // .hasMinimumStep = true,
    // .minimumStep.intValue = 1,
  },
  [1] = { //public.hap.characteristic.name
    .type = "23",
    .valueType = ValueType_string,
    .hasStaticValue = true,
    .value.stringValue = "Infra red",
    .secureRead = true,
    .secureWrite = false,
    .hasEvents = false,
  }
};

struct _hapService_t micokit_demo_services[6] = 
{
  [0] = { //public.hap.service.accessory-information
    .type = "3E",
    .num_of_characteristics = 5,
    .characteristic = acc_info_characteristics,
  },
  [1] = { //public.hap.service.lightbulb
          .type = "43",
          .num_of_characteristics = 5,
          .characteristic = lightbulb_characteristics,
        },
  [2] = { //public.hap.service.fan
          .type = "40",
          .num_of_characteristics = 2,
          .characteristic = fan_characteristics,
        },
  [3] = { //public.hap.service.sensor.temperature
          .type = "8A",
          .num_of_characteristics = 2,
          .characteristic = temp_characteristics,
        },
  [4] = { //public.hap.service.sensor.humidity
          .type = "82",
          .num_of_characteristics = 2,
          .characteristic = humidity_characteristics,
        },
  [5] = { //public.hap.service.sensor.occupancy
          .type = "86",
          .num_of_characteristics = 2,
          .characteristic = occupancy_characteristics,
        },
};

struct _hapAccessory_t demo_accs = 
{
  .num_of_services = 6,
  .services = micokit_demo_services,
};

struct _hapProduct_t hap_product = 
{
  .num_of_accessories = 1,
  .accessories = &demo_accs,
};



