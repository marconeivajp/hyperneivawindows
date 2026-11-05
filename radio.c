#include "radio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>

char radioNames[500][128];
char radioUrls[500][256];
char radioFavicons[500][512];
char radioTags[500][256];
char radioCodecs[500][32];
int radioBitrates[500];
int radioCount = 0;

static AVFormatContext* fmtCtx = NULL;
static AVCodecContext* codecCtx = NULL;
static SwrContext* swrCtx = NULL;
static int streamIdx = -1;
static AVFrame* frame = NULL;
static AVPacket* packet = NULL;
static uint8_t* resampleBuf = NULL;

static volatile bool isPlaying = false;
static volatile bool isBuffering = false;
static volatile bool shouldStop = false;
static char radioStatus[128] = "Parado";

#define RING_BUFFER_SIZE (44100 * 2 * 2 * 4) // ~4 seconds of 16-bit stereo PCM
static uint8_t* ringBuffer = NULL;
static volatile int ringWrite = 0;
static volatile int ringRead = 0;
static volatile int ringCount = 0;
static pthread_mutex_t ringMutex;

static pthread_t threadId;
static char currentUrl[512] = "";

void radio_init() {
    avformat_network_init();
    ringBuffer = (uint8_t*)malloc(RING_BUFFER_SIZE);
    pthread_mutex_init(&ringMutex, NULL);
}

void radio_close() {
    radio_stop();
    if (ringBuffer) { free(ringBuffer); ringBuffer = NULL; }
    pthread_mutex_destroy(&ringMutex);
    avformat_network_deinit();
}

static void cleanup_ffmpeg() {
    if (swrCtx) { swr_free(&swrCtx); swrCtx = NULL; }
    if (codecCtx) { avcodec_free_context(&codecCtx); codecCtx = NULL; }
    if (fmtCtx) { avformat_close_input(&fmtCtx); fmtCtx = NULL; }
    if (frame) { av_frame_free(&frame); frame = NULL; }
    if (packet) { av_packet_free(&packet); packet = NULL; }
    if (resampleBuf) { av_free(resampleBuf); resampleBuf = NULL; }
    streamIdx = -1;
}

static void* radio_thread_func(void* arg) {
    strcpy(radioStatus, "Conectando...");
    
    fmtCtx = avformat_alloc_context();
    
    if (avformat_open_input(&fmtCtx, currentUrl, NULL, NULL) < 0) {
        strcpy(radioStatus, "Erro na conexao");
        isPlaying = false; isBuffering = false; cleanup_ffmpeg(); return NULL;
    }
    
    if (shouldStop) { cleanup_ffmpeg(); return NULL; }
    
    strcpy(radioStatus, "Buscando streams...");
    if (avformat_find_stream_info(fmtCtx, NULL) < 0) {
        strcpy(radioStatus, "Erro nas streams");
        isPlaying = false; isBuffering = false; cleanup_ffmpeg(); return NULL;
    }
    
    streamIdx = -1;
    for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            streamIdx = i; break;
        }
    }
    
    if (streamIdx == -1) {
        strcpy(radioStatus, "Sem stream de audio");
        isPlaying = false; isBuffering = false; cleanup_ffmpeg(); return NULL;
    }
    
    AVCodecParameters* codecpar = fmtCtx->streams[streamIdx]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        strcpy(radioStatus, "Codec nao suportado");
        isPlaying = false; isBuffering = false; cleanup_ffmpeg(); return NULL;
    }
    
    codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, codecpar);
    if (avcodec_open2(codecCtx, codec, NULL) < 0) {
        strcpy(radioStatus, "Falha ao abrir codec");
        isPlaying = false; isBuffering = false; cleanup_ffmpeg(); return NULL;
    }
    
    AVChannelLayout ch_layout;
    av_channel_layout_copy(&ch_layout, &codecCtx->ch_layout);
    if (ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
        av_channel_layout_default(&ch_layout, codecCtx->ch_layout.nb_channels);
    }
    
    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, 2);
    
    swr_alloc_set_opts2(&swrCtx, 
                        &out_ch_layout, AV_SAMPLE_FMT_S16, 44100,
                        &ch_layout, codecCtx->sample_fmt, codecCtx->sample_rate, 
                        0, NULL);
                                
    swr_init(swrCtx);
    
    frame = av_frame_alloc();
    packet = av_packet_alloc();
    resampleBuf = (uint8_t*)av_malloc(192000);
    
    isBuffering = true;
    strcpy(radioStatus, "Carregando Buffer...");
    
    while (!shouldStop) {
        bool hasSpace = false;
        pthread_mutex_lock(&ringMutex);
        if (RING_BUFFER_SIZE - ringCount > 32000) hasSpace = true;
        pthread_mutex_unlock(&ringMutex);
        
        if (!hasSpace) {
            usleep(10000); // 10ms
            continue;
        }
        
        if (av_read_frame(fmtCtx, packet) >= 0) {
            if (packet->stream_index == streamIdx) {
                if (avcodec_send_packet(codecCtx, packet) == 0) {
                    while (avcodec_receive_frame(codecCtx, frame) == 0) {
                        int samples = swr_convert(swrCtx, &resampleBuf, 44100, (const uint8_t**)frame->data, frame->nb_samples);
                        if (samples > 0) {
                            int bytesToWrite = samples * 4;
                            pthread_mutex_lock(&ringMutex);
                            if (ringWrite + bytesToWrite <= RING_BUFFER_SIZE) {
                                memcpy(ringBuffer + ringWrite, resampleBuf, bytesToWrite);
                            } else {
                                int p1 = RING_BUFFER_SIZE - ringWrite;
                                int p2 = bytesToWrite - p1;
                                memcpy(ringBuffer + ringWrite, resampleBuf, p1);
                                memcpy(ringBuffer, resampleBuf + p1, p2);
                            }
                            ringWrite = (ringWrite + bytesToWrite) % RING_BUFFER_SIZE;
                            ringCount += bytesToWrite;
                            
                            if (isBuffering && ringCount > 44100 * 4 * 2) {
                                isBuffering = false;
                                strcpy(radioStatus, "Tocando");
                            }
                            
                            pthread_mutex_unlock(&ringMutex);
                        }
                    }
                }
            }
            av_packet_unref(packet);
        } else {
            strcpy(radioStatus, "Desconectado do stream");
            break;
        }
    }
    
    cleanup_ffmpeg();
    isPlaying = false;
    return NULL;
}

