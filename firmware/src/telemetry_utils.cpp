#include <APRSPacketLib.h>
#include <Arduino.h>
#include <vector>
#include "telemetry_utils.h"
#include "configuration.h"
#include "station_utils.h"
#include "battery_utils.h"
#include "lora_utils.h"
#include "wx_utils.h"
#include "display.h"

// -----------------------------------------------------------------------------
// APRS Telemetry: EQNS/UNIT/PARM y codificación Base91
// -----------------------------------------------------------------------------


extern Configuration    Config;
extern Beacon           *currentBeacon;
extern int              wxModuleType;
extern bool             sendStartTelemetry;

int telemetryCounter    = random(1,999);


namespace TELEMETRY_Utils {

    String joinWithCommas(const std::vector<String>& items) { // Une campos con comas para telemetría
        String result;
        for (size_t i = 0; i < items.size(); ++i) {
            result += items[i];
            if (i < items.size() - 1) result += ",";
        }
        return result;
    }

    std::vector<String> getEquationCoefficients() { // Coeficientes EQNS (a*x+b)+c por sensor
        std::vector<String> coefficients;
        if (Config.battery.sendVoltage && Config.battery.voltageAsTelemetry) coefficients.push_back("0,0.01,0");
        if (Config.telemetry.sendTelemetry) {
            switch (wxModuleType) {
                case 1: coefficients.insert(coefficients.end(), {"0,0.1,-50", "0,0.01,0", "0,0.125,0"}); break;
                case 2: coefficients.insert(coefficients.end(), {"0,0.1,-50", "0,0.125,0"}); break;
                case 3: coefficients.insert(coefficients.end(), {"0,0.1,-50", "0,0.01,0", "0,0.125,0", "0,0.01,0"}); break;
                case 4: coefficients.insert(coefficients.end(), {"0,0.1,-50", "0,0.01,0"}); break;
            }
        }
        return coefficients;
    }

    std::vector<String> getUnitLabels() { // Etiquetas UNIT por canal (VDC, C, %, hPa, etc.)
        std::vector<String> labels;
        if (Config.battery.sendVoltage && Config.battery.voltageAsTelemetry) labels.push_back("VDC");
        if (Config.telemetry.sendTelemetry) {
            switch (wxModuleType) {
                case 1: labels.insert(labels.end(), {"C", "%", "hPa"}); break;
                case 2: labels.insert(labels.end(), {"C", "hPa"}); break;
                case 3: labels.insert(labels.end(), {"C", "%", "hPa", "%"}); break;
                case 4: labels.insert(labels.end(), {"C", "%"}); break;
            }
        }
        return labels;
    }

    std::vector<String> getParameterNames() { // Nombres PARM por canal (Celsius, Rel_Hum, etc.)
        std::vector<String> names;
        if (Config.battery.sendVoltage && Config.battery.voltageAsTelemetry) names.push_back("Voltage");
        if (Config.telemetry.sendTelemetry) {
            switch (wxModuleType) {
                case 1: names.insert(names.end(), {"Celsius", "Rel_Hum", "Atm_Press"}); break;
                case 2: names.insert(names.end(), {"Celsius", "Atm_Press"}); break;
                case 3: names.insert(names.end(), {"Celsius", "Rel_Hum", "Atm_Press", "GAS"}); break;
                case 4: names.insert(names.end(), {"Celsius", "Rel_Hum"}); break;
            }
        }
        return names;
    }

    void sendEquationCoefficients() { // Envía tramas "EQNS." con coeficientes
        String equationPacket = "EQNS." + joinWithCommas(getEquationCoefficients());
        String tempPacket = APRSPacketLib::generateMessagePacket(currentBeacon->callsign, "APLRT1", Config.path, currentBeacon->callsign, equationPacket);
        displayShow("<<< TX >>>", "Telemetry Packet:", "Equation Coefficients", 100);
        LoRa_Utils::sendNewPacket(tempPacket);
    }

    void sendUnitLabels() { // Envía tramas "UNIT." con etiquetas
        String unitPacket = "UNIT." + joinWithCommas(getUnitLabels());
        String tempPacket = APRSPacketLib::generateMessagePacket(currentBeacon->callsign, "APLRT1", Config.path, currentBeacon->callsign, unitPacket);
        displayShow("<<< TX >>>", "Telemetry Packet:", "Unit/Label", 100);
        LoRa_Utils::sendNewPacket(tempPacket);
    }

    void sendParameterNames() { // Envía tramas "PARM." con nombres de parámetros
        String parameterPacket = "PARM." + joinWithCommas(getParameterNames());
        String tempPacket = APRSPacketLib::generateMessagePacket(currentBeacon->callsign, "APLRT1", Config.path, currentBeacon->callsign, parameterPacket);
        displayShow("<<< TX >>>", "Telemetry Packet:", "Parameter Name",100);
        LoRa_Utils::sendNewPacket(tempPacket);
    }

    void sendEquationsUnitsParameters() { // Secuencia de EQNS→UNIT→PARM con pausas de 3 s
        sendEquationCoefficients();
        delay(3000);
        sendUnitLabels();
        delay(3000);
        sendParameterNames();
        delay(3000);
        sendStartTelemetry = false;
    }

    String generateEncodedTelemetryBytes(float value, bool counterBytes, byte telemetryType) { // Codifica en 2 bytes (91-base) según tipo
        String encodedBytes;
        int tempValue;

        if (counterBytes) {
            tempValue = value;
        } else {
            switch (telemetryType) {
                case 0: tempValue = value * 100; break;         // Voltaje/Humedad/Gas → 0..9999 escala         // Internal voltage (0-4,2V), Humidity, Gas calculation
                case 1: tempValue = (value * 100) / 2; break;   // Voltaje externo 0..15 V   // External voltage calculation (0-15V)
                case 2: tempValue = (value * 10) + 500; break;  // Temperatura (offset +500)  // Temperature
                case 3: tempValue = (value * 8); break;         // Presión hPa * 8         // Pressure
                default: tempValue = value; break;
            }
        }        

        int firstByte   = tempValue / 91;
        tempValue       -= firstByte * 91;

        encodedBytes    = char(firstByte + 33);
        encodedBytes    += char(tempValue + 33);
        return encodedBytes;
    }

    String generateEncodedTelemetry() { // Construye "|<CNT><CH1><CH2>...|" con canales habilitados
        String telemetry = "|";
        telemetry += generateEncodedTelemetryBytes(telemetryCounter, true, 0);
        telemetryCounter++;
        if (telemetryCounter == 1000) telemetryCounter = 0;
        
        if (Config.battery.sendVoltage && Config.battery.voltageAsTelemetry) {  
            String batteryVoltage = BATTERY_Utils::getBatteryInfoVoltage();
            telemetry += generateEncodedTelemetryBytes(batteryVoltage.toFloat(), false, 0); // voltage
        }
        if (Config.telemetry.sendTelemetry) telemetry += WX_Utils::readDataSensor(0);
        telemetry += "|";
        return telemetry;
    }

}
