#include "coap-simple.h"
#include "Arduino.h"

#define LOGGING
//Metoda dodaje opcje do obiektu klasy CoapPacket
void CoapPacket::addOption(uint8_t number, uint8_t length, uint8_t *opt_payload)
{
    //Dodajemy do tablicy obiektów CoapOption parametry
    options[optionnum].number = number;
    options[optionnum].length = length;
    options[optionnum].buffer = opt_payload;
    //Zwiększamy optionnum (czyli informację o ilości opcji)
    ++optionnum;
}

//Coap::Coap(
//    UDP& udp
//) {
//    this->_udp = &udp;
//}

Coap::Coap(ObirEthernetUDP& obirEthernetUDP) {
    this->_obirEthernetUDP = &obirEthernetUDP;
}

//Metoda rozpoczyna pracę protokołu
bool Coap::start() {
    this->start(COAP_DEFAULT_PORT);
    return true;
}

//Metoda rozpoczyna pracę protokołu
bool Coap::start(int port) {
    //this->_udp->begin(port);
    this->_obirEthernetUDP->begin(port);
    return true;
}

//Metoda pozwala wysłać pakiet na domyślny adres portu
//Zmiana IPAddress na ObirIPAddress
uint16_t Coap::sendPacket(CoapPacket &packet, ObirIPAddress ip) {
    return this->sendPacket(packet, ip, COAP_DEFAULT_PORT);
}

//Metoda pozwala wysłać pakiet na wybrany adres portu
//Zmiana IPAddress na ObirIPAddress
uint16_t Coap::sendPacket(CoapPacket &packet, ObirIPAddress ip, int port) {
    uint8_t buffer[COAP_BUF_MAX_SIZE];
    uint8_t *p = buffer; //wskaźnik na tablice buffer
    uint16_t running_delta = 0;
    uint16_t packetSize = 0;

    // make coap packet base header
    *p = 0x01 << 6; //Operacja przesunięcia bitowego *p = 01000000
    *p |= (packet.type & 0x03) << 4;  // *p = *p | (packet.type & 0x03)
    *p++ |= (packet.tokenlen & 0x0F); // *p = *p | (packet.tokenlen & 0x0F), a potem skaczemy do następnego indeksu w bufforze
    *p++ = packet.code;
    *p++ = (packet.messageid >> 8);
    *p++ = (packet.messageid & 0xFF);
    p = buffer + COAP_HEADER_SIZE; // Przechodzimy do odpowiedniego indeksu w tablicy buffer
    packetSize += 4; //Zwiększamy rozmiar packetSize o rozmiar Header'a.

    // make token
    if (packet.token != NULL && packet.tokenlen <= 0x0F) {
        memcpy(p, packet.token, packet.tokenlen); // Kopiujemy packet.token do adresu wskazanego przez p
        p += packet.tokenlen; // Przechodzimy do odpowiedniego indeksu w tablicy buffer
        packetSize += packet.tokenlen; //Zwiększamy rozmiar packetSize o tokenlen
    }

    // Opcje dodatkowe
    // make option header
    for (int i = 0; i < packet.optionnum; i++)  {
        uint32_t optdelta;
        uint8_t len, delta;

        //Jeśli rozmiar pakietu + 5 + długość opcji >= 128
        if (packetSize + 5 + packet.options[i].length >= COAP_BUF_MAX_SIZE) {
            return 0;
        }

        optdelta = packet.options[i].number - running_delta;
        COAP_OPTION_DELTA(optdelta, &delta);
        COAP_OPTION_DELTA((uint32_t)packet.options[i].length, &len);

        *p++ = (0xFF & (delta << 4 | len));
        if (delta == 13) {
            *p++ = (optdelta - 13);
            packetSize++;
        } else if (delta == 14) {
            *p++ = ((optdelta - 269) >> 8);
            *p++ = (0xFF & (optdelta - 269));
            packetSize+=2;
        }
        if (len == 13) {
            *p++ = (packet.options[i].length - 13);
            packetSize++;
        } else if (len == 14) {
            *p++ = (packet.options[i].length >> 8);
            *p++ = (0xFF & (packet.options[i].length - 269));
            packetSize+=2;
        }

        memcpy(p, packet.options[i].buffer, packet.options[i].length);
        p += packet.options[i].length;
        packetSize += packet.options[i].length + 1;
        running_delta = packet.options[i].number;
    }

    // Wpisujemy payload do pakietu do wysłania
    // make payload
    if (packet.payloadlen > 0) {
        if ((packetSize + 1 + packet.payloadlen) >= COAP_BUF_MAX_SIZE) {
            return 0;
        }
        *p++ = 0xFF;
        memcpy(p, packet.payload, packet.payloadlen);
        packetSize += 1 + packet.payloadlen;
    }

    //_udp->beginPacket(ip, port);
    //_udp->write(buffer, packetSize);
    //_udp->endPacket();
    //Korzystając z UDP wysyłamy pakiet
    _obirEthernetUDP->beginPacket(ip, port);
    _obirEthernetUDP->write(buffer, packetSize);
    _obirEthernetUDP->endPacket();

    //Zwraca MID
    return packet.messageid;
}
//Zmiana IPAddress na ObirIPAddress
uint16_t Coap::get(ObirIPAddress ip, int port, const char *url) {
    return this->send(ip, port, url, COAP_CON, COAP_GET, NULL, 0, NULL, 0);
}

