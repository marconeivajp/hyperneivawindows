#include "video_player.h"
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

static volatile bool vpQuit = false;

// ============================================================
// CONFIGURATION
// ============================================================
#define AUDIO_RING_BYTES   (44100 * 2 * 2 * 2)  // 2 sec stereo S16
#define AUDIO_CHUNK_FRAMES 4096                   // frames per Raylib update
#define VIDEO_PKT_Q        256
#define AUDIO_PKT_Q        512
#define PICTURE_Q_SIZE     8                      // decoded frames ready to show

// ============================================================
// PACKET QUEUES (circular, pre-allocated)
// ============================================================
typedef struct {
    AVPacket** data;
    int        cap;
    int        wr, rd, n;
} PktQueue;

static bool pq_push(PktQueue* q, AVPacket* pkt) {
    if (q->n >= q->cap) return false;
    q->data[q->wr] = av_packet_clone(pkt);
    q->wr = (q->wr + 1) % q->cap;
    q->n++;
    return true;
}
static AVPacket* pq_pop(PktQueue* q) {
    if (q->n == 0) return NULL;
    AVPacket* p = q->data[q->rd];
    q->data[q->rd] = NULL;
    q->rd = (q->rd + 1) % q->cap;
    q->n--;
    return p;
}
static void pq_flush(PktQueue* q) {
    while (q->n > 0) {
        AVPacket* p = pq_pop(q);
        if (p) av_packet_free(&p);
    }
    q->wr = q->rd = q->n = 0;
}
static void pq_free(PktQueue* q) {
    pq_flush(q);
    free(q->data);
    q->data = NULL; q->cap = 0;
}

// ============================================================
// AUDIO RING BUFFER (byte level, interleaved S16 stereo)
// ============================================================
static uint8_t  audioRing[AUDIO_RING_BYTES];
static int      audioRingWr  = 0;
static int      audioRingRd  = 0;
static int      audioRingAvail = 0;

static void aring_push(const uint8_t* src, int bytes) {
    int space = AUDIO_RING_BYTES - audioRingAvail;
    if (bytes > space) bytes = space;
    for (int i = 0; i < bytes; i++) {
        audioRing[audioRingWr] = src[i];
        audioRingWr = (audioRingWr + 1) % AUDIO_RING_BYTES;
    }
    audioRingAvail += bytes;
}
static int aring_pop(uint8_t* dst, int bytes) {
    if (bytes > audioRingAvail) bytes = audioRingAvail;
    for (int i = 0; i < bytes; i++) {
        dst[i] = audioRing[audioRingRd];
        audioRingRd = (audioRingRd + 1) % AUDIO_RING_BYTES;
    }
    audioRingAvail -= bytes;
    return bytes;
}

// ============================================================
// DECODED PICTURE QUEUE (thread-safe, between SDL decode thread
// and the raylib render thread)
// ============================================================
typedef struct {
    uint8_t* rgba;     // w*h*4 contiguous RGBA (already converted by decode thread)
    double   pts;
    int      w, h;
    bool     used;
} Picture;

typedef struct {
    Picture  pic[PICTURE_Q_SIZE];
    int      rindex, windex, size;
    SDL_mutex* mutex;
    SDL_cond* cond;     // signaled when a picture is added
    SDL_cond* freeCond; // signaled when a slot becomes free
} PictureQueue;

static PictureQueue pq = {0};

