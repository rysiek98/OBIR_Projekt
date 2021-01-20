/*
This file is part of the ESP-COAP Server library for Arduino

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

*/

#include "OBIR_coap_server.h"

//Tworzymy instancje klasy
//WiFiUDP Udp;
ObirEthernetUDP Udp;

int defaultMaxAge = 15000;
coapUri uri;
resource_dis resource[MAX_CALLBACK];
coapPacket *request = new coapPacket();
coapObserver observer[MAX_OBSERVER];
//Licznik dodanych zasobów
//counter for maintaining resource count
static uint8_t rcount = 0;
//Licznik obserwatorów
//counter for maintaing observer count
static uint8_t obscount = 0;
//Atrybuty funkcji obserwatora
//attributes of observe request
static uint8_t obsstate = 10;
//char *previousPayload = "";
//int16_t messid = 200;

//Konstruktor CoapURI
//constructor of coapuri class
coapUri::coapUri()
{
    for (int i = 0; i < MAX_CALLBACK; i++)
    {
        u[i] = "";
        c[i] = NULL;
    }
}

//Dodawanie zasobu
//adding resources
void coapUri::add(callback call, String url, resource_dis resource[])
{

    for (int i = 0; i < MAX_CALLBACK; i++)
        if (c[i] != NULL && u[i].equals(url))
        {
            c[i] = call;          //Dodanie "wywołania" do tablicy call
            rcount++;             //Zwiekszenie licznika zasobów
            resource[i].rt = url; //Dodanie adresu URL do tablicy obiektów klasy Resource_Dis (obsługa fun. Discovery)
            resource[i].ct = 0;   //Ustawienie parametru control flag obiektu klasy Resource_Dis
            if (i == 0)
                //resource[i].title="observable resource";
                return;
        }

    for (int i = 0; i < MAX_CALLBACK; i++)
        if (c[i] == NULL)
        {
            c[i] = call;
            u[i] = url;
            rcount++;
            resource[i].rt = url;
            resource[i].ct = 0;
            return;
        }
}
//Wyszukiwanie uri
//finding request url(resource)
callback coapUri::find(String url)
{
    for (int i = 0; i < MAX_CALLBACK; i++)
        if (c[i] != NULL && u[i].equals(url))
            return c[i];
    return NULL;
}

void coapServer::server(callback c, String url)
{
    uri.add(c, url, resource);
}
//Konstruktor klasy Coap
//constructor of coap class
coapServer::coapServer()
{
}
//Rozpoczynamy działanie serwera
//coap server begin
bool coapServer::start()
{
    this->start(COAP_DEFAULT_PORT);
    return true;
}

bool coapServer::start(int port)
{
    Udp.begin(port);
    return true;
}
//Konstruktor klasy CoapPacket
//constructor of coappacket class
coapPacket::coapPacket()
{
}

uint8_t coapPacket::version_()
{
    return version;
}

uint8_t coapPacket::type_()
{
    return type;
}

uint8_t coapPacket::code_()
{
    return code;
}

uint16_t coapPacket::messageid_()
{
    return messageid;
}

uint8_t *coapPacket::token_()
{
    return token;
}
//Parsowanie dodatkowej opcji
//parse option
int coapPacket::parseOption(coapOption *option, uint16_t *running_delta, uint8_t **buf, size_t buflen)
{

    uint8_t *p = *buf;
    uint8_t headlen = 1;
    uint16_t len, delta;

    if (buflen < headlen)
        return -1;
    //Początkowa wartość delty
    delta = (p[0] & 0xF0) >> 4;
    len = p[0] & 0x0F;

    if (delta == 13)
    {
        headlen++;
        if (buflen < headlen)
            return -1;
        delta = p[1] + 13;
        p++;
    }
    else if (delta == 14)
    {
        headlen += 2;
        if (buflen < headlen)
            return -1;
        delta = ((p[1] << 8) | p[2]) + 269;
        p += 2;
    }
    else if (delta == 15)
        return -1;

    if (len == 13)
    {
        headlen++;
        if (buflen < headlen)
            return -1;
        len = p[1] + 13;
        p++;
    }
    else if (len == 14)
    {
        headlen += 2;
        if (buflen < headlen)
            return -1;
        len = ((p[1] << 8) | p[2]) + 269;
        p += 2;
    }
    else if (len == 15)
        return -1;

    if ((p + 1 + len) > (*buf + buflen))
        return -1;
    option->number = delta + *running_delta;
    option->buffer = p + 1;
    option->length = len;
    *buf = p + 1 + len;
    *running_delta += delta;

    return 0;
}

