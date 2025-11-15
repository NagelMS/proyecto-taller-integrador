#include <OneButton.h>
#include "keyboard_utils.h"
#include "configuration.h"
#include "board_pinout.h"
#include "button_utils.h"
#include "power_utils.h"
#include "display.h"

#ifdef BUTTON_PIN

    extern Configuration    Config;
    extern int              menuDisplay;
    extern uint32_t         displayTime;
    extern uint32_t         menuTime;


    namespace BUTTON_Utils {
        
        OneButton userButton = OneButton(BUTTON_PIN, true, true);

        #ifdef RPC_Electronics_1W_LoRa_GPS
            OneButton userButton2 = OneButton(BUTTON2_PIN, true, true);
            OneButton userButton3 = OneButton(BUTTON3_PIN, true, true);
            OneButton userButton4 = OneButton(BUTTON4_PIN, true, true);
        #endif
        
        // singlePress1()
        // Maneja una pulsación simple del botón principal: actualiza el temporizador del menú
        // y simula la acción de "flecha abajo" en la navegación del menú.
        void singlePress1() {
            menuTime = millis();
            KEYBOARD_Utils::downArrow();
        }
        #ifdef RPC_Electronics_1W_LoRa_GPS
            void singlePress2() {
                menuTime = millis();
                KEYBOARD_Utils::upArrow();
            }
            void singlePress3() {
                menuTime = millis();
                KEYBOARD_Utils::rightArrow();
            }
            void singlePress4() {
                menuTime = millis();
                KEYBOARD_Utils::leftArrow();
            }
        #endif
        
        // longPress1()
        // Maneja una pulsación larga del botón principal: actualiza el temporizador del menú
        // y ejecuta la acción asociada a "flecha derecha" (usada aquí como acción prolongada).
        void longPress1() {
            menuTime = millis();
            KEYBOARD_Utils::rightArrow();
        }
        // doublePress1()
        // Maneja una pulsación doble: enciende la pantalla si está apagada o vuelve al estado
        // principal (menuDisplay = 0) si está en un submenú; actualiza timers relevantes.
        void doublePress1() {
            displayToggle(true);
            menuTime = millis();
            if (menuDisplay == 0) {
                menuDisplay = 1;
            } else if (menuDisplay > 0) {
                menuDisplay = 0;
                displayTime = millis();
            }
        }
        // multiPress1()
        // Maneja múltiples pulsaciones rápidas: enciende la pantalla, actualiza el temporizador
        // y cambia el menú a la entrada especial multi-press (menuDisplay = 9000).
        void multiPress1() {
            displayToggle(true);
            menuTime = millis();
            menuDisplay = 9000;
        }

        // loop()
        // Debe llamarse periódicamente desde el loop principal: procesa el objeto OneButton
        // para detectar eventos (click, long press, double, multi) y despachar callbacks.
        // No hace nada si estamos en modo simplificado.
        void loop() {
            if (!Config.simplifiedTrackerMode) {
                userButton.tick();
                #ifdef RPC_Electronics_1W_LoRa_GPS
                    userButton2.tick();
                    userButton3.tick();
                    userButton4.tick();
                #endif
            }
        }
        // setup()
        // Inicializa los callbacks del/los botones (attachClick, attachLongPressStart, etc.)
        // y se llama una vez en el arranque. No registra handlers si estamos en modo simplificado.
        void setup() {
            if (!Config.simplifiedTrackerMode) {
                userButton.attachClick(singlePress1);
                userButton.attachLongPressStart(longPress1);
                userButton.attachDoubleClick(doublePress1);
                userButton.attachMultiClick(multiPress1);
                #ifdef RPC_Electronics_1W_LoRa_GPS
                    userButton2.attachClick(singlePress2);
                    userButton3.attachClick(singlePress3);
                    userButton4.attachClick(singlePress4);
                #endif
            }
        }

    }

#endif