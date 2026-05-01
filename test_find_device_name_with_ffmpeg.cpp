#include "include\libav_h.h"
#include <string>
#include <iostream>

static std::string audioDevName;

static bool is_audio_device(const AVDeviceInfo* dev)
{
    for (int i = 0; i < dev->nb_media_types; ++i) 
    {
        if (dev->media_types[i] == AVMEDIA_TYPE_AUDIO) 
        {
            return true;
        }
    }
    return false;
}

static int list_dshow_audio_devices()
{
    const AVInputFormat* iformat = av_find_input_format("dshow");
    if (!iformat) {
        printf("dshow input format not found\n");
        return -1;
    }

    AVDeviceInfoList* device_list = nullptr;

    // audio=true, video=false
    int ret = avdevice_list_input_sources(
        iformat,
        nullptr,
        nullptr,
        &device_list
    );

    if (ret < 0) {
        printf("avdevice_list_input_sources failed\n");
        return -1;
    }

    bool is_find = false;

    for (int i = 0; i < device_list->nb_devices; ++i) 
    {
        AVDeviceInfo* dev = device_list->devices[i];

        printf("Device %d:\n", i);
        printf("  name        : %s\n", dev->device_name);
        printf("  description : %s\n",
            dev->device_description ? dev->device_description : "null");
        if (is_audio_device(dev))
        {
            audioDevName = std::string("audio=") + dev->device_name;
            is_find = true;
            break;
        }
    }

    if (!is_find)
    {
        std::cout << "not Found any audio device!" << std::endl;
        return -1;
    }

    avdevice_free_list_devices(&device_list);
    
    AVFormatContext* aFmtCtx = avformat_alloc_context();
    if (!aFmtCtx)
    {
        std::cout << "avformat_alloc_context() failed\n";
        return -1;
    }

    // 打开设备
    ret = avformat_open_input(&aFmtCtx, audioDevName.c_str(), iformat, nullptr);
    if (ret < 0)
    {
        std::cout << "avformat_open_input(&aFmtCtx, audioDevName.c_str(), iformat, nullptr) failed\n";
        return ret;
    }

    // 关闭设备
    if (aFmtCtx)
    {
        avformat_close_input(&aFmtCtx);
        avformat_free_context(aFmtCtx);
    }

    return 0;
}

int main()
{
    // 注册设备
    avdevice_register_all();
    // 选择第一个可录制音频的设备
    list_dshow_audio_devices();

	return 0;
}