//Główna pętla programu
bool coapServer::loop()
{

    uint8_t buffer[BUF_MAX_SIZE];
    int32_t packetlen = Udp.parsePacket();

    //Warunek decydujący o wrzuceniu odebranego pakietu do buffora
    if (packetlen > 0) {

        packetlen = Udp.read(buffer, packetlen >= BUF_MAX_SIZE ? BUF_MAX_SIZE : packetlen);
        //Odebrany pakiet parsujemy na obiekt klasy CoapPacket
        request->bufferToPacket(buffer, packetlen);
        //Sprawdzamy URI_PATH pakietu
        String url = "";
        for (int i = 0; i < request->optionnum; i++) {
            if (request->options[i].number == COAP_URI_PATH && request->options[i].length > 0) {
                char urlname[request->options[i].length + 1];
                memcpy(urlname, request->options[i].buffer, request->options[i].length);
                urlname[request->options[i].length] = NULL;
                if (url.length() > 0)
                    url += "/";
                url += urlname;
            }
        }

        if (!uri.find(url) && !(url == String(".well-known/core"))) {
            notFound(request, Udp);
        } else {
            if (request->code_() == COAP_EMPTY && (request->type_() == COAP_CON || request->type_() == COAP_RESET)) {
                request->type = COAP_RESET;
                request->code = COAP_EMPTY_MESSAGE;
                request->payload = NULL;
                request->payloadlen = 0;
                request->optionnum = 0;

                if (request->type_() == COAP_RESET) {
                    //Gdy dostaniemy puste żądanie typu COAP_RESET, usuwamy użytkownika z listy obserwatorów
                    for (uint8_t i = 0; i < obscount; i++) {
                        if (observer[i].observer_clientip == Udp.remoteIP()) {
                            delete observer[i].observer_token;
                            observer[i] = observer[i + 1];
                            observer[i + 1] = {0};
                            obscount = obscount - 1;
                        }
                    }
                }
                sendPacket(request, Udp.remoteIP(), Udp.remotePort());

            }else if (request->code_() == COAP_GET || request->code_() == COAP_PUT ||
            request->code_() == COAP_POST || request->code_() == COAP_DELETE) {
                //Generacja odpowiedzi gdy dostaniemy żądanie: GET, PUT, POST lub DELETE
                //Dla żądania GET sprawdzamy czy jest ustawiona opcja Observe
                if (request->code_() == COAP_GET) {
                    uint8_t num;
                    for (uint8_t i = 0; i <= request->optionnum; i++) {
                        if (request->options[i].number == COAP_OBSERVE) {
                            num = i;
                            break;
                        }
                    }
                    //Usunięcie użytkownika z listy obserwatorów
                    if (request->options[num].number == COAP_OBSERVE) {
                        if (*(request->options[num].buffer) == 1) {
                            for (uint8_t i = 0; i < obscount; i++) {
                                if (observer[i].observer_clientip == Udp.remoteIP() &&
                                    observer[i].observer_url == url) {
                                    delete observer[i].observer_token;
                                    observer[i] = observer[i + 1];
                                    observer[i + 1] = {0};
                                    obscount = obscount - 1;
                                }
                            }
                            //Wyszukiwanie żądanego zasobu
                            uri.find(url)(request, Udp.remoteIP(), Udp.remotePort(), 0);
                        } else {
                            //Dodanie użytkownika do listy obserwatorów
                            addObserver(url, request, Udp.remoteIP(), Udp.remotePort());
                        }
                    } else if (url == String(".well-known/core")) {
                        //Gdy wyszukiwane URI to .well-known/core wykonuje funkcje Discovery
                        resourceDiscovery(request, Udp.remoteIP(), Udp.remotePort(), resource);
                    } else {
                        //Gdy znajdziesz URI wyślij odp.
                        uri.find(url)(request, Udp.remoteIP(), Udp.remotePort(), 0);
                    }
                } else if (request->code_() == COAP_PUT) {
                    uri.find(url)(request, Udp.remoteIP(), Udp.remotePort(), 0);
                } else if (request->code == COAP_POST) {
                    int i;
                    for (i = 0; i < rcount; i++) {
                        if (resource[i].rt == url) {
                            uri.find(url)(request, Udp.remoteIP(), Udp.remotePort(), 0);
                            break;
                        }
                    }
                    if (i == rcount) {
                        //To do, add new resource
                    }
                } else if (request->code == COAP_DELETE) {
                    //To do, delete resource
                }
            }
            request->token = NULL;
        }
    }

    //checking for the change for resource
    unsigned long currentMillis = millis();
    for (int i = 0; i < obscount; i++)
    {
       if (currentMillis - observer[i].observer_prevMillis >= (unsigned long)observer[i].observer_maxAge) {
           request->code = COAP_GET;
           request->type = COAP_NONCON;
           request->tokenlen = observer[i].observer_tokenlen;
           request->token = observer[i].observer_token;
           uri.find(observer[i].observer_url)(request, observer[i].observer_clientip,
                                              observer[i].observer_clientport, 1);
           previousMillis = millis();
           observer[i].observer_prevMillis = (unsigned long)millis();
       }
    }

}
//Parsowanie zawartosci buffora na obiekt klasy CoapPacket
void coapPacket::bufferToPacket(uint8_t buffer[], int32_t packetlen)
{
    //Parsownie nagłówka
    //parse coap packet header
    version = (buffer[0] & 0xC0) >> 6;
    type = (buffer[0] & 0x30) >> 4;
    tokenlen = buffer[0] & 0x0F;
    code = buffer[1];
    messageid = 0xFF00 & (buffer[2] << 8);
    messageid |= 0x00FF & buffer[3];
    if (tokenlen == 0)
        token = NULL;
    else if (tokenlen <= 8)
    {

        token = new uint8_t(tokenlen);
        memset(token, 0, tokenlen);

        for (int i = 0; i < tokenlen; i++)
        {
            token[i] = buffer[4 + i];
        }
    }
    else
    {
        packetlen = Udp.parsePacket();
    }
    //Parsowanie opcji dodatkowych i payload'u
    //parse packet options/payload
    if (COAP_HEADER_SIZE + tokenlen < packetlen)
    {
        int optionIndex = 0;
        uint16_t delta = 0;
        uint8_t *end = buffer + packetlen;
        uint8_t *p = buffer + COAP_HEADER_SIZE + tokenlen;

        while (optionIndex < MAX_OPTION_NUM && *p != 0xFF && p < end)
        {

            options[optionIndex];
            if (0 == parseOption(&options[optionIndex], &delta, &p, end - p))

                optionIndex++;
        }
        optionnum = optionIndex;

        if (p + 1 < end && *p == 0xFF)
        {
            payload = p + 1;
            payloadlen = end - (p + 1);
        }
        else
        {

            payload = NULL;
            payloadlen = 0;
        }
    }
}

