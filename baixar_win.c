#include "baixar_win.h"

#include <pthread.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <windows.h>

#define MAX_PATH_LEN 1024

static char g_dirBase[MAX_PATH_LEN] = "";
static char g_dirFila[MAX_PATH_LEN] = "";
static char g_dirBaixado[MAX_PATH_LEN] = "";

char baixarUrlInput[2048] = "";
int baixarUrlCursor = 0;
bool baixarTyping = false;

char baixarStatus[512] = "Pronto.";
float baixarProgresso = 0.0f;
int baixarArquivoAtual = 0;
int baixarTotalArquivos = 0;
bool baixarBaixando = false;

BaixarItem baixarLista[BAIXAR_MAX_LINKS];
int baixarTotal = 0;
char baixarLojaErro[256] = "";

char baixarPastaRepos[128] = "";
static char g_xmlAtual[256] = "";

static char g_filaTxt[MAX_PATH_LEN] = "";
static char g_filaTmp[MAX_PATH_LEN] = "";

static bool g_lojaCarregando = false;
static char g_lojaErro[256] = "";
static pthread_t g_lojaThread;
static bool g_lojaThreadAtiva = false;


const char* baixar_dir_base(void) { return g_dirBase; }
const char* baixar_dir_fila(void) { return g_dirFila; }

static void criar_dir_rec(const char* p) {
    char tmp[MAX_PATH_LEN];
    strcpy(tmp, p);
    for (char* c = tmp + 1; *c; c++) {
        if (*c == '/' || *c == '\\') {
            char save = *c;
            *c = 0;
            _mkdir(tmp);
            *c = save;
        }
    }
    _mkdir(tmp);
}

void baixar_init(void) {
    char exePath[MAX_PATH_LEN] = ".";
    DWORD n = GetModuleFileNameA(NULL, exePath, MAX_PATH_LEN);
    if (n > 0) {
        char* slash = strrchr(exePath, '\\');
        if (!slash) slash = strrchr(exePath, '/');
        if (slash) *slash = 0;
    } else {
        char* base = getenv("USERPROFILE");
        if (base) strcpy(exePath, base);
    }
    sprintf(g_dirBase, "%s/data/HyperNeiva/baixar", exePath);
    sprintf(g_dirFila, "%s/fila", g_dirBase);
    sprintf(g_dirBaixado, "%s/baixado", g_dirBase);
    criar_dir_rec(g_dirBase);
    criar_dir_rec(g_dirFila);
    criar_dir_rec(g_dirBaixado);

    sprintf(g_filaTxt, "%s/fila_downloads.txt", g_dirFila);
    sprintf(g_filaTmp, "%s/fila_temp.txt", g_dirFila);

    baixarTotal = 0;
}

static long arquivo_tamanho(const char* path) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad)) return -1;
    LARGE_INTEGER sz;
    sz.LowPart = fad.nFileSizeLow;
    sz.HighPart = fad.nFileSizeHigh;
    return (long)sz.QuadPart;
}

bool baixar_adicionar_link_fila(const char* nome, const char* link) {
    if (!link || strlen(link) < 4) return false;
    char nomeSafe[BAIXAR_NOME_LEN];
    if (!nome || nome[0] == 0) strcpy(nomeSafe, "arquivo");
    else { strncpy(nomeSafe, nome, BAIXAR_NOME_LEN - 1); nomeSafe[BAIXAR_NOME_LEN - 1] = 0; }
    char linhaBuf[BAIXAR_NOME_LEN + BAIXAR_LINK_LEN + 8];
    sprintf(linhaBuf, "%s|%s", nomeSafe, link);
    FILE* f = fopen(g_filaTxt, "r");
    if (f) {
        char linha[BAIXAR_NOME_LEN + BAIXAR_LINK_LEN + 8];
        while (fgets(linha, sizeof(linha), f)) {
            linha[strcspn(linha, "\r\n")] = 0;
            if (strcmp(linha, linhaBuf) == 0) { fclose(f); return false; }
        }
        fclose(f);
    }
    FILE* o = fopen(g_filaTxt, "a");
    if (!o) return false;
    fprintf(o, "%s\n", linhaBuf);
    fclose(o);
    baixar_iniciar_fila();
    return true;
}

