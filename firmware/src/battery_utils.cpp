

#include <Arduino.h>
#include "configuration.h"
#include "battery_utils.h"
#include "board_pinout.h"
#include "power_utils.h"
#include "display.h"


// -----------------------------------------------------------------------------
// Variables relacionadas con control ADC (si está definido ADC_CTRL)
// -----------------------------------------------------------------------------
#ifdef ADC_CTRL
    uint32_t    adcCtrlTime         = 0;    // Marca de tiempo para controlar estabilización del ADC
    uint8_t     measuringState      = 0;    // Estado de la máquina de estados para la medición
#endif

// -----------------------------------------------------------------------------
// Soporte para PMU externas (AXP192 / AXP2101)
// Si el hardware usa alguna de estas PMU, se declara extern para usar la instancia
// -----------------------------------------------------------------------------
#ifdef HAS_AXP192
    extern XPowersAXP192 PMU;
#endif
#ifdef HAS_AXP2101
    extern XPowersAXP2101 PMU;
#endif


// -----------------------------------------------------------------------------
// Variables globales del módulo batería
// -----------------------------------------------------------------------------
extern      Configuration           Config;                     // Configuración global externa
uint32_t    batteryMeasurmentTime   = 0;                        // Última vez que se midió la batería
int         averageReadings         = 20;                       // Cantidad de lecturas ADC para promediar

String      batteryVoltage          = "";                       // Cadena con voltaje formateado
bool        batteryConnected        = false;                    // Flag si la batería está conectada

extern      String                  batteryChargeDischargeCurrent; // Variable externa usada para corriente

// Corrección porcentual para lecturas en ciertas placas Lora32 (ajustable)
float       lora32BatReadingCorr    = 6.5; // % de corrección para llevar lectura ADC a voltaje real


namespace BATTERY_Utils {

    // Devuelve un string con el porcentaje calculado a partir del voltaje.
    // El rango asumido es 3.0V (0%) - 4.2V (100%).
    String getPercentVoltageBattery(float voltage) {
        int percent = ((voltage - 3.0) / (4.2 - 3.0)) * 100;
        // Formatea para que siempre tenga al menos 2 espacios antes si es <10 (alineado visual)
        return (percent < 100) ? (((percent < 10) ? "  ": " ") + String(percent)) : "100";
    }

    // Retorna la cadena que contiene el voltaje de batería formateado
    String getBatteryInfoVoltage() {
        return batteryVoltage;
    }

    // Lee el voltaje de la batería. El comportamiento depende del hardware detectado:
    // - Si hay AXP192/AXP2101, pregunta a la PMU.
    // - Si hay pin ADC definido, hace lecturas promediadas y aplica divisores/correcciones
    float readBatteryVoltage() {
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            // Si hay PMU, usar su lectura (devuelve mV, por eso /1000.0)
            return (PMU.getBattVoltage() / 1000.0);
        #else
            #ifdef BATTERY_PIN
                // Lectura analógica por ADC con promedio para reducir ruido
                int sampleSum = 0;
                analogRead(BATTERY_PIN);    // Lectura dummy para estabilizar ADC
                delay(1);
                for (int i = 0; i < averageReadings; i++) {
                    sampleSum += analogRead(BATTERY_PIN);
                    delay(3);               // pequeño retardo entre lecturas
                }
                int adc_value = sampleSum/averageReadings;
                double voltage = (adc_value * 3.3 ) / 4095.0; // Conversión básica ADC -> V (12-bit)

                // A continuación, distintos bloques para placas específicas que tienen
                // divisores de tensión distintos o requieren offsets/correcciones.

                #ifdef LIGHTTRACKER_PLUS_1_0
                    // Divisor 560k + 100k (100k en bajo). Se calcula factor y se compensa.
                    double inputDivider = (1.0 / (560.0 + 100.0)) * 100.0;
                    return (voltage / inputDivider) + 0.1; // +0.1 por no-linealidad/offset medido
                #endif

                // Placas LORA32 y variantes: normalmente usan divisor 2:1 (2 * V)
                #if defined(TTGO_T_Beam_V0_7) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_GPS_915) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(TTGO_T_LORA32_V2_1_TNC_915) || defined(ESP32_DIY_LoRa_GPS) || defined(ESP32_DIY_LoRa_GPS_915) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(ESP32_DIY_1W_LoRa_GPS_915) || defined(ESP32_DIY_1W_LoRa_GPS_LLCC68) || defined(OE5HWN_MeshCom) || defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS) || defined(ESP32S3_DIY_LoRa_GPS) || defined(ESP32S3_DIY_LoRa_GPS_915) || defined(TROY_LoRa_APRS) || defined(RPC_Electronics_1W_LoRa_GPS)
                    // Se multiplica por 2 por el divisor 2:1 y se añade +0.1V por offset del ADC,
                    // además se aplica la corrección porcentual configurada para Lora32.
                    return (2 * (voltage + 0.1)) * (1 + (lora32BatReadingCorr/100)); // Lectura corregida
                #endif

                // Ejemplo: placas Heltec V3 y otras con divisor 390k + 100k
                #if defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC) || defined(HELTEC_WIRELESS_TRACKER) || defined(HELTEC_WSL_V3_GPS_DISPLAY) || defined(ESP32_C3_DIY_LoRa_GPS) || defined(ESP32_C3_DIY_LoRa_GPS_915) || defined(WEMOS_ESP32_Bat_LoRa_GPS)
                    double inputDivider = (1.0 / (390.0 + 100.0)) * 100.0;  // 390k + 100k
                    // Se añade offset grande porque el ADC en algunos chips (ESP32-S3 etc.) es impreciso.
                    return (voltage / inputDivider) + 0.285;
                #endif

                // Otra familia: Heltec V2 y algunas variantes con divisor 220k + 100k
                #if defined(HELTEC_V2_GPS) || defined(HELTEC_V2_GPS_915) || defined(HELTEC_V2_TNC) || defined(F4GOH_1W_LoRa_Tracker) || defined(F4GOH_1W_LoRa_Tracker_LLCC68)
                    double inputDivider = (1.0 / (220.0 + 100.0)) * 100.0;  // 220k + 100k
                    return (voltage / inputDivider) + 0.285;
                #endif

            #else
                // No hay pin de bater�a definido ni PMU, devolver 0.0 para indicar no disponible
                return 0.0;
            #endif
        #endif
    }

