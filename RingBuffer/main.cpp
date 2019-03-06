#include <QCoreApplication>
#include <QDebug>
#include <zringbuffer.h>
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    ZRingBuffer rb(6,1024);
    rb.ZDoInit();
    for(qint32 i=0;i<20;i++)
    {
        if(rb.ZPush("hello",5)<0)
        {
            qDebug()<<"failed to put "<<i;
        }else{
            qDebug()<<"put okay:"<<i;
        }
    }

    for(qint32 i=0;i<20;i++)
    {
        char buffer[256];
        int nLen=rb.ZPop(buffer,256);
        if(nLen<0)
        {
            qDebug()<<"failed to pop";
        }else{
            buffer[nLen]=0;
            qDebug()<<"pop:"<<buffer;
        }
    }
    return 0;
}
