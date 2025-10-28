// LoRa_APRS_Tracker.cpp
// Archivo principal del proyecto LoRa APRS Tracker
// ===========================================================================
// Bibliotecas del proyecto
// ===========================================================================
#include <HardwareSerial.h>    // Soporte para puertos serie de hardware 
#include <BluetoothSerial.h>  // Soporte para Bluetooth clásico
#include <APRSPacketLib.h>     // Biblioteca para manejar paquetes APRS
#include <TinyGPS++.h>        // Biblioteca para parseo de datos GPS
#include <Arduino.h>          // Núcleo de Arduino
#include <logger.h>           // Logger interno para mensajes de debug
#include <WiFi.h>             // Gestión de WiFi 
#include "smartbeacon_utils.h"
#include "bluetooth_utils.h"
#include "keyboard_utils.h"
#include "joystick_utils.h"
#include "configuration.h"
#include "battery_utils.h"
#include "station_utils.h"
#include "board_pinout.h"
#include "button_utils.h"
#include "power_utils.h"
#include "sleep_utils.h"
#include "menu_utils.h"
#include "lora_utils.h"
#include "wifi_utils.h"
#include "msg_utils.h"
#include "gps_utils.h"
#include "web_utils.h"
#include "ble_utils.h"
#include "wx_utils.h"
#include "display.h"
#include "utils.h"
#ifdef HAS_TOUCHSCREEN
#include "touch_utils.h"
#endif

// ========================== Objetos y configuración global ==========================

Configuration                       Config;                // Objeto con toda la configuración cargable
HardwareSerial                      gpsSerial(1);          // Puerto serie para el GPS (puerto 1)
TinyGPSPlus                         gps;                   // Instancia del parser TinyGPS++
#ifdef HAS_BT_CLASSIC
    BluetoothSerial                 SerialBT;              // Instancia para BT clásico (si aplica)
#endif

String      versionDate             = "2025-08-26";         // Fecha/versión del firmware

// Índices y punteros a configuraciones seleccionadas (beacons, tipos LoRa)
uint8_t     myBeaconsIndex          = 0;
int         myBeaconsSize           = Config.beacons.size();
Beacon      *currentBeacon          = &Config.beacons[myBeaconsIndex];
uint8_t     loraIndex               = 0;
int         loraIndexSize           = Config.loraTypes.size();
LoraType    *currentLoRaType        = &Config.loraTypes[loraIndex];


// Variables para manejo del menú y tiempo de refresco
int         menuDisplay             = 100;
uint32_t    menuTime                = millis();

// Estados del sistema y display
bool        statusState             = true;                 // Estado general del "status" (activar/desactivar checks)
bool        displayEcoMode          = Config.display.ecoMode;
bool        displayState            = true;
uint32_t    displayTime             = millis();
uint32_t    refreshDisplayTime      = millis();

bool        sendUpdate              = true;                 // Flag que indica si debemos enviar un beacon

// Configuración de Bluetooth
bool        bluetoothActive         = Config.bluetooth.active;
bool        bluetoothConnected      = false;


// Variables de transmisión (timing, ubicación anterior)
uint32_t    lastTx                  = 0.0;
uint32_t    txInterval              = 60000L;               // Intervalo por defecto entre TX en ms
uint32_t    lastTxTime              = 0;
double      lastTxLat               = 0.0;
double      lastTxLng               = 0.0;
double      lastTxDistance          = 0.0;

// Flags de funcionalidad adicional
bool        flashlight              = false;
bool        digipeaterActive        = false;
bool        sosActive               = false;

bool        miceActive              = false;                // MicE active (formato Mic-E para APRS)

bool        smartBeaconActive       = true;                 // Si el smart beacon automático está activo

uint32_t    lastGPSTime             = 0;                    // Última vez que tuvimos GPS activo

APRSPacket                          lastReceivedPacket;     // Último paquete APRS recibido

logging::Logger                     logger;                 // Logger para info/debug
//#define DEBUG

extern bool gpsIsActive;                                    // Variable externa que indica si el GPS está activo


