#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include <linux/input.h>

#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>

#include <ao/ao.h>
#include <dbus/dbus.h>

#define UNUSED(x)           (void)(x)

volatile sig_atomic_t quit = 0;

static void signal_handler(int signal)
{
    UNUSED(signal);
    quit = 1;
}

//static void *thread_start(void *arg)
//{
//    sleep(5);
//    return NULL;
//}

int main(int argc, char *argv[])
{
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    assert(sigaction(SIGINT, &sa, NULL) != -1);
    assert(sigaction(SIGTERM, &sa, NULL) != -1);

    {
//        FILE *fd = fopen("/dev/input/event0", "rb");
//        struct input_event ev;
//        while (!quit) {
//            fread(&ev, sizeof(ev), 1, fd);
//            if (ev.type == EV_KEY && ev.value == 1) {
//                fprintf(stdout, "c:%d,v:%d\n", ev.code, ev.value);
//                fflush(stdout);
//            }
//        }
//        fclose(fd);

//    {
//        pthread_t thread;
//        pthread_create(&thread, 0, &thread_start, NULL);
//        pthread_join(thread, 0);
//    }

        if (strcmp(argv[1], "receive") == 0) {
            DBusError err;
            dbus_error_init(&err);

//            DBusConnection *bus = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
            DBusConnection *bus = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
            if (dbus_error_is_set(&err)) {
                printf("%s\n", err.message);
                dbus_error_free(&err);
                assert(0);
            }

//            dbus_connection_set_exit_on_disconnect(bus, FALSE);

            int res = dbus_bus_request_name(bus,
                                            "org.formatique.MediaPlayer",
                                            0,
                                            &err);
            //assert(res == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER);
            if (dbus_error_is_set(&err)) {
                printf("%s\n", err.message);
                dbus_error_free(&err);
                assert(0);
            }

//            dbus_bus_add_match(bus, "type='signal',interface='org.formatique.signal.Type'", &err);
//            dbus_connection_flush(bus);
//            assert(!dbus_error_is_set(&err));

            DBusMessage *msg;
//            DBusMessageIter arg;
//            char *value;

            while (!quit) {
                dbus_connection_read_write_dispatch(bus, 0);

                msg = dbus_connection_pop_message(bus);
                if (msg) {
                    if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Introspectable", "Introspect")) {
                        DBusMessage *reply = dbus_message_new_method_return(msg);
                        DBusMessageIter args;
                        dbus_message_iter_init_append(reply, &args);

                        static const char *xml =
                            DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE
                            "<node>                                                     "
                            "  <interface name=\"org.formatique.MediaPlayer\">			"
                            "    <method name=\"Ping\">									"
                            "      <arg direction=\"out\" type=\"s\" />					"
                            "    </method>												"
                            "    <method name=\"Key\">                                  "
                            "      <arg direction=\"in\" type=\"i\" />                  "
                            "    </method>                                              "
                            "  </interface>												"
                            "  <interface name=\"org.freedesktop.DBus.Introspectable\"> "
                            "    <method name=\"Introspect\">                           "
                            "      <arg direction=\"out\" type=\"s\" />					"
                            "    </method>                                              "
                            "  </interface>                                             "
                            "</node>                                                    ";
                        res = dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, (const void *)&xml);
                        assert(res);

                        dbus_uint32_t serial = 0;
                        res = dbus_connection_send(bus, reply, &serial);
                        assert(res);

                        dbus_connection_flush(bus);
                        dbus_message_unref(reply);
                    }
                    else if (dbus_message_is_method_call(msg, "org.formatique.MediaPlayer", "Ping")) {
                        DBusMessage *reply = dbus_message_new_method_return(msg);
                        DBusMessageIter args;
                        dbus_message_iter_init_append(reply, &args);
                        static const char *value = "Ping()";
                        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, (const void *)&value);

                        dbus_uint32_t serial = 0;
                        dbus_connection_send(bus, reply, &serial);

                        printf("Ping()\n");

                        dbus_connection_flush(bus);
                        dbus_message_unref(reply);
                    }
                    else if (dbus_message_is_method_call(msg, "org.formatique.MediaPlayer", "Key")) {
                        DBusMessageIter args;
                        dbus_message_iter_init(msg, &args);
                        assert(dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_INT32);
                        dbus_int32_t param;
                        dbus_message_iter_get_basic(&args, &param);
                        printf("0x%02x\n", param);

                        if (param == 0x1b) // ESC
                            quit = 1;
                    }

//                    if (dbus_message_is_signal(msg, "org.formatique.signal.Type", "QUIT")) {
//                        res = dbus_message_iter_init(msg, &arg);
//                        assert(res);

//                        dbus_message_iter_get_arg_type(&arg);
//                        dbus_message_iter_get_basic(&arg, &value);

//                        fprintf(stdout, "%s\n", value);
//                        fflush(stdout);

//                        if (strcmp(value, "QUIT") == 0)
//                            quit = 1;
//                    }

                    dbus_message_unref(msg);
                }

                usleep(1);
            }

