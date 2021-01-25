#include <ObirDhcp.h>
#include <ObirEthernet.h>
#include <ObirEthernetUdp.h>
#include <OBIR_coap_server.h>
#include <SPI.h>

#define INT_MAX 100 //nieskonczonosc dla fukcji liczacej najkrotsze sciezki
#define VERT_MAX 10 //maksymalna liczba wierzcholkow
#define EDGE_MAX 10 //maksymalna liczba krawedzi

byte MAC[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
coapServer coap;
int adj_matrix[VERT_MAX][VERT_MAX]; //macierz sasiedztwa dla funkcji liczacej najkrotsze sciezki
int count = 0;                      //licznik wierzcholkow
int edgesNum = 0;                   //liczba krawedzi
int tmp = 0;                        //zmienna pomocnicza uzywana do uzupelnienia tablicy krawedzi
int vertices[VERT_MAX] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};   //tablica wierzcholkow
char edges[EDGE_MAX*4] = {0};                                        //tablica krawedzi; do wypisywania
char centVert[VERT_MAX*2] = {0};                                     //tablica wierz. cent.; do wypisywania
int path[VERT_MAX];           //dlugosc sciezki
unsigned int putNum = 0;      //liczba zadan typu "put"
unsigned int prevPutNum = 0;  //poprzedni payload "put", uzywany przy opcji "observe"
unsigned int getNum = 0;      //liczba zadan typu "get"

//f. liczaca dlugosc liczby w znakach char
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
//f. do tworzenia payloadu
void makePayload(char *payload, int number, int len)
{
    while (len > 0)
    {
        payload[len - 1] = (number % 10) + 48;
        number /= 10;
        len--;
    }
}
//sprawdzenie czy wierzcholek znajduje sie juz w tablicy wierzcholkow
bool checkVer(int a)
{ 
    for (int i = 0; i < VERT_MAX; i++)
    {
        if (vertices[i] == a)
            return true;
    }
    return false;
}

