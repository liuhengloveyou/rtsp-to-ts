#include "decoder.h"
#include "encoder.h"

static struct SwsContext *sws_ctx;

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

    return encode_video_frame(dstFrame);
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

    // if (sws_ctx == NULL)
    // {
    //     sws_ctx = sws_getContext(frame->width, frame->height, frame->format,
    //                              dstFrame->width, dstFrame->height, dstFrame->format,
    //                              SWS_BILINEAR, NULL, NULL, NULL);
    //     if (sws_ctx == NULL)
    //     {
    //         fprintf(stderr, "sws_getContext() failed");
    //         return NULL;
    //     }
    // }

    // // convert color space from YUV420 to RGBA
    // res = sws_scale(sws_ctx, (const uint8_t *const *)frame->data, frame->linesize, 0, frame->height,
    //                 dstFrame->data, dstFrame->linesize);
    // if (res < 0)
    // {
    //     fprintf(stderr, "sws_scale() failed");
    //     return NULL;
    // }

    return dstFrame;
}
