#include "gemini_ia.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <pthread.h>
#include "api_crypt.h"

extern void exec_silent(const char* cmd);

static const char* GEMINI_BASE = "https://generativelanguage.googleapis.com/v1beta";
static const char* GEMINI_REQ = "data/HyperNeiva/gemini_req.json";
static const char* GEMINI_RESP = "data/HyperNeiva/gemini_resp.json";

static const char b64tab[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static volatile int g_iaBusy = 0;
static char g_ultimoErro[1024] = "";

const char* gemini_ultimo_erro(void) { return g_ultimoErro; }

int gemini_obter_chave(char* outKey, int maxLen) {
    if (outKey && maxLen > 0) outKey[0] = 0;
    if (apiKeyTotal <= 0) apiKeys_carregar();
    for (int i = 0; i < apiKeyTotal; i++) {
        if (strcasecmp(apiKeys[i].nome, "Gemini") == 0 && apiKeys[i].chave[0]) {
            snprintf(outKey, maxLen, "%s", apiKeys[i].chave);
            return 1;
        }
    }
    return 0;
}

static void json_escape(const char* in, char* out, int maxOut) {
    int oi = 0;
    for (int i = 0; in[i] && oi < maxOut - 6; i++) {
        char c = in[i];
        if (c == '"') { out[oi++] = '\\'; out[oi++] = '"'; }
        else if (c == '\\') { out[oi++] = '\\'; out[oi++] = '\\'; }
        else if (c == '\n') { out[oi++] = '\\'; out[oi++] = 'n'; }
        else if (c == '\r') { out[oi++] = '\\'; out[oi++] = 'r'; }
        else if (c == '\t') { out[oi++] = '\\'; out[oi++] = 't'; }
        else if ((unsigned char)c < 32) { }
        else out[oi++] = c;
    }
    out[oi] = 0;
}

static long file_size(const char* p) {
    struct stat st;
    if (stat(p, &st) == 0) return st.st_size;
    return -1;
}

static char* read_file_malloc(const char* path, long* outSz) {
    long sz = file_size(path);
    if (sz <= 0) return NULL;
    char* buf = (char*)malloc(sz + 1);
    if (!buf) return NULL;
    FILE* f = fopen(path, "rb");
    if (!f) { free(buf); return NULL; }
    long n = (long)fread(buf, 1, sz, f);
    fclose(f);
    buf[n] = 0;
    *outSz = n;
    return buf;
}

void gemini_base64_encode_file(const char* path, char* out, int maxOut) {
    if (!out || maxOut <= 0) return;
    out[0] = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char* raw = (unsigned char*)malloc(sz > 0 ? (size_t)sz : 1);
    if (!raw) { fclose(f); return; }
    if (sz > 0) fread(raw, 1, (size_t)sz, f);
    fclose(f);
    int oi = 0;
    for (long i = 0; i < sz && oi < maxOut - 4; i += 3) {
        unsigned long v = ((unsigned long)raw[i]) << 16;
        if (i + 1 < sz) v |= ((unsigned long)raw[i+1]) << 8;
        if (i + 2 < sz) v |= (unsigned long)raw[i+2];
        out[oi++] = b64tab[(v >> 18) & 63];
        out[oi++] = b64tab[(v >> 12) & 63];
        out[oi++] = (i + 1 < sz) ? b64tab[(v >> 6) & 63] : '=';
        out[oi++] = (i + 2 < sz) ? b64tab[v & 63] : '=';
    }
    out[oi] = 0;
    free(raw);
}

static int b64val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

int gemini_base64_decode(const char* in, unsigned char* out, int maxOut) {
    if (!in || !out) return 0;
    int oi = 0, qi = 0;
    int quad[4];
    int n = (int)strlen(in);
    for (int i = 0; i < n && oi < maxOut; i++) {
        char c = in[i];
        if (c == '\n' || c == '\r' || c == ' ') continue;
        if (c == '=') { quad[qi++] = 0; }
        else {
            int v = b64val((unsigned char)c);
            if (v < 0) continue;
            quad[qi++] = v;
        }
        if (qi == 4) {
            out[oi++] = (unsigned char)((quad[0] << 2) | (quad[1] >> 4));
            if (oi < maxOut) out[oi++] = (unsigned char)(((quad[1] & 15) << 4) | (quad[2] >> 2));
            if (oi < maxOut) out[oi++] = (unsigned char)(((quad[2] & 3) << 6) | quad[3]);
            qi = 0;
        }
    }
    return oi;
}

static int json_last_string(const char* json, const char* key, char* out, int maxOut) {
    if (!json || !out || maxOut <= 0) return 0;
    out[0] = 0;
    char pat[96];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char* found = NULL;
    const char* p = json;
    while ((p = strstr(p, pat)) != NULL) {
        const char* q = p + strlen(pat);
        while (*q == ' ' || *q == '\t') q++;
        if (*q == '"') found = q;
        p = q;
    }
    if (!found) return 0;
    found++;
    int oi = 0;
    for (int i = 0; found[i] && oi < maxOut - 1; i++) {
        char c = found[i];
        if (c == '\\' && found[i+1]) {
            char n = found[i+1];
            if (n == 'n') { out[oi++] = '\n'; i++; }
            else if (n == 'r') { out[oi++] = '\r'; i++; }
            else if (n == 't') { out[oi++] = '\t'; i++; }
            else if (n == '"') { out[oi++] = '"'; i++; }
            else if (n == '\\') { out[oi++] = '\\'; i++; }
            else { out[oi++] = n; i++; }
            continue;
        }
        if (c == '"') break;
        out[oi++] = c;
    }
    out[oi] = 0;
    return oi > 0;
}

static int run_curl(const char* model, const char* apiKey) {
    remove(GEMINI_RESP);
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
        "curl -s --connect-timeout 20 -m 180 -X POST \"%s/models/%s:generateContent\" -H \"x-goog-api-key: %s\" -H \"Content-Type: application/json\" -d @\"%s\" -o \"%s\"",
        GEMINI_BASE, model, apiKey, GEMINI_REQ, GEMINI_RESP);
    exec_silent(cmd);
    return file_size(GEMINI_RESP) > 0;
}