void radio_stop() {
    if (isPlaying || isBuffering) {
        shouldStop = true;
        pthread_join(threadId, NULL);
        isPlaying = false;
        isBuffering = false;
    }
    pthread_mutex_lock(&ringMutex);
    ringWrite = 0; ringRead = 0; ringCount = 0;
    pthread_mutex_unlock(&ringMutex);
    strcpy(radioStatus, "Parado");
}

void radio_play(const char* url) {
    radio_stop();
    strcpy(currentUrl, url);
    shouldStop = false;
    isPlaying = true;
    isBuffering = true;
    
    // Attempt to download favicon for the current url
    int currentIdx = -1;
    for (int i = 0; i < radioCount; i++) {
        if (strcmp(radioUrls[i], url) == 0) { currentIdx = i; break; }
    }
    remove("data/HyperNeiva/current_radio_logo.jpg");
    remove("data/HyperNeiva/current_radio_logo.png");
    if (currentIdx != -1 && strlen(radioFavicons[currentIdx]) > 4) {
        char cmd[1024];
        sprintf(cmd, "curl -s \"%s\" -o data/HyperNeiva/current_radio_logo.jpg", radioFavicons[currentIdx]);
        exec_silent(cmd);
    }
    
    pthread_create(&threadId, NULL, radio_thread_func, NULL);
}

int radio_read_pcm(int16_t* outSamples, int maxFrames) {
    if (!isPlaying || isBuffering) return 0;
    
    int maxBytes = maxFrames * 4;
    int bytesRead = 0;
    
    pthread_mutex_lock(&ringMutex);
    if (ringCount >= maxBytes) {
        if (ringRead + maxBytes <= RING_BUFFER_SIZE) {
            memcpy(outSamples, ringBuffer + ringRead, maxBytes);
        } else {
            int p1 = RING_BUFFER_SIZE - ringRead;
            int p2 = maxBytes - p1;
            memcpy(outSamples, ringBuffer + ringRead, p1);
            memcpy((uint8_t*)outSamples + p1, ringBuffer, p2);
        }
        ringRead = (ringRead + maxBytes) % RING_BUFFER_SIZE;
        ringCount -= maxBytes;
        bytesRead = maxBytes;
    }
    pthread_mutex_unlock(&ringMutex);
    
    return bytesRead / 4;
}

bool radio_is_playing() { return isPlaying; }
bool radio_is_buffering() { return isBuffering; }
void radio_get_status(char* outMsg) { strcpy(outMsg, radioStatus); }

void radio_get_metadata(char* outTags, char* outCodec, int* outBitrate) {
    if (outTags) outTags[0] = '\0';
    if (outCodec) outCodec[0] = '\0';
    if (outBitrate) *outBitrate = 0;
    
    for (int i = 0; i < radioCount; i++) {
        if (strcmp(radioUrls[i], currentUrl) == 0) {
            if (outTags) strcpy(outTags, radioTags[i]);
            if (outCodec) strcpy(outCodec, radioCodecs[i]);
            if (outBitrate) *outBitrate = radioBitrates[i];
            break;
        }
    }
}

