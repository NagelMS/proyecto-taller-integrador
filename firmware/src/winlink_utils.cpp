#include "winlink_utils.h"
#include "configuration.h"
#include "msg_utils.h"
#include "display.h"
#include "logger.h"

// -----------------------------------------------------------------------------
// Winlink: reto-respuesta y login
// -----------------------------------------------------------------------------


extern      Configuration           Config;
extern      int                     menuDisplay;
extern      logging::Logger         logger;

uint8_t     winlinkStatus           = 0;
String      winlinkMailNumber       = "_?";
String      winlinkAddressee        = "";
String      winlinkSubject          = "";
String      winlinkBody             = "";
String      winlinkAlias            = "";
String      winlinkAliasComplete    = "";
bool        winlinkCommentState     = false;

uint32_t    lastChallengeTime       = 0;
String      challengeAnswer;


namespace WINLINK_Utils {

    void processWinlinkChallenge(const String& winlinkInteger) { // Decodifica números→índices de password y arma respuesta pseudo-aleatoria
        uint32_t currenTime = millis();
        if (lastChallengeTime == 0 || (currenTime - lastChallengeTime) > 10 * 60 * 10000) {
            challengeAnswer = "";
            for (char c : winlinkInteger) {
                int digit = c - '0'; // Convierte ASCII a entero                                            // Convert '0'-'9' to 0-9
                if (digit < 1 || digit > Config.winlink.password.length()) {    // Ensure valid range
                    displayShow(" WINLINK", "", "PASS Length<REQUIRED", "", "", "", 2000);
                    challengeAnswer += Config.winlink.password[0];
                } else {
                    challengeAnswer += Config.winlink.password[digit - 1];  // Adjust for 1-based indexing
                }
            }
            challengeAnswer += char(random(65, 91)); // Mezcla letras A-Z
            challengeAnswer += char(random(48, 57)); // Mezcla dígitos 0-9
            challengeAnswer += char(random(65, 91)); // Mezcla letras A-Z
            lastChallengeTime = currenTime;
        }
        MSG_Utils::addToOutputBuffer(1, "WLNK-1", challengeAnswer);
    }

    void login() { // Inicia sesión Winlink y cambia menú de navegación
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Winlink","---> Start Login");
        displayShow(" WINLINK", "" , "Login Initiation ...", "", "" , "<Back");
        if (winlinkStatus == 5) {
            menuDisplay = 5000;
        } else {
            menuDisplay = 500;
            winlinkStatus = 1;
            MSG_Utils::addToOutputBuffer(1, "WLNK-1", "Start");
        }
    }

}
