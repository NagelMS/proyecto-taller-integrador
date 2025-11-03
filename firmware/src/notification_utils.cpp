#include "notification_utils.h"
#include "configuration.h"

// -----------------------------------------------------------------------------
// Alertas sonoras: tonos de arranque, TX, mensajes, bajo voltaje y apagado
// -----------------------------------------------------------------------------

uint8_t channel                 = 0;
uint8_t resolution              = 8; 
uint8_t pauseDuration           = 20;

int     startUpSound[]          = {440, 880, 440, 1760};
uint8_t startUpSoundDuration[]  = {100, 100, 100, 200};

int     shutDownSound[]         = {1720, 880, 400};
uint8_t shutDownSoundDuration[] = {60, 60, 200};

extern Configuration    Config;
extern bool             digipeaterActive;

namespace NOTIFICATION_Utils {

    void playTone(int frequency, uint8_t duration) { // Genera tono PWM en buzzer según frecuencia y duración {
        ledcSetup(channel, frequency, resolution);
        ledcAttachPin(Config.notification.buzzerPinTone, 0);
        ledcWrite(channel, 128);
        delay(duration);
        ledcWrite(channel, 0);
        delay(pauseDuration);
    }

    void beaconTxBeep() { // Beep de transmisión; doble si es digipeater {
        digitalWrite(Config.notification.buzzerPinVcc, HIGH);
        playTone(1320,100);
        if (digipeaterActive) {
            playTone(1560,100);
        }
        digitalWrite(Config.notification.buzzerPinVcc, LOW);
    }

    void messageBeep() { // Beep doble para mensaje recibido {
        digitalWrite(Config.notification.buzzerPinVcc, HIGH);
        playTone(1100,100);
        playTone(1100,100);
        digitalWrite(Config.notification.buzzerPinVcc, LOW);
    }

    void stationHeardBeep() { // Beep al escuchar otra estación {
        digitalWrite(Config.notification.buzzerPinVcc, HIGH);
        playTone(1200,100);
        playTone(600,100);
        digitalWrite(Config.notification.buzzerPinVcc, LOW);
    }

    void shutDownBeep() { // Secuencia descendente al apagar {
        digitalWrite(Config.notification.buzzerPinVcc, HIGH);
        for (int i = 0; i < sizeof(shutDownSound) / sizeof(shutDownSound[0]); i++) {
            playTone(shutDownSound[i], shutDownSoundDuration[i]);
        }
        digitalWrite(Config.notification.buzzerPinVcc, LOW);
    }

    void lowBatteryBeep() { // Beep alternante rápido por batería baja {
        digitalWrite(Config.notification.buzzerPinVcc, HIGH);
        playTone(1550,100);
        playTone(650,100);
        playTone(1550,100);
        playTone(650,100);
        digitalWrite(Config.notification.buzzerPinVcc, LOW);
    }

    void start() { // Secuencia ascendente de encendido (bootUp) {
        digitalWrite(Config.notification.buzzerPinVcc, HIGH);
        for (int i = 0; i < sizeof(startUpSound) / sizeof(startUpSound[0]); i++) {
            playTone(startUpSound[i], startUpSoundDuration[i]);
        }
        digitalWrite(Config.notification.buzzerPinVcc, LOW);
    }

}