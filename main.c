#include <stdio.h>
#include <assert.h>

#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>

int main(int argc, char *argv[])
{
    {
        av_register_all();
        avformat_network_init();

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

        AVPacket pkt = { 0 };
        AVFrame *frame = av_frame_alloc();
        int got_picture = 0;

        while (!got_picture) {
            av_free_packet(&pkt);

            res = av_read_frame(ic, &pkt);
            assert(res == 0);

            if (pkt.stream_index == st_vid_idx) {
                res = avcodec_decode_video2(ic->streams[st_vid_idx]->codec, frame, &got_picture, &pkt);
                assert(res >= 0);

                if (got_picture) {
                    assert(ic->streams[st_vid_idx]->codec->pix_fmt == AV_PIX_FMT_YUV420P);

                    double duration = (frame_rate.num && frame_rate.den ? av_q2d((AVRational){ frame_rate.den, frame_rate.num }) : 0);
                    frame->pts = av_frame_get_best_effort_timestamp(frame);
                    double pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(time_base);

                    printf(
                        "duration = %.10f, pts = %.10f, %d,%d,%d,%d,%d,%d,%d,%d\n",
                        duration,
                        pts,
                        frame->linesize[0],
                        frame->linesize[1],
                        frame->linesize[2],
                        frame->linesize[3],
                        frame->linesize[4],
                        frame->linesize[5],
                        frame->linesize[6],
                        frame->linesize[7]
                    );

                    {
                        uint8_t *video_dst_data[4] = { 0 };
                        int video_dst_linesize[4];
                        int video_dst_bufsize;

                        res = av_image_alloc(video_dst_data,
                                             video_dst_linesize,
                                             ic->streams[st_vid_idx]->codec->width,
                                             ic->streams[st_vid_idx]->codec->height,
                                             ic->streams[st_vid_idx]->codec->pix_fmt,
                                             1);
                        assert(res >= 0);
                        video_dst_bufsize = res;
                        printf("video_dst_bufsize = %d\n", video_dst_bufsize);

                        av_image_copy(video_dst_data,
                                      video_dst_linesize,
                                      (const uint8_t **)(frame->data),
                                      frame->linesize,
                                      ic->streams[st_vid_idx]->codec->pix_fmt,
                                      ic->streams[st_vid_idx]->codec->width,
                                      ic->streams[st_vid_idx]->codec->height);

                        FILE *f = fopen("frame.yuv", "wb");
                        assert(f);
                        fwrite(video_dst_data[0], 1, video_dst_bufsize, f);
                        fclose(f);
                    }
                }

                av_frame_unref(frame);
            }
            else if (pkt.stream_index == st_aud_idx) {
            }
        }

        av_free_packet(&pkt);
        av_frame_free(&frame);

        avformat_close_input(&ic);

        avformat_network_deinit();
    }

    return 0;
}