static void mime_from_ext(const char* path, char* out, int maxOut) {
    const char* ext = strrchr(path, '.');
    if (ext) {
        if (strcasecmp(ext, ".wav") == 0) { snprintf(out, maxOut, "audio/wav"); return; }
        if (strcasecmp(ext, ".mp3") == 0) { snprintf(out, maxOut, "audio/mpeg"); return; }
        if (strcasecmp(ext, ".ogg") == 0) { snprintf(out, maxOut, "audio/ogg"); return; }
        if (strcasecmp(ext, ".m4a") == 0) { snprintf(out, maxOut, "audio/mp4"); return; }
        if (strcasecmp(ext, ".aac") == 0) { snprintf(out, maxOut, "audio/aac"); return; }
        if (strcasecmp(ext, ".flac") == 0) { snprintf(out, maxOut, "audio/flac"); return; }
        if (strcasecmp(ext, ".mp4") == 0) { snprintf(out, maxOut, "video/mp4"); return; }
        if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) { snprintf(out, maxOut, "image/jpeg"); return; }
        if (strcasecmp(ext, ".png") == 0) { snprintf(out, maxOut, "image/png"); return; }
        if (strcasecmp(ext, ".bmp") == 0) { snprintf(out, maxOut, "image/bmp"); return; }
        if (strcasecmp(ext, ".webp") == 0) { snprintf(out, maxOut, "image/webp"); return; }
    }
    snprintf(out, maxOut, "application/octet-stream");
}

static void extrair_erro(const char* resp, char* errOut, int maxErr) {
    char tmp[1024];
    if (json_last_string(resp, "message", tmp, sizeof(tmp))) {
        snprintf(errOut, maxErr, "Erro da API: %s", tmp);
        return;
    }
    snprintf(errOut, maxErr, "Erro da API. Verifique a chave (Gemini) nas Chaves API e o modelo.");
}

