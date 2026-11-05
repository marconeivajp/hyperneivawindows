#include "midia_win.h"
#include "baixar_win.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

MidiaItem midiaResultados[MIDIA_MAX];
int midiaTotal = 0;
char midiaStatus[256] = "";
bool midiaBuscando = false;
char midiaTermo[256] = "";

extern void exec_async_silent(const char* cmd);

static pthread_t g_th;
static bool g_ativa = false;
static char g_termo[256] = "";
static int g_modo = 0;

static int midia_exec_curl(const char* cmd) {
    char full[4200]; sprintf(full, "cmd /c \"%s\"", cmd);
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(NULL, full, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ec = 0; GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return (int)ec;
}

static void midia_urlencode(const char* in, char* out, int sz) {
    int o = 0;
    for (int i = 0; in[i] && o < sz - 4; i++) {
        unsigned char c = (unsigned char)in[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out[o++] = (char)c;
        } else {
            sprintf(out + o, "%%%02X", c); o += 3;
        }
    }
    out[o] = 0;
}

static void midia_strdup_json(const char* json, const char* chave, char* out, int sz) {
    char pat[64]; sprintf(pat, "\"%s\"", chave);
    const char* p = strstr(json, pat);
    if (!p) { out[0] = 0; return; }
    p += strlen(pat);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') { out[0] = 0; return; }
    p++;
    int o = 0;
    while (*p && *p != '"' && o < sz - 1) {
        if (*p == '\\' && *(p+1) == 'n') { out[o++] = ' '; p += 2; continue; }
        if (*p == '\\' && *(p+1) == '"') { out[o++] = '"'; p += 2; continue; }
        if (*p == '\\' && *(p+1) == '\\') { out[o++] = '\\'; p += 2; continue; }
        if (*p == '\\' && *(p+1) == '/') { out[o++] = '/'; p += 2; continue; }
        if (*p == '\\' && *(p+1) == 'u') { out[o++] = '?'; p += 6; continue; }
        out[o++] = *p++;
    }
    out[o] = 0;
}

static char* midia_ler(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(sz + 1);
    if (fread(buf, 1, sz, f) != (size_t)sz) {}
    buf[sz] = 0; fclose(f);
    return buf;
}

static void midia_add(const char* nome, const char* link, const char* info, int tipo) {
    if (midiaTotal >= MIDIA_MAX) return;
    if (!link || !link[0]) return;
    MidiaItem* m = &midiaResultados[midiaTotal];
    strncpy(m->nome, nome, 255); m->nome[255] = 0;
    strncpy(m->link, link, 1023); m->link[1023] = 0;
    strncpy(m->info, info, 127); m->info[127] = 0;
    m->tipo = tipo;
    midiaTotal++;
}

static void* midia_thread(void* arg) {
    (void)arg;
    char base[1024]; sprintf(base, "%s", baixar_dir_base());
    char tmp[1024];
    char enc[512]; midia_urlencode(g_termo, enc, sizeof(enc));
    midiaTotal = 0;

    if (g_modo == 0) {
        /* ANIME - Jikan */
        sprintf(midiaStatus, "Buscando anime...");
        char url[1024]; sprintf(url, "https://api.jikan.moe/v4/anime?q=%s&limit=20", enc);
        sprintf(tmp, "%s/midia_anime.json", base);
        char cmd[2400]; sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s --get -o \"%s\" \"%s\"", tmp, url);
        midia_exec_curl(cmd);
        char* j = midia_ler(tmp);
        if (j) {
            const char* p = strstr(j, "\"data\"");
            if (p) { p = strchr(p, '[');
                while (p && midiaTotal < MIDIA_MAX) {
                    p = strchr(p + 1, '{');
                    if (!p) break;
                    const char* fim = strchr(p, '}');
                    if (!fim) break;
                    char* item = (char*)malloc(fim - p + 2);
                    memcpy(item, p, fim - p + 1); item[fim - p + 1] = 0;
                    char title[256]; midia_strdup_json(item, "title", title, sizeof(title));
                    char url2[1024]; midia_strdup_json(item, "url", url2, sizeof(url2));
                    char eps[32]; midia_strdup_json(item, "episodes", eps, sizeof(eps));
                    char score[32]; midia_strdup_json(item, "score", score, sizeof(score));
                    char info[128]; sprintf(info, "EPS:%s Score:%s", eps, score);
                    midia_add(title[0] ? title : "Sem titulo", url2, info, 0);
                    free(item);
                }
            }
            free(j); remove(tmp);
        }
    }
    else if (g_modo == 1) {
        /* MUSICA - Jamendo */
        sprintf(midiaStatus, "Buscando musica...");
        char url[1024]; sprintf(url, "https://api.jamendo.com/v3.0/tracks/?client_id=56d30c95&format=json&limit=20&search=%s", enc);
        sprintf(tmp, "%s/midia_musica.json", base);
        char cmd[2400]; sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s --get -o \"%s\" \"%s\"", tmp, url);
        midia_exec_curl(cmd);
        char* j = midia_ler(tmp);
        if (j) {
            const char* p = strstr(j, "\"results\"");
            if (p) { p = strchr(p, '[');
                while (p && midiaTotal < MIDIA_MAX) {
                    p = strchr(p + 1, '{');
                    if (!p) break;
                    const char* fim = strchr(p, '}');
                    if (!fim) break;
                    char* item = (char*)malloc(fim - p + 2);
                    memcpy(item, p, fim - p + 1); item[fim - p + 1] = 0;
                    char name[256]; midia_strdup_json(item, "name", name, sizeof(name));
                    char artist[256]; midia_strdup_json(item, "artist_name", artist, sizeof(artist));
                    char audio[1024]; midia_strdup_json(item, "audio", audio, sizeof(audio));
                    char info[128]; sprintf(info, "Artista: %s", artist);
                    midia_add(name[0] ? name : "Sem nome", audio, info, 1);
                    free(item);
                }
            }
            free(j); remove(tmp);
        }
    }
    else if (g_modo == 2) {
        /* LIVRO - Open Library */
        sprintf(midiaStatus, "Buscando livros...");
        char url[1024]; sprintf(url, "https://openlibrary.org/search.json?q=%s&limit=20", enc);
        sprintf(tmp, "%s/midia_livro.json", base);
        char cmd[2400]; sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s --get -o \"%s\" \"%s\"", tmp, url);
        midia_exec_curl(cmd);
        char* j = midia_ler(tmp);
        if (j) {
            const char* p = strstr(j, "\"docs\"");
            if (p) { p = strchr(p, '[');
                while (p && midiaTotal < MIDIA_MAX) {
                    p = strchr(p + 1, '{');
                    if (!p) break;
                    const char* fim = strchr(p, '}');
                    if (!fim) break;
                    char* item = (char*)malloc(fim - p + 2);
                    memcpy(item, p, fim - p + 1); item[fim - p + 1] = 0;
                    char title[256]; midia_strdup_json(item, "title", title, sizeof(title));
                    char author[256]; midia_strdup_json(item, "author_name", author, sizeof(author));
                    char key[256]; midia_strdup_json(item, "key", key, sizeof(key));
                    char link[1024]; sprintf(link, "https://openlibrary.org%s", key);
                    char info[128]; sprintf(info, "Autor: %s", author);
                    midia_add(title[0] ? title : "Sem titulo", link, info, 2);
                    free(item);
                }
            }
            free(j); remove(tmp);
        }
    }
    else if (g_modo == 3) {
        /* LEGENDAS - Legendas.TV */
        sprintf(midiaStatus, "Buscando legendas...");
        char url[1024]; sprintf(url, "https://www.legendas.tv/busca/%s", enc);
        sprintf(tmp, "%s/midia_leg.json", base);
        char cmd[2400]; sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s --get -o \"%s\" \"%s\"", tmp, url);
        midia_exec_curl(cmd);
        char* h = midia_ler(tmp);
        if (h) {
            const char* p = h;
            while (p && midiaTotal < MIDIA_MAX) {
                p = strstr(p, "<a href=\"/download/");
                if (!p) break;
                const char* q1 = strchr(p + 9, '"');
                char link[512]; int l = (int)(q1 - (p + 9)); if (l > 510) l = 510;
                memcpy(link, p + 9, l); link[l] = 0;
                const char* a2 = strstr(q1, "<a");
                char nome[256]; nome[0] = 0;
                if (a2) {
                    const char* gt = strchr(a2, '>');
                    const char* lt = strchr(gt ? gt : a2, '<');
                    if (gt && lt && lt > gt + 1) { int nl = (int)(lt - (gt + 1)); if (nl > 255) nl = 255; memcpy(nome, gt + 1, nl); nome[nl] = 0; }
                }
                char full[1024]; sprintf(full, "https://www.legendas.tv%s", link);
                midia_add(nome[0] ? nome : link, full, "Legenda", 3);
                p = q1 + 1;
            }
            free(h); remove(tmp);
        }
    }
    else if (g_modo == 4) {
        /* ANIME TORRENT - Nyaa */
        sprintf(midiaStatus, "Buscando torrents de anime...");
        char url[1024]; sprintf(url, "https://nyaa.si/?q=%s&c=1_2&f=0", enc);
        sprintf(tmp, "%s/midia_nyaa.html", base);
        char cmd[2400]; sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s --get -o \"%s\" \"%s\"", tmp, url);
        midia_exec_curl(cmd);
        char* h = midia_ler(tmp);
        if (h) {
            const char* p = h;
            while (p && midiaTotal < MIDIA_MAX) {
                p = strstr(p, "magnet:?");
                if (!p) break;
                const char* q1 = strchr(p, '"');
                if (!q1) q1 = strchr(p, '<');
                if (!q1) break;
                char mag[1024]; int l = (int)(q1 - p); if (l > 1023) l = 1023;
                memcpy(mag, p, l); mag[l] = 0;
                /* nome: antes do magnet, pega o title da tag anterior */
                const char* a = strstr(p, "class=\"comment\"");
                char nome[256]; nome[0] = 0;
                if (a) {
                    const char* t0 = strstr(a, "title=\"");
                    if (t0) { const char* t1 = strchr(t0 + 7, '"'); int nl = (int)(t1 - (t0 + 7)); if (nl > 255) nl = 255; memcpy(nome, t0 + 7, nl); nome[nl] = 0; }
                }
                midia_add(nome[0] ? nome : mag, mag, "Nyaa", 4);
                p = q1 + 1;
            }
            free(h); remove(tmp);
        }
    }

    if (midiaTotal == 0) sprintf(midiaStatus, "Nenhum resultado. Tente outro termo.");
    else sprintf(midiaStatus, "%d resultados encontrados.", midiaTotal);
    midiaBuscando = false;
    g_ativa = false;
    return NULL;
}

