#include "tmdb_win.h"
#include "baixar_win.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <windows.h>

#include "segredos.h"
#define TMDB_IMG "https://image.tmdb.org/t/p/w500"

TmdbItem tmdbResultados[TMDB_MAX_RESULTADOS];
int tmdbTotal = 0;
char tmdbStatus[256] = "";
bool tmdbBaixando = false;
float tmdbProgresso = 0;

static pthread_t g_tmdbThread;
static bool g_tmdbAtiva = false;
static char g_termoBusca[256] = "";
static char g_dirDestino[TMDB_PATH_LEN] = "";
static char g_dirTemp[TMDB_PATH_LEN] = "";
static char g_ultimoCaminho[TMDB_PATH_LEN] = "";

static int tmdb_exec_curl(const char* cmd) {
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

static void tmdb_urlencode(const char* in, char* out, int sz) {
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

static char* tmdb_strdup_json(const char* json, const char* chave, char* out, int sz) {
    char pat[64]; sprintf(pat, "\"%s\"", chave);
    const char* p = strstr(json, pat);
    if (!p) { out[0] = 0; return out; }
    p += strlen(pat);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') { out[0] = 0; return out; }
    p++;
    int o = 0;
    while (*p && *p != '"' && o < sz - 1) {
        if (*p == '\\' && *(p+1) == 'n') { out[o++] = '\n'; p += 2; continue; }
        if (*p == '\\' && *(p+1) == 't') { out[o++] = ' '; p += 2; continue; }
        if (*p == '\\' && *(p+1) == '"') { out[o++] = '"'; p += 2; continue; }
        if (*p == '\\' && *(p+1) == '\\') { out[o++] = '\\'; p += 2; continue; }
        if (*p == '\\' && *(p+1) == '/') { out[o++] = '/'; p += 2; continue; }
        out[o++] = *p++; }
    out[o] = 0;
    return out;
}

static int tmdb_int_json(const char* json, const char* chave) {
    char pat[64]; sprintf(pat, "\"%s\"", chave);
    const char* p = strstr(json, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == ':') p++;
    return atoi(p);
}

void tmdb_init(void) {
    sprintf(g_dirDestino, "%s/baixado/filmes", baixar_dir_base());
    sprintf(g_dirTemp, "%s/baixado/filmes_temp", baixar_dir_base());
    _mkdir(g_dirDestino);
    _mkdir(g_dirTemp);
}

void tmdb_buscar(const char* termo) {
    strncpy(g_termoBusca, termo, 255); g_termoBusca[255] = 0;
    tmdbTotal = 0;
    sprintf(tmdbStatus, "Buscando '%s'...", termo);
    char enc[512]; tmdb_urlencode(termo, enc, sizeof(enc));
    char url[1024]; sprintf(url, "https://api.themoviedb.org/3/search/multi?api_key=" TMDB_API_KEY "&language=pt-BR&query=%s", enc);
    char tmpJson[1024]; sprintf(tmpJson, "%s/tmdb_tmp_search.json", baixar_dir_base());
    char cmd[2200]; sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s --get -o \"%s\" \"%s\"", tmpJson, url);
    tmdb_exec_curl(cmd);

    FILE* f = fopen(tmpJson, "rb");
    if (!f) { sprintf(tmdbStatus, "Falha na busca (sem resposta)."); remove(tmpJson); return; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char* json = (char*)malloc(sz + 1); if (fread(json, 1, sz, f) != (size_t)sz) {}
    json[sz] = 0; fclose(f); remove(tmpJson);

    int total = tmdb_int_json(json, "total_results");
    if (strstr(json, "\"results\"") == NULL) { sprintf(tmdbStatus, "Erro na resposta TMDB."); free(json); return; }

    const char* p = strstr(json, "\"results\"");
    if (p) p = strchr(p, '[');
    int n = 0;
    while (p && n < TMDB_MAX_RESULTADOS) {
        p = strchr(p + 1, '{');
        if (!p) break;
        const char* fim = strchr(p, '}');
        if (!fim) break;
        int len = (int)(fim - p) + 1;
        char* item = (char*)malloc(len + 1);
        memcpy(item, p, len); item[len] = 0;

        int media = tmdb_int_json(item, "media_type");
        int ehSerie = (media == 2) || (strstr(item, "\"name\"") != NULL && strstr(item, "\"title\"") == NULL);
        tmdbResultados[n].ehSerie = ehSerie;
        if (ehSerie) {
            tmdb_strdup_json(item, "name", tmdbResultados[n].titulo, 256);
            tmdb_strdup_json(item, "original_name", tmdbResultados[n].tituloOrig, 256);
            tmdb_strdup_json(item, "first_air_date", tmdbResultados[n].data, 32);
        } else {
            tmdb_strdup_json(item, "title", tmdbResultados[n].titulo, 256);
            tmdb_strdup_json(item, "original_title", tmdbResultados[n].tituloOrig, 256);
            tmdb_strdup_json(item, "release_date", tmdbResultados[n].data, 32);
        }
        tmdb_strdup_json(item, "overview", tmdbResultados[n].overview, 2048);
        tmdb_strdup_json(item, "poster_path", tmdbResultados[n].poster, 512);
        tmdb_strdup_json(item, "backdrop_path", tmdbResultados[n].backdrop, 512);
        tmdb_strdup_json(item, "id", tmdbResultados[n].id, 32);

        free(item);
        n++;
        p = fim;
    }
    free(json);
    tmdbTotal = n;
    if (n == 0) sprintf(tmdbStatus, "Nenhum resultado para '%s'.", termo);
    else sprintf(tmdbStatus, "%d resultado(s) para '%s'.", n, termo);
}

static void* thread_tmdb(void* arg) {
    int idx = (int)(intptr_t)arg;
    if (idx < 0 || idx >= tmdbTotal) { g_tmdbAtiva = false; return NULL; }
    TmdbItem* it = &tmdbResultados[idx];
    tmdbBaixando = true; tmdbProgresso = 0;
    char pasta[TMDB_PATH_LEN];
    char nomeSafe[256];
    int si = 0;
    for (int i = 0; it->titulo[i] && si < 250; i++) {
        char c = it->titulo[i];
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
        nomeSafe[si++] = c;
    }
    nomeSafe[si] = 0;
    if (it->data[0]) { int dl = 4; if (strlen(it->data) < 4) dl = (int)strlen(it->data); char ano[8]; memcpy(ano, it->data, dl); ano[dl] = 0; strcat(nomeSafe, " ("); strcat(nomeSafe, ano); strcat(nomeSafe, ")"); }
    char nomeCompleto[300]; sprintf(nomeCompleto, "%s%s", nomeSafe, it->ehSerie ? " [SERIE]" : "");
    char pastaTemp[TMDB_PATH_LEN];
    sprintf(pastaTemp, "%s/%s", g_dirTemp, nomeCompleto);
    _mkdir(pastaTemp);

    sprintf(tmdbStatus, "Baixando: %s", nomeSafe);

    if (it->poster[0]) {
        char url[1024], dst[TMDB_PATH_LEN];
        sprintf(url, "%s%s", TMDB_IMG, it->poster);
        sprintf(dst, "%s/capa.jpg", pastaTemp);
        char cmd[2200]; sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s -o \"%s\" \"%s\"", dst, url);
        tmdb_exec_curl(cmd); tmdbProgresso = 0.5f;
    }
    if (it->backdrop[0]) {
        char url[1024], dst[TMDB_PATH_LEN];
        sprintf(url, "%s%s", TMDB_IMG, it->backdrop);
        sprintf(dst, "%s/fundo.jpg", pastaTemp);
        char cmd[2200]; sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s -o \"%s\" \"%s\"", dst, url);
        tmdb_exec_curl(cmd); tmdbProgresso = 0.8f;
    }
    if (it->overview[0]) {
        char dst[TMDB_PATH_LEN]; sprintf(dst, "%s/sinopse.txt", pastaTemp);
        FILE* t = fopen(dst, "wb");
        if (t) { fwrite(it->overview, 1, strlen(it->overview), t); fprintf(t, "\n\nTitulo original: %s\nData: %s\nTipo: %s\n", it->tituloOrig, it->data, it->ehSerie ? "Serie" : "Filme"); fclose(t); }
        tmdbProgresso = 1.0f;
    }

    char pastaFinal[TMDB_PATH_LEN];
    sprintf(pastaFinal, "%s/%s", g_dirDestino, nomeCompleto);
    remove(pastaFinal);
    if (rename(pastaTemp, pastaFinal) != 0) {
        sprintf(tmdbStatus, "Erro ao mover para permanente.");
    } else {
        sprintf(tmdbStatus, "Salvo em: %s", pastaFinal);
    }
    strcpy(g_ultimoCaminho, pastaFinal);
    tmdbBaixando = false; g_tmdbAtiva = false;
    return NULL;
}

