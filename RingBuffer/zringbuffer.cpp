#include "zringbuffer.h"
ZElement::ZElement()
{
    this->_pData=NULL;
}
//default each element maximum space is 1KB.
int ZElement::ZDoInit(int nMaxBytes)
{
    if(nMaxBytes<=0)
    {
        return -1;
    }
    this->_pData=new char[nMaxBytes];
    this->_nMaxBytes=nMaxBytes;
    this->_nValidBytes=0;
    return 0;
}
ZElement::~ZElement()
{
    if(NULL!=this->_pData)
    {
        delete [] this->_pData;
        this->_pData=NULL;
    }
}
//nMaxElements:how many elements in RingBuffer.
//nEachElementMaxBytes:how many bytes in each element.
ZRingBuffer::ZRingBuffer(int nMaxElements,int nEachElementMaxBytes)
{
    this->m_nMaxElements=nMaxElements;
    this->m_nEachElementMaxBytes=nEachElementMaxBytes;

    this->m_nRdPos=0;
    this->m_nWrPos=0;
}
int ZRingBuffer::ZDoInit()
{
    if(this->m_nMaxElements<=0 || this->m_nEachElementMaxBytes<=0)
    {
        return -1;
    }
    this->m_pElement=new ZElement[this->m_nMaxElements];
    for(int i=0;i<this->m_nMaxElements;i++)
    {
        if(this->m_pElement[i].ZDoInit(this->m_nEachElementMaxBytes)<0)
        {
            return -1;
        }
    }
    return 0;
}
ZRingBuffer::~ZRingBuffer()
{
    delete [] this->m_pElement;
}

//push/pop a element to ring buffer.
int ZRingBuffer::ZPush(const char *data,int len)
{
    //1.check if ring buffer is not full.
    if(this->ZIsFull())
    {
        return -1;
    }

    //2.write data in.
    if(len>this->m_pElement[this->m_nWrPos]._nMaxBytes)
    {
        return -1;
    }
    memcpy(this->m_pElement[this->m_nWrPos]._pData,data,len);
    this->m_pElement[this->m_nWrPos]._nValidBytes=len;

    //3.move wr pos to next position for next writing.
    this->m_nWrPos++;
    //check for wrap-around.
    if(this->m_nWrPos==this->m_nMaxElements)
    {
        this->m_nWrPos=0;
    }
    return 0;
}
int ZRingBuffer::ZPop(char *buffer,int nBufferLen)
{
    int nRdBytes=0;
    //1.check if we have valid data.
    if(this->ZIsEmpty())
    {
        return -1;
    }

    //2.read data out.
    if(nBufferLen<this->m_pElement[this->m_nRdPos]._nValidBytes)
    {
        return -1;
    }
    memcpy(buffer,this->m_pElement[this->m_nRdPos]._pData,this->m_pElement[this->m_nRdPos]._nValidBytes);
    nRdBytes=this->m_pElement[this->m_nRdPos]._nValidBytes;

    //3.move rd pos to next position for next reading.
    this->m_nRdPos++;
    //check for wrap-around.
    if(this->m_nRdPos==this->m_nMaxElements)
    {
        this->m_nRdPos=0;
    }
    return nRdBytes;
}

//check empty or full?
bool ZRingBuffer::ZIsFull()
{
    if((this->m_nWrPos+1)%this->m_nMaxElements==this->m_nRdPos)
    {
        return true;
    }
    return false;
}
//when rdPos==wrPos means empty.
bool ZRingBuffer::ZIsEmpty()
{
    if(this->m_nRdPos==this->m_nWrPos)
    {
        return true;
    }
    return false;
}