    // Actualiza variables globales con la información de la batería.
    // - Si hay PMU, verifica si la batería está conectada y lee corriente de carga/descarga.
    // - Si no hay PMU, se obtiene el voltaje vía ADC y se considera conectada si >1.5V
    void obtainBatteryInfo() {
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            batteryConnected = PMU.isBatteryConnect(); // Pregunta a la PMU si la batería está física conectada
            if (batteryConnected) {
                batteryVoltage                  = String(readBatteryVoltage(), 2); // Formatea con 2 decimales
                batteryChargeDischargeCurrent   = String(POWER_Utils::getBatteryChargeDischargeCurrent(), 0); // Corriente como entero
            }
        #else
            batteryVoltage = String(readBatteryVoltage(), 2);
            if (batteryVoltage.toFloat() > 1.5) batteryConnected = true; // Umbral simple para detectar batería
        #endif
    }   

    // Función de monitor que se debe llamar periódicamente desde el loop principal.
    // Controla cada cuánto tiempo se mide la batería y maneja la lógica de ADC_CTRL
    // (encender/desconectar MOSFET que alimenta el divisor para ahorrar energía).
    void monitor() {
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            // Si hay PMU, medir al menos 1 vez por segundo
            if (batteryMeasurmentTime == 0 || (millis() - batteryMeasurmentTime) > 1 * 1000){
                obtainBatteryInfo();
                POWER_Utils::handleChargingLed(); // Actualizar LED según estado de carga
                batteryMeasurmentTime = millis();
            }
        #elif defined(BATTERY_PIN)
            // Si usamos BATTERY_PIN por ADC, medimos como máximo cada 30s (para ahorrar energía)
            if (batteryMeasurmentTime == 0 || (millis() - batteryMeasurmentTime) > 30 * 1000){ // 30 segundos
                #ifdef ADC_CTRL
                    // Si existe control por MOSFET (ADC_CTRL), usamos máquina de estados:
                    switch(measuringState){
                        case 0:     // Estado inicial: encender ADC_CTRL, esperar y medir
                            POWER_Utils::adc_ctrl_ON();
                            adcCtrlTime = millis();
                            delay(50);
                            obtainBatteryInfo();
                            POWER_Utils::adc_ctrl_OFF();
                            measuringState = 1;
                            break;
                        case 1:     // Estado ADC_CTRL_OFF, siguiente llamada enciende ADC_CTRL
                            POWER_Utils::adc_ctrl_ON();
                            adcCtrlTime = millis();
                            measuringState = 2;
                            break;
                        case 2:     // Se espera que pasen al menos 50ms para que la tensión se estabilice
                            if((millis() - adcCtrlTime) > 50){ // 50ms de estabilización
                                obtainBatteryInfo();
                                POWER_Utils::adc_ctrl_OFF();
                                measuringState = 1;
                                
                                // Si el voltaje cae por debajo del umbral de sleepVoltage - 0.1 => forzar apagado
                                if (batteryVoltage.toFloat() < (Config.battery.sleepVoltage - 0.1)) {
                                    displayShow("!BATTERY!", "", "LOW BATTERY VOLTAGE!",5000);
                                    POWER_Utils::shutdown(); // Apagado seguro por batería baja
                                }
                            }
                            break;
                    }
                #else
                    // Si no hay control ADC (ADC_CTRL), medir directamente
                    obtainBatteryInfo();
                #endif
                batteryMeasurmentTime = millis();
            }
        #endif
    }
    

}