//Metoda pozwalająca wysłać pakiet
uint16_t coapServer::sendPacket(coapPacket *packet, ObirIPAddress ip, int port)
{
    uint8_t buffer[BUF_MAX_SIZE];
    uint8_t *p = buffer;
    uint16_t running_delta = 0;
    uint16_t packetSize = 0;
    //Tworzenie nagłówka
    //make coap packet base header
    *p = (packet->version) << 6;
    *p |= (packet->type & 0x03) << 4;
    *p++ |= (packet->tokenlen & 0x0F);
    *p++ = packet->code;
    *p++ = (packet->messageid >> 8);
    *p++ = (packet->messageid & 0xFF);
    p = buffer + COAP_HEADER_SIZE;
    packetSize += 4;
    //Token (z żądania)
    // make token
    if (packet->token != NULL && packet->tokenlen <= 8)
    {
        memcpy(p, packet->token, packet->tokenlen);
        p += packet->tokenlen;
        packetSize += packet->tokenlen;
    }
    //Opcje dodatkowe
    // make option header
    for (int i = 0; i < packet->optionnum; i++)
    {

        uint32_t optdelta;
        uint8_t len, delta;

        if (packetSize + 5 + packet->options[i].length >= BUF_MAX_SIZE)
        {
            return 0;
        }
        optdelta = packet->options[i].number - running_delta;
        COAP_OPTION_DELTA(optdelta, &delta);
        COAP_OPTION_DELTA((uint32_t)packet->options[i].length, &len);

        *p++ = (0xFF & (delta << 4 | len));
        if (delta == 13)
        {
            *p++ = (optdelta - 13);
            packetSize++;
        }
        else if (delta == 14)
        {
            *p++ = ((optdelta - 269) >> 8);
            *p++ = (0xFF & (optdelta - 269));
            packetSize += 2;
        }
        if (len == 13)
        {
            *p++ = (packet->options[i].length - 13);
            packetSize++;
        }
        else if (len == 14)
        {
            *p++ = (packet->options[i].length >> 8);
            *p++ = (0xFF & (packet->options[i].length - 269));
            packetSize += 2;
        }

        memcpy(p, packet->options[i].buffer, packet->options[i].length);
        p += packet->options[i].length;
        packetSize += packet->options[i].length + 1;
        running_delta = packet->options[i].number;
    }
    //Payload
    // make payload
    if (packet->payloadlen > 0)
    {

        if ((packetSize + 1 + packet->payloadlen) >= BUF_MAX_SIZE)
        {
            return 0;
        }
        *p++ = 0xFF;
        memcpy(p, packet->payload, packet->payloadlen);
        packetSize += 1 + packet->payloadlen;
    }

    Udp.beginPacket(ip, port);
    Udp.write(buffer, packetSize);
    Udp.endPacket();
}
//Funkcja Discovery
//resource discovery
void coapServer::resourceDiscovery(coapPacket *response, ObirIPAddress ip, int port, resource_dis resource[])
{

    String str_res;
    response->messageid++;
    for (int i = 0; i < rcount; i++)
    {
        str_res += "</";
        str_res += resource[i].rt;
        str_res += ">;";
        str_res += resource[i].rt;
        str_res += ";rt=";
        str_res += "\"";
        str_res += "observe";
        str_res += "\"";

        str_res += ";";
        str_res += "ct=";
        str_res += resource[i].ct;
        str_res += ";";
        if (i == 0)
        {
            str_res += "title=\"";
            str_res += "observable resource";
            str_res += "\"";
        }
        str_res += ",";
    }

    const char *payload = str_res.c_str();

    response->optionnum = 0;
    char optionBuffer[2];
    char optionBuffer_1[2];

    optionBuffer[0] = ((uint16_t)COAP_APPLICATION_LINK_FORMAT & 0xFF00) >> 8;
    optionBuffer[1] = ((uint16_t)COAP_APPLICATION_LINK_FORMAT & 0x00FF);

    response->options[response->optionnum].buffer = (uint8_t *)optionBuffer;
    response->options[response->optionnum].length = 2;
    response->options[response->optionnum].number = COAP_CONTENT_FORMAT;
    response->optionnum++;

    //optionBuffer_1[0] = ((uint16_t)MAX_AGE_DEFAULT & 0xFF00) >> 8;
    //optionBuffer_1[1] = ((uint16_t)MAX_AGE_DEFAULT & 0x00FF) ;

    //response->options[response->optionnum].buffer = (uint8_t *)optionBuffer_1;
    //response->options[response->optionnum].length = 2;
    //response->options[response->optionnum].number =COAP_MAX_AGE ;
    //response->optionnum++;

    response->code = COAP_CONTENT;
    response->payload = (uint8_t *)payload;
    response->payloadlen = strlen(payload);

    sendPacket(response, Udp.remoteIP(), Udp.remotePort());
}

