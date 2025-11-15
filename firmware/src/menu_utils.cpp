#include <APRSPacketLib.h>
#include <TinyGPS++.h>
#include <vector>
#include "notification_utils.h"
#include "custom_characters.h"
#include "station_utils.h"
#include "configuration.h"
#include "battery_utils.h"
#include "board_pinout.h"
#include "power_utils.h"
#include "menu_utils.h"
#include "msg_utils.h"
#include "gps_utils.h"
#include "wx_utils.h"
#include "display.h"
#include "utils.h"


extern int                  menuDisplay;
extern Beacon               *currentBeacon;
extern Configuration        Config;
extern TinyGPSPlus          gps;
extern std::vector<String>  loadedAPRSMessages;
extern std::vector<String>  loadedWLNKMails;
extern int                  messagesIterator;
extern uint8_t              loraIndex;
extern uint32_t             menuTime;
extern bool                 symbolAvailable;
extern bool                 keyDetected;
extern bool                 keyboardConnected;
extern String               messageCallsign;
extern String               messageText;
extern bool                 flashlight;
extern bool                 digipeaterActive;
extern bool                 sosActive;
extern bool                 bluetoothActive;
extern bool                 displayEcoMode;
extern bool                 screenBrightness;
extern bool                 disableGPS;
extern bool                 showHumanHeading;
extern APRSPacket           lastReceivedPacket;

extern uint8_t              winlinkStatus;
extern String               winlinkMailNumber;
extern String               winlinkAddressee;
extern String               winlinkSubject;
extern String               winlinkBody;
extern String               winlinkAlias;
extern String               winlinkAliasComplete;
extern bool                 winlinkCommentState;

extern bool                 batteryConnected;
extern int                  wxModuleType;
extern bool                 gpsIsActive;

String      freqChangeWarning;
uint8_t     lowBatteryPercent       = 21;

#if defined(TTGO_T_DECK_PLUS) || defined(TTGO_T_DECK_GPS)
    String topHeader1   = "";
    String topHeader1_1 = "";
    String topHeader1_2 = "";
    String topHeader1_3 = "";
    String topHeader2   = "";
#endif


namespace MENU_Utils {

    // Devuelve el tipo de Bluetooth configurado
    const String checkBTType() {
        if (Config.bluetooth.useBLE) {      // Si BLE está habilitado
            return "BLE";                   // Se usa modo Bluetooth Low Energy
        } else {                             // Si no está habilitado BLE
            return "BT Clasico";            // Se usa Bluetooth clásico
        }
    }

    // Convierte un booleano a ON/OFF para mostrarlo en el menú
    const String checkProcessActive(const bool process) {
        if (process) {                      // Si el proceso está activo
            return "Encendido";
        } else {                            // Si el proceso está inactivo
            return "Apagado";
        }
    }

    // Convierte el valor numérico del brillo a un texto legible
    const String screenBrightnessAsString(const uint8_t bright) {
        #ifdef HAS_TFT                       // Lógica especial si el dispositivo tiene pantalla TFT
            if (bright == 255) {             // Brillo máximo
                return "Max";
            } else if (bright == 70) {       // Brillo bajo predefinido para TFT
                return "Bajo";
            } else {                         // Cualquier otro valor → brillo medio
                return "Medio";
            }
        #else                                // Dispositivos sin TFT (p.ej. OLED)
            if (bright == 255) {             // Brillo máximo
                return "Max";
            } else if (bright == 1) {        // Brillo bajo para pantallas sin TFT
                return "Bajo";
            } else {                         // Cualquier otro valor → brillo medio
                return "Medio";
            }
        #endif
    }

