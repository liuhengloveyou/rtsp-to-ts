
#include "decoder.h"
#include "utils.h"

Decoder *init_decoder(enum AVCodecID codecID, onFrameFunPtr cb)
{
    // av_log_set_level(AV_LOG_DEBUG);
    
    Decoder *de = (Decoder *)malloc(sizeof(Decoder));
    if (!de)
    {
        fprintf(stderr, "malloc failed");
        return NULL;
    }
    memset(de, 0, sizeof(Decoder));

    // DEMO
    if (codecID == AV_CODEC_ID_H264) {
        de->frameCB = send_to_encode;
    }
    
    // de->frameCB = cb;

    const AVCodec *decoder = avcodec_find_decoder(codecID);
    if (decoder == NULL)
    {
        fprintf(stderr, "avcodec_find_decoder() failed");
        goto fail;
    }

    de->decoder_ctx = avcodec_alloc_context3(decoder);
    if (de->decoder_ctx == NULL)
    {
        fprintf(stderr, "avcodec_alloc_context3() failed");
        goto fail;
    }

    int ret = avcodec_open2(de->decoder_ctx, decoder, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "avcodec_open2() failed");
        goto fail;
    }

    return de;

fail:
    if (de)
    {
        if (de->decoder_ctx)
        {
            avcodec_close(de->decoder_ctx);
        }
        free(de);
    }

    return NULL;
}

void close_decoder(Decoder *de)
{
    if (de)
    {
        if (de->sws_ctx)
        {
            sws_freeContext(de->sws_ctx);
        }

        if (de->decoder_ctx)
        {
            avcodec_close(de->decoder_ctx);
        }
        free(de);
    }
}

int decode(Decoder *de, AVPacket *pkt)
{
    if (!de) {
        return -1;
    }

    int res = avcodec_send_packet(de->decoder_ctx, pkt);
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

        res = avcodec_receive_frame(de->decoder_ctx, frame);
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
            // send_to_encode(frame);
            // break;
            if (de->frameCB) {
                de->frameCB(frame);
            }
        }
    }
}