//            dbus_connection_close(bus);
            dbus_connection_unref(bus);
        }
        else if (strcmp(argv[1], "send") == 0) {
            DBusError err;
            dbus_error_init(&err);

            DBusConnection *bus = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
            if (dbus_error_is_set(&err)) {
                printf("%s\n", err.message);
                dbus_error_free(&err);
                assert(0);
            }

            {
                struct termios o;
                tcgetattr(STDIN_FILENO, &o);
                struct termios n = o;
                n.c_lflag &= ~(ICANON | ECHO | ECHOCTL | ECHONL);
                n.c_cflag |= HUPCL;
                n.c_cc[VMIN] = 0;
                tcsetattr(STDIN_FILENO, TCSANOW, &n);

                int c;
                while (!quit) {
                    c = getchar();
                    if (c != EOF) {
                        DBusMessage *msg = dbus_message_new_method_call("org.formatique.MediaPlayer",
                                                                        "/org/formatique/MediaPlayer",
                                                                        "org.formatique.MediaPlayer",
                                                                        "Key");

                        DBusMessageIter args;
                        dbus_message_iter_init_append(msg, &args);

                        dbus_int32_t value = c;
                        dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &value);

                        dbus_uint32_t serial;
                        dbus_connection_send(bus, msg, &serial);
                        dbus_message_unref(msg);

                        printf("0x%02x\n", c);

                        if (c == 0x1b) // ESC
                            quit = 1;
                    }

                    usleep(1);
                }

                tcsetattr(STDIN_FILENO, TCSANOW, &o);

                return 0;
            }

//            {
//                DBusPendingCall *call;
//                dbus_connection_send_with_reply(bus, msg, &call, -1);
//                dbus_pending_call_block(call);
//                dbus_pending_call_steal_reply(call);
//                dbus_pending_call_unref(call);
//                dbus_message_iter_init(msg, &args);

//                char *value;
//                dbus_message_iter_get_basic(&args, &value);
//                printf("%s\n", value);
//            }

//            dbus_message_unref(msg);

//            DBusConnection *bus = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
//            assert(bus);

//            dbus_connection_set_exit_on_disconnect(bus, FALSE);

//            int res = dbus_bus_request_name(bus,
//                                            "org.formatique.source",
//                                            DBUS_NAME_FLAG_DO_NOT_QUEUE,
//                                            &err);
//            assert(res == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER);

//            DBusMessage *msg = dbus_message_new_signal("/org/formatique/signal/Object",
//                                                       "org.formatique.signal.Type",
//                                                       "QUIT");
//            assert(msg);

//            DBusMessageIter arg;
//            dbus_message_iter_init_append(msg, &arg);

//            char *value = "QUIT";
//            res = dbus_message_iter_append_basic(&arg, DBUS_TYPE_STRING, &value);
//            assert(res);

//            res = dbus_connection_send(bus, msg, NULL);
//            assert(res);

//            dbus_connection_flush(bus);
//            dbus_message_unref(msg);
        }

