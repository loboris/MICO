/**
 *
 * @file   aca.h
 * @brief  Arrayent Connect Agent (ACA) library interface.
 *
 * Copyright (c) 2014 Arrayent Inc.  Company confidential.
 * Please contact sales@arrayent.com to get permission to use in your application.
 *
 **/

/**
@mainpage

<!-- This comment block is the index page for the project documentation. To generate the
     documentation, run "doxygen Doxyfile" in the same directory containing Doxyfile.
     See http://www.doxygen.org for more information.
 -->

@section Summary
The Arrayent Connect Agent (ACA) provides connectivity for devices to the Arrayent Connect Cloud.

@section Changelog
    @li 1.5.3.0: 2014/10/08
                 Handled downstream messages more than 128 bytes.
    @li 1.5.2.0: 2014/09/12
                 Fixed potential buffer overflow situations if customer application attempts to
                 transmit over long data buffers.
                 Added state logging over IP.
                 Improved connection testing when heartbeat message is lost.
                 Added support for multi-threaded access to API functions.
    @li 1.5.1.0: 2014/08/04
                 Fixed ACA single attribute messages fail when using TCP.
                 Fixed ACA ArrayentNetStatus returns timeout when wifi router WAN port unplugged.
                 Fixed ACA does not return error code on retry failure to set property to cloud.
                 Fixed ACA stuck when cloud resets.
                 Fixed ACA encryption login sequence fails if cloud sends attribute to device 
                 during the login process.
                 Added unsigned integer data types support in ACA.
    @li 1.5.0.0: 2014/06/26
                 Added to configure message max payload length for POSIX platforms.
                 Enhance ArrayentNetStatus to disclose progress during login sequence.
                 Added AMP ID and SessionEnd ID in arrayent_net_status_t.
                 Fix ACA returns same message twice if two messages arrive before host app calls
                 ArrayentRecvXxx().
                 Improve ACA error reporting.
    @li 1.4.3.0: 2014/05/29	
                 Fix ACA intermittently adds garbage characters to end of device name or password.
    @li 1.4.2.0: 2014/05/23
                 ACA stops sending heartbeats and goes offline two minutes after restart.
    @li 1.4.1.0: 2014/04/16
                 Fix ArrayentInit() hangs when ArrayentConfigure() has not been successfully 
                 called.
                 Fix ArrayentNetStatus() is not verified prior to ArrayentSetProperty() then 
                 ArrayentSetProperty() returns ARRAYENT_SUCCESS even though the property was not 
                 updated in Utility.
    @li 1.4.0.0: 2014/03/26
                 Add aca_stack_size field to arrayent_config_t to allow application to
                 configure the stack size for ACA Gateway thread.
                 Add support of data message(200) sub-type.
                 Add ArrayentFactoryResetDevice() to reset device to factory settings.
                 Removed use of message queues for messaging between ACA daemon and ACA APIs.
                 Messages are defined as global variables protected by semaphores and mutexes.
                 Removed use of dataReaderThread.
                 Add aca_thread_priority field to arrayent_config_t to allow application to
                 configure the priority of ACA Gateway thread.
                 Optimized stack utilization by defining messages as global variables.
                 Add ArrayentSleep() and ArrayentWake() to allow application to disable/enable
                 ACA daemon.
                 Increased the sleep time in AicdThread() to 200 ms.
                 Reduced the DNS lookup frequency to (1/600) if 100 DNS lookup fails in a row.
    @li 1.3.0.0: 2014/02/14
                 Add multi-attribute message support.
                 Add ArrayentSetConfigDefaults(), so apps may upgrade ACA without changing code.
                 Require AES encryption.
                 Fix ArrayentNetStatus() advertising cloud connectivity prematurely.
                 Change arguments of ArrayentSendData() and ArrayentRecvData().
    @li 1.2.0.0: 2014/01/27
                 Add encryption support.
                 Change ACA threads' priorities to 7.
                 Automatically call ArrayentReset() after successful post-initialization call to
                 ArrayentConfigure().
                 Fix ArrayentConfigure() accepting input that could later be rejected by
                 ArrayentInit().
                 Fix main ACA thread not sleeping between loops on WICED.
    @li 1.1.2.0: 2014/01/09
                 Fix ArrayentRecvProperty() not setting its length argument to zero on receive
                 failure.
                 Add IAR ARM-compatible versions of the library to release packages.
    @li 1.1.1.0: 2013/12/16
                 Make ArrayentConfigure() take a monolithic configuration structure.
                 Null-terminate properties received using ArrayentRecvProperty().
                 Rename library interface file to aca.h.
    @li 1.1.0.1: 2013/11
                 First public beta release.

@section ACA Details
Your connected device firmware will be composed of two parts: an embedded client application and
the Arrayent Connect Agent. The embedded application refers to the embedded software that drives
your product. This part of the software is developed by your product engineering team. Your product
engineering team will integrate the Arrayent Connect Agent and the embedded client application.
This guide explains the functions available in the Arrayent Connect Agent Application Programming
Interface (the Arrayent API) for communicating with the Arrayent cloud.

Arrayent will supply the Arrayent Connect Agent and a board support package to abstract the Arrayent
Agent from the hardware-specific code. Therefore, it is important to identify the microcontroller
hardware and development toolchain early in the development process, so that Arrayent can ensure
that your hardware is supported. The Arrayent Connect Agent requires 32 to 64 KB of program
memory and approximately 15KB of RAM on a 32-bit embedded processor.

Arrayent Connect Agent's duties include:
* Logging in to the Arrayent Cloud and managing your device's session with the Arrayent cloud.
* Accepting messages from your
* Embedded Client Application and forwarding these messages into the
Arrayent cloud, and vice versa.

Messages are represented as key-value pairs. The key represents an attribute of your device, and
the value represents the current value of that attribute. For example, suppose that your device has
an embedded temperature sensor, which your device samples at a predefined interval (say every 10
minutes). To send this data into the Arrayent Cloud, your client application sends the following
message to the Arrayent agent: "temperature 84"

The Arrayent Endpoint simply forwards the message up into the cloud.
*/


