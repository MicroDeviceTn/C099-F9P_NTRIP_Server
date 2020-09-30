/* WiFi Example
 * Copyright (c) 2016 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mbed.h"
#include <stdio.h>
#include <cstring>
#include <fstream>

#define MBED_CONF_APP_WIFI_SSID "wifiap"
#define MBED_CONF_APP_WIFI_PASSWORD "wifipass"

#define USE_RTK2GO

int NBR_CHAR_SENT;
int SEND_DATA_TRIES;
int REQUEST_RTK2GO_TRIES;

#ifdef USE_RTK2GO
    #define SERVER_ADD "69.75.31.235"
#else
    #define SERVER_ADD "192.168.0.2" 
#endif

#define SERVER_PORT 2101
#define RTK2GO_PASSWORD "password"
#define RTK2GO_MOUNT_POINT "mountpoint"
    
#define BUFFER_SIZE 1024*2
#define RX_TIMEOUT_MS 450

typedef enum 
    {
        SM_STATE_WIFI_CNX,
        SM_STATE_SOCKET_CNX,
        SM_STATE_AUTHENTIFICATION,
        SM_STATE_PRE_CONNECTED,
        SM_STATE_CONNECTED,
    } SM_STATE_TYPEDEF;

WiFiInterface *wifi;

TCPSocket socket;

RawSerial UART1(PA_9, PA_10, 230400);

DigitalOut LedRed(LED_RED); // red
DigitalOut LedGreen(LED_GREEN); // green
DigitalOut LedBlue(LED_BLUE); // blue

DigitalOut R_LedRed(LED_RED);


char buf[BUFFER_SIZE];
unsigned int bufIndex=0;;
bool RxTriggred=false;
SM_STATE_TYPEDEF state;

uint32_t TotalRxCounter=0;
uint32_t TotalTxCounter=0;

// Read data from uart1
//Thread DataRxHandler;
void DataRx()
{
   
   while(UART1.readable())
   {
        const char x=UART1.getc();
        if (state==SM_STATE_CONNECTED)
        {
            buf[bufIndex]=x;
            if (bufIndex<BUFFER_SIZE)
                {
                    bufIndex++;
                    TotalRxCounter++;
                }
            else
            {
                bufIndex=0;
                bufIndex++;
                }
        }
        else
            bufIndex=0;
        
        RxTriggred=true;
    }
}

bool Sending=false;

// Send data to server
Thread DataTxHandler;
void DataTx()
{
   while(1)
    {
        if(Sending)
        {
            RxTriggred=false;
            wait_ms(RX_TIMEOUT_MS);  
            if ((RxTriggred==false)&&(bufIndex>0))
            {
                NBR_CHAR_SENT=socket.send(buf, bufIndex);
                if(NBR_CHAR_SENT<0){
                    SEND_DATA_TRIES++;
                }
                else{
                    TotalTxCounter=TotalTxCounter+bufIndex;
                    SEND_DATA_TRIES=0;
                }

                
                memset(&buf,0,BUFFER_SIZE);
                bufIndex=0;
                if(SEND_DATA_TRIES>0)
                    printf("Error sending to server = %d \n",SEND_DATA_TRIES);
                if(SEND_DATA_TRIES>=5)
                {
                    socket.close();
                    if(wifi->get_connection_status()!=1){
                        wifi->disconnect();
                        state=SM_STATE_WIFI_CNX;
                        printf("\nRe");
                        }
                    else
                    {
                        state=SM_STATE_SOCKET_CNX;
                        printf("\nRe-");
                    }
                    SEND_DATA_TRIES=0;
                    UART1.attach(0);
                    Sending=false;                   
                }
            }
        }
    }
    
}



typedef enum 
    {
        CLWHITE,
        CLYELLOW,
        CLCYAN,
        CLBLUE,
        CLGREEN,
        CLRED   
    } COLOR_TYPEDEF;

void LED_SetColor(COLOR_TYPEDEF Color)
{
    switch (Color)
    {
        case CLGREEN : LedRed=1;LedGreen=0;LedBlue=1; break;
        case CLCYAN :LedRed=1;LedGreen=0;LedBlue=0; break;
        case CLBLUE : LedRed=1;LedGreen=1;LedBlue=0; break;
        case CLWHITE : {
            DigitalOut W_lEDRed(LED_RED,0);
            DigitalOut W_LedGreen(LED_GREEN,0);
            DigitalOut W_LedBlue(LED_BLUE,0);
        }
        break;
        case CLYELLOW : {
            DigitalOut Y_lEDRed(LED_RED,0);
            DigitalOut Y_LedGreen(LED_GREEN,0);
            DigitalOut Y_LedBlue(LED_BLUE,1);
        }
        break;
        case CLRED : {
            DigitalOut R_LedRed(LED_RED,0);
            DigitalOut R_LedGreen(LED_GREEN,1);
            DigitalOut R_LedBlue(LED_BLUE,1);
        } 
         break;
        
        default : LedRed=1;LedGreen=1;LedBlue=1; break;
     }
}
bool exitLoop=false;
int main()
{
    
#ifdef MBED_MAJOR_VERSION
    printf("Mbed OS version %d.%d.%d\n", MBED_MAJOR_VERSION, MBED_MINOR_VERSION, MBED_PATCH_VERSION);
#endif

    LED_SetColor(CLWHITE);
    wait(2);
      
    printf("\n\n===================  C099-F9P & RTK2GO example  ===================\n\n");
    state=SM_STATE_WIFI_CNX;
    NBR_CHAR_SENT=0;
    SEND_DATA_TRIES=0;
    REQUEST_RTK2GO_TRIES=0;
    
    while (!exitLoop){
        
        switch(state){
            case SM_STATE_WIFI_CNX:
                {
                    printf("\n>SM_STATE:SM_STATE_WIFI_CNX\n");
                    LED_SetColor(CLCYAN);
                    printf("connecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
                    wifi = WiFiInterface::get_default_instance();
                    int ret=1;
                    ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
                    if (ret != 0) {
                        printf("\nConnection error: %d\n", ret);
                    }
                    else {
                        printf("Success\n\n");
                        printf("MAC: %s\n", wifi->get_mac_address());
                        printf("IP: %s\n", wifi->get_ip_address());
                        printf("Netmask: %s\n", wifi->get_netmask());
                        printf("Gateway: %s\n", wifi->get_gateway());
                        printf("RSSI: %d\n\n", wifi->get_rssi());
                        state=SM_STATE_SOCKET_CNX;
                    }
                }
            break;
            
            case SM_STATE_SOCKET_CNX:
                {
                    printf("\n>SM_STATE:SM_STATE_SOCKET_CNX\n");
                    LED_SetColor(CLBLUE);
                    printf("opening socket\n");
                    if(wifi->get_connection_status()==1){
                        int result =socket.open(wifi);
                        if (result != 0) printf("Error! socket.open() returned: %d\n", result);
                        else{
                            printf("Connecting to socket\n");
                            nsapi_error_t err=NSAPI_ERROR_NO_CONNECTION;
                            err=socket.connect(SERVER_ADD, SERVER_PORT);
                            if (err != NSAPI_ERROR_OK){
                                printf("Unable to connect to (%s) on port (%d) err (%d)\n", SERVER_ADD, SERVER_PORT,err);
                                socket.close();
                                wait(2);
                            }
                                else
                                {
                                    printf("Connected to Server at %s\n",SERVER_ADD);
                                    #ifdef USE_RTK2GO
                                        state=SM_STATE_AUTHENTIFICATION;
                                    #else
                                        state=SM_STATE_PRE_CONNECTED;
                                    #endif
                                }
                        }          
                    }
                    else state=SM_STATE_WIFI_CNX;
                }
            break;
                    
            case SM_STATE_AUTHENTIFICATION:
                {
                    char buffer[256];
                    printf("\n>SM_STATE:SM_STATE_AUTHENTIFICATION\n");
                    LED_SetColor(CLBLUE);
                    const string s= string("SOURCE ") +string(RTK2GO_PASSWORD)+string(" ")+string(RTK2GO_MOUNT_POINT)+string("\r\nSource-Agent: NtripServerCMD/1.1\r\n\r\n");
                    char dataToSend[s.size()+1];
                    s.copy(dataToSend, s.size()+1);
                    dataToSend[s.size()]='\0';
                    printf("Request confirmation from server\n");
                    socket.send(dataToSend, sizeof(dataToSend) - 1);
                    wait(3);
                    int b = socket.recv(buffer, 256);
                    buffer[b] = '\0';
                    if (strstr(buffer,"ICY 200 OK")!=NULL)
                        {
                            printf("Response from server: '%s'\n", buffer);
                            state=SM_STATE_PRE_CONNECTED;
                        }
                    else
                    {
                        printf("Can't get the confirmation from server\n");
                        REQUEST_RTK2GO_TRIES++;
                        if(REQUEST_RTK2GO_TRIES>0)
                        {
                            if(REQUEST_RTK2GO_TRIES>=3)
                            {
                                printf("\nError getting confirmation from server\n");
                                wait(2);
                                exitLoop=true;
                            }
                            else 
                            {
                                printf("\nRetry ");
                                wait(2);
                            }
                        }
                    }                       
                }
            break;
            
            case SM_STATE_PRE_CONNECTED : // active read and write threads
                {
                    printf("\n>SM_STATE:SM_STATE_PRE_CONNECTED\n");
                    printf("Activating read && send data ...\n");
                    LED_SetColor(CLYELLOW);
                    UART1.attach(&DataRx);
                    Sending=true;
                    if(DataTxHandler.get_state()==16) DataTxHandler.start(&DataTx);
                    state=SM_STATE_CONNECTED;
                    wait(1);

                }
            break;
            
            case SM_STATE_CONNECTED:
                {
                    printf("\n>SM_STATE:SM_STATE_CONNECTED TTX[%d] TRX[%d]\n\n",TotalTxCounter,TotalRxCounter);
                    LED_SetColor(CLGREEN);
                    if(wifi->get_connection_status()!=1){
                        printf("\nwifi disconnected \n");
                        socket.close();
                        wifi->disconnect();
                        UART1.attach(0);
                        Sending=false;
                        state=SM_STATE_WIFI_CNX;
                    } 
                }
            break;
            
        }
        
        wait(1);        
    }
 
    LED_SetColor(CLRED);
    printf("Error !  Verify the server state then reset your device \n");
    
    while(1)
    {
        R_LedRed= !R_LedRed;
        wait_ms(300);
    }
}
