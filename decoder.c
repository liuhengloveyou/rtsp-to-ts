#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/frame.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

#include "c_api.h"

static AVCodecContext *decoder_ctx;
static const AVCodec *encoder;
struct SwsContext *sws_ctx;

int init_decoder()
{
    sws_ctx = NULL;
    const AVCodec *decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (decoder == NULL)
    {
        fprintf(stderr, "avcodec_find_decoder() failed");
        return -1;
    }

    decoder_ctx = avcodec_alloc_context3(decoder);
    if (decoder_ctx == NULL)
    {
        fprintf(stderr, "avcodec_alloc_context3() failed");
        return -1;
    }

    int ret = avcodec_open2(decoder_ctx, decoder, NULL);
    if (ret < 0)
    {
        avcodec_close(decoder_ctx);
        fprintf(stderr, "avcodec_open2() failed");
        return -1;
    }

    return 0;
}

int close_decoder()
{
    if (sws_ctx)
    {
        sws_freeContext(sws_ctx);
    }

    avcodec_close(decoder_ctx);
}

int send_to_encode(AVFrame *frame)
{
    AVFrame *dstFrame = av_frame_alloc();
    if (!dstFrame)
    {
        fprintf(stderr, "av_frame_allo Error\n");
        exit(-1);
    }
    dstFrame->width = frame->width;
    dstFrame->height = frame->height;
    dstFrame->format = AV_PIX_FMT_YUV420P;
    dstFrame->color_range = AVCOL_RANGE_JPEG;
    int res = av_frame_get_buffer(dstFrame, 1);
    if (res < 0)
    {
        fprintf(stderr, "av_frame_get_buffer() failed");
        return res;
    }

    if (sws_ctx == NULL)
    {
        sws_ctx = sws_getContext(frame->width, frame->height, frame->format,
                                 dstFrame->width, dstFrame->height, dstFrame->format,
                                 SWS_BILINEAR, NULL, NULL, NULL);
        if (sws_ctx == NULL)
        {
            fprintf(stderr, "sws_getContext() failed");
            return -1;
        }
    }

    res = sws_scale(sws_ctx, (const uint8_t *const *)frame->data, frame->linesize, 0, frame->height,
                    dstFrame->data, dstFrame->linesize);
    if (res < 0)
    {
        fprintf(stderr, "sws_scale() failed");
        return res;
    }

    return mux_video_frame(dstFrame);
}

AVFrame *frame_to_image(AVFrame *frame)
{
    AVFrame *dstFrame = av_frame_alloc();
    if (!dstFrame)
    {
        fprintf(stderr, "av_frame_allo Error\n");
        exit(-1);
    }
    dstFrame->width = frame->width;
    dstFrame->height = frame->height;
    dstFrame->format = AV_PIX_FMT_RGBA;
    dstFrame->color_range = AVCOL_RANGE_JPEG;
    int res = av_frame_get_buffer(dstFrame, 1);
    if (res < 0)
    {
        fprintf(stderr, "av_frame_get_buffer() failed");
        return NULL;
    }

    if (sws_ctx == NULL)
    {
        sws_ctx = sws_getContext(frame->width, frame->height, frame->format,
                                 dstFrame->width, dstFrame->height, dstFrame->format,
                                 SWS_BILINEAR, NULL, NULL, NULL);
        if (sws_ctx == NULL)
        {
            fprintf(stderr, "sws_getContext() failed");
            return NULL;
        }
    }

    // convert color space from YUV420 to RGBA
    res = sws_scale(sws_ctx, (const uint8_t *const *)frame->data, frame->linesize, 0, frame->height,
                    dstFrame->data, dstFrame->linesize);
    if (res < 0)
    {
        fprintf(stderr, "sws_scale() failed");
        return NULL;
    }

    return dstFrame;
}
int decode(AVPacket *pkt, AVFrame **rstFrame)
{
    int res = avcodec_send_packet(decoder_ctx, pkt);
    if (res < 0)
    {
        fprintf(stderr, "avcodec_send_packet failed: %s\n", av_err2str(res));
        return res;
    }

    while (res >= 0)
    {
        AVFrame *frame = av_frame_alloc();
        if (!frame)
        {
            fprintf(stderr, "av_frame_allo Error\n");
            exit(-1);
        }

        res = avcodec_receive_frame(decoder_ctx, frame);
        if (res == AVERROR_EOF || res == AVERROR(EAGAIN))
        {
            continue;
        }
        else if (res < 0)
        {
            fprintf(stderr, "Error during decoding: %s\n", av_err2str(res));
            return res;
        }

        if (res >= 0)
        {
            printf("decode frame: %d %d %d\n", frame->width, frame->height, frame->format);

            // *rstFrame = frame_to_image(frame);
            send_to_encode(frame);
            break;
        }
    }
}