#ifndef _ACA_H_
#define _ACA_H_

#include <stdint.h>


// Version of the code shipped with this header file.
#define ACA_VERSION_MAJOR       1   ///< Incremented after ten minor versions.
#define ACA_VERSION_MINOR       5   ///< Incremented on feature release.
#define ACA_VERSION_REVISION    3   ///< Incremented on bug fix.
#define ACA_VERSION_SUBREVISION 0   ///< Incremented internally.
extern const char* const aca_version_string;    ///< Global ASCII string containing ACA version.

/************************************************
 *                   CONSTANTS
 ***********************************************/
#define ARRAYENT_AES_KEY_LENGTH                 32 ///< Length of AES keys
#define ARRAYENT_NUM_LOAD_BALANCERS              3 ///< Number of load balancers
#define ARRAYENT_LOAD_BALANCER_HOSTNAME_LENGTH  36 ///< Maximum length of load balancer domain name
#define ARRAYENT_DEVICE_NAME_LENGTH             15 ///< Maximum length of device name
#define ARRAYENT_DEVICE_PASSWORD_LENGTH         10 ///< Maximum length of device password
#define ARRAYENT_MIN_STACK_SIZE                 0x300 ///< Minimum stack size of AICD thread

/************************************************
 *              ARRAYENT DATA TYPES
 ***********************************************/
/** @brief All possible return values of ACA function calls.
 */
