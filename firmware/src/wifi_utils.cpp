#include <logger.h>
#include <WiFi.h>
#include "configuration.h"
#include "web_utils.h"
#include "display.h"

// -----------------------------------------------------------------------------
// Manejo de WiFi AP para configuración web
// -----------------------------------------------------------------------------


extern      Configuration       Config;
extern      logging::Logger     logger;

uint32_t    noClientsTime        = 0;


namespace WIFI_Utils {

    void startAutoAP() { // Levanta AP "LoRaTracker-AP" con clave de Config
        WiFi.mode(WIFI_MODE_NULL);
        WiFi.mode(WIFI_AP);
        WiFi.softAP("LoRaTracker-AP", Config.wifiAP.password);
    }

    void checkIfWiFiAP() { // Entra a modo Web-Config si activo o callsign=NOCALL-7; detiene por inactividad
        if (Config.wifiAP.active || Config.beacons[0].callsign == "NOCALL-7"){
            displayShow(" LoRa APRS", "    ** WEB-CONF **","", "WiFiAP:LoRaTracker-AP", "IP    :   192.168.4.1","");
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "WebConfiguration Started!");
            startAutoAP();
            WEB_Utils::setup();
            while (true) { // Bucle de servicio del portal: cierra tras 2 min sin clientes
                if (WiFi.softAPgetStationNum() > 0) {
                    noClientsTime = 0;
                } else {
                    if (noClientsTime == 0) {
                        noClientsTime = millis();
                    } else if ((millis() - noClientsTime) > 2 * 60 * 1000) {
                        logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Main", "WebConfiguration Stopped!");
                        displayShow("", "", "  STOPPING WiFi AP", 2000);
                        Config.wifiAP.active = false;
                        Config.writeFile();
                        WiFi.softAPdisconnect(true);
                        ESP.restart();
                    }
                }
            }
        } else {
            WiFi.mode(WIFI_OFF);
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Main", "WiFi controller stopped");
        }
    }
}