bool gemini_gerar_texto(const char* apiKey, const char* model, const char* prompt, const char* mediaPaths[], int mediaCount, char* outText, int outSz) {
    if (outText && outSz > 0) outText[0] = 0;
    if (mediaCount > 4) mediaCount = 4;
    FILE* f = fopen(GEMINI_REQ, "wb");
    if (!f) return false;
    fputs("{\"contents\":[{\"parts\":[", f);
    for (int m = 0; m < mediaCount; m++) {
        if (!mediaPaths[m] || !mediaPaths[m][0]) continue;
        char mime[64];
        mime_from_ext(mediaPaths[m], mime, sizeof(mime));
        char* b64 = (char*)malloc(1048576);
        if (!b64) { fclose(f); snprintf(outText, outSz, "Sem memoria."); return false; }
        b64[0] = 0;
        gemini_base64_encode_file(mediaPaths[m], b64, 1048576);
        if (b64[0]) {
            fprintf(f, "{\"inline_data\":{\"mime_type\":\"%s\",\"data\":\"%s\"}},", mime, b64);
        }
        free(b64);
    }
    char esc[8192];
    json_escape(prompt ? prompt : "", esc, sizeof(esc));
    fprintf(f, "{\"text\":\"%s\"}", esc);
    fputs("]}]}", f);
    fclose(f);

    if (!run_curl(model, apiKey)) {
        snprintf(outText, outSz, "Falha na requisicao (curl nao encontrado ou sem conexao).");
        return false;
    }
    long rsz = 0;
    char* resp = read_file_malloc(GEMINI_RESP, &rsz);
    if (!resp) {
        snprintf(outText, outSz, "Sem resposta da API.");
        return false;
    }
    if (!json_last_string(resp, "text", outText, outSz)) {
        char err[1024];
        extrair_erro(resp, err, sizeof(err));
        snprintf(g_ultimoErro, sizeof(g_ultimoErro), "%s", err);
        snprintf(outText, outSz, "%s", err);
        free(resp);
        return false;
    }
    free(resp);
    return true;
}

bool gemini_gerar_imagem(const char* apiKey, const char* model, const char* prompt, const char* aspectRatio, const char* outPath) {
    FILE* f = fopen(GEMINI_REQ, "wb");
    if (!f) return false;
    char esc[8192];
    json_escape(prompt ? prompt : "", esc, sizeof(esc));
    fprintf(f, "{\"contents\":[{\"parts\":[{\"text\":\"%s\"}]}],\"generationConfig\":{\"responseModalities\":[\"IMAGE\"],\"imageConfig\":{\"aspectRatio\":\"%s\"}}}",
        esc, aspectRatio ? aspectRatio : "1:1");
    fclose(f);

    if (!run_curl(model, apiKey)) {
        snprintf(g_ultimoErro, sizeof(g_ultimoErro), "Falha na requisicao de geracao de imagem.");
        return false;
    }
    long rsz = 0;
    char* resp = read_file_malloc(GEMINI_RESP, &rsz);
    if (!resp) return false;
    long bsz = 4194304; // ate ~3MB base64 de imagem
    char* b64 = (char*)malloc((size_t)bsz);
    if (!b64) { free(resp); return false; }
    if (!json_last_string(resp, "data", b64, (int)bsz)) {
        char err[1024];
        extrair_erro(resp, err, sizeof(err));
        snprintf(g_ultimoErro, sizeof(g_ultimoErro), "%s", err);
        free(resp);
        free(b64);
        return false;
    }
    free(resp);
    unsigned char* img = (unsigned char*)malloc((size_t)bsz);
    if (!img) { free(b64); return false; }
    int n = gemini_base64_decode(b64, img, (int)bsz);
    free(b64);
    if (n <= 0) { free(img); return false; }
    FILE* o = fopen(outPath, "wb");
    if (!o) { free(img); return false; }
    fwrite(img, 1, (size_t)n, o);
    fclose(o);
    free(img);
    return file_size(outPath) > 0;
}