static void picq_init(void) {
    pq.mutex   = SDL_CreateMutex();
    pq.cond    = SDL_CreateCond();
    pq.freeCond= SDL_CreateCond();
}
static void picq_free(void) {
    if (pq.mutex)   { SDL_DestroyMutex(pq.mutex);   pq.mutex = NULL; }
    if (pq.cond)    { SDL_DestroyCond(pq.cond);     pq.cond = NULL; }
    if (pq.freeCond){ SDL_DestroyCond(pq.freeCond); pq.freeCond = NULL; }
    for (int i = 0; i < PICTURE_Q_SIZE; i++) {
        if (pq.pic[i].rgba) { free(pq.pic[i].rgba); pq.pic[i].rgba = NULL; }
        pq.pic[i].used = false;
    }
    pq.rindex = pq.windex = pq.size = 0;
}
// Allocate a free slot (blocking) and return pointer to fill. Caller must
// call picq_push() after filling. Returns NULL if quitting and no slot frees.
static Picture* picq_get_free(void) {
    SDL_LockMutex(pq.mutex);
    while (pq.size >= PICTURE_Q_SIZE) {
        if (vpQuit) { SDL_UnlockMutex(pq.mutex); return NULL; }
        SDL_CondWait(pq.freeCond, pq.mutex);
    }
    Picture* p = &pq.pic[pq.windex];
    SDL_UnlockMutex(pq.mutex);
    return p;
}
static void picq_push(Picture* p) {
    SDL_LockMutex(pq.mutex);
    p->used = true;
    pq.windex = (pq.windex + 1) % PICTURE_Q_SIZE;
    pq.size++;
    SDL_CondSignal(pq.cond);
    SDL_UnlockMutex(pq.mutex);
}
// Take the next ready picture (non-blocking). Returns NULL if empty.
static Picture* picq_try_get(void) {
    SDL_LockMutex(pq.mutex);
    Picture* p = NULL;
    if (pq.size > 0) {
        p = &pq.pic[pq.rindex];
        if (!p->used) p = NULL;
    }
    SDL_UnlockMutex(pq.mutex);
    return p;
}
static void picq_release(Picture* p) {
    SDL_LockMutex(pq.mutex);
    p->used = false;
    pq.rindex = (pq.rindex + 1) % PICTURE_Q_SIZE;
    pq.size--;
    SDL_CondSignal(pq.freeCond);
    SDL_UnlockMutex(pq.mutex);
}
// Wake the decode thread if it is blocked waiting for a free slot (used on
// cleanup / seek so it never deadlocks inside SDL_CondWait).
static void picq_wake_all(void) {
    if (!pq.mutex) return;
    SDL_LockMutex(pq.mutex);
    SDL_CondBroadcast(pq.freeCond);
    SDL_CondBroadcast(pq.cond);
    SDL_UnlockMutex(pq.mutex);
}

// ============================================================
// STATE
// ============================================================
static AVFormatContext*  fmtCtx    = NULL;
static AVCodecContext*   vCtx      = NULL;
static AVCodecContext*   aCtx      = NULL;
static int               vIdx      = -1;
static int               aIdx      = -1;
static struct SwsContext* swsCtx   = NULL;
static struct SwrContext* swrCtx   = NULL;

static SDL_Thread* decodeThread = NULL;

static void vp_seek_to(double seekSec);

static Texture2D  videoTex   = { 0 };
static Image      videoImg   = { 0 };

static AudioStream audioSt   = { 0 };
static bool        audioReady = false;

static PktQueue    vQueue, aQueue;
static long long   audioFramesSent = 0; // audio master clock

static bool   vpOpen      = false;
static bool   vpPaused    = false;
static bool   vpWantsClose= false;
static float  vpVolume    = 1.0f;
static double vpDuration  = 0.0;
static double vpElapsed   = 0.0;
static double vpPlayStart = 0.0;
static double vpPauseAt   = 0.0;

static double frameDur    = 0.033;
static double nextVidTime = 0.0;

static char   vpTitle[512] = "";
static double lastMouseMove = 0.0;
#define CONTROLS_FADE 3.0

static char   vpLastError[256] = "";   // msg de erro da ultima abertura/decodificacao
#define MAX_PACKET_BYTES (16 * 1024 * 1024)  // limite de seguranca p/ arquivos corrompidos

