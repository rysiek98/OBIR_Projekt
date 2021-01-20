#include <Arduino.h>
#include <ObirDhcp.h>
#include <ObirEthernet.h>
#include <ObirEthernetUdp.h>
#include <OBIR_coap_server.h>
#include <SPI.h>

byte MAC[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
coapServer coap;
int vertex[20] = {0};
int edge[20] = {0};
int vertexLength = 0;
int edgeLength = 0;
int addedVertex = 0;
int addedEdge = 0;
int test = 0;
bool flag = true;

//CoAP server Text endpoint
void callback_text(coapPacket *packet, ObirIPAddress ip, int port, int obs)
{
    test++;
    if (obs == 0)
    {
        Serial.println("Hello World!");
        coap.sendResponse(ip, port, COAP_TEXT_PLAIN, "Hello World!", (uint8_t)strlen("Hello World!"));
    }
    else
    {
        unsigned long time = millis();
        Serial.print("Time: ");
        Serial.println(time);
        Serial.print("preMilis: ");
        Serial.println(coap.getPreviousMillis());
        coap.sendResponse(ip, port, COAP_TEXT_PLAIN, "Hello World!", (uint8_t)strlen("Hello World!"));
    }
}

void graphParser(char payload[], int *a)
{

    char tmp[4];
    char tmp2[4];
    bool flag = true;
    int i = 1;
    int j = 0;

    while (i < strlen(payload))
    {
        if (payload[i] != ',')
        {
            if (flag)
            {
                tmp[j] = payload[i];
                j++;
            }
            else
            {
                tmp2[j] = payload[i];
                j++;
            }
        }
        else
        {
            flag = false;
            tmp[j] = '\0';
            j = 0;
        }
        if (payload[i + 1] == ')')
        {
            i = strlen(payload);
            tmp2[j] = '\0';
        }
        i++;
    }
    *a = atoi(tmp);
    a++;
    *a = atoi(tmp2);
}

void addVertex(int newVertex)
{

    int place = 0;
    int tmp = newVertex;
    int tmp2 = 0;
    bool flag = true;

    if (addedVertex == 0)
    {
        place = 0;
    }
    else if (vertex[0] < newVertex)
    {

        for (int i = 0; i <= addedVertex; i++)
        {
            if (vertex[i] == newVertex)
            {
                flag = false;
                break;
            }

            if (newVertex > vertex[i] and newVertex < vertex[i + 1])
            {
                place = i + 1;
                break;
            }

            if (newVertex > vertex[i] and vertex[i + 1] == 0)
            {
                place = i + 1;
                break;
            }
        }
    }
    else
    {
        place = 0;
    }

    if (flag)
    {
        if (newVertex / 10 < 1)
        {
            vertexLength++;
        }
        else if (newVertex / 10 > 1 && newVertex / 10 < 10)
        {
            vertexLength += 2;
        }
        else
        {
            vertexLength += 3;
        }
        addedVertex++;
        tmp2 = tmp;
        for (int i = place; i <= addedVertex; i++)
        {
            tmp2 = vertex[i];
            vertex[i] = tmp;
            tmp = tmp2;
        }
    }
}

void calculateEdgeLength(int newEdge[2])
{
    for (int i = 0; i < 2; i++)
    {
        if (newEdge[i] / 10 < 1)
        {
            edgeLength++;
        }
        else if (newEdge[i] / 10 > 1 && newEdge[i] / 10 < 10)
        {
            edgeLength += 2;
        }
        else
        {
            edgeLength += 3;
        }
    }
}

void addEdge(int newEdge[2])
{
    bool flag = true;
    if (addedEdge == 0)
    {
        edge[0] = newEdge[0];
        edge[1] = newEdge[1];
        calculateEdgeLength(newEdge);
        addedEdge += 2;
    }
    else
    {
        for (int i = 0; i < addedEdge; i++)
        {
            if (edge[i] == newEdge[0])
            {
                if (edge[i + 1] == newEdge[1])
                {
                    flag = false;
                    break;
                }
            }
        }

        if (flag)
        {
            edge[addedEdge] = newEdge[0];
            edge[addedEdge + 1] = newEdge[1];
            addedEdge += 2;
            calculateEdgeLength(newEdge);
        }
    }
}

void putNodeToGraph(coapPacket *packet, ObirIPAddress ip, int port)
{
    Serial.println("Graph PUT Node");
    int tmp[2] = {0};
    graphParser(packet->payload, &tmp[0]);
    if (addedVertex < 20)
    {
        addVertex(tmp[0]);
        addVertex(tmp[1]);
        addEdge(tmp);
        coap.sendResponse(ip, port, COAP_TEXT_PLAIN, "Vertex added!", (uint8_t)strlen("Vertex added!"));
    }
    else
    {
        coap.sendResponse(ip, port, COAP_TEXT_PLAIN, "Failed vertex add! Storage is full.", (uint8_t)strlen("Failed vertex add! Storage is full."));
    }

    Serial.println("------");
    Serial.println("Vertex");
    Serial.print("(");
    for (int i = 0; i < addedVertex; i++)
    {
        Serial.print(vertex[i]);
        if (i + 1 != addedVertex)
        {
            Serial.print(",");
        }
    }
    Serial.println(")");
    Serial.println("------");
    Serial.println("Edge");
    Serial.print("(");
    for (int i = 0; i < addedEdge; i += 2)
    {
        Serial.print(edge[i]);
        Serial.print("-");
        Serial.print(edge[i + 1]);
        if (i + 2 != addedEdge)
        {
            Serial.print(",");
        }
    }
    Serial.println(")");
    Serial.println("------");
}

// void getGraphVertex(coapPacket *packet, ObirIPAddress ip, int port)
// {
//     Serial.println("GET Graph vertex");
//     char tmp[3];
//     int j = 1;
//     char payload[2 * vertexLength + 2] = {0};
//     payload[0] = '(';
//     for (int i = 0; i < addedVertex; i++)
//     {
//         sprintf(tmp, "%d", vertex[i]);
//         for (int k = 0; k < strlen(tmp); k++)
//         {
//             payload[j] = tmp[k];
//             j++;
//         }
//         payload[j] = ',';
//         j++;
//     }
//     payload[j - 1] = ')';
//     payload[j] = '\0';
//     coap.sendResponse(ip, port, COAP_TEXT_PLAIN, payload, (uint8_t)strlen(payload));
// }

void getGraphEdge(coapPacket *packet, ObirIPAddress ip, int port)
{
    Serial.println("GET Graph edge");
    if (edgeLength > 0)
    {
        char tmp[3] = {0};
        int j = 1;
        char *payload = new char[(2 * edgeLength) + 2];
        payload[0] = '(';
        for (int i = 0; i < addedEdge; i += 2)
        {
            sprintf(tmp, "%d", edge[i]);
            for (int k = 0; k < strlen(tmp); k++)
            {
                payload[j] = tmp[k];
                j++;
            }
            payload[j] = '-';
            j++;
            tmp[0] = 0;
            tmp[1] = 0;
            tmp[2] = 0;
            sprintf(tmp, "%d", edge[i + 1]);
            for (int k = 0; k < strlen(tmp); k++)
            {
                payload[j] = tmp[k];
                j++;
            }
            payload[j] = ',';
            j++;
        }
        payload[j - 1] = ')';
        payload[j] = '\0';
        coap.sendResponse(ip, port, COAP_TEXT_PLAIN, payload, (uint8_t)strlen(payload));
        delete[] payload;
    }
    else
    {
        coap.sendResponse(ip, port, COAP_TEXT_PLAIN, "Database is empty!", (uint8_t)strlen("Database is empty!"));
    }
}
// CoAP server Graph endpoint URL
void callback_graph(coapPacket *packet, ObirIPAddress ip, int port, int obs)
{
    if (packet->code == 3)
    {
        putNodeToGraph(packet, ip, port);
    }
    else
    {
        getGraphEdge(packet, ip, port);
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
    coap.server(callback_graph, "graph");
    coap.start();
}

void loop()
{
    coap.loop();
    // if (test == 3 && flag)
    // {
    //     Serial.println("testtt");
    //     coap.notification("TEST!", "text");
    //     flag = false;
    // }
}
