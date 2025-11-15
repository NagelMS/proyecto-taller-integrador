#include "kiss_utils.h"

namespace KISS_Utils {

    // Valida que un frame en formato TNC2 tenga la sintaxis básica:
    // debe contener '>' (separador src>dst) y ':' (inicio del payload) y el ':' debe ir después de '>'
    bool validateTNC2Frame(const String& tnc2FormattedFrame) {
        int colonPos        = tnc2FormattedFrame.indexOf(':');
        int greaterThanPos  = tnc2FormattedFrame.indexOf('>');
        return (colonPos != -1) && (greaterThanPos != -1) && (colonPos > greaterThanPos);
    }

    // Valida que un frame KISS tenga el byte FEND al inicio y al final
    bool validateKISSFrame(const String& kissFormattedFrame) {
        return kissFormattedFrame.charAt(0) == (char)KissChar::FEND && kissFormattedFrame.charAt(kissFormattedFrame.length() - 1) == (char)KissChar::FEND;
    }

    // Decodifica una dirección AX.25 (7 bytes) a texto legible.
    // - ax25Address: 7 bytes formateados (6 chars shifted, 1 byte SSID/flags)
    // - isLastAddress: output flag que indica si esta dirección es la última en la lista
    // - isRelay: indica si tratar el '*' (digipeated) como indicador
    String decodeAddressAX25(const String& ax25Address, bool& isLastAddress, bool isRelay) {
        String address = "";
        // Los primeros 6 bytes contienen caracteres ASCII desplazados >> 1
        for (int i = 0; i < 6; ++i) {
            uint8_t currentCharacter = ax25Address.charAt(i);
            currentCharacter >>= 1;                 // Desplazar hacia la derecha para recuperar el ascii
            if (currentCharacter != ' ') address += (char)currentCharacter; // Ignorar espacios padding
        }

        // El séptimo byte contiene SSID y flags
        auto ssidChar           = (uint8_t)ax25Address.charAt(6);
        bool hasBeenDigipited   = ssidChar & HAS_BEEN_DIGIPITED_MASK;      // flag de digipeat
        isLastAddress           = ssidChar & IS_LAST_ADDRESS_POSITION_MASK; // flag último address
        ssidChar >>= 1; // desplazar para obtener bits del SSID

        int ssid = 0b1111 & ssidChar; // SSID son los 4 bits menos significativos
        if (ssid) {
            address += '-';
            address += ssid; // añadir "-N" si SSID != 0
        }
        // Marcar con '*' si es relay y ya fue digipeado
        if (isRelay && hasBeenDigipited) address += '*';
        return address;
    }

    // Quita el encapsulado KISS (FEND / escapes) y devuelve el frame AX.25 interno
    String decapsulateKISS(const String& frame) {
        String ax25Frame = "";
        // Saltar los primeros 2 bytes (FEND + CMD) y el último FEND
        for (int i = 2; i < frame.length() - 1; ++i) {
            char currentChar = frame.charAt(i);
            if (currentChar == (char)KissChar::FESC) {
                // Si hay FESC, interpretar el siguiente byte según TFEND/TFESC
                char nextChar = frame.charAt(i + 1);
                if (nextChar == (char)KissChar::TFEND) {
                    ax25Frame += (char)KissChar::FEND; // reemplazar secuencia por FEND real
                } else if (nextChar == (char)KissChar::TFESC) {
                    ax25Frame += (char)KissChar::FESC; // reemplazar secuencia por FESC real
                }
                i++; // saltar el siguiente caracter ya procesado
            } else {
                ax25Frame += currentChar; // carácter normal
            }
        }
        return ax25Frame;
    }

    // Encapsula un frame AX.25 dentro de KISS, escapando FEND y FESC según KISS
    String encapsulateKISS(const String& ax25Frame, uint8_t command) {
        String kissFrame = "";
        kissFrame += (char)KissChar::FEND;                // inicio FEND
        kissFrame += (char)(0x0f & command);              // comando KISS (4 bits bajos)

        // Recorrer bytes AX.25 y escapar según sea necesario
        for (int i = 0; i < ax25Frame.length(); ++i) {
            char currentChar = ax25Frame.charAt(i);
            if (currentChar == (char)KissChar::FEND) {
                kissFrame += (char)KissChar::FESC;
                kissFrame += (char)KissChar::TFEND;
            } else if (currentChar == (char)KissChar::FESC) {
                kissFrame += (char)KissChar::FESC;
                kissFrame += (char)KissChar::TFESC;
            } else {
                kissFrame += currentChar;
            }
        }

        kissFrame += (char)KissChar::FEND; // fin de frame
        return kissFrame;
    }