void tmdb_baixar_item(int idx) {
    if (idx < 0 || idx >= tmdbTotal) return;
    if (!g_tmdbAtiva) { pthread_create(&g_tmdbThread, NULL, thread_tmdb, (void*)(intptr_t)idx); g_tmdbAtiva = true; }
}

void tmdb_aguardar(void) {
    if (g_tmdbAtiva) { pthread_join(g_tmdbThread, NULL); g_tmdbAtiva = false; }
}

const char* tmdb_ultimo_caminho(void) { return g_ultimoCaminho; }
const char* tmdb_ultimo_caminho_base(void) { return g_dirDestino; }

void tmdb_baixar_capa_preview(int idx, const char* dst) {
    if (idx < 0 || idx >= tmdbTotal) return;
    if (!tmdbResultados[idx].poster[0]) return;
    char url[1024];
    sprintf(url, "%s%s", TMDB_IMG, tmdbResultados[idx].poster);
    char cmd[2200]; sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s -o \"%s\" \"%s\"", dst, url);
    tmdb_exec_curl(cmd);
}

void tmdb_listar_baixados(char nomes[][256], int* total) {
    *total = 0;
    struct _finddata_t fd; char busca[TMDB_PATH_LEN]; sprintf(busca, "%s/*", g_dirDestino);
    intptr_t h = _findfirst(busca, &fd);
    if (h == -1) return;
    do {
        if (strcmp(fd.name, ".") == 0 || strcmp(fd.name, "..") == 0) continue;
        if ((fd.attrib & _A_SUBDIR) == 0) continue;
        if (*total < 1000) strcpy(nomes[(*total)++], fd.name);
    } while (_findnext(h, &fd) == 0);
    _findclose(h);
}
