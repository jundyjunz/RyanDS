#pragma once
#include "esp_wifi.h"        // esp_wifi_init, esp_wifi_set_mode, esp_wifi_set_config, esp_wifi_start, wifi_config_t, WIFI_MODE_AP, etc.
#include "esp_http_server.h"
#include "BuilderWarning.hpp"
#include <initializer_list> 
#include "../JSONObject.hpp"    
#include <string.h>
#include "HttpServerUtils.hpp" 

#define RESPONSE_DELAY 500

#define SETTINGS_ENTRY_TEMPLATE "<label>%s</label><input name=\"%s\"><br><br>" 
#define SETTINGS_ENTRY_TEMPLATE_LEN strlen(SETTINGS_ENTRY_TEMPLATE)-4 // %s%s = 4characters, need to remove from calculation.

#define CSS "<style>"\
            "form { "\
            "width: 90%;"\
            "max-width: 320px;"\
            "margin: 40px auto;"\
            "padding: 20px;"\
            "border: 1px solid #ccc;"\
            "border-radius: 30px;"\
            "text-align: center;"\
            "font-family: Helvetica;"\
            "font-weight: bold;"\
            "}"\
            "input{"\
            "width: 100%;" \
            "box-sizing: border-box;"\
            "padding: 8px;"\
            "background-color: white;"\
            "border: 2px solid #ccc;"\
            "border-radius: 15px;"\
            "font-size: 15px;"\
            "font-family: Helvetica;"\
            "font-weight: bold;"\
            "}"\
            "input:hover{"\
            "background-color:lightgray;"\
            "}"\
            "h2{"\
            "padding: 20px;"\
            "text-align: center;"\
            "font-family: Helvetica;"\
            "font-weight: bold;"\
            "}"\
            "hr{"\
            "border: 1px solid #ccc;"\
            "width: 25%;"\
            "}"\
            "</style>" 

#define CSS_LEN strlen(CSS) 

#define HTML_UPPER  "<h2>Settings</h2>"\
                    "<hr>"\
                    "<form id= \"theForm\">" 

#define HTML_UPPER_LEN strlen(HTML_UPPER) 

#define HTML_LOWER  "<input type=\"submit\" value=\"Confirm Settings\">"\
                    "</form>" 

#define HTML_LOWER_LEN strlen(HTML_LOWER)

#define JS  "<script>"\
            "let theForm = document.getElementById(\"theForm\");"\
            "theForm.addEventListener(\"submit\", async(aEvent)=>{"\
            "aEvent.preventDefault();"\
            "let theFormData= new FormData(theForm);"\
            "let theData = {};"\
            "theFormData.forEach((aValue, aKey)=>{ theData[aKey]=aValue; });"\
            "let theResponse = await fetch(\"/submit\", {"\
            "method: \"POST\","\
            "headers: {\"Content-Type\":\"application/json\"},"\
            "body: JSON.stringify(theData)"\
            "});"\
            "if (theResponse.ok) alert(\"Settings saved!\");"\
            "else alert(\"Server returned an error: \" + theResponse.status);"\
            "});"\
            "</script>"

#define JS_LEN strlen(JS)

class HttpSettingsServer: Builder<HttpSettingsServer>{ 
    protected: 
    httpd_handle_t myWebServerHandle; 
    httpd_config_t myWebServerConfig; 
    
    char* myFormHTML;  
    size_t myFormHTMLLen;
    void (*myPostCallback)(JSONObject& aRoot);
    bool myEsp32ResetsUponSubmission;

