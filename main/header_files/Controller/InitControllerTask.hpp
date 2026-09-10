#pragma once
#include <stdio.h> 

#include "../Networking/WebSocketClient.hpp" 
#include "../Networking/HttpClient.hpp"
#include "ShiftRegister74HC165.hpp"
#include "JoyStick.hpp"
#include "../SystemLED.hpp"

#define PORT_SUBSCRIBE_ROUTE "/subscribe_port/"
#define PORT_SEND_COMMAND_ROUTE "/serial_post/"
#define PORT_SERIAL_CONNECTIONS_ROUTE "/serial_connections_ct"

#define COMMAND_BYTE_BUFFER_SIZE 5
#define START_BYTE 0xFA
#define PORT_BACKEND_BUFFER_SIZE 65536
#define PORT_TIMEOUT_MS 10
#define UPPER_BOUND 2500
#define LOWER_BOUND 500
#define BLINK_AMT 5
#define PORT_FRAME_RATE_MS 4

#define BLUE_CONTROLLER 0
#define GREEN_CONTROLLER 1
#define RED_CONTROLLER 2

class ControllerConnection{ 
    private: 
    WebSocketClient myWebSocketClient; 
    size_t myMaxControllers;
    size_t myClientId; 
    CircularCounter myController; 
    uint8_t myByteBuffer[COMMAND_BYTE_BUFFER_SIZE]={};
    char* myControllerSendCommandRoute;
    char* myBaseURI;

    void connectToController(){ 
        size_t myCounterAsInt = static_cast<int>( myController);
        size_t theControllerSubscribeRouteLen = strlen(PORT_SUBSCRIBE_ROUTE)+snprintf(nullptr, 0, "%d", myCounterAsInt)+1;
        char* theControllerSubscribeRoute = new char[theControllerSubscribeRouteLen];
        snprintf(theControllerSubscribeRoute, theControllerSubscribeRouteLen, "%s%d", PORT_SUBSCRIBE_ROUTE, myCounterAsInt);

        HttpClient::getRequest<32>(myBaseURI, theControllerSubscribeRoute, this, [](void* aArgs, JSONObject& aResponse){ 
            ControllerConnection* theControllerConnection = static_cast<ControllerConnection*>(aArgs);
            theControllerConnection->myClientId= aResponse["portClientId"].as<int>();
        }); 
        
        printf("%s\n",theControllerSubscribeRoute);

        delete[] theControllerSubscribeRoute;

        size_t theControllerSendCommandRouteLen = strlen(PORT_SEND_COMMAND_ROUTE) + snprintf(nullptr, 0, "%d", myCounterAsInt) + 1 + snprintf(nullptr, 0, "%d", myClientId)+ 1;// extra plus 1 for "/" symbol
        myControllerSendCommandRoute = new char[theControllerSendCommandRouteLen];  
        snprintf(myControllerSendCommandRoute, theControllerSendCommandRouteLen, "%s%d/%d", PORT_SEND_COMMAND_ROUTE, myCounterAsInt, myClientId); 

        myWebSocketClient 
        .buildFrameSize(COMMAND_BYTE_BUFFER_SIZE, PORT_BACKEND_BUFFER_SIZE)
        .buildWebsocketURI(myBaseURI, myControllerSendCommandRoute)
        .buildHandlers([](uint8_t** aBuffer, size_t aBufferSize){return;})
        .buildInit();

        printf("%s\n",myControllerSendCommandRoute);
    }

    public: 

    ControllerConnection(const char* aURI){
        myBaseURI = new char[strlen(aURI) + 1];
        strcpy(myBaseURI, aURI);
        myByteBuffer[0]=START_BYTE;
        myControllerSendCommandRoute = nullptr;
        HttpClient::getRequest<32>(myBaseURI, PORT_SERIAL_CONNECTIONS_ROUTE, this, [](void* aArgs, JSONObject& aResponse){ 
            ControllerConnection* theControllerConnection = static_cast<ControllerConnection*>(aArgs);
            theControllerConnection->myMaxControllers = aResponse["count"].as<int>();
        });

        myController= CircularCounter(myMaxControllers);  

        connectToController();
    }     

    void setByteBuffer(int aLevel, int aBuffer, int aPosition){ 
       aLevel ? myByteBuffer[aBuffer] |= 0x1<<aPosition : myByteBuffer[aBuffer] &= ~(0x1<<aPosition);
    }

    void sendCommand(){ 
        myWebSocketClient.sendBytes(myByteBuffer, COMMAND_BYTE_BUFFER_SIZE, pdMS_TO_TICKS(PORT_TIMEOUT_MS));
    } 

    ControllerConnection& operator++(){ 
        delete[] myControllerSendCommandRoute; 
        myWebSocketClient.destroyWebSocket();
        myController++;
        connectToController(); 
        return *this;
    } 

    size_t getCurrentControllerIndex(){ 
        return myController;
    }

    size_t getNextControllerIndex(){ 
        return myController.peek();
    }
};

