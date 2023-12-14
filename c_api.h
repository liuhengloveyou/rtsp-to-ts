#ifndef C_API_H
#define C_API_H

#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

// 解码
int init_decoder();
int close_decoder();
int decode(AVPacket *pkt, AVFrame **rstFrame);

// 封装
int init_mux(const char *filename);
int close_mux();
int mux_video_frame(AVFrame *frame);

#endif