    // needs to be static since the uri struct takes in non-capture lambdas
    static esp_err_t getSettingsHandler(httpd_req_t *aRequest){
        HttpSettingsServer* theInstance = static_cast<HttpSettingsServer*>(aRequest->user_ctx);
        httpd_resp_send(aRequest, theInstance->myFormHTML, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }  

    static esp_err_t postSettingsHandler(httpd_req_t *aRequest){ 
        HttpSettingsServer* theInstance = static_cast<HttpSettingsServer*>(aRequest->user_ctx);

        char* theResponse =HttpServerUtils::getResponseBody(aRequest);

        JSONObject theObject = cJSON_Parse(theResponse);

        theInstance->myPostCallback(theObject);

        delete[] theResponse;

        httpd_resp_set_type(aRequest, "text/html");
        httpd_resp_sendstr(aRequest, "<h1>Settings Saved!</h1>"); 

        if(theInstance->myEsp32ResetsUponSubmission){ 
            vTaskDelay(pdMS_TO_TICKS(RESPONSE_DELAY));   // give time for the response to transmit.
            esp_restart();
        }

        return ESP_OK;
    }

    httpd_uri_t myGetSettingsURI; 

    httpd_uri_t myPostSettingsURI;
   
    int processEntry(const char* aKey, char** theEntries ,size_t& aIndex){ 
        size_t theEntryLen =2*strlen(aKey)+SETTINGS_ENTRY_TEMPLATE_LEN;
        theEntries[aIndex] = new char[theEntryLen+1]{};
        snprintf(theEntries[aIndex], theEntryLen+1, SETTINGS_ENTRY_TEMPLATE, aKey, aKey); 
        aIndex++; 
        return theEntryLen;
    }

    public:  
    HttpSettingsServer (){  
        myWebServerHandle = NULL;
        myWebServerConfig = HTTPD_DEFAULT_CONFIG(); 
        myFormHTML=nullptr; 
        myFormHTMLLen=0; 
        myPostCallback=nullptr;  
        myEsp32ResetsUponSubmission=false;  

        myGetSettingsURI={
            .uri       = "/",               
            .method    = HTTP_GET,          
            .handler   = getSettingsHandler, 
            .user_ctx  = this               
        }; 

        myPostSettingsURI= {
            .uri       = "/submit",               
            .method    = HTTP_POST,          
            .handler   = postSettingsHandler, 
            .user_ctx  = this              
        };
    }  

    HttpSettingsServer& buildPostCallback(void (*aCallback)(JSONObject& aRoot)){ 
        myPostCallback = aCallback;
        return *this;
    }

    HttpSettingsServer& buildSettingsForm(std::initializer_list<const char*> aKeys){         
        char** theEntries = new char*[aKeys.size()]{};
        size_t HTML_MIDDLE_LEN = 0;
        size_t i=0;

        for(const char* aKey: aKeys) HTML_MIDDLE_LEN += processEntry(aKey, theEntries, i);

        myFormHTMLLen = CSS_LEN + HTML_UPPER_LEN + HTML_MIDDLE_LEN + HTML_LOWER_LEN + JS_LEN; 
        myFormHTML =  new char[myFormHTMLLen+1]{}; 
        
        strcat(myFormHTML,CSS); 
        strcat(myFormHTML, HTML_UPPER); 
        for (int j=0; j<i; j++){ strcat(myFormHTML,theEntries[j]); delete[] theEntries[j]; } 
        delete[] theEntries; 
        strcat(myFormHTML, HTML_LOWER); 
        strcat(myFormHTML, JS);

        return *this;
    } 

    HttpSettingsServer& buildEsp32ResetsUponSubmission(){ 
        myEsp32ResetsUponSubmission=true;
        return *this;
    }

    HttpSettingsServer& buildInit(){ 
        BuilderWarning<HttpSettingsServer, 3> theWarning; 
        theWarning 
            .setRequired(myFormHTML!=nullptr) 
            .setRequired(myFormHTMLLen!=0)
            .setRequired(myPostCallback!=nullptr) 
            .enforce();
        return *this;
    }

    HttpSettingsServer& initWebServer(size_t aStackSize){ 
        myWebServerConfig.stack_size = aStackSize;
        httpd_start(&myWebServerHandle, &myWebServerConfig);
        httpd_register_uri_handler(myWebServerHandle, &myGetSettingsURI);
        httpd_register_uri_handler(myWebServerHandle, &myPostSettingsURI);
        return *this;
    }

    HttpSettingsServer& deinitWebServer(){ 
        httpd_stop(myWebServerHandle); 
        myWebServerHandle=NULL; 
        return *this;
    }
};