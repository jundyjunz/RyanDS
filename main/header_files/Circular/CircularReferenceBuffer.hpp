#pragma once
#include "CircularCounter.hpp" 
#include "esp_heap_caps.h"
#include <string.h> 
#include <atomic>

//dont need to make ppoiters atomic, since the count updates atomically, and there's only one producer and consumer

class CircularReferenceBuffer{ 
    private: 
    size_t myBufferSize;  
    CircularCounter myHead;
    CircularCounter myTail; 
    std::atomic<uint8_t> myCurrentCount;
    
    uint8_t** myCircularReferenceBuffer; 
    size_t* myFrameSizes; 

    size_t myMaxFrameSize;
    uint8_t myMaxCount; 

    public:
    static void swapBytePointers(uint8_t** aPointerA, uint8_t** aPointerB){ 
        uint8_t* thePointerA = *aPointerA; 
        *aPointerA= *aPointerB; 
        *aPointerB = thePointerA;
    }

    CircularReferenceBuffer(uint8_t aBufferSize, size_t aMaxFrameSize, int aAlignment=-1, int aMemAllocScheme=MALLOC_CAP_SPIRAM):  
    myBufferSize(aBufferSize), 
    myHead(CircularCounter(aBufferSize)), 
    myTail(CircularCounter(aBufferSize)), 
    myCurrentCount(0), 
    myMaxFrameSize(aMaxFrameSize),
    myMaxCount(aBufferSize){    
        myCircularReferenceBuffer = new uint8_t*[aBufferSize];
        if(aAlignment==-1)for(int i=0; i< aBufferSize; i++) myCircularReferenceBuffer[i] = static_cast<uint8_t*>(heap_caps_malloc(aMaxFrameSize, aMemAllocScheme));
        else for(int i=0; i< aBufferSize; i++) myCircularReferenceBuffer[i] = static_cast<uint8_t*>(heap_caps_aligned_alloc(aAlignment, aMaxFrameSize, aMemAllocScheme));
        myFrameSizes = new size_t[aBufferSize]; 
    } 

    void put(uint8_t** aBuffer, size_t aFrameSize){  
        if(myCurrentCount.load()==myMaxCount)return;
        myFrameSizes[myHead]=aFrameSize; 
        swapBytePointers(&myCircularReferenceBuffer[myHead++] , aBuffer);
        myCurrentCount++;
    }  

    void get(uint8_t** aReplacementBuffer, size_t* aSize){   
        *aSize = 0;
        if(!myCurrentCount.load())return;
        *aSize = myFrameSizes[myTail]; 
        swapBytePointers(&myCircularReferenceBuffer[myTail], aReplacementBuffer);
        myTail++;
        myCurrentCount--;  
    } 
    
    void destroyBuffer(){ 
        for(uint8_t i=0; i<myMaxCount; i++) heap_caps_free(myCircularReferenceBuffer[i]);
        delete[] myCircularReferenceBuffer;
        delete[] myFrameSizes;
    }

    ~CircularReferenceBuffer(){ destroyBuffer(); }
};