class InitControllerTask{ 
    
    private: 
    static inline ShiftRegister74HC165<2>* myShiftRegister=nullptr;  
    static inline JoySticks<2>* myJoySticks=nullptr; 
    static inline ControllerConnection* myConnection = nullptr;

    public:
    template<size_t PRIORITY, size_t CORE, size_t STACK_SIZE>
    static void initControllerTask(const char* aURI){ 
                
        myJoySticks = new JoySticks<2>(); 
        myJoySticks
        ->buildJoyStickPins(0 ,	ADC_CHANNEL_3 ,	ADC_CHANNEL_4) //gpio 4,5
        .buildJoyStickPins(1,	ADC_CHANNEL_5, 	ADC_CHANNEL_6) //gpio 6,7
        .buildJoyStickCallBacks(0,
            [](int aXLevel){ 
                myConnection->setByteBuffer(aXLevel>UPPER_BOUND, 3, 6); //left
                myConnection->setByteBuffer(aXLevel<LOWER_BOUND, 3, 5); //right
                
            },
            [](int aYLevel){
                //myConnection->setByteBuffer(aYLevel>UPPER_BOUND, 3, 7); //up 
                //myConnection->setByteBuffer(aYLevel<LOWER_BOUND, 2, 0); //down
            })
        .buildJoyStickCallBacks(1,
            [](int aXLevel){
                myConnection->setByteBuffer(aXLevel>UPPER_BOUND, 2, 2);//cleft
                myConnection->setByteBuffer(aXLevel<LOWER_BOUND, 2, 1); //cright
            },
            [](int aYLevel){
                //myConnection->setByteBuffer(aYLevel>UPPER_BOUND, 2, 4); //cup
                //myConnection->setByteBuffer(aYLevel<LOWER_BOUND, 2, 3); //cdown
            });

        myShiftRegister =  new ShiftRegister74HC165<2>(); 
        myShiftRegister
        ->buildSHLDPin(GPIO_NUM_13) 
        .buildCLKPin(GPIO_NUM_12) 
        .buildQHPin(GPIO_NUM_11)  
        .buildCallBack(0,[](int aLevel){ myConnection->setByteBuffer(aLevel, 1, 7);})//a        
        .buildCallBack(1,[](int aLevel){ myConnection->setByteBuffer(aLevel, 1, 6);})//b
        .buildCallBack(2,[](int aLevel){ myConnection->setByteBuffer(aLevel, 1, 5);})//x
        .buildCallBack(3,[](int aLevel){ myConnection->setByteBuffer(aLevel, 1, 4);})//y
        .buildCallBack(4,[](int aLevel){ myConnection->setByteBuffer(aLevel, 1, 3);})//dup
        .buildCallBack(5,[](int aLevel){ myConnection->setByteBuffer(aLevel, 1, 2);})//ddown
        .buildCallBack(6,[](int aLevel){ myConnection->setByteBuffer(aLevel, 1, 1);})//dleft
        .buildCallBack(7,[](int aLevel){ myConnection->setByteBuffer(aLevel, 1, 0);})//dright
        .buildCallBack(8,[](int aLevel){ myConnection->setByteBuffer(aLevel, 2, 7);})//start 
        .buildCallBack(9,[](int aLevel){ myConnection->setByteBuffer(aLevel, 2, 6);})//ltrigger
        .buildCallBack(10,[](int aLevel){myConnection->setByteBuffer(aLevel, 2, 5);})//rtrigger
        .buildCallBack(11,[](int aLevel){myConnection->setByteBuffer(aLevel, 3, 4);})//ztrigger
        .buildCallBack(12,[](int aLevel){ 
            if(aLevel){ 
                
                if(myConnection->getNextControllerIndex() == BLUE_CONTROLLER) SystemLED::getInstance().writeColor(0,0,255);
                if(myConnection->getNextControllerIndex() == GREEN_CONTROLLER) SystemLED::getInstance().writeColor(0,255,0);
                if(myConnection->getNextControllerIndex() == RED_CONTROLLER) SystemLED::getInstance().writeColor(255,0,0);

                ++(*myConnection); 
            }  
        })
        .buildCallBack(13,[](int aLevel){})
        .buildCallBack(14,[](int aLevel){})
        .buildCallBack(15,[](int aLevel){})
        .buildInit(); 

        myConnection= new ControllerConnection(aURI); 

        xTaskCreatePinnedToCore( 
            [](void* aArgs){while(true){ 
                myShiftRegister->readRegisters();  
                myJoySticks->readJoyStickValues(0);
                myJoySticks->readJoyStickValues(1);
                myConnection->sendCommand();
                vTaskDelay(pdMS_TO_TICKS(PORT_FRAME_RATE_MS));  
                if(SystemLED::getInstance().isOn())SystemLED::getInstance().turnOff();
            }},  
            "ControllerTask",  
            STACK_SIZE, 
            nullptr, 
            PRIORITY, 
            nullptr, 
            CORE);
    }

};