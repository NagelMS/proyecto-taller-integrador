#include <TinyGPS++.h>
#include "TimeLib.h"
#include <APRSPacketLib.h>
#include "smartbeacon_utils.h"
#include "configuration.h"
#include "station_utils.h"
#include "board_pinout.h"
#include "power_utils.h"
#include "sleep_utils.h"
#include "gps_utils.h"
#include "display.h"
#include "logger.h"


#ifdef GPS_BAUDRATE
    #define GPS_BAUD    GPS_BAUDRATE
#else
    #define GPS_BAUD    9600
#endif


extern Configuration        Config;                      // Configuración global
extern HardwareSerial       gpsSerial;                   // Puerto serie conectado al GPS
extern TinyGPSPlus          gps;                         // Objeto TinyGPS++ con datos GPS
extern Beacon               *currentBeacon;              // Beacon activo (configuración del perfil)
extern logging::Logger      logger;                       // Logger del sistema
extern bool                 sendUpdate;                  // Flag para solicitar envío de update
extern bool		            sendStandingUpdate;          // Flag para envío de standing update

extern uint32_t             lastTxTime;                  // Tiempo (ms) desde última transmisión
extern uint32_t             txInterval;                  // Intervalo de transmisión en ms
extern double               lastTxLat;                   // Latitud de la última tx
extern double               lastTxLng;                   // Longitud de la última tx
extern double               lastTxDistance;              // Distancia desde la última tx hasta posición actual
extern uint32_t             lastTx;                      // Temporizador/contador para lógica de SmartBeacon
extern bool                 disableGPS;                  // Flag para deshabilitar uso del GPS por SW
extern bool                 gpsShouldSleep;              // Señal para que el GPS entre a modo sleep
extern SmartBeaconValues    currentSmartBeaconValues;    // Parámetros inteligentes para beaconing

// Variables internas del módulo GPS_Utils
double      currentHeading  = 0;   // rumbo actual (grados)
double      previousHeading = 0;   // rumbo anterior (grados)
float       bearing         = 0;   // variable auxiliar para cálculo de dirección cardinal

bool        gpsIsActive     = true; // estado local si GPS activo (puede duplicar disableGPS)



/*
 * Contiene utilidades para inicializar, leer y procesar datos GPS,
 * además de la lógica SmartBeacon (distancias, ángulos, etc.).
 */
namespace GPS_Utils {

    // Inicializa el puerto serie del GPS y alimenta el módulo si la placa lo requiere.
    void setup() {
        if (disableGPS) {
            // Si la configuración desactiva el GPS, loguear y salir.
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "GPS disabled");
            return;
        }
        #ifdef LIGHTTRACKER_PLUS_1_0
            // En algunas placas es necesario controlar VCC del GPS por software
            pinMode(GPS_VCC, OUTPUT);
            digitalWrite(GPS_VCC, LOW);
            delay(200);
        #endif
        #if defined(F4GOH_1W_LoRa_Tracker) || defined(F4GOH_1W_LoRa_Tracker_LLCC68)
            // Otras placas requieren activar VCC y esperar un poco
            pinMode(GPS_VCC, OUTPUT);
            digitalWrite(GPS_VCC, HIGH);
            delay(200);
        #endif
        
