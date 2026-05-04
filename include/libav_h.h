#pragma once
#define _CRT_SECURE_NO_WARNINGS
// #define __STDC_CONSTANT_MACROS; 在这里定义也好，还是在 “预处理器定义” 中添加也好，反正要有，不然报错，很疑惑？为什么在 ffmpeg 操作详解中不用添加
// 该宏定义？
extern "C"
{
#include <libavutil/avutil.h>
#include <libavutil/fifo.h>
#include <libavutil/time.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include <libavdevice/avdevice.h>
#include <libavutil/audio_fifo.h>
}

#include <libavutil/hwcontext_d3d11va.h>