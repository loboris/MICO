/**
 * sitewhere.h - Library for interacting with SiteWhere using Google Protocol Buffers.
 * See https://developers.google.com/protocol-buffers/ for details on protocol buffers.
 * The lightweight C implementation was generated with nanopb (http://code.google.com/p/nanopb/).
 *
 * Copyright 2013-2014 Reveal Technologies LLC.
 */
#ifndef sitewhere_h
#define sitewhere_h
#include "sitewhere-arduino.pb.h"
#include "Mico.h"
//#include "Arduino.h"//weiruan

#ifdef __cplusplus
extern "C" {
#endif

/** Create an encoded registration message for sending to SiteWhere */
unsigned int sw_register(char* hardwareId, char* specificationToken, uint8_t* buffer, size_t length,
		char* originator);

/** Create an encoded acknowledgement message for sending to SiteWhere */
unsigned int sw_acknowledge(char* hardwareId, char* message, uint8_t* buffer, size_t length, char* originator);

/** Create an encoded measurement message for sending to SiteWhere */
unsigned int sw_measurement(char* hardwareId,device_func *cmd,uint8_t count, int64_t eventDate,uint8_t* buffer, size_t length, char* originator);

/** Create an encoded location message for sending to SiteWhere */
unsigned int sw_location(char* hardwareId, float lat, float lon, float elevation, int64_t eventDate,
		uint8_t* buffer, size_t length, char* originator);

/** Create an encoded alert message for sending to SiteWhere */
unsigned int sw_alert(char* hardwareId, char* type, char* message, int64_t eventDate,
		uint8_t* buffer, size_t length, char* originator);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
