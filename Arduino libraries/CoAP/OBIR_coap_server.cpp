/*
This file is part of the ESP-COAP Server library for Arduino

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

The ESP-CoAP is maintained by thingTronics Innovations.
Main contributor:

    Poornima Nagesh @poornima.nagesh@thingtronics.com
    Lovelesh Patel @lovelesh.patel@thingtronics.com

https://github.com/automote/ESP-CoAP
*/

#include "OBIR_coap_server.h"

//WiFiUDP Udp;
//Tworzymy instancje klasy ObirEthernetUDP
ObirEthernetUDP Udp;
//Tworzymy instancje klasy coapUri
coapUri uri;
//Tworzymy tablice obiektów klasy resource_dis, reprezentuja dostępne zasoby
resource_dis resource[MAX_CALLBACK];
//Tworzymy instancje klasy coapPacket
coapPacket *request = new coapPacket();
//coapPacket *response=new coapPacket();
//Tworzymy tablice obiektów klasy coapObserver, bedą w niej przechowywani aktualni obserwatorowie
coapObserver observer[MAX_OBSERVER];
//Tworzymy wskaźnik typu coapObserver
coapObserver *actualObserver = nullptr;
//Licznik dodanych zasobów
static uint8_t rcount = 0;
//Licznik obserwatorów
static uint8_t obscount = 0;
//Atrybuty funkcji obserwatora
static uint8_t obsstate = 10;
//unsigned long interval =1000;
//unsigned long previousMillis=0;
//char *previousPayload = "";
//int16_t messid = 200;

//Zmienne globalne stored przechowują zapisaną odpowiedź. Funkcjonalność własna
uint8_t *storedETag = nullptr;
uint8_t storedETagLen = 0;
uint8_t *storedResponse = nullptr;
uint8_t storedResponseLen = 0;

//Konstruktor coapURI. Funkcja niemodyfikowana.
coapUri::coapUri()
{
    for (int i = 0; i < MAX_CALLBACK; i++)
    {
        u[i] = "";
        c[i] = NULL;
    }
}

