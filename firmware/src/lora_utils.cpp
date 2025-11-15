#include <RadioLib.h>
#include <logger.h>
#include <SPI.h>
#include "notification_utils.h"
#include "configuration.h"
#include "board_pinout.h"
#include "lora_utils.h"
#include "display.h"

// Variables externas definidas en otros módulos
extern logging::Logger  logger;             // Logger global
extern Configuration    Config;             // Configuración global cargada desde JSON u otro sitio
extern LoraType         *currentLoRaType;   // Puntero a la configuración LoRa actualmente seleccionada
extern uint8_t          loraIndex;          // Índice de la configuración LoRa actual
extern int              loraIndexSize;      // Tamaño del arreglo de configuraciones LoRa

// Flags de control del módulo LoRa
bool operationDone   = true;   // Indica si la operación asíncrona (Rx/Tx) ha finalizado
bool transmitFlag    = true;   // Control para alternar entre recibir y transmitir

// Instanciación del objeto radio según el chip disponible en la placa
#if defined(HAS_SX1262)
    // Para SX1262 se crea un Module con los pines CS, DIO1, RST, BUSY
    SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
#endif

#if defined(HAS_SX1268)
    #if defined(LIGHTTRACKER_PLUS_1_0)
        // En la placa LIGHTTRACKER_PLUS_1_0 se usa un SPIClass dedicado
        SPIClass loraSPI(FSPI);
        SX1268 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN, loraSPI); 
    #else
        // Modo estándar para SX1268
        SX1268 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
    #endif
#endif

#if defined(HAS_SX1278)
    // SX1278: constructor con pines CS, BUSY, RST (orden específico para esta librería)
    SX1278 radio = new Module(RADIO_CS_PIN, RADIO_BUSY_PIN, RADIO_RST_PIN);
#endif

#if defined(HAS_SX1276)
    // SX1276: similar a SX1278
    SX1276 radio = new Module(RADIO_CS_PIN, RADIO_BUSY_PIN, RADIO_RST_PIN);
#endif

#if defined(HAS_LLCC68)
    // LLCC68: módulo LoRa distinto (soporta SF entre 5 y 11)
    LLCC68 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
#endif


namespace LoRa_Utils {

    // Callback asociado a DIO que marca el fin de la operación
    void setFlag(void) {
        operationDone = true;
    }

