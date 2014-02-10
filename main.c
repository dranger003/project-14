#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <limits.h>

#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>

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

        AVFormatContext *ic = NULL;
        AVDictionary *opts = NULL;
        av_dict_set(&opts, "seekable", "1", 0);
        int res = avformat_open_input(&ic, argv[1], NULL, &opts);
        assert(res == 0);
        av_dict_free(&opts);
        opts = NULL;

        int st_vid_idx = 0;
        AVCodec *vid_decoder = NULL;
        st_vid_idx = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, &vid_decoder, 0);
        assert(st_vid_idx >= 0);
        assert(vid_decoder);

        int st_aud_idx = 0;
        AVCodec *aud_decoder = NULL;
        st_aud_idx = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, &aud_decoder, 0);
        assert(st_aud_idx >= 0);
        assert (aud_decoder);

//        AVRational aspect_ratio = av_guess_sample_aspect_ratio(ic, ic->streams[st_vid_idx], NULL);
//        printf("aspect_ratio = %d/%d\n", aspect_ratio.num, aspect_ratio.den);

//        av_dump_format(ic, 0, "", 0);

        opts = NULL;
        av_dict_set(&opts, "refcounted_frames", "1", 0);
        res = avcodec_open2(ic->streams[st_vid_idx]->codec, vid_decoder, &opts);
        assert(res >=0);
        res = avcodec_open2(ic->streams[st_aud_idx]->codec, aud_decoder, &opts);
        assert(res >=0);
        av_dict_free(&opts);

        AVRational time_base = ic->streams[st_vid_idx]->time_base;
        AVRational frame_rate = av_guess_frame_rate(ic, ic->streams[st_vid_idx], 0);
        printf("%d/%d\n", time_base.num, time_base.den);
        printf("%d/%d\n", frame_rate.num, frame_rate.den);

//        {
//            res = avformat_seek_file(ic,
//                                     st_vid_idx,
//                                     INT64_MIN,
//                                     atoi(argv[2]),//av_rescale(atoi(argv[2]), ic->streams[st_vid_idx]->time_base.den, ic->streams[st_vid_idx]->time_base.num),
//                                     INT64_MAX,
//                                     AVSEEK_FLAG_ANY);
//            assert(res >= 0);
//            avcodec_flush_buffers(ic->streams[st_vid_idx]->codec);
//        }

//        AVFormatContext *oc = NULL;
//        res = avformat_alloc_output_context2(&oc, NULL, "alsa", "default");
//        assert(res >= 0);

//        AVStream *os = avformat_new_stream(oc, 0);
//        os->codec->codec_type = AVMEDIA_TYPE_AUDIO;
//        os->codec->codec_id = CODEC_ID_PCM_S16LE;
//        os->codec->sample_rate = 48000;
//        os->codec->channels = 6;
//        os->codec->time_base.num = 1;
//        os->codec->time_base.den = os->codec->sample_rate;

//        res = avformat_write_header(oc, NULL);
//        assert(res >= 0);

        AVPacket pkt = { 0 };
        AVFrame *frame = av_frame_alloc();
        int got_frame = 0;

//        AVPacket opkt = { 0 };
//        av_init_packet(&opkt);

//        const unsigned int rate = 48000;
//        const unsigned int chunk = 960;
//        unsigned int old = -1, cur;
//        int64_t dts, wall;
//        unsigned int i, j, k;
//        const unsigned int period[2] = { 60, 30 };

//        int16_t *buf[2];
//        buf[0] = av_malloc(rate * 2 * sizeof(int16_t) * 2);
//        buf[1] = buf[0] + rate * 2;
//        memset(buf[0], 0, rate * 2 * sizeof(int16_t) * 2);
//        for (i = 0; i < 2; i++)
//            for (j = 0; j < rate; j++)
//                buf[i][j * 2 + i] = 10000 * sin(2 * M_PI * j / period[i]);