typedef enum {
    ARRAYENT_SUCCESS = 0,   ///< Command completed successfully.
    ARRAYENT_FAILURE = -1,  ///< Command failed.
    ARRAYENT_TIMEOUT = -2,  ///< Command failed: timed out.
    ARRAYENT_BUFFER_TOO_BIG = -3,   ///< Command failed; buffer too large.
    ARRAYENT_CANNOT_MULTI_PROPERTY = -4,    ///< Command failed; device is not configured for
                                            ///< receiving multi-property messages.
    ARRAYENT_ACCEPTED_CONFIG_BUT_CANNOT_RESET = -5, ///< Reconfiguration successful, but unable to
                                                    ///< reset afterward.
    ARRAYENT_BAD_PRODUCT_ID = -100,         ///< Product ID is invalid.
    ARRAYENT_BAD_PRODUCT_AES_KEY = -101,    ///< Product AES key is invalid.
    ARRAYENT_BAD_LOAD_BALANCER_DOMAIN_NAME_1 = -102,    ///< First load balancer is invalid.
    ARRAYENT_BAD_LOAD_BALANCER_DOMAIN_NAME_2 = -103,    ///< Second load balancer is invalid.
    ARRAYENT_BAD_LOAD_BALANCER_DOMAIN_NAME_3 = -104,    ///< Third load balancer is invalid.
    ARRAYENT_BAD_LOAD_BALANCER_UDP_PORT = -105, ///< Load balancer UDP port is invalid.
    ARRAYENT_BAD_LOAD_BALANCER_TCP_PORT = -106, ///< Load balancer TCP port is invalid.
    ARRAYENT_BAD_DEVICE_NAME = -107,    ///< Device name is invalid.
    ARRAYENT_BAD_DEVICE_PASSWORD = -108,    ///< Device password is invalid.
    ARRAYENT_BAD_DEVICE_AES_KEY = -109,  ///< Device AES key is invalid.
    ARRAYENT_BAD_ACA_THREAD_PRIORITY = -110,    ///< Device ACA thread priority is invalid.
    ARRAYENT_BAD_ACA_STACK_SIZE = -111,  ///< Device ACA stack size is invalid
    ARRAYENT_BAD_CONFIGS = -112,	///< Device is not configured
    ARRAYENT_FAIL_LOCK_MUTEX_RX_CTRL = -113,     ///< Failed to get mutex lock for mutexRxCtrl
    ARRAYENT_FAIL_LOCK_MUTEX_TX_CTRL = -114,     ///< Failed to get mutex lock for mutexTxCtrl
    ARRAYENT_FAIL_LOCK_MUTEX_TX_PROP = -115,     ///< Failed to get mutex lock for mutexTxProp
    ARRAYENT_FAIL_LOCK_MUTEX_RX_PROP = -116,     ///< Failed to get mutex lock for mutexRxProp
    ARRAYENT_FAIL_PUT_SEM_TX_CTRL = -117,        ///< Failed to put semaphore for semTxReq
    ARRAYENT_FAIL_PUT_SEM_TX_PROP = -118,        ///< Failed to put semaphore for semTxReq
    ARRAYENT_MSG_DATA_TOO_LONG = -119,           ///< Data message size exceeds
    ARRAYENT_RX_CTRL_RSP_STATUS_FAILURE = -120,  ///< mRxRsp response status is failure
    ARRAYENT_BAD_MSG_TYPE = -121,                ///< Invalid msg type
    ARRAYENT_CREATE_GATEWAY_THREAD_FAIL = -122,   ///< Arrayent fail to create gateway thread
    ARRAYENT_DEAMON_NOT_INITIALIZED = -123,       ///< Arrayent deamon is not initialized
    ARRAYENT_INVALID_INPUT_ARGUMENT = -124,       ///< Arrayent invalid input arguments
    ARRAYENT_NO_GATEWAY_SESSION_EXIST = -125,     ///< Gateway session does not exist
    ARRAYENT_INVALID_RESPONSE = -126              ///< Old response received
} arrayent_return_e;

/** @brief Structure to use with @c ArrayentConfigure() to configure the ACA.
 */
