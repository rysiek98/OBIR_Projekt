#include <ObirDhcp.h>
#include <ObirEthernet.h>
#include <ObirEthernetUdp.h>
#include <OBIR_coap_server.h>
#include <SPI.h>

byte MAC[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
int vertex[50] = {0};
int edge[50] = {0};
int vertexLength = 0;
int edgeLength = 0;
int addedVertex = 0;
int addedEdge = 0;
coapServer coap;

//CoAP server Text endpoint
void callback_text(coapPacket *packet, ObirIPAddress ip, int port, int obs)
{
    if (obs == 0)
    {
        Serial.println("Hello World!");
        coap.sendResponse(ip, port, COAP_TEXT_PLAIN, "Hello World!");
    }
    else
    {
        unsigned long time = millis();
        Serial.print("Time: ");
        Serial.println(time);
        Serial.print("preMilis: ");
        Serial.println(coap.getPreviousMillis());
        coap.sendResponse(ip, port, COAP_TEXT_PLAIN, "Hello World!");
    }
}

void setup()
{
    Serial.begin(9600);

    ObirEthernet.begin(MAC);
    Serial.print("My IP address: ");
    Serial.print(ObirEthernet.localIP());
    Serial.println();
    Serial.println("Setup Callback");
    coap.server(callback_text, "text");
    coap.start();
}

void loop()
{
    coap.loop();
}
