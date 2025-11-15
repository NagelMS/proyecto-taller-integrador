#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "configuration.h"
#include "board_pinout.h"
#include "display.h"
#include "logger.h"

extern logging::Logger logger;


void Configuration::writeFile() {

    Serial.println("Saving config..");

    // Document JSON estático en memoria (tamaño estimado - ajustar si cambia la estructura)
    StaticJsonDocument<2800> data;
    // Abrir/crear archivo de configuración en SPIFFS para sobrescribirlo
    File configFile = SPIFFS.open("/tracker_conf.json", "w");

    // --- Wifi AP ---
    data["wifiAP"]["active"]                    = wifiAP.active;     // activar AP al arrancar
    data["wifiAP"]["password"]                  = wifiAP.password;   // contraseña del AP

    // --- Beacons: array de perfiles de beacon (callsign, símbolo, comentario, flags...) ---
    for (int i = 0; i < beacons.size(); i++) {
        data["beacons"][i]["callsign"]              = beacons[i].callsign;            // indicativo
        data["beacons"][i]["symbol"]                = beacons[i].symbol;              // símbolo APRS
        data["beacons"][i]["overlay"]               = beacons[i].overlay;             // overlay del símbolo
        data["beacons"][i]["comment"]               = beacons[i].comment;             // comentario asociado
        data["beacons"][i]["smartBeaconActive"]     = beacons[i].smartBeaconActive;   // smartbeacon activado?
        data["beacons"][i]["smartBeaconSetting"]    = beacons[i].smartBeaconSetting;  // configuración smartbeacon (preset)
        data["beacons"][i]["micE"]                  = beacons[i].micE;                // Mic-E activado?
        data["beacons"][i]["gpsEcoMode"]            = beacons[i].gpsEcoMode;          // modo eco GPS para este beacon
        data["beacons"][i]["profileLabel"]          = beacons[i].profileLabel;        // etiqueta de perfil (UI)
    }

    // --- Display: parámetros visuales ---
    data["display"]["showSymbol"]               = display.showSymbol; // mostrar símbolo en UI?
    data["display"]["ecoMode"]                  = display.ecoMode;    // modo ECO global de pantalla
    data["display"]["timeout"]                  = display.timeout;    // tiempo de timeout pantalla (ms)
    data["display"]["turn180"]                  = display.turn180;    // invertir orientación 180°?

    // --- Battery: parámetros de monitorización ---
    data["battery"]["sendVoltage"]              = battery.sendVoltage;         // enviar voltaje como telemetría?
    data["battery"]["voltageAsTelemetry"]       = battery.voltageAsTelemetry;  // mapear voltaje como telemetría
    data["battery"]["sendVoltageAlways"]        = battery.sendVoltageAlways;   // enviar siempre voltaje
    data["battery"]["monitorVoltage"]           = battery.monitorVoltage;      // monitorizar voltaje
    data["battery"]["sleepVoltage"]             = battery.sleepVoltage;        // voltaje para entrar en sleep

    // --- Winlink: credenciales (solo password aquí) ---
    data["winlink"]["password"]                 = winlink.password;

    // --- Telemetría: control y correcciones ---
    data["telemetry"]["active"]                 = telemetry.active;
    data["telemetry"]["sendTelemetry"]          = telemetry.sendTelemetry;
    data["telemetry"]["temperatureCorrection"]  = telemetry.temperatureCorrection;

    // --- Notificaciones: LEDs, buzzer y comportamientos ---
    data["notification"]["ledTx"]               = notification.ledTx;
    data["notification"]["ledTxPin"]            = notification.ledTxPin;
    data["notification"]["ledMessage"]          = notification.ledMessage;
    data["notification"]["ledMessagePin"]       = notification.ledMessagePin;
    data["notification"]["ledFlashlight"]       = notification.ledFlashlight;
    data["notification"]["ledFlashlightPin"]    = notification.ledFlashlightPin;
    data["notification"]["buzzerActive"]        = notification.buzzerActive;
    data["notification"]["buzzerPinTone"]       = notification.buzzerPinTone;
    data["notification"]["buzzerPinVcc"]        = notification.buzzerPinVcc;
    data["notification"]["bootUpBeep"]          = notification.bootUpBeep;
    data["notification"]["txBeep"]              = notification.txBeep;
    data["notification"]["messageRxBeep"]       = notification.messageRxBeep;
    data["notification"]["stationBeep"]         = notification.stationBeep;
    data["notification"]["lowBatteryBeep"]      = notification.lowBatteryBeep;
    data["notification"]["shutDownBeep"]        = notification.shutDownBeep;
    
    // --- LoRa types: array de configuraciones (frecuencia, SF, BW, CR, potencia) ---
    for (int i = 0; i < loraTypes.size(); i++) {
        data["lora"][i]["frequency"]                = loraTypes[i].frequency;
        data["lora"][i]["spreadingFactor"]          = loraTypes[i].spreadingFactor;
        data["lora"][i]["signalBandwidth"]          = loraTypes[i].signalBandwidth;
        data["lora"][i]["codingRate4"]              = loraTypes[i].codingRate4;
        data["lora"][i]["power"]                    = loraTypes[i].power;
    }

    // --- PTT trigger: configuración de PTT (GPIO, tiempos, inversión) ---
    data["pttTrigger"]["active"]                = ptt.active;
    data["pttTrigger"]["io_pin"]                = ptt.io_pin;
    data["pttTrigger"]["preDelay"]              = ptt.preDelay;
    data["pttTrigger"]["postDelay"]             = ptt.postDelay;
    data["pttTrigger"]["reverse"]               = ptt.reverse;

    // --- Bluetooth: estado y preferencias ---
    data["bluetooth"]["active"]                 = bluetooth.active;
    data["bluetooth"]["deviceName"]             = bluetooth.deviceName;
    #ifdef HAS_BT_CLASSIC
        data["bluetooth"]["useBLE"]             = bluetooth.useBLE; // según la configuración
    #else
        data["bluetooth"]["useBLE"]             = true; // en plataformas sin BT Classic forzamos BLE
    #endif
    data["bluetooth"]["useKISS"]                = bluetooth.useKISS; // usar KISS sobre BT?

    // --- Otros: parámetros misceláneos del tracker ---
    data["other"]["simplifiedTrackerMode"]      = simplifiedTrackerMode;      // modo simplificado (menos opciones)
    data["other"]["sendCommentAfterXBeacons"]   = sendCommentAfterXBeacons;   // comportamiento APRSThursday
    data["other"]["path"]                       = path;                       // camino/path APRS
    data["other"]["nonSmartBeaconRate"]         = nonSmartBeaconRate;         // intervalo cuando no es smart beacon
    data["other"]["rememberStationTime"]        = rememberStationTime;        // tiempo para recordar estaciones escuchadas
    data["other"]["standingUpdateTime"]         = standingUpdateTime;         // intervalo standing update
    data["other"]["sendAltitude"]               = sendAltitude;               // enviar altitud?
    data["other"]["disableGPS"]                 = disableGPS;                 // desactivar GPS por SW
    data["other"]["acceptOwnFrameFromTNC"]      = acceptOwnFrameFromTNC;      // aceptar frames propios desde TNC?
    data["other"]["email"]                      = email;                      // correo configurado para posmsg

    // --- Serializar JSON y guardar en SPIFFS ---
    serializeJson(data, configFile); // escribe el JSON en el archivo abierto
    configFile.close();              // cerrar el archivo
    Serial.println("Config saved");
}