//Dodawanie nowego zasobu.  Funkcja niemodyfikowana.
void coapUri::add(callback call, String url, resource_dis resource[])
{

    for (int i = 0; i < MAX_CALLBACK; i++)
        if (c[i] != NULL && u[i].equals(url))  //aktualizacja zasobu
        {
            c[i] = call;          //Dodanie "wywołania" do tablicy callback
            rcount++;             //Zwiekszenie licznika zasobów
            resource[i].rt = url; //Dodanie adresu URL do tablicy obiektów klasy resource_dis (obsługa fun. Discovery)
            resource[i].ct = 0;   //Ustawienie parametru control flag obiektu klasy resource_dis
         /*   if (i == 0)
                //resource[i].title="observable resource";
                return;*/
        }

    for (int i = 0; i < MAX_CALLBACK; i++)
        if (c[i] == NULL) //nowy zasób
        {
            c[i] = call;
            u[i] = url;
            rcount++;
            resource[i].rt = url;
            resource[i].ct = 0;
            return;
        }
}
//Wyszukiwanie żądanego uri.  Funkcja niemodyfikowana.
callback coapUri::find(String url)
{
    for (int i = 0; i < MAX_CALLBACK; i++)
        if (c[i] != NULL && u[i].equals(url))
            return c[i];
    return NULL;
}
//Funkcja dodaje nowy zasób. Funkcja niemodyfikowana.
void coapServer::server(callback c, String url)
{
    uri.add(c, url, resource);
}
//Konstruktor klasy Coap. Funkcja niemodyfikowana.
coapServer::coapServer()
{
}
//Rozpoczynamy działanie serwera. Funkcja niemodyfikowana.
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
//Konstruktor klasy CoapPacket.
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
//Parsowanie dodatkowej opcji. Funkcja niemodyfikowana.
int coapPacket::parseOption(coapOption *option, uint16_t *running_delta, uint8_t **buf, size_t buflen)
{

    uint8_t *p = *buf;
    uint8_t headlen = 1;
    uint16_t len = 0;
    uint16_t delta = 0;

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

//Główna pętla programu. Funkcja znacznie zmodyfikowana.
bool coapServer::loop()
{
    //Rezerwujemy pamięć na odebrany pakiet.
    uint8_t *buffer = new uint8_t[BUF_MAX_SIZE];
    for(int i=0;i<BUF_MAX_SIZE;i++)
    {
        buffer[i]=0;
    }
    int32_t packetlen = Udp.parsePacket();

    //Warunek decydujący o rozpoczęciu przetwarzania odebranego pakietu.
    if (packetlen > 0) {

        packetlen = Udp.read(buffer, packetlen >= BUF_MAX_SIZE ? BUF_MAX_SIZE : packetlen);
        //Odebrany pakiet parsujemy na obiekt klasy coapPacket.
        request->bufferToPacket(buffer, packetlen);
        //Sprawdzamy czy pakiet ma URI_PATH i odczytujemy jego zawartość.
        String url = "";
        for (int i = 0; i < request->optionnum; i++) {
            if (request->options[i].number == COAP_URI_PATH && request->options[i].length > 0) {
                char urlname[request->options[i].length + 1] = {0};
                memcpy(urlname, request->options[i].buffer, request->options[i].length);
                urlname[request->options[i].length] = NULL;
                if (url.length() > 0)
                    url += "/";
                url += urlname;
            }
        }
        //Jeśli odebrane URI nie jest w liście zasobów, nie jest to .well-known/core a pakiet nie jest pusty to  wyśli Not Found.
        if (!uri.find(url) && !(url == String(".well-known/core")) && !(request->code_() == COAP_EMPTY)) {
            notFound(request, Udp);
        } else {//Gdy dostaniemy pusty pakiet.
            if (request->code_() == COAP_EMPTY && (request->type_() == COAP_CON || request->type_() == COAP_RESET || request->type_() == COAP_NONCON)) {
                if(request->type_() == COAP_CON) {
                    request->type = COAP_ACK;
                }else{
                    request->type = COAP_RESET;
                }
                request->code = COAP_EMPTY_MESSAGE;
                request->payload = nullptr;
                request->payloadlen = 0;
                request->optionnum = 0;

                if (request->type_() == COAP_RESET) {//Gdy otrzymamy pusty pakiet, z codem Reset to czyścimy liste obserwatorów z tego IP.
                    //Gdy dostaniemy puste żądanie typu COAP_RESET, usuwamy użytkownika z listy obserwatorów.
                    for (uint8_t i = 0; i < obscount; i++) {
                        if (observer[i].observer_clientip == Udp.remoteIP()) {
                            observer[i].deleteObserver();
                            observer[i] = observer[i + 1];
                            observer[i + 1] = {0};
                            obscount--;
                        }
                    }
                }
                sendPacket(request, Udp.remoteIP(), Udp.remotePort());

            }else if (request->code_() == COAP_GET || request->code_() == COAP_PUT ||
            request->code_() == COAP_POST || request->code_() == COAP_DELETE) {
                //Generacja odpowiedzi na żądanie: GET, PUT
                //Dla żądania GET sprawdzamy czy jest ustawiona opcja Observe.
                if (request->code_() == COAP_GET) {
                    uint8_t num;
                    for (uint8_t i = 0; i <= request->optionnum; i++) {
                        if (request->options[i].number == COAP_OBSERVE) {
                            num = i;
                            break;
                        }
                    }
                    //Usunięcie użytkownika z listy obserwatorów.
                    if (request->options[num].number == COAP_OBSERVE) {
                        if (*(request->options[num].buffer) == 1) {
                            for (uint8_t i = 0; i < obscount; i++) {
                                if (observer[i].observer_clientip == Udp.remoteIP() &&
                                    observer[i].observer_url == url) {
                                    observer[i].deleteObserver();
                                    observer[i] = observer[i + 1];
                                    observer[i + 1] = {0};
                                    obscount--;
                                }
                            }
                            //Wyszukiwanie żądanego zasobu i wywołanie callback zasobu.
                            uri.find(url)(request, Udp.remoteIP(), Udp.remotePort(), 0,request->accept());
                        } else {
                            //Dodanie użytkownika do listy obserwatorów.
                            addObserver(url, request, Udp.remoteIP(), Udp.remotePort());
                        }
                    } else if (url == String(".well-known/core")) {
                        //Gdy wyszukiwane URI to .well-known/core wykonuje funkcje Discovery.
                        resourceDiscovery(request, Udp.remoteIP(), Udp.remotePort(), resource);
                    } else {
                        //Gdy znajdziesz żądane uri wywołaj callback zasobu.
                        uri.find(url)(request, Udp.remoteIP(), Udp.remotePort(), 0, request->accept());
                    }
                } else if (request->code_() == COAP_PUT) {
                    uri.find(url)(request, Udp.remoteIP(), Udp.remotePort(), 0, request->accept());
                }
            }
        }
    }

    //checking for the change for resource
    //Obsługa obserwatora.  Funkcjonalność znacznie zmodyfikowana.
    unsigned long currentMillis = millis();
    for (int i = 0; i < obscount; i++)
    {   //Sprawdzamy czy czas jaki czas upłynął od ostatniego wysłanego pakietu do obserwatora (czy jeszcze jest świeży czy już należy go odświeżyć).
        if (currentMillis - observer[i].observer_prevMillis >= (unsigned long)observer[i].observer_maxAge) {
            //Jeśli wymagane jest odświeżenie ważności odpowiedzi to ustaw wartości pakietu do wysłania i wywołaj callback zasobu.
            request->version = COAP_VERSION;
            request->code = COAP_GET;
            request->type = COAP_NONCON;
            request->optionnum = 0;
            request->options[0].buffer = &obsstate;
            request->options[0].length = 1;
            request->options[0].number = COAP_OBSERVE;
            request->optionnum = 1;
            //Ustawia wskaźnik na aktualnie aktualizowanego obserwatora.
            actualObserver = &observer[i];
            uri.find(observer[i].observer_url)(request, observer[i].observer_clientip,
                                               observer[i].observer_clientport, 1, request->accept());
            currentMillis = (unsigned long)millis();
            //Aktualizuje czas ostanie aktualizacji zasobu.
            observer[i].observer_prevMillis = currentMillis;
            actualObserver = nullptr;
        }
    }
    //Resetowanie atrybutów obiektu.
    delete [] request->token;
    request->token = nullptr;
    request->tokenlen = 0;
    request->optionnum = 0;
    request->payloadlen = 0;
    request->payload = nullptr;
    request->type = 0;
    request->code = 0;
    request->version = COAP_VERSION;
    if(request->messageid < 0 && request->messageid > 32000) {
        request->messageid = 10;
    }
    //Opróźnianie zarezerwowanej pamięci.
    delete [] buffer;
    buffer = nullptr;
}

// Orginalna funkcja loop
/*bool coapServer::loop() {

    uint8_t buffer[BUF_MAX_SIZE];
    int32_t packetlen = Udp.parsePacket();

    if (packetlen > 0) {

        packetlen = Udp.read(buffer, packetlen >= BUF_MAX_SIZE ? BUF_MAX_SIZE : packetlen);

        request->bufferToPacket(buffer,packetlen);

        //call endpoint url function

        String url = "";
        for (int i = 0; i < request->optionnum; i++) {
            if (request->options[i].number == COAP_URI_PATH && request->options[i].length > 0) {
                char urlname[request->options[i].length + 1];
                memcpy(urlname,request->options[i].buffer,request->options[i].length);
                urlname[request->options[i].length] = NULL;
                if(url.length() > 0)
                    url += "/";
                url += urlname;

            }
        }


        //response


        if(request->code_()==COAP_EMPTY && request->type_()== COAP_CON ){

            response->version=request->version;
            response->type=COAP_RESET;
            response->code=COAP_EMPTY_MESSAGE;
            response->messageid=request->messageid;
            response->token=request->token;
            response->payload=NULL;
            response->payloadlen=0;
            sendPacket(response,Udp.remoteIP(),Udp.remotePort());
        }else if(request->code_()==COAP_EMPTY && request->type_()==COAP_RESET){

            for(uint8_t i=0;i<obscount;i++){
                if(observer[i].observer_clientip==Udp.remoteIP()  && observer[i].observer_url==url){

                    observer[i]=observer[i+1];
                    observer[i+1]={0};
                    obscount=obscount-1;
                }
            }

        }
        else if(request->code_()==COAP_GET||request->code_()==COAP_PUT||request->code_()==COAP_POST||request->code_()==COAP_DELETE){

            if(request->type_()== COAP_CON){

                response->version=request->version;
                response->type=COAP_ACK;
                response->tokenlen=request->tokenlen;
                response->messageid=request->messageid;
                response->token=request->token;
            }
            else if (request->type_()==COAP_NONCON){
                response->version=request->version;
                response->type=COAP_NONCON;
                response->tokenlen=request->tokenlen;
                response->messageid=request->messageid;
                response->token=request->token;
            }

            if(request->code_()==COAP_GET){

                uint8_t num;
                for(uint8_t i=0;i<=request->optionnum;i++)
                {
                    if(request->options[i].number==COAP_OBSERVE){
                        num=i;
                        break;
                    }
                }



                if(request->options[num].number==COAP_OBSERVE){

                    if(*(request->options[num].buffer)==1){

                        for(uint8_t i=0;i<obscount;i++){
                            if(observer[i].observer_clientip==Udp.remoteIP()  && observer[i].observer_url==url){

                                observer[i]=observer[i+1];
                                observer[i+1]={0};
                                obscount=obscount-1;
                            }

                        }

                        uri.find(url)(request,Udp.remoteIP(),Udp.remotePort(),0);

                    }

                    else {

                        addObserver(url,request,Udp.remoteIP(),Udp.remotePort());

                    }
                }
                else if(url==String(".well-known/core")){

                    resourceDiscovery(response,Udp.remoteIP(),Udp.remotePort(),resource);

                }else if(!uri.find(url)){


                    response->payload=NULL;
                    response->payloadlen=0;
                    response->code=COAP_NOT_FOUND;

                    response->optionnum=0;

                    char optionBuffer[2];
                    optionBuffer[0] = ((uint16_t)COAP_TEXT_PLAIN  & 0xFF00) >> 8;
                    optionBuffer[1] = ((uint16_t)COAP_TEXT_PLAIN  & 0x00FF) ;
                    response->options[response->optionnum].buffer = (uint8_t *)optionBuffer;
                    response->options[response->optionnum].length = 2;
                    response->options[response->optionnum].number = COAP_CONTENT_FORMAT;
                    response->optionnum++;

                    sendPacket(response,Udp.remoteIP(),Udp.remotePort());

                }else{

                    uri.find(url)(request,Udp.remoteIP(),Udp.remotePort(),0);
                }

            }else if(request->code_()==COAP_PUT){

                if(!uri.find(url)){

                    response->payload=NULL;
                    response->payloadlen=0;
                    response->code=COAP_NOT_FOUND;


                    response->optionnum=0;

                    char optionBuffer[2];
                    optionBuffer[0] = ((uint16_t)COAP_TEXT_PLAIN  & 0xFF00) >> 8;
                    optionBuffer[1] = ((uint16_t)COAP_TEXT_PLAIN  & 0x00FF) ;
                    response->options[response->optionnum].buffer = (uint8_t *)optionBuffer;
                    response->options[response->optionnum].length = 2;
                    response->options[response->optionnum].number = COAP_CONTENT_FORMAT;
                    response->optionnum++;

                    sendPacket(response,Udp.remoteIP(),Udp.remotePort());

                }else{
                    uri.find(url)(request,Udp.remoteIP(),Udp.remotePort(),0);
                }
            }else if(request->code==COAP_POST){

                int i;
                for( i=0;i<rcount;i++){
                    if(resource[i].rt==url){

                        uri.find(url)(request,Udp.remoteIP(),Udp.remotePort(),0);
                        break;
                    }
                }
                if(i==rcount){
                    //add new resource

                }

            }else if(request->code==COAP_DELETE){


                if(!uri.find(url)){
                    response->payload=NULL;
                    response->payloadlen=0;
                    response->code=COAP_NOT_FOUND;

                    response->optionnum=0;

                    char optionBuffer[2];
                    optionBuffer[0] = ((uint16_t)COAP_TEXT_PLAIN  & 0xFF00) >> 8;
                    optionBuffer[1] = ((uint16_t)COAP_TEXT_PLAIN  & 0x00FF) ;
                    response->options[response->optionnum].buffer = (uint8_t *)optionBuffer;
                    response->options[response->optionnum].length = 2;
                    response->options[response->optionnum].number = COAP_CONTENT_FORMAT;
                    response->optionnum++;

                    sendPacket(response,Udp.remoteIP(),Udp.remotePort());

                }else{//delete

                }

            }
        }
        delete request->token;

    }

    //checking for the change for resource
    unsigned currentMillis=millis();
    if ((unsigned long)(currentMillis-previousMillis)>=interval)
    {
        //observing a resouce

        uri.find(resource[0].rt)(request,(0,0,0,0),NULL,1);
        previousMillis=millis();
    }

}*/


//Parsowanie zawartosci buffora na obiekt klasy coapPacket. Funkcja delikatnie modyfikowana.
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
        token = nullptr;
    else if (tokenlen <= 8)
    {
        //token = new uint8_t(tokenlen);
        token = new uint8_t[tokenlen];
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
        //payload = NULL;
        payload = nullptr;

        if (p + 1 < end && *p == 0xFF)
        {
            payload = p + 1;
            payloadlen = end - (p + 1);
        }
        else
        {
            //payload = NULL;
            payload = nullptr;
            payloadlen = 0;
        }
    }
}

