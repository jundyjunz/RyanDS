#pragma once
#include <string.h>

class URIUtils{ 
    public: 
        static char* buildURI(const char* aProtocol, const char* aURI, const char* aSubRoute){ 
        size_t theURISize = strlen(aProtocol) + strlen(aURI) + strlen(aSubRoute);
        char* theReturnURI = new char[theURISize+1]{}; 
        strcat(theReturnURI , aProtocol); 
        strcat(theReturnURI , aURI); 
        strcat(theReturnURI , aSubRoute);   
        return theReturnURI;
    }
};