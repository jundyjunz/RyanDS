#pragma once
#include "../BuilderWarning.hpp"
#include "esp_websocket_client.h"   
#include "../SystemSetup.hpp"  
#include "URIUtils.hpp"
#include <string.h>
#include "../Circular/CircularCounter.hpp"

#define WEBSOCKET_PROTOCOL "ws://"
class WebSocketClient: Builder<WebSocketClient>{ 
    
    protected:
    char* myFrameURI;  
    bool myClientIsBuilt; 
    bool myKeepAliveIsBuilt;
    void (*myFrameCallback)(uint8_t**, size_t); 

    esp_websocket_client_config_t myWebSocketConfig = {};
    esp_websocket_client_handle_t myClient; 
    uint8_t* myFrameBuffer;  
    size_t myFrameSize;
  

    static void webSocketClientFrameHandler(void *aHandlerArgs, esp_event_base_t aBase, int32_t aEventId, void *aEventData){ 
        if (aEventId != WEBSOCKET_EVENT_DATA) return;
        
        esp_websocket_event_data_t* theEventData = static_cast<esp_websocket_event_data_t*>(aEventData);
        WebSocketClient* theInstance = static_cast<WebSocketClient*>(aHandlerArgs);

        int thePayloadFullLen = theEventData->payload_len;
        int thePayloadOffsetLen = theEventData->payload_offset;
        int thePayloadFragmentLen = theEventData->data_len; 
        const char* thePayload = theEventData->data_ptr;
        uint8_t** theFrameBuffer = &(theInstance->myFrameBuffer);

        if((size_t)(thePayloadOffsetLen + thePayloadFragmentLen) > theInstance->myFrameSize) return;
        
        memcpy(*theFrameBuffer + thePayloadOffsetLen, thePayload, thePayloadFragmentLen);

        if(thePayloadFullLen - thePayloadOffsetLen == thePayloadFragmentLen) theInstance->myFrameCallback(theFrameBuffer, thePayloadFullLen);  
    }
    public: 
    WebSocketClient(){ 
        myFrameURI = nullptr;       
        myFrameCallback=nullptr; 
        myClientIsBuilt=false; 
        myFrameBuffer =  nullptr; 
        myKeepAliveIsBuilt=true; 
        myFrameSize=0;
    }

    WebSocketClient& buildFrameSize(size_t aFrameSize, size_t aBackendBufferSize){  
        myFrameSize=aFrameSize;
        myFrameBuffer = static_cast<uint8_t*>(heap_caps_malloc(aFrameSize*sizeof(uint8_t), MALLOC_CAP_SPIRAM));
        myWebSocketConfig.buffer_size = aBackendBufferSize;
        return *this;
    }

    WebSocketClient& buildWebsocketURI(const char* aBaseURI, const char* aSubRouteURI){  
        myFrameURI = URIUtils::buildURI(WEBSOCKET_PROTOCOL, aBaseURI, aSubRouteURI);
        myWebSocketConfig.uri=myFrameURI;
        myClient = esp_websocket_client_init(&myWebSocketConfig);
        myClientIsBuilt=true;
        return *this;
    } 

    WebSocketClient& buildKeepAliveParameters(size_t aIntervalTokeepAlive, size_t aFailureTolerance, size_t aIdleTolerance){ 
        myWebSocketConfig.keep_alive_enable = true;
        myWebSocketConfig.keep_alive_idle = aIdleTolerance;
        myWebSocketConfig.keep_alive_interval = aIntervalTokeepAlive;
        myWebSocketConfig.keep_alive_count = aFailureTolerance; 
        myKeepAliveIsBuilt=true; 
        return *this;
    }

    WebSocketClient& buildHandlers(void (*aFrameCallback)(uint8_t**, size_t)){   
        if(!myClientIsBuilt || myFrameBuffer==nullptr || !myKeepAliveIsBuilt) return *this;
        myFrameCallback=aFrameCallback;
        esp_websocket_register_events(this->myClient, WEBSOCKET_EVENT_ANY, webSocketClientFrameHandler, this); 
        return *this;
    }

    WebSocketClient& buildInit() override { 
        BuilderWarning<WebSocketClient, 5> theWarning; 
        theWarning 
            .setRequired(myFrameURI!=nullptr) 
            .setRequired(myFrameCallback!=nullptr)
            .setRequired(myClientIsBuilt!=false) 
            .setRequired(myFrameBuffer!=nullptr) 
            .setRequired(myKeepAliveIsBuilt!=false)
            .enforce();
            if(!SystemSetup::getInstance().waitForWifiToConnect()) return *this;
            esp_websocket_client_start(myClient);
        return *this;
    } 

    void sendBytes(uint8_t* aBytesToSend, size_t aBytesToSendLen, TickType_t aTicksToWait){ 
        esp_websocket_client_send_bin(myClient, (const char *)aBytesToSend, aBytesToSendLen, aTicksToWait); //const char needed as that's how the api was written
    }

    void destroyWebSocket(){ 
        esp_websocket_client_stop(myClient);
        esp_websocket_client_destroy(myClient);
        heap_caps_free(myFrameBuffer);  
        delete[] myFrameURI;  

        myFrameURI = nullptr;
        myClientIsBuilt = false;
        myFrameBuffer = nullptr;
        myKeepAliveIsBuilt = false;
        myFrameCallback = nullptr;
        myFrameSize = 0;
    }
};

