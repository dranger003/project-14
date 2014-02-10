#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <limits.h>

#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>

#include <ao/ao.h>

#define UNUSED(x)           (void)(x)

volatile sig_atomic_t quit = 0;

static void signal_handler(int signal)
{
    UNUSED(signal);
    quit = 1;
}

int main(int argc, char *argv[])
{
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    assert(sigaction(SIGINT, &sa, NULL) != -1);

    {
        av_register_all();

        avformat_network_init();
        avdevice_register_all();

        AVFormatContext *fc = NULL;
        AVDictionary *opts = NULL;
        av_dict_set(&opts, "seekable", "1", 0);
        int res = avformat_open_input(&fc, argv[1], NULL, &opts);
        assert(res == 0);
        av_dict_free(&opts);
        opts = NULL;

        int st_vid_idx = 0;
        AVCodec *vid_decoder = NULL;
        st_vid_idx = av_find_best_stream(fc, AVMEDIA_TYPE_VIDEO, -1, -1, &vid_decoder, 0);
        assert(st_vid_idx >= 0);
        assert(vid_decoder);

        int st_aud_idx = 0;
        AVCodec *aud_decoder = NULL;
        st_aud_idx = av_find_best_stream(fc, AVMEDIA_TYPE_AUDIO, -1, -1, &aud_decoder, 0);
        assert(st_aud_idx >= 0);
        assert (aud_decoder);

//        AVRational aspect_ratio = av_guess_sample_aspect_ratio(fc, fc->streams[st_vid_idx], NULL);
//        printf("aspect_ratio = %d/%d\n", aspect_ratio.num, aspect_ratio.den);

//        av_dump_format(fc, 0, "", 0);

        opts = NULL;
        av_dict_set(&opts, "refcounted_frames", "1", 0);
        res = avcodec_open2(fc->streams[st_vid_idx]->codec, vid_decoder, &opts);
        assert(res >=0);
        res = avcodec_open2(fc->streams[st_aud_idx]->codec, aud_decoder, &opts);
        assert(res >=0);
        av_dict_free(&opts);

        AVRational time_base = fc->streams[st_vid_idx]->time_base;
        AVRational frame_rate = av_guess_frame_rate(fc, fc->streams[st_vid_idx], 0);
        printf("%d/%d\n", time_base.num, time_base.den);
        printf("%d/%d\n", frame_rate.num, frame_rate.den);

        {
            res = avformat_seek_file(fc,
                                     st_aud_idx,
                                     INT64_MIN,
                                     atoi(argv[2]),//av_rescale(atoi(argv[2]), fc->streams[st_vid_idx]->time_base.den, fc->streams[st_vid_idx]->time_base.num),
                                     INT64_MAX,
                                     0);
//            res = av_seek_frame(fc, st_aud_idx, atoi(argv[2]),0);
            assert(res >= 0);
            avcodec_flush_buffers(fc->streams[st_vid_idx]->codec);
        }

//        AVFormatContext *ofc = NULL;
//        res = avformat_alloc_output_context2(&ofc, NULL, "alsa", "default");
//        assert(res >= 0);

//        AVStream *os = avformat_new_stream(ofc, 0);
//        os->codec->codec_type = AVMEDIA_TYPE_AUDIO;
//        os->codec->codec_id = CODEC_ID_PCM_S16LE;
//        os->codec->sample_rate = 48000;
//        os->codec->channels = 6;
//        os->codec->time_base.num = 1;
//        os->codec->time_base.den = os->codec->sample_rate;

//        res = avformat_write_header(ofc, NULL);
//        assert(res >= 0);

//        AVPacket opkt = { 0 };

        AVPacket pkt = { 0 };
        AVFrame *frame = av_frame_alloc();
        int got_frame = 0;

        ao_initialize();

        ao_sample_format sample_fmt = { 0 };
        sample_fmt.bits = 16;
        sample_fmt.channels = fc->streams[st_aud_idx]->codec->channels;
        sample_fmt.rate = fc->streams[st_aud_idx]->codec->sample_rate;
        sample_fmt.byte_format = AO_FMT_NATIVE;

        int driver = ao_default_driver_id();
        ao_device *device = ao_open_live(driver, &sample_fmt, NULL);

        // AVCODEC_MAX_AUDIO_FRAME_SIZE + FF_INPUT_BUFFER_PADDING_SIZE;
        uint8_t samples[192000 + 16];
//        uint8_t *buf = samples;

//        SwrContext *swr_ctx = swr_alloc();
//        av_opt_set_int(swr_ctx, "in_channel_layout", fc->streams[st_aud_idx]->codec->channel_layout, 0);
//        av_opt_set_int(swr_ctx, "out_channel_layout", fc->streams[st_aud_idx]->codec->channel_layout, 0);
//        av_opt_set_int(swr_ctx, "in_channel_count", fc->streams[st_aud_idx]->codec->channels, 0);
//        av_opt_set_int(swr_ctx, "out_channel_count", fc->streams[st_aud_idx]->codec->channels, 0);
//        av_opt_set_int(swr_ctx, "in_sample_rate", fc->streams[st_aud_idx]->codec->sample_rate, 0);
//        av_opt_set_int(swr_ctx, "out_sample_rate", fc->streams[st_aud_idx]->codec->sample_rate, 0);
//        av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt",  AV_SAMPLE_FMT_FLTP, 0);
//        av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16,  0);
//        res = swr_init(swr_ctx);
//        assert(res >= 0);

        system("setterm -cursor off");

        struct timespec a, b;
        clock_gettime(CLOCK_MONOTONIC, &a);

        while (!quit) {
            av_free_packet(&pkt);

            res = av_read_frame(fc, &pkt);
            assert(res == 0);

            if (pkt.stream_index == st_aud_idx) {
                int size, i = 0, j, k;

                while (pkt.size > 0) {
                    size = avcodec_decode_audio4(fc->streams[st_aud_idx]->codec, frame, &got_frame, &pkt);
                    assert(size >= 0);

                    // Avoid decoder packet over-read
                    size = FFMIN(size, pkt.size);

                    if (got_frame) {
                        assert(fc->streams[st_aud_idx]->codec->sample_fmt == AV_SAMPLE_FMT_FLTP);

                        for (j = 0; j < frame->nb_samples; j++)
                            for (k = 0; k < fc->streams[st_aud_idx]->codec->channels; k++)
                                ((uint16_t *)samples)[i++] = ((float *)frame->extended_data[k])[j] * SHRT_MAX;

//                        res = swr_convert(swr_ctx,
//                                          &buf,
//                                          frame->nb_samples,
//                                          (const uint8_t **)frame->extended_data,
//                                          frame->nb_samples);
//                        assert(res >= 0);

//                        opkt.size = frame->nb_samples * fc->streams[st_aud_idx]->codec->channels * sizeof(uint16_t);
//                        opkt.data = samples;
//                        opkt.dts = opkt.pts = pkt.pts;
//                        opkt.duration = pkt.duration;

//                        res = av_write_frame(ofc, &opkt);
//                        assert(res >= 0);

                        long double pts = (long double)pkt.pts / 1000.0;
                        long double ela = (b.tv_sec - a.tv_sec) + ((long double)(b.tv_nsec - a.tv_nsec) / 1000000000);
                        long double dif = pts - ela;

                        clock_gettime(CLOCK_MONOTONIC, &b);
                        fprintf(stdout,
                                "PTS: %.6Lf, E: %.6Lf, D: %.6Lf              \r",
                                pts,
                                ela,
                                dif);
                        fflush(stdout);

                        ao_play(device,
                                (char *)samples,
                                (uint_32)(frame->nb_samples * fc->streams[st_aud_idx]->codec->channels * sizeof(uint16_t)));

//                        switch (sfmt) {
//                            case AV_SAMPLE_FMT_S16P:
//                                for (int nb=0;nb<plane_size/sizeof(uint16_t);nb++){
//                                    for (int ch = 0; ch < ctx->channels; ch++) {
//                                        out[write_p] = ((uint16_t *) frame->extended_data[ch])[nb];
//                                        write_p++;
//                                    }
//                                }
//                                ao_play(adevice, (char*)samples, (plane_size) * ctx->channels  );
//                                break;
//                            case AV_SAMPLE_FMT_S16:
//                                ao_play(adevice, (char*)frame->extended_data[0],frame->linesize[0] );
//                                break;
//                            case AV_SAMPLE_FMT_FLT:
//                                for (int nb=0;nb<plane_size/sizeof(float);nb++){
//                                    out[nb] = static_cast<short> ( ((float *) frame->extended_data[0])[nb] * std::numeric_limits<short>::max() );
//                                }
//                                ao_play(adevice, (char*)samples, ( plane_size/sizeof(float) )  * sizeof(uint16_t) );
//                                break;
//                            case AV_SAMPLE_FMT_U8P:
//                                for (int nb=0;nb<plane_size/sizeof(uint8_t);nb++){
//                                    for (int ch = 0; ch < ctx->channels; ch++) {
//                                        out[write_p] = ( ((uint8_t *) frame->extended_data[0])[nb] - 127) * std::numeric_limits<short>::max() / 127 ;
//                                        write_p++;
//                                    }
//                                }
//                                ao_play(adevice, (char*)samples, ( plane_size/sizeof(uint8_t) )  * sizeof(uint16_t) * ctx->channels );
//                                break;
//                            case AV_SAMPLE_FMT_U8:
//                                for (int nb=0;nb<plane_size/sizeof(uint8_t);nb++){
//                                    out[nb] = static_cast<short> ( ( ((uint8_t *) frame->extended_data[0])[nb] - 127) * std::numeric_limits<short>::max() / 127 );
//                                }
//                                ao_play(adevice, (char*)samples, ( plane_size/sizeof(uint8_t) )  * sizeof(uint16_t) );
//                                break;
//                            default:
//                                DBG("PCM type not supported");
//                        }

                    }

                    pkt.size -= size;
                    pkt.data += size;
                }
            }

//            if (pkt.stream_index == st_vid_idx) {
//                res = avcodec_decode_video2(fc->streams[st_vid_idx]->codec, frame, &got_frame, &pkt);
//                assert(res >= 0);

//                if (got_frame) {
//                    assert(fc->streams[st_vid_idx]->codec->pix_fmt == AV_PIX_FMT_YUV420P);

//                    double duration = (frame_rate.num && frame_rate.den ? av_q2d((AVRational){ frame_rate.den, frame_rate.num }) : 0);
//                    frame->pts = av_frame_get_best_effort_timestamp(frame);
//                    double pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(time_base);

//                    printf(
//                        "duration = %.10f, pts = %.10f, %d,%d,%d,%d,%d,%d,%d,%d\n",
//                        duration,
//                        pts,
//                        frame->linesize[0],
//                        frame->linesize[1],
//                        frame->linesize[2],
//                        frame->linesize[3],
//                        frame->linesize[4],
//                        frame->linesize[5],
//                        frame->linesize[6],
//                        frame->linesize[7]
//                    );

//                    {
//                        uint8_t *video_dst_data[4] = { 0 };
//                        int video_dst_linesize[4];
//                        int video_dst_bufsize;

//                        res = av_image_alloc(video_dst_data,
//                                             video_dst_linesize,
//                                             fc->streams[st_vid_idx]->codec->width,
//                                             fc->streams[st_vid_idx]->codec->height,
//                                             fc->streams[st_vid_idx]->codec->pix_fmt,
//                                             1);
//                        assert(res >= 0);
//                        video_dst_bufsize = res;
//                        printf("video_dst_bufsize = %d\n", video_dst_bufsize);

//                        av_image_copy(video_dst_data,
//                                      video_dst_linesize,
//                                      (const uint8_t **)(frame->data),
//                                      frame->linesize,
//                                      fc->streams[st_vid_idx]->codec->pix_fmt,
//                                      fc->streams[st_vid_idx]->codec->width,
//                                      fc->streams[st_vid_idx]->codec->height);

//                        FILE *f = fopen("frame.yuv", "wb");
//                        assert(f);
//                        fwrite(video_dst_data[0], 1, video_dst_bufsize, f);
//                        fclose(f);
//                    }
//                }

//                av_frame_unref(frame);
//            }
        }

        system("setterm -cursor on");

//        av_write_trailer(ofc);

        ao_shutdown();

        av_free_packet(&pkt);
        av_frame_free(&frame);

        avformat_close_input(&fc);

        avformat_network_deinit();
    }

    printf("Done.\n");

    return 0;
}