static void parse_radio_json() {
    FILE* f = fopen("data/HyperNeiva/radio_api.json", "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buffer = (char*)malloc(fsize + 1);
    if (!buffer) { fclose(f); return; }
    fread(buffer, 1, fsize, f);
    buffer[fsize] = '\0';
    fclose(f);

    radioCount = 0;
    strcpy(radioNames[0], "Voltar");
    strcpy(radioUrls[0], "VOLTAR");
    radioFavicons[0][0] = '\0';
    radioTags[0][0] = '\0';
    radioCodecs[0][0] = '\0';
    radioBitrates[0] = 0;
    radioCount++;

    char* ptr = buffer;
    while (radioCount < 500 && ptr != NULL) {
        char* namePtr = strstr(ptr, "\"name\":\"");
        if (!namePtr) break;
        char* streamPtr = strstr(namePtr, "\"url_resolved\":\"");
        if (!streamPtr) break;
        
        char* favPtr = strstr(namePtr, "\"favicon\":\"");
        char* tagPtr = strstr(namePtr, "\"tags\":\"");
        char* codecPtr = strstr(namePtr, "\"codec\":\"");
        char* bitratePtr = strstr(namePtr, "\"bitrate\":");

        namePtr += 8;
        char* nameEnd = strchr(namePtr, '\"');
        if (!nameEnd) break;

        streamPtr += 16;
        char* streamEnd = strchr(streamPtr, '\"');
        if (!streamEnd) break;

        int nameLen = nameEnd - namePtr;
        if (nameLen > 127) nameLen = 127;
        strncpy(radioNames[radioCount], namePtr, nameLen);
        radioNames[radioCount][nameLen] = '\0';

        int streamLen = streamEnd - streamPtr;
        if (streamLen > 255) streamLen = 255;
        strncpy(radioUrls[radioCount], streamPtr, streamLen);
        radioUrls[radioCount][streamLen] = '\0';
        
        if (favPtr && favPtr < strstr(streamEnd, "\"stationuuid\"") && strstr(streamEnd, "\"stationuuid\"") != NULL || favPtr) {
            favPtr += 11;
            char* favEnd = strchr(favPtr, '\"');
            if (favEnd) {
                int favLen = favEnd - favPtr;
                if (favLen > 511) favLen = 511;
                strncpy(radioFavicons[radioCount], favPtr, favLen);
                radioFavicons[radioCount][favLen] = '\0';
            } else { radioFavicons[radioCount][0] = '\0'; }
        } else { radioFavicons[radioCount][0] = '\0'; }
        
        if (tagPtr && tagPtr < strstr(streamEnd, "\"stationuuid\"") && strstr(streamEnd, "\"stationuuid\"") != NULL || tagPtr) {
            tagPtr += 8;
            char* tagEnd = strchr(tagPtr, '\"');
            if (tagEnd) {
                int tagLen = tagEnd - tagPtr;
                if (tagLen > 255) tagLen = 255;
                strncpy(radioTags[radioCount], tagPtr, tagLen);
                radioTags[radioCount][tagLen] = '\0';
            } else { radioTags[radioCount][0] = '\0'; }
        } else { radioTags[radioCount][0] = '\0'; }
        
        if (codecPtr && codecPtr < strstr(streamEnd, "\"stationuuid\"") && strstr(streamEnd, "\"stationuuid\"") != NULL || codecPtr) {
            codecPtr += 9;
            char* codecEnd = strchr(codecPtr, '\"');
            if (codecEnd) {
                int codecLen = codecEnd - codecPtr;
                if (codecLen > 31) codecLen = 31;
                strncpy(radioCodecs[radioCount], codecPtr, codecLen);
                radioCodecs[radioCount][codecLen] = '\0';
            } else { radioCodecs[radioCount][0] = '\0'; }
        } else { radioCodecs[radioCount][0] = '\0'; }
        
        if (bitratePtr && bitratePtr < strstr(streamEnd, "\"stationuuid\"") && strstr(streamEnd, "\"stationuuid\"") != NULL || bitratePtr) {
            bitratePtr += 10;
            radioBitrates[radioCount] = atoi(bitratePtr);
        } else { radioBitrates[radioCount] = 0; }

        radioCount++;
        ptr = streamEnd;
    }
    free(buffer);
}

#define INITGUID
#include <windows.h>

void exec_silent(const char* cmd) {
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char cmdCopy[4100];
    // Use cmd /c with the command as-is (no extra outer quotes to avoid breaking inner quotes)
    snprintf(cmdCopy, sizeof(cmdCopy), "cmd /c %s", cmd);
    if (CreateProcess(NULL, cmdCopy, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

void exec_async_silent(const char* cmd) {
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char cmdCopy[4100];
    snprintf(cmdCopy, sizeof(cmdCopy), "cmd /c %s", cmd);
    if (CreateProcess(NULL, cmdCopy, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

static void* _exec_async_capture_thread(void* arg) {
    struct CapArgs { const char* cmd; char* out; size_t outsz; } *p = arg;
    FILE* fp = popen(p->cmd, "r");
    if (fp) {
        size_t total = 0;
        while (!feof(fp) && total < p->outsz - 1) {
            size_t r = fread(p->out + total, 1, p->outsz - 1 - total, fp);
            if (r == 0) break;
            total += r;
        }
        p->out[total] = '\0';
        pclose(fp);
    } else {
        p->out[0] = '\0';
    }
    free(arg);
    return NULL;
}

void exec_async_capture(const char* cmd, char* outBuf, size_t outBufSize) {
    struct CapArgs { const char* cmd; char* out; size_t outsz; } *p = malloc(sizeof(*p));
    if (!p) { outBuf[0] = '\0'; return; }
    p->cmd = cmd; p->out = outBuf; p->outsz = outBufSize;
    pthread_t th;
    pthread_create(&th, NULL, _exec_async_capture_thread, p);
    pthread_detach(th);
}

#include <tlhelp32.h>
#define COBJMACROS
#include <mmdeviceapi.h>
#include <endpointvolume.h>
static HANDLE grav_ffmpeg_process = NULL;
static HANDLE grav_ffmpeg_stdin = NULL;
static char grav_last_error[256] = "";

#define GRAV_PREVIEW_W 320
#define GRAV_PREVIEW_H 240
#define GRAV_PREVIEW_FRAME (GRAV_PREVIEW_W * GRAV_PREVIEW_H * 4)

typedef struct RLImage {
    void *data;
    int width;
    int height;
    int mipmaps;
    int format;
} RLImage;
#define RLFORMAT_R8G8B8A8 7
extern bool ExportImage(RLImage image, const char *fileName);

static unsigned char g_previewBuf[GRAV_PREVIEW_FRAME];
static bool g_previewHasFrame = false;
static bool g_previewNewFrame = false;
static bool g_previewRunning = false;
static pthread_mutex_t g_previewMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_previewThread;
static HANDLE g_previewPipeRead = NULL;

static void* grav_preview_reader(void* arg) {
    (void)arg;
    unsigned char tmp[GRAV_PREVIEW_FRAME];
    while (g_previewRunning) {
        size_t got = 0;
        while (got < GRAV_PREVIEW_FRAME) {
            DWORD readN = 0;
            if (!ReadFile(g_previewPipeRead, tmp + got, (DWORD)(GRAV_PREVIEW_FRAME - got), &readN, NULL)) {
                got = 0;
                break;
            }
            if (readN == 0) { got = 0; break; }
            got += readN;
        }
        if (got == 0) break;
        pthread_mutex_lock(&g_previewMutex);
        memcpy(g_previewBuf, tmp, GRAV_PREVIEW_FRAME);
        g_previewHasFrame = true;
        g_previewNewFrame = true;
        pthread_mutex_unlock(&g_previewMutex);
    }
    return NULL;
}

int grav_video_preview_w(void) { return GRAV_PREVIEW_W; }
int grav_video_preview_h(void) { return GRAV_PREVIEW_H; }

int grav_video_preview_active(void) { return g_previewRunning ? 1 : 0; }

int grav_video_preview_get(unsigned char* outBuf, int bufSize) {
    if (!g_previewHasFrame || bufSize < GRAV_PREVIEW_FRAME) return 0;
    int gotNew = 0;
    pthread_mutex_lock(&g_previewMutex);
    if (g_previewNewFrame) {
        memcpy(outBuf, g_previewBuf, GRAV_PREVIEW_FRAME);
        g_previewNewFrame = false;
        gotNew = 1;
    }
    pthread_mutex_unlock(&g_previewMutex);
    return gotNew;
}

bool grav_capture_camera_to_png(const char* outputPath) {
    if (!g_previewHasFrame) return false;
    unsigned char frameBuf[GRAV_PREVIEW_FRAME];
    pthread_mutex_lock(&g_previewMutex);
    memcpy(frameBuf, g_previewBuf, GRAV_PREVIEW_FRAME);
    pthread_mutex_unlock(&g_previewMutex);

    RLImage img = {
        .data = frameBuf,
        .width = GRAV_PREVIEW_W,
        .height = GRAV_PREVIEW_H,
        .mipmaps = 1,
        .format = RLFORMAT_R8G8B8A8
    };
    return ExportImage(img, outputPath);
}

bool grav_capture_screen_to_png(const char* outputPath, bool activeWindowOnly) {
    HWND hwnd = activeWindowOnly ? GetForegroundWindow() : GetDesktopWindow();
    if (!hwnd) hwnd = GetDesktopWindow();

    RECT rc;
    GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
        w = GetSystemMetrics(SM_CXSCREEN);
        h = GetSystemMetrics(SM_CYSCREEN);
        rc.left = 0; rc.top = 0;
    }

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, w, h);
    HGDIOBJ hOld = SelectObject(hdcMem, hbm);

    BitBlt(hdcMem, 0, 0, w, h, hdcScreen, rc.left, rc.top, SRCCOPY);

    BITMAPINFOHEADER bi = {0};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = w;
    bi.biHeight = -h;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    unsigned char* pixels = (unsigned char*)malloc(w * h * 4);
    if (!pixels) {
        SelectObject(hdcMem, hOld);
        DeleteObject(hbm);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return false;
    }
    GetDIBits(hdcMem, hbm, 0, h, pixels, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    SelectObject(hdcMem, hOld);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    for (int i = 0; i < w * h * 4; i += 4) {
        unsigned char b = pixels[i];
        pixels[i] = pixels[i+2];
        pixels[i+2] = b;
        pixels[i+3] = 255;
    }

    RLImage img = {
        .data = pixels,
        .width = w,
        .height = h,
        .mipmaps = 1,
        .format = RLFORMAT_R8G8B8A8
    };

    bool ok = ExportImage(img, outputPath);
    free(pixels);
    return ok;
}

const char* grav_last_error_msg() { return grav_last_error; }

static long grav_file_size(const char* path);
static bool try_start_ffmpeg(const char* ffmpegPath, const char* deviceName, const char* outputFile) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s -y -f dshow -audio_buffer_size 100 -i audio=\"%s\" -f s16le -ac 1 -ar 44100 \"%s\"", ffmpegPath, deviceName, outputFile);
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hInRead = NULL, hInWrite = NULL;
    if (!CreatePipe(&hInRead, &hInWrite, &sa, 0)) {
        snprintf(grav_last_error, sizeof(grav_last_error), "Falha ao criar pipe de gravacao.");
        return false;
    }
    STARTUPINFO si = { sizeof(si), 0 };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = hInRead;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;
    if (!CreateProcess(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hInRead);
        CloseHandle(hInWrite);
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(hInRead);
    grav_ffmpeg_process = pi.hProcess;
    grav_ffmpeg_stdin = hInWrite;
    return true;
}

static bool grav_probe_device(const char* ffmpegPath, const char* deviceName) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s -y -f dshow -audio_buffer_size 100 -i audio=\"%s\" -t 1 -f null NUL", ffmpegPath, deviceName);
    STARTUPINFO si = { sizeof(si), 0 };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;
    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return false;
    }
    CloseHandle(pi.hThread);
    DWORD wait = WaitForSingleObject(pi.hProcess, 10000);
    if (wait != WAIT_OBJECT_0) {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        return false;
    }
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    return code == 0;
}

void grav_get_exe_dir(char* out, int maxLen) {
    out[0] = '\0';
    HMODULE hm = GetModuleHandleA(NULL);
    if (hm) {
        GetModuleFileNameA(hm, out, maxLen - 1);
        char* slash = strrchr(out, '\\');
        if (slash) *slash = '\0';
    }
}

static void grav_ensure_dir_for_file(const char* filePath) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", filePath);
    int n = (int)strlen(tmp);
    for (int i = 0; i < n; i++) {
        if (tmp[i] == '\\') tmp[i] = '/';
    }
    for (int i = 0; i < n; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (tmp[0] != '\0') CreateDirectoryA(tmp, NULL);
            tmp[i] = '/';
        }
    }
}

