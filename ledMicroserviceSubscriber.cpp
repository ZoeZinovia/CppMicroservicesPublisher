//
// Created by Shani du Plessis on 20/04/2021.
//

extern "C" {
    #include <wiringPi.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include "MQTTClient.h"
}
#include <csignal>
#include <iostream>


#define ADDRESS     "10.35.0.229:1883"
#define CLIENTID    "ledSubscriber"
#define TOPIC       "LED"
#define PAYLOAD     "Hello World!"
#define QOS         1
#define TIMEOUT     10000L

//using namespace std;

bool RUNNING = true;
int pin = 17;

volatile MQTTClient_deliveryToken deliveredtoken;

void switch_led(int pin){
    int status = digitalRead(pin);
    pinMode(pin, OUTPUT);
    if(status == 0) {
        digitalWrite(pin, HIGH);
        std::cout << "\nON!\n";
    } else{
        digitalWrite(pin, LOW);
        std::cout << "\nOFF!\n";
    }
}

void delivered(void *context, MQTTClient_deliveryToken dt)
{
    printf("Message with token value %d delivery confirmed\n", dt);
    deliveredtoken = dt;
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    int i;
    char* payloadptr;

    printf("Message arrived\n");
    printf("     topic: %s\n", topicName);
    printf("   message: ");

    payloadptr = (char*)message->payload;
    for(i=0; i<message->payloadlen; i++)
    {
        putchar(*payloadptr++);
    }
    putchar('\n');

    switch_led(pin);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void connlost(void *context, char *cause)
{
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
}

//void exit_program(int s){
//    MQTTClient_unsubscribe(client, TOPIC);
//    MQTTClient_disconnect(client, 10000);
//    MQTTClient_destroy(&client);
//    std::cout << "Program ended\n";
//}

int main(){
//    std::signal(SIGINT, exit_program);
    wiringPiSetupGpio();

    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;
    int ch;

    MQTTClient_create(&client, ADDRESS, CLIENTID,
                      MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }
    printf("Subscribing to topic %s\nfor client %s using QoS%d\n\n"
           "Press Q<Enter> to quit\n\n", TOPIC, CLIENTID, QOS);
    MQTTClient_subscribe(client, TOPIC, QOS);

    do
    {
        ch = getchar();
    } while(ch!='Q' && ch != 'q');

    MQTTClient_unsubscribe(client, TOPIC);
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return rc;
}