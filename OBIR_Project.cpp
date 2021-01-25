#include <ObirDhcp.h>
#include <ObirEthernet.h>
#include <ObirEthernetUdp.h>
#include <OBIR_coap_server.h>
#include <SPI.h>

#define INT_MAX 100
#define VERT_MAX 10
#define EDGE_MAX 10

byte MAC[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
coapServer coap;
//int VERT_MAX = 10;
//int EDGE_MAX = 10;
int adj_matrix[10][10]; //macierz sasiedztwa
int count = 0;          //licznik wierzcholkow
int edgesNum = 0;
int tmp = 0;
int vertices[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1}; //tablica wierzcholkow
char edges[40] = {0};                                        //tablica krawedzi
char centVert[10] = {0};                                     //tablica wierz. cent.
int path[10];
//int INT_MAX = 100;
unsigned int sendPackets = 0;
unsigned int putNum = 0;
unsigned int prevPutNum = 0;
unsigned int getNum = 0;

int arrayLen(int number)
{
    uint8_t len = 0;
    while (number >= 10)
    {
        number /= 10;
        len++;
    }
    len++;
    return len;
}

void makePayload(char *payload, int number, int len)
{
    while (len > 0)
    {
        payload[len - 1] = (number % 10) + 48;
        number /= 10;
        len--;
    }
}

// czy pierwszy wierz. jest juz na liscie
bool checkVerA(int a)
{
    bool flagA = false;

    for (int i = 0; i < VERT_MAX; i++)
    {
        if (vertices[i] == a)
            flagA = true;
    }
    return flagA;
}
// czy drugi wierz. jest juz na liscie
bool checkVerB(int b)
{
    bool flagB = false;

    for (int i = 0; i < VERT_MAX; i++)
    {
        if (vertices[i] == b)
            flagB = true;
    }
    return flagB;
}
//sortowanie wierzcholkow w tablicy od najmniejszego
void sort(int arr[])
{
    {
        for (int i = 0; i < count - 1; i++)

            for (int j = 0; j < count - i - 1; j++)
                if (arr[j] > arr[j + 1])
                {

                    int tmp = arr[j];
                    arr[j] = arr[j + 1];
                    arr[j + 1] = tmp;
                }
    }
}
//funkcja do dodawania nowych krawedzi grafu
bool addEdge(int a, int b)
{
    //sprawdzenie czy graf zachowa spojnosc
    if (count != 0 && checkVerA(a) == false && checkVerB(b) == false)
    {
        return false;
    }
    else
    {

        if (checkVerA(a) == false)
        {
            vertices[count] = a;
            count++;
        }
        if (checkVerB(b) == false)
        {
            vertices[count] = b;
            count++;
        }
        sort(vertices);
        if (adj_matrix[a][b] != 1)
        {
            adj_matrix[a][b] = 1; //uzupelnienie macierzy sasiedztwa
            adj_matrix[b][a] = 1;
            char ap = a + 48;
            char bp = b + 48;
            if (edges[0] != '\0')
            {
                edges[tmp] = {44};
                tmp++;
            }
            edges[tmp] = {ap};
            tmp++;
            edges[tmp] = {45};
            tmp++;
            edges[tmp] = {bp};
            tmp++;

            edgesNum++;
            return true;
        }
        return false;
    }
}

int edgeSum(int dist[])
{
    int sum = 0;
    for (int i = 0; i < VERT_MAX; i++)
    {
        if (dist[i] != INT_MAX)
        {
            sum = sum + dist[i];
        }
    }
    return sum;
}
int minDistance(int dist[], bool sptSet[])
{
    // Initialize min value
    int min = INT_MAX, min_index = -1;

    for (int v = 0; v < VERT_MAX; v++)
        if (sptSet[v] == false && dist[v] <= min)
        {
            min = dist[v];
            min_index = v;
        }

    return min_index;
}

int dijkstra(int src)
{
    int dist[VERT_MAX] = {0}; // The output array.  dist[i] will hold the shortest
    // distance from src to i

    bool sptSet[VERT_MAX] = {0}; // sptSet[i] will be true if vertex i is included in shortest
    // path tree or shortest distance from src to i is finalized

    // Initialize all distances as INFINITE and stpSet[] as false
    for (int i = 0; i < VERT_MAX; i++)
    {
        dist[i] = INT_MAX;
        sptSet[i] = false;
    }

    // Distance of source vertex from itself is always 0
    dist[src] = 0;

    // Find shortest path for all vertices
    for (int i = 0; i < VERT_MAX - 1; i++)
    {
        // Pick the minimum distance vertex from the set of vertices not
        // yet processed. u is always equal to src in the first iteration.
        int u = minDistance(dist, sptSet);

        // Mark the picked vertex as processed
        sptSet[u] = true;

        // Update dist value of the adjacent vertices of the picked vertex.
        for (int v = 0; v < VERT_MAX; v++)

            // Update dist[v] only if is not in sptSet, there is an edge from
            // u to v, and total weight of path from src to  v through u is
            // smaller than current value of dist[v]
            if (adj_matrix[u][v] > 0 && sptSet[v] == false && dist[u] != INT_MAX && (dist[u] + adj_matrix[u][v]) < dist[v])
                dist[v] = dist[u] + adj_matrix[u][v];
    }

    return edgeSum(dist);
}
void centralVert()
{
    if (count != 0)
    {
        for (int i = 0; i < count; i++)
        {
            //path[i] = dijkstra(i);
            path[i] = dijkstra(vertices[i]);
        }
        int tmp2 = 0;
        int centralVert[VERT_MAX] = {0};
        int ile = 0;
        int len = INT_MAX;
        for (int i = 0; i < count; i++)
        {
            if (path[i] < len && path[i] != 0)
            {
                len = path[i];
                centralVert[0] = vertices[i];
                ile = 1;
            }
            else if (path[i] == len && path[i] != 0)
            {
                centralVert[ile] = vertices[i];
                ile++;
            }
        }
        for (int i = 0; i < ile * 2; i += 2)
        {
            centVert[i] = centralVert[tmp2] + 48;
            tmp2++;
            centVert[i + 1] = ',';
        }
        centVert[ile * 2 - 1] = '\0';
    }
}

int parsePacket(uint8_t *payload, int payloadLen)
{
    int x, y, z;
    x = *payload - 48;
    payload++;
    z = *payload;
    payload++;
    y = *payload - 48;
    payload++;
    //czyszczenie bufora TRZEBA ZROBIC CHOCIAZ WCALE NIE TRZEBA
    //czyli doopanowac payload
    if ((x >= 0 && x <= 9) && (y >= 0 && y <= 9) && z == ',' && *payload == 0 && edgesNum < EDGE_MAX && x != y)
    {
        if (addEdge(x, y))
            return 0;
        else
        {
            return 2;
        }
    }
    else if (edgesNum >= EDGE_MAX)
    {
        return 1;
    }
    else
        return 2;
}


void callback_center(coapPacket *packet, ObirIPAddress ip, int port, int obs, uint8_t accept)
{
    Serial.println("Central Vertices");
    if (packet->code == COAP_GET && count == 0)
    {
        getNum++;
        sendPackets++;
        coap.sendResponse(ip, port, 132, COAP_TEXT_PLAIN, "", (uint8_t)0);
    }
    else if (packet->code == COAP_GET && count != 0)
    {
        getNum++;
        centralVert();
        sendPackets++;
        if(accept == 97 || accept == 100){
            coap.sendResponse(ip, port,  COAP_TEXT_PLAIN, centVert, (uint8_t)strlen(centVert));
        }else if (accept == 40){
            int len = ((int)strlen(centVert))+4;
            char payload[len]={0};
            payload[0] = '<';
            payload[1] = '/';
            for(int i=0; i < strlen(centVert); i++){
                payload[i+2] = centVert[i];
            }
            payload[len-2] = '>';
            payload[len-1] = ';';
            coap.sendResponse(ip, port, COAP_APPLICATION_LINK_FORMAT, payload, (uint8_t)len);
        }
    }
}
//CoAP server Edges endpoint
void callback_edges(coapPacket *packet, ObirIPAddress ip, int port, int obs, uint8_t accept)
{
    Serial.println("Edges");

    if (packet->code == COAP_PUT)
    {
        putNum++;
        if (parsePacket(packet->payload, packet->payloadlen) == 0)
        {
            sendPackets++;
            coap.sendResponse(ip, port, 65, COAP_TEXT_PLAIN, "", (uint8_t)0);
        }
        else if (parsePacket(packet->payload, packet->payloadlen) == 1)
        {
            sendPackets++;
            coap.sendResponse(ip, port, 160, COAP_TEXT_PLAIN, "", (uint8_t)0);
        }
        else if (parsePacket(packet->payload, packet->payloadlen) == 2)

        {
            //ewntualnie wporwadzic kod bledu
            sendPackets++;
            coap.sendResponse(ip, port, 133, COAP_TEXT_PLAIN, "", (uint8_t)0);

            //coap.sendResponse()
        }
    }
    else if (packet->code == COAP_GET)
    {
        getNum++;
        if (count != 0)
        {
            sendPackets++;
            if (accept == 50)
            {
                int len = ((int)strlen(edges)) + 12;
                char payload[len] = {0};
                char tmp[] = "{\"Edges\":\"";
                for (int i = 0; i < strlen(tmp); i++)
                {
                    payload[i] = tmp[i];
                }
                for (int i = 0; i < strlen(edges); i++)
                {
                    payload[i + 10] = edges[i];
                }
                payload[len - 2] = '"';
                payload[len - 1] = '}';

                coap.sendResponse(ip, port, COAP_APPLICATION_JSON, payload, (uint8_t)len);
            }
            else
            {
                coap.sendResponse(ip, port, COAP_TEXT_PLAIN, edges, (uint8_t)strlen(edges));
            }
        }

        else
        {
            sendPackets++;
            coap.sendResponse(ip, port, 132, COAP_TEXT_PLAIN, "", (uint8_t)0);
        }
    }
}

//CoAP server Sendpackets endpoint
void callback_sendPackets(coapPacket *packet, ObirIPAddress ip, int port, int obs, uint8_t accept)
{
    if (packet->code == COAP_GET)
    {
        Serial.println("Sendpackets endpoint");
        sendPackets++;
        getNum++;
        uint8_t len = arrayLen(sendPackets);
        char payload[len];
        makePayload(payload, sendPackets, len);
        coap.sendResponse(ip, port, COAP_TEXT_PLAIN, payload, (uint8_t)len);
    }
}

void callback_PutNumber(coapPacket *packet, ObirIPAddress ip, int port, int obs, uint8_t accept)
{
    if (packet->code == COAP_GET)
    {
        getNum++;
        Serial.println("PutNumber");
        uint8_t len = arrayLen(putNum);
        char payload[len];
        makePayload(payload, putNum, len);
        sendPackets++;
        coap.sendResponse(ip, port, COAP_TEXT_PLAIN, payload, len);
    }
}
void callback_GetNumber(coapPacket *packet, ObirIPAddress ip, int port, int obs, uint8_t accept)
{
    if (packet->code == COAP_GET)
    {
        getNum++;
        Serial.println("GetNumber");
        uint8_t len = arrayLen(getNum);
        char payload[len];
        makePayload(payload, getNum, len);
        sendPackets++;
        coap.sendResponse(ip, port, COAP_TEXT_PLAIN, payload, (uint8_t)len);
    }
}

void setup()
{
    Serial.begin(9600);

    ObirEthernet.begin(MAC);
    Serial.print("My IP address: ");
    Serial.print(ObirEthernet.localIP());
    Serial.println();
    coap.server(callback_center, "CentralVertices");
    coap.server(callback_edges, "Edges");
    coap.server(callback_sendPackets, "SendPackets");
    coap.server(callback_PutNumber, "PutNumber");
    coap.server(callback_GetNumber, "GetNumber");
    coap.start();
}

void loop()
{
    if (prevPutNum != putNum)
    {
        uint8_t len = arrayLen(putNum);
        char payload[len + 1];
        payload[len] = '\0';
        makePayload(payload, putNum, len);
        coap.notification(payload, "PutNumber");
        prevPutNum = putNum;
    }

    coap.loop();
}