/**
 * Configuration::readFile()
 *
 * Propósito:
 *   Leer y cargar la configuración del dispositivo desde el archivo JSON
 *   persistente (SPIFFS: /tracker_conf.json). Si el archivo no existe
 *   devuelve false; si existe, deserializa el JSON y rellena las
 *   estructuras/variables de configuración del sistema.
 *
 * Comportamiento:
 *   - Intenta abrir el archivo en SPIFFS y deserializarlo con ArduinoJson.
 *   - Para cada sección del JSON (wifiAP, beacons, display, battery, winlink,
 *     telemetry, notification, lora, pttTrigger, bluetooth, other) se asignan
 *     valores a las variables/estructuras internas; si falta una clave se usa
 *     un valor por defecto mediante el operador `|`.
 *   - Construye arrays dinámicos (beacons, loraTypes) según el contenido del JSON.
 *   - Cierra el archivo y devuelve true si la lectura fue satisfactoria, o
 *     false si el archivo no existe.
 */
bool Configuration::readFile() {
    Serial.println("Reading config..");
    File configFile = SPIFFS.open("/tracker_conf.json", "r");

    if (configFile) {
        StaticJsonDocument<2800> data;
        DeserializationError error = deserializeJson(data, configFile);
        if (error) {
            Serial.println("Failed to read file, using default configuration");
        }

        wifiAP.active               = data["wifiAP"]["active"] | true;
        wifiAP.password             = data["wifiAP"]["password"] | "1234567890";

        JsonArray BeaconsArray = data["beacons"];
        for (int i = 0; i < BeaconsArray.size(); i++) {
            Beacon bcn;

            bcn.callsign                = BeaconsArray[i]["callsign"] | "NOCALL-7";
            bcn.callsign.toUpperCase();
            bcn.symbol                  = BeaconsArray[i]["symbol"] | "[";
            bcn.overlay                 = BeaconsArray[i]["overlay"] | "/";
            bcn.comment                 = BeaconsArray[i]["comment"] | "";
            bcn.smartBeaconActive       = BeaconsArray[i]["smartBeaconActive"] | true;
            bcn.smartBeaconSetting      = BeaconsArray[i]["smartBeaconSetting"] | 0;
            bcn.micE                    = BeaconsArray[i]["micE"] | "";
            bcn.gpsEcoMode              = BeaconsArray[i]["gpsEcoMode"] | false;
            bcn.profileLabel            = BeaconsArray[i]["profileLabel"] | "";
            
            beacons.push_back(bcn);
        }

        display.showSymbol              = data["display"]["showSymbol"] | true;
        display.ecoMode                 = data["display"]["ecoMode"] | false;
        display.timeout                 = data["display"]["timeout"] | 4;
        display.turn180                 = data["display"]["turn180"] | false;

        battery.sendVoltage             = data["battery"]["sendVoltage"] | false;
        battery.voltageAsTelemetry      = data["battery"]["voltageAsTelemetry"] | false;
        battery.sendVoltageAlways       = data["battery"]["sendVoltageAlways"] | false;
        battery.monitorVoltage          = data["battery"]["monitorVoltage"] | false;
        battery.sleepVoltage            = data["battery"]["sleepVoltage"] | 2.9;

        winlink.password                = data["winlink"]["password"] | "NOPASS";

        telemetry.active                = data["telemetry"]["active"] | false;
        telemetry.sendTelemetry         = data["telemetry"]["sendTelemetry"] | false;
        telemetry.temperatureCorrection = data["telemetry"]["temperatureCorrection"] | 0.0;
        
        notification.ledTx              = data["notification"]["ledTx"] | false;
        notification.ledTxPin           = data["notification"]["ledTxPin"]| 13;
        notification.ledMessage         = data["notification"]["ledMessage"] | false;
        notification.ledMessagePin      = data["notification"]["ledMessagePin"] | 2;
        notification.ledFlashlight      = data["notification"]["ledFlashlight"] | false;
        notification.ledFlashlightPin   = data["notification"]["ledFlashlightPin"] | 14;
        notification.buzzerActive       = data["notification"]["buzzerActive"] | false;
        notification.buzzerPinTone      = data["notification"]["buzzerPinTone"] | 33;
        notification.buzzerPinVcc       = data["notification"]["buzzerPinVcc"] | 25;
        notification.bootUpBeep         = data["notification"]["bootUpBeep"] | false;
        notification.txBeep             = data["notification"]["txBeep"] | false;
        notification.messageRxBeep      = data["notification"]["messageRxBeep"] | false;
        notification.stationBeep        = data["notification"]["stationBeep"] | false;
        notification.lowBatteryBeep     = data["notification"]["lowBatteryBeep"] | false;
        notification.shutDownBeep       = data["notification"]["shutDownBeep"] | false;

        JsonArray LoraTypesArray = data["lora"];
        for (int j = 0; j < LoraTypesArray.size(); j++) {
            LoraType loraType;

            loraType.frequency          = LoraTypesArray[j]["frequency"] | 433775000;
            loraType.spreadingFactor    = LoraTypesArray[j]["spreadingFactor"] | 12;
            loraType.signalBandwidth    = LoraTypesArray[j]["signalBandwidth"] | 125000;
            loraType.codingRate4        = LoraTypesArray[j]["codingRate4"] | 5;
            loraType.power              = LoraTypesArray[j]["power"] | 20;
            loraTypes.push_back(loraType);
        }

        ptt.active                      = data["pttTrigger"]["active"] | false;
        ptt.io_pin                      = data["pttTrigger"]["io_pin"] | 4;
        ptt.preDelay                    = data["pttTrigger"]["preDelay"] | 0;
        ptt.postDelay                   = data["pttTrigger"]["postDelay"] | 0;
        ptt.reverse                     = data["pttTrigger"]["reverse"] | false;

        bluetooth.active                = data["bluetooth"]["active"] | false;
        bluetooth.deviceName            = data["bluetooth"]["deviceName"] | "LoRaTracker";
        #ifdef HAS_BT_CLASSIC
            bluetooth.useBLE            = data["bluetooth"]["useBLE"] | false;
            bluetooth.useKISS           = data["bluetooth"]["useKISS"] | false;
        #else
            bluetooth.useBLE            = true;    // fixed as BLE
            bluetooth.useKISS           = data["bluetooth"]["useKISS"] | true;    // true=KISS,  false=TNC2            
        #endif

        simplifiedTrackerMode           = data["other"]["simplifiedTrackerMode"] | false;
        sendCommentAfterXBeacons        = data["other"]["sendCommentAfterXBeacons"] | 10;
        path                            = data["other"]["path"] | "WIDE1-1";
        nonSmartBeaconRate              = data["other"]["nonSmartBeaconRate"] | 15;
        rememberStationTime             = data["other"]["rememberStationTime"] | 30;
        standingUpdateTime              = data["other"]["standingUpdateTime"] | 15;
        sendAltitude                    = data["other"]["sendAltitude"] | true;
        disableGPS                      = data["other"]["disableGPS"] | false;
        acceptOwnFrameFromTNC           = data["other"]["acceptOwnFrameFromTNC"] | false;
        email                           = data["other"]["email"] | "";

        configFile.close();
        Serial.println("Config read successfuly");
        return true;
    } else {
        Serial.println("Config file not found");
        return false;
    }
}

