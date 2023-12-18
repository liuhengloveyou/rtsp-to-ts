#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#include "encoder.h"

#define STREAM_DURATION 10.0

// a wrapper around a single output AVStream
typedef struct OutputStream
{
    AVStream *st;
    AVCodecContext *enc;

    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;

    AVFrame *frame;
    AVFrame *tmp_frame;

    AVPacket *tmp_pkt;

    float t, tincr, tincr2;

    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
} OutputStream;

static const AVOutputFormat *ofmt;
static AVFormatContext *ofmt_ctx = NULL;
static OutputStream video_st = {0}, audio_st = {0};

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

    printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           pkt->stream_index);
}

static int write_frame(AVFormatContext *fmt_ctx, AVCodecContext *encode_ctx, AVStream *st, AVFrame *frame, AVPacket *pkt)
{
    int ret = 0;

    // send the frame to the encoder
    ret = avcodec_send_frame(encode_ctx, frame);
    if (ret < 0)
    {
        fprintf(stderr, "Error sending a frame to the encoder: %s\n", av_err2str(ret));
        exit(1);
    }

    while (ret >= 0)
    {
        ret = avcodec_receive_packet(encode_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            fprintf(stderr, "Error encoding a frame: %s\n", av_err2str(ret));
            exit(1);
        }

        av_packet_rescale_ts(pkt, encode_ctx->time_base, st->time_base);
        pkt->stream_index = st->index;

        log_packet(fmt_ctx, pkt);
        ret = av_interleaved_write_frame(fmt_ctx, pkt);
        if (ret < 0)
        {
            fprintf(stderr, "Error while writing output packet: %s\n", av_err2str(ret));
            exit(1);
        }
    }

    return ret == AVERROR_EOF ? 1 : 0;
}

static void add_stream(OutputStream *ost, AVFormatContext *ofmt_ctx, const AVCodec **codec, enum AVCodecID codec_id)
{
    int i = 0;
    AVCodecContext *c = NULL;

    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec))
    {
        fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
        exit(1);
    }

    ost->tmp_pkt = av_packet_alloc();
    if (!ost->tmp_pkt)
    {
        fprintf(stderr, "Could not allocate AVPacket\n");
        exit(1);
    }

    ost->st = avformat_new_stream(ofmt_ctx, NULL);
    if (!ost->st)
    {
        fprintf(stderr, "Could not allocate stream\n");
        exit(1);
    }
    ost->st->id = ofmt_ctx->nb_streams - 1;
    c = avcodec_alloc_context3(*codec);
    if (!c)
    {
        fprintf(stderr, "Could not alloc an encoding context\n");
        exit(1);
    }
    ost->enc = c;

    switch ((*codec)->type)
    {
    case AVMEDIA_TYPE_AUDIO:
        c->sample_fmt = (*codec)->sample_fmts ? (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        c->bit_rate = 64000;
        c->sample_rate = 44100;
        if ((*codec)->supported_samplerates)
        {
            c->sample_rate = (*codec)->supported_samplerates[0];
            for (i = 0; (*codec)->supported_samplerates[i]; i++)
            {
                if ((*codec)->supported_samplerates[i] == 44100)
                    c->sample_rate = 44100;
            }
        }
        av_channel_layout_copy(&c->ch_layout, &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO);
        ost->st->time_base = (AVRational){1, c->sample_rate};
        break;

    case AVMEDIA_TYPE_VIDEO:
        c->codec_id = codec_id;

        c->bit_rate = 400000;
        c->width = 1920;
        c->height = 1080;
        ost->st->time_base = (AVRational){1, 25};
        c->time_base = ost->st->time_base;

        c->gop_size = 12;
        c->pix_fmt = AV_PIX_FMT_YUV420P;
        if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO)
        {
            c->max_b_frames = 2;
        }
        if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO)
        {
            c->mb_decision = 2;
        }
        break;

    default:
        break;
    }

    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
                                  const AVChannelLayout *channel_layout,
                                  int sample_rate,
                                  int nb_samples)
{
    AVFrame *frame = av_frame_alloc();
    if (!frame)
    {
        fprintf(stderr, "Error allocating an audio frame\n");
        exit(1);
    }

    frame->format = sample_fmt;
    av_channel_layout_copy(&frame->ch_layout, channel_layout);
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;

    if (nb_samples)
    {
        if (av_frame_get_buffer(frame, 0) < 0)
        {
            fprintf(stderr, "Error allocating an audio buffer\n");
            exit(1);
        }
    }

    return frame;
}