    // Cambia a la siguiente configuración/frecuencia LoRa disponible
    void changeFreq() {
        // Incrementar índice circularmente
        if (loraIndex >= (loraIndexSize - 1)) {
            loraIndex = 0;
        } else {
            loraIndex++;
        }
        // Actualizar puntero a la configuración actual
        currentLoRaType = &Config.loraTypes[loraIndex];

        // Configurar parámetros del radio basados en la estructura currentLoRaType
        float freq = (float)currentLoRaType->frequency / 1000000.0; // frecuencia en MHz
        radio.setFrequency(freq);
        radio.setSpreadingFactor(currentLoRaType->spreadingFactor);
        float signalBandwidth = currentLoRaType->signalBandwidth / 1000.0; // en kHz
        radio.setBandwidth(signalBandwidth);
        radio.setCodingRate(currentLoRaType->codingRate4);

        // Ajuste de potencia según el chip/plataforma
        #if (defined(HAS_SX1268) || defined(HAS_SX1262)) && !defined(HAS_1W_LORA)
            // Algunos módulos requieren un offset de potencia
            radio.setOutputPower(currentLoRaType->power + 2); // ejemplo: mapa 20->22
        #endif
        #if defined(HAS_SX1278) || defined(HAS_SX1276) || defined(HAS_1W_LORA)
            radio.setOutputPower(currentLoRaType->power);
        #endif

        // Construir una cadena informativa de la configuración actual
        String loraCountryFreq;
        switch (loraIndex) {
            case 0: loraCountryFreq = "EU/WORLD"; break;
            case 1: loraCountryFreq = "POLAND"; break;
            case 2: loraCountryFreq = "UK"; break;
        }
        String currentLoRainfo = "LoRa ";
        currentLoRainfo += loraCountryFreq;
        currentLoRainfo += " / Freq: ";
        currentLoRainfo += String(currentLoRaType->frequency);
        currentLoRainfo += " / SF:";
        currentLoRainfo += String(currentLoRaType->spreadingFactor);
        currentLoRainfo += " / CR: ";
        currentLoRainfo += String(currentLoRaType->codingRate4);
        
        // Log y feedback visual
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "LoRa", currentLoRainfo.c_str());
        displayShow("LORA FREQ>", "", "CHANGED TO: " + loraCountryFreq, "", "", "", 2000);
    }

    // Inicializa el hardware SPI y el radio según la configuración actual
    void setup() {
        #ifdef LIGHTTRACKER_PLUS_1_0
            // Alimentar externamente el módulo radio (si la placa lo requiere)
            pinMode(RADIO_VCC_PIN, OUTPUT);
            digitalWrite(RADIO_VCC_PIN, HIGH);
        #endif

        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "LoRa", "Set SPI pins!");

        // Inicializar SPI con pines personalizados o los por defecto
        #if defined(LIGHTTRACKER_PLUS_1_0)
            loraSPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CS_PIN);
        #else
            SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);
        #endif

        // Preparar frecuencia inicial (MHz)
        float freq = (float)currentLoRaType->frequency / 1000000.0;

        #if defined(RADIO_HAS_XTAL)
            // Indicar a la librería que el módulo tiene cristal externo
            radio.XTAL = true;
        #endif

        // Iniciar el radio y comprobar estado
        int state = radio.begin(freq);
        if (state == RADIOLIB_ERR_NONE) {
            #if defined(HAS_SX1262) || defined(HAS_SX1268)
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Initializing SX126X ...");
            #else
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Initializing SX127X ...");
            #endif
        } else {
            // Si falla la inicialización, log y bloquear (fail-safe)
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Starting LoRa failed! State: %d", state);
            while (true); // detener ejecución (se puede reemplazar por reboot o manejador)
        }

        // Asociar callbacks DIO según el radio para detectar fin de operación
        #if defined(HAS_SX1262) || defined(HAS_SX1268) || defined(HAS_LLCC68)
            radio.setDio1Action(setFlag);
        #endif
        #if defined(HAS_SX1278) || defined(HAS_SX1276)
            radio.setDio0Action(setFlag, RISING);
        #endif

        // Ajustar parámetros provenientes de la configuración
        radio.setSpreadingFactor(currentLoRaType->spreadingFactor);
        float signalBandwidth = currentLoRaType->signalBandwidth / 1000.0;
        radio.setBandwidth(signalBandwidth);
        radio.setCodingRate(currentLoRaType->codingRate4);
        radio.setCRC(true); // activar CRC

        // Configuración de pines de conmutación Rx/Tx si existen
        #if defined(RADIO_RXEN) && defined(RADIO_TXEN)
            radio.setRfSwitchPins(RADIO_RXEN, RADIO_TXEN);
        #endif

        // Ajustes específicos para módulos 1W o ajustes de potencia por familia
        #ifdef HAS_1W_LORA
            // Para módulos Ebyte E22/E220: ajuste de potencia y límite de corriente
            state = radio.setOutputPower(currentLoRaType->power);
            radio.setCurrentLimit(140); // valor a validar según módulo
        #endif

        #if (defined(HAS_SX1268) || defined(HAS_SX1262)) && !defined(HAS_1W_LORA)
            // Aumentar potencia con offset en SX126x en placas estándar
            state = radio.setOutputPower(currentLoRaType->power + 2);
            radio.setCurrentLimit(140);
        #endif
        
        #if defined(HAS_SX1278) || defined(HAS_SX1276)
            state = radio.setOutputPower(currentLoRaType->power);
            radio.setCurrentLimit(100); // valor a validar
        #endif

        // Mejora de ganancia RX para chips que lo soportan
        #if defined(HAS_SX1262) || defined(HAS_SX1268) || defined(HAS_LLCC68)
            radio.setRxBoostedGainMode(true);
        #endif

        // Verificar resultado final de configuración
        if (state == RADIOLIB_ERR_NONE) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "LoRa init done!");
        } else {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Starting LoRa failed! State: %d", state);
            while (true);
        }        
    }

    // Envía un nuevo paquete LoRa (texto) con pre/post acciones (PTT, LED, buzzer)
    void sendNewPacket(const String& newPacket) {
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa Tx","---> %s", newPacket.c_str());

        // Si se usa PTT, activarlo antes de transmitir
        if (Config.ptt.active) {
            digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? LOW : HIGH);
            delay(Config.ptt.preDelay); // retardo previo definido en configuración
        }

        // Indicar transmisión por LED / buzzer si está configurado
        if (Config.notification.ledTx) digitalWrite(Config.notification.ledTxPin, HIGH);
        if (Config.notification.buzzerActive && Config.notification.txBeep) NOTIFICATION_Utils::beaconTxBeep();
        
        // Transmitir: se antepone el encabezado 0x3c 0xff 0x01 (protocolo interno)
        int state = radio.transmit("\x3c\xff\x01" + newPacket);
        transmitFlag = true;

        // Comprobación de resultado
        if (state == RADIOLIB_ERR_NONE) {
            // éxito (se puede añadir log si se desea)
        } else {
            Serial.print(F("failed, code "));
            Serial.println(state);
        }
        
        // Apagar indicador LED y desactivar PTT tras la transmisión
        if (Config.notification.ledTx) digitalWrite(Config.notification.ledTxPin, LOW);
        if (Config.ptt.active) {
            delay(Config.ptt.postDelay);
            digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? HIGH : LOW);
        }

        /* Opcional: limpiar TFT si aplica
        #ifdef HAS_TFT
            cleanTFT();
        #endif
        */
    }

    // Inicia el receptor (modo continuo de recepción)
    void wakeRadio() {
        radio.startReceive();
    }

    // Lectura simplificada de datos cuando el radio está en modo sleep/standby
    ReceivedLoRaPacket receiveFromSleep() {
        ReceivedLoRaPacket receivedLoraPacket;
        String packet = "";
        int state = radio.readData(packet); // lectura bloqueante o no bloqueante según la librería
        if (state == RADIOLIB_ERR_NONE) {
            // Rellenar estructura con métricas del paquete recibido
            receivedLoraPacket.text       = packet;
            receivedLoraPacket.rssi       = radio.getRSSI();
            receivedLoraPacket.snr        = radio.getSNR();
            receivedLoraPacket.freqError  = radio.getFrequencyError();
        } else {
            // en caso de error se devuelve estructura vacía (se puede loggear si se desea)
        }
        return receivedLoraPacket;
    }

    // Lectura de paquete en modo normal, con control de operación asíncrona
    ReceivedLoRaPacket receivePacket() {
        ReceivedLoRaPacket receivedLoraPacket;
        String packet = "";
        if (operationDone) {          // solo procesar cuando la operación anterior haya terminado
            operationDone = false;    // bloquear hasta completar esta iteración
            if (transmitFlag) {
                // Si previamente transmitimos, arrancar modo recepción
                radio.startReceive();
                transmitFlag = false;
            } else {
                // Intentar leer datos si no estamos en la transición Tx->Rx
                int state = radio.readData(packet);
                if (state == RADIOLIB_ERR_NONE) {
                    if (!packet.isEmpty()) {
                        // Log del payload (omitimos los 3 bytes de encabezado al mostrar)
                        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa Rx","---> %s", packet.substring(3).c_str());
                        receivedLoraPacket.text       = packet;
                        receivedLoraPacket.rssi       = radio.getRSSI();
                        receivedLoraPacket.snr        = radio.getSNR();
                        receivedLoraPacket.freqError  = radio.getFrequencyError();
                    }
                } else {
                    // Manejo de error de lectura (p. ej. CRC mismatch u otros códigos)
                    Serial.print(F("failed, code "));
                    Serial.println(state);
                }
            }
        }
        return receivedLoraPacket;
    }

    // Poner el radio en modo sleep para ahorrar energía
    void sleepRadio() {
        radio.sleep();
    }

}