    // Codifica una dirección legible "CALL-SSID[*]" a formato AX.25 (7 bytes)
    String encodeAddressAX25(String address) {
        bool hasBeenDigipited = address.indexOf('*') != -1;
        // Si no existe SSID explícito, añadir "-0"
        if (address.indexOf('-') == -1) {
            if (hasBeenDigipited) address = address.substring(0, address.length() - 1); // quitar '*'
            address += "-0";
        }

        int separatorIndex  = address.indexOf('-');
        int ssid            = address.substring(separatorIndex + 1).toInt();
        String kissAddress  = "";

        // Los primeros 6 bytes son los caracteres del callsign shift-left 1 (padding con espacios)
        for (int i = 0; i < 6; ++i) {
            char addressChar = ' ';
            if (address.length() > i && i < separatorIndex) addressChar = address.charAt(i);
            kissAddress += (char)(addressChar << 1); // shift left 1
        }

        // Séptimo byte: SSID (bits) + flags (marca de digipeat si aplica)
        kissAddress += (char)((ssid << 1) | 0b01100000 | (hasBeenDigipited ? HAS_BEEN_DIGIPITED_MASK : 0));
        return kissAddress;
    }

    // Decodifica un frame KISS a TNC2/AX.25 legible. dataFrame indica si es frame de datos (true) o comando (false)
    String decodeKISS(const String& inputFrame, bool& dataFrame) {
        String frame = "";
        if (KISS_Utils::validateKISSFrame(inputFrame)) {
            // El segundo byte contiene el tipo de comando; comparar con KissCmd::Data
            dataFrame = inputFrame.charAt(1) == KissCmd::Data;
            if (dataFrame) {
                // Si es frame de datos, decapsular y parsear direcciones AX.25
                String ax25Frame    = decapsulateKISS(inputFrame);
                bool isLastAddress         = false;

                // Destino: bytes 0..6 (7 bytes)
                String dstAddr      = decodeAddressAX25(ax25Frame.substring(0, 7), isLastAddress, false);
                // Origen: bytes 7..13
                String srcAddr      = decodeAddressAX25(ax25Frame.substring(7, 14), isLastAddress, false);

                // Construir prefijo "src>dst"
                frame = srcAddr + ">" + dstAddr;

                // Procesar digipeaters si existen (cada uno ocupa 7 bytes)
                int digiInfoIndex = 14;
                while (!isLastAddress && digiInfoIndex + 7 < ax25Frame.length()) {
                    String digiAddr = decodeAddressAX25(ax25Frame.substring(digiInfoIndex, digiInfoIndex + 7), isLastAddress, true);
                    frame += ',' + digiAddr;
                    digiInfoIndex += 7;
                }

                // Añadir ':' y el payload (se asume que el control + PID ocupan 2 bytes)
                frame += ':';
                frame += ax25Frame.substring(digiInfoIndex + 2);
            } else {
                // No es frame de datos: devolver el frame original (p.ej. comandos)
                frame += inputFrame;
            }
        }
        return frame;
    }

    // Codifica una línea TNC2 (src>dst[:payload]) a un frame KISS (AX.25 encapsulado + escapes)
    String encodeKISS(const String& frame) {
        String ax25Frame = "";

        if (KISS_Utils::validateTNC2Frame(frame)) {
            int colonIndex = frame.indexOf(':');

            String address = "";
            bool destinationAddressWritten = false;

            // Recorrer la parte de direcciones hasta el ':' e ir codificando cada address
            for (int i = 0; i <= colonIndex; i++) {
                char currentChar = frame.charAt(i);
                if (currentChar == ':' || currentChar == '>' || currentChar == ',') {
                    // Si aún no se escribió la dirección destino y encontramos ',' o ':',
                    // entonces insertar la dirección destino al inicio del ax25Frame
                    if (!destinationAddressWritten && (currentChar == ',' || currentChar == ':')) {
                        ax25Frame = encodeAddressAX25(address) + ax25Frame;
                        destinationAddressWritten = true;
                    } else {
                        // Añadir la dirección al final (origen o digipeaters)
                        ax25Frame += encodeAddressAX25(address);
                    }
                    address = ""; // reset para la próxima dirección
                } else {
                    address += currentChar; // acumular caracteres del callsign/SSID
                }
            }

            // Marcar el último address (poner la bandera IS_LAST_ADDRESS_POSITION_MASK en el último byte)
            auto lastAddressChar = (uint8_t)ax25Frame.charAt(ax25Frame.length() - 1);
            ax25Frame.setCharAt(ax25Frame.length() - 1, (char)(lastAddressChar | IS_LAST_ADDRESS_POSITION_MASK));

            // Añadir campos de control (Control + PID) siguiendo AX.25: 0x03 0xF0 típicos para UI frames
            ax25Frame += (char)AX25Char::ControlField;
            ax25Frame += (char)AX25Char::InformationField;

            // Añadir el payload (texto después de ':')
            ax25Frame += frame.substring(colonIndex + 1);
        }

        // Finalmente encapsular en KISS (escapando FEND/FESC) y marcar como comando Data
        String kissFrame = encapsulateKISS(ax25Frame, KissCmd::Data);
        return kissFrame;
    }

}