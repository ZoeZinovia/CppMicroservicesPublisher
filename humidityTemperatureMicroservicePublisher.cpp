////
//// Created by Zoe Zinovia on 21/04/2021.
////

extern "C" {
    #include <wiringPi.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include "MQTTClient.h"
}
#include <csignal>
#include <iostream>
#include "include/rapidjson/document.h"
#include "include/rapidjson/stringbuffer.h"
#include "include/rapidjson/prettywriter.h"
#include "include/rapidjson/writer.h"
#include <chrono>
#include <fstream>
#include <typeinfo>

// MQTT variables

#define CLIENTID    "hum_temp_client"
#define TOPIC_T       "Temperature"
#define TOPIC_H       "Humidity"
#define QOS         0
#define TIMEOUT     10000L

char* ADDRESS;

// Pi dht11 variables
#define MAXTIMINGS	85
#define DHTPIN		7

int dht11_dat[5] = { 0, 0, 0, 0, 0 }; //first 8bits is for humidity integral value, second 8bits for humidity decimal, third for temp integral, fourth for temperature decimal and last for checksum

// RapidJson variables

using namespace rapidjson;
using namespace std::chrono;

int publish_message(std::string str_message, const char *topic, MQTTClient client){
    // Initializing components for MQTT publisher
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    // Updating values of pubmsg object
    char *message = new char[str_message.length() + 1];
    strcpy(message, str_message.c_str());
    pubmsg.payload = message;
    pubmsg.payloadlen = (int)std::strlen(message);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_publishMessage(client, topic, &pubmsg, &token); // Publish the message
    int rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    return rc;
}

std::string json_to_string(const rapidjson::Document& doc){
    //Serialize JSON to string for the message
    rapidjson::StringBuffer string_buffer;
    string_buffer.Clear();
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(string_buffer);
    doc.Accept(writer);
    return std::string(string_buffer.GetString());
}

// Reading of the dht11 is rather complex in C/C++. See this site that explains how readings are made: http://www.uugear.com/portfolio/dht11-humidity-temperature-sensor-module/
int* read_dht11_dat()
{
    auto start1 = high_resolution_clock::now();
    uint8_t laststate	= HIGH;
    uint8_t counter		= 0;
    uint8_t j		= 0, i;

    dht11_dat[0] = dht11_dat[1] = dht11_dat[2] = dht11_dat[3] = dht11_dat[4] = 0;

    // pull pin down for 18 milliseconds. This is called “Start Signal” and it is to ensure DHT11 has detected the signal from MCU.
    pinMode( DHTPIN, OUTPUT );
    digitalWrite( DHTPIN, LOW );
    delay( 18 );
    // Then MCU will pull up DATA pin for 40us to wait for DHT11’s response.
    digitalWrite( DHTPIN, HIGH );
    delayMicroseconds( 40 );
    // Prepare to read the pin
    pinMode( DHTPIN, INPUT );

    // Detect change and read data
    for ( i = 0; i < MAXTIMINGS; i++ )
    {
        counter = 0;
        while ( digitalRead( DHTPIN ) == laststate )
        {
            counter++;
            delayMicroseconds( 1 );
            if ( counter == 255 )
            {
                break;
            }
        }
        laststate = digitalRead( DHTPIN );

        if ( counter == 255 )
            break;

        // Ignore first 3 transitions
        if ( (i >= 4) && (i % 2 == 0) )
        {
            // Add each bit into the storage bytes
            dht11_dat[j / 8] <<= 1;
            if ( counter > 16 )
                dht11_dat[j / 8] |= 1;
            j++;
        }
    }

    // Check that 40 bits (8bit x 5 ) were read + verify checksum in the last byte
    if ( (j >= 40) && (dht11_dat[4] == ( (dht11_dat[0] + dht11_dat[1] + dht11_dat[2] + dht11_dat[3]) & 0xFF) ) )
    {
//        auto end1 = high_resolution_clock::now();
//        std::chrono::duration<double> timer1 = end1-start1;
//        std::cout << "Humidity and temperature runtime readings = " << timer1.count() << "\n";
        return dht11_dat; // If all ok, return pointer to the data array
    } else  {
        return dht11_dat; //If there was an error, set first array element to -1 as flag to main function
    }
}

