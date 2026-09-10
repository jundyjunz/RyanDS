#pragma once
#include "esp_http_server.h"

class HttpServerUtils{ 
    private: 
    static bool processResponseFragment(char* aResponseBody, int aResponseBodyLen,  httpd_req_t *aResponse, size_t& aBytesRead){ 
        int theByteFragment = httpd_req_recv(aResponse, aResponseBody + aBytesRead, aResponseBodyLen - aBytesRead);
        if (theByteFragment > 0) aBytesRead += (size_t)theByteFragment;
        else if (theByteFragment == HTTPD_SOCK_ERR_TIMEOUT) return true; //socket error, keep retrying
        else return false; 
        return true;
    }

    public:
    static char* getResponseBody(httpd_req_t *aResponse){ 
        size_t theResponseBodyLen= aResponse->content_len; 
        char* theResponseBody = new char[theResponseBodyLen+1]{};
        size_t theBytesRecieved = 0;
        while (theBytesRecieved < theResponseBodyLen) if(!processResponseFragment(theResponseBody, theResponseBodyLen, aResponse, theBytesRecieved)) return nullptr;
        return theResponseBody;
    }  
};