    void showOnScreen() {
        String lastLine;
        uint32_t lastMenuTime = millis() - menuTime; // Tiempo transcurrido desde que se abrió el menú
        // Si el menú actual NO es 0, 400, 410, 300 ni está dentro de 500–5100
        // y han pasado más de 30 segundos sin interacción,
        // entonces se reinicia el menú y se limpian los mensajes en edición.
        if (!(menuDisplay==0) && !(menuDisplay==400) && !(menuDisplay==410) && !(menuDisplay==300) && !(menuDisplay>=500 && menuDisplay<=5100) && lastMenuTime > 30*1000) {
            menuDisplay     = 0;   // Regresar al menú principal
            messageCallsign = "";  // Limpiar el CALLSIGN usado para escribir mensajes
            messageText     = "";  // Limpiar el texto del mensaje
        }
        // Definir la última línea del display dependiendo si hay teclado detectado
        if (keyDetected) {  
            lastLine = "<Volver Arriba/Abajo Seleccionar>";  // Texto cuando se usa keypad
        } else {
            lastLine = "1P=Down 2P=Back LP=Go";            // Texto cuando se usan botones físicos
        }

        #if defined(TTGO_T_DECK_PLUS) || defined(TTGO_T_DECK_GPS)
            topHeader1 = currentBeacon->callsign;  // Mostrar el CALLSIGN del beacon activo

            const auto time_now = now();           // Obtener hora actual del RTC o sistema
            topHeader1_1 = Utils::createDateString(time_now); // Fecha formateada (ej: 2025-11-14)
            topHeader1_2 = Utils::createTimeString(time_now); // Hora formateada (ej: 18:05:00)
            topHeader1_3 = "";                     // Campo actualmente no usado

            // Construir la segunda línea con latitud y longitud a 4 decimales
            topHeader2  = String(gps.location.lat(), 4);
            topHeader2 += " ";
            topHeader2 += String(gps.location.lng(), 4);

            // Rellenar con espacios hasta alcanzar largo mínimo para alinear el texto
            for (int i = topHeader2.length(); i < 19; i++) {
                topHeader2 += " ";
            }

            // Si satélites <= 9, agregar un espacio extra para mantener alineación
            if (gps.satellites.value() <= 9)
                topHeader2 += " ";

            // Agregar número de satélites detectados
            topHeader2 += "SAT:";
            topHeader2 += String(gps.satellites.value());

            // Añadir símbolo de calidad de señal basado en HDOP:
            if (gps.hdop.hdop() > 5) {             // HDOP alta → mala precisión
                topHeader2 += "X";                 // Indicador de baja calidad
            } else if (gps.hdop.hdop() > 2 && gps.hdop.hdop() < 5) {
                topHeader2 += "-";                 // Calidad media
            } else if (gps.hdop.hdop() <= 2) {
                topHeader2 += "+";                 // Excelente calidad de GPS
            }
        #endif

        switch (menuDisplay) { // Menu Gráfico
            case 1:     // 1. Messages  →  Menú principal de Mensajes
                // Muestra el menú con la opción 1 (Mensajes) seleccionada.
                // Las flechas > indican la opción activa, y las demás se muestran como navegación previa/siguiente.
                displayShow("<< MENU >>",
                            "  6.Extras",        // Opción anterior
                            "> 1.Mensajes",      // Opción seleccionada
                            "  2.Configuracion", // Siguiente opción
                            "  3.Reportes",      // Segunda siguiente
                            lastLine);           // Línea inferior dinámica (botones/ayuda)
                break;

            case 2:     // 2. Configuration  →  Menú de Configuración
                // Muestra el menú con Configuración como opción activa.
                displayShow("<< MENU >>",
                            "  1.Mensajes",       // Opción anterior
                            "> 2.Configuracion",  // Opción actual/seleccionada
                            "  3.Reportes",
                            "  4.Estaciones",
                            lastLine);
                break;

            case 3:     // 3. Reports  →  Menú de Reportes
                // Muestra el menú con Reportes como activo.
                displayShow("<< MENU >>",
                            "  2.Configuracion",
                            "> 3.Reportes",
                            "  4.Estaciones",
                            "  5.Winlink/Mail",
                            lastLine);
                break;

            case 4:     // 4. Estaciones  →  Menú de Estaciones disponibles
                // Muestra el menú con Estaciones como seleccionado.
                displayShow("<< MENU >>",
                            "  3.Reportes",
                            "> 4.Estaciones",
                            "  5.Winlink/Mail",
                            "  6.Extras",
                            lastLine);
                break;

            case 5:     // 5. Winlink → Cliente Winlink Email
                // Menú Winlink/Mail como activo.
                displayShow("<< MENU >>",
                            "  4.Estaciones",
                            "> 5.Winlink/Mail",
                            "  6.Extras",
                            "  1.Mensajes",
                            lastLine);
                break;

            case 6:     // 6. Extras → Herramientas adicionales
                // Muestra el menú con Extras seleccionado.
                displayShow("<< MENU >>",
                            "  5.Winlink/Mail",
                            "> 6.Extras",
                            "  1.Mensajes",
                            "  2.Configuracion",
                            lastLine);
                break;

//////////
            case 10:    // 1.Mensajes ---> Leer Mensajes (Messages Read)
                // Muestra el submenú de Mensajes con la opción "Leer" seleccionada.
                // Se indica también la cantidad de mensajes APRS disponibles.
                displayShow(" MENSAJES>",                           // Título del submenú
                            "> Leer (" + String(MSG_Utils::getNumAPRSMessages()) + ")", // Opción activa
                            "  Escribir",                           // Siguiente opción
                            "  Borrar",                             // Opción adicional
                            "  APRSThursday",                       // Función especial
                            lastLine);                              // Línea inferior con ayuda de controles
                break;


            case 100:   // 1.Mensajes ---> Leer Mensajes ---> Mostrar mensaje APRS recibido/guardado
                {
                    // Extrae el remitente: todo lo que está antes de la coma
                    String msgSender = loadedAPRSMessages[messagesIterator]
                                            .substring(0, loadedAPRSMessages[messagesIterator].indexOf(","));

                    // Extrae el texto del mensaje: todo lo que está después de la coma
                    String msgText = loadedAPRSMessages[messagesIterator]
                                            .substring(loadedAPRSMessages[messagesIterator].indexOf(",") + 1);

                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            // Versión para pantalla TFT del Heltec Tracker
                            displayShow(" MSJ APRS>",               // Título: Mensaje APRS
                                        "De --> " + msgSender,      // Remitente
                                        msgText,                    // Texto del mensaje
                                        "                 Sig=Abajo", // Indicador: "Siguiente" con tecla abajo
                                        "",
                                        "");
                        #else   // T-Deck
                            // Versión para pantalla TFT del T-Deck
                            displayShow("MSJ APRS>", 
                                        "De --> " + msgSender,
                                        msgText,
                                        "             Sig=Abajo",
                                        "",
                                        "");
                        #endif
                    #else
                        // Versión para pantallas monocromáticas (OLED típicas)
                        displayShow(" MSJ APRS>",
                                    "De --> " + msgSender,
                                    msgText,
                                    "",
                                    "",
                                    "           Sig=Abajo");       // Indicador en la última línea
                    #endif
                }
                break;
            case 11:    // 1.Menú para escribir mensajes
                displayShow(" MESSAGES>", 
                            "  Leer (" + String(MSG_Utils::getNumAPRSMessages()) + ")", 
                            "> Escribir", 
                            "  Borrar", 
                            "  APRSThursday", 
                            lastLine);
                break;

            case 110:   // 1.Pantalla para ingresar el callsign del mensaje
                if (keyDetected || keyboardConnected) {   // Si se detecta teclado físico o está conectado

                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            // Pantalla para escribir el mensaje en Heltec Wireless Tracker
                            displayShow("ESCRIBIR MSJ>", 
                                        "", 
                                        "CALLSIGN = " + String(messageCallsign), 
                                        "", 
                                        "", 
                                        "<Volver           Enter>");
                        #else   // T-DECK
                            // Pantalla para escribir en T-Deck (alineación diferente)
                            displayShow("ESCRIBIR MSJ>", 
                                        "", 
                                        "CALLSIGN = " + String(messageCallsign), 
                                        "", 
                                        "", 
                                        "<Volver        Enter>");
                        #endif
                    #else
                        // Pantalla en dispositivo sin TFT
                        displayShow("ESCRIBIR MSJ>", 
                                    "", 
                                    "CALLSIGN = " + String(messageCallsign), 
                                    "", 
                                    "", 
                                    "<Volver        Enter>");
                    #endif

                } else {
                    // Caso sin teclado: no se puede escribir mensaje
                    displayShow("ESCRIBIR MSJ>", 
                                "", 
                                "Sin Teclado", 
                                "No se puede escribir", 
                                "", 
                                "1P = Volver");           
                }
                break;
            case 111:
                // Caso: edición/composición del mensaje (antes de enviar).
                // Se permite hasta 67 caracteres; si supera, se mostrará aviso de mensaje demasiado largo.
                if (messageText.length() <= 67) {

                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            // Versión para Heltec con pantalla TFT
                            // Si la longitud es menor que 10, se antepone un '0' en el contador para mantener alineación "(0N)"
                            if (messageText.length() < 10) {
                                displayShow("ESCRIBIR MSJ>",                                   // Título en pantalla
                                            "CALLSIGN -> " + messageCallsign,                // Línea: callsign/CALLSIGN
                                            "MSJ -> " + messageText,                           // Línea: texto del mensaje
                                            "<Volver      (0" + String(messageText.length()) + ")     Enter>", // Hint con contador formateado
                                            "",
                                            "");
                            } else {
                                // Longitud >= 10, mostrar contador sin cero inicial
                                displayShow("ESCRIBIR MSJ>",
                                            "CALLSIGN -> " + messageCallsign,
                                            "MSJ -> " + messageText,
                                            "<Volver      (" + String(messageText.length()) + ")     Enter>",
                                            "",
                                            "");
                            }
                        #else   // T-Deck
                            // Versión para T-Deck (alineación distinta por ancho del display)
                            if (messageText.length() < 10) {
                                displayShow("ESCRIBIR MSJ>",
                                            "CALLSIGN -> " + messageCallsign,
                                            "MSJ -> " + messageText,
                                            "<Volver    (0" + String(messageText.length()) + ")   Enter>",
                                            "",
                                            "");
                            } else {
                                displayShow("ESCRIBIR MSJ>",
                                            "CALLSIGN -> " + messageCallsign,
                                            "MSJ -> " + messageText,
                                            "<Volver    (" + String(messageText.length()) + ")   Enter>",
                                            "",
                                            "");
                            }
                        #endif
                    #else
                        // Versión para dispositivos sin TFT (p. ej. OLED monocromo)
                        if (messageText.length() < 10) {
                            displayShow("ESCRIBIR MSJ>",
                                        "CALLSIGN -> " + messageCallsign,
                                        "MSJ -> " + messageText,
                                        "",
                                        "",
                                        "<Volver   (0" + String(messageText.length()) + ")   Enter>"); // Hint en la última línea
                        } else {
                            displayShow("ESCRIBIR MSJ>",
                                        "CALLSIGN -> " + messageCallsign,
                                        "MSJ -> " + messageText,
                                        "",
                                        "",
                                        "<Volver   (" + String(messageText.length()) + ")   Enter>");
                        }
                    #endif

                } else {
                    // Mensaje demasiado largo (> 67 caracteres) -> mostrar advertencia y el texto que excede
                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            // Heltec TFT: aviso con alineación específica
                            displayShow("ESCRIBIR MSJ>",
                                        "  --- MSJ MUY LARGO! ---",   // Aviso breve traducido
                                        " -> " + messageText,         // Mostrar el texto (posible truncamiento por display)
                                        "<Volver   (" + String(messageText.length()) + ")", // Contador grande
                                        "",
                                        "");
                        #else   // T-Deck
                            // T-Deck TFT
                            displayShow("ESCRIBIR MSJ>",
                                        "  --- MSJ MUY LARGO! ---",
                                        " -> " + messageText,
                                        "<Volver   (" + String(messageText.length()) + ")",
                                        "",
                                        "");
                        #endif
                    #else
                        // Sin TFT
                        displayShow("ESCRIBIR MSJ>",
                                    "--- MSJ MUY LARGO! ---",
                                    " -> " + messageText,
                                    "",
                                    "",
                                    "<Volver   (" + String(messageText.length()) + ")"); // Hint con longitud
                    #endif
                }
                break;
            case 12:    // Submenú: opción "Borrar" seleccionada, mostrando también cantidad de mensajes APRS
            displayShow(" MENSAJES>",
                        "  Leer (" + String(MSG_Utils::getNumAPRSMessages()) + ")",
                        "  Escribir",
                        "> Borrar",
                        "  APRSThursday",
                        lastLine);
            break;
            case 120:   // Confirmación para borrar todos los mensajes APRS
                displayShow("BORRAR MSJ",                // Título del diálogo de borrado
                            "",
                            "  ¿BORRAR MSJ APRS?",      // Pregunta al usuario
                            "",
                            "",
                            "Confirmar = LP o '>'");    // Indicación de cómo confirmar (LP = Long Press)
                break;

            case 13:    // Submenú APRS Thursday (opción seleccionada)
                displayShow(" MENSAJES>",
                            "  Leer (" + String(MSG_Utils::getNumAPRSMessages()) + ")",
                            "  Escribir",
                            "  Borrar",
                            "> APRSThursday",
                            lastLine);
                break;

            case 130:   // Menú con acciones relacionadas (Check In, Join, Unsubscribe, KeepSubscribed+12h)
                displayShow(" APRS Jue.",
                            "> Registrarse",         // Check In
                            "  Unirse",              // Join
                            "  Darse de baja",       // Unsubscribe
                            "  Mantener+12h",        // Mantener suscripción por +12 horas
                            lastLine);
                break;
            case 1300:
                // Escritura del mensaje para APRS Thursday.
                // Se valida que el mensaje no supere los 67 caracteres permitidos.
                if (messageText.length() <= 67) {

                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            // Versión para dispositivos HELTEC con pantalla TFT
                            if (messageText.length() < 10) {
                                // Mostrar contador con cero inicial para mantener el formato
                                displayShow("ESCRIBIR MSJ>",                   // Título
                                            "    - APRSThursday -",              // Encabezado de la función 
                                            "MSJ -> " + messageText,           // Contenido del mensaje
                                            "<Volver      (0" + String(messageText.length()) + ")     Enter>", // Indicación
                                            "",
                                            "");
                            } else {
                                // Contador sin cero inicial (>=10 caracteres)
                                displayShow("ESCRIBIR MSJ>",
                                            "    - APRSThursday -",
                                            "MSJ -> " + messageText,
                                            "<Volver      (" + String(messageText.length()) + ")     Enter>",
                                            "",
                                            "");
                            }

                        #else   // T-Deck
                            // Versión para T-Deck TFT (alineación distinta)
                            if (messageText.length() < 10) {
                                displayShow("ESCRIBIR MSJ>",
                                            "    - APRSThursday -",
                                            "MSJ -> " + messageText,
                                            "<Volver    (0" + String(messageText.length()) + ")   Enter>",
                                            "",
                                            "");
                            } else {
                                displayShow("ESCRIBIR MSJ>",
                                            "    - APRSThursday -",
                                            "MSJ -> " + messageText,
                                            "<Volver    (" + String(messageText.length()) + ")   Enter>",
                                            "",
                                            "");
                            }
                        #endif

                    #else
                        // Versión para pantallas sin TFT (OLED)
                        if (messageText.length() < 10) {
                            displayShow("ESCRIBIR MSJ>",
                                        "  - APRSThursday -",
                                        "MSJ -> " + messageText,
                                        "",
                                        "",
                                        "<Volver   (0" + String(messageText.length()) + ")   Enter>");
                        } else {
                            displayShow("ESCRIBIR MSJ>",
                                        "  - APRSThursday -",
                                        "MSJ -> " + messageText,
                                        "",
                                        "",
                                        "<Volver   (" + String(messageText.length()) + ")   Enter>");
                        }
                    #endif                    

                } else {
                    // El mensaje excede los 67 caracteres → mostrar advertencia de longitud
                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            displayShow("ESCRIBIR MSJ>",                       // Título
                                        "  --- MSJ MUY LARGO! ---",            // Advertencia traducida
                                        " -> " + messageText,                  // Mostrar el mensaje completo
                                        "<Volver   (" + String(messageText.length()) + ")", // Contador
                                        "",
                                        "");
                        #else   // T-Deck
                            displayShow("ESCRIBIR MSJ>",
                                        "  --- MSJ MUY LARGO! ---",
                                        " -> " + messageText,
                                        "<Volver   (" + String(messageText.length()) + ")",
                                        "",
                                        "");
                        #endif
                    #else
                        // Pantallas sin TFT
                        displayShow("ESCRIBIR MSJ>",
                                    "--- MSJ MUY LARGO! ---",
                                    " -> " + messageText,
                                    "",
                                    "",
                                    "<Volver   (" + String(messageText.length()) + ")");
                    #endif                    
                }
                break;

            case 131:   // Menú de APRSThursday con la opción "Join" seleccionada
                displayShow(" APRS Thu.",          // Encabezado del menú
                            "  Check In",          // Registrarse
                            "> Unirse",            // Join (seleccionada)
                            "  Cancelar",          // Unsubscribe
                            "  Mantener+12h",      // KeepSubscribed+12h
                            lastLine);
                break;

            case 1310:
                // Pantalla para escribir mensaje de APRSJueves (Join)
                // Se valida que el mensaje tenga máximo 67 caracteres
                if (messageText.length() <= 67) {

                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            // HELTEC con pantalla TFT
                            if (messageText.length() < 10) {
                                displayShow("ESCRIBIR MSJ>",
                                            "    - APRSThursday -",
                                            "MSJ -> " + messageText,
                                            "<Volver      (0" + String(messageText.length()) + ")     Enter>",
                                            "",
                                            "");
                            } else {
                                displayShow("ESCRIBIR MSJ>",
                                            "    - APRSThursday -",
                                            "MSJ -> " + messageText,
                                            "<Volver      (" + String(messageText.length()) + ")     Enter>",
                                            "",
                                            "");
                            }

                        #else   // T-Deck TFT
                            if (messageText.length() < 10) {
                                displayShow("ESCRIBIR MSJ>",
                                            "    - APRSThursday -",
                                            "MSJ -> " + messageText,
                                            "<Volver    (0" + String(messageText.length()) + ")   Enter>",
                                            "",
                                            "");
                            } else {
                                displayShow("ESCRIBIR MSJ>",
                                            "    - APRSThursday -",
                                            "MSJ -> " + messageText,
                                            "<Volver    (" + String(messageText.length()) + ")   Enter>",
                                            "",
                                            "");
                            }
                        #endif

                    #else
                        // Pantallas sin TFT (OLED)
                        if (messageText.length() < 10) {
                            displayShow("ESCRIBIR MSJ>",
                                        "  - APRSThursday -",
                                        "MSJ -> " + messageText,
                                        "",
                                        "",
                                        "<Volver   (0" + String(messageText.length()) + ")   Enter>");
                        } else {
                            displayShow("ESCRIBIR MSJ>",
                                        "  - APRSThursday -",
                                        "MSJ -> " + messageText,
                                        "",
                                        "",
                                        "<Volver   (" + String(messageText.length()) + ")   Enter>");
                        }
                    #endif

                } else {
                    // Mensaje demasiado largo (más de 67 caracteres)
                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            displayShow("ESCRIBIR MSJ>",
                                        "  --- MSJ MUY LARGO! ---",
                                        " -> " + messageText,
                                        "<Volver   (" + String(messageText.length()) + ")",
                                        "",
                                        "");
                        #else   // T-Deck TFT
                            displayShow("ESCRIBIR MSJ>",
                                        "  --- MSJ MUY LARGO! ---",
                                        " -> " + messageText,
                                        "<Volver   (" + String(messageText.length()) + ")",
                                        "",
                                        "");
                        #endif
                    #else
                        // OLED
                        displayShow("ESCRIBIR MSJ>",
                                    "--- MSJ MUY LARGO! ---",
                                    " -> " + messageText,
                                    "",
                                    "",
                                    "<Volver   (" + String(messageText.length()) + ")");
                    #endif
                }
                break;

            case 132:   // 1.Messages ---> APRSJueves ---> Cancelar suscripción
                displayShow(" APRS Thu.",
                            "  Check In",
                            "  Unirse",
                            "> Cancelar",          // Unsubscribe seleccionado
                            "  Mantener+12h",
                            lastLine);
                break;

            case 133:   // 1.Messages ---> APRSJueves ---> Mantener suscripción por 12 h
                displayShow(" APRS Thu.",
                            "  Check In",
                            "  Unirse",
                            "  Cancelar",
                            "> Mantener+12h",      // KeepSubscribed+12h seleccionado
                            lastLine);
                break;

//////////            
            case 20:    // 2.Configuration ---> Callsign
                // Submenú de Configuración con la opción "Cambiar Callsign" seleccionada.
                displayShow(" CONFIG>",                     // Encabezado del menú de configuración
                            "  Apagar",                     // Opción anterior: Power Off
                            "> Cambiar Callsign ",        // Opción seleccionada: Change Callsign
                            "  Cambiar Frecuencia",         // Siguiente opción: Change Frequency
                            "  Pantalla",                   // Segunda siguiente: Display
                            lastLine);                      // Línea inferior dinámica (hints)
                break;

            case 21:    // 2.Configuration ---> Change Freq
                // Submenú con "Change Frequency" seleccionado; muestra también estado del Bluetooth
                displayShow(" CONFIG>",
                            "  Cambiar Callsign ", 
                            "> Cambiar Frecuencia",
                            "  Pantalla",
                            "  " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")", // p.ej. "BLE (ON)"
                            lastLine);
                break;

            case 22:    // 2.Configuration ---> Display
                // Submenú de Pantalla; la penúltima línea muestra tipo/estado de Bluetooth
                displayShow(" CONFIG>",
                            "  Cambiar Frecuencia",
                            "> Pantalla",
                            "  " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")",
                            "  Estado",   // Status
                            lastLine);
                break;

            case 23:    // 2.Configuration ---> Bluetooth
                // Submenú del Bluetooth: muestra tipo/estado como opción seleccionable
                displayShow(" CONFIG>",
                            "  Pantalla",
                            "> " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")", // opción activa: p.ej. "BT Classic (OFF)"
                            "  Estado",
                            "  Notificaciones", // Notifications
                            lastLine);
                break;

            case 24:    // 2.Configuration ---> Status
                // Submenú Estado (Status) con opciones relacionadas
                displayShow(" CONFIG>",
                            "  " + checkBTType() + " (" + checkProcessActive(bluetoothActive) + ")", // Mostrar tipo/estado BT
                            "> Estado",
                            "  Notificaciones",
                            "  Reiniciar", // Reboot
                            lastLine);
                break;

            case 25:    // 2.Configuration ---> Notifications
                // Submenú Notificaciones seleccionado
                displayShow(" CONFIG>",
                            "  Estado",
                            "> Notificaciones",
                            "  Reiniciar",
                            "  Apagar",  // Power Off
                            lastLine);
                break;

            case 26:    // 2.Configuration ---> Reboot
                // Submenú Reiniciar (Reboot) con confirmaciones y navegación
                displayShow(" CONFIG>",
                            "  Notificaciones",
                            "> Reiniciar",
                            "  Apagar",
                            "  Cambiar Callsign",
                            lastLine);
                break;

            case 27:    // 2.Configuration ---> Power Off
                // Submenú Apagar (Power Off)
                displayShow(" CONFIG>",
                            "  Reiniciar",
                            "> Apagar",
                            "  Cambiar Callsign",
                            "  Cambiar Frecuencia",
                            lastLine);
                break;


            case 200:   // 2.Configuration ---> Change Callsign
                // Menú de confirmación para cambiar el indicativo
                displayShow(" INDICATIVO>",      // Encabezado: Callsign
                            "",                  
                            "  ¿Confirmar cambio?",   // Confirm Change?
                            "", 
                            "",
                            "<Atrás        Seleccionar>");  // Back / Select
                break;

            case 210:   // 2.Configuration ---> Change Frequency
                // Determina el mensaje de advertencia según la región LoRa actual
                switch (loraIndex) {
                    case 0: freqChangeWarning = "      Eu --> PL"; break;
                    case 1: freqChangeWarning = "      PL --> UK"; break;
                    case 2: freqChangeWarning = "      UK --> Eu"; break;
                }

                // Pantalla de confirmación para cambiar la frecuencia LoRa
                displayShow("FREC LORA>",          // LORA FREQ>
                            "",
                            "  ¿Confirmar cambio?",  // Confirm Change?
                            freqChangeWarning,       // Mensaje dinámico de región
                            "",
                            "<Atrás        Seleccionar>");
                break;

            case 220:   // 2.Configuration ---> Display ---> ECO Mode
                // Menú de Display con ECO Mode seleccionado
                displayShow(" PANTALLA>",  
                            "",
                            "> Modo ECO   (" + checkProcessActive(displayEcoMode) + ")",  // ON / OFF
                            "  Brillo     (" + screenBrightnessAsString(screenBrightness) + ")", // Low/Mid/Max
                            "",
                            lastLine);
                break;

            case 221:   // 2.Configuration ---> Display ---> Brightness
                // Menú Display con Brightness seleccionado
                displayShow(" PANTALLA>",
                            "",
                            "  Modo ECO   (" + checkProcessActive(displayEcoMode) + ")",
                            "> Brillo     (" + screenBrightnessAsString(screenBrightness) + ")",
                            "",
                            lastLine);
                break;

            case 2210:   // Display ---> Brightness: MIN
                displayShow(" BRILLO",      // BRIGHTNESS
                            "",
                            "> Bajo",       // Low
                            "  Medio",      // Mid
                            "  Máximo",     // Max
                            lastLine);
                break;

            case 2211:   // Display ---> Brightness: MID
                displayShow(" BRILLO",
                            "",
                            "  Bajo",
                            "> Medio",
                            "  Máximo",
                            lastLine);
                break;

            case 2212:   // Display ---> Brightness: MAX
                displayShow(" BRILLO",
                            "",
                            "  Bajo",
                            "  Medio",
                            "> Máximo",
                            lastLine);
                break;

            case 230:
                // Alterna el estado del Bluetooth y muestra notificación
                if (bluetoothActive) {
                    bluetoothActive = false;
                    displayShow("BLUETOOTH>", "", " Bluetooth --> OFF", 1000);  // Bluetooth apagado
                } else {
                    bluetoothActive = true;
                    displayShow("BLUETOOTH>", "", " Bluetooth --> ON", 1000);   // Bluetooth encendido
                }
                menuDisplay = 23;  // Regresa al menú anterior
                break;

            case 240:    // 2.Configuration ---> Status (primer ítem seleccionado)
                displayShow(" ESTADO>",   // STATUS
                            "",
                            "> Escribir",  // Write
                            "  Seleccionar", // Select
                            "",
                            lastLine);
                break;

            case 241:    // 2.Configuration ---> Status (segundo ítem seleccionado)
                displayShow(" ESTADO>",
                            "",
                            "  Escribir",
                            "> Seleccionar",
                            "",
                            lastLine);
                break;

            case 250:    // 2.Configuration ---> Notifications
                // Pantalla para desactivar sonidos o LED
                displayShow(" NOTIFIC>", 
                            "> Apagar Sonido/LED",
                            "",
                            "",
                            "",
                            lastLine);
                break;

            case 260:   // 2.Configuration ---> Reboot
                if (keyDetected) {
                    // Confirmación de reinicio cuando hay teclado disponible
                    displayShow(" ¿REINICIAR?",   // REBOOT?
                                "",
                                "Confirmar reinicio...", // Confirm Reboot...
                                "",
                                "",
                                "<Atrás   Enter=Confirmar");  // Back / Confirm
                } else {
                    // No hay teclado: instrucción manual
                    displayShow(" ¿REINICIAR?",
                                "No se detecta teclado",
                                " Use el botón RST para",
                                "reiniciar el tracker",
                                "",
                                lastLine);
                }
                break;

            case 270:   // 2.Configuration ---> Power Off
                // Confirmación para apagar el dispositivo
                displayShow("¿APAGAR?",           // POWER OFF?
                            "",
                            "Confirmar apagado...",  // Confirm Power Off...
                            "",
                            "",
                            "<Atrás  Enter/=Confirmar");
                break;

//////////
            case 30:     // 3. Reports : Wx Report (Reporte meteorológico)
                // Menú de Reportes — opción 1 seleccionada: Reporte del clima (Weather Report)
                displayShow(" REPORTES >",               // REPORTS >
                            "> 1.Reporte Clima",         // > 1.Wx Report
                            "  2.Hospital Cercano",      // 2.Hospital QTH
                            "  3.Estación de Policía",   // 3.Police QTH
                            "  4.Estación de Bomberos",  // 4.Fire Station QTH
                            lastLine);
                break;

            case 31:     // 3. Reports : Nearest Hospital
                // Opción 2 seleccionada: Hospital más cercano
                displayShow(" REPORTES >",
                            "  1.Reporte Clima",
                            "> 2.Hospital Cercano",
                            "  3.Estación de Policía",
                            "  4.Estación de Bomberos",
                            lastLine);
                break;

            case 32:     // 3. Reports : Nearest Police Station
                // Opción 3 seleccionada: Policía más cercana
                displayShow(" REPORTES >",
                            "  1.Reporte Clima",
                            "  2.Hospital Cercano",
                            "> 3.Estación de Policía",
                            "  4.Estación de Bomberos",
                            lastLine);
                break;

            case 33:     // 3. Reports : Nearest Fire Station
                // Opción 4 seleccionada: Bomberos más cercanos
                displayShow(" REPORTES >",
                            "  1.Reporte Clima",
                            "  2.Hospital Cercano",
                            "  3.Estación de Policía",
                            "> 4.Estación de Bomberos",
                            lastLine);
                break;

            case 300:
                // Esperando datos de reporte (operación en progreso)
                break;

//////////
            case 40:    // 4. Stations ---> Packet Decoder
                // Menú principal de Estaciones: opción "Decodificador de paquetes" seleccionada
                displayShow(" ESTACIONES>",    // Encabezado: STATIONS
                            "", 
                            "> Decodificador Paq.", // Packet Decoder
                            "  Estaciones Cercanas", // Near By Stations
                            "", 
                            "<Atrás");              // Hint para volver
                break;

            case 41:    // 4. Stations ---> Near By Stations
                // Menú principal de Estaciones: opción "Estaciones Cercanas" seleccionada
                displayShow(" ESTACIONES>",
                            "",
                            "  Decodificador Paq.",
                            "> Estaciones Cercanas",
                            "",
                            "<Atrás");
                break;

            case 400:   // 4. Stations ---> Packet Decoder (muestra detalles del último paquete recibido)
                // Solo mostrar si el último paquete NO es de este propio beacon
                if (lastReceivedPacket.sender != currentBeacon->callsign) {

                    // Preparar primera línea: remitente (callsign) y símbolo
                    String firstLineDecoder = lastReceivedPacket.sender;
                    for (int i = firstLineDecoder.length(); i < 9; i++) {
                        firstLineDecoder += ' '; // Rellenar con espacios hasta longitud mínima para alinear
                    }
                    firstLineDecoder += lastReceivedPacket.symbol;

                    // Paquetes de tipo GPS (0) o Mic-E GPS (4): mostrar posición, alt/speed/course, distancia y rumbo
                    if (lastReceivedPacket.type == 0 || lastReceivedPacket.type == 4) {      // gps and Mic-E gps

                        // Formatear altitud/velocidad/rumbo en un buffer (ej: "A=0123m  45km/h 180")
                        char bufferCourseSpeedAltitude[24];
                        sprintf(bufferCourseSpeedAltitude, "A=%04dm %3dkm/h %3d",
                                lastReceivedPacket.altitude,
                                lastReceivedPacket.speed,
                                lastReceivedPacket.course);
                        String courseSpeedAltitude = String(bufferCourseSpeedAltitude);

                        // Calcular distancia (km) y rumbo hacia la posición del paquete
                        double distanceKm = TinyGPSPlus::distanceBetween(
                                                gps.location.lat(), gps.location.lng(),
                                                lastReceivedPacket.latitude, lastReceivedPacket.longitude) / 1000.0;
                        double courseTo   = TinyGPSPlus::courseTo(
                                                gps.location.lat(), gps.location.lng(),
                                                lastReceivedPacket.latitude, lastReceivedPacket.longitude);

                        // Preparar la cadena PATH/RUTA; si es larga usar prefijo corto "R:"
                        String pathDec = (lastReceivedPacket.path.length() > 14) ? "R:" : "RUTA:  ";
                        pathDec += lastReceivedPacket.path;

                        // Mostrar la información en las 6 líneas del display
                        // Línea 1: callsign + símbolo
                        // Línea 2: GPS lat lon (3 decimales)
                        // Línea 3: Alt/Vel/Rumbo
                        // Línea 4: Distancia en km y rumbo (curso) hacia el objetivo
                        // Línea 5: PATH / RUTA
                        // Línea 6: RSSI y SNR (indicadores de señal)
                        displayShow(firstLineDecoder,
                                    "GPS " + String(lastReceivedPacket.latitude, 3) + " " + String(lastReceivedPacket.longitude, 3),
                                    courseSpeedAltitude,
                                    "D:" + String(distanceKm) + "km    " + String(courseTo, 0),
                                    pathDec,
                                    "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));

                    } else if (lastReceivedPacket.type == 1) {    // message
                        // Paquete tipo mensaje: mostrar remitente/destinatario y cuerpo
                        displayShow(firstLineDecoder,
                                    "DESTINATARIO: " + lastReceivedPacket.addressee, // traducido
                                    "MSJ:  " + lastReceivedPacket.payload,           // MSG -> MSJ
                                    "",
                                    "",
                                    "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));

                    } else if (lastReceivedPacket.type == 2) {    // status
                        // Paquete tipo estado
                        displayShow(firstLineDecoder,
                                    "-----ESTADO------",       // STATUS -> ESTADO
                                    lastReceivedPacket.payload,
                                    "",
                                    "",
                                    "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));

                    } else if (lastReceivedPacket.type == 3) {    // telemetry
                        // Paquete de telemetría
                        displayShow(firstLineDecoder,
                                    "----TELEMETRÍA----",     // TELEMETRY -> TELEMETRÍA
                                    "",
                                    "",
                                    "",
                                    "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));

                    } else if (lastReceivedPacket.type == 5) {    // object
                        // Paquete tipo objeto (APRS object)
                        displayShow(firstLineDecoder,
                                    "-----OBJETO-------",     // OBJECT -> OBJETO
                                    "",
                                    "",
                                    "",
                                    "< RSSI:" + String(lastReceivedPacket.rssi) + " SNR:" + String(lastReceivedPacket.snr));
                    }
                }
                break;

            case 410:    // 4. Stations ---> Near By Stations (lista de estaciones cercanas)
                // Mostrar hasta 4 estaciones cercanas obtenidas desde STATION_Utils
                displayShow(" CERCANAS>", 
                            STATION_Utils::getNearStation(0),
                            STATION_Utils::getNearStation(1),
                            STATION_Utils::getNearStation(2),
                            STATION_Utils::getNearStation(3),
                            "<Atrás");
                break;
//////////
            case 50:    // 5.Winlink MENU
                if (winlinkStatus == 5) {
                    menuDisplay = 5000;
                } else {
                    displayShow(" WINLINK>", "> Login" , "  Read SavedMails(" + String(MSG_Utils::getNumWLNKMails()) + ")", "  Delete SavedMails", "  Wnlk Comment (" + checkProcessActive(winlinkCommentState) + ")" , lastLine);
                }
                break;
            case 51:    // 5.Winlink
                displayShow(" WINLINK>", "  Login" , "> Read SavedMails(" + String(MSG_Utils::getNumWLNKMails()) + ")", "  Delete SavedMails", "  Wnlk Comment (" + checkProcessActive(winlinkCommentState) + ")" , lastLine);
                break;
            case 52:    // 5.Winlink
                displayShow(" WINLINK>", "  Login" , "  Read SavedMails(" + String(MSG_Utils::getNumWLNKMails()) + ")", "> Delete SavedMails", "  Wnlk Comment (" + checkProcessActive(winlinkCommentState) + ")" , lastLine);
                break;
            case 53:    // 5.Winlink
                displayShow(" WINLINK>", "  Login" , "  Read SavedMails(" + String(MSG_Utils::getNumWLNKMails()) + ")", "  Delete SavedMails", "> Wnlk Comment (" + checkProcessActive(winlinkCommentState) + ")" , lastLine);
                break;

            case 500:    // 5.Winlink ---> Login
                displayShow(" WINLINK>", "" , "Login Initiation ...", "Challenge -> waiting", "" , "");
                break;
            case 501:    // 5.Winlink ---> Login
                displayShow(" WINLINK>", "" , "Login Initiation ...", "Challenge -> sent", "" , "");
                break;
            case 502:    // 5.Winlink ---> Login
                displayShow(" WINLINK>", "" , "Login Initiation ...", "Challenge -> ack ...", "" , "");
                break;

            case 5000:   // WINLINK: List Pend. Mail //
                displayShow("WLNK MENU>", "  Write Mail" , "> List Pend. Mails", "  Downloaded Mails", "  Read Mail    (R#)", lastLine);
                break;

            case 5010:    // WINLINK: Downloaded Mails //
                displayShow("WLNK MENU>", "  List Pend. Mails", "> Downloaded Mails", "  Read Mail    (R#)", "  Reply Mail   (Y#)", lastLine);
                break;
            case 50100:    // WINLINK: Downloaded Mails //
                displayShow(" WINLINK>", "" , "> Read SavedMails(" + String(MSG_Utils::getNumWLNKMails()) + ")", "  Delete SavedMails", "" , lastLine);
                break;
            case 50101:    // WINLINK: Downloaded Mails //
                {
                    String mailText = loadedWLNKMails[messagesIterator];

                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            displayShow("WLNK MAIL>", mailText, "                 Next=Down", "", "", "");
                        #else   // T-Deck
                            displayShow("WLNK MAIL>", mailText, "             Next=Down", "", "", "");
                        #endif
                    #else
                        displayShow("WLNK MAIL>", "", mailText, "", "", "           Next=Down");
                    #endif
                    
                }
                break;
            case 50110:    // WINLINK: Downloaded Mails //
                displayShow(" WINLINK>", "" , "  Read SavedMails(" + String(MSG_Utils::getNumWLNKMails()) + ")", "> Delete SavedMails", "" , lastLine);
                break;
            case 50111:    // WINLINK: Downloaded Mails //
                displayShow("WLNK DEL>", "", "  DELETE ALL MAILS?", "", "", " Confirm = LP or '>'");
                break;

            case 5020:    // WINLINK: Read Mail //
                displayShow("WLNK MENU>", "  Downloaded Mails", "> Read Mail    (R#)", "  Reply Mail   (Y#)", "  Forward Mail (F#)", lastLine);
                break;
            case 5021:
                displayShow("WLNK READ>", "", "    READ MAIL N." + winlinkMailNumber, "", "", "<Back          Enter>");
                break;

            case 5030:    // WINLINK: Reply Mail //
                displayShow("WLNK MENU>", "  Read Mail    (R#)", "> Reply Mail   (Y#)", "  Forward Mail (F#)", "  Delete Mail  (K#)", lastLine);
                break;
            case 5031:
                displayShow("WLNK REPLY", "", "   REPLY MAIL N." + winlinkMailNumber , "", "", "<Back          Enter>");
                break;

            case 5040:    // WINLINK: Foward Mail //
                displayShow("WLNK MENU>", "  Reply Mail   (Y#)", "> Forward Mail (F#)", "  Delete Mail  (K#)", "  Alias Menu", lastLine);
                break;
            case 5041:    // WINLINK: Forward Mail //
                displayShow("WLNK FORW>", "", "  FORWARD MAIL N." + winlinkMailNumber , "", "", "<Back          Enter>");
                break;
            case 5042:    // WINLINK: Forward Mail //
                displayShow("WLNK FORW>", "  FORWARD MAIL N." + winlinkMailNumber , "To = " + winlinkAddressee, "", "", "<Back          Enter>");
                break;

            case 5050:    // WINLINK: Delete Mail //
                displayShow("WLNK MENU>", "  Forward Mail (F#)", "> Delete Mail  (K#)", "  Alias Menu", "  Log Out", lastLine);
                break;
            case 5051:    // WINLINK: Delete Mail //
                displayShow("WLNK DEL>", "", "   DELETE MAIL N."  + winlinkMailNumber, "", "", "<Back          Enter>");
                break;
            
            case 5060:    // WINLINK: Alias Menu //
                displayShow("WLNK MENU>", "  Delete Mail  (K#)", "> Alias Menu", "  Log Out", "  Write Mail", lastLine);
                break;
            case 5061:    // WINLINK: Alias Menu : Create Alias //
                displayShow("WLNK ALIAS", "> Create Alias" , "  Delete Alias ", "  List All Alias", "", lastLine);
                break;
            case 50610:   // WINLINK: Alias Menu : Create Alias //
                displayShow("WLNK ALIAS", "", "Write Alias to Create", "     -> " + winlinkAlias, "", "<Back          Enter>");
                break;
            case 50611:   // WINLINK: Alias Menu : Create Alias //
                displayShow("WLNK ALIAS", "", "      " + winlinkAlias + " =", winlinkAliasComplete, "", "<Back          Enter>");
                break;
            case 5062:    // WINLINK: Alias Menu : Delete Alias //
                displayShow("WLNK ALIAS", "  Create Alias" , "> Delete Alias ", "  List All Alias", "", lastLine);
                break;
            case 50620:   // WINLINK: Alias Menu : Delete Alias //
                displayShow("WLNK ALIAS", "Write Alias to Delete", "", "     -> " + winlinkAlias, "", "<Back          Enter>");
                break;
            case 5063:    // WINLINK: Alias Menu : List Alias//
                displayShow("WLNK ALIAS", "  Create Alias" , "  Delete Alias ", "> List All Alias", "", lastLine);
                break;

            case 5070:    // WINLINK: Log Out MAIL //
                displayShow("WLNK MENU>", "  Alias Menu", "> Log Out", "  Write Mail", "  List Pend. Mails", lastLine);
                break;

            case 5080:    // WINLINK: WRITE MAIL //
                displayShow("WLNK MENU>", "  Log Out", "> Write Mail", "  List Pend. Mails", "  Downloaded Mails", lastLine);
                break;
            case 5081:    // WINLINK: WRITE MAIL: Addressee //
                #ifdef HAS_TFT
                    #if defined(HELTEC_WIRELESS_TRACKER)
                        displayShow("WLNK MAIL>", "   --- Send Mail to ---", "-> " + winlinkAddressee, "<Back               Enter>", "", "");
                    #else   // T-Deck?
                        displayShow("WLNK MAIL>", "   --- Send Mail to ---", "-> " + winlinkAddressee, "<Back           Enter>", "", "");
                    #endif
                #else
                    displayShow("WLNK MAIL>", "--- Send Mail to ---", "-> " + winlinkAddressee, "", "", "<Back          Enter>");
                #endif
                break;
            case 5082:    // WINLINK: WRITE MAIL: Subject //
                #ifdef HAS_TFT
                    #if defined(HELTEC_WIRELESS_TRACKER)
                        displayShow("WLNK MAIL>", "   --- Write Subject ---", "-> " + winlinkSubject, "<Back               Enter>", "", "");
                    #else   // T-Deck?
                        displayShow("WLNK MAIL>", "   --- Write Subject ---", "-> " + winlinkSubject, "<Back           Enter>", "", "");
                    #endif
                #else
                    displayShow("WLNK MAIL>", "--- Write Subject ---", "-> " + winlinkSubject, "", "", "<Back          Enter>");
                #endif                
                break;
            case 5083:    // WINLINK: WRITE MAIL: Body //
                if (winlinkBody.length() <= 67) {
                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            displayShow("WLNK MAIL>", "-- Body (Lenght =" + String(winlinkBody.length()) + ")", "-> " + winlinkBody, "<Clear Body         Enter>", "", "");
                        #else   // T-Deck
                            displayShow("WLNK MAIL>", "-- Body (Lenght =" + String(winlinkBody.length()) + ")", "-> " + winlinkBody, "<Clear Body     Enter>", "", "");
                        #endif
                    #else
                        displayShow("WLNK MAIL>", "-- Body (Lenght =" + String(winlinkBody.length()) + ")", "-> " + winlinkBody, "", "", "<Clear Body    Enter>");
                    #endif
                } else {
                    #ifdef HAS_TFT
                        #if defined(HELTEC_WIRELESS_TRACKER)
                            displayShow("WLNK MAIL>", "-- Body Too Long = " + String(winlinkBody.length()), "-> " + winlinkBody, "<Clear Body", "", "");
                        #else   // T-Deck
                            displayShow("WLNK MAIL>", "-- Body Too Long = " + String(winlinkBody.length()), "-> " + winlinkBody, "<Clear Body", "", "");
                        #endif
                    #else
                        displayShow("WLNK MAIL>", "-- Body Too Long = " + String(winlinkBody.length()), "-> " + winlinkBody, "", "", "<Clear Body");
                    #endif                    
                }
                break;
            case 5084:    // WINLINK: WRITE MAIL: End Mail? //
                displayShow("WLNK MAIL>", "", "> End Mail", "  1 More Line", "", "      Up/Down Select>");
                break;
            case 5085:    // WINLINK: WRITE MAIL: One More Line(Body) //
                displayShow("WLNK MAIL>", "", "  End Mail", "> 1 More Line", "", "      Up/Down Select>");
                break;

                // validar winlinkStatus = 0
                // check si no esta logeado o si

//////////
            case 60:    // 6. Extras ---> Enviar Email con info GPS
                displayShow(" EXTRAS>", 
                            "  Linterna       (" + checkProcessActive(flashlight) + ")",
                            "> Enviar Email(GPS)",
                            "  Digipeater     (" + checkProcessActive(digipeaterActive) + ")",
                            "  S.O.S.         (" + checkProcessActive(sosActive) + ")",
                            lastLine);
                break;

            case 61:    // 6. Extras ---> Digipeater
                displayShow(" EXTRAS>",
                            "  Enviar Email(GPS)",
                            "> Digipeater     (" + checkProcessActive(digipeaterActive) + ")",
                            "  S.O.S.         (" + checkProcessActive(sosActive) + ")",
                            "  Beacon(GPS) + Comentario",
                            lastLine);
                break;

            case 62:    // 6. Extras ---> S.O.S.
                displayShow(" EXTRAS>",
                            "  Digipeater     (" + checkProcessActive(digipeaterActive) + ")",
                            "> S.O.S.         (" + checkProcessActive(sosActive) + ")",
                            "  Beacon(GPS)+Comentario",
                            "  Linterna       (" + checkProcessActive(flashlight) + ")",
                            lastLine);
                break;

            case 63:    // 6. Extras ---> Beacon(GPS) + Comment
                displayShow(" EXTRAS>",
                            "  S.O.S.         (" + checkProcessActive(sosActive) + ")",
                            "> Beacon(GPS)+Comentario",
                            "  Linterna       (" + checkProcessActive(flashlight) + ")",
                            "  Enviar Email(GPS)",
                            lastLine);
                break;

            case 64:    // 6. Extras ---> Flashlight
                displayShow(" EXTRAS>",
                            "  Beacon(GPS)+Comentario",
                            "> Linterna       (" + checkProcessActive(flashlight) + ")",
                            "  Enviar Email(GPS)",
                            "  Digipeater     (" + checkProcessActive(digipeaterActive) + ")",
                            lastLine);
                break;

            case 630:
                if (keyDetected) {
                    if (messageText.length() <= 67) {
                        String lengthStr = (messageText.length() < 10) ? "0" + String(messageText.length()) : String(messageText.length());
                        displayShow(" COMENTARIO>",
                                    "Enviar este Comentario",
                                    "en la próxima Beacon GPS:",
                                    messageText,
                                    "",
                                    "<Atrás   (" + lengthStr + ")   Enter>");
                    } else {
                        displayShow(" COMENTARIO>",
                                    "¡Comentario demasiado largo!",
                                    "Muy largo: " + messageText,
                                    "",
                                    "",
                                    "<Atrás   (" + String(messageText.length()) + ")>");
                    }
                } else {
                    displayShow(" COMENTARIO>",
                                "No se detectó teclado",
                                "",
                                "",
                                "",
                                lastLine);
                }
                break;

            /////////

            case 9000:  // 9. multiPress Menu ---> Apagar Tracker
                displayShow(" CONFIG>",
                            "> Apagar Tracker",
                            "  Configurar WiFi AP",
                            "",
                            "",
                            lastLine);
                break;

            case 9001:  // 9. multiPress Menu ---> Configurar WiFi
                displayShow(" CONFIG>",
                            "  Apagar Tracker",
                            "> Configurar WiFi AP",
                            "",
                            "",
                            lastLine);
                break;


//////////
            case 0: // Menu Principal
                String hdopState, firstRowMainMenu, secondRowMainMenu, thirdRowMainMenu, fourthRowMainMenu, fifthRowMainMenu, sixthRowMainMenu;

                firstRowMainMenu = currentBeacon->callsign;
                if (Config.display.showSymbol) {
                    for (int j = firstRowMainMenu.length(); j < 9; j++) {
                        firstRowMainMenu += " ";
                    }
                    if (!symbolAvailable) {
                        firstRowMainMenu += currentBeacon->symbol;
                    }
                }
                // Segunda fila del menú principal: Fecha y Hora o mensaje LoRa APRS TNC
                if (disableGPS) {
                    secondRowMainMenu = "";
                    thirdRowMainMenu = "    LoRa APRS TNC";
                    fourthRowMainMenu = "";
                } else {
                    const auto time_now = now();
                    secondRowMainMenu = Utils::createDateString(time_now) + "   " + Utils::createTimeString(time_now);
                    if (time_now % 10 < 5) {
                        thirdRowMainMenu = String(gps.location.lat(), 4);
                        thirdRowMainMenu += " ";
                        thirdRowMainMenu += String(gps.location.lng(), 4);
                    } else {
                        thirdRowMainMenu = String(Utils::getMaidenheadLocator(gps.location.lat(), gps.location.lng(), 8));
                        thirdRowMainMenu += " LoRa[";
                        // Indicación de la frecuencia LoRa en el menú principal
                        switch (loraIndex) {
                            case 0: thirdRowMainMenu += "Eu]"; break;
                            case 1: thirdRowMainMenu += "PL]"; break;
                            case 2: thirdRowMainMenu += "UK]"; break;
                        }
                    }
                    // Tercera fila del menú principal: Latitud/Longitud o Locator + Satélites y HDOP
                    for (int i = thirdRowMainMenu.length(); i < 18; i++) {
                        thirdRowMainMenu += " ";
                    }

                    if (gps.hdop.hdop() > 5) {
                        hdopState = "X";
                    } else if (gps.hdop.hdop() > 2 && gps.hdop.hdop() < 5) {
                        hdopState = "-";
                    } else if (gps.hdop.hdop() <= 2) {
                        hdopState = "+";
                    }

                    if (gps.satellites.value() <= 9) thirdRowMainMenu += " ";
                    if (gpsIsActive) {
                        thirdRowMainMenu += String(gps.satellites.value());
                        thirdRowMainMenu += hdopState;
                    } else {
                        thirdRowMainMenu += "--";
                    }

                    // Cuarta fila del menú principal: Altitud, Velocidad y Rumbo
                    String fourthRowAlt = String(gps.altitude.meters(),0);
                    fourthRowAlt.trim();
                    for (int a = fourthRowAlt.length(); a < 4; a++) {
                        fourthRowAlt = "0" + fourthRowAlt;
                    }
                    String fourthRowSpeed = String(gps.speed.kmph(),0);
                    fourthRowSpeed.trim();
                    for (int b = fourthRowSpeed.length(); b < 3; b++) {
                        fourthRowSpeed = " " + fourthRowSpeed;
                    }
                    String fourthRowCourse = String(gps.course.deg(),0);
                    if (fourthRowSpeed == "  0") {
                        fourthRowCourse = "---";
                    } else {
                        fourthRowCourse.trim();
                        for (int c = fourthRowCourse.length(); c < 3; c++) {
                            fourthRowCourse = "0" + fourthRowCourse;
                        }
                    }
                    fourthRowMainMenu = "A=";
                    fourthRowMainMenu += fourthRowAlt;
                    fourthRowMainMenu += "m  ";
                    fourthRowMainMenu += fourthRowSpeed;
                    fourthRowMainMenu += "km/h  ";
                    fourthRowMainMenu += fourthRowCourse;
                    // Mensajes WLNK/APRS o dato sensor en lugar de Alt/Vel/Rumbo
                    if (Config.telemetry.active && (time_now % 10 < 5) && wxModuleType != 0) {
                        fourthRowMainMenu = WX_Utils::readDataSensor(1);
                    }
                    if (MSG_Utils::getNumWLNKMails() > 0) {
                        fourthRowMainMenu = "** WLNK Correo: ";
                        fourthRowMainMenu += String(MSG_Utils::getNumWLNKMails());
                        fourthRowMainMenu += " **";
                    }
                    if (MSG_Utils::getNumAPRSMessages() > 0) {
                        fourthRowMainMenu = "*** Mensajes: ";
                        fourthRowMainMenu += String(MSG_Utils::getNumAPRSMessages());
                        fourthRowMainMenu += " ***";
                    }
                    if (!gpsIsActive) {
                        fourthRowMainMenu = "*** GPS  Apagado ***";
                    }
                }
                
                // Quinta fila del menú principal: Dirección cardinal o último tracker escuchado
                if (showHumanHeading) {
                    fifthRowMainMenu = GPS_Utils::getCardinalDirection(gps.course.deg());
                } else {
                    fifthRowMainMenu = "Ultimo Rx = ";
                    fifthRowMainMenu += MSG_Utils::getLastHeardTracker();
                }
                
                // Estado de la batería para imprimir en el display
                if (batteryConnected) {
                    String batteryVoltage = BATTERY_Utils::getBatteryInfoVoltage();
                    #if defined(TTGO_T_Beam_V0_7) || defined(TTGO_T_LORA32_V2_1_GPS) || defined(TTGO_T_LORA32_V2_1_GPS_915) || defined(TTGO_T_LORA32_V2_1_TNC) || defined(TTGO_T_LORA32_V2_1_TNC_915) || defined(HELTEC_V3_GPS) || defined(HELTEC_V3_TNC) || defined(HELTEC_V3_2_GPS) || defined(HELTEC_V3_2_TNC) || defined(HELTEC_WIRELESS_TRACKER) || defined(HELTEC_WSL_V3_GPS_DISPLAY) || defined(TTGO_T_DECK_GPS) || defined(TTGO_T_DECK_PLUS) || defined(LIGHTTRACKER_PLUS_1_0)
                        sixthRowMainMenu = "Bateria: ";
                        sixthRowMainMenu += batteryVoltage;
                        sixthRowMainMenu += "V   ";
                        sixthRowMainMenu += BATTERY_Utils::getPercentVoltageBattery(batteryVoltage.toFloat());
                        sixthRowMainMenu += "%";
                    #endif
                    #if defined(HAS_AXP192) || defined(HAS_AXP2101)
                        String batteryCharge = POWER_Utils::getBatteryInfoCurrent();
                        #ifdef HAS_AXP192
                            if (batteryCharge.toInt() == 0) {
                                sixthRowMainMenu = "Bateria cargada ";
                                sixthRowMainMenu += batteryVoltage;
                                sixthRowMainMenu += "V";
                            } else if (batteryCharge.toInt() > 0) {
                                sixthRowMainMenu = "Bat: ";
                                sixthRowMainMenu += batteryVoltage;
                                sixthRowMainMenu += "V (cargando)";
                            } else {
                                sixthRowMainMenu = "Bateria ";
                                sixthRowMainMenu += batteryVoltage;
                                sixthRowMainMenu += "V ";
                                sixthRowMainMenu += batteryCharge;
                                sixthRowMainMenu += "mA";
                            }
                        #endif
                        #ifdef HAS_AXP2101
                            if (Config.notification.lowBatteryBeep && !POWER_Utils::isCharging() && batteryCharge.toInt() < lowBatteryPercent) {
                                lowBatteryPercent = batteryCharge.toInt();
                                NOTIFICATION_Utils::lowBatteryBeep();
                                if (batteryCharge.toInt() < 6) {
                                    NOTIFICATION_Utils::lowBatteryBeep();
                                }
                            } 
                            if (POWER_Utils::isCharging()) {
                                lowBatteryPercent = 21;
                            }
                            if (POWER_Utils::isCharging() && batteryCharge != "100") {
                                sixthRowMainMenu = "Bat: ";
                                sixthRowMainMenu += String(batteryVoltage);
                                sixthRowMainMenu += "V (cargando)";
                            } else if (!POWER_Utils::isCharging() && batteryCharge == "100") {
                                sixthRowMainMenu = "Bateria Cargada ";
                                sixthRowMainMenu += String(batteryVoltage);
                                sixthRowMainMenu += "V";
                            } else {
                                sixthRowMainMenu = "Bateria  ";
                                sixthRowMainMenu += String(batteryVoltage);
                                sixthRowMainMenu += "V   ";
                                sixthRowMainMenu += batteryCharge;
                                sixthRowMainMenu += "%";
                            }
                        #endif
                    #endif
                } else {
                    sixthRowMainMenu = "Bateria Desconectada";
                }
                displayShow(firstRowMainMenu,
                            secondRowMainMenu,
                            thirdRowMainMenu,
                            fourthRowMainMenu,
                            fifthRowMainMenu,
                            sixthRowMainMenu);
                break;
        }
    }

}