//        i = j = 0;
//        for (k = 0; k < 50 * 2 * 3; k++) {
//            opkt.data = (void *)(buf[i] + j * 2);
//            opkt.size = chunk * 2 * sizeof(int16_t);
//            av_write_frame(oc, &opkt);
//            opkt.pts = opkt.dts += chunk;
//            j += chunk;
//            if (j == rate) {
//                j = 0;
//                i = 1 - i;
//            }
//            av_get_output_timestamp(oc, 0, &dts, &wall);
//            cur = (dts / rate) & 1;
//            if (old != cur) {
//                old = cur;
//                printf(cur == 0 ? "<--     \n" : "     -->\n");
//            }
//        }

        ao_initialize();

        ao_sample_format sample_fmt = { 0 };
        sample_fmt.bits = 16;
        sample_fmt.channels = ic->streams[st_aud_idx]->codec->channels;
        sample_fmt.rate = ic->streams[st_aud_idx]->codec->sample_rate;
        sample_fmt.byte_format = AO_FMT_NATIVE;

        int driver = ao_default_driver_id();
        ao_device *device = ao_open_live(driver, &sample_fmt, NULL);

        // AVCODEC_MAX_AUDIO_FRAME_SIZE + FF_INPUT_BUFFER_PADDING_SIZE;
        uint8_t samples[192000 + 16];

        while (!quit) {
            av_free_packet(&pkt);

            res = av_read_frame(ic, &pkt);
            assert(res == 0);

            if (pkt.stream_index == st_aud_idx) {
                res = avcodec_decode_audio4(ic->streams[st_aud_idx]->codec, frame, &got_frame, &pkt);
                assert(res >= 0);

                int plane_size;
                av_samples_get_buffer_size(&plane_size,
                                           ic->streams[st_aud_idx]->codec->channels,
                                           frame->nb_samples,
                                           ic->streams[st_aud_idx]->codec->sample_fmt,
                                           1);

                uint16_t *out = (uint16_t *)samples;

                if (got_frame) {
                    assert(ic->streams[st_aud_idx]->codec->sample_fmt == AV_SAMPLE_FMT_FLTP);

                    int p = 0, nb, ch;
                    for (nb = 0; nb < plane_size / sizeof(float); nb++) {
                        for (ch = 0; ch < ic->streams[st_aud_idx]->codec->channels; ch++) {
                            out[p] = ((float *)frame->extended_data[ch])[nb] * SHRT_MAX ;
                            p++;
                        }
                    }

                    ao_play(device,
                            (char *)samples,
                            (plane_size / sizeof(float)) * sizeof(uint16_t) * ic->streams[st_aud_idx]->codec->channels);
               }
            }

//            if (pkt.stream_index == st_vid_idx) {
//                res = avcodec_decode_video2(ic->streams[st_vid_idx]->codec, frame, &got_frame, &pkt);
//                assert(res >= 0);

//                if (got_frame) {
//                    assert(ic->streams[st_vid_idx]->codec->pix_fmt == AV_PIX_FMT_YUV420P);

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
//                                             ic->streams[st_vid_idx]->codec->width,
//                                             ic->streams[st_vid_idx]->codec->height,
//                                             ic->streams[st_vid_idx]->codec->pix_fmt,
//                                             1);
//                        assert(res >= 0);
//                        video_dst_bufsize = res;
//                        printf("video_dst_bufsize = %d\n", video_dst_bufsize);

//                        av_image_copy(video_dst_data,
//                                      video_dst_linesize,
//                                      (const uint8_t **)(frame->data),
//                                      frame->linesize,
//                                      ic->streams[st_vid_idx]->codec->pix_fmt,
//                                      ic->streams[st_vid_idx]->codec->width,
//                                      ic->streams[st_vid_idx]->codec->height);

//                        FILE *f = fopen("frame.yuv", "wb");
//                        assert(f);
//                        fwrite(video_dst_data[0], 1, video_dst_bufsize, f);
//                        fclose(f);
//                    }
//                }

//                av_frame_unref(frame);
//            }
        }

//        av_write_trailer(oc);

        ao_shutdown();

        av_free_packet(&pkt);
        av_frame_free(&frame);

        avformat_close_input(&ic);

        avformat_network_deinit();
    }

    printf("Done.\n");

    return 0;
}