//sortowanie wierzcholkow w tablicy od najmniejszego do najwiekszego (bubble sort)
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
    if (count != 0 && checkVer(a) == false && checkVer(b) == false)
    {
        return false;
    }
    else
    {
        //wierzcholek nie jest dodawany do tablicy w przypadku gdy juz sie tam znajduje
        if (checkVer(a) == false)
        {
            vertices[count] = a;
            count++;
        }
        if (checkVer(b) == false)
        {
            vertices[count] = b;
            count++;
        }
        sort(vertices);
        if (adj_matrix[a][b] != 1)
        {   
            adj_matrix[a][b] = 1; //uzupelnienie macierzy sasiedztwa
            adj_matrix[b][a] = 1;
            //dodanie krawedzi do tablicy
            char ap = a + 48;
            char bp = b + 48;
            if (edges[0] != '\0') 
            {
                edges[tmp] = {44}; //zmienna tmp zadeklarowana globalnie, pomaga w poprawnym uzupelnieniu tablicy krawedzi
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
//wyliczenie sumy dlugosci sciezek
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
//f. obliczajaca najkrotsze sciezki 
int minDistance(int dist[], bool inc[])
{
    int min = INT_MAX, min_index = -1;
    for (int v = 0; v < VERT_MAX; v++)
        if (inc[v] == false && dist[v] <= min)
        {
            min = dist[v];
            min_index = v;
        }

    return min_index;
}
//agorytm do obliczania najkrotszych sciezek dla wierzcholka
int dijkstra(int src)
{
    int dist[VERT_MAX]; //odleglosci do poszczegolnych wierzcholkow od wierzcholka badanego
    bool inc[VERT_MAX]; //wierzholek zawarty w najkrotszej sciezce
    for (int i = 0; i < VERT_MAX; i++)
    {
        dist[i] = INT_MAX;
        inc[i] = false;
    }
    dist[src] = 0; //odleglosc wierzcholka do samego siebie 
    for (int i = 0; i < VERT_MAX - 1; i++)
    {
        int u = minDistance(dist, inc);
        inc[u] = true;
        for (int v = 0; v < VERT_MAX; v++)
        {
          if (adj_matrix[u][v] > 0 && inc[v] == false && dist[u] != INT_MAX && (dist[u] + adj_matrix[u][v]) < dist[v])
          {
             dist[v] = dist[u] + adj_matrix[u][v];
          }
               
        }      
    }
    return edgeSum(dist);
}
//wyliczanie wszystkich wierzcholkow centralnych
void centralVert()
{
    if (count != 0)
    {
        for (int i = 0; i < count; i++)
        {
            path[i] = dijkstra(vertices[i]);//obliczanie najkrotszych sciezek dla kazdego wierzcholka
        }
        int tmp2 = 0;
        int centralVert[VERT_MAX] = {0}; //tablica zawierajaca wierzcholki centralne
        int ile = 0;                     //liczba wierzcholkow centralnych
        int len = INT_MAX;               //dlugosc sciezek
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
//f. sprawdzajaca jak ubsluzyc zadanie get dla krawedzi
int parsePacket(uint8_t *payload, int payloadLen)
{
    int x, y, z;
    x = *payload - 48;
    payload++;
    z = *payload;
    payload++;
    y = *payload - 48;
    payload++;
    if ((x >= 0 && x <= 9) && (y >= 0 && y <= 9) && z == ',' && *payload == 0 && edgesNum < EDGE_MAX && x != y)
    {
        if (addEdge(x, y))
            return 0;//mozna dodac krawedz
        else
        {
            return 2;//nie mozna dodac krawedzi
        }
    }
    else if (edgesNum >= EDGE_MAX)
    {
        return 1;//osiagnieta maksymalna liczba krawedzi
    }
    else
        return 2;//nie mozna dodac wierzcholka (blad formatowania)
}

//wywolanie wierzcholkow centralnych (get)
void callback_center(coapPacket *packet, ObirIPAddress ip, int port, int obs, uint8_t accept)
{
    if (packet->code == COAP_GET && count == 0)
    {
        getNum++;
        coap.sendResponse(ip, port, 132, COAP_TEXT_PLAIN, "", (uint8_t)0);//brak wierzcholkow centralnych
    }
    else if (packet->code == COAP_GET && count != 0)
    {
        getNum++;
        centralVert();
        if(accept == 97 || accept == 100){
          coap.sendResponse(ip, port,  COAP_TEXT_PLAIN, centVert, (uint8_t)strlen(centVert));//wierzcholki centralne
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
           coap.sendResponse(ip, port, COAP_APPLICATION_LINK_FORMAT, payload, (uint8_t)len);//wierzcholki centralne
          }
    }
}
//wywolanie krawedzi (get i put)
void callback_edges(coapPacket *packet, ObirIPAddress ip, int port, int obs, uint8_t accept)
{

    if (packet->code == COAP_PUT)
    {
        putNum++;
        if (parsePacket(packet->payload, packet->payloadlen) == 0)
        {
            coap.sendResponse(ip, port, 65, COAP_TEXT_PLAIN, "", (uint8_t)0);//poprawne utworzenie krawedzi
        }
        else if (parsePacket(packet->payload, packet->payloadlen) == 1)
        {
            coap.sendResponse(ip, port, 160, COAP_TEXT_PLAIN, "", (uint8_t)0);//blad wewnetrzny serwera - przepelnienie pamieci
        }
        else if (parsePacket(packet->payload, packet->payloadlen) == 2)

        {
            coap.sendResponse(ip, port, 133, COAP_TEXT_PLAIN, "", (uint8_t)0);//niedozwolona operacja (graf niespojny, zle formatowanie, bledne dane

        }
    }
    else if (packet->code == COAP_GET)
    {
        getNum++;
        if (count != 0)
        {
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
               
                coap.sendResponse(ip, port, COAP_APPLICATION_JSON, payload, (uint8_t)len, 1);//wypisanie krawedzi
            }
            else
            {
                coap.sendResponse(ip, port, COAP_TEXT_PLAIN, edges, (uint8_t)strlen(edges), 1);//wypisanie krawedzi
            }
        }

        else
        {
            coap.sendResponse(ip, port, 132, COAP_TEXT_PLAIN, "", (uint8_t)0);//brak krawedzi
        }
    }
}

//wywolanie liczby wszystkich odp. wyslanych przez serwer (suma get+put)
void callback_sendPackets(coapPacket *packet, ObirIPAddress ip, int port, int obs, uint8_t accept)
{
    if (packet->code == COAP_GET)
    {
        //Serial.println("Sendpackets endpoint");
        getNum++;
        uint8_t len = arrayLen(getNum + putNum);
        char payload[len];
        makePayload(payload, (getNum + putNum), len);
        coap.sendResponse(ip, port, COAP_TEXT_PLAIN, payload, (uint8_t)len);
    }
}
//wywolanie liczby odp. na zadanie typu put wyslanych przez serwer
void callback_PutNumber(coapPacket *packet, ObirIPAddress ip, int port, int obs, uint8_t accept)
{
    if (packet->code == COAP_GET)
    {
        getNum++;
        uint8_t len = arrayLen(putNum);
        char payload[len];
        makePayload(payload, putNum, len);
        coap.sendResponse(ip, port, COAP_TEXT_PLAIN, payload, len);
    }
}
//wywolanie liczby odp. na zadanie typu get wyslanych przez serwer
void callback_GetNumber(coapPacket *packet, ObirIPAddress ip, int port, int obs, uint8_t accept)
{
    if (packet->code == COAP_GET)
    {
        getNum++;
        uint8_t len = arrayLen(getNum);
        char payload[len];
        makePayload(payload, getNum, len);
        coap.sendResponse(ip, port, COAP_TEXT_PLAIN, payload, (uint8_t)len);
    }
}
//konfiguracja serwera
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
        char payload[len];
        makePayload(payload, putNum, len);
        coap.notification(payload, "PutNumber", len);
        prevPutNum = putNum;
    }

    coap.loop();
}