bool grav_start_recording(const char* outputFile) {
    grav_stop_recording();
    grav_last_error[0] = 0;
    grav_ensure_dir_for_file(outputFile);

    const char* paths[] = {
        "C:\\msys64\\mingw64\\bin\\ffmpeg.exe",
        "C:\\msys64\\usr\\bin\\ffmpeg.exe",
        "ffmpeg.exe"
    };
    const char* devices[] = { "Microfone (Realtek Audio)", "Microfone", "Microphone", "Microfone (Realtek High Definition Audio)", "Microphone (Realtek High Definition Audio)" };
    const char* foundPath = NULL;
    char tryPath[512];

    for (int p = 0; p < 3; p++) {
        if (p == 2) {
            // Just try "ffmpeg" directly
            strcpy(tryPath, "ffmpeg.exe");
        } else {
            strcpy(tryPath, paths[p]);
        }
        // Check if file exists
        FILE* f = fopen(tryPath, "rb");
        if (f) { fclose(f); foundPath = tryPath; break; }
    }

    if (!foundPath) {
        // On PATH fallback
        foundPath = "ffmpeg.exe";
    }

    const char* foundDevice = NULL;
    int nDevices = (int)(sizeof(devices) / sizeof(devices[0]));
    for (int d = 0; d < nDevices; d++) {
        if (grav_probe_device(foundPath, devices[d])) {
            foundDevice = devices[d];
            break;
        }
    }

    if (!foundDevice) {
        snprintf(grav_last_error, sizeof(grav_last_error), "Nenhum microfone encontrado. Verifique se o microfone esta conectado e liberado para o sistema.");
        return false;
    }

    return try_start_ffmpeg(foundPath, foundDevice, outputFile);
}

