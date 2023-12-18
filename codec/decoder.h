#ifndef DECODER_H
#define DECODER_H

#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/frame.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

// frame 回调
typedef int (*onFrameFunPtr)(AVFrame *frame);

typedef struct Decoder
{
    AVCodecContext *decoder_ctx;
    struct SwsContext *sws_ctx;

    onFrameFunPtr frameCB;
} Decoder;

// 解码
Decoder* init_decoder(enum AVCodecID codecID, onFrameFunPtr cb);
void close_decoder(Decoder *de);
int decode(Decoder *de, AVPacket *pkt);

#endif