// ========================== setup() ==========================
// Se ejecuta una sola vez al iniciar el dispositivo
void setup() {
    Serial.begin(115200);  // Serial principal para debugging

    #ifndef DEBUG
        // Si no estamos en modo DEBUG, seteamos nivel de log a INFO (evitar saturar)
        logger.setDebugLevel(logging::LoggerLevel::LOGGER_LEVEL_INFO);
    #endif

    POWER_Utils::setup();                                   // Inicializa control de energía (pines, ADC, etc.)
    displaySetup();                                        // Inicializa la pantalla
    POWER_Utils::externalPinSetup();                       // Configura pines externos relacionados con energía


    STATION_Utils::loadIndex(0);    // callsign Index
    STATION_Utils::loadIndex(1);    // lora freq settins Index
    STATION_Utils::nearStationInit();
    startupScreen(loraIndex, versionDate);                 // Muestra pantalla de arranque con versión

    WIFI_Utils::checkIfWiFiAP();                           // Comprueba si debe arrancar como AP WiFi

    MSG_Utils::loadNumMessages();                          // Carga número de mensajes almacenados
    GPS_Utils::setup();                                    // Inicializa GPS y puerto serie asociado
    currentLoRaType = &Config.loraTypes[loraIndex];        // Asegura que currentLoRaType apunte a la configuración actual
    LoRa_Utils::setup();                                   // Inicializa LoRa (SPI, pines, config)
    Utils::i2cScannerForPeripherals();                     // Escanea I2C para periféricos conectados (sensores, etc.)
    WX_Utils::setup();                                     // Inicializa utilidades de tiempo/meteorología


    WiFi.mode(WIFI_OFF);                                   // Apaga WiFi por defecto para ahorrar energía
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Main", "WiFi controller stopped");

    // Inicializa Bluetooth (BLE o clásico según configuración)
    if (bluetoothActive) {
        if (Config.bluetooth.useBLE) {
            BLE_Utils::setup();
        } else {
            #ifdef HAS_BT_CLASSIC
                BLUETOOTH_Utils::setup();
            #endif
        }
    }


    // Inicializa botones, joystick y teclado si existen las opciones / pines definidos
    #ifdef BUTTON_PIN
        BUTTON_Utils::setup();
    #endif
    #ifdef HAS_JOYSTICK
        JOYSTICK_Utils::setup();
    #endif
    KEYBOARD_Utils::setup();
    #ifdef HAS_TOUCHSCREEN
        TOUCH_Utils::setup();
    #endif

    POWER_Utils::lowerCpuFrequency();                       // Baja frecuencia del CPU para ahorro energético
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Main", "Smart Beacon is: %s", Utils::getSmartBeaconState());
    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "Setup Done!");
    menuDisplay = 0;                                       // Forzar refresco de menú al inicio

}