typedef struct {
    uint16_t    product_id;
    const char* product_aes_key;
    const char* load_balancer_domain_names[3];
    uint16_t    load_balancer_udp_port;
    uint16_t    load_balancer_tcp_port;
    const char* device_name;
    const char* device_password;
    const char* device_aes_key;
    uint8_t     device_can_multi_attribute:1;   ///< Indicates that the device is capable of
                                                ///< receiving multi-attribute messages. Setting
                                                ///< this to 1 will cause the server to encapsulate
                                                ///< all attributes sent to this device in multi-
                                                ///< attribute messages.
                                                ///< @warn Currently unused.
    uint8_t     enable_logging:1;               ///< set to 1 to allow ACC remote enable of logging output
    uint8_t     aca_thread_priority;            ///< Indicates the thread priority at which AICD
                                                ///< thread will be created.
#ifdef HARDCODE_SWITCH
    char *switch_ip;
    int switch_port;
#endif
    uint16_t    aca_stack_size;                 ///< Indicates the stack size for AICD thread.
} arrayent_config_t;


/** @brief Arrayent sleep levels - use these with @c ArrayentSleep()
 */
typedef enum {
    ARRAYENT_MIN_SLEEP_LEVEL = 0,
    ARRAYENT_DEEPSLEEP = 0,
    ARRAYENT_MAX_SLEEP_LEVEL
} arrayent_sleep_level_e;

/** @brief Arrayent network status - returned by @c ArrayentNetStatus()
 */
typedef struct {
    uint8_t server_ip_obtained:1;   ///< Arrayent server IP has been found
    uint8_t heartbeats_ok:1;        ///< Heartbeats to the Arrayent server are fine
    uint8_t using_udp:1;            ///< Connection to Arrayent server is over UDP
    uint8_t using_tcp:1;            ///< Connection to Arrayent server is over TCP
    uint8_t connected_to_server:1;  ///< Connection to Arrayent server is okay
    uint8_t product_key_ok:1;                   ///< Product key is Okay
    uint8_t product_key_ok_username_bad:1;      ///< Product key is Okay but Username is Invalid
    uint8_t login_successful:1;                 ///< Log-in successfully to Arrayent server
    uint8_t key_exchange_successful:1;          ///< Key exchange to Arrayent server successfully
    uint16_t padding:7;
    uint32_t device_id;				///< Contains AMP-ID
    uint32_t service_endpoint_id;   ///< Contains service end point deviceID
} arrayent_net_status_t;

/** @brief Arrayent timestamp data type (output of @c ArrayentGetTime())
 */
typedef struct {
    uint16_t year;      ///< year AD
    uint16_t month;     ///< 0-11 (0 = Jan, 1 = Feb...)
    uint16_t day;       ///< 1-31
    uint16_t hour;      ///< 0-23
    uint16_t minute;    ///< 0-59
    uint16_t second;    ///< 0-59
} arrayent_timestamp_t;



/************************************************
 *              ARRAYENT FUNCTIONS
 ***********************************************/
/** Populate an ACA configuration structure with default settings.
 *
 * Use this function to populate an ACA configuration structure with default values. This
 * futureproofs your application by allowing you to upgrade your version of ACA without changing
 * any of your application code, even when the ACA configuration structure adds new features.
 *
 * This function invalidates all mandatory fields of @c config. Your application's order of events
 * should be
 *  1. Call @c ArrayentSetConfigDefaults() on configuration structure
 *  2. Populate configuration structure with mandatory values and overrides of optional values
 *  3. Call @c ArrayentConfigure() with configuration structure
 *
 * @param config[out]   ACA configuration structure to populate.
 *
 * @return              Whether or not ACA default settings were placed into @c config.
 *                      On Success: ARRAYENT_SUCCESS
 *                      On Failure: Error Code "arrayent_return_e"
 */
arrayent_return_e ArrayentSetConfigDefaults(arrayent_config_t* config);

