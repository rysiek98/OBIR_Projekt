#include <ObirDhcp.h>           
#include <ObirEthernet.h>       
#include <ObirEthernetUdp.h>
#include <coap-simple.h>


byte MAC[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };
int vertex[100] = {0};
int vertexLength = 0;
int addedVertex = 0; 

// CoAP client response callback
void callback_response(CoapPacket &packet, ObirIPAddress ip, int port);

// ObirEthernetUDP and CoAP class
ObirEthernetUDP Udp;
Coap coap(Udp);

//CoAP server Core endpoint
void callback_core(CoapPacket &packet, ObirIPAddress ip, int port) {
  Serial.println("core");
  char payload[] = "</graph>";
  int payloadLen = strlen(payload);
  CoapOption *options = packet.options;
  coap.sendResponse(ip, port, packet.messageid, payload, payloadLen, COAP_CONTENT, COAP_APPLICATION_LINK_FORMAT, packet.token, packet.tokenlen, packet.optionnum, options);
}

void graphParser(char payload[], int *a){
  
  char tmp[4];
  char tmp2[4];
  bool flag = true;
  int i = 1;
  int j = 0;

  while(i < strlen(payload)){
    if(payload[i] != ','){ 
      if(flag){
        tmp[j] = payload[i];
        j++;
      }else{
         tmp2[j] = payload[i];
         j++;
        }
    }else{
      flag = false;
      tmp[j] = '\0';
      j = 0;
     }
    if(payload[i+1] == ')'){
      i = strlen(payload);
      tmp2[j] = '\0';
    }
    i++;     
  }
  *a = atoi(tmp);
  a++;
  *a = atoi(tmp2);
}

void addVertex(int newVertex){

  int place = 0;
  int tmp = newVertex;
  int tmp2 = 0;
  bool flag = true;
  
  if(vertex[0] < newVertex){
    
    for(int i=0; i<99; i++){
      if(vertex[i] == newVertex){
        flag = false;
        break;
      }
            
      if(newVertex > vertex[i] and newVertex < vertex[i+1]){
        place = i+1;
        break;
       }
        
      if(newVertex > vertex[i] and vertex[i+1] == 0){
         place = i+1;
         break;
      }
   }  
  }else{
    place = 0;
  }

  if(flag){
    if(newVertex/10 < 1){
      vertexLength++;
      }
     else if(newVertex/10 > 1 && newVertex/10 < 10){
      vertexLength+=2;
     }else{
      vertexLength+=3;
      }
    addedVertex++;   
    tmp2 = tmp;
    for(int i=place; i<100; i++){
      tmp2 = vertex[i];
      vertex[i] = tmp;
      tmp = tmp2; 
    }
  } 
}

void putNodeToGraph(CoapPacket &packet, ObirIPAddress ip, int port){
  Serial.println("Graph");
  int tmp[2] ={0};
  graphParser(packet.payload, &tmp[0]);
  addVertex(tmp[0]);
  addVertex(tmp[1]);
  char payload[] = "Vertex added!";
  int payloadLen = strlen(payload);
  Serial.println("------");
  for(int i=0; i<15; i++){  
  Serial.println(vertex[i]);
  }
  Serial.println("------");
  CoapOption *options = packet.options;
  coap.sendResponse(ip, port, packet.messageid, payload, payloadLen, COAP_CONTENT, COAP_TEXT_PLAIN, packet.token, packet.tokenlen, packet.optionnum, options);
}

void getGraphNode(CoapPacket &packet, ObirIPAddress ip, int port){
  Serial.println("GET Graph");
  char tmp[3]; 
  int  j=1;
  char tmpPayload[2*vertexLength+2] = {0};
  tmpPayload[0]= '(';
  for(int i=1; i<=addedVertex; i++){
    sprintf(tmp, "%d", vertex[i]);
    for(int k=0; k<strlen(tmp); k++){
      tmpPayload[j] = tmp[k];
      Serial.println(tmp[k]);
      j++;
    }
    tmpPayload[j] = ',';
    j++;
  }   
  tmpPayload[j-1] = ')';
  tmpPayload[j] = '\0';
  int payloadLen = strlen(tmpPayload);
  char payload[payloadLen];
  memcpy(payload, tmpPayload, payloadLen);
  CoapOption *options = packet.options;
  coap.sendResponse(ip, port, packet.messageid, payload, payloadLen, COAP_CONTENT, COAP_TEXT_PLAIN, packet.token, packet.tokenlen, packet.optionnum, options);
}


// CoAP server Graph endpoint URL
void callback_graph(CoapPacket &packet, ObirIPAddress ip, int port) {
  if(packet.code == 3){
   putNodeToGraph(packet, ip, port);
  }else{
    getGraphNode(packet, ip,port);
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
  
  // add server url endpoints.
  // can add multiple endpoint urls.
  // exp) coap.server(callback_switch, "switch");
  //      coap.server(callback_env, "env/temp");
  //      coap.server(callback_env, "env/humidity");
  Serial.println("Setup Callback");
  coap.server(callback_core, ".well-known/core");
  coap.server(callback_graph, "graph");

  // client response callback.
  // this endpoint is single callback.
  Serial.println("Setup Response Callback");
  coap.response(callback_response);

  // start coap server/client
  coap.start();
}

void loop() {
  coap.loop();
}