//Zmiana IPAddress na ObirIPAddress
uint16_t Coap::put(ObirIPAddress ip, int port, const char *url, const char *payload) {
    return this->send(ip, port, url, COAP_CON, COAP_PUT, NULL, 0, (uint8_t *)payload, strlen(payload));
}

//Zmiana IPAddress na ObirIPAddress
uint16_t Coap::put(ObirIPAddress ip, int port, const char *url, const char *payload, size_t payloadlen) {
    return this->send(ip, port, url, COAP_CON, COAP_PUT, NULL, 0, (uint8_t *)payload, payloadlen);
}

//Zmiana IPAddress na ObirIPAddress
uint16_t Coap::send(ObirIPAddress ip, int port, const char *url, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenlen, const uint8_t *payload, size_t payloadlen) {
    return this->send(ip, port, url, type, method, token, tokenlen, payload, payloadlen, COAP_NONE);
}

//Zmiana IPAddress na ObirIPAddress
uint16_t Coap::send(ObirIPAddress ip, int port, const char *url, COAP_TYPE type, COAP_METHOD method, const uint8_t *token, uint8_t tokenlen, const uint8_t *payload, size_t payloadlen, COAP_CONTENT_TYPE content_type) {

    // make packet
    CoapPacket packet;

    packet.type = type;
    packet.code = method;
    packet.token = token;
    packet.tokenlen = tokenlen;
    packet.payload = payload;
    packet.payloadlen = payloadlen;
    packet.optionnum = 0;
    packet.messageid = rand();

    // use URI_HOST UIR_PATH
    char ipaddress[16] = "";
    sprintf(ipaddress, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    packet.addOption(COAP_URI_HOST, strlen(ipaddress), (uint8_t *)ipaddress);

    // parse url
    int idx = 0;
    for (int i = 0; i < strlen(url); i++) {
        if (url[i] == '/') {
			packet.addOption(COAP_URI_PATH, i-idx, (uint8_t *)(url + idx));
            idx = i + 1;
        }
    }

    if (idx <= strlen(url)) {
		packet.addOption(COAP_URI_PATH, strlen(url)-idx, (uint8_t *)(url + idx));
    }

	// if Content-Format option
	uint8_t optionBuffer[2] {0};
	if (content_type != COAP_NONE) {
		optionBuffer[0] = ((uint16_t)content_type & 0xFF00) >> 8;
		optionBuffer[1] = ((uint16_t)content_type & 0x00FF) ;
		packet.addOption(COAP_CONTENT_FORMAT, 2, optionBuffer);
	}

    // send packet
    return this->sendPacket(packet, ip, port);
}

//Parsowanie opcji z odebrnego pakietu
int Coap::parseOption(CoapOption *option, uint16_t *running_delta, uint8_t **buf, size_t buflen) {
    uint8_t *p = *buf;
    uint8_t headlen = 1;
    uint16_t len, delta;

    if (buflen < headlen) return -1;

    delta = (p[0] & 0xF0) >> 4;
    len = p[0] & 0x0F;

    if (delta == 13) {
        headlen++;
        if (buflen < headlen) return -1;
        delta = p[1] + 13;
        p++;
    } else if (delta == 14) {
        headlen += 2;
        if (buflen < headlen) return -1;
        delta = ((p[1] << 8) | p[2]) + 269;
        p+=2;
    } else if (delta == 15) return -1;

    if (len == 13) {
        headlen++;
        if (buflen < headlen) return -1;
        len = p[1] + 13;
        p++;
    } else if (len == 14) {
        headlen += 2;
        if (buflen < headlen) return -1;
        len = ((p[1] << 8) | p[2]) + 269;
        p+=2;
    } else if (len == 15)
        return -1;

    if ((p + 1 + len) > (*buf + buflen))  return -1;
    option->number = delta + *running_delta;
    option->buffer = p+1;
    option->length = len;
    *buf = p + 1 + len;
    *running_delta += delta;

    return 0;
}

//Pętla protokołu
bool Coap::loop() {

    uint8_t buffer[COAP_BUF_MAX_SIZE];
    //int32_t packetlen = _udp->parsePacket();
    int32_t packetlen = _obirEthernetUDP->parsePacket();

    while (packetlen > 0) {
        //packetlen = _udp->read(buffer, packetlen >= COAP_BUF_MAX_SIZE ? COAP_BUF_MAX_SIZE : packetlen);
        packetlen = _obirEthernetUDP->read(buffer, packetlen >= COAP_BUF_MAX_SIZE ? COAP_BUF_MAX_SIZE : packetlen);

        CoapPacket packet;

        // parse coap packet header
        if (packetlen < COAP_HEADER_SIZE || (((buffer[0] & 0xC0) >> 6) != 1)) {
            //packetlen = _udp->parsePacket();
            packetlen = _obirEthernetUDP->parsePacket();
            continue;
        }

        packet.type = (buffer[0] & 0x30) >> 4;
        packet.tokenlen = buffer[0] & 0x0F;
        packet.code = buffer[1];
        packet.messageid = 0xFF00 & (buffer[2] << 8);
        packet.messageid |= 0x00FF & buffer[3];

        if (packet.tokenlen == 0)  packet.token = NULL;
        else if (packet.tokenlen <= 8)  packet.token = buffer + 4;
        else {
            //packetlen = _udp->parsePacket();
            packetlen = _obirEthernetUDP->parsePacket();
            continue;
        }

        // parse packet options/payload
        if (COAP_HEADER_SIZE + packet.tokenlen < packetlen) {
            int optionIndex = 0;
            uint16_t delta = 0;
            uint8_t *end = buffer + packetlen;
            uint8_t *p = buffer + COAP_HEADER_SIZE + packet.tokenlen;
            while(optionIndex < COAP_MAX_OPTION_NUM && *p != 0xFF && p < end) {
                //packet.options[optionIndex];
                if (0 != parseOption(&packet.options[optionIndex], &delta, &p, end-p))
                    return false;
                optionIndex++;
            }
            packet.optionnum = optionIndex;

            if (p+1 < end && *p == 0xFF) {
                packet.payload = p+1;
                packet.payloadlen = end-(p+1);
            } else {
                packet.payload = NULL;
                packet.payloadlen= 0;
            }
        }

        if (packet.type == COAP_ACK) {
            // call response function
            //resp(packet, _udp->remoteIP(), _udp->remotePort());
            resp(packet, _obirEthernetUDP->remoteIP(), _obirEthernetUDP->remotePort());

        } else {

            String url = "";
            // call endpoint url function
            for (int i = 0; i < packet.optionnum; i++) {
                if (packet.options[i].number == COAP_URI_PATH && packet.options[i].length > 0) {
                    char urlname[packet.options[i].length + 1];
                    memcpy(urlname, packet.options[i].buffer, packet.options[i].length);
                    urlname[packet.options[i].length] = 0;
                    if(url.length() > 0)
                      url += "/";
                    url += urlname;
                }
            }

            if (!uri.find(url)) {
                //sendResponse(_udp->remoteIP(), _udp->remotePort(), packet.messageid, NULL, 0,
                sendResponse(_obirEthernetUDP->remoteIP(), _obirEthernetUDP->remotePort(), packet.messageid, NULL, 0,
                        COAP_NOT_FOUND, COAP_NONE, NULL, 0);
            } else {
                //uri.find(url)(packet, _udp->remoteIP(), _udp->remotePort());
                uri.find(url)(packet, _obirEthernetUDP->remoteIP(), _obirEthernetUDP->remotePort());
            }
        }

        /* this type check did not use.
        if (packet.type == COAP_CON) {
            // send response
             sendResponse(_udp->remoteIP(), _udp->remotePort(), packet.messageid);
        }
         */

        // next packet
        //packetlen = _udp->parsePacket();
        packetlen = _obirEthernetUDP->parsePacket();
    }

    return true;
}

//Zmiana IPAddress na ObirIPAddress
uint16_t Coap::sendResponse(ObirIPAddress ip, int port, uint16_t messageid) {
    return this->sendResponse(ip, port, messageid, NULL, 0, COAP_CONTENT, COAP_TEXT_PLAIN, NULL, 0);
}

//Zmiana IPAddress na ObirIPAddress
uint16_t Coap::sendResponse(ObirIPAddress ip, int port, uint16_t messageid, const char *payload) {
    return this->sendResponse(ip, port, messageid, payload, strlen(payload), COAP_CONTENT, COAP_TEXT_PLAIN, NULL, 0);
}

//Zmiana IPAddress na ObirIPAddress
uint16_t Coap::sendResponse(ObirIPAddress ip, int port, uint16_t messageid, const char *payload, size_t payloadlen) {
    return this->sendResponse(ip, port, messageid, payload, payloadlen, COAP_CONTENT, COAP_TEXT_PLAIN, NULL, 0);
}

//Zmiana IPAddress na ObirIPAddress
uint16_t Coap::sendResponse(ObirIPAddress ip, int port, uint16_t messageid, const char *payload, size_t payloadlen,
                COAP_RESPONSE_CODE code, COAP_CONTENT_TYPE type, const uint8_t *token, int tokenlen) {
    // make packet
    CoapPacket packet;

    packet.type = COAP_ACK;
    packet.code = code;
    packet.token = token;
    packet.tokenlen = tokenlen;
    packet.payload = (uint8_t *)payload;
    packet.payloadlen = payloadlen;
    packet.optionnum = 0;
    packet.messageid = messageid;

    // if more options?
    uint8_t optionBuffer[2] = {0};
    optionBuffer[0] = ((uint16_t)type & 0xFF00) >> 8;
    optionBuffer[1] = ((uint16_t)type & 0x00FF) ;
	packet.addOption(COAP_CONTENT_FORMAT, 2, optionBuffer);

    return this->sendPacket(packet, ip, port);
}

//Moja wersja
uint16_t Coap::sendResponse(ObirIPAddress ip, int port, uint16_t messageid, const char *payload, size_t payloadlen,
                            COAP_RESPONSE_CODE code, COAP_CONTENT_TYPE type, const uint8_t *token, int tokenlen, int optionnum, CoapOption *options) {
    // make packet
    CoapPacket packet;

    packet.type = COAP_NONCON;
    packet.code = code;
    packet.token = token;
    packet.tokenlen = tokenlen;
    packet.payload = (uint8_t *)payload;
    packet.payloadlen = payloadlen;
    packet.optionnum = 0;
    packet.messageid = messageid;

    //ETag
    for(int i=0; i<optionnum; i++){
        if(options->number == 4){
            packet.addOption(COAP_E_TAG, options->length, options->buffer);
        }
        options++;
    }

    // COAP_CONTENT_TYPE
    uint8_t optionBuffer[2] = {0};
    optionBuffer[0] = ((uint16_t)type & 0xFF00) >> 8;
    optionBuffer[1] = ((uint16_t)type & 0x00FF);
    packet.addOption(COAP_CONTENT_FORMAT, 2, optionBuffer);

    //Potrzebujemy obsługi: ETag (od klienta), CONTENT_TYPE (od klienta), 

    return this->sendPacket(packet, ip, port);
}