void midia_init(void) { midiaTotal = 0; }

static void midia_iniciar(int modo, const char* termo) {
    if (midiaBuscando) return;
    strncpy(g_termo, termo, 255); g_termo[255] = 0;
    strncpy(midiaTermo, termo, 255); midiaTermo[255] = 0;
    g_modo = modo; midiaBuscando = true;
    sprintf(midiaStatus, "Iniciando...");
    g_ativa = true;
    pthread_create(&g_th, NULL, midia_thread, NULL);
}

void midia_buscar_anime(const char* t) { midia_iniciar(0, t); }
void midia_buscar_musica(const char* t) { midia_iniciar(1, t); }
void midia_buscar_livro(const char* t) { midia_iniciar(2, t); }
void midia_buscar_legenda(const char* t) { midia_iniciar(3, t); }
void midia_buscar_anime_torrent(const char* t) { midia_iniciar(4, t); }

void midia_aguardar(void) { if (g_ativa) pthread_join(g_th, NULL); }

void midia_abrir(int idx) {
    if (idx < 0 || idx >= midiaTotal) return;
    char cmd[1100];
    MidiaItem* m = &midiaResultados[idx];
    if (strstr(m->link, "http") == m->link) sprintf(cmd, "cmd.exe /c start \"\" \"%s\"", m->link);
    else sprintf(cmd, "cmd.exe /c start \"\" \"%s\"", m->link);
    exec_async_silent(cmd);
}

void midia_baixar(int idx) {
    if (idx < 0 || idx >= midiaTotal) return;
    MidiaItem* m = &midiaResultados[idx];
    char dst[1024];
    char nomeSeg[256]; int o = 0;
    for (int i = 0; m->nome[i] && o < 250; i++) {
        char c = m->nome[i];
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
        nomeSeg[o++] = c;
    }
    nomeSeg[o] = 0;
    sprintf(dst, "%s/baixado/%s", baixar_dir_base(), nomeSeg);
    char cmd[2600];
    if (strstr(m->link, "magnet") == m->link) {
        sprintf(cmd, "cmd.exe /c start \"\" \"%s\"", m->link);
    } else if (strstr(m->link, "http") == m->link) {
        sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s -L -o \"%s\" \"%s\"", dst, m->link);
    } else {
        sprintf(cmd, "cmd.exe /c start \"\" \"%s\"", m->link);
    }
    exec_async_silent(cmd);
}
