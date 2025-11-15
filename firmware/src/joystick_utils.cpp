#include "joystick_utils.h"
#include "configuration.h"
#include "keyboard_utils.h"
#include "board_pinout.h"
#include "button_utils.h"

extern  int                     menuDisplay; // Estado/índice actual del menú
extern  Configuration           Config;      // Configuración global (JSON / estructura)

// Flag usado para indicar que el joystick pidió ejecutar una acción
// que debe finalizarse fuera del contexto de la ISR (evitar SPIFFS/IO en ISR).
bool    exitJoystickInterrupt  = false;

typedef void (*DirectionFunc)(); // Tipo función para handlers de dirección (puntero a función)

#ifdef HAS_JOYSTICK

    namespace JOYSTICK_Utils {

        // Tiempo de "debounce" en ms para evitar rebotes múltiples del joystick
        int         debounceDelay       = 400;
        // Marca de tiempo de la última interrupción válida del joystick
        uint32_t    lastInterruptTime   = 0;

        // Comprueba si ha pasado suficiente tiempo desde la última interrupción
        // para considerarla válida (debounce software).
        bool checkLastJoystickInterrupTime() {
            if ((millis() - lastInterruptTime) > debounceDelay) {
                lastInterruptTime = millis(); // actualizar marca de tiempo
                return true;                 // permitir la acción
            } else {
                return false;                // ignorar (rebote o pulsación muy cercana)
            }
        }

        // Determina si, en el menú actual, una acción desde ISR debe
        // "salir" del contexto de la interrupción y ejecutarse de forma segura
        // desde el loop principal (p. ej. operaciones con SPIFFS o I/O pesados).
        bool checkMenuDisplayToExitInterrupt(int menu) {
            // Lista de menús que requieren ejecución fuera de la ISR:
            // 10, 120, 130..133, 200, 210, 1300, 1310, 2210..2212, 51, 50100..50101, 50110..50111, 9001
            if (menu == 10 || menu == 120  || (menu >= 130 && menu <= 133) || menu == 200 || menu == 210 || menu == 1300 || menu == 1310 || (menu >= 2210 && menu <= 2212) || menu == 51 || (menu >= 50100 && menu <= 50101) || (menu >= 50110 && menu <= 50111) || menu == 9001) {
                return true;    // Ejecución fuera de ISR necesaria (leer/editar/delete/guardar)
            } else {
                return false;   // Acción simple, puede ejecutarse desde ISR (o no requiere salida)
            }
        }

        // Ejecutado dentro del loop principal: si la ISR marcó `exitJoystickInterrupt`
        // y el menú actual permite la salida, llamamos a la rutina larga (longPress1).
        // Esto evita hacer operaciones costosas dentro de la ISR.
        void loop() {   // for running process with SPIFFS outside interrupt
            if (checkMenuDisplayToExitInterrupt(menuDisplay) && exitJoystickInterrupt) BUTTON_Utils::longPress1();
        }

        // Handler genérico para la interrupción del joystick.
        // directionFunc es la función que se desea ejecutar (p. ej. up/down/left/right).
        // Comportamiento:
        //  - Verifica debounce temporal.
        //  - Si el menú actual requiere salida de ISR y la función es longPress1,
        //    marca exitJoystickInterrupt = true para que el loop lo ejecute fuera de ISR.
        //  - En otro caso ejecuta directamente la función (rápida) desde ISR.
        void IRAM_ATTR joystickHandler(DirectionFunc directionFunc) {
            if (checkLastJoystickInterrupTime() && menuDisplay != 0) { // solo si no estamos en pantalla principal
                if (checkMenuDisplayToExitInterrupt(menuDisplay) && directionFunc == BUTTON_Utils::longPress1) {
                    // Marcar para ejecutar fuera de ISR (operación larga)
                    exitJoystickInterrupt = true;
                } else {
                    // Ejecutar acción rápida directamente (ej: navegación simple)
                    exitJoystickInterrupt = false;
                    directionFunc();
                }
            }
        }

        // Wrappers específicos que enlazan el joystick con las funciones de teclado/acciones.
        // Se declaran IRAM_ATTR para que la ISR sea lo más rápida y determinística posible.
        void IRAM_ATTR joystickUp() { joystickHandler(KEYBOARD_Utils::upArrow); }        // movimiento arriba
        void IRAM_ATTR joystickDown() { joystickHandler(KEYBOARD_Utils::downArrow); }    // movimiento abajo
        void IRAM_ATTR joystickLeft() { joystickHandler(KEYBOARD_Utils::leftArrow); }    // movimiento izquierda
        void IRAM_ATTR joystickRight() { joystickHandler(BUTTON_Utils::longPress1); }    // movimiento derecha = acción larga

        // Inicialización: configura pines y attachInterrupt para cada dirección del joystick.
        // Solo si NO estamos en modo simplificado (Config.simplifiedTrackerMode == false).
        void setup() {
            if (!Config.simplifiedTrackerMode) {
                // Configurar pines del joystick como INPUT_PULLUP para lecturas por pulsación.
                pinMode(JOYSTICK_CENTER, INPUT_PULLUP);
                pinMode(JOYSTICK_UP, INPUT_PULLUP);
                pinMode(JOYSTICK_DOWN, INPUT_PULLUP);
                pinMode(JOYSTICK_LEFT, INPUT_PULLUP);
                pinMode(JOYSTICK_RIGHT, INPUT_PULLUP);

                // Enlazar interrupciones para cada dirección; usar FALLING para boton activo a masa.
                attachInterrupt(digitalPinToInterrupt(JOYSTICK_UP), joystickUp, FALLING);
                attachInterrupt(digitalPinToInterrupt(JOYSTICK_DOWN), joystickDown, FALLING);
                attachInterrupt(digitalPinToInterrupt(JOYSTICK_LEFT), joystickLeft, FALLING);
                attachInterrupt(digitalPinToInterrupt(JOYSTICK_RIGHT), joystickRight, FALLING);
            }
        }
    }

#endif