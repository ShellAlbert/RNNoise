/* Use the newer ALSA API */
#include <stdio.h>
#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>
extern "C"
{
#include <rnnoise.h>
}
#include <signal.h>
//use usleep() to reduce cpu heavy load.
#define AUDIO_THREAD_SCHEDULE_US    (1000*10) //10ms.
#define VIDEO_THREAD_SCHEDULE_US    (1000*10) //10ms.
#define SAMPLE_RATE      48000
#define CHANNELS_NUM    2
#define BYTES_PER_FRAME 4
#define ALSA_PERIOD     20
#define BLOCK_SIZE      48000
#define SND_CAP_NAME    "plughw:CARD=USBSA,DEV=0"
#define SND_PLAY_NAME    "plughw:CARD=USBSA,DEV=0"

static int g_Exit=0;
void gSigHandler(int signo)
{
    printf("Waiting exit...\n");
    g_Exit=1;
    return;
}
void do_audio_suppression(DenoiseState *st,char *pcmIn,unsigned int pcmLen)
{
    //because original pcm data is 48000 bytes.
    //so here we choose 480/960/xx.
    //to make sure we have no remaing bytes.
    //here FRAME_SIZE=480.
    //const int frame_size=FRAME_SIZE;
    const int frame_size=480;//48000.
    short sTemp[frame_size];
    float fTemp[frame_size];

    int nBlkSize=sizeof(short)*frame_size;
    //nNoisePCMLen返回的是字节数，但此处我们要以int16_t作为基本单位，所以要除以sizeof(int16_t).
    unsigned int nPcmDataInt16Len=pcmLen/sizeof(short);
    //计算需要循环处理多少次，以及还余多少字节.
    int frames=pcmLen/nBlkSize;
    int remainBytes=pcmLen%nBlkSize;
    if(remainBytes>0)
    {
        printf("<Warning>:RNNoise has remaining bytes.\n");
    }
    //qDebug()<<"pcm size:"<<baPCM.size()<<",blk size="<<nBlkSize;
    //qDebug()<<"size="<<frame_size<<",frame="<<frames<<",remaingBytes="<<remainBytes;
    for(int i=0;i<frames;i++)
    {

        //prepare data.
        memcpy((char*)sTemp,pcmIn+nBlkSize*i,nBlkSize);
        //convert.
        for(int j=0;j<frame_size;j++)
        {
            fTemp[j]=sTemp[j];
        }
        //process with differenct noise view.
        rnnoise_process_frame(st,fTemp,fTemp);

        //convert.
        for(int j=0;j<frame_size;j++)
        {
            sTemp[j]=fTemp[j];
        }
        //move back.
        memcpy(pcmIn+nBlkSize*i,(char*)sTemp,nBlkSize);
    }
    return;
}