bool baixar_obter_primeiro_link_fila(char* nomeOut, char* linkOut) {
    FILE* f = fopen(g_filaTxt, "r");
    if (!f) return false;
    bool ok = false;
    char linha[BAIXAR_NOME_LEN + BAIXAR_LINK_LEN + 8];
    if (fgets(linha, sizeof(linha), f)) {
        linha[strcspn(linha, "\r\n")] = 0;
        char* bar = strchr(linha, '|');
        if (bar) {
            *bar = 0;
            strncpy(nomeOut, linha, BAIXAR_NOME_LEN - 1); nomeOut[BAIXAR_NOME_LEN - 1] = 0;
            strncpy(linkOut, bar + 1, BAIXAR_LINK_LEN - 1); linkOut[BAIXAR_LINK_LEN - 1] = 0;
            ok = (strlen(linkOut) > 0);
        }
    }
    fclose(f);
    return ok;
}

void baixar_remover_primeiro_link_fila(void) {
    FILE* f = fopen(g_filaTxt, "r");
    if (!f) return;
    FILE* o = fopen(g_filaTmp, "w");
    if (!o) { fclose(f); return; }
    char linha[BAIXAR_NOME_LEN + BAIXAR_LINK_LEN + 8];
    bool primeira = true;
    while (fgets(linha, sizeof(linha), f)) {
        if (primeira) { primeira = false; continue; }
        fputs(linha, o);
    }
    fclose(f); fclose(o);
    remove(g_filaTxt);
    rename(g_filaTmp, g_filaTxt);
}

int baixar_contar_fila(void) {
    FILE* f = fopen(g_filaTxt, "r");
    if (!f) return 0;
    int c = 0; char linha[BAIXAR_NOME_LEN + BAIXAR_LINK_LEN + 8];
    while (fgets(linha, sizeof(linha), f)) { if (strlen(linha) > 2) c++; }
    fclose(f);
    return c;
}

static void nome_arquivo_de_url(const char* url, char* out, int sz) {
    const char* p = url;
    const char* ult = url;
    for (; *p; p++) if (*p == '/' || *p == '\\') ult = p + 1;
    if (*ult == 0) ult = url;
    const char* q = ult;
    for (; *q; q++) if (*q == '?') break;
    int n = (int)(q - ult);
    if (n <= 0) n = (int)strlen(ult);
    if (n >= sz) n = sz - 1;
    memcpy(out, ult, n);
    out[n] = 0;
    if (out[0] == 0) strcpy(out, "arquivo.bin");
}

static long obter_content_length(const char* url) {
    char cmd[MAX_PATH_LEN + 256];
    sprintf(cmd, "curl -sI \"%s\"", url);
    FILE* p = _popen(cmd, "r");
    if (!p) return -1;
    char linha[1024];
    long cl = -1;
    while (fgets(linha, sizeof(linha), p)) {
        if (_strnicmp(linha, "content-length:", 14) == 0) {
            cl = atol(linha + 14);
        }
    }
    _pclose(p);
    return cl;
}

static void baixar_arquivo(const char* nomeApp, const char* url, int idx, int total) {
    baixarArquivoAtual = idx;
    baixarTotalArquivos = total;
    baixarProgresso = 0.0f;

    char urlNome[256];
    nome_arquivo_de_url(url, urlNome, sizeof(urlNome));
    char* ext = strrchr(urlNome, '.');
    char nomeFinal[256];
    if (nomeApp && nomeApp[0]) {
        strncpy(nomeFinal, nomeApp, 250); nomeFinal[250] = 0;
        if (ext && strlen(ext) <= 5) {
            if (strrchr(nomeFinal, '.') == NULL) strcat(nomeFinal, ext);
        }
    } else {
        strcpy(nomeFinal, urlNome);
    }
    char dest[MAX_PATH_LEN];
    sprintf(dest, "%s/%s", g_dirBaixado, nomeFinal);
    strcpy(baixarUltimoArquivo, dest);

    sprintf(baixarStatus, "Baixando %s", nomeFinal);
    fflush(stdout);

    long totalBytes = obter_content_length(url);
    char cmd[MAX_PATH_LEN + 512];
    sprintf(cmd, "curl -s -L -o \"%s\" \"%s\"", dest, url);

    STARTUPINFOA si; PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        sprintf(baixarStatus, "Falha ao iniciar curl para %s", nomeFinal);
        return;
    }
    while (1) {
        DWORD ec = 0;
        if (!GetExitCodeProcess(pi.hProcess, &ec)) break;
        if (ec != STILL_ACTIVE) break;
        if (totalBytes > 0) {
            long cur = arquivo_tamanho(dest);
            if (cur >= 0) baixarProgresso = (float)cur / (float)totalBytes;
            if (baixarProgresso > 0.99f) baixarProgresso = 0.99f;
        } else {
            baixarProgresso = 0.5f;
        }
        Sleep(100);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    baixarProgresso = 1.0f;
    sprintf(baixarStatus, "Concluido: %s", nomeFinal);
}