static const char* find_ffmpeg_path(void) {
    static char ffPath[512] = "";
    if (ffPath[0]) return ffPath;
    const char* candidates[] = {
        "C:\\msys64\\mingw64\\bin\\ffmpeg.exe",
        "C:\\msys64\\usr\\bin\\ffmpeg.exe",
        "ffmpeg.exe"
    };
    for (int i = 0; i < 3; i++) {
        FILE* f = fopen(candidates[i], "rb");
        if (f) { fclose(f); strcpy(ffPath, candidates[i]); return ffPath; }
    }
    strcpy(ffPath, "ffmpeg.exe");
    return ffPath;
}

bool grav_finish_recording(const char* rawPath, const char* finalPath) {
    grav_ensure_dir_for_file(finalPath);
    const char* ff = find_ffmpeg_path();
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "\"%s\" -y -f s16le -ac 1 -ar 44100 -i \"%s\" -c:a pcm_s16le \"%s\"", ff, rawPath, finalPath);
    STARTUPINFO si = { sizeof(si), 0 };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;
    if (CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 15000);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    FILE* f = fopen(finalPath, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fclose(f);
        return sz > 100;
    }
    return false;
}

static void grav_resume_all_threads() {
    if (!grav_ffmpeg_process) return;
    DWORD code = 0;
    if (GetExitCodeProcess(grav_ffmpeg_process, &code) && code != STILL_ACTIVE) return;
    DWORD procId = GetProcessId(grav_ffmpeg_process);
    if (procId == 0 || procId == GetCurrentProcessId()) return;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te = { sizeof(te) };
    if (Thread32First(hSnap, &te)) {
        do {
            if (te.th32OwnerProcessID == procId) {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (hThread) { ResumeThread(hThread); CloseHandle(hThread); }
            }
        } while (Thread32Next(hSnap, &te));
    }
    CloseHandle(hSnap);
}

bool grav_screen_start_recording(const char* outputFile) {
    grav_stop_recording();
    grav_last_error[0] = 0;
    grav_ensure_dir_for_file(outputFile);

    const char* paths[] = {
        "C:\\msys64\\mingw64\\bin\\ffmpeg.exe",
        "C:\\msys64\\usr\\bin\\ffmpeg.exe",
        "ffmpeg.exe"
    };
    char tryPath[512];
    const char* foundPath = NULL;
    for (int p = 0; p < 3; p++) {
        if (p == 2) strcpy(tryPath, "ffmpeg.exe");
        else strcpy(tryPath, paths[p]);
        FILE* f = fopen(tryPath, "rb");
        if (f) { fclose(f); foundPath = tryPath; break; }
    }
    if (!foundPath) foundPath = "ffmpeg.exe";

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s -y -hide_banner -loglevel error -f gdigrab -framerate 30 -i desktop -c:v libx264 -preset ultrafast -pix_fmt yuv420p -crf 23 -t 36000 \"%s\" -f rawvideo -pix_fmt rgba -s %dx%d -", foundPath, outputFile, GRAV_PREVIEW_W, GRAV_PREVIEW_H);

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hInRead = NULL, hInWrite = NULL;
    HANDLE hOutRead = NULL, hOutWrite = NULL;
    if (!CreatePipe(&hInRead, &hInWrite, &sa, 0)) {
        snprintf(grav_last_error, sizeof(grav_last_error), "Falha ao criar pipe de gravacao de tela.");
        return false;
    }
    if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0)) {
        CloseHandle(hInRead);
        CloseHandle(hInWrite);
        snprintf(grav_last_error, sizeof(grav_last_error), "Falha ao criar pipe de preview de tela.");
        return false;
    }
    STARTUPINFO si = { sizeof(si), 0 };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = hInRead;
    si.hStdOutput = hOutWrite;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;
    if (CreateProcess(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(hInRead);
        CloseHandle(hOutWrite);
        grav_ffmpeg_process = pi.hProcess;
        grav_ffmpeg_stdin = hInWrite;
        g_previewPipeRead = hOutRead;
        g_previewRunning = true;
        g_previewHasFrame = false;
        g_previewNewFrame = false;
        pthread_create(&g_previewThread, NULL, grav_preview_reader, NULL);
        return true;
    }
    DWORD err = GetLastError();
    CloseHandle(hInRead);
    CloseHandle(hInWrite);
    CloseHandle(hOutRead);
    CloseHandle(hOutWrite);
    snprintf(grav_last_error, sizeof(grav_last_error), "Falha ao iniciar gravacao de tela (erro %lu).", (unsigned long)err);
    return false;
}

static bool grav_video_probe_device(const char* ffmpegPath, const char* deviceName) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s -y -f dshow -i video=\"%s\" -t 1 -f null NUL", ffmpegPath, deviceName);
    STARTUPINFO si = { sizeof(si), 0 };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;
    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return false;
    }
    CloseHandle(pi.hThread);
    DWORD wait = WaitForSingleObject(pi.hProcess, 6000);
    if (wait != WAIT_OBJECT_0) {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        return false;
    }
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    return code == 0;
}

