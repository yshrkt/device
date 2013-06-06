#include "Arduino.h"
#include <SoftwareSerial.h>
#include "BGLib.h"
#include "MemoryFree.h"
#include "pgmStrToRAM.h"

// gatt.xml allows max: 255 for service.characteristic.value.length
// but BGLib I2C fails (waiting for attributes_write response timeouts) sending 255Bytes
// looks like 254Bytes is the longest length allowed
#define BLE112_MAX_CHARACTERISTIC_VALUE_LENGTH 254
#define LED_PIN 13

// BLE112 module connections:
// - BLE P0_2 -> GND (CTS tied to ground to bypass flow control)
// - BLE P0_4 -> Arduino Digital Pin 6 (BLE TX -> Arduino soft RX)
// - BLE P0_5 -> Arduino Digital Pin 5 (BLE RX -> Arduino soft TX)

// NOTE: this demo REQUIRES the BLE112 be programmed with the UART connected
// to the "api" endpoint in hardware.xml, have "mode" set to "packet", and be
// configured for 38400 baud, 8/N/1. This may change in the future, but be
// aware. The BLE SDK archive contains an /examples/uartdemo project which is
// a good starting point for this communication, though some changes are
// required to enable packet mode and change the baud rate. The BGLib
// repository also includes a project you can use for this in the folder
// /BLEFirmware/BGLib_U1A1P_38400_noflow_wake16_hwake15.

// iMote git:8fa00b089894132e3f6906fea1009a4e53ce5834
SoftwareSerial ble112uart( 6, 5 ); // RX, TX
// create BGLib object:
//  - use SoftwareSerial por for module comms
//  - use nothing for passthrough comms (0 = null pointer)
//  - enable packet mode on API protocol since flow control is unavailable
BGLib ble112((HardwareSerial *)&ble112uart, 0, 1);

// ================================================================
// INTERNAL BGLIB CLASS CALLBACK FUNCTIONS
// ================================================================

void onBusy() {
    // turn LED on when we're busy
    digitalWrite(LED_PIN, HIGH);
}

void onIdle() {
    // turn LED off when we're no longer busy
    digitalWrite(LED_PIN, LOW);
}

void onTimeout() {
    Serial.println(P("!!!\tTimeout occurred!"));
}

void onBeforeTXCommand() {
}

void onTXCommandComplete() {
}

// ================================================================
// USER-DEFINED BGLIB RESPONSE CALLBACKS
// ================================================================

void my_rsp_system_hello(const ble_msg_system_hello_rsp_t *msg) {
    Serial.println(P("<--\tsystem_hello"));
}

void my_rsp_gap_set_scan_parameters(const ble_msg_gap_set_scan_parameters_rsp_t *msg) {
    Serial.print(P("<--\tgap_set_scan_parameters: { "));
    Serial.print(P("result: ")); Serial.print((uint16_t)msg -> result, HEX);
    Serial.println(P(" }"));
}

void my_rsp_gap_discover(const ble_msg_gap_discover_rsp_t *msg) {
    Serial.print(P("<--\tgap_discover: { "));
    Serial.print(P("result: ")); Serial.print((uint16_t)msg -> result, HEX);
    Serial.println(P(" }"));
}

void my_rsp_gap_end_procedure(const ble_msg_gap_end_procedure_rsp_t *msg) {
    Serial.print(P("<--\tgap_end_procedure: { "));
    Serial.print(P("result: ")); Serial.print((uint16_t)msg -> result, HEX);
    Serial.println(P(" }"));
}

void my_rsp_gap_set_mode(const ble_msg_gap_set_mode_rsp_t *msg) {
    Serial.print(P("<--\tgap_set_mode: { "));

    // 0x020C: Command Disallowed
    Serial.print(P("result: ")); Serial.print((uint16_t)msg -> result, HEX);

    Serial.println(P(" }"));
}

void my_rsp_attributes_write(const ble_msg_attributes_write_rsp_t *msg) {
    Serial.print(P("<--\tattributes_write: { "));
    Serial.print(P("result: ")); Serial.print((uint16_t)msg -> result, HEX);
    Serial.println(P(" }"));
}

void my_rsp_connection_get_rssi(const ble_msg_connection_get_rssi_rsp_t *msg) {
    Serial.print(P("<--\tconnection_get_rssi: { "));
    Serial.print(P("conn: "));   Serial.print((uint8)msg -> connection, HEX);
    Serial.print(P(", rssi: ")); Serial.print((uint8)msg -> rssi);
    Serial.println(P(" }"));
}

// ================================================================
// USER-DEFINED BGLIB EVENT CALLBACKS
// ================================================================