static pthread_t g_filaThread;
static bool g_filaThreadAtiva = false;
char baixarUltimoArquivo[MAX_PATH_LEN] = "";

static void* thread_fila(void* arg) {
    (void)arg;
    int total = baixar_contar_fila();
    if (total == 0) { sprintf(baixarStatus, "Fila vazia."); baixarBaixando = false; return NULL; }
    char nome[BAIXAR_NOME_LEN];
    char link[BAIXAR_LINK_LEN];
    int idx = 1;
    while (baixar_obter_primeiro_link_fila(nome, link)) {
        baixar_remover_primeiro_link_fila();
        baixar_arquivo(nome, link, idx, total);
        idx++;
    }
    baixarBaixando = false;
    sprintf(baixarStatus, "Fila concluida. %d arquivo(s).", total);
    baixarProgresso = 0.0f;
    return NULL;
}

void baixar_iniciar_fila(void) {
    if (baixarBaixando) return;
    if (baixar_contar_fila() == 0) { sprintf(baixarStatus, "Fila vazia."); return; }
    baixarBaixando = true;
    baixarProgresso = 0.0f;
    if (g_filaThreadAtiva) { pthread_join(g_filaThread, NULL); g_filaThreadAtiva = false; }
    pthread_create(&g_filaThread, NULL, thread_fila, NULL);
    g_filaThreadAtiva = true;
}

void baixar_fila_aguardar(void) {
    if (g_filaThreadAtiva) { pthread_join(g_filaThread, NULL); g_filaThreadAtiva = false; }
}

void baixar_baixar_url_direta(const char* url) {
    if (!url || strlen(url) < 4) return;
    if (!baixar_adicionar_link_fila("", url)) {
        sprintf(baixarStatus, "Link ja esta na fila ou invalido.");
    }
}

void baixar_obter_ip_local(char* ipOut) {
    strcpy(ipOut, "127.0.0.1");
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) return;
    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("8.8.8.8");
    serv.sin_port = htons(53);
    if (connect(s, (const struct sockaddr*)&serv, sizeof(serv)) == 0) {
        struct sockaddr_in name;
        int namelen = sizeof(name);
        if (getsockname(s, (struct sockaddr*)&name, &namelen) == 0) {
            const char* ip = inet_ntoa(name.sin_addr);
            if (ip) strcpy(ipOut, ip);
        }
    }
    closesocket(s);
}

void baixar_update(void) {
}

