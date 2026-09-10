#pragma once
class CircularCounter{ 
    private: 
    
    size_t myMax;
    size_t myValue; 
    
    public: 

    CircularCounter(){}
    CircularCounter(size_t aMax, size_t aDefaultValue=0) : myMax(aMax), myValue(aDefaultValue%aMax){} 

    CircularCounter& operator++(){ 
        ++myValue%=myMax; 
        return *this;
    }  

    size_t operator++(int){ 
        size_t theReturnValue = myValue; 
        ++(*this); 
        return theReturnValue;
    }

    operator int(){ return myValue; }

    size_t peek(){ 
        return (myValue+1)%myMax;
    } 

    size_t getMax(){ 
        return myMax;
    }

};