bool grav_video_start_recording(const char* outputFile) {
    grav_stop_recording();
    grav_last_error[0] = 0;
    grav_ensure_dir_for_file(outputFile);

    const char* paths[] = {
        "C:\\msys64\\mingw64\\bin\\ffmpeg.exe",
        "C:\\msys64\\usr\\bin\\ffmpeg.exe",
        "ffmpeg.exe"
    };
    const char* devices[] = { "Integrated Webcam", "USB Camera", "USB 2.0 Camera", "Cam", "Webcam", "Camera" };
    const char* foundPath = NULL;
    char tryPath[512];

    for (int p = 0; p < 3; p++) {
        if (p == 2) strcpy(tryPath, "ffmpeg.exe");
        else strcpy(tryPath, paths[p]);
        FILE* f = fopen(tryPath, "rb");
        if (f) { fclose(f); foundPath = tryPath; break; }
    }
    if (!foundPath) foundPath = "ffmpeg.exe";

    const char* foundDevice = NULL;
    int nDevices = (int)(sizeof(devices) / sizeof(devices[0]));
    for (int d = 0; d < nDevices; d++) {
        if (grav_video_probe_device(foundPath, devices[d])) {
            foundDevice = devices[d];
            break;
        }
    }

    if (!foundDevice) {
        snprintf(grav_last_error, sizeof(grav_last_error), "Nenhuma webcam encontrada. Conecte uma camera e tente novamente.");
        return false;
    }

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s -y -hide_banner -loglevel error -f dshow -i video=\"%s\" -c:v libx264 -preset ultrafast -pix_fmt yuv420p -crf 23 -t 36000 \"%s\" -f rawvideo -pix_fmt rgba -s %dx%d -", foundPath, foundDevice, outputFile, GRAV_PREVIEW_W, GRAV_PREVIEW_H);

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hInRead = NULL, hInWrite = NULL;
    HANDLE hOutRead = NULL, hOutWrite = NULL;
    if (!CreatePipe(&hInRead, &hInWrite, &sa, 0)) {
        snprintf(grav_last_error, sizeof(grav_last_error), "Falha ao criar pipe de gravacao de video.");
        return false;
    }
    if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0)) {
        CloseHandle(hInRead);
        CloseHandle(hInWrite);
        snprintf(grav_last_error, sizeof(grav_last_error), "Falha ao criar pipe de preview de video.");
        return false;
    }
    STARTUPINFO si = { sizeof(si), 0 };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = hInRead;
    si.hStdOutput = hOutWrite;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;
    if (CreateProcess(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(hInRead);
        CloseHandle(hOutWrite);
        grav_ffmpeg_process = pi.hProcess;
        grav_ffmpeg_stdin = hInWrite;
        g_previewPipeRead = hOutRead;
        g_previewRunning = true;
        g_previewHasFrame = false;
        g_previewNewFrame = false;
        pthread_create(&g_previewThread, NULL, grav_preview_reader, NULL);
        return true;
    }
    DWORD err = GetLastError();
    CloseHandle(hInRead);
    CloseHandle(hInWrite);
    CloseHandle(hOutRead);
    CloseHandle(hOutWrite);
    snprintf(grav_last_error, sizeof(grav_last_error), "Falha ao iniciar gravacao de video (erro %lu).", (unsigned long)err);
    return false;
}

bool grav_print_preview_start(int source) {
    grav_stop_recording();
    grav_last_error[0] = 0;

    const char* foundPath = find_ffmpeg_path();

    char cmd[1024];
    if (source == 0) {
        const char* devices[] = { "Integrated Webcam", "USB Camera", "USB 2.0 Camera", "Cam", "Webcam", "Camera" };
        const char* foundDevice = NULL;
        int nDevices = (int)(sizeof(devices) / sizeof(devices[0]));
        for (int d = 0; d < nDevices; d++) {
            if (grav_video_probe_device(foundPath, devices[d])) {
                foundDevice = devices[d];
                break;
            }
        }
        if (!foundDevice) {
            snprintf(grav_last_error, sizeof(grav_last_error), "Nenhuma webcam encontrada. Conecte uma camera e tente novamente.");
            return false;
        }
        snprintf(cmd, sizeof(cmd), "%s -y -hide_banner -loglevel error -f dshow -i video=\"%s\" -f rawvideo -pix_fmt rgba -s %dx%d -",
            foundPath, foundDevice, GRAV_PREVIEW_W, GRAV_PREVIEW_H);
    } else {
        snprintf(cmd, sizeof(cmd), "%s -y -hide_banner -loglevel error -f gdigrab -framerate 10 -i desktop -vf scale=%d:%d -f rawvideo -pix_fmt rgba -",
            foundPath, GRAV_PREVIEW_W, GRAV_PREVIEW_H);
    }

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hOutRead = NULL, hOutWrite = NULL;
    if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0)) {
        snprintf(grav_last_error, sizeof(grav_last_error), "Falha ao criar pipe de preview.");
        return false;
    }
    STARTUPINFO si = { sizeof(si), 0 };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = hOutWrite;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;
    if (CreateProcess(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(hOutWrite);
        grav_ffmpeg_process = pi.hProcess;
        grav_ffmpeg_stdin = NULL;
        g_previewPipeRead = hOutRead;
        g_previewRunning = true;
        g_previewHasFrame = false;
        g_previewNewFrame = false;
        pthread_create(&g_previewThread, NULL, grav_preview_reader, NULL);
        return true;
    }
    DWORD err = GetLastError();
    CloseHandle(hOutRead);
    CloseHandle(hOutWrite);
    snprintf(grav_last_error, sizeof(grav_last_error), "Falha ao iniciar preview (erro %lu).", (unsigned long)err);
    return false;
}

void grav_print_preview_stop(void) {
    if (grav_ffmpeg_process) {
        if (grav_ffmpeg_stdin) {
            DWORD written = 0;
            WriteFile(grav_ffmpeg_stdin, "q", 1, &written, NULL);
            if (WaitForSingleObject(grav_ffmpeg_process, 3000) != WAIT_OBJECT_0)
                TerminateProcess(grav_ffmpeg_process, 0);
            CloseHandle(grav_ffmpeg_stdin);
            grav_ffmpeg_stdin = NULL;
        } else {
            TerminateProcess(grav_ffmpeg_process, 0);
            WaitForSingleObject(grav_ffmpeg_process, 3000);
        }
        CloseHandle(grav_ffmpeg_process);
        grav_ffmpeg_process = NULL;
    }
    if (g_previewRunning) {
        g_previewRunning = false;
        if (g_previewPipeRead) {
            CloseHandle(g_previewPipeRead);
            g_previewPipeRead = NULL;
        }
        pthread_join(g_previewThread, NULL);
        g_previewHasFrame = false;
        g_previewNewFrame = false;
    }
}