int main(void)
{
    int pcmBlkSize= BLOCK_SIZE;	// Raw input or output frame size in bytes
    snd_pcm_uframes_t pcmFrames=pcmBlkSize/BYTES_PER_FRAME;// Convert bytes to frames
    unsigned int nSampleRate=SAMPLE_RATE;
    unsigned int nNearSampleRate=nSampleRate;
    unsigned int periods=ALSA_PERIOD;
    unsigned int request_periods=periods;
    int dir=0;
    ////////////////////////////////////////////////
    snd_pcm_t	*pcmHandle;
    snd_pcm_hw_params_t *hwparams;

    if (snd_pcm_open(&pcmHandle,SND_CAP_NAME,SND_PCM_STREAM_CAPTURE,0)<0)
    {
        printf("failed to open capture device.\n");
        exit(-1);
    }

    snd_pcm_hw_params_alloca(&hwparams);
    if(snd_pcm_hw_params_any(pcmHandle,hwparams)<0)
    {
        printf("<Error>:Audio CapThread,Cannot configure this PCM device.\n");
        exit(-1);
    }

    if(snd_pcm_hw_params_set_access(pcmHandle,hwparams,SND_PCM_ACCESS_RW_INTERLEAVED)<0)
    {
        printf("<Error>:Audio CapThread,error at snd_pcm_hw_params_set_access().\n");
        exit(-1);
    }

    /* Set sample format */
    if(snd_pcm_hw_params_set_format(pcmHandle,hwparams,SND_PCM_FORMAT_S16_LE)<0)
    {
        printf("<Error>:Audio CapThread,error at snd_pcm_hw_params_set_format().\n");
        exit(-1);
    }

    if(snd_pcm_hw_params_set_rate_near(pcmHandle,hwparams,&nNearSampleRate,0u)<0)
    {
        printf("<Error>:Audio CapThread,error at snd_pcm_hw_params_set_rate_near().\n");
        exit(-1);
    }
    if(nNearSampleRate!=nSampleRate)
    {
        printf("<Warning>:Audio CapThread,the sampled rate %d Hz is not supported by hardware.\n",nSampleRate);
        printf("<Warning>:Using %d Hz instead.\n",nNearSampleRate);
    }

    /* Set number of channels */
    if(snd_pcm_hw_params_set_channels(pcmHandle,hwparams,CHANNELS_NUM)<0)
    {
        printf("<Error>:CapThread,error at snd_pcm_hw_params_set_channels().\n");
        exit(-1);
    }


    //	Restrict a configuration space to contain only one periods count
    if(snd_pcm_hw_params_set_periods_near(pcmHandle,hwparams,&periods,&dir)<0)
    {
        printf("<Error>:Audio CapThread,error at setting periods.\n");
        exit(-1);
    }
    if(request_periods!=periods)
    {
        printf("<Warning>:Audio CapThread,requested %d periods,but recieved %d.\n", request_periods, periods);
    }

    /*
            The unit of the buffersize depends on the function.
            Sometimes it is given in bytes, sometimes the number of frames has to be specified.
            One frame is the sample data vector for all channels.
            For 16 Bit stereo data, one frame has a length of four bytes.
        */
    if(snd_pcm_hw_params_set_buffer_size_near(pcmHandle,hwparams,&pcmFrames)<0)
    {
        printf("<Error>:CapThread,error at snd_pcm_hw_params_set_buffer_size_near().\n");
        exit(-1);
    }

    /*
            If your hardware does not support a buffersize of 2^n,
            you can use the function snd_pcm_hw_params_set_buffer_size_near.
            This works similar to snd_pcm_hw_params_set_rate_near.
            Now we apply the configuration to the PCM device pointed to by pcm_handle.
            This will also prepare the PCM device.
        */
    /* Apply HW parameter settings to PCM device and prepare device*/
    if(snd_pcm_hw_params(pcmHandle,hwparams)<0)
    {
        printf("<Error>:Audio CapThread,error at snd_pcm_hw_params().\n");
        exit(-1);
    }


    //////////////////////////////////////////////
    snd_pcm_t *pcmHandle2;
    snd_pcm_hw_params_t *hwparams2;

    if(snd_pcm_open(&pcmHandle2,SND_PLAY_NAME,SND_PCM_STREAM_PLAYBACK,0)<0)
    {
        printf("failed to open play device.\n");
        exit(-1);
    }

    /* Allocate the snd_pcm_hw_params_t structure on the stack. */
    snd_pcm_hw_params_alloca(&hwparams2);

    /*
        Before we can write PCM data to the soundcard,
        we have to specify access type, sample format, sample rate, number of channels,
        number of periods and period size.
        First, we initialize the hwparams structure with the full configuration space of the soundcard.
    */
    /* Init hwparams with full configuration space */
    if(snd_pcm_hw_params_any(pcmHandle2,hwparams2)<0)
    {
        printf("<Error>:Audio PlayThread,error at snd_pcm_hw_params_any().\n");
        exit(-1);
    }


    if(snd_pcm_hw_params_set_access(pcmHandle2,hwparams2,SND_PCM_ACCESS_RW_INTERLEAVED)<0)
    {
        printf("<Error>:Audio PlayThread,error at snd_pcm_hw_params_set_access().\n");
        exit(-1);
    }

    /* Set sample format */
    if(snd_pcm_hw_params_set_format(pcmHandle2,hwparams2,SND_PCM_FORMAT_S16_LE)<0)
    {
        printf("<Error>:Audio PlayThread,error at snd_pcm_hw_params_set_format().\n");
        exit(-1);
    }

    /* Set sample rate. If the exact rate is not supported */
    /* by the hardware, use nearest possible rate.         */
    if(snd_pcm_hw_params_set_rate_near(pcmHandle2,hwparams2,&nNearSampleRate,0u)<0)
    {
        printf("<Error>:Audio PlayThread,error at snd_pcm_hw_params_set_rate_near().\n");
        exit(-1);
    }
    if(nNearSampleRate!=nSampleRate)
    {
        printf("<Warning>:Audio PlayThread,the rate %d Hz is not supported by hardware.\n",nSampleRate);
        printf("<Warning>:Using %d instead.\n",nNearSampleRate);
    }

    /* Set number of channels */
    if(snd_pcm_hw_params_set_channels(pcmHandle2,hwparams2,CHANNELS_NUM)<0)
    {
        printf("<Error>:Audio PlayThread,error at snd_pcm_hw_params_set_channels().\n");
        exit(-1);
    }

    //	Restrict a configuration space to contain only one periods count
    if(snd_pcm_hw_params_set_periods_near(pcmHandle2,hwparams2,&periods,&dir)<0)
    {
        printf("<Error>:Audio PlayThread,error at snd_pcm_hw_params_set_periods_near().\n");
        exit(-1);
    }
    if(request_periods!=periods)
    {
        printf("<Warning>:Audio PlayThread,requested %d periods,but recieved %d.\n", request_periods, periods);
    }

    /*
        The unit of the buffersize depends on the function.
        Sometimes it is given in bytes, sometimes the number of frames has to be specified.
        One frame is the sample data vector for all channels.
        For 16 Bit stereo data, one frame has a length of four bytes.
    */
    if(snd_pcm_hw_params_set_buffer_size_near(pcmHandle2,hwparams2,&pcmFrames)<0)
    {
        printf("<Error>:Audio PlayThread,error at snd_pcm_hw_params_set_buffer_size_near().\n");
        exit(-1);
    }

    /*
        If your hardware does not support a buffersize of 2^n,
        you can use the function snd_pcm_hw_params_set_buffer_size_near.
        This works similar to snd_pcm_hw_params_set_rate_near.
        Now we apply the configuration to the PCM device pointed to by pcm_handle.
        This will also prepare the PCM device.
    */
    /* Apply HW parameter settings to PCM device and prepare device*/
    if(snd_pcm_hw_params(pcmHandle2,hwparams2)<0)
    {
        printf("<Error>:Audio PlayThread,error at snd_pcm_hw_params().\n");
        exit(-1);
    }


    DenoiseState *m_st=rnnoise_create();
    if(m_st==NULL)
    {
        printf("failed to create DenoiseState!\n");
        return -1;
    }
    //allocate buffer to store input pcm data.
    char *pcmBuffer=new char[pcmBlkSize];

    signal(SIGINT,gSigHandler);
    while(!g_Exit)
    {
        // Read capture buffer from ALSA input device.
        if(snd_pcm_readi(pcmHandle,pcmBuffer,pcmFrames)<0)
        {
            snd_pcm_prepare(pcmHandle);
            printf("<cap>:Buffer Overrun\n");
            continue;
        }


        //do noise suppression here.
        //...............
        //...............
        //stero,two channels,pcm data.
        //data address:pcmBuffer, size:pcmBlkSize.
        do_audio_suppression(m_st,pcmBuffer,pcmBlkSize);


        //write pcm data to play device.
        while(snd_pcm_writei(pcmHandle2,pcmBuffer,pcmFrames)<0)
        {
            snd_pcm_prepare(pcmHandle2);
            printf("<playback>:Buffer Underrun\n");
            continue;
        }
    }

    //do some clean here.
    /*
        If we want to stop playback, we can either use snd_pcm_drop or snd_pcm_drain.
        The first function will immediately stop the playback and drop pending frames.
        The latter function will stop after pending frames have been played.
    */
    /* Stop PCM device and drop pending frames */
    snd_pcm_drop(pcmHandle);
    snd_pcm_drop(pcmHandle2);
    /* Stop PCM device after pending frames have been played */
    //snd_pcm_drain(pcmHandle);
    snd_pcm_close(pcmHandle);
    snd_pcm_close(pcmHandle2);

    if(pcmBuffer!=NULL)
    {
        delete [] pcmBuffer;
    }
    rnnoise_destroy(m_st);
    printf("ends.\n");
    return 0;
}

