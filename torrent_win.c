#include "torrent_win.h"
#include "baixar_win.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

TorrentItem torrentResultados[TORRENT_MAX];
int torrentTotal = 0;
char torrentStatus[256] = "";
bool torrentBuscando = false;
char torrentTermo[256] = "";

extern void exec_async_silent(const char* cmd);

static pthread_t g_th;
static bool g_ativa = false;
static char g_termo[256] = "";
static int g_servico = 2; /* 0=YTS 1=1337x 2=Ambos 3=Jackett 4=Prowlarr 5=Radarr 6=Sonarr 7=Bazaar */

static int torrent_exec_curl(const char* cmd) {
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

static void torrent_urlencode(const char* in, char* out, int sz) {
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

static void torrent_adicionar(const char* nome, const char* magnet, const char* tam, const char* seeds, int yts);

static void torrent_strdup_json(const char* json, const char* chave, char* out, int sz) {
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

static int torrent_int_json(const char* json, const char* chave) {
    char pat[64]; sprintf(pat, "\"%s\"", chave);
    const char* p = strstr(json, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == ':') p++;
    return atoi(p);
}

static char* torrent_ler_arquivo(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(sz + 1);
    if (fread(buf, 1, sz, f) != (size_t)sz) {}
    buf[sz] = 0; fclose(f);
    return buf;
}

/* le a api key de um servico no arquivo data/HyperNeiva/arr_keys.txt (formato NOME=key) */
static void arr_ler_key(const char* nome, char* out, int sz) {
    out[0] = 0;
    char path[1024]; sprintf(path, "%s/arr_keys.txt", baixar_dir_base());
    char* buf = torrent_ler_arquivo(path);
    if (!buf) return;
    char tag[64]; sprintf(tag, "%s=", nome);
    const char* p = strstr(buf, tag);
    if (p) {
        p += strlen(tag);
        int o = 0;
        while (*p && *p != '\n' && *p != '\r' && o < sz - 1) out[o++] = *p++;
        out[o] = 0;
    }
    free(buf);
}

static void arr_adicionar_de_json(const char* json, int svc) {
    /* Jackett e Prowlarr retornam "Results":[ {Title, Link/MagnetUrl, Seeders, Size} ] */
    const char* p = strstr(json, "\"Results\"");
    if (!p) p = strstr(json, "\"results\"");
    if (!p) return;
    p = strchr(p, '[');
    while (p && torrentTotal < TORRENT_MAX) {
        p = strchr(p + 1, '{');
        if (!p) break;
        const char* fim = strchr(p, '}');
        if (!fim) break;
        char* item = (char*)malloc(fim - p + 2);
        memcpy(item, p, fim - p + 1); item[fim - p + 1] = 0;
        char title[256]; torrent_strdup_json(item, "Title", title, sizeof(title));
        char link[1024]; torrent_strdup_json(item, "MagnetUrl", link, sizeof(link));
        if (!link[0]) torrent_strdup_json(item, "Link", link, sizeof(link));
        char seeds[32]; torrent_strdup_json(item, "Seeders", seeds, sizeof(seeds));
        if (!seeds[0]) sprintf(seeds, "0");
        char sz[64]; torrent_strdup_json(item, "Size", sz, sizeof(sz));
        if (link[0]) torrent_adicionar(title[0] ? title : link, link, sz, seeds, 0);
        free(item);
    }
}

static void torrent_buscar_servico(int svc) {
    const char* nomes[8] = { "", "", "", "Jackett", "Prowlarr", "Radarr", "Sonarr", "Bazaar" };
    const int portas[8] = { 0, 0, 0, 9117, 9696, 7878, 8989, 6969 };
    const char* baseUrl[8] = { "", "", "",
        "http://localhost:9117/api/v2.0/indexers/all/results?apikey=%s&Query=%s",
        "http://localhost:9696/api/v1/search?query=%s&apikey=%s",
        "http://localhost:7878/api/v3/movie?apikey=%s",
        "http://localhost:8989/api/v3/series?apikey=%s",
        "http://localhost:6969/api/v1/search?query=%s&apikey=%s" };
    if (svc < 3 || svc > 7) return;
    sprintf(torrentStatus, "Buscando em %s...", nomes[svc]);
    char key[256]; arr_ler_key(nomes[svc], key, sizeof(key));
    char enc[512]; torrent_urlencode(g_termo, enc, sizeof(enc));
    char url[1200];
    if (svc == 3) sprintf(url, baseUrl[3], key, enc);
    else if (svc == 4) sprintf(url, baseUrl[4], enc, key);
    else if (svc == 5) sprintf(url, baseUrl[5], key);
    else if (svc == 6) sprintf(url, baseUrl[6], key);
    else if (svc == 7) sprintf(url, baseUrl[7], enc, key);
    char tmp[1024]; sprintf(tmp, "%s/torrent_arr.json", baixar_dir_base());
    char cmd[2600]; sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s --max-time 10 --get -o \"%s\" \"%s\"", tmp, url);
    int ec = torrent_exec_curl(cmd);
    char* j = torrent_ler_arquivo(tmp);
    if (!j || ec != 0) {
        sprintf(torrentStatus, "%s nao encontrado em localhost:%d (servico offline ou apikey ausente).", nomes[svc], portas[svc]);
        if (j) free(j);
        remove(tmp);
        return;
    }
    arr_adicionar_de_json(j, svc);
    free(j); remove(tmp);
    if (torrentTotal == 0) sprintf(torrentStatus, "%s sem resultados ou apikey invalida.", nomes[svc]);
}

static void torrent_adicionar(const char* nome, const char* magnet, const char* tam, const char* seeds, int yts) {
    if (torrentTotal >= TORRENT_MAX) return;
    if (!magnet || !magnet[0]) return;
    TorrentItem* t = &torrentResultados[torrentTotal];
    strncpy(t->nome, nome, 255); t->nome[255] = 0;
    strncpy(t->magnet, magnet, 1023); t->magnet[1023] = 0;
    strncpy(t->tamanho, tam, 63); t->tamanho[63] = 0;
    strncpy(t->seeds, seeds, 31); t->seeds[31] = 0;
    t->seedsNum = atoi(seeds);
    t->ehYTS = yts;
    torrentTotal++;
}

static void torrent_ordenar_por_seeds(void) {
    for (int i = 0; i < torrentTotal - 1; i++) {
        for (int j = i + 1; j < torrentTotal; j++) {
            if (torrentResultados[j].seedsNum > torrentResultados[i].seedsNum) {
                TorrentItem tmp = torrentResultados[i];
                torrentResultados[i] = torrentResultados[j];
                torrentResultados[j] = tmp;
            }
        }
    }
}

static void* torrent_thread(void* arg) {
    (void)arg;
    char base[1024]; sprintf(base, "%s", baixar_dir_base());
    char tmp[1024];
    char enc[512]; torrent_urlencode(g_termo, enc, sizeof(enc));

    torrentTotal = 0;
    sprintf(torrentStatus, "Buscando em YTS...");

    /* YTS - filmes */
    char urlY[1024]; sprintf(urlY, "https://yts.mx/api/v2/list_movies.json?query_term=%s&limit=20", enc);
    sprintf(tmp, "%s/torrent_yts.json", base);
    char cmd[2400]; sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s --max-time 25 --get -o \"%s\" \"%s\"", tmp, urlY);
    torrent_exec_curl(cmd);
    char* jy = torrent_ler_arquivo(tmp);
    if (jy) {
        const char* p = strstr(jy, "\"movies\"");
        if (p) {
            p = strchr(p, '[');
            while (p && torrentTotal < TORRENT_MAX) {
                p = strchr(p + 1, '{');
                if (!p) break;
                const char* fim = strchr(p, '}');
                if (!fim) break;
                char* item = (char*)malloc(fim - p + 2);
                memcpy(item, p, fim - p + 1); item[fim - p + 1] = 0;
                char title[256]; torrent_strdup_json(item, "title", title, sizeof(title));
                const char* pt = strstr(item, "\"torrents\"");
                if (pt) {
                    pt = strchr(pt, '[');
                    const char* tfim = strchr(pt, ']');
                    const char* t0 = strchr(pt, '{');
                    if (t0 && tfim && t0 < tfim) {
                        char* tor = (char*)malloc(tfim - t0 + 2);
                        memcpy(tor, t0, tfim - t0 + 1); tor[tfim - t0 + 1] = 0;
                        char q[64]; torrent_strdup_json(tor, "quality", q, sizeof(q));
                        char sz[64]; torrent_strdup_json(tor, "size", sz, sizeof(sz));
                        char sd[32]; torrent_strdup_json(tor, "seeds", sd, sizeof(sd));
                        char mg[1024]; torrent_strdup_json(tor, "url", mg, sizeof(mg));
                        char nome[320]; sprintf(nome, "%s [%s] (YTS)", title, q);
                        /* YTS retorna link do torrent, montar magnet via hash se possivel */
                        char mag[1024];
                        if (strstr(mg, "magnet") == mg) sprintf(mag, "%s", mg);
                        else {
                            char hash[64]; torrent_strdup_json(tor, "hash", hash, sizeof(hash));
                            if (hash[0]) sprintf(mag, "magnet:?xt=urn:btih:%s&dn=%s", hash, enc);
                            else sprintf(mag, "%s", mg);
                        }
                        torrent_adicionar(nome, mag, sz, sd, 1);
                        free(tor);
                    }
                }
                free(item);
            }
        }
        free(jy); remove(tmp);
    }

    sprintf(torrentStatus, "Buscando em 1337x...");

    /* 1337x - filmes e series */
    char urlX[1024]; sprintf(urlX, "https://1337x.to/search/%s/1/", enc);
    sprintf(tmp, "%s/torrent_1337.html", base);
    sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s --max-time 25 --get -o \"%s\" \"%s\"", tmp, urlX);
    torrent_exec_curl(cmd);
    char* hx = torrent_ler_arquivo(tmp);
    if (hx) {
        const char* p = hx;
        while (p && torrentTotal < TORRENT_MAX) {
            p = strstr(p, "class=\"coll-1 name\"");
            if (!p) break;
            const char* a1 = strstr(p, "<a href=\"");
            if (!a1) break;
            const char* q1 = strchr(a1 + 9, '"');
            char link[512];
            int l = (int)(q1 - (a1 + 9)); if (l > 510) l = 510;
            memcpy(link, a1 + 9, l); link[l] = 0;
            /* nome esta dentro do primeiro <a>, entre '>' e '<' */
            const char* gt = strchr(q1, '>');
            const char* lt = strchr(gt ? gt : q1, '<');
            char nome[256]; nome[0] = 0;
            if (gt && lt && lt > gt + 1) {
                int nl = (int)(lt - (gt + 1)); if (nl > 255) nl = 255;
                memcpy(nome, gt + 1, nl); nome[nl] = 0;
            }
            /* seeds e size depois */
            const char* seedsT = strstr(p, "class=\"coll-2 seeds\"");
            char sd[32]; sd[0] = 0;
            if (seedsT) {
                const char* st = strchr(seedsT, '>');
                const char* sl = strchr(st ? st : seedsT, '<');
                if (st && sl && sl > st + 1) {
                    int ns = (int)(sl - (st + 1)); if (ns > 31) ns = 31;
                    memcpy(sd, st + 1, ns); sd[ns] = 0;
                }
            }
            /* montar magnet a partir da pagina do torrent */
            char mag[1024];
            if (strstr(link, "http") == link) sprintf(mag, "%s", link);
            else sprintf(mag, "https://1337x.to%s", link);
            char nomeF[320]; sprintf(nomeF, "%s", nome[0] ? nome : link);
            torrent_adicionar(nomeF, mag, "", sd, 0);
            p = q1 + 1;
        }
        free(hx); remove(tmp);
    }

    if (g_servico >= 3 && g_servico <= 7) {
        torrent_buscar_servico(g_servico);
    }

    torrent_ordenar_por_seeds();
    if (torrentTotal == 0) sprintf(torrentStatus, "Nenhum resultado. Tente outro termo.");
    else sprintf(torrentStatus, "%d resultados encontrados (por seeds).", torrentTotal);
    torrentBuscando = false;
    g_ativa = false;
    return NULL;
}