static void open_audio(AVFormatContext *oc, const AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
    AVCodecContext *c;
    int nb_samples;
    int ret;
    AVDictionary *opt = NULL;

    c = ost->enc;

    /* open it */
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0)
    {
        fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret));
        exit(1);
    }

    /* init signal generator */
    ost->t = 0;
    ost->tincr = 2 * M_PI * 110.0 / c->sample_rate;
    /* increment frequency by 110 Hz per second */
    ost->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;

    if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
        nb_samples = 10000;
    else
        nb_samples = c->frame_size;

    ost->frame = alloc_audio_frame(c->sample_fmt, &c->ch_layout, c->sample_rate, nb_samples);
    ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, &c->ch_layout, c->sample_rate, nb_samples);

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0)
    {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }

    /* create resampler context */
    ost->swr_ctx = swr_alloc();
    if (!ost->swr_ctx)
    {
        fprintf(stderr, "Could not allocate resampler context\n");
        exit(1);
    }

    /* set options */
    av_opt_set_chlayout(ost->swr_ctx, "in_chlayout", &c->ch_layout, 0);
    av_opt_set_int(ost->swr_ctx, "in_sample_rate", c->sample_rate, 0);
    av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    av_opt_set_chlayout(ost->swr_ctx, "out_chlayout", &c->ch_layout, 0);
    av_opt_set_int(ost->swr_ctx, "out_sample_rate", c->sample_rate, 0);
    av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt", c->sample_fmt, 0);

    /* initialize the resampling context */
    if ((ret = swr_init(ost->swr_ctx)) < 0)
    {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        exit(1);
    }
}

static AVFrame *get_audio_frame(OutputStream *ost)
{
    AVFrame *frame = ost->tmp_frame;
    int j, i, v;
    int16_t *q = (int16_t *)frame->data[0];

    /* check if we want to generate more frames */
    if (av_compare_ts(ost->next_pts, ost->enc->time_base, STREAM_DURATION, (AVRational){1, 1}) > 0)
        return NULL;

    for (j = 0; j < frame->nb_samples; j++)
    {
        v = (int)(sin(ost->t) * 10000);
        for (i = 0; i < ost->enc->ch_layout.nb_channels; i++)
            *q++ = v;
        ost->t += ost->tincr;
        ost->tincr += ost->tincr2;
    }

    frame->pts = ost->next_pts;
    ost->next_pts += frame->nb_samples;

    return frame;
}

static int write_audio_frame(AVFormatContext *ofmt_ctx, OutputStream *ost, AVFrame *frame)
{
    int ret;
    int dst_nb_samples;

    AVCodecContext *encode_ctx = ost->enc;

    dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, encode_ctx->sample_rate) + frame->nb_samples,
                                    encode_ctx->sample_rate, encode_ctx->sample_rate, AV_ROUND_UP);
    av_assert0(dst_nb_samples == frame->nb_samples);

    ret = av_frame_make_writable(ost->frame);
    if (ret < 0)
        exit(1);

    /* convert to destination format */
    ret = swr_convert(ost->swr_ctx,
                      ost->frame->data, dst_nb_samples,
                      (const uint8_t **)frame->data, frame->nb_samples);
    if (ret < 0)
    {
        fprintf(stderr, "Error while converting\n");
        exit(1);
    }
    frame = ost->frame;

    frame->pts = av_rescale_q(ost->samples_count, (AVRational){1, encode_ctx->sample_rate}, encode_ctx->time_base);
    ost->samples_count += dst_nb_samples;

    return write_frame(ofmt_ctx, encode_ctx, ost->st, frame, ost->tmp_pkt);
}

static AVFrame *alloc_frame(enum AVPixelFormat pix_fmt, int width, int height)
{
    int ret;

    AVFrame *frame = av_frame_alloc();
    if (!frame)
        return NULL;

    frame->format = pix_fmt;
    frame->width = width;
    frame->height = height;

    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0)
    {
        fprintf(stderr, "Could not allocate frame data.\n");
        exit(1);
    }

    return frame;
}

static void open_video(AVFormatContext *ofmt_ctx,
                       const AVCodec *codec,
                       OutputStream *ost,
                       AVDictionary *opt_arg)
{
    int ret;
    AVCodecContext *c = ost->enc;
    AVDictionary *opt = NULL;

    av_dict_copy(&opt, opt_arg, 0);

    /* open the codec */
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0)
    {
        fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
        exit(1);
    }

    /* allocate and init a re-usable frame */
    ost->frame = alloc_frame(c->pix_fmt, c->width, c->height);
    if (!ost->frame)
    {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }

    ost->tmp_frame = NULL;
    if (c->pix_fmt != AV_PIX_FMT_YUV420P)
    {
        ost->tmp_frame = alloc_frame(AV_PIX_FMT_YUV420P, c->width, c->height);
        if (!ost->tmp_frame)
        {
            fprintf(stderr, "Could not allocate temporary video frame\n");
            exit(1);
        }
    }

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0)
    {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }
}

/* Prepare a dummy image. */
static void fill_yuv_image(AVFrame *pict, int frame_index, int width, int height)
{
    int x, y, i;

    i = frame_index;

    /* Y */
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;

    /* Cb and Cr */
    for (y = 0; y < height / 2; y++)
    {
        for (x = 0; x < width / 2; x++)
        {
            pict->data[1][y * pict->linesize[1] + x] = 128 + y + i * 2;
            pict->data[2][y * pict->linesize[2] + x] = 64 + x + i * 5;
        }
    }
}

