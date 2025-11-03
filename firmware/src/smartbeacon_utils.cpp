#include "smartbeacon_utils.h"
#include "configuration.h"
#include "winlink_utils.h"

// -----------------------------------------------------------------------------
// SmartBeacon: cálculo de intervalo y estado
// -----------------------------------------------------------------------------

extern Configuration    Config;
extern Beacon           *currentBeacon;
extern bool             smartBeaconActive;
extern uint32_t         txInterval;
extern uint32_t         lastTxTime;
extern bool             sendUpdate;
extern uint8_t          winlinkStatus;


SmartBeaconValues   currentSmartBeaconValues;
byte                smartBeaconSettingsIndex    = 10;
bool                wxRequestStatus             = false;
uint32_t            wxRequestTime               = 0;


SmartBeaconValues   smartBeaconSettings[3] = {
    {120,  3, 60, 15,  50, 20, 12, 60},     // Runner settings  = SLOW
    {120,  5, 60, 40, 100, 12, 12, 60},     // Bike settings    = MEDIUM
    {120, 10, 60, 70, 100, 12, 10, 80}      // Car settings     = FAST
};


namespace SMARTBEACON_Utils {

    void checkSettings(byte index) { // Aplica un perfil (runner/bike/car) si cambia el índice
        if (smartBeaconSettingsIndex != index) {
            currentSmartBeaconValues = smartBeaconSettings[index];
            smartBeaconSettingsIndex = index;
        }
    }

    void checkInterval(int speed) { // Ajusta txInterval en función de la velocidad actual
        if (smartBeaconActive) {
            if (speed < currentSmartBeaconValues.slowSpeed) {
                txInterval = currentSmartBeaconValues.slowRate * 1000; // Velocidad por debajo del umbral lento
            } else if (speed > currentSmartBeaconValues.fastSpeed) {
                txInterval = currentSmartBeaconValues.fastRate * 1000; // Velocidad por encima del umbral rápido
            } else {
                txInterval = min(currentSmartBeaconValues.slowRate, currentSmartBeaconValues.fastSpeed * currentSmartBeaconValues.fastRate / speed) * 1000; // Interpolación: más rápido → más frecuente
            }
        }
    }

    void checkFixedBeaconTime() { // En modo no-SmartBeacon: fuerza beacon fijo por minutos
        if (!smartBeaconActive) {
            uint32_t lastTxSmartBeacon = millis() - lastTxTime;
            if (lastTxSmartBeacon >= Config.nonSmartBeaconRate * 60 * 1000) sendUpdate = true;
        }
    }

    void checkState() { // Inhibe SmartBeacon si Winlink activo o hay solicitud WX en progreso
        if (wxRequestStatus && (millis() - wxRequestTime) > 20000) wxRequestStatus = false; // Timeout de 20 s para solicitud WX
        smartBeaconActive = (winlinkStatus == 0 && !wxRequestStatus) ? currentBeacon->smartBeaconActive : false;
    }
    
}
