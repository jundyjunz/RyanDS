#pragma once
struct BuilderWarningCheck{ 
    bool IsRequired;
    void (*Callback)();
};

template<typename T, int REQUIREMENTS>
class BuilderWarning{ 
    protected: 
    BuilderWarningCheck myChecks[REQUIREMENTS];
    int myCurrentCheck;
    
    public:
    BuilderWarning(){ myCurrentCheck=0;}

    BuilderWarning& setRequired(bool aRequirement, void (*aCallback)() = [](){}){ 
        if(myCurrentCheck<REQUIREMENTS) myChecks[myCurrentCheck++] = {aRequirement, aCallback };
        return *this;
    }  

    BuilderWarning& enforce(){ 
        for(int i=0; i<REQUIREMENTS; i++) if(myChecks[i].IsRequired)myChecks[i].Callback(); 
        return *this;
    }
}; 

template<typename T> class Builder{ virtual T& buildInit()=0; };