#pragma once
#include <stdio.h> 

#include "../Networking/WebSocketClient.hpp" 
#include "../Networking/HttpClient.hpp"
#include "../Circular/CircularReferenceBuffer.hpp"

#include "Speaker.hpp"

//#define AUDIO_FRAME_SIZE 16384
#define AUDIO_FRAME_ROUTE "/audio_data_low_res/" 
#define AUDIO_SUBSCRIBE_ROUTE "/subscribe_audio"

#define AUDIO_METADATA_ROUTE "/audio_metadata_low_res" 

#define AUDIO_BUFFER_SIZE 3
#define AUDIO_DMA_SIZE 5
#define AUDIO_TIMEOUT_MS 10 
#define AUDIO_BACKEND_BUFFER_SIZE 32768
#define AUDIO_FRAME_RATE_MS 8
class InitAudioTask{ 
    
    private: 
    static inline CircularReferenceBuffer* myAudioByteBuffer;
    static inline Speaker* mySpeaker; // inline needed because linker seems to call upon the functions before this is declared
    static inline uint8_t* myHoldingBuffer;
    static void sendFrame(){
        size_t theFrameBytes;
        myAudioByteBuffer->get(&myHoldingBuffer, &theFrameBytes);
        if(!theFrameBytes) return vTaskDelay(pdMS_TO_TICKS(AUDIO_TIMEOUT_MS));
        mySpeaker->writeToSpeaker(myHoldingBuffer, theFrameBytes, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS));
    }

    struct AudioStruct{ 
        char* AudioSubRoute; 
        double SampleRate; 
        size_t FramesPerBuffer;
    };

    public:
    template<size_t PRIORITY, size_t CORE, size_t STACK_SIZE>
    static void initAudioTask(const char* aURI){ 

        AudioStruct theAudioStruct = {};

        HttpClient::getRequest<32>(aURI, AUDIO_SUBSCRIBE_ROUTE, &theAudioStruct, [](void* aArgs, JSONObject& aResponse){ 
            AudioStruct* theAudioStructArg = static_cast<AudioStruct*>(aArgs);
                        
            int theID = aResponse["audioClientId"].as<int>(); 

            int theDigitCount = snprintf(nullptr, 0, "%d", theID);
            size_t theSubRouteSize=strlen(AUDIO_FRAME_ROUTE) + theDigitCount + 1; 
            char** theSubRoute = &(theAudioStructArg->AudioSubRoute);
            *theSubRoute = new char[theSubRouteSize];
            snprintf(*theSubRoute, theSubRouteSize, "%s%d", AUDIO_FRAME_ROUTE, theID);
        });

        HttpClient::getRequest<64>(aURI, AUDIO_METADATA_ROUTE, &theAudioStruct, [](void* aArgs, JSONObject& aResponse){ 
            AudioStruct* theAudioStructArg = static_cast<AudioStruct*>(aArgs);

            theAudioStructArg->SampleRate =  aResponse["sampleRate"].as<double>(); 
            theAudioStructArg->FramesPerBuffer =  aResponse["framesPerBuffer"].as<int>(); 
        });

        printf("SubRoute: %s\n  Sample Rate:%f\n FramesPerBuffer:%d,\n",theAudioStruct.AudioSubRoute, theAudioStruct.SampleRate, theAudioStruct.FramesPerBuffer); 
        
        myHoldingBuffer = static_cast<uint8_t*>(heap_caps_malloc(theAudioStruct.FramesPerBuffer*sizeof(uint8_t), MALLOC_CAP_SPIRAM));
        myAudioByteBuffer = new CircularReferenceBuffer(AUDIO_BUFFER_SIZE, theAudioStruct.FramesPerBuffer);
        mySpeaker = new Speaker(); 

        mySpeaker 
        ->buildSpeakerConfig(static_cast<size_t>(theAudioStruct.SampleRate), theAudioStruct.FramesPerBuffer, AUDIO_DMA_SIZE)
        .buildPinout(GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_21) 
        .buildInit();

        static WebSocketClient theWebSocket; 
        theWebSocket  
        .buildFrameSize(theAudioStruct.FramesPerBuffer, AUDIO_BACKEND_BUFFER_SIZE)
        .buildWebsocketURI(aURI, theAudioStruct.AudioSubRoute)
        .buildHandlers([](uint8_t** aBuffer, size_t aBufferSize){InitAudioTask::myAudioByteBuffer->put(aBuffer, aBufferSize);})
        .buildInit();

        xTaskCreatePinnedToCore( 
            [](void* aArgs){while(true){ sendFrame(); vTaskDelay(pdMS_TO_TICKS(AUDIO_FRAME_RATE_MS));}},  
            "SpeakerTask",  
            STACK_SIZE, 
            nullptr, 
            PRIORITY, 
            nullptr, 
            CORE);
    }

};