static void listar_pasta_xmls(const char* dir, BaixarItem* out, int* total) {
    WIN32_FIND_DATAA fd;
    char padrao[MAX_PATH_LEN];
    sprintf(padrao, "%s/*", dir);
    HANDLE h = FindFirstFileA(padrao, &fd);
    *total = 0;
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        if (strstr(fd.cFileName, ".xml")) {
            strcpy(out[*total].nome, fd.cFileName);
            out[*total].link[0] = 0;
            (*total)++;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

void baixar_listar_repositorios(BaixarItem* out, int* total) {
    *total = 0;
    strcpy(out[0].nome, "Games"); out[0].link[0] = 0;
    strcpy(out[1].nome, "Imagens para perfil"); out[1].link[0] = 0;
    strcpy(out[2].nome, "XML Games"); out[2].link[0] = 0;
    *total = 3;
}

void baixar_entrar_repositorio(const char* pasta) {
    strcpy(baixarPastaRepos, pasta);
}

void baixar_listar_xmls_repositorio(BaixarItem* out, int* total) {
    char dir[MAX_PATH_LEN];
    sprintf(dir, "%s/repositorios/%s", g_dirBase, baixarPastaRepos);
    criar_dir_rec(dir);
    listar_pasta_xmls(dir, out, total);
    if (*total == 0) {
        strcpy(out[0].nome, "Nenhum XML encontrado");
        out[0].link[0] = 0;
        *total = 1;
    }
}

void baixar_abrir_xml(const char* arquivoXml, BaixarItem* out, int* total) {
    char path[MAX_PATH_LEN];
    strcpy(g_xmlAtual, arquivoXml);
    sprintf(path, "%s/repositorios/%s/%s", g_dirBase, baixarPastaRepos, arquivoXml);
    FILE* f = fopen(path, "rb");
    *total = 0;
    if (!f) return;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(sz + 1);
    if (!buf) { fclose(f); return; }
    fread(buf, 1, sz, f); buf[sz] = 0; fclose(f);

    char* p = buf;
    while (*total < BAIXAR_MAX_LINKS - 1) {
        p = strstr(p, "<game name=\"");
        if (!p) break;
        p += 12;
        char* fim = strchr(p, '\"');
        if (!fim) break;
        int l = (int)(fim - p);
        if (l >= BAIXAR_NOME_LEN) l = BAIXAR_NOME_LEN - 1;
        memcpy(out[*total].nome, p, l);
        out[*total].nome[l] = 0;
        out[*total].link[0] = 0;
        (*total)++;
        p = fim;
    }
    free(buf);
}

static void decodificar_html(char* dst, const char* src, int dstSz) {
    int j = 0;
    for (int i = 0; src[i] && j < dstSz - 1; i++) {
        if (src[i] == '&' && _strnicmp(src + i, "&amp;", 5) == 0) {
            dst[j++] = '&'; i += 4;
        } else if (src[i] == '\n' || src[i] == '\r' || src[i] == '\t') {
            continue;
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = 0;
}

void baixar_mostrar_links_jogo(int gameIndex, BaixarItem* out, int* total) {
    char path[MAX_PATH_LEN];
    sprintf(path, "%s/repositorios/%s/%s", g_dirBase, baixarPastaRepos, g_xmlAtual);
    FILE* f = fopen(path, "rb");
    *total = 0;
    if (!f) return;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(sz + 1);
    if (!buf) { fclose(f); return; }
    fread(buf, 1, sz, f); buf[sz] = 0; fclose(f);

    char* p = buf;
    int currentIdx = -1;
    while (*total < BAIXAR_MAX_LINKS - 1) {
        p = strstr(p, "<game name=\"");
        if (!p) break;
        currentIdx++;
        if (currentIdx == gameIndex) {
            char* blockEnd = strstr(p, "</game>");
            char* lPtr = p;
            while (*total < BAIXAR_MAX_LINKS - 1) {
                lPtr = strstr(lPtr, "<link>");
                if (!lPtr || (blockEnd && lPtr > blockEnd)) break;
                lPtr += 6;
                char* lEnd = strstr(lPtr, "</link>");
                if (!lEnd) break;
                int len = (int)(lEnd - lPtr);
                if (len >= BAIXAR_LINK_LEN) len = BAIXAR_LINK_LEN - 1;
                char raw[BAIXAR_LINK_LEN];
                memcpy(raw, lPtr, len);
                raw[len] = 0;
                decodificar_html(out[*total].link, raw, BAIXAR_LINK_LEN);
                sprintf(out[*total].nome, "Opcao %d", *total + 1);
                (*total)++;
                lPtr = lEnd;
            }
            break;
        }
        p += 12;
    }
    free(buf);
}

void baixar_listar_lojas(BaixarItem* out, int* total) {
    *total = 0;
    strcpy(out[0].nome, "HB Store"); out[0].link[0] = 0;
    strcpy(out[1].nome, "Apollo Saves"); out[1].link[0] = 0;
    strcpy(out[2].nome, "Retroarch"); out[2].link[0] = 0;
    *total = 3;
}

void baixar_listar_dropbox(BaixarItem* out, int* total) {
    *total = 0;
    strcpy(out[0].nome, "Download (colar link direto)"); out[0].link[0] = 0;
    strcpy(out[1].nome, "Abrir pasta do Dropbox"); out[1].link[0] = 0;
    strcpy(out[2].nome, "Navegar no Dropbox (root)"); out[2].link[0] = 0;
    *total = 3;
}

void baixar_listar_ftp(BaixarItem* out, int* total) {
    *total = 0;
    strcpy(out[0].nome, "Download via URL (ftp://...)"); out[0].link[0] = 0;
    strcpy(out[1].nome, "Ver IP local"); out[1].link[0] = 0;
    *total = 2;
}

// =======================================================================
//  LOJAS (HB Store / Apollo Saves / Retroarch) - scraping interno
// =======================================================================

static void baixar_url_para_arquivo(const char* url, const char* outPath) {
    char cmd[MAX_PATH_LEN + 256];
    sprintf(cmd, "curl -s -L -m 30 -o \"%s\" \"%s\"", outPath, url);
    system(cmd);
}

static void limpar_nome(char* dst, const char* src, int dstSz) {
    int j = 0; bool ultimoEspaco = true;
    for (int i = 0; src[i] && j < dstSz - 1; i++) {
        char c = src[i];
        if (c == '%' && src[i+1] == '2' && src[i+2] == '0') { c = ' '; i += 2; }
        else if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        if (c == ' ') { if (!ultimoEspaco) { dst[j++] = ' '; ultimoEspaco = true; } }
        else { dst[j++] = c; ultimoEspaco = false; }
    }
    if (j > 0 && dst[j-1] == ' ') j--;
    dst[j] = 0;
}

static void* thread_hbstore(void* arg) {
    (void)arg;
    char tmp[MAX_PATH_LEN];
    sprintf(tmp, "%s/temp_hbstore.html", g_dirBase);
    baixar_url_para_arquivo("https://pkg-zone.com/", tmp);

    FILE* f = fopen(tmp, "rb");
    if (!f) { strcpy(g_lojaErro, "Falha ao baixar a HB Store"); g_lojaCarregando = false; return NULL; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char* h = (char*)malloc(sz + 1);
    if (!h) { fclose(f); strcpy(g_lojaErro, "Sem memoria"); g_lojaCarregando = false; return NULL; }
    fread(h, 1, sz, f); h[sz] = 0; fclose(f);

    baixarTotal = 0;
    char* p = h;
    while (baixarTotal < BAIXAR_MAX_LINKS - 1) {
        char* tag = strstr(p, "<article");
        if (!tag) break;
        char* href = strstr(tag, "href=\"https://pkg-zone.com/details/");
        char* artEnd = strstr(tag, "</article>");
        if (href && artEnd && href < artEnd) {
            href += strlen("href=\"https://pkg-zone.com/details/");
            char* fimHref = strchr(href, '\"');
            if (fimHref) {
                int idLen = (int)(fimHref - href);
                if (idLen > 0 && idLen < 64) {
                    char id[64]; memcpy(id, href, idLen); id[idLen] = 0;
                    // procura o nome no bloco
                    char* titleTag = strstr(tag, "<div class=\"title font-bold\">");
                    char nome[256] = "";
                    if (titleTag) {
                        titleTag += strlen("<div class=\"title font-bold\">");
                        char* fimT = strstr(titleTag, "</div>");
                        if (fimT) {
                            int nLen = (int)(fimT - titleTag);
                            if (nLen > 0 && nLen < 255) { memcpy(nome, titleTag, nLen); nome[nLen] = 0; }
                        }
                    }
                    if (nome[0] == 0) strcpy(nome, id);
                    limpar_nome(baixarLista[baixarTotal].nome, nome, BAIXAR_NOME_LEN);
                    sprintf(baixarLista[baixarTotal].link, "https://pkg-zone.com/download/ps4/%s/latest", id);
                    baixarTotal++;
                }
            }
        }
        p = artEnd ? artEnd + 1 : tag + 8;
    }
    free(h);
    if (baixarTotal == 0) strcpy(g_lojaErro, "Nenhum item na loja");
    g_lojaCarregando = false;
    return NULL;
}

static void apollo_coletar_zips(const char* jsonPath, int* limiteSub) {
    FILE* f = fopen(jsonPath, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char* h = (char*)malloc(sz + 1);
    if (!h) { fclose(f); return; }
    fread(h, 1, sz, f); h[sz] = 0; fclose(f);

    if (strstr(h, "API rate limit exceeded") || strstr(h, "Not Found") || strstr(h, "\"message\"")) {
        free(h); return;
    }

    // 1) pega .zip diretos neste nivel
    char* p = h;
    while (baixarTotal < BAIXAR_MAX_LINKS - 1) {
        p = strstr(p, "\"name\"");
        if (!p) break;
        char* colon = strchr(p, ':');
        if (!colon || (colon - p) > 15) { p += 6; continue; }
        char* q1 = strchr(colon, '\"');
        if (!q1) break;
        char* q2 = strchr(q1 + 1, '\"');
        if (!q2) break;
        int nLen = (int)(q2 - (q1 + 1));
        if (nLen <= 0 || nLen >= 128) { p = q2 + 1; continue; }
        char nome[128]; memcpy(nome, q1 + 1, nLen); nome[nLen] = 0;

        if (strstr(nome, ".zip") == NULL) { p = q2 + 1; continue; }

        char* du = strstr(q2, "\"download_url\"");
        if (!du) { p = q2 + 1; continue; }
        char* dcolon = strchr(du, ':');
        if (!dcolon) { p = q2 + 1; continue; }
        char* v = dcolon + 1;
        while (*v == ' ' || *v == '\t') v++;
        if (*v != '\"') { p = q2 + 1; continue; }
        char* dq1 = v;
        char* dq2 = strchr(dq1 + 1, '\"');
        if (!dq2) { p = q2 + 1; continue; }
        int uLen = (int)(dq2 - (dq1 + 1));
        if (uLen <= 0 || uLen >= BAIXAR_LINK_LEN) { p = q2 + 1; continue; }

        limpar_nome(baixarLista[baixarTotal].nome, nome, BAIXAR_NOME_LEN);
        memcpy(baixarLista[baixarTotal].link, dq1 + 1, uLen); baixarLista[baixarTotal].link[uLen] = 0;
        baixarTotal++;
        p = dq2 + 1;
    }

    // 2) entra nas subpastas (dir) recursivamente, limitado
    if (*limiteSub > 0) {
        p = h;
        while (baixarTotal < BAIXAR_MAX_LINKS - 1 && *limiteSub > 0) {
            p = strstr(p, "\"name\"");
            if (!p) break;
            char* colon = strchr(p, ':');
            if (!colon || (colon - p) > 15) { p += 6; continue; }
            char* q1 = strchr(colon, '\"');
            if (!q1) break;
            char* q2 = strchr(q1 + 1, '\"');
            if (!q2) break;
            int nLen = (int)(q2 - (q1 + 1));
            if (nLen <= 0 || nLen >= 128) { p = q2 + 1; continue; }
            char nome[128]; memcpy(nome, q1 + 1, nLen); nome[nLen] = 0;
            if (strstr(nome, ".zip") != NULL) { p = q2 + 1; continue; }

            // eh dir se download_url for null
            char* du = strstr(q2, "\"download_url\"");
            bool isDir = true;
            if (du) {
                char* dc = strchr(du, ':');
                if (dc) { char* vv = dc + 1; while (*vv == ' ' || *vv == '\t') vv++; if (*vv == '\"') isDir = false; }
            }
            if (!isDir) { p = q2 + 1; continue; }

            (*limiteSub)--;
            char subUrl[512];
            sprintf(subUrl, "https://api.github.com/repos/bucanero/apollo-saves/contents/PS4/%s", nome);
            char subTmp[MAX_PATH_LEN];
            sprintf(subTmp, "%s/temp_apollo_sub.json", g_dirBase);
            baixar_url_para_arquivo(subUrl, subTmp);
            apollo_coletar_zips(subTmp, limiteSub);
            p = q2 + 1;
        }
    }
    free(h);
}

static void* thread_apollo(void* arg) {
    (void)arg;
    char tmp[MAX_PATH_LEN];
    sprintf(tmp, "%s/temp_apollo.json", g_dirBase);
    baixar_url_para_arquivo("https://api.github.com/repos/bucanero/apollo-saves/contents/PS4", tmp);

    baixarTotal = 0;
    int limite = 10;
    apollo_coletar_zips(tmp, &limite);

    if (baixarTotal == 0) strcpy(g_lojaErro, "Nenhum save na pasta PS4");
    g_lojaCarregando = false;
    return NULL;
}

static void* thread_retroarch(void* arg) {
    (void)arg;
    baixarTotal = 0;
    char tmp[MAX_PATH_LEN];
    sprintf(tmp, "%s/temp_retroarch.html", g_dirBase);
    baixar_url_para_arquivo("https://www.retroarch.com/?page=platforms", tmp);

    FILE* f = fopen(tmp, "rb");
    if (!f) { strcpy(g_lojaErro, "Falha ao baixar dados do RetroArch"); g_lojaCarregando = false; return NULL; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char* h = (char*)malloc(sz + 1);
    if (!h) { fclose(f); strcpy(g_lojaErro, "Sem memoria"); g_lojaCarregando = false; return NULL; }
    fread(h, 1, sz, f); h[sz] = 0; fclose(f);

    char* p = h;
    while (baixarTotal < BAIXAR_MAX_LINKS - 1 && (p = strstr(p, "buildbot.libretro.com/stable/"))) {
        char* fim = p;
        while (*fim && *fim != '"' && *fim != ' ' && *fim != '>' && *fim != '<' && *fim != '\n' && *fim != '\r') fim++;
        int uLen = (int)(fim - p);
        if (uLen <= 0 || uLen >= BAIXAR_LINK_LEN) { p = fim; continue; }
        char url[BAIXAR_LINK_LEN];
        if (uLen > 6 && strncmp(p, "http://", 7) != 0 && strncmp(p, "https://", 8) != 0) {
            sprintf(url, "https://%.*s", uLen, p);
        } else {
            memcpy(url, p, uLen); url[uLen] = 0;
        }

        char* barra = strrchr(url, '/');
        char* nomeUrl = barra ? barra + 1 : url;
        bool isWin = (strstr(url, "/windows/") != NULL) || (strstr(url, "/windows-msvc") != NULL);
        bool isExeOuZip = (strstr(nomeUrl, ".exe") != NULL) || (strstr(nomeUrl, ".zip") != NULL);
        if (!isWin || !isExeOuZip) { p = fim; continue; }

        char nome[BAIXAR_NOME_LEN];
        limpar_nome(nome, nomeUrl, BAIXAR_NOME_LEN);
        strcpy(baixarLista[baixarTotal].nome, nome);
        strcpy(baixarLista[baixarTotal].link, url);
        baixarTotal++;
        p = fim;
    }

    free(h);
    if (baixarTotal == 0) {
        strcpy(g_lojaErro, "Nenhum binario do Windows encontrado");
        strcpy(baixarLista[baixarTotal].nome, "Ver site oficial");
        strcpy(baixarLista[baixarTotal].link, "https://www.retroarch.com/?page=platforms");
        baixarTotal++;
    }
    g_lojaCarregando = false;
    return NULL;
}

void baixar_loja_cancelar(void) {
    // aguarda a thread terminar (nao mata a meia via para evitar corrupcao)
    if (g_lojaThreadAtiva) {
        pthread_join(g_lojaThread, NULL);
        g_lojaThreadAtiva = false;
    }
}

bool baixar_loja_carregando(void) { return g_lojaCarregando; }

void baixar_carregar_hbstore(void) {
    baixar_loja_cancelar();
    baixarTotal = 0; g_lojaCarregando = true; g_lojaErro[0] = 0;
    pthread_create(&g_lojaThread, NULL, thread_hbstore, NULL);
    g_lojaThreadAtiva = true;
}

void baixar_carregar_apollo(void) {
    baixar_loja_cancelar();
    baixarTotal = 0; g_lojaCarregando = true; g_lojaErro[0] = 0;
    pthread_create(&g_lojaThread, NULL, thread_apollo, NULL);
    g_lojaThreadAtiva = true;
}

void baixar_carregar_retroarch(void) {
    baixar_loja_cancelar();
    baixarTotal = 0; g_lojaCarregando = true; g_lojaErro[0] = 0;
    pthread_create(&g_lojaThread, NULL, thread_retroarch, NULL);
    g_lojaThreadAtiva = true;
}