/** Configure the Arrayent Connect Agent.
 *
 * Use this function to set all of your device's Arrayent attributes before starting the ACA.
 *
 * This function must be successfully called before calling @c ArrayentConfigure().
 *
 * If this function is called after @c ArrayentInit(), it will automatically call @c ArrayentReset()
 * if the reconfiguration structure was accepted. In the unlikely case that reconfiguration is
 * successful but resetting is not, the host application must retry resetting the ACA until it
 * succeeds before attempting any communication through the ACA.
 *
 * @param[in] config    Structure fully populated with Arrayent configuration arguments.
 *
 * @return          Indicates whether or not the configuration was accepted.
 *                  On Success: ARRAYENT_SUCCESS
 *                  On Failure: Error Code "arrayent_return_e"
 */
arrayent_return_e ArrayentConfigure(arrayent_config_t* config);


/** Start the ACA.
 *
 * This function prepares the ACA for communication with the Arrayent server.
 * Do not call this function until every Arrayent property has been configured successfully using
 * @c ArrayentConfigure(). Doing so may cause the Arrayent thread to hang.
 *
 * Note that before calling this function, the Arrayent code expects the user application to have
 * seeded the machine's random number generator using srand().
 *
 * @return    Indicates if initialization was completed successfully.
 *            On Success: ARRAYENT_SUCCESS
 *            On Failure: Error Code "arrayent_return_e"
 */
arrayent_return_e ArrayentInit(void);


/** Check connection to the ACA.
 *
 * This function sends a hello message to the ACA. If the ACA interface is functioning correctly,
 * the hello message will receive a response message from the routing daemon to indicate a working
 * connection.
 *
 * @return    Indicates if the hello was verified.
 *            On Success: ARRAYENT_SUCCESS
 *            On Failure: Error Code "arrayent_return_e"
 */
arrayent_return_e ArrayentHello(void);


/** Check Arrayent network status.
 *
 * This function returns a bit field indicating the network status of the ACA.
 *
 * @param[out]  status  Pointer to the status structure to be updated. See @c arrayent_net_status_t.
 *
 * @return    Indicates if the status was updated.
 *            On Success: ARRAYENT_SUCCESS
 *            On Failure: Error Code "arrayent_return_e"
 */
arrayent_return_e ArrayentNetStatus(arrayent_net_status_t* status);


/** Put the ACA to sleep.
 *
 * This function instructs the ACA to go to sleep until ArrayentWake() is called.
 * In this state, the application can call ArrayentConfigure() to change the configuration.
 *
 * @param[in]   sleep_level   Sets the exact sleep behavior.
 *      ARRAYENT_DEEPSLEEP:  Turns client connection state machine off.  Sockets are closed and no
 *      messages are sent to the cloud.
 *
 * @return  Indicates whether or not the ACA was successfully suspended.
 *          On Success: ARRAYENT_SUCCESS
 *          On Failure: Error Code "arrayent_return_e"
 */
arrayent_return_e ArrayentSleep(arrayent_sleep_level_e sleep_level);


/** Wake up the ACA.
 *
 * This function brings the routing daemon out of sleep, causing it to reestablish its
 * connection with the Arrayent server.
 *
 * Applications should poll ArrayentNetStatus() for success after this function has been called and
 * before sending property updates to the cloud.
 *
 * @return  Indicates whether or not the ACA is now in the process of resuming.
 *          On Success: ARRAYENT_SUCCESS
 *          On Failure: Error Code "arrayent_return_e"
 */
arrayent_return_e ArrayentWake(void);


/** Reset the ACA.
 *
 * This function forces the ACA's state machine to reset and log back in to the server. It may be
 * called at any time after initialization is complete.
 *
 * Be sure to poll @c ArrayentNetStatus() for server connection before attempting Arrayent
 * communication after a reset.
 *
 * @return    Indicates if the reset was accepted.
 *            On Success: ARRAYENT_SUCCESS
 *            On Failure: Error Code "arrayent_return_e"
 */
arrayent_return_e ArrayentReset(void);


