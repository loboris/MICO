#include "pb.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "sitewhere.h"
#include "sitewhere-arduino.pb.h"


/****************************************************/

#include "mico.h"
#include "MicoAES.h"
#include "libemqtt.h"
#include "custom.h"
#include "micokit_ext.h"

#define baseEvents_log(M, ...) custom_log("BaseEvents", M, ##__VA_ARGS__)
#define baseEvents_log_trace() custom_log_trace("BaseEvents")

extern mqtt_broker_handle_t broker_mqtt;
//mico_Context_t *context;
/*****************************************************/
// Update these with values suitable for your network.
//byte mac[]  = { 0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xED };
//byte mqtt[] = { 192, 168, 1, 68 };
/** Callback function header */
void callback(char* topic, byte* payload, unsigned int length);

/** Connectivity */
//PubSubClient mqttClient(mqtt, 1883, callback, ethClient);

/** Message buffer */
uint8_t buffer[300];

/** Keeps up with whether we have registered */
bool registered = false;

/** Timestamp for last event */
//uint32_t lastEvent = 0;

/** MQTT client name */
char* clientName = "MiCOKit";

/** Unique hardware id for this device */
char hardwareId[HARDWARE_ID_SIZE] = {"MiCOKit-3288_000000"};

/** Device specification token for hardware configuration */
char* specificationToken = "2f5f26ed-ba66-48d7-a42f-40786b64b1f5";

/** Outbound MQTT topic */
char* outbound = "SiteWhere/input/protobuf";

/** Inbound custom command topic */
char Command1[COMMAND1_SIZE] = "SiteWhere/commands/MiCOKit-3288_000000";

/** Inbound system command topic */
char System1[SYSTEM1_SIZE] = "SiteWhere/system/MiCOKit-3288_000000";


const pb_field_t ArduinoCustom_RGB_fields[4] = {
  PB_FIELD2(  1, INT32  , REQUIRED, STATIC  , FIRST, ArduinoCustom_RGB_LED, rgbled_h, rgbled_h, 0),
  PB_FIELD2(  2, INT32  , REQUIRED, STATIC  , OTHER, ArduinoCustom_RGB_LED, rgbled_s, rgbled_h, 0),
  PB_FIELD2(  3, INT32  , REQUIRED, STATIC  , OTHER, ArduinoCustom_RGB_LED, rgbled_b, rgbled_s, 0),
  PB_LAST_FIELD
};

/** Handle the 'ping' command */
void handlePing(ArduinoCustom_ping ping, char* originator) {
  unsigned int len = 0;
  memset(buffer,0,300);
  if (len = sw_acknowledge(hardwareId, "Ping received.", buffer, sizeof(buffer), originator)) {
    mqtt_publish(&broker_mqtt,outbound,(char*)buffer,len,0);
  }
}

void handleMotor(ArduinoCustom_motor motor, char* originator) {
  unsigned int len = 0;
  memset(buffer,0,300);
  if (len = sw_acknowledge(hardwareId, "motor received.", buffer, sizeof(buffer), originator)) {
    mqtt_publish(&broker_mqtt,outbound,(char*)buffer,len,0);
  }
}

/** Handle the 'serialPrintln' command */
void handleSerialPrintln(ArduinoCustom_serialPrintln serialPrintln,char* originator) {
  unsigned int len = 0;
  memset(buffer,0,300);
  OLED_Init();
  char oled_show_line[OLED_DISPLAY_MAX_CHAR_PER_ROW+1] = {'\0'};
  baseEvents_log("%s",serialPrintln.message);
  snprintf(oled_show_line, OLED_DISPLAY_MAX_CHAR_PER_ROW+1, "%s",serialPrintln.message);
  OLED_ShowString(OLED_DISPLAY_COLUMN_START, OLED_DISPLAY_ROW_3, (uint8_t*)oled_show_line);
  if (len = sw_acknowledge(hardwareId, "Message sent to Serial.println().", buffer, sizeof(buffer), originator)) {
    mqtt_publish(&broker_mqtt,outbound,(char*)buffer,len,0);
  }
}

