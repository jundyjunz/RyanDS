#pragma once
#include "cJSON.h"    
#include <type_traits>

class JSONObject{ 
    private:
    cJSON* myJSONObject=nullptr;
    bool myObjectIsASubNode=false;
    bool myObjectIsAlreadyAssigned=false;

    public: 

    JSONObject(cJSON* aCJSONObject, bool aIsSubNode=false){ 
        myJSONObject=aCJSONObject;
        myObjectIsASubNode=aIsSubNode;
    }

    JSONObject(JSONObject& aOtherJSONObject)=delete;
    JSONObject& operator=(const JSONObject&) = delete;

    JSONObject& operator=(cJSON* aCJSONObject){ 
        if(myObjectIsAlreadyAssigned) return *this;
        myJSONObject=aCJSONObject; 
        myObjectIsAlreadyAssigned=true;
        return *this;
    }    

    template<typename T>
    T as(){ 
        if constexpr (std::is_same_v<T, int>) return myJSONObject->valueint;
        if constexpr (std::is_same_v<T, double>) return myJSONObject->valuedouble; 
        if constexpr (std::is_same_v<T, char*>) return myJSONObject->valuestring; 

        printf("UNSUPPORTED CAST ATTEMPTED IN JSON OBJECT!!!!");
    } 

    JSONObject operator[](const char* aKey){ 
        return JSONObject(cJSON_GetObjectItem(myJSONObject, aKey), true);
    }

    ~JSONObject(){ 
        if(!myObjectIsASubNode) cJSON_Delete(myJSONObject);
    }

};
