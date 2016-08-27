
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>

#define SDL_AUDIO_BUFFER_SIZE 1024*4
#define MAX_AUDIO_FRAME_SIZE 192000



typedef struct PacketQueue
{
    AVPacketList* first_pkt, *last_pkt;
    int nb_packets;
    int size;
    SDL_mutex* mutex;
    SDL_cond* cond;
}PacketQueue;

PacketQueue audioq;
int quit = 0;

AVFrame wanted_frame;


void packet_queue_init(PacketQueue* q)
{
    if( !q ) return;

    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
}

int packet_queue_put(PacketQueue* q, AVPacket* pkt)
{
    AVPacketList* pktl;
    if( av_dup_packet(pkt) < 0)
        return -1;

    pktl = av_malloc(sizeof(AVPacketList));
    if( !pktl) return -1;

    pktl->pkt = *pkt;
    pktl->next = NULL;

    SDL_LockMutex(q->mutex);
    if( !q->last_pkt )
        q->first_pkt = pktl;
    else
        q->last_pkt->next = pktl;

    q->last_pkt = pktl;
    q->nb_packets++;
    q->size += pktl->pkt.size;
    SDL_CondSignal(q->cond);
    SDL_UnlockMutex(q->mutex);

    return 0;
}


static int packet_queue_get(PacketQueue* q, AVPacket* pkt, int block)
{
    AVPacketList* pktl;
    int ret;

    SDL_LockMutex(q->mutex);
    while(1)
    {
        if(quit)
        {
            ret = -1;
            break;
        }

        pktl = q->first_pkt;
        if(pktl)
        {
            q->first_pkt = pktl->next;
            if(!q->first_pkt)
                q->last_pkt = NULL;

            q->nb_packets--;
            q->size -= pktl->pkt.size;
            *pkt = pktl->pkt;
            av_free(pktl);
            ret = 1;
            break;
        }
        else if( !block )
        {
            ret = 0;
            break;
        }
        else
        {
            SDL_CondWait(q->cond, q->mutex);
        }
    }

    SDL_UnlockMutex(q->mutex);

    return ret;
}

int audio_decode_frame(AVCodecContext* aCodecCtx, uint8_t* audio_buf, int buf_size)
{
    static AVPacket pkt;
    static u_int8_t* audio_pkt_data = NULL;
    static int audio_pkt_size = 0;
    static AVFrame frame;

    int len1, data_size = 0;
    SwrContext* swr_ctx = NULL;
//    int64_t in_channel_layout;

    for(;;)
    {
        while(audio_pkt_size > 0)
        {
            int got_frame = 0;

            len1 = avcodec_decode_audio4(aCodecCtx, &frame, &got_frame, &pkt);
            fprintf(stdout, "%s: frame.format: %d\n", __FUNCTION__, frame.format);
            if(len1 < 0)
            {
                audio_pkt_size = 0;
                break;
            }

            audio_pkt_data += len1;
            audio_pkt_size -= len1;
            data_size = 0;

            if(got_frame)
            {                
                data_size = av_samples_get_buffer_size(NULL, av_frame_get_channels(&frame), frame.nb_samples,
                                                       frame.format, 0);
                assert(data_size <= buf_size);

#if 0
                in_channel_layout = (frame.channel_layout && frame.channels ==
                                      av_get_channel_layout_nb_channels(frame.channel_layout) )
                        ? frame.channel_layout
                        : av_get_default_channel_layout(frame.channels);
#endif
                if(frame.channels > 0 && frame.channel_layout == 0)
                    frame.channel_layout =
                            av_get_default_channel_layout(frame.channels);
                else if(frame.channels == 0 && frame.channel_layout > 0)
                    frame.channels =
                            av_get_channel_layout_nb_channels(frame.channel_layout);
#if 0
                if(frame.format != wanted_frame.format
                        || dec_channel_layout != wanted_frame.channel_layout
                        || frame.sample_rate != wanted_frame.sample_rate
                        )
#endif
                {
                    if( swr_ctx )
                    {
                        swr_free(swr_ctx);
                        swr_ctx = NULL;
                    }

                    swr_ctx = swr_alloc_set_opts(NULL, wanted_frame.channel_layout, wanted_frame.format, wanted_frame.sample_rate,
                                                 frame.channel_layout, frame.format, frame.sample_rate, 0, NULL);

                    if( !swr_ctx || swr_init(swr_ctx) < 0)
                    {
                        fprintf(stderr, "swr_init() failed\n");
                        break;
                    }

                    int len2 = swr_convert(swr_ctx, &audio_buf, buf_size / wanted_frame.channels / av_get_bytes_per_sample(wanted_frame.format),
                                           frame.data, frame.nb_samples);
                    if( len2 < 0 )
                    {
                        fprintf(stderr, "swr_convert failed\n");
                        break;
                    }

                    return wanted_frame.channels * len2 * av_get_bytes_per_sample(wanted_frame.format);
                }
#if 0
                else
                {
                    memcpy(audio_buf, frame.data[0], data_size);
                }
#endif
            }

            if(data_size <= 0)
            {
                continue;
            }

            return data_size;

        }

        if(pkt.data)
            av_free_packet(&pkt);

        memset(&pkt, 0, sizeof(pkt));

        if(quit) return -1;

        if(packet_queue_get(&audioq, &pkt, 1) < 0)
            return -1;

        audio_pkt_data = pkt.data;
        audio_pkt_size = pkt.size;
    }

    return 0;
}


