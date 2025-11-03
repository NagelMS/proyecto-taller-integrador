#include "board_pinout.h"
#include "sleep_utils.h"
#include "power_utils.h"

// -----------------------------------------------------------------------------
// Gestión de reposo del GPS (eco de energía)
// -----------------------------------------------------------------------------


extern uint32_t         lastGPSTime;
extern bool             gpsIsActive;

bool gpsShouldSleep     = false;


namespace SLEEP_Utils {

    void gpsSleep() { // Apaga el GPS si está activo y registra el sello de tiempo
        #ifdef HAS_GPS_CTRL
            if (gpsIsActive) {
                POWER_Utils::deactivateGPS(); // Cortar alimentación/señal del módulo GPS
                lastGPSTime = millis(); // Marca el instante para controlar tiempos de sueño
                //
                Serial.println("GPS SLEEPING"); // Traza: estado de reposo
                //
            }
        #endif
    }

    void gpsWakeUp() { // Reactiva el GPS y limpia el flag de sueño
        #ifdef HAS_GPS_CTRL
            if (!gpsIsActive) {
                POWER_Utils::activateGPS(); // Reenergiza el módulo GPS
                gpsShouldSleep = false; // Evita apagarlo inmediatamente tras despertar
                //
                Serial.println("GPS WAKEUP"); // Traza: estado de activo
                //
            }
        #endif
    }

    void checkIfGPSShouldSleep() { // Verifica flag global para dormir el GPS
        if (gpsShouldSleep) {
            gpsSleep();
        }
    }

}