//        {
//            struct termios o;
//            tcgetattr(STDIN_FILENO, &o);
//            struct termios n = o;
//            n.c_lflag &= ~(ICANON | ECHO | ECHOCTL | ECHONL);
//            n.c_cflag |= HUPCL;
//            n.c_cc[VMIN] = 0;
//            tcsetattr(STDIN_FILENO, TCSANOW, &n);

//            int c;
//            while (!quit) {
//                c = getchar();
//                if (c != EOF)
//                    printf("0x%02x\n", c);

//                usleep(1);
//            }

//            tcsetattr(STDIN_FILENO, TCSANOW, &o);

//            return 0;
//        }

        return 0;
    }

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

        int i;
        for (i = 0; i < fc->nb_streams; i++)
            fc->streams[i]->discard = AVDISCARD_ALL;

//        int st_vid_idx = 0;
//        AVCodec *vid_decoder = NULL;
//        st_vid_idx = av_find_best_stream(fc, AVMEDIA_TYPE_VIDEO, -1, -1, &vid_decoder, 0);
//        assert(st_vid_idx >= 0);
//        assert(vid_decoder);
//        fc->streams[st_vid_idx]->discard = AVDISCARD_DEFAULT;

        int st_aud_idx = 0;
        AVCodec *aud_decoder = NULL;
        st_aud_idx = av_find_best_stream(fc, AVMEDIA_TYPE_AUDIO, -1, -1, &aud_decoder, 0);
        assert(st_aud_idx >= 0);
        assert (aud_decoder);
        fc->streams[st_aud_idx]->discard = AVDISCARD_DEFAULT;

//        AVRational aspect_ratio = av_guess_sample_aspect_ratio(fc, fc->streams[st_vid_idx], NULL);
//        printf("aspect_ratio = %d/%d\n", aspect_ratio.num, aspect_ratio.den);

//        av_dump_format(fc, 0, "", 0);

        {
            opts = NULL;
            av_dict_set(&opts, "refcounted_frames", "1", 0);
//            res = avcodec_open2(fc->streams[st_vid_idx]->codec, vid_decoder, &opts);
//            assert(res >=0);
            res = avcodec_open2(fc->streams[st_aud_idx]->codec, aud_decoder, &opts);
            assert(res >=0);
            av_dict_free(&opts);
        }