void audio_callback(void* userdata, Uint8* stream, int len)
{
    printf("%s: len = %d\n", __FUNCTION__, len);
    AVCodecContext* aCodecCtx = (AVCodecContext*)userdata;
    int len1, audio_size;

    static u_int8_t audio_buf[MAX_AUDIO_FRAME_SIZE*3/2];
    static unsigned int audio_buf_size = 0;
    static unsigned int audio_buf_index = 0;

    while( len > 0)
    {
        if(audio_buf_index >= audio_buf_size)
        {
            audio_size = audio_decode_frame(aCodecCtx, audio_buf, sizeof(audio_buf));
            if(audio_size < 0)
            {
                audio_buf_size = 1024;
                memset(audio_buf, 0, audio_buf_size);
            }
            else
            {
                audio_buf_size = audio_size;
            }
            audio_buf_index = 0;
        }

        len1 = audio_buf_size - audio_buf_index;
        if(len1 > len)
            len1 = len;


        memcpy(stream, (uint8_t*)audio_buf+audio_buf_index, len1);
        len -= len1;
        stream += len1;
        audio_buf_index += len1;
    }
}


int main(int argc, char* argv[])
{
    if( argc < 2 )
    {
        printf("usage: %s  filepath\n", argv[0]);
        return -1;
    }

    int i, frameFinished;
    int videoStream = -1;
    int audioStream = -1;
    AVCodec* pCodec = NULL;
    AVCodecContext* pCodecCtxOrig = NULL;
    AVCodecContext* pCodecCtx = NULL;
    AVFormatContext* pFormatCtx = NULL;
    AVFrame* pFrame = NULL;
    AVFrame* pFrameRGB = NULL;
    u_int8_t* buffer = NULL;
    AVPacket packet;
    SDL_Rect rect;
    SDL_Event event;
    SDL_Surface* screen = NULL;

    AVCodecContext* aCodecCtxOrig = NULL;
    AVCodecContext* aCodecCtx = NULL;
    AVCodec* aCodec = NULL;
    SDL_AudioSpec   wanted_spec, spec;



    av_register_all();

    if( SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        fprintf(stderr, "could not initialize SDL -%s\n", SDL_GetError() );
        exit(1);
    }

    if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
    {
        printf("open %s failed!!!\n", argv[1] );
        return -1;
    }

    if(avformat_find_stream_info(pFormatCtx, NULL) < 0)
        return -1;

    av_dump_format(pFormatCtx, 0, argv[1], 0);

    for(i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO
                && videoStream < 0)
        {
            videoStream = i;
        }
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO
                && audioStream < 0)
        {
            audioStream = i;
        }
    }


    if(-1 == audioStream )
        return -1;

    fprintf(stdout, "audioStream: %d\n", audioStream);

    aCodecCtxOrig = pFormatCtx->streams[audioStream]->codec;
    aCodec = avcodec_find_decoder(aCodecCtxOrig->codec_id);
    if( !aCodec )
    {
        printf("Unsupported codec!\n");
        return -1;
    }

    aCodecCtx = avcodec_alloc_context3(aCodec);
    if(avcodec_copy_context(aCodecCtx, aCodecCtxOrig) != 0)
    {
        printf("couldn't copy codec context\n");
        return -1;
    }

    avcodec_open2(aCodecCtx, aCodec, NULL);

    wanted_spec.freq = aCodecCtx->sample_rate;
    wanted_spec.format =  AUDIO_S16SYS;
    wanted_spec.channels = aCodecCtx->channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
    wanted_spec.callback = audio_callback;
    wanted_spec.userdata = aCodecCtx;


    if(SDL_OpenAudio(&wanted_spec, &spec) != 0)
    {
        fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError() );
        return -1;
    }

    wanted_frame.format = AV_SAMPLE_FMT_S16;
    wanted_frame.sample_rate = spec.freq;
    wanted_frame.channel_layout = av_get_default_channel_layout(spec.channels);
    wanted_frame.channels = spec.channels;

    packet_queue_init(&audioq);
    SDL_PauseAudio(0);

