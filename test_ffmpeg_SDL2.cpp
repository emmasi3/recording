#define CRT_SECURE_NO_WARNING
#define SDL_MAIN_HANDLED
#include <iostream>
extern"C"
{
#include <SDL.h>
#include <SDL_audio.h>
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

}
int main()
{
	av_log_set_level(AV_LOG_DEBUG);

	const AVCodec* codec = nullptr;

	codec = avcodec_find_encoder_by_name("h264_nvenc"); // Inter
	if (!codec)
	{
		av_log(NULL, AV_LOG_ERROR, "Failed to find encoder -- h264_qsv!\n");
		return -1;
	}

	av_log(NULL, AV_LOG_INFO, "hello\n");

	return 0;
}