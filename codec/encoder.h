#ifndef ENCODER_H
#define ENCODER_H

#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/frame.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>


int init_mux(const char *filename);
int close_mux();
int encode_video_frame(AVFrame *frame);

#endif