//Metoda służy do wysyłania odpowiedzi na żądanie
void coapServer::sendResponse(ObirIPAddress ip, int port, COAP_CONTENT_TYPE contentType, char *payload, uint8_t payloadLen)
{
    coapPacket *response = new coapPacket();

    request->messageid++;
    response->version = request->version;
    response->tokenlen = request->tokenlen;
    response->messageid = request->messageid;
    response->token = request->token;

    if (request->type_() == COAP_CON)
    {
        response->type = COAP_ACK;
    }
    else if (request->type_() == COAP_NONCON)
    {
        response->type = COAP_NONCON;
    }

    uint8_t num, num2;
    for (uint8_t i = 0; i <= request->optionnum; i++)
    {
        if (request->options[i].number == COAP_OBSERVE)
        {
            num = i;
        }

        if (request->options[i].number == COAP_E_TAG)
        {
            num2 = i;
        }
    }

    response->optionnum = 0;

    if (request->options[num2].number == COAP_E_TAG)
    {
        response->options[response->optionnum].buffer = request->options[num2].buffer;
        response->options[response->optionnum].length = request->options[num2].length;
        response->options[response->optionnum].number = COAP_E_TAG;
        response->optionnum++;
    }

    if (request->code_() == COAP_GET)
    {
        response->code = COAP_CONTENT;
        response->payload = (uint8_t *)payload;
        response->payloadlen = strlen(payload);

        if (request->options[num].number == COAP_OBSERVE)
        {

            obsstate = obsstate + 1;

            response->options[response->optionnum].buffer = &obsstate;
            response->options[response->optionnum].length = 1;
            response->options[response->optionnum].number = COAP_OBSERVE;
            response->optionnum++;
        }

        char optionBuffer2[2];
        optionBuffer2[0] = ((uint16_t)contentType & 0xFF00) >> 8;
        optionBuffer2[1] = ((uint16_t)contentType & 0x00FF);
        response->options[response->optionnum].buffer = (uint8_t *)optionBuffer2;
        response->options[response->optionnum].length = 2;
        response->options[response->optionnum].number = COAP_CONTENT_FORMAT;
        response->optionnum++;

        sendPacket(response, ip, port);
    }
    else if (request->code_() == COAP_PUT)
    {
        //String str = "PUT OK";
        //const char *payload = str.c_str();
        response->code = COAP_CHANGED;
        response->payload = (uint8_t *)payload;
        response->payloadlen = payloadLen;
        char optionBuffer[2];
        optionBuffer[0] = ((uint16_t)contentType & 0xFF00) >> 8;
        optionBuffer[1] = ((uint16_t)contentType & 0x00FF);
        response->options[response->optionnum].buffer = (uint8_t *)optionBuffer;
        response->options[response->optionnum].length = 2;
        response->options[response->optionnum].number = COAP_CONTENT_FORMAT;
        response->optionnum++;

        sendPacket(response, ip, port);
    }
    else if (request->code_() == COAP_POST)
    {
        String str = "Post changed";
        const char *payload = str.c_str();
        response->code = COAP_CHANGED;
        response->payload = (uint8_t *)payload;
        response->payloadlen = payloadLen;
        char optionBuffer[2];
        optionBuffer[0] = ((uint16_t)contentType & 0xFF00) >> 8;
        optionBuffer[1] = ((uint16_t)contentType & 0x00FF);
        response->options[response->optionnum].buffer = (uint8_t *)optionBuffer;
        response->options[response->optionnum].length = 2;
        response->options[response->optionnum].number = COAP_CONTENT_FORMAT;
        response->optionnum++;

        sendPacket(response, ip, port);
    }
    delete response;
}