// ========================== loop() ==========================
// Bucle principal que corre continuamente
void loop() {
    // Actualiza puntero al beacon seleccionado
    currentBeacon = &Config.beacons[myBeaconsIndex];

    // Si el estado de "status" está activo, valida la configuración del beacon actual.
    if (statusState) {
        if (Config.validateConfigFile(currentBeacon->callsign)) {
            // Si la validación indica un cambio, simula pulsación derecha para recorrer la lista
            KEYBOARD_Utils::rightArrow();
            currentBeacon = &Config.beacons[myBeaconsIndex];
        }
        // Comprueba si el MicE (mice) está activo con la configuración del beacon actual
        miceActive = Config.validateMicE(currentBeacon->micE);
    }
    
    // Revisa ajustes y estado del smartbeacon
    SMARTBEACON_Utils::checkSettings(currentBeacon->smartBeaconSetting);
    SMARTBEACON_Utils::checkState();
    
    BATTERY_Utils::monitor();                              // Monitoring de batería (nivel, voltaje)
    Utils::checkDisplayEcoMode();                          // Ajusta modo eco del display si corresponde

    // Lectura de periféricos y entradas de usuario
    #ifdef BUTTON_PIN
        BUTTON_Utils::loop();
    #endif
    KEYBOARD_Utils::read();
    #ifdef HAS_JOYSTICK
        JOYSTICK_Utils::loop();
    #endif
    #ifdef HAS_TOUCHSCREEN
        TOUCH_Utils::loop();
    #endif

    // Recepción de paquetes LoRa (no bloqueante): devuelve estructura ReceivedLoRaPacket
    ReceivedLoRaPacket packet = LoRa_Utils::receivePacket();

    // Manejo de mensajes: revisar si hay mensajes entrantes, procesar buffer de salida, limpiar buffer temporal
    MSG_Utils::checkReceivedMessage(packet);
    MSG_Utils::processOutputBuffer();
    MSG_Utils::clean15SegBuffer();

    // Si Bluetooth está activo y conectado, enviar datos relevantes al teléfono
    if (bluetoothActive && bluetoothConnected) {
        if (Config.bluetooth.useBLE) {
            // En BLE enviamos la porción de texto del paquete (substring(3) para omitir prefijo)
            BLE_Utils::sendToPhone(packet.text.substring(3));
            BLE_Utils::sendToLoRa();
        } else {
            #ifdef HAS_BT_CLASSIC
                BLUETOOTH_Utils::sendToPhone(packet.text.substring(3));
                BLUETOOTH_Utils::sendToLoRa();
            #endif
        }
    }
    
    MSG_Utils::ledNotification();                           // Notificar con LED si hay eventos
    Utils::checkFlashlight();                              // Revisar estado de linterna (flashlight)
    STATION_Utils::checkListenedStationsByTimeAndDelete(); // Borra estaciones escuchadas hace mucho tiempo

    // Calcula tiempo desde la última transmisión
    lastTx = millis() - lastTxTime;

    // Si el GPS está activo, obtener datos y decidir si enviar beacons, refrescar pantalla
    if (gpsIsActive) {
        GPS_Utils::getData();                              // Lee datos del GPS (actualiza objeto gps)
        bool gps_time_update = gps.time.isUpdated();      // Indica si se actualizó la hora del GPS
        bool gps_loc_update  = gps.location.isUpdated();  // Indica si se actualizó la posición
        GPS_Utils::setDateFromData();                      // Actualiza fecha del sistema a partir del GPS

        int currentSpeed = (int) gps.speed.kmph();         // Velocidad actual en km/h

        if (gps_loc_update) Utils::checkStatus();         // Chequea y actualiza estado si hubo cambio de posición

        // Si no estamos enviando update y hay nueva posición y smartBeacon activo => calcular distancias/heading
        if (!sendUpdate && gps_loc_update && smartBeaconActive) {
            GPS_Utils::calculateDistanceTraveled();       // Calcula distancias desde último TX
            if (!sendUpdate) GPS_Utils::calculateHeadingDelta(currentSpeed); // Calcula cambio de rumbo si aplica
            STATION_Utils::checkStandingUpdateTime();    // Comprueba si debe forzar envío por estar parado
        }
        SMARTBEACON_Utils::checkFixedBeaconTime();       // Comprueba intervalos fijos para fixed beacons
        if (sendUpdate && gps_loc_update) STATION_Utils::sendBeacon(); // Si se debe enviar y hay nueva loc => enviar beacon
        if (gps_time_update) SMARTBEACON_Utils::checkInterval(currentSpeed); // Ajusta intervalos según velocidad

        // Refresca la pantalla cada ~1s o si hubo actualización de hora GPS
        if (millis() - refreshDisplayTime >= 1000 || gps_time_update) {
            GPS_Utils::checkStartUpFrames();              // Chequeos iniciales del GPS (frames)
            MENU_Utils::showOnScreen();                   // Dibuja la UI en pantalla
            refreshDisplayTime = millis();
        }
        SLEEP_Utils::checkIfGPSShouldSleep();            // Decide si poner GPS a dormir para ahorrar energía
    } else {
        // Si GPS no está activo, despertarlo si ha pasado txInterval desde lastGPSTime
        if (millis() - lastGPSTime > txInterval) {
            SLEEP_Utils::gpsWakeUp();
        }
        STATION_Utils::checkStandingUpdateTime();        // Chequeos relativos a estado "standing"
        if (millis() - refreshDisplayTime >= 1000) {
            MENU_Utils::showOnScreen();                   // Actualizar pantalla periódicamente
            refreshDisplayTime = millis();
        }
    }
}

// ========================== randnum ==========================
// Genera un número aleatorio entre min y max (inclusive) usando esp_random() (ESP32)
int randnum(int min, int max) {
    return min + (esp_random() % (max - min + 1));
}