// ============================================================
// CLEANUP
// ============================================================
static void vp_cleanup(void) {
    vpQuit = true;
    picq_wake_all();   // unblock decode thread if waiting for a free slot
    if (decodeThread) {
        SDL_WaitThread(decodeThread, NULL);
        decodeThread = NULL;
    }
    if (swsCtx)  { sws_freeContext(swsCtx);    swsCtx  = NULL; }
    if (swrCtx)  { swr_free(&swrCtx);          swrCtx  = NULL; }
    if (vCtx)    { avcodec_free_context(&vCtx); vCtx    = NULL; }
    if (aCtx)    { avcodec_free_context(&aCtx); aCtx    = NULL; }
    if (fmtCtx)  { avformat_close_input(&fmtCtx); fmtCtx = NULL; }
    if (videoTex.id != 0) { UnloadTexture(videoTex); memset(&videoTex, 0, sizeof(Texture2D)); }
    if (videoImg.data)    { free(videoImg.data);      memset(&videoImg, 0, sizeof(Image)); }
    if (audioReady) { StopAudioStream(audioSt); UnloadAudioStream(audioSt); audioReady = false; }
    pq_free(&vQueue);
    pq_free(&aQueue);
    picq_free();
    audioRingWr = audioRingRd = audioRingAvail = 0;
    audioFramesSent = 0;
    vpOpen = vpPaused = vpWantsClose = false;
    vIdx = aIdx = -1;
}

// ============================================================
// DEMUX — read packets into queues (runs in decode thread)
// ============================================================
static void demux_packets(void) {
    if (!fmtCtx) return;
    while (vQueue.n < (vQueue.cap * 3/4) || aQueue.n < (aQueue.cap * 3/4)) {
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) break;
        int ret = av_read_frame(fmtCtx, pkt);
        if (ret < 0) {
            av_packet_free(&pkt);
            if (ret != AVERROR_EOF) {
                snprintf(vpLastError, sizeof(vpLastError), "Arquivo corrompido ou incompleto (falha na leitura)");
            }
            break;
        }
        // Safety: skip absurdly large packets from corrupt files to avoid
        // exhausting memory in the queues.
        if (pkt->size > MAX_PACKET_BYTES) {
            av_packet_unref(pkt);
            av_packet_free(&pkt);
            continue;
        }

        if (pkt->stream_index == vIdx) {
            if (!pq_push(&vQueue, pkt)) { /* drop if full */ }
        } else if (pkt->stream_index == aIdx) {
            if (!pq_push(&aQueue, pkt)) { /* drop if full */ }
        }
        av_packet_unref(pkt);
        av_packet_free(&pkt);

        if (vQueue.n >= vQueue.cap && aQueue.n >= aQueue.cap) break;
    }
}

// ============================================================
// AUDIO DECODE (runs in decode thread)
// ============================================================
static void decode_audio_to_ring(void) {
    if (!aCtx || !swrCtx) return;
    while (audioRingAvail < AUDIO_RING_BYTES - (AUDIO_CHUNK_FRAMES * 2 * 2)) {
        AVPacket* pkt = pq_pop(&aQueue);
        if (!pkt) break;
        AVFrame* aFrame = av_frame_alloc();
        if (avcodec_send_packet(aCtx, pkt) == 0) {
            while (avcodec_receive_frame(aCtx, aFrame) == 0) {
                int outSamples = av_rescale_rnd(
                    swr_get_delay(swrCtx, aCtx->sample_rate) + aFrame->nb_samples,
                    44100, aCtx->sample_rate, AV_ROUND_UP);
                uint8_t* outBuf = NULL;
                int outLineSize = 0;
                if (av_samples_alloc(&outBuf, &outLineSize, 2, outSamples, AV_SAMPLE_FMT_S16, 0) >= 0) {
                    int converted = swr_convert(swrCtx,
                        &outBuf, outSamples,
                        (const uint8_t**)aFrame->data, aFrame->nb_samples);
                    if (converted > 0)
                        aring_push(outBuf, converted * 2 * sizeof(int16_t));
                    av_freep(&outBuf);
                }
            }
        }
        av_frame_free(&aFrame);
        av_packet_free(&pkt);
    }
}

