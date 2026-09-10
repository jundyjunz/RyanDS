#pragma once
#include "esp_http_client.h"
#include "../SystemSetup.hpp" 
#include "URIUtils.hpp"
#include "../JSONObject.hpp"    

#define CONNECTION_TIMEOUT_MS 15000

#define HTTP_PROTOCOL "http://"

class HttpClient{ 
    private: 
    template<size_t RESPONSE_SIZE>
    static bool readChunk(esp_http_client_handle_t& aClient, char* aResponseBody, size_t& aOutBytesRead){ 
        if (aOutBytesRead + 1 >= RESPONSE_SIZE) return false;   // buffer full — stop before any unsigned underflow can happen
        int theBytesReceived = esp_http_client_read(aClient, aResponseBody + aOutBytesRead, RESPONSE_SIZE - aOutBytesRead - 1);
        if(theBytesReceived<=0)return false; 
        aOutBytesRead+=(size_t)theBytesReceived;
        return true;
    }

    public:
    template<size_t RESPONSE_SIZE>
    static bool getRequest(const char* aBaseURI, const char* aSubRoute, void* aArgsForCallback, void (*aCallback)(void* aArgs, JSONObject& aResponse)){ 
        if(!SystemSetup::waitForWifiToConnect()) return false;       
        
        char* theURI = URIUtils::buildURI(HTTP_PROTOCOL, aBaseURI, aSubRoute);

        esp_http_client_config_t theConfig = {};
        theConfig.url = theURI;
        esp_http_client_handle_t theClient = esp_http_client_init(&theConfig);
        esp_http_client_open(theClient, 0);   // 0 = no write payload for GET
        esp_http_client_fetch_headers(theClient); 
        
        char theResponseBody[RESPONSE_SIZE]={}; 
        size_t theBytesRead =0;
        while(readChunk<RESPONSE_SIZE>(theClient, theResponseBody, theBytesRead));
        
        JSONObject theJSONObject = cJSON_Parse(theResponseBody);
        aCallback(aArgsForCallback, theJSONObject);

        esp_http_client_close(theClient);
        esp_http_client_cleanup(theClient); 
        delete[] theURI;  
        
        return true;
    }
};

