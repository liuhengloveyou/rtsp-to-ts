#ifndef UTILS_H
#define UTILS_H

#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/frame.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

AVFrame *frame_to_image(AVFrame *frame);
int send_to_encode(AVFrame *frame);

#endif