bool gemini_audio_para_texto(const char* apiKey, const char* audioPath, const char* instrucao, char* outText, int outSz) {
    if (outText && outSz > 0) outText[0] = 0;
    long asz = file_size(audioPath);
    if (asz <= 0) {
        snprintf(outText, outSz, "Arquivo de audio nao encontrado.");
        return false;
    }
    long need = ((asz + 2) / 3) * 4 + 4;
    char* b64 = (char*)malloc((size_t)need);
    if (!b64) {
        snprintf(outText, outSz, "Sem memoria para transcricao.");
        return false;
    }
    FILE* r = fopen(audioPath, "rb");
    if (!r) { free(b64); snprintf(outText, outSz, "Nao foi possivel abrir o audio."); return false; }
    fseek(r, 0, SEEK_END); long rsz2 = ftell(r); fseek(r, 0, SEEK_SET);
    unsigned char* raw = (unsigned char*)malloc((size_t)(rsz2 > 0 ? rsz2 : 1));
    if (!raw) { free(b64); fclose(r); snprintf(outText, outSz, "Sem memoria."); return false; }
    if (rsz2 > 0) fread(raw, 1, (size_t)rsz2, r);
    fclose(r);
    int oi = 0;
    for (long i = 0; i < rsz2; i += 3) {
        unsigned long v = ((unsigned long)raw[i]) << 16;
        if (i + 1 < rsz2) v |= ((unsigned long)raw[i+1]) << 8;
        if (i + 2 < rsz2) v |= (unsigned long)raw[i+2];
        b64[oi++] = b64tab[(v >> 18) & 63];
        b64[oi++] = b64tab[(v >> 12) & 63];
        b64[oi++] = (i + 1 < rsz2) ? b64tab[(v >> 6) & 63] : '=';
        b64[oi++] = (i + 2 < rsz2) ? b64tab[v & 63] : '=';
    }
    b64[oi] = 0;
    free(raw);

    char mime[64];
    mime_from_ext(audioPath, mime, sizeof(mime));
    char esc[4096];
    json_escape(instrucao && instrucao[0] ? instrucao : "Transcreva o audio completo.", esc, sizeof(esc));
    FILE* f = fopen(GEMINI_REQ, "wb");
    if (!f) { free(b64); snprintf(outText, outSz, "Nao foi possivel criar requisicao."); return false; }
    fprintf(f, "{\"contents\":[{\"parts\":[{\"inline_data\":{\"mime_type\":\"%s\",\"data\":\"%s\"}},{\"text\":\"%s\"}]}]}",
        mime, b64, esc);
    fclose(f);
    free(b64);

    if (!run_curl("gemini-3.5-flash", apiKey)) {
        snprintf(outText, outSz, "Falha na requisicao de transcricao.");
        return false;
    }
    long rsz3 = 0;
    char* resp = read_file_malloc(GEMINI_RESP, &rsz3);
    if (!resp) {
        snprintf(outText, outSz, "Sem resposta da API.");
        return false;
    }
    if (!json_last_string(resp, "text", outText, outSz)) {
        char err[1024];
        extrair_erro(resp, err, sizeof(err));
        snprintf(g_ultimoErro, sizeof(g_ultimoErro), "%s", err);
        snprintf(outText, outSz, "%s", err);
        free(resp);
        return false;
    }
    free(resp);
    return true;
}

static void write_wav_header(FILE* w, long dataSz, int sampleRate) {
    unsigned char h[44];
    memset(h, 0, sizeof(h));
    h[0]='R'; h[1]='I'; h[2]='F'; h[3]='F';
    unsigned int riff = (unsigned int)(36 + dataSz);
    h[4]=riff&255; h[5]=(riff>>8)&255; h[6]=(riff>>16)&255; h[7]=(riff>>24)&255;
    h[8]='W'; h[9]='A'; h[10]='V'; h[11]='E';
    h[12]='f'; h[13]='m'; h[14]='t'; h[15]=' ';
    h[16]=16; h[17]=0; h[18]=0; h[19]=0;
    h[20]=1; h[21]=0;          // PCM
    h[22]=1; h[23]=0;          // mono
    h[24]=sampleRate&255; h[25]=(sampleRate>>8)&255; h[26]=(sampleRate>>16)&255; h[27]=(sampleRate>>24)&255;
    unsigned int byteRate = (unsigned int)(sampleRate * 2);
    h[28]=byteRate&255; h[29]=(byteRate>>8)&255; h[30]=(byteRate>>16)&255; h[31]=(byteRate>>24)&255;
    h[32]=2; h[33]=0;          // block align
    h[34]=16; h[35]=0;         // bits per sample
    h[36]='d'; h[37]='a'; h[38]='t'; h[39]='a';
    h[40]=dataSz&255; h[41]=(dataSz>>8)&255; h[42]=(dataSz>>16)&255; h[43]=(dataSz>>24)&255;
    fwrite(h, 1, sizeof(h), w);
}