//Metoda pozwalająca wysłać pakiet. Funkcja delikatnie modyfikowana.
uint16_t coapServer::sendPacket(coapPacket *packet, ObirIPAddress ip, int port)
{

    if(packet->type_() == COAP_RESET || packet->type_() == COAP_NONCON || packet->type_() == COAP_CON) {
        packet->messageid++;
        request->messageid = packet->messageid;
    }

    uint8_t buffer[BUF_MAX_SIZE];
    for(int i=0;i<BUF_MAX_SIZE;i++)
    {
        buffer[i]=0;
    }
    uint8_t *p = buffer;
    uint16_t running_delta = 0;
    uint16_t packetSize = 0;
    //Tworzenie nagłówka
    //make coap packet base header
    *p = (1) << 6;
    *p |= (packet->type & 0x03) << 4;
    *p++ |= (packet->tokenlen & 0x0F);
    *p++ = packet->code;
    *p++ = (packet->messageid >> 8);
    *p++ = (packet->messageid & 0xFF);
    p = buffer + COAP_HEADER_SIZE;
    packetSize += 4;

    //Token
    //if (packet->token != NULL && packet->tokenlen <= 8)
    if (packet->token != nullptr && packet->tokenlen <= 8)
    {
        memcpy(p, packet->token, packet->tokenlen);
        p += packet->tokenlen;
        packetSize += packet->tokenlen;
    }
    //Opcje dodatkowe
    for (int i = 0; i < packet->optionnum; i++)
    {

        uint32_t optdelta = 0;
        uint8_t len = 0;
        uint8_t delta = 0;

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

//Funkcja Discovery. Funkcja modyfikowana.
//resource discovery
void coapServer::resourceDiscovery(coapPacket *response, ObirIPAddress ip, int port, resource_dis resource[])
{
    int length = 0;
    int j = 0;
    //Obliczamy długość payload'u
    for (int i = 0; i < rcount; i++)
    {
        length += resource[i].rt.length();
        length += 5;
    }
    length--;
    //Zrezygnowaliśmy z wykorzystania Stringa w funkcji, uznaliśmy że char [] będzie lepszy.
    char payload[length] = {0};
    //String str_res;
    for (int i = 0; i < rcount; i++)
    {
//        str_res += "</";
//        str_res += resource[i].rt;
//        str_res += ">;";
//        str_res += resource[i].rt;
//        str_res += ";rt=";
//        str_res += "\"";
//        str_res += "observe";
//        str_res += "\"";
//
//        str_res += ";";
//        str_res += "ct=";
//        str_res += resource[i].ct;
//        str_res += ";";
//        if (i == 0)
//        {
//            str_res += "title=\"";
//            str_res += "observable resource";
//            str_res += "\"";
//        }
//        str_res += ",";
        payload[j] = '<';
        j++;
        payload[j] = '/';
        j++;
        for(int k = 0; k < resource[i].rt.length(); k++){
            payload[j] = resource[i].rt[k];
            j++;
        }
        payload[j] = '>';
        j++;
        payload[j] = ';';
        j++;
        payload[j] = ',';
        j++;
    }
    //const char *payload = str_res.c_str();

    response->optionnum = 0;
    char optionBuffer[2]={0};
    //Dodajemy opcje z formatowaniem payloadu'u w formacie COAP_APPLICATION_LINK_FORMAT
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
    response->payload = (uint8_t *)&payload;
    response->payloadlen = length;

    sendPacket(response, Udp.remoteIP(), Udp.remotePort());
}

//Metoda służy do przygotowania pakietu z odpowiedzią do wysłania. Funkcja modyfikowana.
void coapServer::sendResponse(ObirIPAddress ip, int port, int erType, COAP_CONTENT_TYPE contentType, char *payload, uint8_t payloadLen, int store)
{
    //Tworzymy obiekt w którym bedziemy przechowywać dane do wysłania.
    coapPacket *response = new coapPacket();

    response->version = COAP_VERSION;
    response->tokenlen = request->tokenlen;
    response->messageid = request->messageid;

    if(request->tokenlen > 0) {
        response->token = request->token;
    }

    if (request->type_() == COAP_CON)
    {
        response->type = COAP_ACK;
    }
    else if (request->type_() == COAP_NONCON)
    {
        response->type = COAP_NONCON;
    }

    uint8_t num;
    for (uint8_t i = 0; i <= request->optionnum; i++)
    {
        if (request->options[i].number == COAP_OBSERVE)
        {
            num = i;
        }
    }

    if (request->code_() == COAP_GET) {

        if (erType != -1){
            response->code = erType;
        }else {
            response->code = COAP_CONTENT;
        }
        response->payloadlen = payloadLen;
        if(payloadLen > 0) {
            response->payload = (uint8_t *) payload;
        } else{
            response->payload = nullptr;
        }

        response->optionnum = 0;
        //Obsługa ETag dla COAP_GET. Dodana funkcjonalność.
        if(store && !(request->options[num].number == COAP_OBSERVE)){
            if(storedETag == nullptr){
                storedResponse = new uint8_t[payloadLen];
                memcpy(storedResponse,(uint8_t *)payload,payloadLen);
                storedResponseLen = payloadLen;
                uint8_t eTag = (uint8_t)response->messageid + (uint8_t)55;
                storedETagLen = countLength(eTag);
                storedETag = new uint8_t[storedETagLen];
                memcpy(storedETag,&eTag, storedETagLen);
                response->options[response->optionnum].buffer = storedETag;
                response->options[response->optionnum].length = storedETagLen;
                response->options[response->optionnum].number = COAP_E_TAG;
                response->optionnum++;
                eTag = 0;
            }else if(compareArray(storedResponse, (uint8_t *)payload, storedResponseLen, payloadLen)){
                response->options[response->optionnum].buffer = storedETag;
                response->options[response->optionnum].length = storedETagLen;
                response->options[response->optionnum].number = COAP_E_TAG;
                response->optionnum++;
                response->code = COAP_VALID;
                response->payloadlen = 0;
                response->payload = nullptr;
            } else{
                delete [] storedResponse;
                delete [] storedETag;
                storedResponse = nullptr;
                storedETag = nullptr;
                storedResponse = new uint8_t[payloadLen];
                memcpy(storedResponse,(uint8_t *)payload, payloadLen);
                storedResponseLen = payloadLen;
                uint8_t eTag = (uint8_t)response->messageid + (uint8_t)55;
                storedETagLen = countLength(eTag);
                storedETag = new uint8_t[storedETagLen];
                memcpy( storedETag,&eTag, storedETagLen);
                response->options[response->optionnum].buffer = storedETag;
                response->options[response->optionnum].length = storedETagLen;
                response->options[response->optionnum].number = COAP_E_TAG;
                response->optionnum++;
                eTag = 0;
            }
        }
        //Obsługa obserwatora.
        if (request->options[num].number == COAP_OBSERVE){

            response->token = actualObserver->observer_token;
            response->tokenlen = actualObserver->observer_tokenlen;
            //Obsługa ETag dla obserwatora. Dodana funkcjonalność.
            if(actualObserver != nullptr){
                if(compareArray(actualObserver->observer_storedResponse, (uint8_t *)payload, actualObserver->observer_storedResponseLen, payloadLen)){
                    response->options[response->optionnum].buffer = actualObserver->observer_etag;
                    response->options[response->optionnum].length = actualObserver->observer_etagLen;
                    response->options[response->optionnum].number = COAP_E_TAG;
                    response->optionnum++;
                    response->code = COAP_VALID;
                    actualObserver->observer_repeatedPayload = 0;
                    response->payloadlen = 0;
                    response->payload = nullptr;

                } else if(actualObserver->observer_storedResponse == nullptr){
                    actualObserver->observer_storedResponse = new uint8_t[response->payloadlen];
                    memcpy(actualObserver->observer_storedResponse,(uint8_t *)payload, response->payloadlen);
                    actualObserver->observer_storedResponseLen = response->payloadlen;
                    uint8_t eTag = (uint8_t)response->messageid + obsstate;
                    actualObserver->observer_etagLen = countLength(eTag);
                    actualObserver->observer_etag = new uint8_t[actualObserver->observer_etagLen];
                    memcpy(actualObserver->observer_etag,&eTag, actualObserver->observer_etagLen);
                    response->options[response->optionnum].buffer = actualObserver->observer_etag;
                    response->options[response->optionnum].length = actualObserver->observer_etagLen;
                    response->options[response->optionnum].number = COAP_E_TAG;
                    response->optionnum++;
                    eTag = 0;
                } else if(actualObserver->observer_repeatedPayload >= 3){
                    actualObserver->observer_repeatedPayload = 0;
                    delete [] actualObserver->observer_storedResponse;
                    delete [] actualObserver->observer_etag;
                    actualObserver->observer_storedResponse = nullptr;
                    actualObserver->observer_etag = nullptr;
                    actualObserver->observer_storedResponse = new uint8_t[response->payloadlen];
                    memcpy(actualObserver->observer_storedResponse,(uint8_t *)payload, response->payloadlen);
                    actualObserver->observer_storedResponseLen = response->payloadlen;
                    uint8_t eTag = (uint8_t)response->messageid + obsstate;
                    actualObserver->observer_etagLen = countLength(eTag);
                    actualObserver->observer_etag = new uint8_t[actualObserver->observer_etagLen];
                    memcpy(actualObserver->observer_etag,&eTag, actualObserver->observer_etagLen);
                    response->options[response->optionnum].buffer = actualObserver->observer_etag;
                    response->options[response->optionnum].length = actualObserver->observer_etagLen;
                    response->options[response->optionnum].number = COAP_E_TAG;
                    response->optionnum++;
                    eTag = 0;
                } else{
                    actualObserver->observer_repeatedPayload++;
                }

            }

            obsstate = obsstate + 1;
            response->options[response->optionnum].buffer = &obsstate;
            response->options[response->optionnum].length = 1;
            response->options[response->optionnum].number = COAP_OBSERVE;
            response->optionnum++;

        }

//        char optionBuffer2[2];
//        optionBuffer2[0] = ((uint16_t)contentType & 0xFF00) >> 8;
//        optionBuffer2[1] = ((uint16_t)contentType & 0x00FF);
//        response->options[response->optionnum].buffer = (uint8_t *)optionBuffer2;
//        response->options[response->optionnum].length = 2;
//        response->options[response->optionnum].number = COAP_CONTENT_FORMAT;
//        response->optionnum++;

        //Obsługa Content Format. Dodana funkcjonalność.
        char optionBuffer[2] = {0};
        optionBuffer[0] = ((uint16_t)contentType & 0xFF00) >> 8;
        optionBuffer[1] = ((uint16_t)contentType & 0x00FF);
        response->options[response->optionnum].buffer = (uint8_t *)optionBuffer;
        response->options[response->optionnum].length = 2;
        response->options[response->optionnum].number = COAP_CONTENT_FORMAT;
        response->optionnum++;

        //Obsługa MAX_AGE. Dodana funkcjonalność.
        uint8_t maxAge = 0;
        if(actualObserver != nullptr) {
            maxAge = (uint8_t)(actualObserver->observer_maxAge / 1000);
            response->options[response->optionnum].buffer = &maxAge;
            response->options[response->optionnum].length = 1;
            response->options[response->optionnum].number = COAP_MAX_AGE;
            response->optionnum++;
        }

        sendPacket(response, ip, port);
    }
    else if (request->code_() == COAP_PUT)
    {
        //String str = "PUT OK";
        //const char *payload = str.c_str();
        if (erType != -1){
            response->code = erType;
        }else {
            response->code = COAP_CHANGED;
        }
        response->payloadlen = payloadLen;
        if(payloadLen > 0) {
            response->payload = (uint8_t *) payload;
        } else{
            response->payload = nullptr;
        }
        response->optionnum = 0;
        char optionBuffer[2];
        optionBuffer[0] = ((uint16_t)contentType & 0xFF00) >> 8;
        optionBuffer[1] = ((uint16_t)contentType & 0x00FF);
        response->options[response->optionnum].buffer = (uint8_t *)optionBuffer;
        response->options[response->optionnum].length = 2;
        response->options[response->optionnum].number = COAP_CONTENT_FORMAT;
        response->optionnum++;

        sendPacket(response, ip, port);
    }
    /*else if (request->code_() == COAP_POST)
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
    }*/

    //Usuwamy obiekt przechowujący pakiet po wysłaniu pakietu.
    delete response;
    response = nullptr;
}
//Przeciążona funkcja sendResponse.
void coapServer::sendResponse(ObirIPAddress ip, int port, int erType, COAP_CONTENT_TYPE contentType, char *payload, uint8_t payloadLen){
    this->sendResponse(ip, port, erType, contentType, payload, payloadLen, 0);
}
//Przeciążona funkcja sendResponse
void coapServer::sendResponse(ObirIPAddress ip, int port, COAP_CONTENT_TYPE contentType, char *payload, uint8_t payloadLen, int store){
    this->sendResponse(ip, port, -1, contentType, payload, payloadLen, store);
}
//Przeciążona funkcja sendResponse
void coapServer::sendResponse(ObirIPAddress ip, int port, COAP_CONTENT_TYPE contentType, char *payload, uint8_t payloadLen){
    this->sendResponse(ip, port, -1, contentType, payload, payloadLen,0);
}
//Przeciążona funkcja sendResponse
void coapServer::sendResponse(ObirIPAddress ip, int port, char *payload, uint8_t payloadLen)
{
    this->sendResponse(ip, port, COAP_TEXT_PLAIN, payload, payloadLen,0);
}

//Orginalna funkcja sendResponse.
/*void coapServer::sendResponse( IPAddress ip, int port, char *payload) {


    if(request->code_()==COAP_GET){
        response->code = COAP_CONTENT;
        response->payload = (uint8_t *)payload;
        response->payloadlen = strlen(payload);
        response->optionnum = 0;

        uint8_t num;
        for(uint8_t i=0;i<=request->optionnum;i++)
        {
            if(request->options[i].number==COAP_OBSERVE){
                num=i;
                break;
            }
        }


        if(request->options[num].number==COAP_OBSERVE){


            obsstate=obsstate+1;

            response->options[response->optionnum].buffer =&obsstate;
            response->options[response->optionnum].length = 1;
            response->options[response->optionnum].number = COAP_OBSERVE;
            response->optionnum++;
        }

        char optionBuffer2[2];
        optionBuffer2[0] = ((uint16_t)COAP_TEXT_PLAIN & 0xFF00) >> 8;
        optionBuffer2[1] = ((uint16_t)COAP_TEXT_PLAIN & 0x00FF) ;
        response->options[response->optionnum].buffer = (uint8_t *)optionBuffer2;
        response->options[response->optionnum].length = 2;
        response->options[response->optionnum].number = COAP_CONTENT_FORMAT;
        response->optionnum++;

        sendPacket(response, ip, port);


    }else if(request->code_()==COAP_PUT){
        String str="PUT OK";
        const char *payload=str.c_str();
        response->code = COAP_CHANGED;
        response->payload=(uint8_t *)payload;
        response->payloadlen = strlen(payload);
        response->optionnum = 0;
        char optionBuffer[2];
        optionBuffer[0] = ((uint16_t)COAP_TEXT_PLAIN & 0xFF00) >> 8;
        optionBuffer[1] = ((uint16_t)COAP_TEXT_PLAIN & 0x00FF) ;
        response->options[response->optionnum].buffer = (uint8_t *)optionBuffer;
        response->options[response->optionnum].length = 2;
        response->options[response->optionnum].number = COAP_CONTENT_FORMAT;
        response->optionnum++;

        sendPacket(response, ip, port);

    }else if(request->code_()==COAP_POST){
        String str="Post changed";
        const char *payload=str.c_str();
        response->code = COAP_CHANGED;
        response->payload=(uint8_t *)payload;
        response->payloadlen = strlen(payload);
        response->optionnum = 0;
        char optionBuffer[2];
        optionBuffer[0] = ((uint16_t)COAP_TEXT_PLAIN & 0xFF00) >> 8;
        optionBuffer[1] = ((uint16_t)COAP_TEXT_PLAIN & 0x00FF) ;
        response->options[response->optionnum].buffer = (uint8_t *)optionBuffer;
        response->options[response->optionnum].length = 2;
        response->options[response->optionnum].number = COAP_CONTENT_FORMAT;
        response->optionnum++;

        sendPacket(response, ip, port);

    }

}*/

//Dodawanie obserwatora. Funkcja modyfikowana.
void coapServer::addObserver(String url, coapPacket *request, ObirIPAddress ip, int port)
{
    //storing the details of clients
    observer[obscount].observer_tokenlen = request->tokenlen;
    observer[obscount].observer_token = new uint8_t[observer[obscount].observer_tokenlen];
    memcpy(observer[obscount].observer_token,request->token, request->tokenlen);
    observer[obscount].observer_clientip = ip;
    observer[obscount].observer_clientport = port;
    observer[obscount].observer_url = url;
    observer[obscount].observer_maxAge = MAX_AGE_DEFAULT*1000;
    for(int i=0; i<request->optionnum; i++){
        if(request->options[i].number == COAP_MAX_AGE){
            observer[obscount].observer_maxAge = ((unsigned long)(*request->options[i].buffer))*1000;
            break;
        }
    }
    obscount = obscount + 1;
    observer[obscount].observer_etag = nullptr;
    observer[obscount].observer_etagLen = 0;
    observer[obscount].observer_storedResponse = nullptr;
    observer[obscount].observer_storedResponseLen = 0;
    observer[obscount].observer_repeatedPayload = 0;
    observer[obscount].observer_prevMillis = (unsigned long)millis();
}

//Orginalna funkcja addObserver.
/*void coapServer::addObserver(String url,coapPacket *request,IPAddress ip,int port){

    //uri.find(url)(request,Udp.remoteIP(),Udp.remotePort());
    uri.find(url)(request,ip,port,0);

    //storing the details of clients
    observer[obscount].observer_token=request->token;
    observer[obscount].observer_tokenlen=request->tokenlen;
    observer[obscount].observer_clientip=ip;
    observer[obscount].observer_clientport=port;
    observer[obscount].observer_url=url;

    obscount=obscount+1;

}*/


//Wywoływanie notyfikacji obserwatora. Funkcja modyfikowana.
void coapServer::notification(char *payload, String url, uint8_t payloadLen)
{
    coapPacket *response = new coapPacket();

    response->version = COAP_VERSION;
    response->code = COAP_CONTENT;
    response->type = COAP_NONCON;
    response->payload = nullptr;
    response->payload = payload;
    response->payloadlen = payloadLen;
    response->optionnum = 0;
    obsstate = obsstate + 1;
    response->options[response->optionnum].buffer = &obsstate;
    response->options[response->optionnum].length = 1;
    response->options[response->optionnum].number = COAP_OBSERVE;
    response->optionnum++;
    response->options[response->optionnum].buffer = 0;
    response->options[response->optionnum].length = 0;
    response->options[response->optionnum].number = COAP_CONTENT_FORMAT;
    response->optionnum++;

    for (uint8_t i = 0; i < obscount; i++)
    {
        //send notification
        if (observer[i].observer_url == url)
        {
            observer[i].observer_repeatedPayload++;
            response->tokenlen = observer[i].observer_tokenlen;
            response->token = observer[i].observer_token;
            uint8_t maxAge = (uint8_t)(observer[i].observer_maxAge/1000);
            response->options[response->optionnum].buffer = &maxAge;
            response->options[response->optionnum].length = 1;
            response->options[response->optionnum].number = COAP_MAX_AGE;
            response->optionnum++;
            sendPacket(response, observer[i].observer_clientip,
                       observer[i].observer_clientport);
            observer[i].observer_prevMillis = (unsigned long)millis();
        }
    }
    delete response;
    response = nullptr;
}

//Orginalna funkcja notification
/*void coapServer::notification(char *payload)
{

    response->version=request->version;
    response->type=COAP_ACK;

    response->messageid=messid++;

    response->code = COAP_CONTENT;
    response->payload = (uint8_t *)payload;
    response->payloadlen = strlen(payload);
    response->optionnum = 0;

    obsstate=obsstate+1;

    response->options[response->optionnum].buffer =&obsstate;
    response->options[response->optionnum].length = 1;
    response->options[response->optionnum].number = COAP_OBSERVE;
    response->optionnum++;


    char optionBuffer2[2];
    optionBuffer2[0] = ((uint16_t)COAP_TEXT_PLAIN & 0xFF00) >> 8;
    optionBuffer2[1] = ((uint16_t)COAP_TEXT_PLAIN & 0x00FF) ;
    response->options[response->optionnum].buffer = (uint8_t *)optionBuffer2;
    response->options[response->optionnum].length = 2;
    response->options[response->optionnum].number = COAP_CONTENT_FORMAT;
    response->optionnum++;

    for(uint8_t i=0;i<obscount;i++){
        //send notification
        if(observer[i].observer_url==resource[0].rt){
            response->token=observer[i].observer_token;
            response->tokenlen=observer[i].observer_tokenlen;
            sendPacket(response, observer[i].observer_clientip, observer[i].observer_clientport);

        }

    }
    if(messid==5000)
        messid=0;

}*/


//Checking for the change in resource
//void coapServer::sendResponse(char *payload)
//{
//
//
//    if(strcmp(previousPayload,payload)!=0){
//
//        notification(payload);
//    }
//
//    previousPayload=payload;
//
//}

//Funkcja wysyła odpowieni kod gdy nie znajdzie żądanego zasobu. Funkcja własna.
void coapServer::notFound(coapPacket *request, ObirEthernetUDP Udp){
    //Gdy żądany zasób nie występuje w bazie, serwer wysyła odp. z kodem NOT FOUND
    request->payload = nullptr;
    request->payloadlen = 0;
    request->code = COAP_NOT_FOUND;
    request->optionnum = 0;

    //Dodatkowe opcje dodawane do pakietu.
    char optionBuffer[2];
    optionBuffer[0] = ((uint16_t)COAP_TEXT_PLAIN & 0xFF00) >> 8;
    optionBuffer[1] = ((uint16_t)COAP_TEXT_PLAIN & 0x00FF);
    request->options[request->optionnum].buffer = (uint8_t *)optionBuffer;
    request->options[request->optionnum].length = 2;
    request->options[request->optionnum].number = COAP_CONTENT_FORMAT;
    request->optionnum++;
    //Wyślij odpowiedź.
    sendPacket(request, Udp.remoteIP(), Udp.remotePort());
}

//Funkcja zwraca format payload'u oczekiwany przez klienta. Funkcja własna.
uint8_t coapPacket::accept() {
    if (optionnum != 0){
        for (int i = 0; i < optionnum; ++i) {
            if(options[i].number == COAP_ACCEPT){
                return *options[i].buffer;
            }
        }
        return (uint8_t)100;
    } else
        return (uint8_t)100;
}

//Funkcja porównuje dwie tablice. Funkcja własna.
bool coapServer::compareArray(uint8_t a[], uint8_t b[], uint8_t lenA, uint8_t lenB){
    if(a == NULL || b == NULL){
        return false;
    }
    if(lenA =! lenB){
        return false;
    }else {
        for (int i = 0; i < lenB; i++) {
            if (a[i] != b[i]) {
                return false;
            }
        }
        return true;
    }
}

//Funkcja oblicza długość tablicy w jakiej zmieści się int. Funkcja własna.
uint8_t coapServer::countLength(uint8_t messageid) {
    int tmp = messageid;
    int len = 0;
    while (tmp > 10){
        tmp /=10;
        len++;
    }
    len++;
    return len;
}

//Funkcja wywoływana w momencie usuwania obserwatora z listy. Zeruje dane. Funkcja własna.
void coapObserver::deleteObserver(){
    if(observer_token != NULL) {
        delete[] observer_token;
        observer_token = nullptr;
    }
    observer_tokenlen = 0;
    observer_url = "";
    observer_maxAge = 0;
    observer_prevMillis = 0;
    observer_etagLen = 0;
    observer_tokenlen = 0;
    delete [] observer_etag;
    observer_etag = nullptr;
    observer_repeatedPayload = 0;
    if(observer_storedResponse != NULL) {
        delete[] observer_storedResponse;
        observer_storedResponse = nullptr;
    }
    observer_storedResponseLen = 0;
}