bool grav_video_screen_start_recording(const char* outputFile) {
    grav_stop_recording();
    grav_last_error[0] = 0;
    grav_ensure_dir_for_file(outputFile);

    const char* foundPath = find_ffmpeg_path();
    const char* foundDevice = NULL;
    const char* devices[] = { "Integrated Webcam", "USB Camera", "USB 2.0 Camera", "Cam", "Webcam", "Camera" };
    int nDevices = (int)(sizeof(devices) / sizeof(devices[0]));
    for (int d = 0; d < nDevices; d++) {
        if (grav_video_probe_device(foundPath, devices[d])) {
            foundDevice = devices[d];
            break;
        }
    }
    if (!foundDevice) foundDevice = "Integrated Webcam";

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%s -y -hide_banner -loglevel error -f gdigrab -framerate 30 -i desktop -f dshow -i video=\"%s\" -filter_complex \"[0:v]scale=1280:720[desk];[1:v]scale=320:240[cam];[desk][cam]overlay=W-w-20:H-h-20[outv];[0:v]scale=%d:%d[preview]\" -map \"[outv]\" -c:v libx264 -preset ultrafast -pix_fmt yuv420p -crf 23 -t 36000 \"%s\" -map \"[preview]\" -f rawvideo -pix_fmt rgba -s %dx%d -",
        foundPath, foundDevice, GRAV_PREVIEW_W, GRAV_PREVIEW_H, outputFile, GRAV_PREVIEW_W, GRAV_PREVIEW_H);

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hInRead = NULL, hInWrite = NULL;
    HANDLE hOutRead = NULL, hOutWrite = NULL;
    if (!CreatePipe(&hInRead, &hInWrite, &sa, 0)) return false;
    if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0)) {
        CloseHandle(hInRead); CloseHandle(hInWrite);
        return false;
    }
    STARTUPINFO si = { sizeof(si), 0 };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = hInRead;
    si.hStdOutput = hOutWrite;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;
    if (CreateProcess(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(hInRead);
        CloseHandle(hOutWrite);
        grav_ffmpeg_process = pi.hProcess;
        grav_ffmpeg_stdin = hInWrite;
        g_previewPipeRead = hOutRead;
        g_previewRunning = true;
        g_previewHasFrame = false;
        g_previewNewFrame = false;
        pthread_create(&g_previewThread, NULL, grav_preview_reader, NULL);
        return true;
    }
    CloseHandle(hInRead); CloseHandle(hInWrite);
    CloseHandle(hOutRead); CloseHandle(hOutWrite);
    return false;
}

bool grav_concat_mp4(const char* part1, const char* part2, const char* out) {
    const char* ff = find_ffmpeg_path();
    char listPath[512];
    snprintf(listPath, sizeof(listPath), "%s_list.txt", out);
    FILE* f = fopen(listPath, "w");
    if (!f) return false;
    fprintf(f, "file '%s'\nfile '%s'\n", part1, part2);
    fclose(f);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "\"%s\" -y -f concat -safe 0 -i \"%s\" -c copy \"%s\"", ff, listPath, out);
    STARTUPINFO si = { sizeof(si), 0 };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;
    bool ok = false;
    if (CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 30000);
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        ok = (code == 0);
    }
    remove(listPath);
    return ok;
}

void grav_stop_recording() {
    if (grav_ffmpeg_process) {
        if (grav_ffmpeg_stdin) {
            grav_resume_all_threads();
            DWORD written = 0;
            WriteFile(grav_ffmpeg_stdin, "q", 1, &written, NULL);
            if (WaitForSingleObject(grav_ffmpeg_process, 8000) != WAIT_OBJECT_0) {
                TerminateProcess(grav_ffmpeg_process, 0);
                WaitForSingleObject(grav_ffmpeg_process, 3000);
            }
            CloseHandle(grav_ffmpeg_stdin);
            grav_ffmpeg_stdin = NULL;
        } else {
            TerminateProcess(grav_ffmpeg_process, 0);
            WaitForSingleObject(grav_ffmpeg_process, 5000);
        }
        CloseHandle(grav_ffmpeg_process);
        grav_ffmpeg_process = NULL;
    }
    if (g_previewRunning) {
        g_previewRunning = false;
        if (g_previewPipeRead) {
            CloseHandle(g_previewPipeRead);
            g_previewPipeRead = NULL;
        }
        pthread_join(g_previewThread, NULL);
        g_previewHasFrame = false;
        g_previewNewFrame = false;
    }
}

bool grav_is_recording() {
    if (!grav_ffmpeg_process) return false;
    DWORD code = 0;
    if (GetExitCodeProcess(grav_ffmpeg_process, &code)) {
        if (code == STILL_ACTIVE) return true;
    }
    return false;
}

static long grav_file_size(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    return sz;
}

long grav_temp_file_size(const char* path) {
    return grav_file_size(path);
}

void grav_pause_recording() {
    if (!grav_ffmpeg_process) return;
    DWORD code = 0;
    if (GetExitCodeProcess(grav_ffmpeg_process, &code) && code != STILL_ACTIVE) return;
    DWORD procId = GetProcessId(grav_ffmpeg_process);
    if (procId == 0 || procId == GetCurrentProcessId()) return;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te = { sizeof(te) };
    if (Thread32First(hSnap, &te)) {
        do {
            if (te.th32OwnerProcessID == procId) {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (hThread) { SuspendThread(hThread); CloseHandle(hThread); }
            }
        } while (Thread32Next(hSnap, &te));
    }
    CloseHandle(hSnap);
}