// ============================================================
// VIDEO DECODE THREAD (SDL2)
//   - demuxes, decodes video to YUV420P, pushes to picture queue
// ============================================================
static int video_decode_thread(void* data) {
    (void)data;
    AVFrame* vFrame = av_frame_alloc();
    uint8_t* rgbBuf = NULL;
    int rgbCap = 0;

    while (!vpQuit) {
        demux_packets();
        decode_audio_to_ring();

        if (vQueue.n == 0 && aQueue.n == 0) {
            // Both queues drained. If picture queue also empty and audio
            // finished (or there is no audio), signal end-of-video.
            if (pq.size == 0 && (audioRingAvail == 0 || !audioReady)) {
                vpOpen = false;
                vpWantsClose = true;
                break;
            }
            SDL_Delay(2);
            continue;
        }

        if (vQueue.n == 0) { SDL_Delay(2); continue; }

        AVPacket* pkt = pq_pop(&vQueue);
        if (!pkt) continue;
        int ret = avcodec_send_packet(vCtx, pkt);
        av_packet_free(&pkt);
        if (ret < 0 && ret != AVERROR(EAGAIN)) continue;

        while (avcodec_receive_frame(vCtx, vFrame) == 0) {
            int w = vCtx->width, h = vCtx->height;
            int need = w * h * 4;
            if (need > rgbCap) {
                rgbBuf = (uint8_t*)realloc(rgbBuf, need);
                rgbCap = need;
            }
            uint8_t* dst[1] = { rgbBuf };
            int dstStride[1] = { w * 4 };
            sws_scale(swsCtx, (const uint8_t* const*)vFrame->data, vFrame->linesize,
                      0, h, dst, dstStride);

            double pts = vFrame->best_effort_timestamp;
            double ptsSec = (pts == AV_NOPTS_VALUE)
                ? nextVidTime
                : pts * av_q2d(fmtCtx->streams[vIdx]->time_base);

            Picture* p = picq_get_free();
            if (!p) break;
            if (!p->rgba || p->w != w || p->h != h) {
                free(p->rgba);
                p->rgba = (uint8_t*)malloc(need);
                p->w = w; p->h = h;
            }
            memcpy(p->rgba, rgbBuf, need);
            p->pts = ptsSec;
            nextVidTime = ptsSec + frameDur;
            picq_push(p);
        }
    }

    if (rgbBuf) free(rgbBuf);
    av_frame_free(&vFrame);
    return 0;
}

// ============================================================
// AUDIO FEED (raylib thread)
// ============================================================
static int16_t audioPcm[AUDIO_CHUNK_FRAMES * 2];

static void feed_audio_stream(void) {
    if (!audioReady || vpPaused) return;
    if (!IsAudioStreamProcessed(audioSt)) return;
    int bytesNeeded = AUDIO_CHUNK_FRAMES * 2 * sizeof(int16_t);
    int got = aring_pop((uint8_t*)audioPcm, bytesNeeded);
    if (got < bytesNeeded)
        memset((uint8_t*)audioPcm + got, 0, bytesNeeded - got);
    UpdateAudioStream(audioSt, audioPcm, AUDIO_CHUNK_FRAMES);
    audioFramesSent += AUDIO_CHUNK_FRAMES;
}