void coapServer::sendResponse(ObirIPAddress ip, int port, char *payload, uint8_t payloadLen)
{
    this->sendResponse(ip, port, COAP_TEXT_PLAIN, payload, payloadLen);
}

//add observer
void coapServer::addObserver(String url, coapPacket *request, ObirIPAddress ip, int port)
{
    //storing the details of clients
    observer[obscount].observer_token = request->token;
    observer[obscount].observer_tokenlen = request->tokenlen;
    observer[obscount].observer_clientip = ip;
    observer[obscount].observer_clientport = port;
    observer[obscount].observer_url = url;
    observer[obscount].observer_maxAge = defaultMaxAge;
    for(int i=0; i<request->optionnum; i++){
        if(request->options[i].number == COAP_MAX_AGE){
            observer[obscount].observer_maxAge = (*request->options[i].buffer)*1000;
            break;
        }
    }
    obscount = obscount + 1;
    previousMillis = millis();
    observer[obscount].observer_prevMillis = (unsigned long)millis();
}


//make nofification packet and send
void coapServer::notification(char *payload, String url)
{
    request->code = COAP_GET;
    request->type = COAP_NONCON;
    request->payload = payload;
    request->payloadlen = strlen(payload);
    request->optionnum = 0;
    obsstate = obsstate + 1;
    request->options[request->optionnum].buffer = &obsstate;
    request->options[request->optionnum].length = 1;
    request->options[request->optionnum].number = COAP_OBSERVE;
    request->optionnum++;
    char optionBuffer2[2];
    optionBuffer2[0] = ((uint16_t)COAP_TEXT_PLAIN & 0xFF00) >> 8;
    optionBuffer2[1] = ((uint16_t)COAP_TEXT_PLAIN & 0x00FF);
    request->options[request->optionnum].buffer = (uint8_t *)optionBuffer2;
    request->options[request->optionnum].length = 2;
    request->options[request->optionnum].number = COAP_CONTENT_FORMAT;
    request->optionnum++;
    for (uint8_t i = 0; i < obscount; i++)
    {
        //send notification
        if (observer[i].observer_url == url)
        {
            request->tokenlen = observer[i].observer_tokenlen;
            request->token = observer[i].observer_token;
            sendPacket(request, observer[i].observer_clientip,
                       observer[i].observer_clientport);
            previousMillis = millis();
            observer[i].observer_prevMillis = (unsigned long)millis();
        }
    }
}

long coapServer::getPreviousMillis()
{
    return previousMillis;
}

void coapServer::notFound(coapPacket *request, ObirEthernetUDP Udp){
    //Gdy żądany zasób nie występuje w bazie, serwer wysyła odp. z kodem NOT FOUND
    request->payload = NULL;
    request->payloadlen = 0;
    request->code = COAP_NOT_FOUND;

    request->optionnum = 0;
    //Dodatkowe opcje dodawane do pakietu
    char optionBuffer[2];
    optionBuffer[0] = ((uint16_t)COAP_TEXT_PLAIN & 0xFF00) >> 8;
    optionBuffer[1] = ((uint16_t)COAP_TEXT_PLAIN & 0x00FF);
    request->options[request->optionnum].buffer = (uint8_t *)optionBuffer;
    request->options[request->optionnum].length = 2;
    request->options[request->optionnum].number = COAP_CONTENT_FORMAT;
    request->optionnum++;
    //Wyślij odpowiedź
    sendPacket(request, Udp.remoteIP(), Udp.remotePort());
}