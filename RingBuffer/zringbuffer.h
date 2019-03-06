#ifndef ZRINGBUFFER_H
#define ZRINGBUFFER_H
#include <stdio.h>
#include <string.h>
class ZElement
{
public:
    ZElement();
    ~ZElement();

    //default each element maximum space is 1KB.
    int ZDoInit(int nMaxBytes=1024);

public:
    char *_pData;
    int _nMaxBytes;
    int _nValidBytes;
};
class ZRingBuffer
{
public:
    //nMaxElements:how many elements in RingBuffer.
    //nEachElementMaxBytes:how many bytes in each element.
    ZRingBuffer(int nMaxElements,int nEachElementMaxBytes);
    ~ZRingBuffer();

    int ZDoInit();

    //push/pop an element to ring buffer.
    int ZPush(const char *data,int len);
    int ZPop(char *buffer,int nBufferLen);

    //check empty or full?
    bool ZIsFull();
    bool ZIsEmpty();

private:
    int m_nMaxElements;
    int m_nEachElementMaxBytes;
    ZElement *m_pElement;

    //for write: write first,then move position.
    //for read: read first,then move position.
    int m_nRdPos;
    int m_nWrPos;
};

#endif // ZRINGBUFFER_H