//        AVRational time_base = fc->streams[st_vid_idx]->time_base;
//        AVRational frame_rate = av_guess_frame_rate(fc, fc->streams[st_vid_idx], 0);
//        printf("%d/%d\n", time_base.num, time_base.den);
//        printf("%d/%d\n", frame_rate.num, frame_rate.den);
        printf("AV_TIME_BASE = %d\n", AV_TIME_BASE);

        int time_seek = atoi(argv[2]);
        if (time_seek > 0) {
            for (i = 0; i < fc->nb_chapters; i++) {
                if (av_compare_ts((int64_t)time_seek * AV_TIME_BASE,
                              AV_TIME_BASE_Q,
                              fc->chapters[i]->start,
                              fc->chapters[i]->time_base) < 0) {
                    break;
                }
            }

            fprintf(stdout, "Seeking to %d... (chapter %d)\n", time_seek, i);
            fflush(stdout);
            res = avformat_seek_file(fc,
                                     -1,
                                     INT64_MIN,
                                     (int64_t)time_seek * AV_TIME_BASE,
                                     INT64_MAX,
                                     AVSEEK_FLAG_ANY);
            assert(res >= 0);

//            avcodec_flush_buffers(fc->streams[st_vid_idx]->codec);
//            avcodec_flush_buffers(fc->streams[st_aud_idx]->codec);
        }

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

        system("setterm -cursor off");

        struct timespec a, b;
        clock_gettime(CLOCK_MONOTONIC, &a);

        uint8_t *out_buf = NULL;
        int out_buf_size = 0;

        while (!quit) {
            av_free_packet(&pkt);

            res = av_read_frame(fc, &pkt);
            assert(res == 0);

            if (pkt.stream_index == st_aud_idx) {
                int size;

                while (pkt.size > 0) {
//                    avcodec_flush_buffers(fc->streams[st_vid_idx]->codec);
                    avcodec_flush_buffers(fc->streams[st_aud_idx]->codec);

                    size = avcodec_decode_audio4(fc->streams[st_aud_idx]->codec, frame, &got_frame, &pkt);

                    // Skip errors
                    if (size < 0)
                    {
                        pkt.size = 0;
                        continue;
                    }

                    // Avoid decoder packet over-read
                    size = FFMIN(size, pkt.size);

                    if (size > 0 && got_frame) {
//                        int in_linesize;
//                        int in_size = av_samples_get_buffer_size(&in_linesize,
//                                                                 fc->streams[st_aud_idx]->codec->channels,
//                                                                 frame->nb_samples,
//                                                                 fc->streams[st_aud_idx]->codec->sample_fmt,
//                                                                 0);
                        int out_linesize;
                        int out_size = av_samples_get_buffer_size(&out_linesize,
                                                                  fc->streams[st_aud_idx]->codec->channels,
                                                                  frame->nb_samples,
                                                                  AV_SAMPLE_FMT_S16,
                                                                  1);

//                        int contiguous =
//                                !frame->data[1] ||
//                                (frame->data[1] == frame->data[0] + in_linesize && in_linesize == out_linesize && in_linesize * fc->streams[st_aud_idx]->codec->channels == in_size);

                        if (!out_buf || out_buf_size < out_size + FF_INPUT_BUFFER_PADDING_SIZE)
                        {
                            if (out_buf)
                                av_free(out_buf);

                            out_buf = av_malloc(out_size + FF_INPUT_BUFFER_PADDING_SIZE);
                            assert(out_buf);
                        }

                        {
                            SwrContext *swr = swr_alloc_set_opts(NULL,
                                                                 av_get_default_channel_layout(fc->streams[st_aud_idx]->codec->channels),
                                                                 AV_SAMPLE_FMT_S16,
                                                                 fc->streams[st_aud_idx]->codec->sample_rate,
                                                                 av_get_default_channel_layout(fc->streams[st_aud_idx]->codec->channels),
                                                                 fc->streams[st_aud_idx]->codec->sample_fmt,
                                                                 fc->streams[st_aud_idx]->codec->sample_rate,
                                                                 0,
                                                                 NULL);
                            res = swr_init(swr);
                            assert(res >= 0);

                            uint8_t *out_planes[fc->streams[st_aud_idx]->codec->channels];
                            res = av_samples_fill_arrays(out_planes,
                                                         NULL,
                                                         (const uint8_t *)out_buf,
                                                         fc->streams[st_aud_idx]->codec->channels,
                                                         frame->nb_samples,
                                                         AV_SAMPLE_FMT_S16,
                                                         1);
                            assert(res >= 0);

                            res = swr_convert(swr,
                                              out_planes,
                                              frame->nb_samples,
                                              (const uint8_t **)frame->data,
                                              frame->nb_samples);
                            assert(res >= 0);
                            swr_free(&swr);
                        }

                        ao_play(device, (char *)out_buf, (uint_32)out_size);

                        {
                            long double pts = pkt.pts / (long double)1000.0;
                            long double ela = (b.tv_sec - a.tv_sec) + ((b.tv_nsec - a.tv_nsec) / (long double)1000000000);
                            long double dif = pts - ela;

                            clock_gettime(CLOCK_MONOTONIC, &b);
                            fprintf(stdout,
                                    "PTS: %.6Lf, ELA: %.6Lf, DIF: %.6Lf                                     \r",
                                    pts,
                                    ela,
                                    dif);
                            fflush(stdout);
                        }
                    }

                    pkt.size -= size;
                    pkt.data += size;

                    av_frame_unref(frame);
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

        ao_shutdown();

        av_free_packet(&pkt);
        av_frame_free(&frame);

        avformat_close_input(&fc);
        avformat_network_deinit();
    }

    printf("Done.\n");

    return 0;
}
