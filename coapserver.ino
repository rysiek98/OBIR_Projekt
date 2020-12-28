#include <ObirDhcp.h>           
#include <ObirEthernet.h>       
#include <ObirEthernetUdp.h>
#include <coap-simple.h>


byte MAC[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };

// CoAP client response callback
void callback_response(CoapPacket &packet, ObirIPAddress ip, int port);

// CoAP server endpoint url callback
void callback_light(CoapPacket &packet, ObirIPAddress ip, int port);

// ObirEthernetUDP and CoAP class
ObirEthernetUDP Udp;
Coap coap(Udp);

// LED STATE
bool LEDSTATE;

//CoAP server .well-know endpoint
void callback_well_know(CoapPacket &packet, ObirIPAddress ip, int port) {
  Serial.println(".well-know");
  char payload[] = "light";
  int payloadLen = strlen(payload);
  CoapOption *options = packet.options;
  //coap.sendResponse(ip, port, packet.messageid, payload, payloadLen, COAP_CONTENT, COAP_APPLICATION_LINK_FORMAT, packet.token, packet.tokenlen);
  coap.sendResponse(ip, port, packet.messageid, payload, payloadLen, COAP_CONTENT, COAP_APPLICATION_LINK_FORMAT, packet.token, packet.tokenlen, packet.optionnum, options);
}

// CoAP server light endpoint URL
void callback_light(CoapPacket &packet, ObirIPAddress ip, int port) {
  Serial.println("[Light] ON/OFF");
  
  // send response
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  
  String message(p);

  if (message.equals("0"))
    LEDSTATE = false;
  else if(message.equals("1"))
    LEDSTATE = true;
      
  if (LEDSTATE) {
    coap.sendResponse(ip, port, packet.messageid, "1");
  } else { 
    coap.sendResponse(ip, port, packet.messageid, "0");
  }
}

// CoAP client response callback
void callback_response(CoapPacket &packet, ObirIPAddress ip, int port) {
  Serial.println("[Coap Response got]");
  
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  
  Serial.println(p);
}

void setup() {
  Serial.begin(9600);

  ObirEthernet.begin(MAC);
  Serial.print("My IP address: ");
  Serial.print(ObirEthernet.localIP());
  Serial.println();

  LEDSTATE = true;
  
  // add server url endpoints.
  // can add multiple endpoint urls.
  // exp) coap.server(callback_switch, "switch");
  //      coap.server(callback_env, "env/temp");
  //      coap.server(callback_env, "env/humidity");
  Serial.println("Setup Callback Light");
  coap.server(callback_light, "light");
  coap.server(callback_well_know, ".well-known");

  // client response callback.
  // this endpoint is single callback.
  Serial.println("Setup Response Callback");
  coap.response(callback_response);

  // start coap server/client
  coap.start();
}

void loop() {
  // send GET or PUT coap request to CoAP server.
  // To test, use libcoap, microcoap server...etc
  // int msgid = coap.put(IPAddress(10, 0, 0, 1), 5683, "light", "1");
  Serial.println("Send Request");
  int msgid = coap.put(ObirIPAddress(10, 0, 0, 1), 5683, "light", "1");
  msgid = coap.put(ObirIPAddress(10, 0, 0, 1), 5683, ".well-known", "light");
  delay(1000);
  coap.loop();
}