void grav_resume_recording() {
    grav_resume_all_threads();
}

void grav_kill_orphans() {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32 pe = { sizeof(pe) };
    if (Process32First(hSnap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, "ffmpeg.exe") == 0) {
                HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (h) { TerminateProcess(h, 0); CloseHandle(h); }
            }
        } while (Process32Next(hSnap, &pe));
    }
    CloseHandle(hSnap);
}

void grav_set_mic_mute(bool muted) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return;
    IMMDeviceEnumerator* pEnum = NULL;
    IMMDevice* pDev = NULL;
    IAudioEndpointVolume* pVol = NULL;
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void**)&pEnum);
    if (FAILED(hr) || !pEnum) { hr = E_FAIL; goto done; }
    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(pEnum, eCapture, eCommunications, &pDev);
    if (FAILED(hr) || !pDev) hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(pEnum, eCapture, eConsole, &pDev);
    if (FAILED(hr) || !pDev) { hr = E_FAIL; goto done; }
    hr = IMMDevice_Activate(pDev, &IID_IAudioEndpointVolume, CLSCTX_ALL, NULL, (void**)&pVol);
    if (FAILED(hr) || !pVol) { hr = E_FAIL; goto done; }
    IAudioEndpointVolume_SetMute(pVol, muted ? TRUE : FALSE, NULL);
done:
    if (pVol) IAudioEndpointVolume_Release(pVol);
    if (pDev) IMMDevice_Release(pDev);
    if (pEnum) IMMDeviceEnumerator_Release(pEnum);
    CoUninitialize();
}

void radio_fetch_top50() {
    char url[1024];
    sprintf(url, "curl -s \"http://all.api.radio-browser.info/json/stations/search?limit=50&order=clickcount&reverse=true\" > data/HyperNeiva/radio_api.json");
    exec_silent(url);
    parse_radio_json();
}
void radio_fetch_saopaulo() {
    char url[1024];
    sprintf(url, "curl -s \"http://all.api.radio-browser.info/json/stations/search?state=Sao%%20Paulo&limit=50&order=clickcount&reverse=true\" > data/HyperNeiva/radio_api.json");
    exec_silent(url);
    parse_radio_json();
}
void radio_fetch_podcast() {
    char url[1024];
    sprintf(url, "curl -s \"http://all.api.radio-browser.info/json/stations/search?tag=podcast&limit=50&order=clickcount&reverse=true\" > data/HyperNeiva/radio_api.json");
    exec_silent(url);
    parse_radio_json();
}

void radio_fetch_search(const char* query) {
    char url[1024];
    // Very simple url encoding (just replace spaces with %20)
    char encodedQuery[256] = {0};
    int j = 0;
    for (int i = 0; query[i] != '\0' && i < 255; i++) {
        if (query[i] == ' ') {
            encodedQuery[j++] = '%';
            encodedQuery[j++] = '2';
            encodedQuery[j++] = '0';
        } else {
            encodedQuery[j++] = query[i];
        }
    }
    encodedQuery[j] = '\0';
    sprintf(url, "curl -s \"http://all.api.radio-browser.info/json/stations/search?name=%s&limit=50&order=clickcount&reverse=true\" > data/HyperNeiva/radio_api.json", encodedQuery);
    exec_silent(url);
    parse_radio_json();
}

void radio_fetch_tag(const char* tag) {
    char url[1024];
    char encodedTag[256] = {0};
    int j = 0;
    for (int i = 0; tag[i] != '\0' && i < 255; i++) {
        if (tag[i] == ' ') {
            encodedTag[j++] = '%';
            encodedTag[j++] = '2';
            encodedTag[j++] = '0';
        } else {
            encodedTag[j++] = tag[i];
        }
    }
    encodedTag[j] = '\0';
    sprintf(url, "curl -s \"http://all.api.radio-browser.info/json/stations/search?tag=%s&limit=50&order=clickcount&reverse=true\" > data/HyperNeiva/radio_api.json", encodedTag);
    exec_silent(url);
    parse_radio_json();
}

void radio_fetch_country(const char* country) {
    char url[1024];
    char encoded[256] = {0};
    int j = 0;
    for (int i = 0; country[i] != '\0' && i < 255; i++) {
        if (country[i] == ' ') { encoded[j++] = '%'; encoded[j++] = '2'; encoded[j++] = '0'; }
        else { encoded[j++] = country[i]; }
    }
    encoded[j] = '\0';
    sprintf(url, "curl -s \"http://all.api.radio-browser.info/json/stations/search?country=%s&limit=50&order=clickcount&reverse=true\" > data/HyperNeiva/radio_api.json", encoded);
    exec_silent(url);
    parse_radio_json();
}

void radio_fetch_state(const char* state) {
    char url[1024];
    char encoded[256] = {0};
    int j = 0;
    for (int i = 0; state[i] != '\0' && i < 255; i++) {
        if (state[i] == ' ') { encoded[j++] = '%'; encoded[j++] = '2'; encoded[j++] = '0'; }
        else { encoded[j++] = state[i]; }
    }
    encoded[j] = '\0';
    sprintf(url, "curl -s \"http://all.api.radio-browser.info/json/stations/search?state=%s&limit=50&order=clickcount&reverse=true\" > data/HyperNeiva/radio_api.json", encoded);
    exec_silent(url);
    parse_radio_json();
}

void radio_fetch_language(const char* language) {
    char url[1024];
    char encoded[256] = {0};
    int j = 0;
    for (int i = 0; language[i] != '\0' && i < 255; i++) {
        if (language[i] == ' ') { encoded[j++] = '%'; encoded[j++] = '2'; encoded[j++] = '0'; }
        else { encoded[j++] = language[i]; }
    }
    encoded[j] = '\0';
    sprintf(url, "curl -s \"http://all.api.radio-browser.info/json/stations/search?language=%s&limit=50&order=clickcount&reverse=true\" > data/HyperNeiva/radio_api.json", encoded);
    exec_silent(url);
    parse_radio_json();
}