// ============================================================
// PUBLIC: OPEN
// ============================================================
bool VideoPlayer_Open(const char* filepath) {
    picq_init();          // ensure mutex/conds exist before any cleanup
    vp_cleanup();
    vpQuit = false;
    vpLastError[0] = 0;

    if (SDL_Init(SDL_INIT_TIMER) != 0) { /* non-fatal, threads still work */ }

    picq_init();
    vQueue.data = calloc(VIDEO_PKT_Q, sizeof(AVPacket*)); vQueue.cap = VIDEO_PKT_Q;
    aQueue.data = calloc(AUDIO_PKT_Q, sizeof(AVPacket*)); aQueue.cap = AUDIO_PKT_Q;

    fmtCtx = avformat_alloc_context();
    if (!fmtCtx) { snprintf(vpLastError, sizeof(vpLastError), "Sem memoria para abrir o video."); vp_cleanup(); return false; }
    if (avformat_open_input(&fmtCtx, filepath, NULL, NULL) < 0) { snprintf(vpLastError, sizeof(vpLastError), "Nao foi possivel abrir o arquivo de video."); vp_cleanup(); return false; }
    if (avformat_find_stream_info(fmtCtx, NULL) < 0) { snprintf(vpLastError, sizeof(vpLastError), "Formato de video nao reconhecido."); vp_cleanup(); return false; }

    for (unsigned i = 0; i < fmtCtx->nb_streams; i++) {
        enum AVMediaType t = fmtCtx->streams[i]->codecpar->codec_type;
        if (t == AVMEDIA_TYPE_VIDEO && vIdx < 0) vIdx = (int)i;
        if (t == AVMEDIA_TYPE_AUDIO && aIdx < 0) aIdx = (int)i;
    }
    if (vIdx < 0) { snprintf(vpLastError, sizeof(vpLastError), "Nenhuma trilha de video encontrada."); vp_cleanup(); return false; }

    AVCodecParameters* vpar = fmtCtx->streams[vIdx]->codecpar;
    const AVCodec* vcodec = avcodec_find_decoder(vpar->codec_id);
    if (!vcodec) { snprintf(vpLastError, sizeof(vpLastError), "Codec de video nao suportado."); vp_cleanup(); return false; }
    vCtx = avcodec_alloc_context3(vcodec);
    avcodec_parameters_to_context(vCtx, vpar);
    vCtx->thread_count = 4;
    if (avcodec_open2(vCtx, vcodec, NULL) < 0) { snprintf(vpLastError, sizeof(vpLastError), "Falha ao iniciar o codec de video."); vp_cleanup(); return false; }

    int w = vCtx->width, h = vCtx->height;
    swsCtx = sws_getContext(w, h, vCtx->pix_fmt, w, h, AV_PIX_FMT_RGBA,
                            SWS_BILINEAR, NULL, NULL, NULL);

    videoImg.width = w; videoImg.height = h; videoImg.mipmaps = 1;
    videoImg.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    videoImg.data = calloc(1, (size_t)w * h * 4);
    videoTex = LoadTextureFromImage(videoImg);

    double fps = av_q2d(fmtCtx->streams[vIdx]->r_frame_rate);
    frameDur = (fps > 0.0) ? (1.0 / fps) : 0.033;
    nextVidTime = 0.0;

    if (aIdx >= 0) {
        AVCodecParameters* apar = fmtCtx->streams[aIdx]->codecpar;
        const AVCodec* acodec = avcodec_find_decoder(apar->codec_id);
        if (acodec) {
            aCtx = avcodec_alloc_context3(acodec);
            avcodec_parameters_to_context(aCtx, apar);
            if (avcodec_open2(aCtx, acodec, NULL) == 0) {
                swrCtx = swr_alloc();
                AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
                av_opt_set_chlayout  (swrCtx, "in_chlayout",    &aCtx->ch_layout, 0);
                av_opt_set_int       (swrCtx, "in_sample_rate",  aCtx->sample_rate, 0);
                av_opt_set_sample_fmt(swrCtx, "in_sample_fmt",   aCtx->sample_fmt, 0);
                av_opt_set_chlayout  (swrCtx, "out_chlayout",   &stereo, 0);
                av_opt_set_int       (swrCtx, "out_sample_rate", 44100, 0);
                av_opt_set_sample_fmt(swrCtx, "out_sample_fmt",  AV_SAMPLE_FMT_S16, 0);
                if (swr_init(swrCtx) == 0) {
                    audioSt = LoadAudioStream(44100, 16, 2);
                    SetAudioStreamVolume(audioSt, vpVolume);
                    audioReady = true;
                }
            }
        }
    }

    vpDuration = (fmtCtx->duration > 0) ? ((double)fmtCtx->duration / AV_TIME_BASE) : 0.0;
    vpElapsed  = 0.0;
    vpPaused   = false;
    vpOpen     = true;
    vpPlayStart = GetTime();
    lastMouseMove = GetTime();

    const char* sl = strrchr(filepath, '/');
    if (!sl) sl = strrchr(filepath, '\\');
    strncpy(vpTitle, sl ? sl + 1 : filepath, sizeof(vpTitle) - 1);

    // Start decode thread (SDL2)
    decodeThread = SDL_CreateThread(video_decode_thread, "video_decode", NULL);

    // Prefill audio ring + first video frame before starting the clock,
    // so playback starts smoothly without audio underrun stutter.
    double preT0 = GetTime();
    while (GetTime() - preT0 < 1.0) {
        if (pq.size >= 1 && (!audioReady || audioRingAvail >= 4096 * 2 * 2 * 2)) break;
        SDL_Delay(5);
    }

    if (audioReady) {
        memset(audioPcm, 0, sizeof(audioPcm));
        UpdateAudioStream(audioSt, audioPcm, AUDIO_CHUNK_FRAMES);
        PlayAudioStream(audioSt);
    }

    return true;
}