/** Retrieve the current time from the server.
 *
 * This function blocks for up to three seconds waiting for a response from the server.
 *
 * @warn The month field of the returned timestamp is zero-based: January is represented by a 0,
 * February is represented by a 1, and so on.
 *
 * @param[out]  timestamp   Pointer to structure to fill with current time
 *
 * @param[in]   timezone    GMT offset of desired time, in hours
 *
 * @return          Indicates if the timestamp was received. If not ARRAYENT_SUCCESS, *timestamp
 *                  does NOT contain a valid timestamp.
 *                  On Success: ARRAYENT_SUCCESS
 *                  On Failure: Error Code "arrayent_return_e"
 */
arrayent_return_e ArrayentGetTime(arrayent_timestamp_t* timestamp, int16_t timezone);


/** Write a new property to the server.
 *
 * This function sets an arbitrary property on the Arrayent servers.
 *
 * @param[in]   property  Pointer to the property name.
 *
 * @warn The month field of the returned timestamp is zero-based: January is represented by a 0,
 * February is represented by a 1, and so on.
 *
 * @param[in]   value     Pointer to the property value.
 *
 * @return          Indicates if the property was set correctly.
 *                  On Success: ARRAYENT_SUCCESS
 *                  On Failure: Error Code "arrayent_return_e"
 */
arrayent_return_e ArrayentSetProperty(char* property, char* value);


/** Receive a property message.
 *
 * This function listens for a property message from the Arrayent cloud. It is blocking; it
 * will not return until either a property message has been received from the server or the
 * @c timeout argument has elapsed.
 *
 * @param[out]      data    Where to place the received property message. This buffer is not touched
 *                          if no message is received.
 *
 * @param[in,out]   len     Input: the length of the receive buffer. This buffer should be at least
 *                                 one character longer than the longest expected message, to allow
 *                                 for null-termination.
 *                          Output: if a property message was received, this number is set to the
 *                                  length of the data written to the data buffer. In the event of
 *                                  an insufficiently sized receive buffer, the received message is
 *                                  truncated to the length of the passed buffer. The value of len
 *                                  does not include the null terminator, which is always set by
 *                                  this function.
 *                                  If no property message was received, this is set to zero.
 *
 * @param[in]       timeout  Timeout for the receive operation to complete, in milliseconds.
 *                           Use 0 to indicate no wait.
 *
 * @return    Indicates if a property was received from the server.
 *            On Success: ARRAYENT_SUCCESS
 *            On Failure: Error Code "arrayent_return_e"
 */
arrayent_return_e ArrayentRecvProperty(char* data, uint16_t* len, uint32_t timeout);


/** Write multiple attributes to the server simultaneously.
 *
 * This function sends a multi-attribute message to the Arrayent servers.
 *
 * @param[in]   data    Property buffer. See documentation on Arrayent multi-attribute message
 *                      format for description of contents.
 * @param[in]   len     Length of property buffer. Currently limited by build time macro
 *                      MAX_MESSAGE_SIZE, default is about 128 bytes.
 *
 * @return    Indicates if the attributes were sent to and accepted by the server.
 *            On Success: ARRAYENT_SUCCESS
 *            On Failure: Error Code "arrayent_return_e"
 */
arrayent_return_e ArrayentSetMultiAttribute(uint8_t* data, uint16_t len);


/** Receive a multi-attribute message.
 *
 * This function listens for a multi-attribute message from the Arrayent cloud. It is blocking; it
 * will not return until either a multi-attribute message has been received from the server or the
 * @c timeout argument has elapsed.
 *
 * @warn In order to use this call, you must have specified that your device is capable of
 * receiving multi-attribute messages in your call to @c ArrayentConfigure().
 *
 * @param[out]      data    Where to place the received multi-attribute message. This buffer is
 *                          not touched if no message is received.
 *
 * @param[in,out]   len     Input: the length of the receive buffer. This buffer should be at least
 *                                 as long as the longest expected message.
 *                          Output: if a multi-attribute message was received, this number is set to
 *                                  the length of the data written to the data buffer. In the event
 *                                  of an insufficiently sized receive buffer, this value will
 *                                  equal the size of the buffer, and the received message is
 *                                  truncated to the size of the buffer.
 *                                  If no property message was received, this is set to zero.
 *
 * @param[in]       timeout  Timeout for the receive operation to complete, in milliseconds.
 *                           Use 0 to indicate no wait.
 *
 * @return    Indicates if a multi-attribute message was received from the server.
 *            On Success: ARRAYENT_SUCCESS
 *            On Failure: Error Code "arrayent_return_e"
 */