static AVFrame *get_video_frame(OutputStream *ost)
{
    AVCodecContext *codec_ctx = ost->enc;

    if (av_frame_make_writable(ost->frame) < 0)
        exit(1);

    if (codec_ctx->pix_fmt != AV_PIX_FMT_YUV420P)
    {
        if (!ost->sws_ctx)
        {
            ost->sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height,
                                          AV_PIX_FMT_YUV420P,
                                          codec_ctx->width, codec_ctx->height,
                                          codec_ctx->pix_fmt,
                                          SWS_BICUBIC, NULL, NULL, NULL);
            if (!ost->sws_ctx)
            {
                fprintf(stderr, "Could not initialize the conversion context\n");
                exit(1);
            }
        }
        fill_yuv_image(ost->tmp_frame, ost->next_pts, codec_ctx->width, codec_ctx->height);
        sws_scale(ost->sws_ctx, (const uint8_t *const *)ost->tmp_frame->data,
                  ost->tmp_frame->linesize, 0, codec_ctx->height, ost->frame->data,
                  ost->frame->linesize);
    }
    else
    {
        fill_yuv_image(ost->frame, ost->next_pts, codec_ctx->width, codec_ctx->height);
    }

    ost->frame->pts = ost->next_pts++;

    return ost->frame;
}

static int write_video_frame(AVFormatContext *ofmt_ctx, OutputStream *ost, AVFrame *frame)
{
    /* check if we want to generate more frames */
    if (av_compare_ts(ost->next_pts, ost->enc->time_base, STREAM_DURATION, (AVRational){1, 1}) > 0)
    {
        close_mux();
        exit(1);
    }

    return write_frame(ofmt_ctx, ost->enc, ost->st, frame, ost->tmp_pkt);
}

static void close_stream(AVFormatContext *oc, OutputStream *ost)
{
    avcodec_free_context(&ost->enc);
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    av_packet_free(&ost->tmp_pkt);
    sws_freeContext(ost->sws_ctx);
    swr_free(&ost->swr_ctx);
}

int init_mux(const char *filename)
{
    const AVCodec *audio_codec, *video_codec;
    int ret;
    AVDictionary *opt = NULL;
    int i;

    /* allocate the output media context */
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
    if (!ofmt_ctx)
    {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        avformat_alloc_output_context2(&ofmt_ctx, NULL, "mpeg", filename);
    }
    if (!ofmt_ctx)
        return -1;

    ofmt = ofmt_ctx->oformat;

    // 必须有音频和视频
    if (ofmt->video_codec == AV_CODEC_ID_NONE && ofmt->audio_codec == AV_CODEC_ID_NONE)
    {
        return -1;
    }

    if (ofmt->video_codec != AV_CODEC_ID_NONE)
    {
        add_stream(&video_st, ofmt_ctx, &video_codec, ofmt->video_codec);
    }
    if (ofmt->audio_codec != AV_CODEC_ID_NONE)
    {
        add_stream(&audio_st, ofmt_ctx, &audio_codec, ofmt->audio_codec);
    }

    open_video(ofmt_ctx, video_codec, &video_st, opt);
    open_audio(ofmt_ctx, audio_codec, &audio_st, opt);

    av_dump_format(ofmt_ctx, 0, filename, 1);

    /* open the output file */
    if (!(ofmt->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            fprintf(stderr, "Could not open '%s': %s\n", filename, av_err2str(ret));
            return 1;
        }
    }

    /* Write the stream header, if any. */
    ret = avformat_write_header(ofmt_ctx, &opt);
    if (ret < 0)
    {
        fprintf(stderr, "Error occurred when opening output file: %s\n", av_err2str(ret));
        return 1;
    }

    printf("init_mux: %s\n", filename);

    return 0;
}

int close_mux()
{
    av_write_trailer(ofmt_ctx);

    /* Close each codec. */
    close_stream(ofmt_ctx, &video_st);
    close_stream(ofmt_ctx, &audio_st);

    if (!(ofmt->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);

    /* free the stream */
    avformat_free_context(ofmt_ctx);
}

int encode_video_frame(AVFrame *frame)
{
    if (frame == NULL)
    {
        frame = get_video_frame(&video_st);
    }
    
    
    frame->pts = video_st.next_pts++;
    
    return write_video_frame(ofmt_ctx, &video_st, frame);
}

int mux()
{
    int rst = 0;

    while (1)
    {
        /* select the stream to encode */
        if (av_compare_ts(video_st.next_pts, video_st.enc->time_base,
                          audio_st.next_pts, audio_st.enc->time_base) <= 0)
        {
            AVFrame *frame = get_video_frame(&video_st);

            rst = write_video_frame(ofmt_ctx, &video_st, frame);
        }
        else
        {
            AVFrame *frame = get_audio_frame(&audio_st);
            int rst = write_audio_frame(ofmt_ctx, &audio_st, frame);
        }
        if (rst)
        {
            break;
        }
    }
}