void torrent_init(void) {
    torrentTotal = 0;
}

void torrent_buscar(const char* termo) {
    torrent_buscar_provedor(2, termo);
}

void torrent_buscar_provedor(int provedor, const char* termo) {
    if (torrentBuscando) return;
    g_servico = provedor;
    strncpy(g_termo, termo, 255); g_termo[255] = 0;
    strncpy(torrentTermo, termo, 255); torrentTermo[255] = 0;
    torrentBuscando = true;
    sprintf(torrentStatus, "Iniciando busca...");
    g_ativa = true;
    pthread_create(&g_th, NULL, torrent_thread, NULL);
}

void torrent_aguardar(void) {
    if (g_ativa) pthread_join(g_th, NULL);
}

void torrent_abrir_magnet(int idx) {
    if (idx < 0 || idx >= torrentTotal) return;
    char cmd[1100]; sprintf(cmd, "cmd.exe /c start \"\" \"%s\"", torrentResultados[idx].magnet);
    exec_async_silent(cmd);
}

void torrent_baixar_torrent(int idx) {
    if (idx < 0 || idx >= torrentTotal) return;
    TorrentItem* t = &torrentResultados[idx];
    char dst[1024]; sprintf(dst, "%s/baixado/filmes/%s.torrent", baixar_dir_base(), t->nome);
    char cmd[2600];
    if (strstr(t->magnet, "http") == t->magnet) {
        sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s -L -o \"%s\" \"%s\"", dst, t->magnet);
    } else {
        /* magnet: abrir no cliente padrao */
        sprintf(cmd, "cmd.exe /c start \"\" \"%s\"", t->magnet);
    }
    exec_async_silent(cmd);
}