#if 1
    if(-1 == videoStream )
        return -1;

    pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;
    pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
    if( !pCodec )
    {
        printf("Unsupported codec!\n");
        return -1;
    }

    pCodecCtx = avcodec_alloc_context3( pCodec );
    if( avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0 )
    {
        printf("couldn't copy codec context\n");
        return -1;
    }

    if( avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
        return -1;


    screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
    if( !screen )
    {
        fprintf(stderr, "SDL: could not set video mode - exiting\n");
        exit(1);
    }

    pFrame = av_frame_alloc();


    SDL_Overlay* bmp = NULL;
    struct SwsContext* sws_ctx = NULL;

    bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height, SDL_YV12_OVERLAY, screen);
    sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                             pCodecCtx->width, pCodecCtx->height, PIX_FMT_YUV420P, SWS_BILINEAR,
                             NULL, NULL, NULL);
#endif

    while( av_read_frame(pFormatCtx, &packet) >= 0 )
    {
        if(packet.stream_index == videoStream )
        {
#if 1
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
            if(frameFinished)
            {
                SDL_LockYUVOverlay(bmp);

                AVPicture pict;
                pict.data[0] = bmp->pixels[0];
                pict.data[1] = bmp->pixels[1];
                pict.data[2] = bmp->pixels[2];

                pict.linesize[0] = bmp->pitches[0];
                pict.linesize[1] = bmp->pitches[1];
                pict.linesize[2] = bmp->pitches[2];

                sws_scale(sws_ctx, (const u_int8_t* const*)pFrame->data, pFrame->linesize,
                          0, pCodecCtx->height, pict.data, pict.linesize);

                SDL_UnlockYUVOverlay(bmp);

                rect.x = 0;
                rect.y = 0;
                rect.w = pCodecCtx->width;
                rect.h = pCodecCtx->height;

                SDL_DisplayYUVOverlay(bmp, &rect);

                SDL_Delay(1000 / 24);
            }

            av_free_packet(&packet);
#endif
        }
        else if(packet.stream_index == audioStream )
        {
            packet_queue_put(&audioq, &packet);
        }
        else
        {
            av_free_packet(&packet);
        }


        SDL_PollEvent(&event);
        switch (event.type) {
        case SDL_QUIT:
            SDL_Quit();
            quit = 1;
            exit(0);
            break;
        default:
            break;
        }
    }

//    av_free(buffer);
    av_free(pFrameRGB);
    av_free(pFrame);
    avcodec_close(pCodecCtx);
    avcodec_close(pCodecCtxOrig);
    avformat_close_input(&pFormatCtx);


    return 0;
}