void my_evt_system_boot(const ble_msg_system_boot_evt_t *msg) {
    Serial.print(P("###\tsystem_boot: { "));
    Serial.print(P("major: ")); Serial.print(msg -> major, HEX);
    Serial.print(P(", minor: ")); Serial.print(msg -> minor, HEX);
    Serial.print(P(", patch: ")); Serial.print(msg -> patch, HEX);
    Serial.print(P(", build: ")); Serial.print(msg -> build, HEX);
    Serial.print(P(", ll_version: ")); Serial.print(msg -> ll_version, HEX);
    Serial.print(P(", protocol_version: ")); Serial.print(msg -> protocol_version, HEX);
    Serial.print(P(", hw: ")); Serial.print(msg -> hw, HEX);
    Serial.println(P(" }"));
}

void my_evt_gap_scan_response(const ble_msg_gap_scan_response_evt_t *msg) {
    Serial.print(P("###\tgap_scan_response: { "));
    Serial.print(P("rssi: ")); Serial.print(msg -> rssi);
    Serial.print(P(", packet_type: ")); Serial.print((uint8_t)msg -> packet_type, HEX);
    Serial.print(P(", sender: "));
    // this is a bd_addr data type, which is a 6-byte uint8_t array
    for (uint8_t i = 0; i < 6; i++) {
        if (msg -> sender.addr[i] < 16) Serial.write('0');
        Serial.print(msg -> sender.addr[i], HEX);
    }
    Serial.print(P(", address_type: ")); Serial.print(msg -> address_type, HEX);
    Serial.print(P(", bond: ")); Serial.print(msg -> bond, HEX);
    Serial.print(P(", data: "));
    // this is a uint8array data type, which is a length byte and a uint8_t* pointer
    for (uint8_t i = 0; i < msg -> data.len; i++) {
        if (msg -> data.data[i] < 16) Serial.write('0');
        Serial.print(msg -> data.data[i], HEX);
    }
    Serial.println(P(" }"));
}

void my_evt_connection_status_evt_t(const ble_msg_connection_status_evt_t *msg) {
    Serial.print(P("###\tconnection_status: { "));
    Serial.print(P("conn: "));    Serial.print(msg -> connection, HEX);
    Serial.print(P(", flags: ")); Serial.print(msg -> flags, HEX);
    Serial.print(P(", address: "));
    // this is a bd_addr data type, which is a 6-byte uint8_t array
    for (uint8_t i = 0; i < 6; i++) {
        if (msg -> address.addr[i] < 16) Serial.write('0');
        Serial.print(msg -> address.addr[i], HEX);
    }
    Serial.print(P(", address_type: ")); Serial.print(msg -> address_type, HEX);
    Serial.print(P(", intvl: "));        Serial.print(msg -> conn_interval, HEX);
    Serial.print(P(", timeout: "));      Serial.print(msg -> timeout, HEX);
    Serial.print(P(", latency: "));      Serial.print(msg -> latency, HEX);
    Serial.print(P(", bonding: "));      Serial.print(msg -> bonding, HEX);
    Serial.println(P(" }"));
}

void my_evt_connection_disconnected(const ble_msg_connection_disconnected_evt_t *msg) {
    Serial.println( P("###\tdisconnected") );

    // ble112.ble_cmd_gap_set_mode( BGLIB_GAP_GENERAL_DISCOVERABLE, BGLIB_GAP_UNDIRECTED_CONNECTABLE );
}

void my_evt_attclient_indicated(const struct ble_msg_attclient_indicated_evt_t *msg) {
    Serial.println( P("###\tattclient_indicated") );
    Serial.print(P("conn: "));   Serial.print((uint8)msg -> connection, HEX);
    Serial.print(P(", attrhandle: ")); Serial.print((uint16)msg -> attrhandle);
}