// Valida el archivo de configuración verificando si el indicativo contiene "NOCALL".
// Si encuentra un indicativo no configurado, registra un error y muestra un mensaje en pantalla.
bool Configuration::validateConfigFile(const String& currentBeaconCallsign) {
    if (currentBeaconCallsign.indexOf("NOCALL") != -1) {
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "Config", "Change all your callsigns in WebConfig");
        displayShow("ERROR", "Callsigns = NOCALL!", "---> cambialo !!!", 2000);
        return true;
    } else {
        return false;
    }
}

// Verifica si el valor Mic-E recibido coincide con alguno de los tipos Mic-E válidos.
// Retorna true si el código Mic-E es válido, false si no lo es.
bool Configuration::validateMicE(const String& currentBeaconMicE) {
    String miceMessageTypes[] = {"111", "110", "101", "100", "011", "010", "001" , "000"};
    int arraySize = sizeof(miceMessageTypes) / sizeof(miceMessageTypes[0]);
    bool validType = false;
    for (int i = 0; i < arraySize; i++) {
        if (currentBeaconMicE == miceMessageTypes[i]) {
            validType = true;
        }
    }
    return validType;
}


// Inicializa una configuración nueva con valores por defecto para WiFi, beacons,
// pantalla, batería, notificaciones, LoRa, Bluetooth, telemetría y otros parámetros.
// Se usa cuando no existe el archivo de configuración o cuando debe regenerarse.
void Configuration::init() {
    wifiAP.active                   = true;
    wifiAP.password                 = "1234567890";

    for (int i = 0; i < 3; i++) {
        Beacon beacon;
        beacon.callsign             = "NOCALL-7";
        beacon.symbol               = "[";
        beacon.overlay              = "/";
        beacon.comment              = "";
        beacon.smartBeaconActive    = true;
        beacon.smartBeaconSetting   = 0;
        beacon.micE                 = "";
        beacon.gpsEcoMode           = false;
        beacon.profileLabel         = "";
        beacons.push_back(beacon);
    }

    display.showSymbol              = true;
    display.ecoMode                 = false;
    display.timeout                 = 4;
    display.turn180                 = false;

    battery.sendVoltage             = false;
    battery.voltageAsTelemetry      = false;
    battery.sendVoltageAlways       = false;
    battery.monitorVoltage          = false;
    battery.sleepVoltage            = 2.9;

    winlink.password                = "NOPASS";

    telemetry.active                 = false;
    telemetry.sendTelemetry          = false;
    telemetry.temperatureCorrection  = 0.0;

    notification.ledTx              = false;
    notification.ledTxPin           = 13;
    notification.ledMessage         = false;
    notification.ledMessagePin      = 2;
    notification.ledFlashlight      = false;
    notification.ledFlashlightPin   = 14;
    notification.buzzerActive       = false;
    notification.buzzerPinTone      = 33;
    notification.buzzerPinVcc       = 25;
    notification.bootUpBeep         = false;
    notification.txBeep             = false;
    notification.messageRxBeep      = false;
    notification.stationBeep        = false;
    notification.lowBatteryBeep     = false;
    notification.shutDownBeep       = false;

    for (int j = 0; j < 3; j++) {
        LoraType loraType;
        switch (j) {
            case 0:
                loraType.frequency           = 433775000;
                loraType.spreadingFactor     = 12;
                loraType.codingRate4         = 5;
                break;
            case 1:
                loraType.frequency           = 434855000;
                loraType.spreadingFactor     = 9;
                loraType.codingRate4         = 7;
                break;
            case 2:
                loraType.frequency           = 439912500;
                loraType.spreadingFactor     = 12;
                loraType.codingRate4         = 5;
                break;
        }
        loraType.signalBandwidth    = 125000;
        loraType.power              = 20;
        loraTypes.push_back(loraType);
    }

    ptt.active                      = false;
    ptt.io_pin                      = 4;
    ptt.preDelay                    = 0;
    ptt.postDelay                   = 0;
    ptt.reverse                     = false;

    bluetooth.active                = false;
    bluetooth.deviceName            = "LoRaTracker";
    #ifdef HAS_BT_CLASSIC
        bluetooth.useBLE            = false;
        bluetooth.useKISS           = false;
    #else
        bluetooth.useBLE            = true;    // fixed as BLE
        bluetooth.useKISS           = true;
    #endif
    
    simplifiedTrackerMode           = false;
    sendCommentAfterXBeacons        = 10;
    path                            = "WIDE1-1";
    nonSmartBeaconRate              = 15;
    rememberStationTime             = 30;
    standingUpdateTime              = 15;
    sendAltitude                    = true;
    disableGPS                      = false;
    acceptOwnFrameFromTNC           = false;
    email                           = "";

    Serial.println("New Data Created...");
}

// Constructor de la clase Configuration. Monta el sistema SPIFFS,
// verifica si existe el archivo de configuración y, de no existir,
// genera una configuración nueva y fuerza un reinicio del dispositivo.
Configuration::Configuration() {
    if (!SPIFFS.begin(false)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    bool exists = SPIFFS.exists("/tracker_conf.json");
    if (!exists) {        
        init();
        writeFile();
        ESP.restart();
    }
    readFile();
}