arrayent_return_e ArrayentRecvMultiAttribute(uint8_t* data, uint16_t* len, uint32_t timeout);


/** Send binary data with optional timeout.
 * @warn this function is maintained for legacy applications only.
 * 
 * Note that ArrayentSendData() is used only for custom parser applications. 
 * It is used for sending proprietary binary data to the Arrayent cloud. 
 * User must first make explicit arrangements with Arrayent Customer Support to use this, otherwise
 * the function will not work as expected.
 *
 * This function sends data messages to the Arrayent servers synchronously. It is blocking, and
 * will not return until a response message has been received from the server or the timeout value
 * expires.
 *
 * @param[in]   data    Pointer to the data for transmission.
 *
 * @param[in]   data_type	Data type of data to send
 *
 * @param[in]   len     Length of data to send
 *
 * @return    Indicates if the data was sent to the server correctly.
 *            On Success: ARRAYENT_SUCCESS
 *            On Failure: Error Code "arrayent_return_e"
 */
arrayent_return_e ArrayentSendData(uint8_t* data, uint16_t data_type, uint16_t len);


/** Receive a raw data message.
 * @warn this function is maintained for legacy applications only.
 * 
 * Note that ArrayentRecvData() is used only for custom parser applications. 
 * It is used for receiving proprietary binary data from the Arrayent cloud. 
 * User must first make explicit arrangements with Arrayent Customer Support to use this, otherwise
 * the function will not work as expected.
 *
 * This function attempts to receive a data message from the Arrayent cloud. It is blocking; it
 * will not return until either a data message has been received from the server or the
 * @c timeout argument has elapsed.
 *
 * @param[out]      data    Where to place the received data message. This buffer is not touched if
 *                          no message is received.
 *
 * @param[out]      data_type    Where to place the received data message sub-type. This buffer is
 *                               not touched if no message is received.
 *
 * @param[in,out]   len     Input: the length of the receive buffer. This buffer should be at least
 *                                 the length of the longest expected message.
 *                          Output: if a data message was received, this number is set to the
 *                                  length of the data written to the data buffer. In the event of
 *                                  an insufficiently sized receive buffer, this value will not
 *                                  equal the length of the received data.
 *                                  If no data message was received, this is set to zero.
 *
 * @param[in]       timeout  Timeout for the receive operation to complete, in milliseconds.
 *                           Use 0 to indicate no wait.
 *
 * @return    Indicates if a data message was received from the server.
 *            On Success: ARRAYENT_SUCCESS
 *            On Failure: Error Code "arrayent_return_e"
 */
arrayent_return_e ArrayentRecvData(uint8_t* data, uint16_t* data_type,
                                   uint16_t* len, uint32_t timeout);


/** Send factory reset the command.
 *
 * This function is used for reset device to factory settings.
 *
 * @return    Indicates if the factory reset was accepted.
 *            On Success: ARRAYENT_SUCCESS
 *            On Failure: Error Code "arrayent_return_e"
 */
arrayent_return_e ArrayentFactoryResetDevice(void);

/** Arrayent Debug
 *
 * wrapper function for logging module.
 *
 * @param[in]	ip address for sysLogConfiguration.
 *
 * @return
 *			On Success: ARRYENT_SUCCESS
 *			On Failure: Error COde "arrayent_return_e"
 */
arrayent_return_e ArrayentDebug(char *buf);

#endif /* _ACA_H_ */