// ============================================================
// PUBLIC: UPDATE (raylib thread, every frame)
// ============================================================
void VideoPlayer_Update(void) {
    if (!vpOpen || vpPaused) {
        if (audioReady && IsAudioStreamProcessed(audioSt)) {
            memset(audioPcm, 0, sizeof(audioPcm));
            UpdateAudioStream(audioSt, audioPcm, AUDIO_CHUNK_FRAMES);
        }
        return;
    }

    // Master clock = audio when available, else wall-clock.
    if (audioReady) vpElapsed = (double)audioFramesSent / 44100.0;
    else            vpElapsed = GetTime() - vpPlayStart;

    feed_audio_stream();

    // Present frames whose PTS is due (ffplay-style). Frames that are far
    // behind the master clock are dropped to avoid a burst of stale frames.
    int maxFrames = 8;
    while (maxFrames-- > 0) {
        Picture* p = picq_try_get();
        if (!p) break;
        if (p->pts <= vpElapsed + frameDur * 0.5) {
            if (p->pts < vpElapsed - frameDur * 1.5) {
                picq_release(p); // too late, drop it
                continue;
            }
            UpdateTexture(videoTex, p->rgba);
            picq_release(p);
        } else {
            // Not time yet; keep it for next frame.
            break;
        }
    }
}

// ============================================================
// DRAW HELPERS
// ============================================================
static bool draw_button(Vector2 mp, bool clicked, int x, int y, int w, int h, const char* label) {
    bool hov = (mp.x >= x && mp.x <= x+w && mp.y >= y && mp.y <= y+h);
    DrawRectangle(x, y, w, h, hov ? (Color){0,160,255,255} : (Color){30,30,50,210});
    DrawRectangleLines(x, y, w, h, (Color){90,90,110,255});
    int tw = MeasureText(label, 17);
    DrawText(label, x+(w-tw)/2, y+(h-18)/2, 17, WHITE);
    return hov && clicked;
}

