#pragma once
#include <stdio.h> 

#include "../Networking/WebSocketClient.hpp" 
#include "../Networking/HttpClient.hpp"
#include "../Circular/CircularReferenceBuffer.hpp"
#include <algorithm>
#include "LCD.hpp"

#define VIDEO_FRAME_SIZE 16384
#define VIDEO_FRAME_ROUTE "/frame_data_low_res/" 
#define VIDEO_SUBSCRIBE_ROUTE "/subscribe_video"
#define VIDEO_METADATA_ROUTE "/video_metadata_low_res" 

#define VIDEO_BUFFER_SIZE 1
#define VIDEO_BACKEND_BUFFER_SIZE 16384
#define VIDEO_FRAME_RATE_MS 8

class InitVideoTask{
    private: 
    static inline CircularReferenceBuffer* myVideoBuffer;
    static inline LCD* myLCDDisplay; 
    static inline uint8_t* myHoldingBuffer;
    static void sendFrame(){
        size_t theFrameBytes; 
        myVideoBuffer->get(&myHoldingBuffer, &theFrameBytes); 
        if(!theFrameBytes) return;
        myLCDDisplay->drawJpeg(myHoldingBuffer, theFrameBytes);
    }

    struct VideoStruct{ 
        char* VideoSubRoute; 
        size_t Width; 
        size_t Height;
    };

    public:  
    template<size_t PRIORITY, size_t CORE, size_t STACK_SIZE>
    static void initVideoTask(const char* aURI){
        
        VideoStruct theVideoStruct={};

        HttpClient::getRequest<32>(aURI, VIDEO_SUBSCRIBE_ROUTE, &theVideoStruct, [](void* aArgs, JSONObject& aResponse){ 
           VideoStruct* theVideoStructArg = static_cast<VideoStruct*>(aArgs);
                        
            int theID = aResponse["videoClientId"].as<int>(); 

            int theDigitCount = snprintf(nullptr, 0, "%d", theID);
            size_t theSubRouteSize=strlen(VIDEO_FRAME_ROUTE) + theDigitCount+1;
            char** theVideoSubRoute = &(theVideoStructArg->VideoSubRoute);
            *theVideoSubRoute = new char[theSubRouteSize];
            snprintf(*theVideoSubRoute, theSubRouteSize, "%s%d", VIDEO_FRAME_ROUTE, theID); //copies null terminator as well
        });

        HttpClient::getRequest<64>(aURI, VIDEO_METADATA_ROUTE, &theVideoStruct, [](void* aArgs, JSONObject& aResponse){ 
            VideoStruct* theVideoStructArg = static_cast<VideoStruct*>(aArgs);

            theVideoStructArg->Width = aResponse["width"].as<int>(); 
            theVideoStructArg->Height = aResponse["height"].as<int>();
        });

        printf("SubRoute: %s\n  Width:%d\n Height:%d,\n",theVideoStruct.VideoSubRoute, theVideoStruct.Width, theVideoStruct.Height); 


        myHoldingBuffer = static_cast<uint8_t*>(heap_caps_malloc(VIDEO_FRAME_SIZE*sizeof(uint8_t), MALLOC_CAP_SPIRAM));
        myVideoBuffer = new CircularReferenceBuffer(VIDEO_BUFFER_SIZE, VIDEO_FRAME_SIZE); //todo: write destructr for both of these
        myLCDDisplay = new LCD(theVideoStruct.Width, theVideoStruct.Height); 

        myLCDDisplay
        ->buildDecodeEngine()
        .buildLCDAll(GPIO_NUM_38, GPIO_NUM_39,GPIO_NUM_40, GPIO_NUM_41,GPIO_NUM_42); 

        static WebSocketClient theWebSocket; 
        theWebSocket  
        .buildFrameSize(VIDEO_FRAME_SIZE, VIDEO_BACKEND_BUFFER_SIZE)
        .buildWebsocketURI(aURI, theVideoStruct.VideoSubRoute)
        .buildKeepAliveParameters(10,3,30)
        .buildHandlers([](uint8_t** aBuffer, size_t aBufferSize){ 
            InitVideoTask::myVideoBuffer->put(aBuffer, aBufferSize); 
        })
        .buildInit();

        xTaskCreatePinnedToCore( 
            [](void* aArgs){ while(true){ sendFrame(); vTaskDelay(pdMS_TO_TICKS(VIDEO_FRAME_RATE_MS));}},  
            "LCDtask",  
            STACK_SIZE,  
            nullptr, 
            PRIORITY, 
            nullptr, 
            CORE);
    }
};