        // Abrir serial con la velocidad configurada (definida por GPS_BAUD)
        gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_TX, GPS_RX);
    }

    // Calcula distancia y rumbo hacia un "checkpoint" dado y actualiza listas/ordenamiento
    void calculateDistanceCourse(const String& callsign, double checkpointLatitude, double checkPointLongitude) {
        // distancia en km entre nuestra posición y el checkpoint
        double distanceKm = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), checkpointLatitude, checkPointLongitude) / 1000.0;
        // rumbo desde nuestra posición hacia el checkpoint
        double courseTo   = TinyGPSPlus::courseTo(gps.location.lat(), gps.location.lng(), checkpointLatitude, checkPointLongitude);
        // limpieza y ordenamiento de la lista de estaciones escuchadas
        STATION_Utils::deleteListenedStationsByTime();
        STATION_Utils::orderListenedStationsByDistance(callsign, distanceKm, courseTo);
    }

    // Leer todos los bytes disponibles del puerto serie del GPS y pasarlos al parser TinyGPS++
    void getData() {
        if (disableGPS) return;
        while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());
    }

    // Si el GPS tiene tiempo válido, sincroniza el RTC/software clock con los datos del GPS
    void setDateFromData() {
        if (gps.time.isValid()) setTime(gps.time.hour(), gps.time.minute(), gps.time.second(), gps.date.day(), gps.date.month(), gps.date.year());
    }

    // Calcula la distancia desde la última transmisión y decide si se debe enviar un update
    void calculateDistanceTraveled() {
        currentHeading  = gps.course.deg(); // actualizar rumbo actual
        // Distancia desde la última tx (metros).
        lastTxDistance  = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), lastTxLat, lastTxLng);
        // Si ha transcurrido suficiente tiempo desde la última tx (lastTx es contador/tiempo)
        if (lastTx >= txInterval) {
            // Si supera la distancia mínima configurada, solicitar envío de update
            if (lastTxDistance > currentSmartBeaconValues.minTxDist) {
                sendUpdate = true;
                sendStandingUpdate = false;
            } else {
                // Si no se alcanza la distancia mínima y el beacon tiene modo eco habilitado,
                // activar la sugerencia de que el GPS entre en sleep para ahorrar energía.
                if (currentBeacon->gpsEcoMode) {
                    // Debug en serie para diagnosticar por qué no se envía
                    Serial.print("minTxDistance not achieved : ");
                    Serial.println(lastTxDistance);
                    gpsShouldSleep = true;
                }
            }
        }
    }

    // Calcula la variación de rumbo (delta) y decide si enviar update por giro significativo
    void calculateHeadingDelta(int speed) {
        uint8_t TurnMinAngle;
        double headingDelta = abs(previousHeading - currentHeading); // diferencia absoluta de rumbo
        // Solo evaluar giro si ha pasado suficiente tiempo desde la última tx
        if (lastTx > currentSmartBeaconValues.minDeltaBeacon * 1000) {
            // Calcular umbral dinámico según velocidad:
            // TurnMinAngle = base + slope / speed (evita división por 0 añadiendo +1)
            if (speed == 0) {
                TurnMinAngle = currentSmartBeaconValues.turnMinDeg + (currentSmartBeaconValues.turnSlope/(speed + 1));
            } else {
                TurnMinAngle = currentSmartBeaconValues.turnMinDeg + (currentSmartBeaconValues.turnSlope/speed);
            }
            // Si el giro excede el umbral y se ha movido la distancia mínima, forzar update
            if (headingDelta > TurnMinAngle && lastTxDistance > currentSmartBeaconValues.minTxDist) {
                sendUpdate = true;
                sendStandingUpdate = false;
            }
        }
    }

    // Comprobar si durante el arranque no llegan tramas GPS; si no hay datos sugiere reset físico
    void checkStartUpFrames() {
        if (disableGPS) return;
        // Si tras 10s no se han procesado casi caracteres, no hay tramas -> log y mostrar error
        if ((millis() > 10000 && gps.charsProcessed() < 10)) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "GPS",
                        "No GPS frames detected! Try to reset the GPS Chip with this "
                        "firmware: https://github.com/richonguzman/TTGO_T_BEAM_GPS_RESET");
            displayShow("ERROR", "No GPS frames!", "Reset the GPS Chip", 2000);
        }
    }

    // Construye una representación "humana" del bearing con tres campos (left, center, right)
    // para mostrar en una sola línea con formato visual fijo.
    String getHumanBearing(const String& left, const String& center, const String& right) {
        String bearing = ">.";
        bearing += left;
        for (int i = 0; i < 9; i++) {
            bearing += ".";
        }
        bearing += "(";
        bearing += center;
        bearing += ").....";
        if (right.length() == 1 && center.length() != 2) bearing += ".";
        bearing += right;
        bearing += ".<";
        return bearing;
    }

    // Devuelve una cadena con representación cardinal/visual del rumbo (bearing -> N, NE, E, ...)
    // Cada rango angular devuelve una línea formateada con ejes y símbolo central.
    // Nota: la variable 'bearing' se actualiza si la velocidad es mayor que un umbral pequeño.
    String getCardinalDirection(float course) {
        if (gps.speed.kmph() > 0.5) bearing = course; // solo actualizar bearing si hay velocidad significativa

        // Rutas de decisión: cada rango angular de ~11.25° cubre una dirección compuesta
        // (estas cadenas están cuidadosamente alineadas para el diseño del display).
        if (bearing >= 354.375 || bearing < 5.625)    return ">.NW.....(N).....NE.<"; // N
        if (bearing >= 5.675 && bearing < 16.875)     return ">.......N.|.....NE..<";
        if (bearing >= 16.875 && bearing < 28.125)    return ">.....N...|...NE....<"; // NEN
        if (bearing >= 28.125 && bearing < 39.375)    return ">...N.....|.NE......<";
        if (bearing >= 39.375 && bearing < 50.625)    return ">.N......(NE).....E.<"; // NE
        if (bearing >= 50.625 && bearing < 61.875)    return ">.......NE|.....E...<"; 
        if (bearing >= 61.875 && bearing < 73.125)    return ">.....NE..|...E.....<"; // ENE
        if (bearing >= 73.125 && bearing < 84.375)    return ">...NE....|.E.......<"; 
        if (bearing >= 84.375 && bearing < 95.625)    return ">.NE.....(E).....SE.<"; // E
        if (bearing >= 95.625 && bearing < 106.875)   return ">.......E.|.....SE..<";
        if (bearing >= 106.875 && bearing < 118.125)  return ">.....E...|...SE....<"; // ESE
        if (bearing >= 118.125 && bearing < 129.375)  return ">...E.....|.SE......<";
        if (bearing >= 129.375 && bearing < 140.625)  return ">.E......(SE).....S.<"; // SE
        if (bearing >= 140.625 && bearing < 151.875)  return ">.......SE|.....S...<";
        if (bearing >= 151.875 && bearing < 163.125)  return ">.....SE..|...S.....<"; // SES
        if (bearing >= 163.125 && bearing < 174.375)  return ">...SE....|.S.......<";
        if (bearing >= 174.375 && bearing < 185.625)  return ">.SE.....(S).....SW.<"; // S
        if (bearing >= 185.625 && bearing < 196.875)  return ">.......S.|.....SW..<";
        if (bearing >= 196.875 && bearing < 208.125)  return ">.....S...|...SW....<"; // SWS
        if (bearing >= 208.125 && bearing < 219.375)  return ">...S.....|.SW......<";
        if (bearing >= 219.375 && bearing < 230.625)  return ">.S......(SW).....W.<"; // SW
        if (bearing >= 230.625 && bearing < 241.875)  return ">.......SW|.....W...<";
        if (bearing >= 241.875 && bearing < 253.125)  return ">.....SW..|...W.....<"; // WSW
        if (bearing >= 253.125 && bearing < 264.375)  return ">...SW....|.W.......<";
        if (bearing >= 264.375 && bearing < 275.625)  return ">.SW.....(W).....NW.<"; // W
        if (bearing >= 275.625 && bearing < 286.875)  return ">.......W.|.....NW..<";
        if (bearing >= 286.875 && bearing < 298.125)  return ">.....W...|...NW....<"; // WNW
        if (bearing >= 298.125 && bearing < 309.375)  return ">...W.....|.NW......<";
        if (bearing >= 309.375 && bearing < 320.625)  return ">.W......(NW).....N.<"; // NW
        if (bearing >= 320.625 && bearing < 331.875)  return ">.......NW|.....N...<";
        if (bearing >= 331.875 && bearing < 343.125)  return ">.....NW..|...N.....<"; // NWN
        if (bearing >= 343.125 && bearing < 354.375)  return ">...NW....|.N.......<";
        return ""; // valor por defecto si no entra en ningún rango (no debería ocurrir)
    }

}