// ============================================================
// PUBLIC: DRAW
// ============================================================
void VideoPlayer_Draw(void) {
    if (videoTex.id == 0) return;

    int sw = GetScreenWidth(), sh = GetScreenHeight();

    float scaleX = (float)sw / videoTex.width;
    float scaleY = (float)sh / videoTex.height;
    float scale  = scaleX < scaleY ? scaleX : scaleY;
    int   dw = (int)(videoTex.width * scale);
    int   dh = (int)(videoTex.height * scale);
    DrawTexturePro(videoTex,
        (Rectangle){0, 0, (float)videoTex.width, (float)videoTex.height},
        (Rectangle){(float)((sw-dw)/2), (float)((sh-dh)/2), (float)dw, (float)dh},
        (Vector2){0,0}, 0.0f, WHITE);

    Vector2 mp = GetMousePosition();
    static Vector2 lastMp = {-1,-1};
    if ((int)mp.x != (int)lastMp.x || (int)mp.y != (int)lastMp.y) {
        lastMouseMove = GetTime(); lastMp = mp;
    }
    if (GetTime() - lastMouseMove > CONTROLS_FADE) return;

    bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    DrawRectangle(0, 0, sw, 55, (Color){10,10,25,220});
    DrawText(vpTitle, 20, 17, 19, WHITE);

    int xbx = sw-52, xby = 8, xbs = 40;
    bool hX = (mp.x>=xbx && mp.x<=xbx+xbs && mp.y>=xby && mp.y<=xby+xbs);
    DrawRectangle(xbx, xby, xbs, xbs, hX ? (Color){200,40,40,255} : (Color){80,25,25,210});
    DrawRectangleLines(xbx, xby, xbs, xbs, WHITE);
    DrawText("X", xbx+13, xby+10, 22, WHITE);
    if (hX && clicked) { vpWantsClose = true; return; }

    int barH = 92, barY = sh - barH;
    DrawRectangle(0, barY, sw, barH, (Color){10,10,25,210});

    int pbX = 20, pbY = barY+10, pbW = sw-40, pbH = 9;
    DrawRectangle(pbX, pbY, pbW, pbH, (Color){55,55,55,255});
    double prog = (vpDuration > 0.0) ? (vpElapsed / vpDuration) : 0.0;
    if (prog > 1.0) prog = 1.0;
    DrawRectangle(pbX, pbY, (int)(pbW * prog), pbH, (Color){0,180,255,255});
    DrawCircle(pbX + (int)(pbW * prog), pbY + pbH/2, 9, WHITE);

    if (clicked && mp.y >= pbY-12 && mp.y <= pbY+pbH+12 && mp.x >= pbX && mp.x <= pbX+pbW) {
        double ratio = (double)(mp.x - pbX) / (double)pbW;
        if (ratio < 0.0) ratio = 0.0;
        if (ratio > 1.0) ratio = 1.0;
        if (fmtCtx && vpDuration > 0.0) vp_seek_to(ratio * vpDuration);
    }

    int cm = (int)(vpElapsed/60), cs = (int)fmod(vpElapsed,60);
    int tm = (int)(vpDuration/60), ts = (int)fmod(vpDuration,60);
    char tStr[64]; sprintf(tStr, "%02d:%02d / %02d:%02d", cm, cs, tm, ts);
    DrawText(tStr, pbX, pbY+17, 17, (Color){200,200,200,255});

    int btnY = barY + 40, btnH = 38, btnW = 64;
    int cx   = sw / 2, gap = 10;

    if (draw_button(mp, clicked, cx - btnW*3 - gap*3, btnY, btnW, btnH, "|<<"))
        vp_seek_to(0.0);
    if (draw_button(mp, clicked, cx - btnW*2 - gap*2, btnY, btnW, btnH, "-10s"))
        vp_seek_to(vpElapsed - 10.0);
    if (draw_button(mp, clicked, cx - btnW/2, btnY, btnW, btnH, vpPaused ? "PLAY" : "PAUSE")) {
        if (vpPaused) {
            vpPlayStart += GetTime() - vpPauseAt;
            vpPaused = false;
            if (audioReady) PlayAudioStream(audioSt);
        } else {
            vpPauseAt = GetTime();
            vpPaused  = true;
            if (audioReady) StopAudioStream(audioSt);
        }
    }
    if (IsKeyPressed(KEY_SPACE)) {
        if (vpPaused) {
            vpPlayStart += GetTime() - vpPauseAt;
            vpPaused = false;
            if (audioReady) PlayAudioStream(audioSt);
        } else {
            vpPauseAt = GetTime();
            vpPaused  = true;
            if (audioReady) StopAudioStream(audioSt);
        }
    }
    if (draw_button(mp, clicked, cx + btnW/2 + gap, btnY, btnW, btnH, "+10s"))
        vp_seek_to(vpElapsed + 10.0);
    if (draw_button(mp, clicked, cx + btnW + gap*2 + btnW/2, btnY, btnW, btnH, ">>|"))
        vpWantsClose = true;

    int volX = sw - 230, volY = btnY;
    if (draw_button(mp, clicked, volX, volY, 50, btnH, "VOL-")) {
        vpVolume -= 0.1f; if (vpVolume < 0) vpVolume = 0;
        if (audioReady) SetAudioStreamVolume(audioSt, vpVolume);
    }
    int vbX = volX+58, vbY = volY+6, vbW = 60, vbH = btnH-12;
    DrawRectangle(vbX, vbY, vbW, vbH, (Color){40,40,60,200});
    DrawRectangle(vbX, vbY, (int)(vbW*vpVolume), vbH, (Color){0,200,100,255});
    DrawRectangleLines(vbX, vbY, vbW, vbH, (Color){90,90,110,255});
    if (draw_button(mp, clicked, vbX+vbW+8, volY, 50, btnH, "VOL+")) {
        vpVolume += 0.1f; if (vpVolume > 1) vpVolume = 1;
        if (audioReady) SetAudioStreamVolume(audioSt, vpVolume);
    }

    if (vpPaused) {
        int pW = MeasureText("|| PAUSADO", 42);
        DrawText("|| PAUSADO", (sw-pW)/2, sh/2-21, 42, (Color){255,255,255,190});
    }
}