device_func cmdd[50];
/** Handle the 'testEvents' command */
void handleTestEvents(ArduinoCustom_testData testEvents, char* originator) {
  unsigned int len = 0;
  memset(buffer,0,300);

  memset(cmdd,0,sizeof(cmdd));
  cmdd[0].engine.temp="humidity";
  cmdd[0].engine.value = testEvents.humidity;
  
  cmdd[1].engine.temp="temperature";
  cmdd[1].engine.value = testEvents.temperature;
  
  cmdd[2].engine.temp="infrared";
  cmdd[2].engine.value = testEvents.infrared;
  
  cmdd[3].engine.temp="light";
  cmdd[3].engine.value = testEvents.light;
  
  if (len = sw_measurement(hardwareId,&cmdd[0],4,NULL, buffer, sizeof(buffer), originator)) {
    mqtt_publish(&broker_mqtt,outbound,(char*)buffer,len,0);
  }
}

ArduinoCustom_RGB_LED RGB_LED;
/** Handle a command specific to the specification for this device */
void handleSpecificationCommand(byte* payload, unsigned int length) {
  ArduinoCustom__Header header;
  
  memset(buffer,0,300);
  ArduinoCustom_testData testEvents;
  
  pb_istream_t stream = pb_istream_from_buffer(payload, length);
  if (pb_decode_delimited(&stream, ArduinoCustom__Header_fields, &header)) {
    baseEvents_log("Decoded header for custom command.");
    if (header.command == ArduinoCustom_Command_RGB_LED) {
      if (pb_decode_delimited(&stream, ArduinoCustom_RGB_fields, &RGB_LED)) {
        baseEvents_log("Command: RGB_LED set(h=%d, s=%d, b=%d)", 
                       RGB_LED.rgbled_h, RGB_LED.rgbled_s, RGB_LED.rgbled_b);
        hsb2rgb_led_open(RGB_LED.rgbled_h, RGB_LED.rgbled_s, RGB_LED.rgbled_b);
      }
    }
    else if (header.command == ArduinoCustom_Command_DC_MOTOR) {
      ArduinoCustom_DC_MOTOR dc_motor;
      if (pb_decode_delimited(&stream, ArduinoCustom__Header_fields, &dc_motor)) {
        baseEvents_log("Command: DC_MOTOR set: %d", dc_motor.motor_sw);
        dc_motor_set(dc_motor.motor_sw);
      }
    }
    else if (header.command == ArduinoCustom_Command_PING) {
      ArduinoCustom_ping ping;
      if (pb_decode_delimited(&stream, ArduinoCustom_ping_fields, &ping)) {
        handlePing(ping, header.originator);
      }
    } 
    else if (header.command == ArduinoCustom_Command_TESTEVENTS) {
      if (pb_decode_delimited(&stream, ArduinoCustom_testEvents_fields, &testEvents)) {
         handleTestEvents(testEvents, header.originator);
      }
    } 
    else if (header.command == ArduinoCustom_Command_SERIALPRINTLN) {
      ArduinoCustom_serialPrintln serialPrintln;
      if (pb_decode_delimited(&stream, ArduinoCustom_serialPrintln_fields, &serialPrintln)) {
        handleSerialPrintln(serialPrintln,header.originator);
      }
    }
    else {
      baseEvents_log("Unknown command.");
    }
  }
}

/** Handle a system command */
void handleSystemCommand(byte* payload, unsigned int length) {
  Device_Header header;
  pb_istream_t stream = pb_istream_from_buffer(payload, length);
  
  // Read header to find what type of command follows.
  if (pb_decode_delimited(&stream, Device_Header_fields, &header)) {
    
    // Handle a registration acknowledgement.
    if (header.command == Device_Command_REGISTER_ACK) {
      Device_RegistrationAck ack;
      if (pb_decode_delimited(&stream, Device_RegistrationAck_fields, &ack)) {
        if (ack.state == Device_RegistrationAckState_NEW_REGISTRATION) {
          baseEvents_log("Registered new device.");
          registered = true;
        } else if (ack.state == Device_RegistrationAckState_ALREADY_REGISTERED) {
          baseEvents_log("Device was already registered.");
          registered = true;
        } else if (ack.state == Device_RegistrationAckState_REGISTRATION_ERROR) {
          baseEvents_log("Error registering device.");
        }
      }
    }
  } else {
    baseEvents_log("Unable to decode system command.");
  }
}