bool gemini_texto_para_audio(const char* apiKey, const char* text, const char* voice, const char* outPath) {
    FILE* f = fopen(GEMINI_REQ, "wb");
    if (!f) return false;
    char esc[8192];
    json_escape(text ? text : "", esc, sizeof(esc));
    fprintf(f, "{\"contents\":[{\"parts\":[{\"text\":\"%s\"}]}],\"generationConfig\":{\"responseModalities\":[\"AUDIO\"],\"speechConfig\":{\"voiceConfig\":{\"prebuiltVoiceConfig\":{\"voiceName\":\"%s\"}}}}}",
        esc, voice ? voice : "Kore");
    fclose(f);

    if (!run_curl("gemini-3.1-flash-tts-preview", apiKey)) {
        snprintf(g_ultimoErro, sizeof(g_ultimoErro), "Falha na requisicao de TTS.");
        return false;
    }
    long rsz = 0;
    char* resp = read_file_malloc(GEMINI_RESP, &rsz);
    if (!resp) return false;
    char b64[1048576];
    if (!json_last_string(resp, "data", b64, sizeof(b64))) {
        char err[1024];
        extrair_erro(resp, err, sizeof(err));
        snprintf(g_ultimoErro, sizeof(g_ultimoErro), "%s", err);
        free(resp);
        return false;
    }
    free(resp);
    unsigned char* pcm = (unsigned char*)malloc(1048576);
    if (!pcm) return false;
    int n = gemini_base64_decode(b64, pcm, 1048576);
    if (n <= 0) { free(pcm); return false; }
    FILE* w = fopen(outPath, "wb");
    if (!w) { free(pcm); return false; }
    write_wav_header(w, n, 24000);
    fwrite(pcm, 1, (size_t)n, w);
    fclose(w);
    free(pcm);
    return file_size(outPath) > 44;
}

// ===================== THREAD ASYNC =====================

#define IA_MAX_MIDIA 4

typedef struct {
    char prompt[4096];
    char modelo[128];
    char medias[IA_MAX_MIDIA][512];
    int mediaCount;
    char outPath[512];
    char aspect[32];
    char voz[64];
    char audioPath[512];
    GeminiResposta* resp;
    int tipo; // 0=texto, 1=imagem, 2=audio->texto, 3=texto->audio
} IaTrabalho;

static void* ia_thread(void* arg) {
    IaTrabalho* t = (IaTrabalho*)arg;
    char key[512];
    if (!gemini_obter_chave(key, sizeof(key))) {
        t->resp->ok = false;
        t->resp->temMedia = false;
        snprintf(t->resp->texto, sizeof(t->resp->texto),
            "Chave da API Gemini nao configurada. Vá em Configuracoes > Chaves API e adicione a chave Gemini.");
    } else {
        if (t->tipo == 1) {
            t->resp->ok = gemini_gerar_imagem(key, t->modelo, t->prompt, t->aspect, t->outPath);
            t->resp->temMedia = false;
            if (t->resp->ok) {
                snprintf(t->resp->mediaPath, sizeof(t->resp->mediaPath), "%s", t->outPath);
                t->resp->temMedia = true;
                snprintf(t->resp->texto, sizeof(t->resp->texto), "Imagem gerada com sucesso: %s", t->outPath);
            } else {
                snprintf(t->resp->texto, sizeof(t->resp->texto), "%s", gemini_ultimo_erro());
            }
        } else if (t->tipo == 2) {
            t->resp->temMedia = false;
            t->resp->ok = gemini_audio_para_texto(key, t->audioPath, "Transcreva o audio completo. Se for musica, identifique a faixa.", t->resp->texto, sizeof(t->resp->texto));
        } else if (t->tipo == 3) {
            t->resp->ok = gemini_texto_para_audio(key, t->prompt, t->voz, t->outPath);
            t->resp->temMedia = false;
            if (t->resp->ok) {
                snprintf(t->resp->mediaPath, sizeof(t->resp->mediaPath), "%s", t->outPath);
                t->resp->temMedia = true;
                snprintf(t->resp->texto, sizeof(t->resp->texto), "Audio gerado com sucesso: %s", t->outPath);
            } else {
                snprintf(t->resp->texto, sizeof(t->resp->texto), "%s", gemini_ultimo_erro());
            }
        } else {
            const char* medias[IA_MAX_MIDIA] = { NULL, NULL, NULL, NULL };
            for (int i = 0; i < t->mediaCount; i++) medias[i] = t->medias[i];
            t->resp->temMedia = false;
            t->resp->ok = gemini_gerar_texto(key, t->modelo, t->prompt, medias, t->mediaCount, t->resp->texto, sizeof(t->resp->texto));
            if (t->resp->ok && t->outPath[0]) {
                FILE* fo = fopen(t->outPath, "wb");
                if (fo) { fputs(t->resp->texto, fo); fclose(fo); }
            }
        }
    }
    g_iaBusy = 0;
    free(t);
    return NULL;
}