// ============================================================
// SEEK HELPER
// ============================================================
static void vp_seek_to(double seekSec) {
    if (seekSec < 0.0) seekSec = 0.0;
    if (fmtCtx && vpDuration > 0.0) {
        int64_t seekTs = (int64_t)(seekSec * AV_TIME_BASE);
        av_seek_frame(fmtCtx, -1, seekTs, AVSEEK_FLAG_BACKWARD);
    }
    if (vCtx) avcodec_flush_buffers(vCtx);
    if (aCtx) avcodec_flush_buffers(aCtx);
    pq_flush(&vQueue); pq_flush(&aQueue);
    // Drain picture queue (under lock, then wake decode thread)
    SDL_LockMutex(pq.mutex);
    for (int i = 0; i < PICTURE_Q_SIZE; i++) { pq.pic[i].used = false; }
    pq.rindex = pq.windex = pq.size = 0;
    SDL_UnlockMutex(pq.mutex);
    picq_wake_all();
    audioRingWr = audioRingRd = audioRingAvail = 0;
    audioFramesSent = (long long)(seekSec * 44100.0);
    vpPlayStart = GetTime() - seekSec;
    vpElapsed   = seekSec;
    nextVidTime = seekSec;
}

// ============================================================
// PUBLIC: CLOSE, STATUS, VOLUME
// ============================================================
void VideoPlayer_Close(void) { vp_cleanup(); }
bool VideoPlayer_IsPlaying(void) { return vpOpen && !vpWantsClose; }
bool VideoPlayer_WantsClose(void) { return vpWantsClose; }
void VideoPlayer_SetVolume(float v) {
    vpVolume = v;
    if (audioReady) SetAudioStreamVolume(audioSt, v);
}
const char* VideoPlayer_LastError(void) { return vpLastError[0] ? vpLastError : "Nao foi possivel reproduzir o video"; }
bool VideoPlayer_HouveErro(void) { return vpLastError[0] != 0; }