void setup() {
    // initialize status LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // USB serial
    Serial.begin(115200);

    // welcome!
    Serial.println(P("BLE112 BGAPI Scanner Demo"));

    // set up internal status handlers
    // (these are technically optional)
    ble112.onBusy = onBusy;
    ble112.onIdle = onIdle;
    ble112.onTimeout = onTimeout;

    // ONLY enable these if you are using the <wakeup_pin> parameter in your firmware's hardware.xml file
    ble112.onBeforeTXCommand = onBeforeTXCommand;
    ble112.onTXCommandComplete = onTXCommandComplete;

    // set up BGLib response handlers (called almost immediately after sending commands)
    // (these are also technicaly optional)
    ble112.ble_rsp_system_hello            = my_rsp_system_hello;
    ble112.ble_rsp_gap_set_scan_parameters = my_rsp_gap_set_scan_parameters;
    ble112.ble_rsp_gap_discover            = my_rsp_gap_discover;
    ble112.ble_rsp_gap_end_procedure       = my_rsp_gap_end_procedure;
    ble112.ble_rsp_gap_set_mode            = my_rsp_gap_set_mode;
    ble112.ble_rsp_attributes_write        = my_rsp_attributes_write;
    ble112.ble_rsp_connection_get_rssi     = my_rsp_connection_get_rssi;

    // set up BGLib event handlers (called at unknown times)
    ble112.ble_evt_system_boot             = my_evt_system_boot;
    ble112.ble_evt_gap_scan_response       = my_evt_gap_scan_response;
    ble112.ble_evt_connection_status       = my_evt_connection_status_evt_t;
    ble112.ble_evt_connection_disconnected = my_evt_connection_disconnected;
    ble112.ble_evt_attclient_indicated     = my_evt_attclient_indicated;

    // set the data rate for the SoftwareSerial port
    ble112uart.begin(38400);
}

void loop() {
    static uint8_t writeCount = 1;
    static uint8_t lastCharacter = '0';

    const uint8_t data[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
        0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
        0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
        0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
        0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
        0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
        0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
        0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
        0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
        0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
        0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
        0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
        0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
        0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, /* gatt database size max: 255 */
    };

    Serial.println(P("Operations Menu:"));
    Serial.println(P("0) Reset BLE112 module"));
    Serial.println(P("1) Hello"));
    Serial.println(P("2) Set gap mode(2,2)"));
    Serial.println(P("3) Get rssi"));
    Serial.println(P("4) Write attributes"));
    Serial.println(P("5) Prepare write & execute"));
    Serial.println(P("Command?"));
    while (1) {
        // keep polling for new data from BLE
        ble112.checkActivity();

        // check for input from the user
        if (Serial.available()) {

            Serial.print(P("free:"));
            Serial.println( freeMemory() );

            lastCharacter = Serial.read();
            Serial.print(P("0x"));
            Serial.println( lastCharacter, HEX );

            uint8_t status;
            if (lastCharacter == '0') {
                // Reset BLE112 module
                Serial.println(P("-->\tsystem_reset: { boot_in_dfu: 0 }"));
                ble112.ble_cmd_system_reset(0);

                while ((status = ble112.checkActivity(1000)));
                // system_reset doesn't have a response, but this BGLib
                // implementation allows the system_boot event specially to
                // set the P("busy") flag to false for this particular case
            }
            else if (lastCharacter == '1') {
                // Say hello to the BLE112 and wait for response
                Serial.println(P("-->\tsystem_hello"));
                ble112.ble_cmd_system_hello();

                while ((status = ble112.checkActivity(1000)));
                // response should come back within milliseconds
            }
            else if (lastCharacter == '2') {
                Serial.println(P("-->\tgap_set_mode: { discover: 0x2, connect: 0x2 }"));
                ble112.ble_cmd_gap_set_mode( BGLIB_GAP_GENERAL_DISCOVERABLE, BGLIB_GAP_UNDIRECTED_CONNECTABLE );
                while ((status = ble112.checkActivity(1000)));
            }
            else if (lastCharacter == '3') {
                Serial.println(P("-->\tget_rssi"));
                ble112.ble_cmd_connection_get_rssi( 0x00 ); // connection handle
                while ((status = ble112.checkActivity(1000)));
            }
            else if (lastCharacter == '4') {
                // maximum payload: 20bytes
                // see p26 of Bluetooth_Smart_API_11_11032013.pdf
                uint8 totalSize = BLE112_MAX_CHARACTERISTIC_VALUE_LENGTH;
                // Serial.print(P("totalSize:")); Serial.println(totalSize);

                for (uint8 i=0; i<(totalSize/20)+1; i++) {
                    uint8 sendSize = totalSize - (i * 20);
                    if (sendSize > 20) {
                        sendSize = 20;
                    }

                    Serial.println(P("-->\tattributes_write"));
                    /* Serial.print(P("sendSize:")); Serial.println(sendSize); */
                    /* Serial.print(P("i:"));        Serial.println(i); */

                    // handle value:
                    // When the project is compiled with the BGBuild compiler
                    // a text file called attributes.txt is generated.
                    // This files contains the ids and corresponding handle values.
                    ble112.ble_cmd_attributes_write( (uint16)0x0014,       // handle value
                                                     (uint8)(i*20),        // offset
                                                     sendSize,             // value_len
                                                     (const uint8*)&data[i*20] // value_data
                                                     );
                    while ((status = ble112.checkActivity(1000)));
                }
            }
        }
    }
}