static bool ia_iniciar(IaTrabalho* t) {
    if (g_iaBusy) { free(t); return false; }
    g_iaBusy = 1;
    pthread_t th;
    if (pthread_create(&th, NULL, ia_thread, t) != 0) {
        g_iaBusy = 0;
        free(t);
        return false;
    }
    pthread_detach(th);
    return true;
}

bool gemini_busy(void) { return g_iaBusy != 0; }

bool gemini_gerar_texto_async(const char* prompt, const char* modelo,
                              const char* mediaPaths[], int mediaCount,
                              const char* outPath, GeminiResposta* resp) {
    if (!resp) return false;
    if (mediaCount > IA_MAX_MIDIA) mediaCount = IA_MAX_MIDIA;
    resp->ok = false; resp->temMedia = false; resp->texto[0] = 0; resp->mediaPath[0] = 0;
    IaTrabalho* t = (IaTrabalho*)calloc(1, sizeof(IaTrabalho));
    if (!t) return false;
    t->tipo = 0;
    snprintf(t->prompt, sizeof(t->prompt), "%s", prompt ? prompt : "");
    snprintf(t->modelo, sizeof(t->modelo), "%s", modelo ? modelo : "gemini-3.5-flash");
    t->mediaCount = mediaCount;
    for (int i = 0; i < mediaCount; i++) {
        if (mediaPaths[i]) snprintf(t->medias[i], sizeof(t->medias[i]), "%s", mediaPaths[i]);
    }
    if (outPath) snprintf(t->outPath, sizeof(t->outPath), "%s", outPath);
    t->resp = resp;
    return ia_iniciar(t);
}

bool gemini_gerar_imagem_async(const char* prompt, const char* modelo,
                               const char* aspect, const char* outPng, GeminiResposta* resp) {
    if (!resp || !outPng) return false;
    resp->ok = false; resp->temMedia = false; resp->texto[0] = 0; resp->mediaPath[0] = 0;
    IaTrabalho* t = (IaTrabalho*)calloc(1, sizeof(IaTrabalho));
    if (!t) return false;
    t->tipo = 1;
    snprintf(t->prompt, sizeof(t->prompt), "%s", prompt ? prompt : "");
    snprintf(t->modelo, sizeof(t->modelo), "%s", modelo ? modelo : "gemini-3.1-flash-image");
    snprintf(t->aspect, sizeof(t->aspect), "%s", aspect ? aspect : "1:1");
    snprintf(t->outPath, sizeof(t->outPath), "%s", outPng);
    t->resp = resp;
    return ia_iniciar(t);
}

bool gemini_audio_para_texto_async(const char* audioPath, const char* modelo, GeminiResposta* resp) {
    if (!resp) return false;
    resp->ok = false; resp->temMedia = false; resp->texto[0] = 0; resp->mediaPath[0] = 0;
    IaTrabalho* t = (IaTrabalho*)calloc(1, sizeof(IaTrabalho));
    if (!t) return false;
    t->tipo = 2;
    snprintf(t->audioPath, sizeof(t->audioPath), "%s", audioPath ? audioPath : "");
    snprintf(t->modelo, sizeof(t->modelo), "%s", modelo ? modelo : "gemini-3.5-flash");
    t->resp = resp;
    return ia_iniciar(t);
}

bool gemini_texto_para_audio_async(const char* texto, const char* voice, const char* outMp3, GeminiResposta* resp) {
    if (!resp || !outMp3) return false;
    resp->ok = false; resp->temMedia = false; resp->texto[0] = 0; resp->mediaPath[0] = 0;
    IaTrabalho* t = (IaTrabalho*)calloc(1, sizeof(IaTrabalho));
    if (!t) return false;
    t->tipo = 3;
    snprintf(t->prompt, sizeof(t->prompt), "%s", texto ? texto : "");
    snprintf(t->voz, sizeof(t->voz), "%s", voice ? voice : "Kore");
    snprintf(t->outPath, sizeof(t->outPath), "%s", outMp3);
    t->resp = resp;
    return ia_iniciar(t);
}
