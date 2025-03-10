#include "AudioALSAPlaybackHandlerBase.h"
#include "AudioMTKDec.h"
#include "sound/compress_params.h"
#include <tinycompress/tinycompress.h>
#include <sound/asound.h>
#include "sound/compress_offload.h"
#include <pthread.h>
#include <cutils/list.h>


#ifndef ANDROID_AUDIO_ALSA_PLAYBACK_HANDLER_OFFLOAD_H
#define ANDROID_AUDIO_ALSA_PLAYBACK_HANDLER_OFFLOAD_H


namespace android
{

//below is control message
#define OFFLOAD_IOC_MAGIC    'a'

#define OFFLOADSERVICE_WRITEBLOCK     _IO(OFFLOAD_IOC_MAGIC, 0x01)
#define OFFLOADSERVICE_SETGAIN        _IO(OFFLOAD_IOC_MAGIC, 0x02)
#define OFFLOADSERVICE_SETMODE        _IO(OFFLOAD_IOC_MAGIC, 0x03)
#define OFFLOADSERVICE_SETDRAIN       _IO(OFFLOAD_IOC_MAGIC, 0x04)
#define OFFLOADSERVICE_CHECK_SUPPORT  _IO(OFFLOAD_IOC_MAGIC, 0x05)
//liang add
#define OFFLOADSERVICE_ACTION         _IO(OFFLOAD_IOC_MAGIC, 0x07)
#define OFFLOADSERVICE_GETTIMESTAMP   _IO(OFFLOAD_IOC_MAGIC, 0x08)
#define OFFLOADSERVICE_GETWRITEBLOCK  _IO(OFFLOAD_IOC_MAGIC, 0x09)
#define OFFLOADSERVICE_SETPARAM       _IO(OFFLOAD_IOC_MAGIC, 0x0A)
#define OFFLOADSERVICE_WRITE          _IO(OFFLOAD_IOC_MAGIC, 0x0B)



enum
{
    OFFLOAD_ACTION_OPEN  = 0,
    OFFLOAD_ACTION_START = 1,
    OFFLOAD_ACTION_PAUSE = 2,
    OFFLOAD_ACTION_RESUME = 3,
    OFFLOAD_ACTION_STOP   = 4,
    OFFLOAD_ACTION_CLOSE  = 5,
};


enum
{
    OFFLOAD_STATE_IDLE,
    OFFLOAD_STATE_PLAYING,
    OFFLOAD_STATE_PAUSED,
};

enum
{
    OFFLOAD_WRITE_EMPTY,
    OFFLOAD_WRITE_REMAIN,
};


enum
{
    OFFLOAD_CMD_WRITE,
    OFFLOAD_CMD_DRAIN,
    OFFLOAD_CMD_PAUSE,
    OFFLOAD_CMD_RESUME,
    OFFLOAD_CMD_CLOSE,
    OFFLOAD_CMD_FLUSH,
};


enum
{
    OFFLOAD_MODE_GDMA = 0,
    OFFLOAD_MODE_SW,
    OFFLOAD_MODE_DSP,
};

struct tstamp
{
    unsigned long frames;
    unsigned int samplerate;
};


struct offload_cmd
{
    struct listnode node;
    int cmd;
};
struct offload_thread_property
{

    pthread_mutex_t             offload_mutex;
    pthread_cond_t              offload_cond;
    struct listnode             offload_cmd_list;
    pthread_t                   offload_pthread;
};

struct offload_stream_property
{
    int offload_state;
    int fragment_size;
    int num_channels;
    int sample_rate;
    unsigned int    offload_gain[2];
    void    *tmpbsBuffer;
    int remain_write;
};

struct offload_write_info
{
    void *tmpBuffer;
    unsigned int  bytes;
};


struct offload_buffer
{
    size_t fragment_size;
    int fragments;
};

struct offload_codec
{
    __u32 id;
    __u32 ch_in;
    __u32 ch_out;
    __u32 sample_rate;
    __u32 bit_rate;
    __u32 rate_control;
    __u32 profile;
    __u32 level;
    __u32 ch_mode;
    __u32 format;
    __u32 align;
    union snd_codec_options options;
    __u32 reserved[3];
};


struct snd_offload_params
{
    struct offload_buffer buffer;
    struct offload_codec codec;
};


class AudioALSAPlaybackHandlerOffload : public AudioALSAPlaybackHandlerBase
{
    public:
        AudioALSAPlaybackHandlerOffload(const stream_attribute_t *stream_attribute_source);
        virtual ~AudioALSAPlaybackHandlerOffload();

        /**
         * open/close audio hardware
         */
        virtual status_t open();
        virtual status_t close();
        virtual status_t pause();
        virtual status_t resume();
        virtual status_t flush();
        virtual status_t routing(const audio_devices_t output_devices);
        virtual status_t setVolume(uint32_t vl);

        /**
         * write data to audio hardware
         */
        virtual ssize_t  write(const void *buffer, size_t bytes);

        virtual int drain(audio_drain_type_t type);

        virtual status_t get_timeStamp(unsigned long *frames, unsigned int *samplerate);

        virtual status_t setFilterMng(AudioMTKFilterManager *pFilterMng);

        int process_write();

        void offload_callback(stream_callback_event_t event);

        void offload_initialize();


    protected:

    private:
        status_t openOffloadDriver();
        status_t closeOffloadDriver();
        uint32_t ChooseTargetSampleRate(uint32_t SampleRate);
        AudioMTKDecHandlerBase *AudioDecHandlerCreate();
        bool SetLowJitterMode(bool bEnable, uint32_t SampleRate);
        uint32_t GetLowJitterModeSampleRate();
        struct snd_offload_params mComprConfig;
        struct mixer *mMixer;
        AudioMTKDecHandlerBase *mDecHandler;
        audio_format_t  mFormat;
        uint32_t   mWriteBsbufSize;
        bool       mReady;
};

} // end namespace android

#endif // end of ANDROID_AUDIO_ALSA_PLAYBACK_HANDLER_NORMAL_H