int main(int argc, char* argv[])
{
    auto start = high_resolution_clock::now(); // Starting timer

    std::string input = argv[1]; // IP address as command line argument to avoid hard coding
    input.append(":1883"); // Append MQTT port
    char char_input[input.length() + 1];
    strcpy(char_input, input.c_str());
    ADDRESS = char_input;
    double temperature = 0;
    double humidity = 0;

//    auto end2 = high_resolution_clock::now();
//    std::chrono::duration<double> timer2 = end2-start;
//    std::cout << "Humidity and temperature runtime define stuff = " << timer2.count() << "\n";

    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

//    end2 = high_resolution_clock::now();
//    timer2 = end2-start;
//    std::cout << "Humidity and temperature runtime define client stuff = " << timer2.count() << "\n";

    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

//    end2 = high_resolution_clock::now();
//    timer2 = end2-start;
//    std::cout << "Humidity and temperature runtime connect to client = " << timer2.count() << "\n";

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    } else{
        printf("Connected. Result code %d\n", rc);
    }

    wiringPiSetup(); // Required for wiringPi

//    end2 = high_resolution_clock::now();
//    timer2 = end2-start;
//    std::cout << "Humidity and temperature runtime before readings = " << timer2.count() << "\n";

    int count = 0;
    int num_iterations = 100000;
    auto dhtStart = high_resolution_clock::now();
    auto dhtEnd = high_resolution_clock::now();
    std::chrono::duration<double> dhtTimer;
    while(count <= num_iterations) {
        dhtEnd = high_resolution_clock::now();
        dhtTimer = dhtEnd - dhtStart;
        if((temperature == 0 && humidity == 0) || dhtTimer > (std::chrono::seconds(1))) { //need to get values from
            int *readings = read_dht11_dat();
            dhtStart = high_resolution_clock::now();
            int counter = 0;
            while (readings[0] == -1 && counter < 5) {
                readings = read_dht11_dat(); // Errors frequently occur when reading dht sensor. Keep reading until values are returned.
                counter = counter + 1;
            }
            if (counter == 5) {
                std::cout << "Problem with DHT11 sensor. Check Raspberry Pi \n";
                return 1;
            }
            humidity = readings[0] + (readings[1] / 10);
            temperature = readings[2] + (readings[3] / 10);
        }

//        auto end = high_resolution_clock::now();
//        std::chrono::duration<double> timer = end-start;
//        std::cout << "Humidity and temperature runtime after readings= " << timer.count() << "\n";

        if(count == num_iterations){
            rapidjson::Document document_done;
            document_done.SetObject();
            rapidjson::Document::AllocatorType& allocator1 = document_done.GetAllocator();
            document_done.AddMember("Done", true, allocator1);
            std::string pub_message_done = json_to_string(document_done);
            rc = publish_message(pub_message_done, TOPIC_T, client);
            rc = publish_message(pub_message_done, TOPIC_H, client);
        }
        else {
            //Create JSON DOM document object for humidity
            rapidjson::Document document_humidity;
            document_humidity.SetObject();
            rapidjson::Document::AllocatorType &allocator2 = document_humidity.GetAllocator();
            document_humidity.AddMember("Humidity", humidity, allocator2);
            document_humidity.AddMember("Unit", "%", allocator2);

            //Create JSON DOM document object for temperature
            rapidjson::Document document_temperature;
            document_temperature.SetObject();
            rapidjson::Document::AllocatorType &allocator3 = document_temperature.GetAllocator();
            document_temperature.AddMember("Temp", temperature, allocator3);
            document_temperature.AddMember("Unit", "C", allocator3);
            try {
                std::string pub_message_humidity = json_to_string(document_humidity);
                rc = publish_message(pub_message_humidity, TOPIC_H, client);
                std::string pub_message_temperature = json_to_string(document_temperature);
                rc = publish_message(pub_message_temperature, TOPIC_T, client);
            } catch (const std::exception &exc) {
                // catch anything thrown within try block that derives from std::exception
                std::cerr << exc.what();
            }
        }
        count = count + 1;
    }

    // End of loop. Stop MQTT and calculate runtime
    MQTTClient_disconnect(client, 1000);
    MQTTClient_destroy(&client);
    auto end = high_resolution_clock::now();
    std::chrono::duration<double> timer = end-start;
    std::ofstream outfile;
    outfile.open("piResultsCppLong.txt", std::ios_base::app); // append to the results text file
    outfile << "Humidity and temperature publisher runtime = " << timer.count() << "\n";
    std::cout << "Humidity and temperature runtime = " << timer.count() << "\n";
    return rc;
}
