#include "raylib.h"
#include "segredos.h"
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <pthread.h>

char *strcasestr_custom(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; ++haystack) {
        if (toupper(*haystack) == toupper(*needle)) {
            const char *h = haystack, *n = needle;
            while (*h && *n && toupper(*h) == toupper(*n)) { ++h; ++n; }
            if (!*n) return (char *)haystack;
        }
    }
    return NULL;
}

#include <sys/stat.h>
#include <direct.h>
#include "criar_pastas_win.h"
#include <mupdf/fitz.h>
#include "radio.h"
#include "instrumentos.h"
#include "controle_teste.h"
#include "teclado_teste.h"
#include "ferramentas.h"
#include "video_player.h"
#include "baixar_win.h"
#include "ftp_win.h"
#include "tmdb_win.h"
#include "torrent_win.h"
#include "midia_win.h"
#include "api_crypt.h"
#include "baixar_dropbox_win.h"
#include "baixar_google_drive_win.h"
#include "gemini_ia.h"

fz_context* ctx_pdf = NULL;
fz_document* doc_pdf = NULL;
Texture2D pdfTexture = { 0 };
bool pdfLoaded = false;
int pdfPaginaAtual = 1;
int pdfTotalPaginas = 1;

void fecharPDF() {
    if (pdfLoaded) {
        UnloadTexture(pdfTexture);
        pdfLoaded = false;
    }
    if (doc_pdf) { fz_drop_document(ctx_pdf, doc_pdf); doc_pdf = NULL; }
    if (ctx_pdf) { fz_drop_context(ctx_pdf); ctx_pdf = NULL; }
}

void carregarPaginaPDF(int pagina) {
    if (pagina < 1 || pagina > pdfTotalPaginas) return;
    if (pdfLoaded) { UnloadTexture(pdfTexture); pdfLoaded = false; }

    fz_matrix ctm = fz_scale(2.0f, 2.0f);
    fz_page* page = fz_load_page(ctx_pdf, doc_pdf, pagina - 1);
    fz_pixmap* pix = fz_new_pixmap_from_page_number(ctx_pdf, doc_pdf, pagina - 1, ctm, fz_device_rgb(ctx_pdf), 0);

    if (pix) {
        int w = pix->w; int h = pix->h; int n = pix->n;
        unsigned char* samples = pix->samples;
        
        unsigned char* imgData = (unsigned char*)malloc(w * h * 4);
        for (int i = 0; i < w * h; i++) {
            imgData[i * 4 + 0] = samples[i * n + 0];
            imgData[i * 4 + 1] = samples[i * n + 1];
            imgData[i * 4 + 2] = samples[i * n + 2];
            imgData[i * 4 + 3] = (n == 4) ? samples[i * n + 3] : 255;
        }
        
        Image img = { imgData, w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
        pdfTexture = LoadTextureFromImage(img);
        UnloadImage(img);
        
        pdfLoaded = true;
        pdfPaginaAtual = pagina;
        fz_drop_pixmap(ctx_pdf, pix);
    }
    fz_drop_page(ctx_pdf, page);
}

void abrirPDF(const char* caminho) {
    fecharPDF();
    ctx_pdf = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    if (!ctx_pdf) return;
    fz_register_document_handlers(ctx_pdf);
    fz_try(ctx_pdf) {
        doc_pdf = fz_open_document(ctx_pdf, caminho);
        pdfTotalPaginas = fz_count_pages(ctx_pdf, doc_pdf);
    }
    fz_catch(ctx_pdf) { fecharPDF(); return; }
    if (pdfTotalPaginas > 0) carregarPaginaPDF(1);
}

typedef enum {
    MENU_MAIN = 0,
    MENU_MIDIA,
    MENU_JOGAR,
    MENU_BAIXAR,
    MENU_EDITAR,
    MENU_EXPLORAR,
    MENU_EXTRA,
    MENU_INFORMACOES,
    MENU_MIDIA_VIDEOS,
    MENU_MIDIA_IMAGENS,
    MENU_MIDIA_LEITURA,
    MENU_MIDIA_MUSICAS,
    MENU_MIDIA_ARQUIVOS,
    MENU_MIDIA_RADIO_CATEGORIA,
    MENU_MIDIA_RADIO_LISTA,
    MENU_MIDIA_RADIO_OUTROS,
    MENU_PESQUISA_GLOBAL,
    MENU_AUDIO_OPCOES,
    MENU_ARQUIVOS_OPCOES_GERAL,
    MENU_ARQUIVOS_OPCOES_ARQUIVO,
    MENU_ARQUIVOS_TECLADO,
    MENU_BLOCO_DE_NOTAS,
    MENU_INSTRUMENTOS,
    MENU_CONTROLE_TESTE,
    MENU_TECLADO_TESTE,
    MENU_FERRAMENTAS,
    MENU_VIDEO,
    MENU_BAIXAR_LINK,
    MENU_BAIXAR_REPOS,
    MENU_BAIXAR_REPOS_XML,
    MENU_BAIXAR_REPOS_GAMES,
    MENU_BAIXAR_REPOS_LINKS,
    MENU_BAIXAR_DROPBOX,
    MENU_BAIXAR_DROPBOX_NAV,
    MENU_BAIXAR_GOOGLE_DRIVE_NAV,
    MENU_BAIXAR_LOJAS,
    MENU_BAIXAR_HB,
    MENU_BAIXAR_APOLLO,
    MENU_BAIXAR_RETRO,
    MENU_BAIXAR_FTP,
    MENU_BAIXAR_FTP_EDITAR,
    MENU_BAIXAR_FTP_GERENCIAR,
    MENU_BAIXAR_FILMES,
    MENU_BAIXAR_FILMES_BAIXADOS,
    MENU_BAIXAR_FILMES_TORRENT,
    MENU_BAIXAR_MIDIA,
    MENU_BAIXAR_ANIME,
    MENU_BAIXAR_ANIME_TORRENT,
    MENU_BAIXAR_MUSICA,
    MENU_BAIXAR_LIVROS,
    MENU_BAIXAR_LEGENDAS,
    MENU_BAIXAR_NUVEM,
    MENU_BAIXAR_FILA,
    MENU_CONFIGURACOES,
    MENU_CONFIG_API,
    MENU_CONFIG_API_GERAR_DROPBOX
} MenuState;

MenuState currentState = MENU_MAIN;
int scrollY = 0;
static bool ferramentasViaAbaPrincipal = false;

static int ftpRemoteSel = 0;
static int ftpLocalSel = 0;
static int ftpPainelAtivo = 0;
static int ftpEditandoCampo = -1;
static char ftpEditBuf[256] = "";
static Texture2D tmdbCapa;
static bool tmdbCapaLoaded;
static char tmdbSinopse[4096];
static int tmdbSinopseScroll = 0;
static bool tmdbMostraTexto = false;
static int tmdbSel = 0;
static char notifyMsg[256] = "";
static int notifyTimer = 0;

static void notificar(const char* msg) { strncpy(notifyMsg, msg, 255); notifyMsg[255] = 0; notifyTimer = 180; }
static bool dropboxConectando = false;
static int dbScroll = 0;
static int dropboxRcItem = -1;
static int dropboxRcX = 0, dropboxRcY = 0;
static bool dropboxMarcados[DROPBOX_MAX_ITEMS] = { false };
static int dropboxInputAction = 0; // 0=none, 1=renomear, 2=nova pasta

static char dropboxGenStatus[256] = "";

static int gdInputAction = 0; // 0=none, 2=nova pasta
static char gdInputBuf[128] = "";

static void input_clipboard(char* buf, int maxLen, bool* allSelected) {
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_C)) {
        SetClipboardText(buf);
    }
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_X)) {
        SetClipboardText(buf);
        if (allSelected && *allSelected) { buf[0] = 0; *allSelected = false; }
        else buf[0] = 0;
    }
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_V)) {
        const char* txt = GetClipboardText();
        if (txt && txt[0]) {
            if (allSelected && *allSelected) { buf[0] = 0; *allSelected = false; }
            int l = (int)strlen(buf); int tl = (int)strlen(txt);
            for (int c = 0; c < tl && l < maxLen - 2; c++) { if (txt[c] >= 32) buf[l++] = txt[c]; }
            buf[l] = 0;
        }
    }
}

static void dropbox_exchange(const char* code, const char* key, const char* secret, char* outBuf, int outSz) {
    outBuf[0] = 0;
    char batPath[1024];
    const char* tmp = getenv("TEMP");
    if (!tmp) tmp = "C:\\temp";
    snprintf(batPath, sizeof(batPath), "%s\\dropbox_exchange.bat", tmp);
    FILE* f = fopen(batPath, "w");
    if (!f) { strcpy(dropboxGenStatus, "Erro: falha ao criar bat"); return; }
    fprintf(f, "@echo off\n\"C:\\Windows\\System32\\curl.exe\" -s -X POST \"https://api.dropbox.com/oauth2/token\" -H \"Content-Type: application/x-www-form-urlencoded\" --data-urlencode \"code=%s\" --data-urlencode \"grant_type=authorization_code\" --data-urlencode \"client_id=%s\" --data-urlencode \"client_secret=%s\"\n",
            code, key, secret);
    fclose(f);
    strcpy(dropboxGenStatus, "Requisitando token...");
    char buf[16384]; int bufLen = 0; buf[0] = 0;
    FILE* pipe = _popen(batPath, "r");
    if (!pipe) { remove(batPath); strcpy(dropboxGenStatus, "Erro: falha ao executar curl"); return; }
    while (fgets(buf + bufLen, sizeof(buf) - bufLen, pipe)) { bufLen = (int)strlen(buf); if (bufLen >= (int)sizeof(buf) - 2) break; }
    int ret = _pclose(pipe); (void)ret;
    remove(batPath);
    if (bufLen <= 0) { strcpy(dropboxGenStatus, "Erro: resposta vazia da API (curl falhou?)"); return; }
    // Debug: mostra primeiros 300 caracteres da resposta
    char debugResp[512]; strncpy(debugResp, buf, 300); debugResp[300] = 0;
    char* refStart = strstr(buf, "\"refresh_token\": \"");
    if (refStart) {
        refStart += 18;
        char* refEnd = strchr(refStart, '\"');
        if (refEnd) {
            int refLen = (int)(refEnd - refStart);
            if (refLen > outSz - 1) refLen = outSz - 1;
            strncpy(outBuf, refStart, refLen);
            outBuf[refLen] = 0;
            sprintf(dropboxGenStatus, "OK! Refresh token salvo (%d caracteres)", refLen);
        } else { sprintf(dropboxGenStatus, "Erro formato JSON. Resp: %.250s", debugResp); }
    } else {
        char* errDesc = strstr(buf, "\"error_description\": \"");
        if (errDesc) {
            errDesc += 22;
            char* errEnd = strchr(errDesc, '\"');
            if (errEnd) { int eLen = (int)(errEnd - errDesc); char errMsg[256]; strncpy(errMsg, errDesc, eLen); errMsg[eLen] = 0; sprintf(dropboxGenStatus, "Erro: %s", errMsg); }
            else { sprintf(dropboxGenStatus, "Erro: %s", buf); }
        } else {
            char* errOnly = strstr(buf, "\"error\": \"");
            if (errOnly) {
                errOnly += 10;
                char* errEnd = strchr(errOnly, '\"');
                if (errEnd) { int eLen = (int)(errEnd - errOnly); char errMsg[256]; strncpy(errMsg, errOnly, eLen); errMsg[eLen] = 0; sprintf(dropboxGenStatus, "Erro API: %s", errMsg); }
                else { sprintf(dropboxGenStatus, "Erro: %s", buf); }
            } else {
                sprintf(dropboxGenStatus, "Erro: resposta inesperada. %.250s", debugResp); }
}
    }
}

static void tmdb_carregar_exibicao(const char* pasta) {
    tmdbSinopseScroll = 0;
    if (tmdbCapaLoaded) { UnloadTexture(tmdbCapa); tmdbCapaLoaded = false; }
    tmdbSinopse[0] = 0;
    char capa[TMDB_PATH_LEN]; sprintf(capa, "%s/capa.jpg", pasta);
    if (FileExists(capa)) { tmdbCapa = LoadTexture(capa); tmdbCapaLoaded = true; }
    char sino[TMDB_PATH_LEN]; sprintf(sino, "%s/sinopse.txt", pasta);
    FILE* f = fopen(sino, "rb");
    if (f) { fseek(f, 0, SEEK_END); long sz = ftell(f); if (sz > 4095) sz = 4095; fseek(f, 0, SEEK_SET); if (fread(tmdbSinopse, 1, sz, f) == (size_t)sz) {} tmdbSinopse[sz] = 0; fclose(f); }
}

static int tmdb_desenhar_sinopse(Rectangle area, int fontSize, int* scrollOut) {
    int lineH = fontSize + 6;
    int x = (int)area.x + 12;
    int y0 = (int)area.y + 10;
    int maxW = (int)area.width - 24;
    const char* txt = tmdbSinopse;
    int totalLines = 0;
    const char* p = txt;
    while (*p) {
        int used = 0; const char* q = p;
        char buf[512];
        while (*q && used < (int)sizeof(buf) - 2) {
            const char* wstart = q; int wlen = 0;
            while (*q && *q != ' ' && *q != '\n') { wlen++; q++; }
            int testW = MeasureText(TextFormat("%.*s", used + wlen, p), fontSize);
            if (used > 0 && testW > maxW) break;
            if (used + wlen >= (int)sizeof(buf) - 2) break;
            memcpy(buf + used, wstart, wlen); used += wlen;
            if (*q == ' ') { buf[used++] = ' '; q++; }
            else if (*q == '\n') { q++; break; }
        }
        if (used == 0) { buf[0] = *p; buf[1] = 0; p++; }
        else { buf[used] = 0; p = q; }
        int linhaVisivel = (totalLines * lineH) - *scrollOut;
        if (linhaVisivel >= -lineH && linhaVisivel < (int)area.height) {
            DrawText(buf, x, y0 + linhaVisivel, fontSize, (Color){220,220,220,255});
        }
        totalLines++;
    }
    int contentH = totalLines * lineH;
    int visH = (int)area.height - 20;
    int maxScroll = contentH > visH ? contentH - visH : 0;
    if (*scrollOut < 0) *scrollOut = 0;
    if (*scrollOut > maxScroll) *scrollOut = maxScroll;
    return maxScroll;
}

static void tmdb_mostrar_texto(void);
static void tmdb_selecionar(int idx) {
    if (idx < 0 || idx >= tmdbTotal) return;
    if (tmdbCapaLoaded) { UnloadTexture(tmdbCapa); tmdbCapaLoaded = false; }
    tmdbSinopse[0] = 0;
    tmdbMostraTexto = false;
    char tmp[TMDB_PATH_LEN]; sprintf(tmp, "%s/baixado/filmes_temp/_preview", baixar_dir_base());
    _mkdir(tmp);
    char capa[TMDB_PATH_LEN]; sprintf(capa, "%s/capa.jpg", tmp);
    tmdb_baixar_capa_preview(idx, capa);
    if (FileExists(capa)) { tmdbCapa = LoadTexture(capa); tmdbCapaLoaded = true; }
    tmdbSinopseScroll = 0;
}

static void tmdb_mostrar_texto(void) {
    if (tmdbSel < 0 || tmdbSel >= tmdbTotal) return;
    strncpy(tmdbSinopse, tmdbResultados[tmdbSel].overview, 4095); tmdbSinopse[4095] = 0;
    tmdbMostraTexto = true;
    tmdbSinopseScroll = 0;
}

static int ftpCriandoPasta = 0;
static char tmdbBusca[256] = "";
static bool tmdbTyping = false;

static int torrentProvedor = 2; /* 0=YTS 1=1337x 2=Ambos */
static int torrentSel = 0;
static int torrentScroll = 0;
static int torrentPagina = 0;
#define TORRENT_POR_PAG 12

static int midiaSel = 0;
static int midiaScroll = 0;
static char midiaBusca[256] = "";
static bool midiaTyping = false;

void voltarParaMenuExtra(void) {
    currentState = MENU_EXTRA;
    scrollY = 0;
}

char clipboardPath[1024] = "";
bool clipboardIsCut = false;
char targetFilePath[1024] = "";
int keyboardInputMode = 0; // 0=Rename, 1=New Folder, 2=New File

char linhasNotepad[500][256];
int totalLinhasNotepad = 0;
int linhaSelecionadaNotepad = 0;
char arquivoAbertoNotepad[1024] = "";
int scrollNotepad = 0;

// Background wallpaper
bool bgImageLoaded = false;
bool bgNeedsReload = false;
char bgImagePath[1024] = "data/HyperNeiva/configuracao/background.png";

// Image viewer
Texture2D viewerImage = { 0 };
bool viewerImageLoaded = false;
char viewerImageTitle[256] = "Nenhuma imagem selecionada.";
bool isFullScreenImage = false;
double lastImageClickTime = 0.0;

// Global audio & player state
Music currentMusic = { 0 };
bool musicLoaded = false;
bool isPlaying = false;
char musicTitle[256] = "Nenhuma musica reproduzindo...";
char currentMusicPath[256] = "";
int volumeGeral = 100;

char micTranscribed[8192] = "";
bool micThreadRunning = false;


void copyFileReal(const char* src, const char* dst) {
    FILE* fSrc = fopen(src, "rb"); if (!fSrc) return;
    FILE* fDst = fopen(dst, "wb"); if (!fDst) { fclose(fSrc); return; }
    char buf[8192]; size_t bytes;
    while ((bytes = fread(buf, 1, sizeof(buf), fSrc)) > 0) fwrite(buf, 1, bytes, fDst);
    fclose(fSrc); fclose(fDst);
}

void* mic_thread(void* arg) {
    // Transcribe recorded audio using Google Speech Recognition via Python script
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "python scripts/google_transcribe.py data/HyperNeiva/top_mic.wav");
    char output[512] = "";
    // exec_async_capture captures stdout into output
    extern void exec_async_capture(const char*, char*, size_t);
    exec_async_capture(cmd, output, sizeof(output));
    // Store result (trim newline)
    size_t len = strlen(output);
    if (len > 0 && output[len-1] == '\n') output[len-1] = '\0';
    strncpy(micTranscribed, output, sizeof(micTranscribed)-1);
    micTranscribed[sizeof(micTranscribed)-1] = '\0';
    micThreadRunning = false;
    return NULL;
}

void definirComoBackground(const char* src) {
    const char* ext = strrchr(src, '.');
    char dest[1024];
    if (ext && (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0))
        strcpy(dest, "data/HyperNeiva/configuracao/background.jpg");
    else
        strcpy(dest, "data/HyperNeiva/configuracao/background.png");
    remove("data/HyperNeiva/configuracao/background.png");
    remove("data/HyperNeiva/configuracao/background.jpg");
    copyFileReal(src, dest);
    strcpy(bgImagePath, dest);
    bgImageLoaded = true;
    bgNeedsReload = true;
}

void extrairZip(const char* zipPath, const char* destDir) {
    char winZip[1024], winDest[1024];
    strcpy(winZip, zipPath); strcpy(winDest, destDir);
    for (int i = 0; winZip[i]; i++) if (winZip[i] == '/') winZip[i] = '\\';
    for (int i = 0; winDest[i]; i++) if (winDest[i] == '/') winDest[i] = '\\';
    char cmd[2300];
    sprintf(cmd, "powershell -WindowStyle Hidden -Command \"Expand-Archive -Path '%s' -DestinationPath '%s' -Force\" >nul 2>&1", winZip, winDest);
    system(cmd);
}

void compactarZip(const char* srcPath, const char* zipDest) {
    char winSrc[1024], winDest[1024];
    strcpy(winSrc, srcPath); strcpy(winDest, zipDest);
    for (int i = 0; winSrc[i]; i++) if (winSrc[i] == '/') winSrc[i] = '\\';
    for (int i = 0; winDest[i]; i++) if (winDest[i] == '/') winDest[i] = '\\';
    char cmd[2300];
    sprintf(cmd, "powershell -WindowStyle Hidden -Command \"Compress-Archive -Path '%s' -DestinationPath '%s' -Force\" >nul 2>&1", winSrc, winDest);
    system(cmd);
}

void abrirNotepad(const char* filepath) {
    memset(linhasNotepad, 0, sizeof(linhasNotepad));
    totalLinhasNotepad = 0;
    linhaSelecionadaNotepad = -1;
    scrollNotepad = 0;
    strcpy(arquivoAbertoNotepad, filepath);
    
    FILE* f = fopen(filepath, "r");
    if (f) {
        char linhaTemp[256];
        while (fgets(linhaTemp, sizeof(linhaTemp), f) != NULL && totalLinhasNotepad < 500) {
            int len = strlen(linhaTemp);
            while (len > 0 && (linhaTemp[len - 1] == '\n' || linhaTemp[len - 1] == '\r')) {
                linhaTemp[len - 1] = '\0'; len--;
            }
            strncpy(linhasNotepad[totalLinhasNotepad], linhaTemp, 255);
            totalLinhasNotepad++;
        }
        fclose(f);
    }
    if (totalLinhasNotepad == 0) totalLinhasNotepad = 1;
}

void salvarNotepad() {
    FILE* f = fopen(arquivoAbertoNotepad, "w");
    if (f) {
        for (int i = 0; i < totalLinhasNotepad; i++) {
            fprintf(f, "%s\n", linhasNotepad[i]);
        }
        fclose(f);
    }
}

char caminhoMusicas[512] = "data/HyperNeiva/midia/musicas";
char listaMusicasNomes[1000][128];
char listaMusicasCaminhos[1000][256];
bool listaMusicasEhPasta[1000];
int qtdMusicas = 0;

char caminhoImagens[512] = "data/HyperNeiva/midia/imagens";
char listaImagensNomes[1000][128];
char listaImagensCaminhos[1000][256];
bool listaImagensEhPasta[1000];
int qtdImagens = 0;

char caminhoLeitura[512] = "data/HyperNeiva/midia/leitura";
char listaLeituraNomes[1000][128];
char listaLeituraCaminhos[1000][256];
bool listaLeituraEhPasta[1000];
int qtdLeitura = 0;

char caminhoArquivos[512] = "data/HyperNeiva/midia/arquivos";
char listaArquivosNomes[1000][128];
char listaArquivosCaminhos[1000][256];
bool listaArquivosEhPasta[1000];
int qtdArquivos = 0;

Texture2D coverTextures[3] = { {0}, {0}, {0} };
char coverNames[3][128];
int coverCount = 0;
char dadosText[1024] = "Sem dados";

void carregarMetadadosMusica(const char* fullMusicPath) {
    for (int i = 0; i < coverCount; i++) {
        UnloadTexture(coverTextures[i]);
    }
    coverCount = 0;
    
    char metaPath[1024];
    const char* root = "data/HyperNeiva/midia/musicas";
    if (strncmp(fullMusicPath, root, strlen(root)) == 0) {
        const char* relativePath = fullMusicPath + strlen(root); 
        if (relativePath[0] == '/' || relativePath[0] == '\\') relativePath++; 
        
        char relCopy[1024];
        strcpy(relCopy, relativePath);
        
        char* ext = strrchr(relCopy, '.');
        if (ext) *ext = '\0'; 
        
        sprintf(metaPath, "%s/dados de musica/%s", root, relCopy);
    } else {
        strcpy(dadosText, "Sem dados");
        return;
    }
    
    DIR* d = opendir(metaPath);
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL && coverCount < 3) {
            if (strstr(dir->d_name, ".png") || strstr(dir->d_name, ".PNG") ||
                strstr(dir->d_name, ".jpg") || strstr(dir->d_name, ".JPG")) {
                char imgPath[1024];
                sprintf(imgPath, "%s/%s", metaPath, dir->d_name);
                coverTextures[coverCount] = LoadTexture(imgPath);
                
                char nameCopy[256];
                strncpy(nameCopy, dir->d_name, 255);
                nameCopy[255] = '\0';
                
                char* dot = strrchr(nameCopy, '.');
                if (dot) *dot = '\0';
                
                char* displayName = nameCopy;
                if (strncmp(nameCopy, "a_", 2) == 0) displayName += 2;
                
                strncpy(coverNames[coverCount], displayName, 127);
                coverNames[coverCount][127] = '\0';
                
                coverCount++;
            }
        }
        closedir(d);
    }
    
    char filePath[1024];
    sprintf(filePath, "%s/dados.txt", metaPath);
    if (FileExists(filePath)) {
        char* text = LoadFileText(filePath);
        if (text) {
            strncpy(dadosText, text, 1023);
            dadosText[1023] = '\0';
            UnloadFileText(text); 
        } else {
            strcpy(dadosText, "Sem dados");
        }
    } else {
        strcpy(dadosText, "Sem dados");
    }
}

void criarDadosDeMusica(const char* fullMusicPath) {
    const char* root = "data/HyperNeiva/midia/musicas";
    
    if (strncmp(fullMusicPath, root, strlen(root)) == 0) {
        const char* relativePath = fullMusicPath + strlen(root); 
        if (relativePath[0] == '/' || relativePath[0] == '\\') relativePath++; 
        
        if (strncmp(relativePath, "dados de musica", 15) == 0) return;
        
        char metaFolder[1024];
        sprintf(metaFolder, "%s/dados de musica", root);
        _mkdir(metaFolder); 
        
        char currentPath[1024];
        strcpy(currentPath, metaFolder);
        
        char relCopy[1024];
        strcpy(relCopy, relativePath);
        
        char* ext = strrchr(relCopy, '.');
        if (ext) *ext = '\0'; 
        
        char* token = strtok(relCopy, "/\\");
        while (token != NULL) {
            sprintf(currentPath + strlen(currentPath), "/%s", token);
            _mkdir(currentPath);
            token = strtok(NULL, "/\\");
        }
        
        char txtPath[1024];
        sprintf(txtPath, "%s/dados.txt", currentPath);
        FILE* f = fopen(txtPath, "r");
        if (!f) {
            f = fopen(txtPath, "w");
            if (f) {
                char* lastSlash = strrchr(fullMusicPath, '/');
                if (!lastSlash) lastSlash = strrchr(fullMusicPath, '\\');
                const char* basename = lastSlash ? lastSlash + 1 : fullMusicPath;
                char baseCopy[256];
                strncpy(baseCopy, basename, 255);
                baseCopy[255] = '\0';
                char* bExt = strrchr(baseCopy, '.');
                if (bExt) *bExt = '\0';
                
                fprintf(f, "Nome: %s\nBanda: \nArtista: \nAlbum: \n", baseCopy);
                fclose(f);
            }
        } else {
            fclose(f);
        }
        
        sprintf(txtPath, "%s/letra.txt", currentPath);
        f = fopen(txtPath, "r");
        if (!f) {
            f = fopen(txtPath, "w");
            if (f) {
                fprintf(f, "Letra da musica aqui...\n");
                fclose(f);
            }
        } else {
            fclose(f);
        }
    }
}

void lerPastaMusicas() {
    qtdMusicas = 0;
    
    strcpy(listaMusicasNomes[qtdMusicas], "OPCOES DE AUDIO");
    strcpy(listaMusicasCaminhos[qtdMusicas], "OPCOES");
    listaMusicasEhPasta[qtdMusicas] = false;
    qtdMusicas++;

    strcpy(listaMusicasNomes[qtdMusicas], "Voltar");
    strcpy(listaMusicasCaminhos[qtdMusicas], "VOLTAR");
    listaMusicasEhPasta[qtdMusicas] = false;
    qtdMusicas++;

    DIR* d = opendir(caminhoMusicas);
    if (d) {
        char tempNomes[1000][128];
        char tempPaths[1000][256];
        bool tempEhPasta[1000];
        int count = 0;

        struct dirent* dir;
        while ((dir = readdir(d)) != NULL && count < 998) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

            char fullPath[512];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", caminhoMusicas, dir->d_name);
            struct stat st;
            
            if (stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode)) {
                if (strcmp(dir->d_name, "dados de musica") == 0) continue; 
                snprintf(tempNomes[count], 128, "[%s]", dir->d_name);
                strcpy(tempPaths[count], fullPath);
                tempEhPasta[count] = true;
                count++;
            } else {
                if (strstr(dir->d_name, ".wav") || strstr(dir->d_name, ".WAV") || 
                    strstr(dir->d_name, ".mp3") || strstr(dir->d_name, ".MP3") ||
                    strstr(dir->d_name, ".ogg") || strstr(dir->d_name, ".OGG")) {
                    
                    criarDadosDeMusica(fullPath); 
                    
                    strncpy(tempNomes[count], dir->d_name, 127);
                    tempNomes[count][127] = '\0';
                    strcpy(tempPaths[count], fullPath);
                    tempEhPasta[count] = false;
                    count++;
                }
            }
        }
        closedir(d);

        for (int i = 0; i < count - 1; i++) {
            for (int j = 0; j < count - i - 1; j++) {
                bool trocar = false;
                if (!tempEhPasta[j] && tempEhPasta[j + 1]) trocar = true;
                else if (tempEhPasta[j] == tempEhPasta[j + 1] && strcasecmp(tempNomes[j], tempNomes[j + 1]) > 0) trocar = true;
                
                if (trocar) {
                    char tn[128]; strcpy(tn, tempNomes[j]); strcpy(tempNomes[j], tempNomes[j + 1]); strcpy(tempNomes[j + 1], tn);
                    char tp[256]; strcpy(tp, tempPaths[j]); strcpy(tempPaths[j], tempPaths[j + 1]); strcpy(tempPaths[j + 1], tp);
                    bool tb = tempEhPasta[j]; tempEhPasta[j] = tempEhPasta[j + 1]; tempEhPasta[j + 1] = tb;
                }
            }
        }

        for (int i = 0; i < count; i++) {
            strcpy(listaMusicasNomes[qtdMusicas], tempNomes[i]);
            strcpy(listaMusicasCaminhos[qtdMusicas], tempPaths[i]);
            listaMusicasEhPasta[qtdMusicas] = tempEhPasta[i];
            qtdMusicas++;
        }
    }
}

void tocarMusicaNova(const char* path) {
    if (musicLoaded) {
        UnloadMusicStream(currentMusic);
        musicLoaded = false;
    }
    currentMusic = LoadMusicStream(path);
    if (currentMusic.ctxData != NULL) {
        strcpy(currentMusicPath, path);
        carregarMetadadosMusica(currentMusicPath);
        musicLoaded = true;
        isPlaying = true;
        SetMusicVolume(currentMusic, (float)volumeGeral / 100.0f);
        PlayMusicStream(currentMusic);
        const char* lastSlash = strrchr(path, '/');
        if (!lastSlash) lastSlash = strrchr(path, '\\');
        strncpy(musicTitle, lastSlash ? lastSlash + 1 : path, 255);
        strncpy(musicaAtual, musicTitle, 255);
    } else {
        strcpy(musicTitle, "Erro ao carregar");
        strcpy(musicaAtual, "Erro");
    }
}

void abrirVideoNoApp(const char* path) {
    VideoPlayer_Close();
    if (VideoPlayer_Open(path)) {
        currentState = MENU_VIDEO;
    } else {
        notificar(VideoPlayer_LastError());
    }
}

void abrirImagemNoApp(const char* path) {
    VideoPlayer_Close();
    if (viewerImageLoaded) UnloadTexture(viewerImage);
    viewerImage = LoadTexture(path);
    viewerImageLoaded = true;
    const char* lastSlash = strrchr(path, '/');
    if (!lastSlash) lastSlash = strrchr(path, '\\');
    strncpy(viewerImageTitle, lastSlash ? lastSlash + 1 : path, 255);
    isFullScreenImage = true;
}

void lerPastaImagens() {
    qtdImagens = 0;
    strcpy(listaImagensNomes[qtdImagens], "Voltar");
    strcpy(listaImagensCaminhos[qtdImagens], "VOLTAR");
    listaImagensEhPasta[qtdImagens] = false;
    qtdImagens++;

    DIR* d = opendir(caminhoImagens);
    if (d) {
        char tempNomes[1000][128];
        char tempPaths[1000][256];
        bool tempEhPasta[1000];
        int count = 0;

        struct dirent* dir;
        while ((dir = readdir(d)) != NULL && count < 998) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

            char fullPath[512];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", caminhoImagens, dir->d_name);
            struct stat st;
            
            if (stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode)) {
                snprintf(tempNomes[count], 128, "[%s]", dir->d_name);
                strcpy(tempPaths[count], fullPath);
                tempEhPasta[count] = true;
                count++;
            } else {
                if (strstr(dir->d_name, ".png") || strstr(dir->d_name, ".PNG") || 
                    strstr(dir->d_name, ".jpg") || strstr(dir->d_name, ".JPG") ||
                    strstr(dir->d_name, ".bmp") || strstr(dir->d_name, ".BMP")) {
                    
                    strncpy(tempNomes[count], dir->d_name, 127);
                    tempNomes[count][127] = '\0';
                    strcpy(tempPaths[count], fullPath);
                    tempEhPasta[count] = false;
                    count++;
                }
            }
        }
        closedir(d);

        for (int i = 0; i < count - 1; i++) {
            for (int j = 0; j < count - i - 1; j++) {
                bool trocar = false;
                if (!tempEhPasta[j] && tempEhPasta[j + 1]) trocar = true;
                else if (tempEhPasta[j] == tempEhPasta[j + 1] && strcasecmp(tempNomes[j], tempNomes[j + 1]) > 0) trocar = true;
                
                if (trocar) {
                    char tn[128]; strcpy(tn, tempNomes[j]); strcpy(tempNomes[j], tempNomes[j + 1]); strcpy(tempNomes[j + 1], tn);
                    char tp[256]; strcpy(tp, tempPaths[j]); strcpy(tempPaths[j], tempPaths[j + 1]); strcpy(tempPaths[j + 1], tp);
                    bool tb = tempEhPasta[j]; tempEhPasta[j] = tempEhPasta[j + 1]; tempEhPasta[j + 1] = tb;
                }
            }
        }

        for (int i = 0; i < count; i++) {
            strcpy(listaImagensNomes[qtdImagens], tempNomes[i]);
            strcpy(listaImagensCaminhos[qtdImagens], tempPaths[i]);
            listaImagensEhPasta[qtdImagens] = tempEhPasta[i];
            qtdImagens++;
        }
    }
}

void lerPastaLeitura() {
    qtdLeitura = 0;
    strcpy(listaLeituraNomes[qtdLeitura], "Voltar");
    strcpy(listaLeituraCaminhos[qtdLeitura], "VOLTAR");
    listaLeituraEhPasta[qtdLeitura] = false;
    qtdLeitura++;

    DIR* d = opendir(caminhoLeitura);
    if (d) {
        char tempNomes[1000][128];
        char tempPaths[1000][256];
        bool tempEhPasta[1000];
        int count = 0;

        struct dirent* dir;
        while ((dir = readdir(d)) != NULL && count < 998) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

            char fullPath[512];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", caminhoLeitura, dir->d_name);
            struct stat st;
            
            if (stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode)) {
                snprintf(tempNomes[count], 128, "[%s]", dir->d_name);
                strcpy(tempPaths[count], fullPath);
                tempEhPasta[count] = true;
                count++;
            } else {
                if (strstr(dir->d_name, ".txt") || strstr(dir->d_name, ".TXT") || 
                    strstr(dir->d_name, ".ini") || strstr(dir->d_name, ".INI") ||
                    strstr(dir->d_name, ".json") || strstr(dir->d_name, ".JSON") ||
                    strstr(dir->d_name, ".xml") || strstr(dir->d_name, ".XML") ||
                    strstr(dir->d_name, ".pdf") || strstr(dir->d_name, ".PDF")) {
                    
                    strncpy(tempNomes[count], dir->d_name, 127);
                    tempNomes[count][127] = '\0';
                    strcpy(tempPaths[count], fullPath);
                    tempEhPasta[count] = false;
                    count++;
                }
            }
        }
        closedir(d);

        for (int i = 0; i < count - 1; i++) {
            for (int j = 0; j < count - i - 1; j++) {
                bool trocar = false;
                if (!tempEhPasta[j] && tempEhPasta[j + 1]) trocar = true;
                else if (tempEhPasta[j] == tempEhPasta[j + 1] && strcasecmp(tempNomes[j], tempNomes[j + 1]) > 0) trocar = true;
                
                if (trocar) {
                    char tn[128]; strcpy(tn, tempNomes[j]); strcpy(tempNomes[j], tempNomes[j + 1]); strcpy(tempNomes[j + 1], tn);
                    char tp[256]; strcpy(tp, tempPaths[j]); strcpy(tempPaths[j], tempPaths[j + 1]); strcpy(tempPaths[j + 1], tp);
                    bool tb = tempEhPasta[j]; tempEhPasta[j] = tempEhPasta[j + 1]; tempEhPasta[j + 1] = tb;
                }
            }
        }

        for (int i = 0; i < count; i++) {
            strcpy(listaLeituraNomes[qtdLeitura], tempNomes[i]);
            strcpy(listaLeituraCaminhos[qtdLeitura], tempPaths[i]);
            listaLeituraEhPasta[qtdLeitura] = tempEhPasta[i];
            qtdLeitura++;
        }
    }
}

void lerPastaArquivos() {
    qtdArquivos = 0;
    strcpy(listaArquivosNomes[qtdArquivos], "Voltar"); strcpy(listaArquivosCaminhos[qtdArquivos], "VOLTAR"); listaArquivosEhPasta[qtdArquivos] = false; qtdArquivos++;
    strcpy(listaArquivosNomes[qtdArquivos], "Pesquisar"); strcpy(listaArquivosCaminhos[qtdArquivos], "PESQUISAR"); listaArquivosEhPasta[qtdArquivos] = false; qtdArquivos++;
    strcpy(listaArquivosNomes[qtdArquivos], "Opcoes"); strcpy(listaArquivosCaminhos[qtdArquivos], "OPCOES"); listaArquivosEhPasta[qtdArquivos] = false; qtdArquivos++;

    DIR* d = opendir(caminhoArquivos);
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL && qtdArquivos < 999) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
            char fullPath[512];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", caminhoArquivos, dir->d_name);
            struct stat st;
            if (stat(fullPath, &st) == 0) {
                listaArquivosEhPasta[qtdArquivos] = S_ISDIR(st.st_mode);
                if (listaArquivosEhPasta[qtdArquivos]) {
                    snprintf(listaArquivosNomes[qtdArquivos], 127, "(Pasta) %s", dir->d_name);
                } else {
                    strncpy(listaArquivosNomes[qtdArquivos], dir->d_name, 127);
                }
                listaArquivosNomes[qtdArquivos][127] = '\0';
                strcpy(listaArquivosCaminhos[qtdArquivos], fullPath);
                qtdArquivos++;
            }
        }
        closedir(d);
    }
}

int encontrarIndiceMusicaAtual(const char* caminho) {
    for (int i = 2; i < qtdMusicas; i++) {
        if (!listaMusicasEhPasta[i] && strcmp(listaMusicasCaminhos[i], caminho) == 0) return i;
    }
    return -1;
}

char optExplorarDin[26][64];
const char* ptrExplorar[26];
int qtdExplorarDin = 0;

void initExplorarMenu() {
    qtdExplorarDin = 0;
    strcpy(optExplorarDin[qtdExplorarDin], "Voltar"); ptrExplorar[qtdExplorarDin] = optExplorarDin[qtdExplorarDin]; qtdExplorarDin++;
    strcpy(optExplorarDin[qtdExplorarDin], "Raiz do Player"); ptrExplorar[qtdExplorarDin] = optExplorarDin[qtdExplorarDin]; qtdExplorarDin++;
    strcpy(optExplorarDin[qtdExplorarDin], "Downloads"); ptrExplorar[qtdExplorarDin] = optExplorarDin[qtdExplorarDin]; qtdExplorarDin++;
    strcpy(optExplorarDin[qtdExplorarDin], "Documentos"); ptrExplorar[qtdExplorarDin] = optExplorarDin[qtdExplorarDin]; qtdExplorarDin++;

    for (int i = 0; i < 26; i++) {
        char drivePath[8];
        sprintf(drivePath, "%c:/", 'A' + i);
        DIR* d = opendir(drivePath);
        if (d) {
            closedir(d);
            sprintf(optExplorarDin[qtdExplorarDin], "Disco %c:\\", 'A' + i);
            ptrExplorar[qtdExplorarDin] = optExplorarDin[qtdExplorarDin];
            qtdExplorarDin++;
        }
    }
}

int main(void)
{
    inicializarPastas();

    const int screenWidth = 800;
    const int screenHeight = 600;

    SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(screenWidth, screenHeight, "Media Player Interface");
    InitAudioDevice();
    SetAudioStreamBufferSizeDefault(4096);

    Color bgDark = (Color){ 15, 15, 15, 255 };
    Color panelDark = (Color){ 30, 30, 30, 255 };
    Color btnNormal = (Color){ 50, 50, 50, 255 };
    Color btnHover = (Color){ 75, 75, 75, 255 };
    Color textLight = (Color){ 240, 240, 240, 255 };
    Color scrollBarColor = (Color){ 60, 60, 60, 255 };
    Color scrollThumbColor = (Color){ 120, 120, 120, 255 };
    Color accentColor = (Color){ 0, 121, 241, 255 };

    currentState = MENU_MAIN;
    scrollY = 0;

    baixar_init();
    ftp_init();
    tmdb_init();
    torrent_init();
    midia_init();

    int modoReproducao = 0;
    float speed = 1.0f;

    Texture2D radioLogo = { 0 };
    bool radioLogoLoaded = false;
    
    char* viewerText = NULL;
    int viewerTextScrollY = 0;
    char viewerTextTitle[256] = "Nenhum arquivo de texto aberto.";
    bool isFullScreenText = false;
    
    radio_init();
    AudioStream radioStream = LoadAudioStream(44100, 16, 2);
    PlayAudioStream(radioStream);
    
    AudioStream instStream = LoadAudioStream(48000, 16, 2);

    // Load background image if exists
    Texture2D bgTexture = { 0 };
    if (FileExists("data/HyperNeiva/configuracao/background.png")) {
        bgTexture = LoadTexture("data/HyperNeiva/configuracao/background.png");
        bgImageLoaded = (bgTexture.id != 0);
        strcpy(bgImagePath, "data/HyperNeiva/configuracao/background.png");
    } else if (FileExists("data/HyperNeiva/configuracao/background.jpg")) {
        bgTexture = LoadTexture("data/HyperNeiva/configuracao/background.jpg");
        bgImageLoaded = (bgTexture.id != 0);
        strcpy(bgImagePath, "data/HyperNeiva/configuracao/background.jpg");
    }
    bool bgNeedsReloadLocal = false; (void)bgNeedsReloadLocal;

    while (!WindowShouldClose())
    {
        static int lastHoveredRadio = -1;
        static char globalSearchQuery[64] = "";
        static int globalSearchCount = 0;
        static MenuState previousStateForSearch = MENU_MAIN;
        static int searchTargetMode = 0; // 0=Local, 1=RadioName, 2=RadioTag, 3=RadioCountry, 4=RadioState, 5=RadioLanguage
        
        Vector2 mousePoint = GetMousePosition();
        float wheelMove = GetMouseWheelMove();

        if (IsFileDropped()) {
            FilePathList droppedFiles = LoadDroppedFiles();
            if (droppedFiles.count > 0) {
                const char* filepath = droppedFiles.paths[0];
                char ext[16] = "";
                const char* dot = strrchr(filepath, '.');
                if (dot) {
                    strncpy(ext, dot, sizeof(ext) - 1);
                    ext[sizeof(ext) - 1] = '\0';
                    for (int i = 0; ext[i]; i++) ext[i] = tolower(ext[i]);
                }

                if (strcmp(ext, ".txt") == 0 || strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0 ||
                    strcmp(ext, ".json") == 0 || strcmp(ext, ".py") == 0 || strcmp(ext, ".md") == 0) {
                    VideoPlayer_Close();
                    abrirTextoNoBlocoNotas(filepath);
                    currentState = MENU_FERRAMENTAS;
                }
                else if (strcmp(ext, ".png") == 0 || strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0 ||
                         strcmp(ext, ".bmp") == 0) {
                    VideoPlayer_Close();
                    if (viewerImageLoaded) UnloadTexture(viewerImage);
                    viewerImage = LoadTexture(filepath);
                    viewerImageLoaded = true;
                    strncpy(viewerImageTitle, GetFileName(filepath), 255);
                    isFullScreenImage = true;
                    currentState = MENU_MAIN;
                }
                else if (strcmp(ext, ".mp4") == 0 || strcmp(ext, ".avi") == 0 || strcmp(ext, ".mkv") == 0 ||
                         strcmp(ext, ".mov") == 0 || strcmp(ext, ".webm") == 0) {
                    VideoPlayer_Close();
                    if (VideoPlayer_Open(filepath)) {
                        currentState = MENU_VIDEO;
                    } else {
                        notificar(VideoPlayer_LastError());
                    }
                }
            }
            UnloadDroppedFiles(droppedFiles);
        }
        static float holdTime = 0.0f;
        static int pressedItemIndex = -1;
        static bool longPressTriggered = false;

        bool clicked = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
        if (longPressTriggered && clicked) {
            clicked = false;
            longPressTriggered = false;
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            pressedItemIndex = -1;
            longPressTriggered = false;
        }
        bool holding = IsMouseButtonDown(MOUSE_LEFT_BUTTON);

        bool doubleClicked = false;
        if (clicked) {
            if (GetTime() - lastImageClickTime < 0.4) {
                doubleClicked = true;
            }
            lastImageClickTime = GetTime();
        }

        if (isFullScreenImage || isFullScreenText) {
            Rectangle closeBtn = { screenWidth - 50, 10, 40, 40 };
            if (CheckCollisionPointRec(mousePoint, closeBtn) && clicked) {
                isFullScreenImage = false;
                isFullScreenText = false;
            }
            clicked = false;
            holding = false;
            if (isFullScreenImage) wheelMove = 0.0f;
        }

        if (musicLoaded && isPlaying && !isFullScreenImage && !isFullScreenText) {
            UpdateMusicStream(currentMusic);
            
            if (GetMusicTimePlayed(currentMusic) >= GetMusicTimeLength(currentMusic) - 0.1f) {
                if (modoReproducao == 1) { 
                    SeekMusicStream(currentMusic, 0.0f);
                } else { 
                    int idx = encontrarIndiceMusicaAtual(currentMusicPath);
                    if (idx != -1) {
                        int nextIdx = idx + 1;
                        bool found = false;
                        while (nextIdx < qtdMusicas) {
                            if (!listaMusicasEhPasta[nextIdx]) { found = true; break; }
                            nextIdx++;
                        }
                        if (found) {
                            UnloadMusicStream(currentMusic);
                            currentMusic = LoadMusicStream(listaMusicasCaminhos[nextIdx]);
                            strcpy(currentMusicPath, listaMusicasCaminhos[nextIdx]);
                            carregarMetadadosMusica(currentMusicPath);
                            strncpy(musicTitle, GetFileName(listaMusicasCaminhos[nextIdx]), 255);
                            SetMusicVolume(currentMusic, volumeGeral / 100.0f);
                            PlayMusicStream(currentMusic);
                        } else {
                            StopMusicStream(currentMusic);
                            isPlaying = false;
                        }
                    } else {
                        StopMusicStream(currentMusic);
                        isPlaying = false;
                    }
                }
            }
        }
        
        if (radio_is_playing()) {
            SetAudioStreamVolume(radioStream, (float)volumeGeral / 100.0f);
            
            if (IsAudioStreamProcessed(radioStream)) {
                int16_t* pcm = (int16_t*)malloc(4096 * 4);
                int readFrames = radio_read_pcm(pcm, 4096);
                
                if (readFrames == 0) {
                    memset(pcm, 0, 4096 * 4);
                    readFrames = 4096;
                }
                
                UpdateAudioStream(radioStream, pcm, readFrames);
                free(pcm);
            }
        }

        if (musicLoaded) {
            audioTempoAtual = GetMusicTimePlayed(currentMusic);
            strncpy(musicaAtual, musicTitle, 255);
        } else {
            audioTempoAtual = 0.0f;
            strcpy(musicaAtual, "PARADO");
        }

        if (currentState == MENU_INSTRUMENTOS) {
            if (!IsAudioStreamPlaying(instStream)) PlayAudioStream(instStream);
            SetAudioStreamVolume(instStream, (float)volumeGeral / 100.0f);
            if (IsAudioStreamProcessed(instStream)) {
                int16_t pcm[4096 * 2];
                memset(pcm, 0, sizeof(pcm));
                misturarAudioPiano(pcm, 4096);
                UpdateAudioStream(instStream, pcm, 4096);
            }
        } else {
            if (IsAudioStreamPlaying(instStream)) StopAudioStream(instStream);
        }

        if (currentState == MENU_VIDEO) {
            VideoPlayer_Update();
            if (!VideoPlayer_IsPlaying() || VideoPlayer_WantsClose()) {
                VideoPlayer_Close();
                if (VideoPlayer_HouveErro()) notificar(VideoPlayer_LastError());
                currentState = MENU_MAIN;
            }
        }
        
        BeginDrawing();
        ClearBackground(bgDark);

        if (currentState == MENU_MAIN) {
            static char topSearchBuf[256] = "";
            static bool topSearchFocused = false;
            static bool topMicActive = false;
            static bool micThreadRunning = false;
            static char micTranscribed[256] = "";
            static pthread_t micThread;
            static char topStatusMsg[512] = "Assistente IA pronto. Digite ou clique no microfone.";

            int barX = 50, barY = 12, barW = 350, barH = 40;
            Rectangle barRect = { (float)barX, (float)barY, (float)barW, (float)barH };
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                topSearchFocused = CheckCollisionPointRec(mousePoint, barRect);
            }
            if (topSearchFocused) {
                int key = GetCharPressed();
                while (key > 0) {
                    if ((key >= 32) && (key <= 126)) {
                        int len = strlen(topSearchBuf);
                        if (len < (int)sizeof(topSearchBuf) - 1) {
                            topSearchBuf[len] = (char)key;
                            topSearchBuf[len + 1] = 0;
                        }
                    }
                    key = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    int len = strlen(topSearchBuf);
                    if (len > 0) topSearchBuf[len - 1] = 0;
                }
            }

            DrawRectangleRounded(barRect, 0.3f, 8, (Color){ 20, 22, 35, 235 });
            DrawRectangleRoundedLines(barRect, 0.3f, 8, topSearchFocused ? (Color){ 0, 200, 255, 255 } : (Color){ 60, 80, 110, 255 });
            
            // Search input field (compact, left side)
            Rectangle searchRect = { (float)(barX + 10), (float)(barY + 5), 150, 34 };
            bool searchHover = CheckCollisionPointRec(mousePoint, searchRect);
            DrawRectangleRounded(searchRect, 0.5f, 6, searchHover ? (Color){ 60, 110, 160, 255 } : (Color){ 35, 55, 80, 255 });
            DrawRectangleRoundedLines(searchRect, 0.5f, 6, (Color){ 80, 140, 200, 255 });
            if (topSearchFocused) {
                DrawText(topSearchBuf, (int)searchRect.x + 5, (int)searchRect.y + 10, 14, WHITE);
            } else {
                DrawText("Pesquisar / Comando...", (int)searchRect.x + 5, (int)searchRect.y + 10, 14, GRAY);
            }

            // MIC Button
            Rectangle micRect = { (float)(searchRect.x + searchRect.width + 5), (float)(barY + 5), 56, 34 };
            bool micHov = CheckCollisionPointRec(mousePoint, micRect);
            if (micHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !topMicActive) {
                topMicActive = true;
                grav_start_recording("data/HyperNeiva/top_mic.raw");
                strcpy(topStatusMsg, "Gravando audio... Fale agora.");
            }
            if (topMicActive && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                topMicActive = false;
                grav_stop_recording();
                if (grav_finish_recording("data/HyperNeiva/top_mic.raw", "data/HyperNeiva/top_mic.wav")) {
                    char apiKey[512] = "";
                    gemini_obter_chave(apiKey, sizeof(apiKey));
                    if (apiKey[0]) {
                        strcpy(topStatusMsg, "Transcrevendo audio com IA...");
                        char trans[256] = "";
                        if (gemini_audio_para_texto(apiKey, "data/HyperNeiva/top_mic.wav", "Transcreva exatamente o que foi dito.", trans, sizeof(trans))) {
                            if (trans[0]) {
                                strncpy(topSearchBuf, trans, sizeof(topSearchBuf) - 1);
                                goto process_query;
                            }
                        } else {
                            strcpy(topStatusMsg, "Falha na transcricao do audio.");
                        }
                    } else {
                        strcpy(topStatusMsg, "Chave Gemini nao configurada.");
                    }
                }
            }
            
            DrawRectangleRounded(micRect, 0.5f, 6, topMicActive ? (Color){ 220, 50, 50, 255 } : (micHov ? (Color){ 60, 110, 160, 255 } : (Color){ 35, 55, 80, 255 }));
            DrawRectangleRoundedLines(micRect, 0.5f, 6, topMicActive ? (Color){ 255, 100, 100, 255 } : (Color){ 80, 140, 200, 255 });
            DrawText(topMicActive ? "REC" : "🎙 MIC", (int)(micRect.x + 8), (int)(micRect.y + 10), 14, WHITE);

            // OK Button
            Rectangle sendRect = { (float)(micRect.x + micRect.width + 5), (float)(barY + 5), 58, 34 };
            bool sendHov = CheckCollisionPointRec(mousePoint, sendRect);
            DrawRectangleRounded(sendRect, 0.5f, 6, sendHov ? (Color){ 0, 180, 130, 255 } : (Color){ 0, 120, 80, 255 });
            DrawRectangleRoundedLines(sendRect, 0.5f, 6, (Color){ 0, 220, 160, 255 });
            DrawText("OK", (int)(sendRect.x + 18), (int)(sendRect.y + 10), 14, WHITE);

            bool submit = (sendHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) || (topSearchFocused && IsKeyPressed(KEY_ENTER));
            if (submit && topSearchBuf[0] != 0) {
                goto process_query;
            }
            goto skip_query;

            process_query: {
                char q[256];
                strcpy(q, topSearchBuf);
                char qLower[256];
                for (int i = 0; q[i] && i < 255; i++) qLower[i] = tolower(q[i]);
                qLower[sizeof(qLower)-1] = 0;

                if (strstr(qLower, "ferramenta") || strstr(qLower, "ferramentas")) {
                    initFerramentas();
                    currentState = MENU_FERRAMENTAS;
                    strcpy(topStatusMsg, "Abrindo Ferramentas.");
                } else if (strstr(qLower, "jogar") || strstr(qLower, "jogos")) {
                    currentState = MENU_JOGAR;
                    strcpy(topStatusMsg, "Indo para Jogos.");
                } else if (strstr(qLower, "midia") || strstr(qLower, "musica") || strstr(qLower, "musicas")) {
                    currentState = MENU_MIDIA;
                    strcpy(topStatusMsg, "Indo para Mídia.");
                } else if (strstr(qLower, "baixar") || strstr(qLower, "download")) {
                    currentState = MENU_BAIXAR;
                    strcpy(topStatusMsg, "Indo para Baixar.");
                } else if (strstr(qLower, "config") || strstr(qLower, "chave")) {
                    currentState = MENU_CONFIGURACOES;
                    strcpy(topStatusMsg, "Indo para Configurações.");
                } else if (strstr(qLower, "cmd") || strstr(qLower, "prompt") || strstr(qLower, "terminal")) {
                    system("start cmd");
                    strcpy(topStatusMsg, "Abrindo CMD do PC.");
                } else if (strstr(qLower, "computador") || strstr(qLower, "meu computador") || strstr(qLower, "explorador")) {
                    system("explorer");
                    strcpy(topStatusMsg, "Abrindo Explorador de Arquivos / Meu Computador.");
                } else if (strstr(qLower, "bloco") || strstr(qLower, "notepad")) {
                    system("start notepad");
                    strcpy(topStatusMsg, "Abrindo Bloco de Notas.");
                } else {
                    char apiKey[512] = "";
                    gemini_obter_chave(apiKey, sizeof(apiKey));
                    if (apiKey[0]) {
                        strcpy(topStatusMsg, "Consultando Gemini AI...");
                        char aiAns[2048] = "";
                        if (gemini_gerar_texto(apiKey, "gemini-3.5-flash", q, NULL, 0, aiAns, sizeof(aiAns))) {
                            if (aiAns[0]) {
                                if (gemini_texto_para_audio(apiKey, aiAns, "en-US-Journey-F", "data/HyperNeiva/top_resp.wav")) {
                                    system("start data/HyperNeiva/top_resp.wav");
                                }
                                strncpy(topStatusMsg, aiAns, sizeof(topStatusMsg) - 1);
                            }
                        } else {
                            strcpy(topStatusMsg, "Erro na resposta da IA Gemini.");
                        }
                    } else {
                        strcpy(topStatusMsg, "Chave Gemini nao configurada nas Chaves API.");
                    }
                }
            }
            skip_query:;

            // Draw status at bottom of screen
            DrawRectangle(50, screenHeight - 34, 350, 26, (Color){ 10, 12, 20, 220 });
            DrawRectangleLines(50, screenHeight - 34, 350, 26, (Color){ 40, 70, 100, 200 });
            DrawText(topStatusMsg, 60, screenHeight - 30, 13, (Color){ 0, 220, 255, 255 });
        }
        
        // Reload background if changed
        if (bgNeedsReload) {
            if (bgTexture.id != 0) UnloadTexture(bgTexture);
            bgTexture = LoadTexture(bgImagePath);
            bgImageLoaded = (bgTexture.id != 0);
            bgNeedsReload = false;
        }
        if (bgImageLoaded && bgTexture.id != 0) {
            float scaleX = (float)screenWidth / bgTexture.width;
            float scaleY = (float)screenHeight / bgTexture.height;
            float scale = (scaleX > scaleY) ? scaleX : scaleY;
            int dw = (int)(bgTexture.width * scale);
            int dh = (int)(bgTexture.height * scale);
            int dx = (screenWidth - dw) / 2;
            int dy = (screenHeight - dh) / 2;
            DrawTextureEx(bgTexture, (Vector2){ dx, dy }, 0.0f, scale, (Color){ 255, 255, 255, 200 });
        }
        
        if (currentState == MENU_MIDIA_MUSICAS || currentState == MENU_AUDIO_OPCOES || currentState == MENU_MAIN || currentState == MENU_MIDIA) {
            DrawText("PLAYER DE MUSICA", 50, 50, 24, textLight);
            DrawText(musicTitle, 50, 100, 16, GRAY);

            if (musicLoaded) {
                float timePlayed = GetMusicTimePlayed(currentMusic);
                float timeLength = GetMusicTimeLength(currentMusic);
                
                Rectangle progBarBg = { 50, 150, 350, 15 };
                
                if (CheckCollisionPointRec(mousePoint, progBarBg) && holding) {
                    float clickedX = mousePoint.x - progBarBg.x;
                    float proportion = clickedX / progBarBg.width;
                    if (proportion < 0.0f) proportion = 0.0f;
                    if (proportion > 1.0f) proportion = 1.0f;
                    SeekMusicStream(currentMusic, timeLength * proportion);
                }
                
                DrawRectangleRec(progBarBg, scrollBarColor);
                
                float progWidth = 350 * (timePlayed / timeLength);
                if (progWidth < 0) progWidth = 0;
                if (progWidth > 350) progWidth = 350;
                
                Rectangle progBar = { 50, 150, progWidth, 15 };
                DrawRectangleRec(progBar, accentColor);
                
                char timeText[32];
                sprintf(timeText, "%02d:%02d / %02d:%02d", 
                    (int)timePlayed / 60, (int)timePlayed % 60, 
                    (int)timeLength / 60, (int)timeLength % 60);
                DrawText(timeText, 50, 175, 14, GRAY);
                
                DrawText(isPlaying ? "STATUS: REPRODUZINDO" : "STATUS: PAUSADO", 50, 205, 14, accentColor);

                Rectangle btnPrev = { 50, 230, 80, 30 };
                bool hPrev = CheckCollisionPointRec(mousePoint, btnPrev);
                if (hPrev && clicked) {
                    int idx = encontrarIndiceMusicaAtual(currentMusicPath);
                    if (idx != -1) {
                        int prevIdx = idx - 1; bool found = false;
                        while (prevIdx >= 2) { if (!listaMusicasEhPasta[prevIdx]) { found = true; break; } prevIdx--; }
                        if (found) {
                            UnloadMusicStream(currentMusic); currentMusic = LoadMusicStream(listaMusicasCaminhos[prevIdx]);
                            strcpy(currentMusicPath, listaMusicasCaminhos[prevIdx]); isPlaying = true;
                            carregarMetadadosMusica(currentMusicPath);
                            SetMusicVolume(currentMusic, volumeGeral / 100.0f); SetMusicPitch(currentMusic, speed); PlayMusicStream(currentMusic);
                            strncpy(musicTitle, GetFileName(listaMusicasCaminhos[prevIdx]), 255);
                        }
                    }
                }
                DrawRectangleRec(btnPrev, hPrev ? btnHover : btnNormal);
                DrawText("<< ANT", 65, 238, 14, textLight);

                Rectangle btnPlay = { 140, 230, 80, 30 };
                bool hPlay = CheckCollisionPointRec(mousePoint, btnPlay);
                if (hPlay && clicked) {
                    isPlaying = !isPlaying;
                    if (isPlaying) ResumeMusicStream(currentMusic);
                    else PauseMusicStream(currentMusic);
                }
                DrawRectangleRec(btnPlay, hPlay ? btnHover : btnNormal);
                DrawText(isPlaying ? "PAUSE" : "PLAY", 155, 238, 14, textLight);

                Rectangle btnStop = { 230, 230, 60, 30 };
                bool hStop = CheckCollisionPointRec(mousePoint, btnStop);
                if (hStop && clicked) { StopMusicStream(currentMusic); isPlaying = false; strcpy(musicTitle, "PARADO"); }
                DrawRectangleRec(btnStop, hStop ? btnHover : btnNormal);
                DrawText("STOP", 242, 238, 14, textLight);

                Rectangle btnNext = { 300, 230, 80, 30 };
                bool hNext = CheckCollisionPointRec(mousePoint, btnNext);
                if (hNext && clicked) {
                    int idx = encontrarIndiceMusicaAtual(currentMusicPath);
                    if (idx != -1) {
                        int nextIdx = idx + 1; bool found = false;
                        while (nextIdx < qtdMusicas) { if (!listaMusicasEhPasta[nextIdx]) { found = true; break; } nextIdx++; }
                        if (found) {
                            UnloadMusicStream(currentMusic); currentMusic = LoadMusicStream(listaMusicasCaminhos[nextIdx]);
                            strcpy(currentMusicPath, listaMusicasCaminhos[nextIdx]); isPlaying = true;
                            carregarMetadadosMusica(currentMusicPath);
                            SetMusicVolume(currentMusic, volumeGeral / 100.0f); SetMusicPitch(currentMusic, speed); PlayMusicStream(currentMusic);
                            strncpy(musicTitle, GetFileName(listaMusicasCaminhos[nextIdx]), 255);
                        }
                    }
                }
                DrawRectangleRec(btnNext, hNext ? btnHover : btnNormal);
                DrawText("PROX >>", 310, 238, 14, textLight);

                Rectangle btnBack10 = { 50, 270, 70, 30 };
                bool hBack10 = CheckCollisionPointRec(mousePoint, btnBack10);
                if (hBack10 && clicked) {
                    float t = GetMusicTimePlayed(currentMusic) - 10.0f; if (t < 0.0f) t = 0.0f; SeekMusicStream(currentMusic, t);
                }
                DrawRectangleRec(btnBack10, hBack10 ? btnHover : btnNormal);
                DrawText("- 10s", 65, 278, 14, textLight);

                Rectangle btnFwd10 = { 130, 270, 70, 30 };
                bool hFwd10 = CheckCollisionPointRec(mousePoint, btnFwd10);
                if (hFwd10 && clicked) {
                    float t = GetMusicTimePlayed(currentMusic) + 10.0f; if (t > GetMusicTimeLength(currentMusic)) t = GetMusicTimeLength(currentMusic) - 0.1f; SeekMusicStream(currentMusic, t);
                }
                DrawRectangleRec(btnFwd10, hFwd10 ? btnHover : btnNormal);
                DrawText("+ 10s", 145, 278, 14, textLight);

                Rectangle btnSpeed = { 210, 270, 100, 30 };
                bool hSpeed = CheckCollisionPointRec(mousePoint, btnSpeed);
                if (hSpeed && clicked) {
                    speed += 0.5f; if (speed > 2.0f) speed = 0.5f;
                    SetMusicPitch(currentMusic, speed);
                }
                DrawRectangleRec(btnSpeed, hSpeed ? btnHover : btnNormal);
                char spdText[32]; sprintf(spdText, "VEL: %.1fx", speed);
                DrawText(spdText, 220, 278, 14, textLight);

                Rectangle btnPrevDir = { 50, 310, 90, 30 };
                bool hPrevDir = CheckCollisionPointRec(mousePoint, btnPrevDir);
                if (hPrevDir && clicked) {
                    int idx = encontrarIndiceMusicaAtual(currentMusicPath); 
                    if (idx == -1) idx = 1;
                    int prevIdx = idx - 1; bool found = false;
                    while (prevIdx >= 2) { if (listaMusicasEhPasta[prevIdx]) { found = true; break; } prevIdx--; }
                    if (!found) { 
                        prevIdx = qtdMusicas - 1;
                        while (prevIdx > idx) { if (listaMusicasEhPasta[prevIdx]) { found = true; break; } prevIdx--; }
                    }
                    if (found) {
                        strcpy(caminhoMusicas, listaMusicasCaminhos[prevIdx]);
                        lerPastaMusicas(); scrollY = 0; currentState = MENU_MIDIA_MUSICAS;
                    }
                }
                DrawRectangleRec(btnPrevDir, hPrevDir ? btnHover : btnNormal);
                DrawText("<< PASTA", 60, 318, 14, textLight);

                Rectangle btnNextDir = { 150, 310, 90, 30 };
                bool hNextDir = CheckCollisionPointRec(mousePoint, btnNextDir);
                if (hNextDir && clicked) {
                    int idx = encontrarIndiceMusicaAtual(currentMusicPath); 
                    if (idx == -1) idx = 1; 
                    int nextIdx = idx + 1; bool found = false;
                    while (nextIdx < qtdMusicas) { if (listaMusicasEhPasta[nextIdx]) { found = true; break; } nextIdx++; }
                    if (!found) { 
                        nextIdx = 2;
                        while (nextIdx < idx) { if (listaMusicasEhPasta[nextIdx]) { found = true; break; } nextIdx++; }
                    }
                    if (found) {
                        strcpy(caminhoMusicas, listaMusicasCaminhos[nextIdx]);
                        lerPastaMusicas(); scrollY = 0; currentState = MENU_MIDIA_MUSICAS;
                    }
                }
                DrawRectangleRec(btnNextDir, hNextDir ? btnHover : btnNormal);
                DrawText("PASTA >>", 160, 318, 14, textLight);

                DrawText("Vol:", 250, 318, 16, textLight);
                Rectangle volTrack = { 290, 320, 110, 10 };
                DrawRectangleRec(volTrack, scrollBarColor);
                Rectangle volThumb = { 290, 315, 110 * (volumeGeral / 100.0f), 20 };
                DrawRectangleRec(volThumb, accentColor);
                if (CheckCollisionPointRec(mousePoint, volTrack) && holding) {
                    float volNorm = (mousePoint.x - volTrack.x) / volTrack.width;
                    if (volNorm < 0.0f) volNorm = 0.0f; if (volNorm > 1.0f) volNorm = 1.0f;
                    volumeGeral = (int)(volNorm * 100); SetMusicVolume(currentMusic, volNorm);
                }
                
                if (coverCount > 0) {
                    for (int i = 0; i < coverCount; i++) {
                        float scale = 1.0f;
                        if (coverTextures[i].width > 120 || coverTextures[i].height > 120) {
                            float scaleX = 120.0f / coverTextures[i].width;
                            float scaleY = 120.0f / coverTextures[i].height;
                            scale = (scaleX < scaleY) ? scaleX : scaleY;
                        }
                        float xPos = 50 + (float)(i * 130);
                        DrawTextureEx(coverTextures[i], (Vector2){ xPos, 360 }, 0.0f, scale, WHITE);
                        
                        char dispText[128];
                        strncpy(dispText, coverNames[i], 127); dispText[127] = '\0';
                        if (strlen(dispText) > 16) {
                            dispText[13] = '.'; dispText[14] = '.'; dispText[15] = '.'; dispText[16] = '\0';
                        }
                        DrawText(dispText, (int)xPos, 485, 12, textLight);
                    }
                    DrawText(dadosText, 50, 505, 14, GRAY);
                } else {
                    DrawText(dadosText, 50, 360, 14, GRAY);
                }
            }
        } else if (currentState == MENU_MIDIA_IMAGENS) {
            DrawText("VISUALIZADOR DE IMAGENS", 50, 50, 24, textLight);
            DrawText(viewerImageTitle, 50, 100, 16, GRAY);
            if (viewerImageLoaded) {
                char resText[64];
                sprintf(resText, "Resolucao: %dx%d", viewerImage.width, viewerImage.height);
                DrawText(resText, 50, 125, 14, GRAY);
                
                float scale = 1.0f;
                if (viewerImage.width > 380 || viewerImage.height > 400) {
                    float scaleX = 380.0f / viewerImage.width;
                    float scaleY = 400.0f / viewerImage.height;
                    scale = (scaleX < scaleY) ? scaleX : scaleY;
                }
                
                Rectangle imgBounds = { 50, 150, viewerImage.width * scale, viewerImage.height * scale };
                DrawTextureEx(viewerImage, (Vector2){ 50, 150 }, 0.0f, scale, WHITE);
                
                if (CheckCollisionPointRec(mousePoint, imgBounds) && doubleClicked) {
                    isFullScreenImage = true;
                }
            } else {
                DrawText("Nenhuma imagem selecionada.", 50, 150, 16, GRAY);
            }
        } else if (currentState == MENU_MIDIA_LEITURA) {
            DrawText("LEITOR DE TEXTOS / PDF", 50, 50, 24, textLight);
            DrawText(viewerTextTitle, 50, 100, 16, GRAY);
            
            if (pdfLoaded) {
                char resText[64];
                sprintf(resText, "Pagina: %d / %d", pdfPaginaAtual, pdfTotalPaginas);
                DrawText(resText, 50, 125, 14, GRAY);
                
                float scale = 1.0f;
                if (pdfTexture.width > 380 || pdfTexture.height > 400) {
                    float scaleX = 380.0f / pdfTexture.width;
                    float scaleY = 400.0f / pdfTexture.height;
                    scale = (scaleX < scaleY) ? scaleX : scaleY;
                }
                
                Rectangle pdfBounds = { 50, 150, pdfTexture.width * scale, pdfTexture.height * scale };
                DrawTextureEx(pdfTexture, (Vector2){ 50, 150 }, 0.0f, scale, WHITE);
                
                if (CheckCollisionPointRec(mousePoint, pdfBounds) && doubleClicked) {
                    isFullScreenText = true;
                }
                
                Rectangle btnPrevPdf = { 50, 560, 100, 30 };
                bool hPrevPdf = CheckCollisionPointRec(mousePoint, btnPrevPdf);
                DrawRectangleRec(btnPrevPdf, hPrevPdf ? btnHover : btnNormal);
                DrawText("<< PAGINA", 60, 568, 14, textLight);
                if (hPrevPdf && clicked && pdfPaginaAtual > 1) {
                    carregarPaginaPDF(pdfPaginaAtual - 1);
                }
                
                Rectangle btnNextPdf = { 160, 560, 100, 30 };
                bool hNextPdf = CheckCollisionPointRec(mousePoint, btnNextPdf);
                DrawRectangleRec(btnNextPdf, hNextPdf ? btnHover : btnNormal);
                DrawText("PAGINA >>", 170, 568, 14, textLight);
                if (hNextPdf && clicked && pdfPaginaAtual < pdfTotalPaginas) {
                    carregarPaginaPDF(pdfPaginaAtual + 1);
                }
            } else if (viewerText) {
                char resText[64];
                sprintf(resText, "Caracteres: %zu", strlen(viewerText));
                DrawText(resText, 50, 125, 14, GRAY);
                
                Rectangle textBounds = { 50, 140, 390, 440 };
                if (CheckCollisionPointRec(mousePoint, textBounds) && doubleClicked) {
                    isFullScreenText = true;
                }
                
                if (mousePoint.x < 450 && wheelMove != 0 && !isFullScreenText) {
                    viewerTextScrollY += (int)(wheelMove * 40);
                    if (viewerTextScrollY > 0) viewerTextScrollY = 0;
                }
                BeginScissorMode(50, 140, 390, 440);
                DrawText(viewerText, 50, 150 + viewerTextScrollY, 14, textLight);
                EndScissorMode();
            } else {
                DrawText("Nenhum arquivo aberto.", 50, 150, 16, GRAY);
            }
        } else if (currentState == MENU_MIDIA_ARQUIVOS) {
            DrawText("EXPLORADOR DE ARQUIVOS", 50, 50, 24, textLight);
            DrawText("Arquivos nao suportados abrem no Windows", 50, 100, 16, GRAY);
        } else if (currentState == MENU_MIDIA_RADIO_LISTA || currentState == MENU_MIDIA_RADIO_CATEGORIA) {
            DrawRectangle(50, 50, 380, 130, panelDark);
            DrawText("Radio Web", 60, 60, 24, textLight);
            
            int previewIdx = lastHoveredRadio;
            
            if (previewIdx != -1 || radio_is_playing() || radio_is_buffering()) {
                char rStatus[128] = "Previa";
                if (previewIdx == -1) radio_get_status(rStatus);
                DrawText(rStatus, 200, 60, 20, GREEN);
                
                char rTags[256] = {0};
                char rCodec[32] = {0};
                int rBitrate = 0;
                
                if (previewIdx != -1) {
                    strcpy(rTags, radioTags[previewIdx]);
                    strcpy(rCodec, radioCodecs[previewIdx]);
                    rBitrate = radioBitrates[previewIdx];
                } else {
                    radio_get_metadata(rTags, rCodec, &rBitrate);
                }
                
                char rInfo[512];
                if (rBitrate > 0) {
                    sprintf(rInfo, "%s\n%s | %d kbps", rTags, rCodec, rBitrate);
                } else {
                    sprintf(rInfo, "%s\n%s", rTags, rCodec);
                }
                DrawText(rInfo, 200, 90, 14, GRAY);
                
                if (previewIdx == -1 && radioLogoLoaded) {
                    Rectangle dest = { 70, 90, 80, 80 };
                    DrawTexturePro(radioLogo, (Rectangle){0, 0, radioLogo.width, radioLogo.height}, dest, (Vector2){0,0}, 0.0f, WHITE);
                } else {
                    DrawCircle(110, 130, 40, (Color){ 20, 20, 20, 255 });
                    DrawCircle(110, 130, 15, accentColor);
                    DrawCircle(110, 130, 5, BLACK);
                }
            }
            Rectangle btnStopRadio = { 50, 150, 200, 40 };
            bool hStopRadio = CheckCollisionPointRec(mousePoint, btnStopRadio);
            if (hStopRadio && clicked) radio_stop();
            DrawRectangleRec(btnStopRadio, hStopRadio ? btnHover : btnNormal);
            DrawText("Parar Rádio", 70, 160, 16, textLight);
        }

        DrawRectangle(450, 0, 350, screenHeight, panelDark);

        const char** opcoes = NULL;
        int qtd = 0;
        int espacamento = 60;
        int alturaBtn = 45;

        const char* optMain[] = { "JOGAR", "MIDIA", "BAIXAR", "EDITAR", "EXPLORAR", "EXTRA", "FERRAMENTAS", "CONFIGURACOES", "INFORMACOES" };
        const char* optJogar[] = { "Voltar", "PS4 NATIVO", "JOGOS XML" };
        const char* optMidia[] = { "Voltar", "Radio & Podcast", "musicas", "videos", "imagens", "audios", "leitura", "arquivos" };
        const char* optMidiaVideos[] = { "Voltar", "Filmes", "Series", "Animes", "Outros", "Video" };
        const char* optBaixar[] = { "Voltar", "Repositorios", "LINK DIRETO", "Lojas", "FTP (PC / Android)", "Nuvem", "Midia" };

        static const char* optBaixarDyn[4000];
        static char optBaixarNomes[4000][BAIXAR_NOME_LEN];
        static BaixarItem baixarBuf[4000];
        static int baixarBufTotal = 0;
        static int baixarGameIdx = -1;
        static int ftpEdMap[10];
        static char ftpMenuNomes[2002][FTP_NAME_LEN];
        static const char* ftpMenuDyn[2002];
        static bool ftpMenuIsDir[2002];
        static int ftpMenuRemotoInicio = 2;
        const char* optEditar[] = { "Voltar", "LISTA E GRADE", "CAPA", "DISCO", "CAPA DE VIDEO", "FUNDO", "BARRA LOAD", "MENU AUDIO", "MENU UPLOAD", "FONTE (TEXTO)", "NOTIFICACOES", "EXPLORAR", "ELEMENTO FIXO", "ELEMENTO CTRL", "PONTEIRO", "ELEMENTOS SONOROS", "ANIMACAO (SPRITES)", "INTERFACE ELEMENTOS", "RESETAR TUDO" };
        const char* optExtra[] = { "Voltar", "Teste de Controle", "Teste de Teclado", "Piano Virtual (Instrumento)", "Ferramentas" };
        const char* optConfiguracoes[] = { "Voltar", "Chaves API", "Idioma", "Tema", "Sobre" };
        const char* optInfo[] = { "Voltar", "Informacoes do Sistema" };
        const char* optArqGeral[] = { "Voltar", "Nova Pasta", "Novo Arquivo de Texto", "Colar" };
        const char* optArqArquivo[] = { "Voltar", "Copiar", "Recortar", "Renomear", "Deletar", "Extrair ZIP", "Compactar ZIP", "Usar como Fundo" };
        
        const char* ptrMusicas[1000];
        const char* ptrImagens[1000];
        const char* ptrVideos[1000];
        const char* ptrAudios[1000];
        const char* ptrLeitura[1000];
        const char* ptrArquivos[1000];
        const char* ptrRadio[1000];
        const char* optRadioCategorias[] = { "Voltar", "Pesquisar", "Top 50 Populares", "Rádios de São Paulo", "Podcasts", "Outros Filtros" };
        const char* optRadioOutros[] = { "Voltar", "Pesquisar por Gênero", "Pesquisar por País", "Pesquisar por Estado", "Pesquisar por Idioma" };
        const char* optAudio[10];
        char volPlusStr[64], volMinusStr[64], modoStr[64];

        if (currentState == MENU_MAIN) { opcoes = optMain; qtd = 9; }
        else if (currentState == MENU_JOGAR) { opcoes = optJogar; qtd = 3; }
        else if (currentState == MENU_MIDIA) { opcoes = optMidia; qtd = 8; }
        else if (currentState == MENU_MIDIA_VIDEOS) { opcoes = optMidiaVideos; qtd = 6; }
        else if (currentState == MENU_MIDIA_MUSICAS) {
            ptrMusicas[0] = "Voltar";
            ptrMusicas[1] = "Pesquisar";
            ptrMusicas[2] = "OPÇÕES DE ÁUDIO";
            int v = 3;
            for (int i = 0; i < qtdMusicas; i++) {
                if (strcmp(listaMusicasNomes[i], "Voltar") == 0 || strcmp(listaMusicasNomes[i], "OPCOES DE AUDIO") == 0) continue;
                if (globalSearchQuery[0] != '\0' && !strcasestr_custom(listaMusicasNomes[i], globalSearchQuery)) continue;
                ptrMusicas[v++] = listaMusicasNomes[i];
            }
            opcoes = ptrMusicas; qtd = v;
        }
        else if (currentState == MENU_MIDIA_IMAGENS) {
            ptrImagens[0] = "Voltar";
            ptrImagens[1] = "Pesquisar";
            int v = 2;
            for (int i = 0; i < qtdImagens; i++) {
                if (strcmp(listaImagensNomes[i], "Voltar") == 0) continue;
                if (globalSearchQuery[0] != '\0' && !strcasestr_custom(listaImagensNomes[i], globalSearchQuery)) continue;
                ptrImagens[v++] = listaImagensNomes[i];
            }
            opcoes = ptrImagens; qtd = v;
        }
        else if (currentState == MENU_MIDIA_LEITURA) {
            ptrLeitura[0] = "Voltar";
            ptrLeitura[1] = "Pesquisar";
            int v = 2;
            for (int i = 0; i < qtdLeitura; i++) {
                if (strcmp(listaLeituraNomes[i], "Voltar") == 0) continue;
                if (globalSearchQuery[0] != '\0' && !strcasestr_custom(listaLeituraNomes[i], globalSearchQuery)) continue;
                ptrLeitura[v++] = listaLeituraNomes[i];
            }
            opcoes = ptrLeitura; qtd = v;
        }
        else if (currentState == MENU_MIDIA_ARQUIVOS) {
            ptrArquivos[0] = "Voltar";
            ptrArquivos[1] = "Pesquisar";
            ptrArquivos[2] = "Opcoes";
            int v = 3;
            for (int i = 0; i < qtdArquivos; i++) {
                if (strcmp(listaArquivosNomes[i], "Voltar") == 0) continue;
                if (strcmp(listaArquivosNomes[i], "Pesquisar") == 0) continue;
                if (strcmp(listaArquivosNomes[i], "Opcoes") == 0) continue;
                if (globalSearchQuery[0] != '\0' && !strcasestr_custom(listaArquivosNomes[i], globalSearchQuery)) continue;
                ptrArquivos[v++] = listaArquivosNomes[i];
            }
            opcoes = ptrArquivos; qtd = v;
        }
        else if (currentState == MENU_MIDIA_RADIO_CATEGORIA) {
            opcoes = optRadioCategorias; qtd = 6;
            lastHoveredRadio = -1;
        }
        else if (currentState == MENU_MIDIA_RADIO_OUTROS) {
            opcoes = optRadioOutros; qtd = 5;
            lastHoveredRadio = -1;
        }
        else if (currentState == MENU_MIDIA_RADIO_LISTA) {
            for (int i = 0; i < radioCount; i++) ptrRadio[i] = radioNames[i];
            opcoes = ptrRadio; qtd = radioCount;
        }
        else if (currentState == MENU_AUDIO_OPCOES) {
            sprintf(volPlusStr, "VOLUME + (%d%%)", volumeGeral);
            sprintf(volMinusStr, "VOLUME - (%d%%)", volumeGeral);
            const char* textosModo[] = { "MODO: LINEAR", "MODO: REPETIR FAIXA", "MODO: REPETIR PASTA", "MODO: ALEATORIO PASTA", "MODO: ALEATORIO TODAS" };
            sprintf(modoStr, "%s", textosModo[modoReproducao]);
            
            optAudio[0] = "PLAY / PAUSE"; optAudio[1] = "PARAR"; optAudio[2] = "PROXIMA FAIXA"; optAudio[3] = "FAIXA ANTERIOR";
            optAudio[4] = volPlusStr; optAudio[5] = volMinusStr; optAudio[6] = "ADIANTAR 10s"; optAudio[7] = "RETROCEDER 10s";
            optAudio[8] = modoStr; optAudio[9] = "VOLTAR";
            opcoes = optAudio; qtd = 10; espacamento = 50; alturaBtn = 40;
        }
        else if (currentState == MENU_BAIXAR) { opcoes = optBaixar; qtd = 7; }
        else if (currentState == MENU_BAIXAR_MIDIA) {
            static const char* optMidia[] = { "Voltar", "Anime", "Musica", "Livros", "Legendas", "Filmes" };
            opcoes = optMidia; qtd = 6; espacamento = 50; alturaBtn = 40;
        }
        else if (currentState == MENU_BAIXAR_NUVEM) {
            static const char* optNuvem[] = { "Voltar", "Dropbox", "Google Drive" };
            opcoes = optNuvem; qtd = 3; espacamento = 50; alturaBtn = 40;
        }
        else if (currentState == MENU_BAIXAR_LINK) {
            static const char* optLink[] = { "Voltar", "BAIXAR URL" };
            opcoes = optLink; qtd = 2;
        }
        else if (currentState == MENU_BAIXAR_REPOS) {
            baixar_listar_repositorios(baixarBuf, &baixarBufTotal);
            for (int k = 0; k < baixarBufTotal; k++) strcpy(optBaixarNomes[k], baixarBuf[k].nome);
            strcpy(optBaixarNomes[baixarBufTotal], "Voltar"); baixarBufTotal++;
            for (int k = 0; k < baixarBufTotal; k++) optBaixarDyn[k] = optBaixarNomes[k];
            opcoes = optBaixarDyn; qtd = baixarBufTotal; espacamento = 50; alturaBtn = 40;
        }
        else if (currentState == MENU_BAIXAR_REPOS_XML) {
            baixar_listar_xmls_repositorio(baixarBuf, &baixarBufTotal);
            for (int k = 0; k < baixarBufTotal; k++) strcpy(optBaixarNomes[k], baixarBuf[k].nome);
            strcpy(optBaixarNomes[baixarBufTotal], "Voltar"); baixarBufTotal++;
            for (int k = 0; k < baixarBufTotal; k++) optBaixarDyn[k] = optBaixarNomes[k];
            opcoes = optBaixarDyn; qtd = baixarBufTotal; espacamento = 50; alturaBtn = 40;
        }
        else if (currentState == MENU_BAIXAR_REPOS_GAMES) {
            for (int k = 0; k < baixarBufTotal; k++) strcpy(optBaixarNomes[k], baixarBuf[k].nome);
            strcpy(optBaixarNomes[baixarBufTotal], "Voltar"); baixarBufTotal++;
            for (int k = 0; k < baixarBufTotal; k++) optBaixarDyn[k] = optBaixarNomes[k];
            opcoes = optBaixarDyn; qtd = baixarBufTotal; espacamento = 50; alturaBtn = 40;
        }
        else if (currentState == MENU_BAIXAR_REPOS_LINKS) {
            for (int k = 0; k < baixarBufTotal; k++) strcpy(optBaixarNomes[k], baixarBuf[k].nome);
            strcpy(optBaixarNomes[baixarBufTotal], "Voltar"); baixarBufTotal++;
            for (int k = 0; k < baixarBufTotal; k++) optBaixarDyn[k] = optBaixarNomes[k];
            opcoes = optBaixarDyn; qtd = baixarBufTotal; espacamento = 50; alturaBtn = 40;
        }
        else if (currentState == MENU_BAIXAR_DROPBOX) {
            baixar_listar_dropbox(baixarBuf, &baixarBufTotal);
            for (int k = 0; k < baixarBufTotal; k++) strcpy(optBaixarNomes[k], baixarBuf[k].nome);
            strcpy(optBaixarNomes[baixarBufTotal], "Voltar"); baixarBufTotal++;
            for (int k = 0; k < baixarBufTotal; k++) optBaixarDyn[k] = optBaixarNomes[k];
            opcoes = optBaixarDyn; qtd = baixarBufTotal; espacamento = 50; alturaBtn = 40;
        }
        else if (currentState == MENU_BAIXAR_LOJAS) {
            baixar_listar_lojas(baixarBuf, &baixarBufTotal);
            for (int k = 0; k < baixarBufTotal; k++) strcpy(optBaixarNomes[k], baixarBuf[k].nome);
            strcpy(optBaixarNomes[baixarBufTotal], "Voltar"); baixarBufTotal++;
            for (int k = 0; k < baixarBufTotal; k++) optBaixarDyn[k] = optBaixarNomes[k];
            opcoes = optBaixarDyn; qtd = baixarBufTotal; espacamento = 50; alturaBtn = 40;
        }
        else if (currentState == MENU_BAIXAR_HB || currentState == MENU_BAIXAR_APOLLO || currentState == MENU_BAIXAR_RETRO) {
            static const char* optLojaDyn[BAIXAR_MAX_LINKS + 1];
            static char optLojaNomes[BAIXAR_MAX_LINKS + 1][BAIXAR_NOME_LEN];
            int n = 0;
            for (int k = 0; k < baixarTotal && n < BAIXAR_MAX_LINKS; k++) {
                strcpy(optLojaNomes[n], baixarLista[k].nome);
                optLojaDyn[n] = optLojaNomes[n]; n++;
            }
            strcpy(optLojaNomes[n], "Voltar"); optLojaDyn[n] = optLojaNomes[n]; n++;
            opcoes = optLojaDyn; qtd = n; espacamento = 50; alturaBtn = 40;
        }
        else if (currentState == MENU_BAIXAR_FTP) {
            ftp_carregar_servidores();
            baixarBufTotal = 0;
            strcpy(baixarBuf[baixarBufTotal].nome, "Voltar"); baixarBuf[baixarBufTotal].link[0] = 0; baixarBufTotal++;
            if (ftpTotalServers < 3000) { strcpy(baixarBuf[baixarBufTotal].nome, "[+] Adicionar Novo Servidor"); baixarBuf[baixarBufTotal].link[0] = 0; baixarBufTotal++; }
            for (int s = 0; s < ftpTotalServers && baixarBufTotal < 3000; s++) {
                sprintf(baixarBuf[baixarBufTotal].nome, "%s (%s:%d)", ftpServers[s].name, ftpServers[s].ip, ftpServers[s].port);
                baixarBuf[baixarBufTotal].link[0] = 0; baixarBufTotal++;
            }
            for (int k = 0; k < baixarBufTotal; k++) { strcpy(optBaixarNomes[k], baixarBuf[k].nome); optBaixarDyn[k] = optBaixarNomes[k]; }
            opcoes = optBaixarDyn; qtd = baixarBufTotal; espacamento = 50; alturaBtn = 40;
        }
        else if (currentState == MENU_BAIXAR_FTP_EDITAR) {
            static char edNomes[10][180];
            static const char* edDyn[10];
            char masc[180];
            int n = 0;
            int ti = ftpServers[ftpServerIdx].tipo; if (ti < 0 || ti > 2) ti = 0;
            sprintf(masc, "Tipo: %s", ti == 2 ? "SSH" : ti == 1 ? "SFTP" : "FTP"); strcpy(edNomes[n], masc); ftpEdMap[n] = 5; n++;
            sprintf(masc, "Nome: %s", ftpServers[ftpServerIdx].name); strcpy(edNomes[n], masc); ftpEdMap[n] = 0; n++;
            sprintf(masc, "IP: %s", ftpServers[ftpServerIdx].ip); strcpy(edNomes[n], masc); ftpEdMap[n] = 1; n++;
            sprintf(masc, "Porta: %d", ftpServers[ftpServerIdx].port); strcpy(edNomes[n], masc); ftpEdMap[n] = 2; n++;
            sprintf(masc, "Usuario: %s", ftpServers[ftpServerIdx].user); strcpy(edNomes[n], masc); ftpEdMap[n] = 3; n++;
            strcpy(edNomes[n], strlen(ftpServers[ftpServerIdx].pass) > 0 ? "Senha: ****" : "Senha: (vazio)"); ftpEdMap[n] = 4; n++;
            if (ti == 2) {
                sprintf(masc, "Chave SSH: %s", ftpServers[ftpServerIdx].ssh_key[0] ? ftpServers[ftpServerIdx].ssh_key : "(nenhuma)"); strcpy(edNomes[n], masc); ftpEdMap[n] = 6; n++;
            }
            if (ftpServerIdx >= 0 && ftpServerIdx < ftpTotalServers) { strcpy(edNomes[n], "[ SALVAR SERVIDOR ]"); ftpEdMap[n] = -1; n++; strcpy(edNomes[n], "[ DELETAR SERVIDOR ]"); ftpEdMap[n] = -2; n++; }
            else { strcpy(edNomes[n], "[ SALVAR SERVIDOR ]"); ftpEdMap[n] = -1; n++; strcpy(edNomes[n], "[ CANCELAR ]"); ftpEdMap[n] = -2; n++; }
            strcpy(edNomes[n], "Voltar"); ftpEdMap[n] = -3; n++;
            for (int k = 0; k < n; k++) edDyn[k] = edNomes[k];
            opcoes = edDyn; qtd = n; espacamento = 50; alturaBtn = 40;
        }
        else if (currentState == MENU_BAIXAR_FTP_GERENCIAR) {
            int n = 0;
            strcpy(ftpMenuNomes[n], "Voltar"); ftpMenuDyn[n] = ftpMenuNomes[n]; ftpMenuIsDir[n] = false; n++;
            if (strcmp(ftpRemotePath, "/") != 0) {
                strcpy(ftpMenuNomes[n], ".. (pasta acima)"); ftpMenuDyn[n] = ftpMenuNomes[n]; ftpMenuIsDir[n] = false; n++;
            }
            ftpMenuRemotoInicio = n;
            if (ftpListandoRemoto) {
                strcpy(ftpMenuNomes[n], "Carregando..."); ftpMenuDyn[n] = ftpMenuNomes[n]; ftpMenuIsDir[n] = false; n++;
            } else {
                for (int s = 0; s < ftpRemoteTotal && n < 2000; s++) {
                    strncpy(ftpMenuNomes[n], ftpRemoteList[s].name, FTP_NAME_LEN - 1); ftpMenuNomes[n][FTP_NAME_LEN - 1] = 0;
                    ftpMenuDyn[n] = ftpMenuNomes[n];
                    ftpMenuIsDir[n] = ftpRemoteList[s].isDir;
                    n++;
                }
            }
            opcoes = ftpMenuDyn; qtd = n; espacamento = 60; alturaBtn = 45;
        }

        if (currentState == MENU_BAIXAR_ANIME || currentState == MENU_BAIXAR_MUSICA ||
            currentState == MENU_BAIXAR_LIVROS || currentState == MENU_BAIXAR_LEGENDAS) {
            const char* titulo = "Anime";
            void (*buscar)(const char*) = midia_buscar_anime;
            if (currentState == MENU_BAIXAR_MUSICA) { titulo = "Musica"; buscar = midia_buscar_musica; }
            else if (currentState == MENU_BAIXAR_LIVROS) { titulo = "Livros"; buscar = midia_buscar_livro; }
            else if (currentState == MENU_BAIXAR_LEGENDAS) { titulo = "Legendas"; buscar = midia_buscar_legenda; }

            DrawRectangle(20, 20, 90, 30, (Color){120,50,50,255});
            DrawText("VOLTAR", 30, 28, 15, (Color){255,255,255,255});
            DrawText(titulo, 130, 26, 20, textLight);
            DrawRectangle(20, 60, 520, 34, (Color){20,24,32,255});
            DrawText(midiaTyping ? TextFormat("Buscar: %s_", midiaBusca) : TextFormat("Buscar: %s", midiaBusca), 30, 68, 16, midiaTyping ? (Color){255,220,120,255} : (Color){200,200,200,255});
            DrawRectangle(550, 60, 90, 34, (Color){50,90,50,255});
            DrawText("BUSCAR", 560, 68, 15, (Color){255,255,255,255});
            DrawText(midiaStatus, 20, 104, 14, midiaBuscando ? (Color){255,200,0,255} : (Color){180,220,180,255});

            int lx = 20, lw = screenWidth - 40, ly = 150, lh = 40;
            for (int i = 0; i < midiaTotal; i++) {
                int y = ly + i * lh - midiaScroll;
                if (y < ly - lh || y > screenHeight - 40) continue;
                bool sel = (i == midiaSel);
                if (sel) DrawRectangle(lx, y, lw, lh - 4, (Color){60,80,120,255});
                MidiaItem* m = &midiaResultados[i];
                DrawText(TextFormat("%s", m->nome), lx + 8, y + 4, 14, textLight);
                DrawText(TextFormat("%s", m->info), lx + 8, y + 22, 11, (Color){160,160,160,255});
            }
            DrawText("T: buscar | Enter: abrir | D: baixar | B: voltar", 20, screenHeight - 22, 12, (Color){150,150,150,255});
            if (currentState == MENU_BAIXAR_ANIME) {
                DrawRectangle(screenWidth - 210, screenHeight - 46, 190, 30, (Color){90,90,90,255});
                DrawText("TORRENT ANIME", screenWidth - 205, screenHeight - 40, 12, (Color){200,200,200,255});
            }
        }
        else if (currentState == MENU_BAIXAR_ANIME_TORRENT) {
            DrawRectangle(20, 20, 90, 30, (Color){120,50,50,255});
            DrawText("VOLTAR", 30, 28, 15, (Color){255,255,255,255});
            DrawText(TextFormat("Torrents Anime: %s", midiaTermo), 130, 26, 18, textLight);
            DrawText(midiaStatus, 20, 100, 14, midiaBuscando ? (Color){255,200,0,255} : (Color){180,220,180,255});
            int lx = 20, lw = screenWidth - 40, ly = 140, lh = 30;
            for (int i = 0; i < midiaTotal; i++) {
                int y = ly + i * lh - midiaScroll;
                if (y < ly - lh || y > screenHeight - 40) continue;
                bool sel = (i == midiaSel);
                if (sel) DrawRectangle(lx, y, lw, lh - 2, (Color){60,80,120,255});
                DrawText(TextFormat("%s", midiaResultados[i].nome), lx + 8, y + 4, 13, textLight);
                DrawText(TextFormat("%s", midiaResultados[i].info), lx + 8, y + 18, 11, (Color){160,160,160,255});
            }
            DrawText("Enter: abrir magnet | D: baixar | B: voltar", 20, screenHeight - 22, 12, (Color){150,150,150,255});
        }

        else if (currentState == MENU_BAIXAR_FILA) {
            static const char* optFila[] = { "Voltar", "INICIAR FILA", "ABRIR PASTA" };
            opcoes = optFila; qtd = 3;
        }
        else if (currentState == MENU_EDITAR) { opcoes = optEditar; qtd = 19; espacamento = 50; alturaBtn = 40; }
        else if (currentState == MENU_EXPLORAR) { initExplorarMenu(); opcoes = ptrExplorar; qtd = qtdExplorarDin; }
        else if (currentState == MENU_EXTRA) { opcoes = optExtra; qtd = 5; }
        else if (currentState == MENU_CONFIGURACOES) { opcoes = optConfiguracoes; qtd = 5; }
        else if (currentState == MENU_CONFIG_API) { qtd = 0; }
        else if (currentState == MENU_CONFIG_API_GERAR_DROPBOX) { qtd = 0; }
        else if (currentState == MENU_INFORMACOES) { opcoes = optInfo; qtd = 2; }
        else if (currentState == MENU_ARQUIVOS_OPCOES_GERAL) { opcoes = optArqGeral; qtd = 4; }
        else if (currentState == MENU_ARQUIVOS_OPCOES_ARQUIVO) {
            // Build dynamic options based on file type
            static const char* optArqArquivoDyn[10];
            const char* extArq = strrchr(targetFilePath, '.');
            int qArq = 0;
            optArqArquivoDyn[qArq++] = "Voltar";
            optArqArquivoDyn[qArq++] = "Copiar";
            optArqArquivoDyn[qArq++] = "Recortar";
            optArqArquivoDyn[qArq++] = "Renomear";
            optArqArquivoDyn[qArq++] = "Deletar";
            optArqArquivoDyn[qArq++] = "Compactar ZIP";
            if (extArq && strcasecmp(extArq, ".zip") == 0) optArqArquivoDyn[qArq++] = "Extrair ZIP";
            if (extArq && (strcasecmp(extArq, ".png") == 0 || strcasecmp(extArq, ".jpg") == 0 || strcasecmp(extArq, ".jpeg") == 0 || strcasecmp(extArq, ".bmp") == 0)) optArqArquivoDyn[qArq++] = "Usar como Fundo";
            opcoes = optArqArquivoDyn; qtd = qArq;
        }
        else if (currentState == MENU_BAIXAR_DROPBOX_NAV) { qtd = 0; }

        if (currentState == MENU_BAIXAR || currentState == MENU_BAIXAR_LINK || currentState == MENU_BAIXAR_FILA ||
            currentState == MENU_BAIXAR_REPOS || currentState == MENU_BAIXAR_REPOS_XML ||
            currentState == MENU_BAIXAR_REPOS_GAMES || currentState == MENU_BAIXAR_REPOS_LINKS ||
            currentState == MENU_BAIXAR_DROPBOX || currentState == MENU_BAIXAR_LOJAS ||
            currentState == MENU_BAIXAR_FTP || currentState == MENU_BAIXAR_DROPBOX_NAV ||
            currentState == MENU_BAIXAR_MIDIA || currentState == MENU_BAIXAR_NUVEM ||
            currentState == MENU_BAIXAR_GOOGLE_DRIVE_NAV) {
            DrawText("BAIXAR", 20, 20, 28, textLight);
        }

        if (currentState == MENU_BAIXAR_LINK) {
            DrawText("Cole a URL (CTRL+V para colar) e clique em BAIXAR:", 20, 70, 16, (Color){200,200,200,255});
            DrawRectangle(20, 100, 410, 40, (Color){20,30,50,255});
            DrawRectangleLines(20, 100, 410, 40, baixarTyping ? (Color){50,150,255,255} : (Color){80,80,80,255});
            DrawText(baixarUrlInput[0] ? baixarUrlInput : "(cole a url aqui)", 30, 112, 16, baixarUrlInput[0] ? WHITE : (Color){120,120,120,255});
            if (baixarTyping && (int)(GetTime()*2)%2==0) DrawText("_", 30 + MeasureText(baixarUrlInput, 16), 112, 16, WHITE);
        }
        else if (currentState == MENU_BAIXAR_FILA) {
            int naFila = baixar_contar_fila();
            DrawText("FILA DE DOWNLOADS", 20, 60, 22, textLight);
            DrawText(TextFormat("Itens na fila: %d", naFila), 20, 92, 16, (Color){200,200,200,255});
            char pct[32]; sprintf(pct, "%d%%", (int)(baixarProgresso*100.0f));
            DrawRectangle(20, 118, 410, 26, (Color){40,40,40,255});
            int fill = (int)(410.0f * baixarProgresso);
            DrawRectangle(20, 118, fill, 26, (Color){0,180,80,255});
            DrawText(pct, 440, 121, 16, WHITE);
            DrawText(baixarStatus, 20, 156, 14, (Color){180,220,180,255});
            if (baixarBaixando) DrawText("Baixando...", 20, 178, 14, (Color){255,200,0,255});
            char destino[600];
            sprintf(destino, "Salvo em: %s/baixado", baixar_dir_base());
            DrawText(destino, 20, 205, 13, (Color){150,200,255,255});
            if (baixarUltimoArquivo[0]) {
                const char* nomeArq = strrchr(baixarUltimoArquivo, '/');
                if (!nomeArq) nomeArq = strrchr(baixarUltimoArquivo, '\\');
                DrawText(TextFormat("Ultimo: %s", nomeArq ? nomeArq + 1 : baixarUltimoArquivo), 20, 226, 13, (Color){180,180,180,255});
            }
            DrawText("Botao direita: ABRIR PASTA", 20, 250, 13, (Color){255,200,120,255});
        }
        else if (currentState == MENU_BAIXAR_REPOS || currentState == MENU_BAIXAR_REPOS_XML ||
                 currentState == MENU_BAIXAR_REPOS_GAMES || currentState == MENU_BAIXAR_REPOS_LINKS) {
            DrawText("Repositorios", 20, 60, 20, textLight);
            DrawText(TextFormat("Pasta: %s", baixarPastaRepos[0] ? baixarPastaRepos : "-"), 20, 90, 14, (Color){180,180,180,255});
            DrawText("Use os botoes a direita para navegar.", 20, 115, 14, (Color){150,150,150,255});
        }
        else if (currentState == MENU_BAIXAR_DROPBOX) {
            DrawText("Dropbox", 20, 60, 20, textLight);
            DrawText("Cole um link direto do Dropbox (dl=1) e baixe.", 20, 90, 14, (Color){180,180,180,255});
        }
        else if (currentState == MENU_BAIXAR_LOJAS) {
            DrawText("Lojas", 20, 60, 20, textLight);
            DrawText("Escolha uma loja para ver os itens.", 20, 90, 14, (Color){180,180,180,255});
        }
        else if (currentState == MENU_BAIXAR_HB) {
            DrawText("HB Store (pkg-zone)", 20, 60, 20, textLight);
            if (baixar_loja_carregando()) DrawText("Carregando lista...", 20, 95, 16, (Color){255,200,0,255});
            else if (baixarLojaErro[0]) DrawText(baixarLojaErro, 20, 95, 14, (Color){255,120,120,255});
            else DrawText(TextFormat("Itens: %d  |  clique para baixar", baixarTotal), 20, 95, 14, (Color){180,220,180,255});
        }
        else if (currentState == MENU_BAIXAR_APOLLO) {
            DrawText("Apollo Saves (PS4)", 20, 60, 20, textLight);
            if (baixar_loja_carregando()) DrawText("Carregando lista...", 20, 95, 16, (Color){255,200,0,255});
            else if (baixarLojaErro[0]) DrawText(baixarLojaErro, 20, 95, 14, (Color){255,120,120,255});
            else DrawText(TextFormat("Saves: %d  |  clique para baixar", baixarTotal), 20, 95, 14, (Color){180,220,180,255});
        }
        else if (currentState == MENU_BAIXAR_RETRO) {
            DrawText("RetroArch", 20, 60, 20, textLight);
            if (baixar_loja_carregando()) DrawText("Carregando...", 20, 95, 16, (Color){255,200,0,255});
            else DrawText(TextFormat("Links: %d", baixarTotal), 20, 95, 14, (Color){180,220,180,255});
        }
        else if (currentState == MENU_BAIXAR_FTP) {
            DrawText("FTP (PC / Android)", 20, 60, 20, textLight);
            DrawText("Selecione um servidor ou adicione um novo.", 20, 90, 14, (Color){180,180,180,255});
            if (baixarStatus[0]) DrawText(baixarStatus, 20, 120, 14, (Color){180,220,180,255});
        }
        else if (currentState == MENU_BAIXAR_FTP_EDITAR) {
            DrawText(TextFormat("Editar Servidor FTP: %s", ftpServers[ftpServerIdx].name[0] ? ftpServers[ftpServerIdx].name : "NOVO"), 20, 60, 20, textLight);
            DrawText("X/Enter: editar campo | digite e Enter salva | Esc cancela", 20, 90, 13, (Color){180,180,180,255});
            if (ftpEditandoCampo >= 0) {
                DrawText(TextFormat("Editando: %s", ftpEditBuf), 20, 110, 14, (Color){255,220,120,255});
            }
        }
        else if (currentState == MENU_BAIXAR_FTP_GERENCIAR) {
            DrawText(TextFormat("FTP: %s   [REMOTO: %s]", ftpServers[ftpServerIdx].name, ftpRemotePath), 20, 30, 18, textLight);
            DrawText(ftpStatus, 20, 52, 13, ftpTransferindo ? (Color){255,200,0,255} : (Color){180,220,180,255});
            if (ftpTransferindo) DrawText(TextFormat("%.0f%%", ftpProgresso * 100.0f), 20, 70, 14, (Color){255,220,120,255});

            // PAINEL LOCAL (esquerda)
            int pxL = 20, pwL = 440, py = 95, ph = screenHeight - 160, ih = 26;
            DrawRectangle(pxL, py, pwL, ph, (Color){28,30,38,255});
            DrawText(TextFormat("LOCAL: %s", ftpLocalPath), pxL + 8, py - 18, 13, ftpPainelAtivo == 0 ? (Color){255,220,120,255} : (Color){160,160,160,255});
            for (int i = 0; i < ftpLocalTotal; i++) {
                int y = py + 6 + i * ih - (ftpLocalSel > 12 ? (ftpLocalSel - 12) * ih : 0);
                if (y < py - ih || y > py + ph) continue;
                bool sel = (i == ftpLocalSel && ftpPainelAtivo == 0);
                Rectangle row = { (float)pxL, (float)y, (float)pwL, (float)ih };
                bool hov = CheckCollisionPointRec(mousePoint, row);
                DrawRectangleRec(row, sel ? (Color){60,80,120,255} : (hov ? btnHover : btnNormal));
                int tx = pxL + 20;
                if (ftpLocalList[i].isDir) {
                    DrawRectangle(tx, y + 6, 6, 4, (Color){230,180,60,255});
                    DrawRectangle(tx, y + 10, 15, 8, (Color){230,180,60,255});
                    tx += 22;
                }
                DrawText(ftpLocalList[i].name, tx, y + 5, 14, ftpLocalList[i].isDir ? (Color){120,200,255,255} : textLight);
            }
            DrawText("X: baixar(remoto)/enviar(local) | O: entrar | B: voltar | Tab: trocar painel | Y: nova pasta", 20, screenHeight - 22, 12, (Color){150,150,150,255});
            if (ftpServers[ftpServerIdx].tipo == 2) {
                DrawRectangle(screenWidth - 210, 20, 90, 30, (Color){50,90,50,255});
                DrawText("TERMINAL", screenWidth - 205, 28, 15, (Color){255,255,255,255});
            }
            DrawRectangle(screenWidth - 110, 20, 90, 30, (Color){120,50,50,255});
            DrawText("VOLTAR", screenWidth - 100, 28, 15, (Color){255,255,255,255});
            if (ftpCriandoPasta) {
                DrawRectangle(20, screenHeight - 60, screenWidth - 40, 34, (Color){20,24,32,255});
                DrawText(TextFormat("Nome da pasta: %s_", ftpEditBuf), 30, screenHeight - 52, 16, (Color){255,220,120,255});
            }
        }

        else if (currentState == MENU_BAIXAR_DROPBOX_NAV) {
            if (dropboxConectando) {
                dropbox_iniciar();
                if (dropboxTokenValido) {
                    dropbox_listar("");
                    notificar("Conectado ao Dropbox!");
                } else {
                    notificar(dropboxStatus);
                }
                dropboxConectando = false;
            }
            DrawText("Dropbox - Navegacao", 470, 20, 20, textLight);
            if (dropboxTotal == 0 && !dropboxTokenValido) {
                DrawText("Nao foi possivel conectar ao Dropbox.", 470, 50, 14, (Color){255,150,150,255});
                DrawText(dropboxStatus, 470, 75, 13, (Color){200,200,200,255});
            }
            if (dropboxRcItem < 0 || dropboxRcItem >= dropboxTotal) dropboxRcItem = -1;
            if (wheelMove != 0 && mousePoint.x >= 470 && dropboxRcItem < 0) dbScroll += (int)(wheelMove * 30);
            if (dbScroll > 0) dbScroll = 0;
            int maxDbScroll = dropboxTotal * 30 - 450;
            if (maxDbScroll < 0) maxDbScroll = 0;
            if (dbScroll < -maxDbScroll) dbScroll = -maxDbScroll;
            int y = 60 + dbScroll;
            for (int i = 0; i < dropboxTotal; i++) {
                if (y + 30 >= 50 && y < screenHeight) {
                    Color c = (Color){20,25,40,200};
                    if (dropboxMarcados[i]) c = (Color){80,60,30,200};
                    if (CheckCollisionPointRec(mousePoint, (Rectangle){470, y, 310, 28})) c = dropboxMarcados[i] ? (Color){110,80,40,255} : (Color){40,50,70,255};
                    DrawRectangle(470, y, 310, 28, c);
                    char label[300];
                    if (dropboxItems[i].isDir && i == 0 && strcmp(dropboxItems[i].nome, "..") == 0)
                        snprintf(label, 300, "[..] Subir");
                    else if (dropboxItems[i].isDir)
                        snprintf(label, 300, "[P] %s", dropboxItems[i].nome);
                    else
                        snprintf(label, 300, "[F] %s", dropboxItems[i].nome);
                    DrawText(label, 475, y+5, 12, dropboxItems[i].isDir ? (Color){100,200,255,255} : (Color){200,200,200,255});
                    if (dropboxRcItem < 0) {
                        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePoint, (Rectangle){470, y, 310, 28})) {
                            if (dropboxItems[i].isDir && i == 0 && strcmp(dropboxItems[i].nome, "..") == 0) {
                                char* lastSlash = strrchr(dropboxCurrentPath, '/');
                                if (lastSlash) {
                                    if (lastSlash == dropboxCurrentPath) lastSlash[1] = 0;
                                    else *lastSlash = 0;
                                } else dropboxCurrentPath[0] = 0;
                                dropbox_listar(dropboxCurrentPath);
                            } else if (dropboxItems[i].isDir) {
                                dropbox_listar(dropboxItems[i].path);
                            } else {
                                dropbox_baixar(dropboxItems[i].path, dropboxItems[i].nome);
                                notificar("Download iniciado!");
                            }
                        }
                        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && CheckCollisionPointRec(mousePoint, (Rectangle){470, y, 310, 28})) {
                            dropboxRcItem = i;
                            dropboxRcX = GetMouseX(); dropboxRcY = GetMouseY();
                        }
                    }
                }
                y += 30;
            }
            if (dropboxStatus[0] && dropboxTotal > 0) DrawText(dropboxStatus, 470, screenHeight - 40, 11, (Color){180,180,180,255});
            if (dropboxInputAction > 0) {
                int iw = 400, ih = 100, ix = (screenWidth - iw) / 2, iy = (screenHeight - ih) / 2;
                DrawRectangle(ix, iy, iw, ih, (Color){30,35,50,240});
                DrawRectangleLines(ix, iy, iw, ih, (Color){80,90,120,255});
                DrawText(dropboxInputAction == 1 ? "Novo nome:" : "Nome da pasta:", ix + 10, iy + 10, 14, WHITE);
                DrawRectangle(ix + 10, iy + 35, iw - 20, 28, (Color){20,25,40,240});
                DrawRectangleLines(ix + 10, iy + 35, iw - 20, 28, (Color){60,80,120,255});
                DrawText(TextFormat("%s_", baixarUrlInput), ix + 16, iy + 39, 14, WHITE);
                Rectangle btnOk = { ix + iw - 180, iy + ih - 35, 70, 25 };
                Rectangle btnCancel = { ix + iw - 90, iy + ih - 35, 70, 25 };
                bool hOk = CheckCollisionPointRec(mousePoint, btnOk);
                bool hCancel = CheckCollisionPointRec(mousePoint, btnCancel);
                DrawRectangleRec(btnOk, hOk ? (Color){50,160,50,255} : (Color){30,100,40,200});
                DrawRectangleRec(btnCancel, hCancel ? (Color){160,50,50,255} : (Color){100,30,30,200});
                DrawText("OK", btnOk.x + 22, btnOk.y + 5, 13, WHITE);
                DrawText("Cancelar", btnCancel.x + 8, btnCancel.y + 5, 12, WHITE);
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (hCancel) { dropboxInputAction = 0; baixarUrlInput[0] = 0; }
                    else if (hOk && strlen(baixarUrlInput) > 0) {
                        if (dropboxInputAction == 1 && dropboxRcItem >= 0) {
                            dropbox_renomear(dropboxItems[dropboxRcItem].path, baixarUrlInput);
                            dropbox_listar(dropboxCurrentPath);
                        } else if (dropboxInputAction == 2) {
                            char newPath[1024];
                            if (dropboxCurrentPath[0]) snprintf(newPath, sizeof(newPath), "%s/%s", dropboxCurrentPath, baixarUrlInput);
                            else snprintf(newPath, sizeof(newPath), "/%s", baixarUrlInput);
                            dropbox_criar_pasta(newPath);
                            dropbox_listar(dropboxCurrentPath);
                        }
                        dropboxInputAction = 0; baixarUrlInput[0] = 0;
                    }
                }
                if (IsKeyPressed(KEY_ENTER) && strlen(baixarUrlInput) > 0) {
                    if (dropboxInputAction == 1 && dropboxRcItem >= 0) {
                        dropbox_renomear(dropboxItems[dropboxRcItem].path, baixarUrlInput);
                        dropbox_listar(dropboxCurrentPath);
                    } else if (dropboxInputAction == 2) {
                        char newPath[1024];
                        if (dropboxCurrentPath[0]) snprintf(newPath, sizeof(newPath), "%s/%s", dropboxCurrentPath, baixarUrlInput);
                        else snprintf(newPath, sizeof(newPath), "/%s", baixarUrlInput);
                        dropbox_criar_pasta(newPath);
                        dropbox_listar(dropboxCurrentPath);
                    }
                    dropboxInputAction = 0; baixarUrlInput[0] = 0;
                }
                if (IsKeyPressed(KEY_ESCAPE)) { dropboxInputAction = 0; baixarUrlInput[0] = 0; }
            } else {
                if (dropboxRcItem < 0) {
                    Rectangle btnVoltar = { 470, screenHeight - 80, 100, 30 };
                    bool hVoltar = CheckCollisionPointRec(mousePoint, btnVoltar);
                    DrawRectangleRec(btnVoltar, hVoltar ? btnHover : btnNormal);
                    DrawText("Voltar", 490, screenHeight - 74, 14, textLight);
                    if (hVoltar && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { dropbox_limpar(); currentState = MENU_BAIXAR_NUVEM; scrollY = 0; }
                }
                if (dropboxRcItem >= 0) {
                    int rcIdx = dropboxRcItem;
                    int nMarks = 0; for (int j = 0; j < dropboxTotal; j++) if (dropboxMarcados[j]) nMarks++;
                    const char* sSelTudo = "Selecionar Tudo";
                    const char* sDesTudo = "Desmarcar Tudo";
                    const char* optRc[] = { "Selecionar/Desmarcar", nMarks > 0 ? sDesTudo : sSelTudo, "Baixar", "Copiar Link", "Deletar", "Renomear", "Nova Pasta", "Voltar" };
                    int rcQtd = 8;
                    for (int j = 0; j < rcQtd; j++) {
                        int yPos = 40 + j * 50;
                        if (yPos > -50 && yPos < screenHeight) {
                            Rectangle btn = { 470, yPos, 310, 40 };
                            bool h = CheckCollisionPointRec(mousePoint, btn);
                            DrawRectangleRec(btn, h ? btnHover : btnNormal);
                            Color tc = textLight;
                            if ((j == 3 && dropboxItems[rcIdx].isDir) || (j == 2 && dropboxItems[rcIdx].isDir && rcIdx == 0 && strcmp(dropboxItems[rcIdx].nome, "..") == 0))
                                tc = (Color){100,100,100,255};
                            DrawText(optRc[j], (int)btn.x + 20, (int)btn.y + 10, 16, tc);
                            if (h && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                                if (j == 0) { dropboxMarcados[rcIdx] = !dropboxMarcados[rcIdx]; notificar(dropboxMarcados[rcIdx] ? "Marcado" : "Desmarcado"); dropboxRcItem = -1; }
                                else if (j == 1) {
                                    bool any = false; for (int k = 0; k < dropboxTotal; k++) { if (dropboxMarcados[k]) any = true; break; }
                                    for (int k = 0; k < dropboxTotal; k++) dropboxMarcados[k] = !any;
                                    notificar(any ? "Todos desmarcados" : "Todos marcados");
                                    dropboxRcItem = -1;
                                }
                                else if (j == 2) {
                                    if (nMarks > 0) {
                                        for (int k = 0; k < dropboxTotal; k++) {
                                            if (dropboxMarcados[k] && !dropboxItems[k].isDir) {
                                                dropbox_baixar(dropboxItems[k].path, dropboxItems[k].nome);
                                            }
                                        }
                                        notificar("Download dos selecionados iniciado!");
                                    } else if (dropboxItems[rcIdx].isDir && !(rcIdx == 0 && strcmp(dropboxItems[rcIdx].nome, "..") == 0)) {
                                        notificar("Baixando pasta...");
                                        for (int k = 0; k < dropboxTotal; k++) {
                                            if (!dropboxItems[k].isDir) dropbox_baixar(dropboxItems[k].path, dropboxItems[k].nome);
                                        }
                                    } else if (!dropboxItems[rcIdx].isDir) {
                                        dropbox_baixar(dropboxItems[rcIdx].path, dropboxItems[rcIdx].nome);
                                        notificar("Download iniciado!");
                                    }
                                    dropboxRcItem = -1;
                                }
                                else if (j == 3 && !dropboxItems[rcIdx].isDir) {
                                    char link[1024];
                                    snprintf(link, sizeof(link), "https://www.dropbox.com/home%s", dropboxItems[rcIdx].path);
                                    char cmd[1100]; sprintf(cmd, "cmd /c start \"\" \"%s\"", link);
                                    exec_async_silent(cmd);
                                    notificar("Link aberto no navegador!");
                                    dropboxRcItem = -1;
                                }
                                else if (j == 4) {
                                    if (nMarks > 0) {
                                        for (int k = 0; k < dropboxTotal; k++) if (dropboxMarcados[k]) dropbox_deletar(dropboxItems[k].path);
                                        dropbox_listar(dropboxCurrentPath);
                                    } else {
                                        dropbox_deletar(dropboxItems[rcIdx].path);
                                        dropbox_listar(dropboxCurrentPath);
                                    }
                                    dropboxRcItem = -1;
                                }
                                else if (j == 5) {
                                    baixarUrlInput[0] = 0; dropboxInputAction = 1;
                                }
                                else if (j == 6) {
                                    baixarUrlInput[0] = 0; dropboxInputAction = 2;
                                }
                                else if (j == 7) { dropboxRcItem = -1; }
                            }
                        }
                    }
                }
                if (dropboxRcItem >= 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(mousePoint, (Rectangle){470, 40, 310, 400})) {
                    dropboxRcItem = -1;
                }
                if (dropboxRcItem >= 0 && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && !CheckCollisionPointRec(mousePoint, (Rectangle){470, 40, 310, 400})) {
                    dropboxRcItem = -1;
                }
            }
        }

        else if (currentState == MENU_BAIXAR_GOOGLE_DRIVE_NAV) {
            if (gdConectando) {
                gd_iniciar();
                gdConectando = false;
            }
            if (!gdTokenValido && gdStatus[0] == 0) {
                gd_iniciar();
            }
            if (!gdTokenValido) {
                DrawText("Google Drive", 470, 20, 20, textLight);
                if (gdStatus[0]) DrawText(gdStatus, 470, 50, 14, (Color){200,200,200,255});
                static bool gdAguardandoCodigo = false;
                if (gdRefreshToken[0] == 0 && gdClientId[0] && strcmp(gdClientId, "SEU_CLIENT_ID") != 0 && !gdAguardandoCodigo) {
                    Rectangle btnAuth = { 470, 90, 310, 36 };
                    bool hAuth = CheckCollisionPointRec(mousePoint, btnAuth);
                    DrawRectangleRec(btnAuth, hAuth ? btnHover : btnNormal);
                    DrawText("AUTORIZAR", 470 + 90, 98, 15, WHITE);
                    if (hAuth && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        char url[2048];
                        snprintf(url, sizeof(url), "https://accounts.google.com/o/oauth2/v2/auth?client_id=%s&redirect_uri=urn:ietf:wg:oauth:2.0:oob&response_type=code&scope=https://www.googleapis.com/auth/drive", gdClientId);
                        char cmd[2100]; snprintf(cmd, sizeof(cmd), "cmd /c start \"\" \"%s\"", url);
                        exec_async_silent(cmd);
                        gdAguardandoCodigo = true;
                        strcpy(gdStatus, "Cole o codigo abaixo e pressione Enter.");
                    }
                }
                if (gdAguardandoCodigo) {
                    DrawText("Codigo de autorizacao:", 470, 90, 14, (Color){180,180,180,255});
                    DrawRectangle(470, 115, 310, 30, (Color){20,30,50,255});
                    DrawRectangleLines(470, 115, 310, 30, (Color){60,80,120,255});
                    if (baixarTyping) DrawText(TextFormat("%s_", baixarUrlInput), 476, 121, 14, (Color){255,220,120,255});
                    else DrawText(baixarUrlInput[0] ? baixarUrlInput : "(digite o codigo)", 476, 121, 14, (Color){200,200,200,255});
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePoint, (Rectangle){470, 115, 310, 30})) {
                        baixarTyping = true; baixarUrlInput[0] = 0;
                    }
                    if (baixarTyping && IsKeyPressed(KEY_ENTER) && baixarUrlInput[0]) {
                        gd_exchange_code(baixarUrlInput);
                        baixarUrlInput[0] = 0; baixarTyping = false; gdAguardandoCodigo = false;
                        if (gdTokenValido) { gd_listar("root"); }
                    }
                    if (baixarTyping && IsKeyPressed(KEY_ESCAPE)) { baixarTyping = false; baixarUrlInput[0] = 0; gdAguardandoCodigo = false; }
                }
                Rectangle btnVoltarGD = { 470, screenHeight - 80, 100, 30 };
                bool hVoltGD = CheckCollisionPointRec(mousePoint, btnVoltarGD);
                DrawRectangleRec(btnVoltarGD, hVoltGD ? btnHover : btnNormal);
                DrawText("Voltar", 490, screenHeight - 74, 14, textLight);
                if (hVoltGD && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { gdAguardandoCodigo = false; baixarTyping = false; currentState = MENU_BAIXAR_NUVEM; scrollY = 0; }
            } else {
                DrawText("Google Drive - Navegacao", 470, 20, 20, textLight);
                if (gdRcItem < 0 || gdRcItem >= gdTotal) gdRcItem = -1;
                if (wheelMove != 0 && mousePoint.x >= 470 && gdRcItem < 0) gdScroll += (int)(wheelMove * 30);
                if (gdScroll > 0) gdScroll = 0;
                int maxGdScroll = gdTotal * 30 - 450;
                if (maxGdScroll < 0) maxGdScroll = 0;
                if (gdScroll < -maxGdScroll) gdScroll = -maxGdScroll;
                int y = 60 + gdScroll;
                for (int i = 0; i < gdTotal; i++) {
                    if (y + 30 >= 50 && y < screenHeight) {
                        Color c = (Color){20,25,40,200};
                        if (gdMarcados[i]) c = (Color){80,60,30,200};
                        if (CheckCollisionPointRec(mousePoint, (Rectangle){470, y, 310, 28})) c = gdMarcados[i] ? (Color){110,80,40,255} : (Color){40,50,70,255};
                        DrawRectangle(470, y, 310, 28, c);
                        char label[300];
                        if (gdItems[i].isDir && strcmp(gdItems[i].nome, "..") == 0)
                            snprintf(label, 300, "[..] Subir");
                        else if (gdItems[i].isDir)
                            snprintf(label, 300, "[P] %s", gdItems[i].nome);
                        else
                            snprintf(label, 300, "[F] %s", gdItems[i].nome);
                        DrawText(label, 475, y+5, 12, gdItems[i].isDir ? (Color){100,200,255,255} : (Color){200,200,200,255});
                        if (gdRcItem < 0) {
                            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePoint, (Rectangle){470, y, 310, 28})) {
                                if (gdItems[i].isDir && strcmp(gdItems[i].nome, "..") == 0) {
                                    char* lastSlash = strrchr(gdCurrentPath, '/');
                                    if (lastSlash) gd_listar("root");
                                    else { gd_listar("root"); }
                                } else if (gdItems[i].isDir) {
                                    gd_listar(gdItems[i].id);
                                } else {
                                    gd_baixar(gdItems[i].id, gdItems[i].nome);
                                    notificar("Download iniciado!");
                                }
                            }
                            if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && CheckCollisionPointRec(mousePoint, (Rectangle){470, y, 310, 28})) {
                                gdRcItem = i;
                                gdRcX = GetMouseX(); gdRcY = GetMouseY();
                            }
                        }
                    }
                    y += 30;
                }
                if (gdStatus[0] && gdTotal > 0) DrawText(gdStatus, 470, screenHeight - 40, 11, (Color){180,180,180,255});
                if (gdRcItem < 0) {
                    Rectangle btnVoltarGD = { 470, screenHeight - 80, 100, 30 };
                    bool hVoltGD = CheckCollisionPointRec(mousePoint, btnVoltarGD);
                    DrawRectangleRec(btnVoltarGD, hVoltGD ? btnHover : btnNormal);
                    DrawText("Voltar", 490, screenHeight - 74, 14, textLight);
                    if (hVoltGD && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { gd_limpar(); currentState = MENU_BAIXAR_NUVEM; scrollY = 0; }
                }
                if (gdInputAction > 0) {
                    int iw = 400, ih = 100, ix = (screenWidth - iw) / 2, iy = (screenHeight - ih) / 2;
                    DrawRectangle(ix, iy, iw, ih, (Color){30,35,50,240});
                    DrawRectangleLines(ix, iy, iw, ih, (Color){80,90,120,255});
                    DrawText("Nova pasta:", ix + 10, iy + 10, 14, WHITE);
                    DrawRectangle(ix + 10, iy + 35, iw - 20, 28, (Color){20,25,40,240});
                    DrawRectangleLines(ix + 10, iy + 35, iw - 20, 28, (Color){60,80,120,255});
                    DrawText(TextFormat("%s_", gdInputBuf), ix + 16, iy + 39, 14, (Color){255,220,120,255});
                    Rectangle btnOk = { ix + iw - 180, iy + ih - 35, 70, 25 };
                    Rectangle btnCancel = { ix + iw - 90, iy + ih - 35, 70, 25 };
                    bool hOk = CheckCollisionPointRec(mousePoint, btnOk);
                    bool hCancel = CheckCollisionPointRec(mousePoint, btnCancel);
                    DrawRectangleRec(btnOk, hOk ? (Color){50,160,50,255} : (Color){30,100,40,200});
                    DrawRectangleRec(btnCancel, hCancel ? (Color){160,50,50,255} : (Color){100,30,30,200});
                    DrawText("OK", btnOk.x + 22, btnOk.y + 5, 13, WHITE);
                    DrawText("Cancelar", btnCancel.x + 8, btnCancel.y + 5, 12, WHITE);
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        if (hCancel) { gdInputAction = 0; gdInputBuf[0] = 0; gdRcItem = -1; }
                        else if (hOk && strlen(gdInputBuf) > 0) {
                            gd_criar_pasta(gdInputBuf, gdCurrentPath);
                            gd_listar(gdCurrentPath);
                            gdInputAction = 0; gdInputBuf[0] = 0; gdRcItem = -1;
                        }
                    }
                    if (IsKeyPressed(KEY_ENTER) && strlen(gdInputBuf) > 0) {
                        gd_criar_pasta(gdInputBuf, gdCurrentPath);
                        gd_listar(gdCurrentPath);
                        gdInputAction = 0; gdInputBuf[0] = 0; gdRcItem = -1;
                    }
                    if (IsKeyPressed(KEY_ESCAPE)) { gdInputAction = 0; gdInputBuf[0] = 0; gdRcItem = -1; }
                } else if (gdRcItem >= 0) {
                    int rcIdx = gdRcItem;
                    int nMarks = 0; for (int j = 0; j < gdTotal; j++) if (gdMarcados[j]) nMarks++;
                    const char* sSelTudo = "Selecionar Tudo";
                    const char* sDesTudo = "Desmarcar Tudo";
                    const char* optRc[] = { "Selecionar/Desmarcar", nMarks > 0 ? sDesTudo : sSelTudo, "Baixar", "Copiar Link", "Deletar", "Criar Pasta", "Voltar" };
                    int rcQtd = 7;
                    for (int j = 0; j < rcQtd; j++) {
                        int yPos = 40 + j * 50;
                        if (yPos > -50 && yPos < screenHeight) {
                            Rectangle btn = { 470, yPos, 310, 40 };
                            bool h = CheckCollisionPointRec(mousePoint, btn);
                            DrawRectangleRec(btn, h ? btnHover : btnNormal);
                            Color tc = textLight;
                            if ((j == 3 && gdItems[rcIdx].isDir) || (j == 2 && gdItems[rcIdx].isDir && rcIdx == 0 && strcmp(gdItems[rcIdx].nome, "..") == 0))
                                tc = (Color){100,100,100,255};
                            DrawText(optRc[j], (int)btn.x + 20, (int)btn.y + 10, 16, tc);
                            if (h && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                                if (j == 0) { gdMarcados[rcIdx] = !gdMarcados[rcIdx]; notificar(gdMarcados[rcIdx] ? "Marcado" : "Desmarcado"); gdRcItem = -1; }
                                else if (j == 1) {
                                    bool any = false; for (int k = 0; k < gdTotal; k++) { if (gdMarcados[k]) any = true; break; }
                                    for (int k = 0; k < gdTotal; k++) gdMarcados[k] = !any;
                                    notificar(any ? "Todos desmarcados" : "Todos marcados");
                                    gdRcItem = -1;
                                }
                                else if (j == 2) {
                                    if (nMarks > 0) {
                                        for (int k = 0; k < gdTotal; k++) {
                                            if (gdMarcados[k] && !gdItems[k].isDir) gd_baixar(gdItems[k].id, gdItems[k].nome);
                                        }
                                        notificar("Download dos selecionados iniciado!");
                                    } else if (!gdItems[rcIdx].isDir) {
                                        gd_baixar(gdItems[rcIdx].id, gdItems[rcIdx].nome);
                                        notificar("Download iniciado!");
                                    }
                                    gdRcItem = -1;
                                }
                                else if (j == 3 && !gdItems[rcIdx].isDir) {
                                    char link[1024];
                                    snprintf(link, sizeof(link), "https://drive.google.com/file/d/%s/view", gdItems[rcIdx].id);
                                    char cmd[1100]; sprintf(cmd, "cmd /c start \"\" \"%s\"", link);
                                    exec_async_silent(cmd);
                                    notificar("Link aberto no navegador!");
                                    gdRcItem = -1;
                                }
                                else if (j == 4) {
                                    if (nMarks > 0) {
                                        for (int k = 0; k < gdTotal; k++) if (gdMarcados[k]) gd_deletar(gdItems[k].id);
                                        gd_listar(gdCurrentPath);
                                    } else {
                                        gd_deletar(gdItems[rcIdx].id);
                                        gd_listar(gdCurrentPath);
                                    }
                                    gdRcItem = -1;
                                }
                                else if (j == 5) {
                                    gdInputBuf[0] = 0; gdInputAction = 2;
                                    gdRcItem = -1;
                                }
                                else if (j == 6) { gdRcItem = -1; }
                            }
                        }
                    }
                }
                if (gdInputAction > 0) {
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(mousePoint, (Rectangle){470, 40, 310, 400})) {
                        gdInputAction = 0; gdInputBuf[0] = 0; gdRcItem = -1;
                    }
                }
                if (gdRcItem >= 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(mousePoint, (Rectangle){470, 40, 310, 400})) {
                    gdRcItem = -1;
                }
                if (gdRcItem >= 0 && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && !CheckCollisionPointRec(mousePoint, (Rectangle){470, 40, 310, 400})) {
                    gdRcItem = -1;
                }
            }
        }

        else if (currentState == MENU_BAIXAR_FILMES) {
            DrawText("Filmes / Series (TMDB) - v2", 20, 30, 20, textLight);
            DrawRectangle(20, 60, 520, 34, (Color){20,24,32,255});
            DrawText(tmdbTyping ? TextFormat("Buscar: %s_", tmdbBusca) : TextFormat("Buscar: %s", tmdbBusca), 30, 68, 16, tmdbTyping ? (Color){255,220,120,255} : (Color){200,200,200,255});
            DrawRectangle(550, 60, 90, 34, (Color){50,90,50,255});
            DrawText("BUSCAR", 560, 68, 15, (Color){255,255,255,255});
            DrawText(tmdbStatus, 20, 104, 14, tmdbBaixando ? (Color){255,200,0,255} : (Color){180,220,180,255});
            if (tmdbBaixando) DrawText(TextFormat("%.0f%%", tmdbProgresso * 100.0f), 20, 124, 14, (Color){255,220,120,255});
            int pxL = 20, pwL = 520;
            DrawRectangle(pxL, 150, pwL, screenHeight - 200, (Color){22,24,32,255});
            if (tmdbCapaLoaded) {
                float sc = ((float)(pwL - 40)) / (float)tmdbCapa.width;
                float maxsc = 150.0f / (float)tmdbCapa.height;
                if (sc > maxsc) sc = maxsc;
                if (sc > 0) DrawTextureEx(tmdbCapa, (Vector2){ (float)pxL + 20, 160 }, 0.0f, sc, WHITE);
                if (!tmdbMostraTexto) DrawText("Clique na imagem para ver detalhes", pxL + 20, 320, 13, (Color){150,150,150,255});
            } else {
                DrawText("(capa aparece aqui apos selecionar)", pxL + 20, 170, 14, (Color){120,120,120,255});
            }
            if (tmdbMostraTexto) {
                int by = 318;
                DrawRectangle(20, by, 180, 24, (Color){50,110,50,255});
                DrawText("BAIXAR CONTEUDO", 28, by + 5, 12, (Color){255,255,255,255});
                DrawRectangle(210, by, 170, 24, (Color){90,90,90,255});
                DrawText("BUSCAR TORRENTS", 218, by + 5, 12, (Color){200,200,200,255});

                int dy = by + 34;
                if (tmdbSel >= 0 && tmdbSel < tmdbTotal) {
                    TmdbItem* it = &tmdbResultados[tmdbSel];
                    DrawText(TextFormat("Titulo: %s", it->titulo), pxL + 12, dy, 14, (Color){255,255,255,255}); dy += 18;
                    DrawText(TextFormat("Original: %s", it->tituloOrig), pxL + 12, dy, 13, (Color){180,180,180,255}); dy += 18;
                    DrawText(TextFormat("Data: %s   Tipo: %s", it->data, it->ehSerie ? "Serie" : "Filme"), pxL + 12, dy, 13, (Color){180,180,180,255}); dy += 22;
                }
                int sinoY = dy + 4;
                DrawText("SINOPSE", pxL + 12, sinoY, 12, (Color){255,220,120,255});
                int sinoH = 90;
                int textY = sinoY + 16;
                Rectangle sinoArea = { (float)pxL, (float)textY, (float)pwL, (float)(sinoH - 16) };
                DrawRectangleLinesEx(sinoArea, 1, (Color){80,80,80,255});
                int maxs = tmdb_desenhar_sinopse(sinoArea, 13, &tmdbSinopseScroll);
                if (maxs > 0) {
                    int trackH = (int)sinoArea.height - 4;
                    int barH = trackH * trackH / (trackH + maxs);
                    int barY = (int)sinoArea.y + 2 + (trackH - barH) * tmdbSinopseScroll / maxs;
                    DrawRectangle((float)sinoArea.x + sinoArea.width - 6, (float)barY, 4, barH, (Color){120,120,120,255});
                }
            }

            int pxR = 560, pwR = screenWidth - 580;
            int py = 150, ih = 56;
            for (int i = 0; i < tmdbTotal; i++) {
                int y = py + i * ih - (tmdbSel > 10 ? (tmdbSel - 10) * ih : 0);
                if (y < py - ih || y > screenHeight - 40) continue;
                bool sel = (i == tmdbSel);
                if (sel) DrawRectangle(pxR, y, pwR - 4, ih - 4, (Color){60,80,120,255});
                DrawText(TextFormat("%s %s", tmdbResultados[i].titulo, tmdbResultados[i].ehSerie ? "[SERIE]" : ""), pxR + 10, y + 6, 16, textLight);
                DrawText(TextFormat("Data: %s", tmdbResultados[i].data), pxR + 10, y + 30, 12, (Color){160,160,160,255});
            }
            DrawText("T: buscar | Enter: buscar | B: voltar | L: baixados", 20, screenHeight - 22, 12, (Color){150,150,150,255});
            if (!tmdbBaixando) {
                DrawRectangle(screenWidth - 110, 20, 90, 30, (Color){120,50,50,255});
                DrawText("VOLTAR", screenWidth - 100, 28, 15, (Color){255,255,255,255});
            }
            DrawRectangle(screenWidth - 210, 20, 90, 30, (Color){50,70,110,255});
            DrawText("BAIXADOS", screenWidth - 205, 28, 13, (Color){255,255,255,255});
        }
        else if (currentState == MENU_BAIXAR_FILMES_TORRENT) {
            DrawRectangle(20, 20, 90, 30, (Color){120,50,50,255});
            DrawText("VOLTAR", 30, 28, 15, (Color){255,255,255,255});
            DrawText(TextFormat("Torrents: %s", torrentTermo), 130, 26, 18, textLight);
            DrawText("Provedor:", 20, 62, 14, (Color){200,200,200,255});
            const char* provs[8] = { "YTS", "1337x", "Ambos", "Jackett", "Prowlarr", "Radarr", "Sonarr", "Bazaar" };
            for (int i = 0; i < 8; i++) {
                int bx = 100 + (i % 4) * 165;
                int by = 56 + (i / 4) * 30;
                DrawRectangle(bx, by, 158, 26, (Color){ i == torrentProvedor ? 50 : 30, i == torrentProvedor ? 110 : 40, i == torrentProvedor ? 50 : 55, 255 });
                DrawText(provs[i], bx + 6, by + 6, 12, (Color){255,255,255,255});
            }
            DrawRectangle(screenWidth - 110, 20, 90, 30, (Color){50,90,50,255});
            DrawText("BUSCAR", screenWidth - 100, 28, 14, (Color){255,255,255,255});
            DrawText(torrentStatus, 20, 122, 14, torrentBuscando ? (Color){255,200,0,255} : (Color){180,220,180,255});

            int totalPag = (torrentTotal + TORRENT_POR_PAG - 1) / TORRENT_POR_PAG;
            if (totalPag < 1) totalPag = 1;
            if (torrentPagina >= totalPag) torrentPagina = totalPag - 1;
            if (torrentPagina < 0) torrentPagina = 0;
            int ini = torrentPagina * TORRENT_POR_PAG;
            int fim = ini + TORRENT_POR_PAG; if (fim > torrentTotal) fim = torrentTotal;

            int lx = 20, lw = screenWidth - 40, ly = 152, lh = 30;
            for (int i = ini; i < fim; i++) {
                int y = ly + (i - ini) * lh;
                bool sel = (i == torrentSel);
                if (sel) DrawRectangle(lx, y, lw, lh - 2, (Color){60,80,120,255});
                TorrentItem* t = &torrentResultados[i];
                DrawText(TextFormat("%s", t->nome), lx + 8, y + 4, 13, textLight);
                DrawText(TextFormat("Seeds: %s  Tam: %s  %s", t->seeds, t->tamanho, t->ehYTS ? "[YTS]" : "[1337x]"), lx + 8, y + 18, 11, (Color){160,160,160,255});
            }
            DrawText(TextFormat("Pagina %d/%d  (setas <- -> navegam)", torrentPagina + 1, totalPag), 20, screenHeight - 44, 12, (Color){150,150,150,255});
            DrawText("B/ESC: voltar | Enter: abrir | D: baixar | F: buscar", 20, screenHeight - 22, 12, (Color){150,150,150,255});
            DrawRectangle(screenWidth - 110, screenHeight - 46, 90, 30, (Color){120,50,50,255});
            DrawText("VOLTAR", screenWidth - 100, screenHeight - 40, 13, (Color){255,255,255,255});
        }
        else if (currentState == MENU_BAIXAR_FILMES_BAIXADOS) {
            DrawText("Filmes / Series Baixados", 20, 30, 20, textLight);
            static char baixadosNomes[1000][256];
            static int baixadosTotal = 0;
            tmdb_listar_baixados(baixadosNomes, &baixadosTotal);
            static int baixadosSel = 0;
            int pxL = 20, pwL = 520;
            DrawRectangle(pxL, 80, pwL, screenHeight - 130, (Color){22,24,32,255});
            if (baixadosSel >= 0 && baixadosSel < baixadosTotal) {
                char p[TMDB_PATH_LEN]; sprintf(p, "%s/%s", tmdb_ultimo_caminho_base(), baixadosNomes[baixadosSel]);
                if (tmdbCapaLoaded) { float sc = (pwL - 40) / (float)tmdbCapa.width; float maxsc = (float)(screenHeight - 360) / (float)tmdbCapa.height; if (sc > maxsc) sc = maxsc; if (sc > 0) DrawTextureEx(tmdbCapa, (Vector2){ (float)pxL + 20, 90 }, 0.0f, sc, WHITE); }
            }
            Rectangle sinoArea = { (float)pxL, (float)(screenHeight - 210), (float)pwL, 130 };
            DrawRectangleLinesEx(sinoArea, 1, (Color){80,80,80,255});
            DrawText("SINOPSE", pxL + 12, screenHeight - 206, 12, (Color){255,220,120,255});
            int maxs = tmdb_desenhar_sinopse(sinoArea, 13, &tmdbSinopseScroll);
            if (maxs > 0) {
                int trackH = (int)sinoArea.height - 4;
                int barH = trackH * trackH / (trackH + maxs);
                int barY = (int)sinoArea.y + 2 + (trackH - barH) * tmdbSinopseScroll / maxs;
                DrawRectangle((float)sinoArea.x + sinoArea.width - 6, (float)barY, 4, barH, (Color){120,120,120,255});
            }
            int pxR = 560, pwR = screenWidth - 580;
            int py = 80, ih = 30;
            for (int i = 0; i < baixadosTotal; i++) {
                int y = py + i * ih - (baixadosSel > 18 ? (baixadosSel - 18) * ih : 0);
                if (y < py - ih || y > screenHeight - 40) continue;
                bool sel = (i == baixadosSel);
                if (sel) DrawRectangle(pxR, y, pwR - 4, ih - 2, (Color){60,80,120,255});
                DrawText(baixadosNomes[i], pxR + 10, y + 6, 15, textLight);
            }
            DrawText("X: abrir pasta | B: voltar", 20, screenHeight - 22, 12, (Color){150,150,150,255});
            DrawRectangle(screenWidth - 110, 20, 90, 30, (Color){120,50,50,255});
            DrawText("VOLTAR", screenWidth - 100, 28, 15, (Color){255,255,255,255});
        }

        if ((currentState == MENU_BAIXAR_LINK || currentState == MENU_BAIXAR_NUVEM || currentState == MENU_CONFIGURACOES) && (baixarTyping || dropboxInputAction > 0)) {
            int key = GetCharPressed();
            while (key > 0) {
                int len = (int)strlen(baixarUrlInput);
                if (len < (int)sizeof(baixarUrlInput) - 2 && key >= 32 && key <= 125) {
                    baixarUrlInput[len] = (char)key;
                    baixarUrlInput[len+1] = 0;
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE)) {
                int len = (int)strlen(baixarUrlInput);
                if (len > 0) baixarUrlInput[len-1] = 0;
            }
            if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V)) {
                const char* txt = GetClipboardText();
                if (txt && txt[0]) {
                    int l = (int)strlen(baixarUrlInput);
                    int tl = (int)strlen(txt);
                    for (int c = 0; c < tl && l < (int)sizeof(baixarUrlInput)-2; c++) {
                        if (txt[c] >= 32) baixarUrlInput[l++] = txt[c];
                    }
                    baixarUrlInput[l] = 0;
                }
            }
        }

        if (currentState == MENU_BAIXAR_FILMES && tmdbTyping) {
            int key = GetCharPressed();
            while (key > 0) {
                int len = (int)strlen(tmdbBusca);
                if (len < (int)sizeof(tmdbBusca) - 2 && key >= 32 && key <= 125) {
                    tmdbBusca[len] = (char)key; tmdbBusca[len+1] = 0;
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE)) {
                int len = (int)strlen(tmdbBusca);
                if (len > 0) tmdbBusca[len-1] = 0;
            }
            if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V)) {
                const char* txt = GetClipboardText();
                if (txt && txt[0]) {
                    int l = (int)strlen(tmdbBusca);
                    int tl = (int)strlen(txt);
                    for (int c = 0; c < tl && l < (int)sizeof(tmdbBusca)-2; c++) {
                        if (txt[c] >= 32) tmdbBusca[l++] = txt[c];
                    }
                    tmdbBusca[l] = 0;
                }
            }
            if (IsKeyPressed(KEY_ENTER)) { tmdbTyping = false; if (tmdbBusca[0]) tmdb_buscar(tmdbBusca); }
            if (IsKeyPressed(KEY_ESCAPE)) tmdbTyping = false;
        }

        if (currentState == MENU_BAIXAR_FTP_EDITAR && ftpEditandoCampo >= 0 && ftpServerIdx >= 0 && ftpServerIdx < ftpTotalServers) {
            int key = GetCharPressed();
            while (key > 0) {
                int len = (int)strlen(ftpEditBuf);
                bool ok = true;
                if (ftpEditandoCampo == 2 && !(key >= '0' && key <= '9')) ok = false;
                if (ok && len < (int)sizeof(ftpEditBuf) - 2 && key >= 32 && key <= 125) {
                    ftpEditBuf[len] = (char)key; ftpEditBuf[len+1] = 0;
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE)) { int len = (int)strlen(ftpEditBuf); if (len > 0) ftpEditBuf[len-1] = 0; }
            if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V)) {
                const char* txt = GetClipboardText();
                if (txt && txt[0]) { int l = (int)strlen(ftpEditBuf); int tl = (int)strlen(txt); for (int c = 0; c < tl && l < (int)sizeof(ftpEditBuf)-2; c++) { if (txt[c] >= 32) ftpEditBuf[l++] = txt[c]; } ftpEditBuf[l] = 0; }
            }
            if (IsKeyPressed(KEY_ENTER)) {
                if (ftpEditandoCampo == 0) strcpy(ftpServers[ftpServerIdx].name, ftpEditBuf);
                else if (ftpEditandoCampo == 1) strcpy(ftpServers[ftpServerIdx].ip, ftpEditBuf);
                else if (ftpEditandoCampo == 2) ftpServers[ftpServerIdx].port = atoi(ftpEditBuf);
                else if (ftpEditandoCampo == 3) strcpy(ftpServers[ftpServerIdx].user, ftpEditBuf);
                else if (ftpEditandoCampo == 4) strcpy(ftpServers[ftpServerIdx].pass, ftpEditBuf);
                else if (ftpEditandoCampo == 6) strcpy(ftpServers[ftpServerIdx].ssh_key, ftpEditBuf);
                ftpEditandoCampo = -1;
            }
            if (IsKeyPressed(KEY_ESCAPE)) ftpEditandoCampo = -1;
        }

        if (currentState == MENU_BAIXAR_FTP_GERENCIAR && ftpCriandoPasta) {
            int key = GetCharPressed();
            while (key > 0) {
                int len = (int)strlen(ftpEditBuf);
                if (len < (int)sizeof(ftpEditBuf) - 2 && key >= 32 && key <= 125) { ftpEditBuf[len] = (char)key; ftpEditBuf[len+1] = 0; }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE)) { int len = (int)strlen(ftpEditBuf); if (len > 0) ftpEditBuf[len-1] = 0; }
            if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V)) {
                const char* txt = GetClipboardText();
                if (txt && txt[0]) { int l = (int)strlen(ftpEditBuf); int tl = (int)strlen(txt); for (int c = 0; c < tl && l < (int)sizeof(ftpEditBuf)-2; c++) { if (txt[c] >= 32) ftpEditBuf[l++] = txt[c]; } ftpEditBuf[l] = 0; }
            }
            if (IsKeyPressed(KEY_ENTER)) {
                if (ftpEditBuf[0]) {
                    if (ftpPainelAtivo == 1) ftp_criar_pasta_remota(ftpEditBuf);
                    else ftp_criar_pasta_local(ftpEditBuf);
                }
                ftpCriandoPasta = 0;
            }
            if (IsKeyPressed(KEY_ESCAPE)) ftpCriandoPasta = 0;
        }

        // ---- CONFIG API KEYS ----
        if (currentState == MENU_CONFIG_API) {
            static int apiEditMode = 0; // 0=lista, 1=editando, 2=adicionar (escolher tipo)
            static int apiEditField = -1;
            if (apiKeySel < 0 || apiKeySel >= apiKeyTotal) { apiKeySel = -1; apiEditMode = 0; }
            int px = 470, pw = 310;
            DrawText("Chaves de API", px, 20, 22, textLight);
            if (apiEditMode == 0) {
                DrawText("Lista de chaves:", px, 50, 14, (Color){180,180,180,255});
                for (int i = 0; i < apiKeyTotal; i++) {
                    int by = 75 + i * 50;
                    if (by + 40 < 60 || by > screenHeight) continue;
                    Rectangle r = { px, by, pw, 40 };
                    bool h = CheckCollisionPointRec(mousePoint, r);
                    DrawRectangleRec(r, h ? btnHover : btnNormal);
                    DrawText(apiKeys[i].nome, px + 15, by + 5, 14, WHITE);
                    char prev[80]; snprintf(prev, 80, "%s", apiKeys[i].chave[0] ? (apiKeys[i].desc[0] ? apiKeys[i].desc : "...") : "(vazio)");
                    DrawText(prev, px + 15, by + 22, 11, GRAY);
                    if (h && clicked) { apiKeySel = i; apiEditMode = 1; apiEditField = -1; baixarTyping = false; }
                }
                int by2 = 75 + apiKeyTotal * 50;
                Rectangle btnAdd = { px, by2, pw, 40 };
                bool hAdd = CheckCollisionPointRec(mousePoint, btnAdd);
                DrawRectangleRec(btnAdd, hAdd ? btnHover : btnNormal);
                DrawText("+ ADICIONAR", px + 80, by2 + 10, 15, (Color){150,255,150,255});
                if (hAdd && clicked) { apiEditMode = 2; apiEditField = -1; baixarTyping = false; }
                by2 += 50;
                Rectangle btnVoltar = { px, by2, pw, 40 };
                bool hVolt = CheckCollisionPointRec(mousePoint, btnVoltar);
                DrawRectangleRec(btnVoltar, hVolt ? btnHover : btnNormal);
                DrawText("VOLTAR", px + 110, by2 + 10, 15, textLight);
                if (hVolt && clicked) { currentState = MENU_CONFIGURACOES; scrollY = 0; }
            } else if (apiEditMode == 2) {
                DrawText("Tipo de chave:", px, 50, 14, textLight);
                const char* tipOpts[] = { "Padrao", "Dropbox", "Google Drive", "Gemini", "ChatGPT", "Voltar" };
                for (int t = 0; t < 6; t++) {
                    int ty = 80 + t * 45;
                    Rectangle r = { px, ty, pw, 38 };
                    bool h = CheckCollisionPointRec(mousePoint, r);
                    DrawRectangleRec(r, h ? btnHover : btnNormal);
                    DrawText(tipOpts[t], px + 20, ty + 10, 16, textLight);
                    if (h && clicked) {
                        if (t == 0) {
                            snprintf(apiKeys[apiKeyTotal].nome, 64, "Chave %d", apiKeyTotal+1);
                            apiKeys[apiKeyTotal].desc[0] = 0; apiKeys[apiKeyTotal].chave[0] = 0; apiKeys[apiKeyTotal].app_id[0] = 0;
                            apiKeySel = apiKeyTotal; apiKeyTotal++; apiEditMode = 1; apiEditField = -1;
                        } else if (t == 1) {
                            int dbIdx = -1;
                            for (int k = 0; k < apiKeyTotal; k++) { if (strcmp(apiKeys[k].nome, "Dropbox") == 0) { dbIdx = k; break; } }
                            if (dbIdx < 0 && apiKeyTotal < API_KEY_TOTAL) {
                                strcpy(apiKeys[apiKeyTotal].nome, "Dropbox");
                                strcpy(apiKeys[apiKeyTotal].desc, "API Dropbox (refresh token)");
                                apiKeys[apiKeyTotal].chave[0] = 0; apiKeys[apiKeyTotal].app_id[0] = 0;
                                apiKeySel = apiKeyTotal; apiKeyTotal++;
                            } else if (dbIdx >= 0) { apiKeySel = dbIdx; }
                            apiEditMode = 1; apiEditField = -1;
                            dropboxGenStatus[0] = 0; currentState = MENU_CONFIG_API_GERAR_DROPBOX; scrollY = 0;
                        } else if (t == 2) {
                            int gdIdx = -1;
                            for (int k = 0; k < apiKeyTotal; k++) { if (strcmp(apiKeys[k].nome, "Google Drive") == 0) { gdIdx = k; break; } }
                            if (gdIdx < 0 && apiKeyTotal < API_KEY_TOTAL) {
                                strcpy(apiKeys[apiKeyTotal].nome, "Google Drive");
                                strcpy(apiKeys[apiKeyTotal].desc, "");
                                apiKeys[apiKeyTotal].chave[0] = 0; apiKeys[apiKeyTotal].app_id[0] = 0;
                                apiKeySel = apiKeyTotal; apiKeyTotal++;
                            } else if (gdIdx >= 0) { apiKeySel = gdIdx; }
                            apiEditMode = 1; apiEditField = -1;
                        } else if (t == 3) {
                            int gmIdx = -1;
                            for (int k = 0; k < apiKeyTotal; k++) { if (strcmp(apiKeys[k].nome, "Gemini") == 0) { gmIdx = k; break; } }
                            if (gmIdx < 0 && apiKeyTotal < API_KEY_TOTAL) {
                                strcpy(apiKeys[apiKeyTotal].nome, "Gemini");
                                strcpy(apiKeys[apiKeyTotal].desc, "API Gemini (Google)");
                                apiKeys[apiKeyTotal].chave[0] = 0; apiKeys[apiKeyTotal].app_id[0] = 0;
                                apiKeySel = apiKeyTotal; apiKeyTotal++;
                            } else if (gmIdx >= 0) { apiKeySel = gmIdx; }
                            apiEditMode = 1; apiEditField = -1;
                        } else if (t == 4) {
                            int cgIdx = -1;
                            for (int k = 0; k < apiKeyTotal; k++) { if (strcmp(apiKeys[k].nome, "ChatGPT") == 0) { cgIdx = k; break; } }
                            if (cgIdx < 0 && apiKeyTotal < API_KEY_TOTAL) {
                                strcpy(apiKeys[apiKeyTotal].nome, "ChatGPT");
                                strcpy(apiKeys[apiKeyTotal].desc, "API OpenAI ChatGPT");
                                apiKeys[apiKeyTotal].chave[0] = 0; apiKeys[apiKeyTotal].app_id[0] = 0;
                                apiKeySel = apiKeyTotal; apiKeyTotal++;
                            } else if (cgIdx >= 0) { apiKeySel = cgIdx; }
                            apiEditMode = 1; apiEditField = -1;
                        } else { apiEditMode = 0; }
                    }
                }
            } else if (apiKeySel >= 0 && apiKeySel < apiKeyTotal) {
                DrawText(TextFormat("Editando: %s", apiKeys[apiKeySel].nome), px, 20, 16, textLight);
                char vals[4][300];
                snprintf(vals[0], 300, "Nome: %s", apiKeys[apiKeySel].nome);
                snprintf(vals[1], 300, "Desc: %s", apiKeys[apiKeySel].desc[0] ? apiKeys[apiKeySel].desc : "(vazio)");
                snprintf(vals[2], 300, "App ID: %s", apiKeys[apiKeySel].app_id[0] ? apiKeys[apiKeySel].app_id : "(vazio)");
                snprintf(vals[3], 300, "Chave: %s", apiKeys[apiKeySel].chave[0] ? "****" : "(vazio)");
                for (int f = 0; f < 4; f++) {
                    int fy = 50 + f * 45;
                    Rectangle r = { px, fy, pw, 38 };
                    bool hovF = CheckCollisionPointRec(mousePoint, r);
                    Color bg = (f == apiEditField && baixarTyping) ? (Color){60,80,60,255} : hovF ? btnHover : btnNormal;
                    DrawRectangleRec(r, bg);
                    if (f == apiEditField && baixarTyping)
                        DrawText(TextFormat("%s_", baixarUrlInput), px + 15, fy + 9, 14, (Color){255,220,120,255});
                    else
                        DrawText(vals[f], px + 15, fy + 9, 14, textLight);
                    if (hovF && clicked && f != apiEditField) {
                        apiEditField = f; baixarUrlInput[0] = 0; baixarTyping = true;
                        if (f == 0) strcpy(baixarUrlInput, apiKeys[apiKeySel].nome);
                        else if (f == 1) strcpy(baixarUrlInput, apiKeys[apiKeySel].desc);
                        else if (f == 2) strcpy(baixarUrlInput, apiKeys[apiKeySel].app_id);
                        else if (f == 3) strcpy(baixarUrlInput, apiKeys[apiKeySel].chave);
                    }
                }
                if (apiEditField >= 0 && baixarTyping && IsKeyPressed(KEY_ENTER)) {
                    if (apiEditField == 0) strncpy(apiKeys[apiKeySel].nome, baixarUrlInput, 63);
                    else if (apiEditField == 1) strncpy(apiKeys[apiKeySel].desc, baixarUrlInput, 255);
                    else if (apiEditField == 2) strncpy(apiKeys[apiKeySel].app_id, baixarUrlInput, 255);
                    else if (apiEditField == 3) strncpy(apiKeys[apiKeySel].chave, baixarUrlInput, 511);
                    apiEditField = -1; baixarTyping = false; baixarUrlInput[0] = 0;
                }
                if (apiEditField >= 0 && baixarTyping && IsKeyPressed(KEY_ESCAPE)) {
                    apiEditField = -1; baixarTyping = false; baixarUrlInput[0] = 0;
                }
                int by2 = 50 + 4 * 45 + 10;
                Rectangle btnSave = { px, by2, pw, 34 };
                bool hSave = CheckCollisionPointRec(mousePoint, btnSave);
                DrawRectangleRec(btnSave, hSave ? (Color){50,130,50,255} : (Color){30,90,30,200});
                DrawText("SALVAR", px + 115, by2 + 8, 14, WHITE);
                if (hSave && clicked) { 
                    if (apiEditField >= 0 && baixarUrlInput[0]) {
                        if (apiEditField == 0) strncpy(apiKeys[apiKeySel].nome, baixarUrlInput, 63);
                        else if (apiEditField == 1) strncpy(apiKeys[apiKeySel].desc, baixarUrlInput, 255);
                        else if (apiEditField == 2) strncpy(apiKeys[apiKeySel].app_id, baixarUrlInput, 255);
                        else if (apiEditField == 3) strncpy(apiKeys[apiKeySel].chave, baixarUrlInput, 511);
                        apiEditField = -1; baixarTyping = false; baixarUrlInput[0] = 0;
                    }
                    apiKeys_salvar_raw(); notificar("Chave salva!"); apiEditMode = 0; 
                }
                by2 += 40;
                Rectangle btnDel2 = { px, by2, pw, 34 };
                bool hDel2 = CheckCollisionPointRec(mousePoint, btnDel2);
                DrawRectangleRec(btnDel2, hDel2 ? (Color){130,50,50,255} : (Color){90,30,30,200});
                DrawText("DELETAR", px + 113, by2 + 8, 14, WHITE);
                if (hDel2 && clicked) {
                    for (int j = apiKeySel; j < apiKeyTotal-1; j++) apiKeys[j] = apiKeys[j+1];
                    apiKeyTotal--; if (apiKeySel >= apiKeyTotal) apiKeySel = apiKeyTotal - 1;
                    apiKeys_salvar_raw(); notificar("Chave deletada"); apiEditMode = 0;
                }
                by2 += 40;
                Rectangle btnGenDb = { px, by2, pw, 34 };
                bool hGenDb = CheckCollisionPointRec(mousePoint, btnGenDb);
                DrawRectangleRec(btnGenDb, hGenDb ? btnHover : btnNormal);
                DrawText("GERAR TOKEN DROPBOX", px + 60, by2 + 8, 13, WHITE);
                if (hGenDb && clicked) { dropboxGenStatus[0] = 0; currentState = MENU_CONFIG_API_GERAR_DROPBOX; scrollY = 0; }
                by2 += 40;
                Rectangle btnVolt = { px, by2, pw, 34 };
                bool hVolt2 = CheckCollisionPointRec(mousePoint, btnVolt);
                DrawRectangleRec(btnVolt, hVolt2 ? btnHover : btnNormal);
                DrawText("VOLTAR LISTA", px + 95, by2 + 8, 14, textLight);
                if (hVolt2 && clicked) { apiEditMode = 0; apiEditField = -1; baixarTyping = false; }
            }
        }

        // ---- DROPBOX TOKEN GENERATOR ----
        if (currentState == MENU_CONFIG_API_GERAR_DROPBOX) {
            DrawText("Gerar Token Permanente Dropbox", 50, 20, 18, textLight);

            static int dropboxTab = 0;
            const char* tabs[] = { "AUTO", "AVANCADO", "XML", "MANUAL" };
            int tabX = 50;
            for (int t = 0; t < 4; t++) {
                Rectangle rTab = { (float)tabX, 45, 100, 28 };
                bool hTab = CheckCollisionPointRec(mousePoint, rTab);
                bool ativo = (t == dropboxTab);
                DrawRectangleRec(rTab, ativo ? (Color){0,120,200,255} : hTab ? (Color){40,50,70,255} : (Color){20,30,50,200});
                DrawRectangleLinesEx(rTab, 1, ativo ? (Color){100,200,255,255} : (Color){50,60,80,255});
                DrawText(tabs[t], rTab.x + 20, rTab.y + 6, 13, ativo ? WHITE : (Color){180,180,180,255});
                if (hTab && clicked) dropboxTab = t;
                tabX += 105;
            }

            static char dropboxAuthCode[512] = "";
            static bool dropboxEditAtivo = false;
            static bool dropboxEditKey = false;
            static bool dropboxEditSec = false;
            static char dropboxAdvKey[256] = "";
            static char dropboxAdvSec[256] = "";
            static char dropboxRefreshDisplay[1024] = "";
            static bool dropboxTokenGerado = false;
            static bool dropboxAllSelected = false;
            const char* appKeyPadrao = DROPBOX_DEFAULT_APPKEY;
            const char* appSecPadrao = DROPBOX_DEFAULT_APPSECRET;

            int curY = 90;
            char url[2048];

            if (dropboxTab == 0) {
                DrawText("Modo Auto (chaves padrao)", 50, curY, 14, WHITE);
                DrawText(TextFormat("App Key: %s", appKeyPadrao), 50, curY+20, 12, (Color){180,180,180,255});
                DrawText(TextFormat("App Secret: %s", appSecPadrao), 50, curY+35, 12, (Color){180,180,180,255});

                Rectangle btnAuth = { 50, curY+55, 280, 35 };
                bool hAuth = CheckCollisionPointRec(mousePoint, btnAuth);
                DrawRectangleRec(btnAuth, hAuth ? (Color){50,120,200,255} : (Color){30,60,100,200});
                DrawRectangleLinesEx(btnAuth, 2, hAuth ? WHITE : (Color){50,80,130,255});
                DrawText("1. AUTORIZAR (abre navegador)", btnAuth.x+20, btnAuth.y+9, 12, WHITE);
                if (hAuth && clicked) {
                    strcpy(dropboxGenStatus, "Abrindo no navegador aguarde...");
                    sprintf(url, "https://www.dropbox.com/oauth2/authorize?client_id=%s&token_access_type=offline&response_type=code", appKeyPadrao);
                    char cmd[2100]; sprintf(cmd, "cmd /c start \"\" \"%s\"", url);
                    exec_async_silent(cmd);
                }

                DrawText("Codigo de Autorizacao:", 50, curY+105, 13, WHITE);
                Rectangle rCode = { 50, curY+125, 500, 35 };
                DrawRectangleRec(rCode, (Color){20,30,50,240});
                DrawRectangleLinesEx(rCode, 1, (Color){60,80,120,255});
                DrawText(TextFormat("%s%s", dropboxAuthCode, dropboxEditAtivo ? "_" : ""), rCode.x+8, rCode.y+8, 14, WHITE);
                if (CheckCollisionPointRec(mousePoint, rCode) && clicked) { dropboxEditAtivo = true; dropboxEditKey = false; dropboxEditSec = false; dropboxAllSelected = false; }

                Rectangle btnExch = { 50, curY+170, 280, 35 };
                bool hExch = CheckCollisionPointRec(mousePoint, btnExch);
                DrawRectangleRec(btnExch, hExch ? (Color){50,200,50,255} : (Color){30,100,40,200});
                DrawRectangleLinesEx(btnExch, 2, hExch ? WHITE : (Color){50,150,60,255});
                DrawText("2. GERAR TOKEN PERMANENTE", btnExch.x+25, btnExch.y+9, 13, WHITE);
                if (hExch && clicked && strlen(dropboxAuthCode) > 0) {
                    dropboxTokenGerado = false; dropboxRefreshDisplay[0] = 0;
                    int dbIdx = -1;
                    for (int z = 0; z < apiKeyTotal; z++) { if (strcmp(apiKeys[z].nome, "Dropbox") == 0) { dbIdx = z; break; } }
                    if (dbIdx < 0 && apiKeyTotal < API_KEY_TOTAL) { dbIdx = apiKeyTotal; strcpy(apiKeys[dbIdx].nome, "Dropbox"); strcpy(apiKeys[dbIdx].desc, "API Dropbox (refresh token)"); apiKeyTotal++; }
                    if (dbIdx >= 0) {
                        dropbox_exchange(dropboxAuthCode, appKeyPadrao, appSecPadrao, apiKeys[dbIdx].chave, 511);
                        if (apiKeys[dbIdx].chave[0]) {
                            apiKeys_salvar_raw();
                            strncpy(dropboxRefreshDisplay, apiKeys[dbIdx].chave, 1023);
                            dropboxTokenGerado = true;
                            _mkdir("data/HyperNeiva/configuracao");
                            char cwd[1024]; _getcwd(cwd, 1024);
                            char txtPath[1024]; snprintf(txtPath, sizeof(txtPath), "%s/data/HyperNeiva/configuracao/dropbox_token.txt", cwd);
                            FILE* ft = fopen(txtPath, "w");
                            if (ft) { fprintf(ft, "%s", dropboxRefreshDisplay); fclose(ft); sprintf(dropboxGenStatus + strlen(dropboxGenStatus), " + salvo em disco"); }
                            else { sprintf(dropboxGenStatus + strlen(dropboxGenStatus), " + ERRO ao salvar arquivo!"); }
                        }
                    }
                }
            } else if (dropboxTab == 1) {
                DrawText("Modo Avancado (suas chaves)", 50, curY, 14, WHITE);
                curY += 25;
                DrawText("App Key:", 50, curY, 13, WHITE);
                Rectangle rKey = { 150, curY-3, 350, 28 };
                DrawRectangleRec(rKey, (Color){20,30,50,240});
                DrawRectangleLinesEx(rKey, 1, (Color){60,80,120,255});
                DrawText(TextFormat("%s%s", dropboxAdvKey, dropboxEditKey ? "_" : ""), rKey.x+6, rKey.y+6, 13, WHITE);
                if (CheckCollisionPointRec(mousePoint, rKey) && clicked) { dropboxEditKey = true; dropboxEditSec = false; dropboxEditAtivo = false; dropboxAllSelected = false; }
                curY += 35;
                DrawText("App Secret:", 50, curY, 13, WHITE);
                Rectangle rSec = { 150, curY-3, 350, 28 };
                DrawRectangleRec(rSec, (Color){20,30,50,240});
                DrawRectangleLinesEx(rSec, 1, (Color){60,80,120,255});
                DrawText(TextFormat("%s%s", dropboxAdvSec, dropboxEditSec ? "_" : ""), rSec.x+6, rSec.y+6, 13, WHITE);
                if (CheckCollisionPointRec(mousePoint, rSec) && clicked) { dropboxEditSec = true; dropboxEditKey = false; dropboxEditAtivo = false; dropboxAllSelected = false; }
                curY += 35;
                Rectangle btnAuthAdv = { 50, curY, 280, 35 };
                bool hAuthAdv = CheckCollisionPointRec(mousePoint, btnAuthAdv);
                DrawRectangleRec(btnAuthAdv, hAuthAdv ? (Color){50,120,200,255} : (Color){30,60,100,200});
                DrawRectangleLinesEx(btnAuthAdv, 2, hAuthAdv ? WHITE : (Color){50,80,130,255});
                DrawText("1. AUTORIZAR COM MINHA CHAVE", btnAuthAdv.x+15, btnAuthAdv.y+9, 12, WHITE);
                if (hAuthAdv && clicked) {
                    strcpy(dropboxGenStatus, "Abrindo no navegador aguarde...");
                    const char* k = dropboxAdvKey[0] ? dropboxAdvKey : appKeyPadrao;
                    sprintf(url, "https://www.dropbox.com/oauth2/authorize?client_id=%s&token_access_type=offline&response_type=code", k);
                    char cmd[2100]; sprintf(cmd, "cmd /c start \"\" \"%s\"", url);
                    exec_async_silent(cmd);
                }
                curY += 45;
                DrawText("Codigo de Autorizacao:", 50, curY, 13, WHITE);
                curY += 20;
                Rectangle rCodeAdv = { 50, curY, 500, 35 };
                DrawRectangleRec(rCodeAdv, (Color){20,30,50,240});
                DrawRectangleLinesEx(rCodeAdv, 1, (Color){60,80,120,255});
                DrawText(TextFormat("%s%s", dropboxAuthCode, dropboxEditAtivo ? "_" : ""), rCodeAdv.x+8, rCodeAdv.y+8, 14, WHITE);
                if (CheckCollisionPointRec(mousePoint, rCodeAdv) && clicked) { dropboxEditAtivo = true; dropboxEditKey = false; dropboxEditSec = false; }
                curY += 45;
                Rectangle btnExchAdv = { 50, curY, 280, 35 };
                bool hExchAdv = CheckCollisionPointRec(mousePoint, btnExchAdv);
                DrawRectangleRec(btnExchAdv, hExchAdv ? (Color){50,200,50,255} : (Color){30,100,40,200});
                DrawRectangleLinesEx(btnExchAdv, 2, hExchAdv ? WHITE : (Color){50,150,60,255});
                DrawText("2. CONECTAR PERMANENTE", btnExchAdv.x+30, btnExchAdv.y+9, 13, WHITE);
                if (hExchAdv && clicked && strlen(dropboxAuthCode) > 0) {
                    dropboxTokenGerado = false; dropboxRefreshDisplay[0] = 0;
                    const char* k = dropboxAdvKey[0] ? dropboxAdvKey : appKeyPadrao;
                    const char* s = dropboxAdvSec[0] ? dropboxAdvSec : appSecPadrao;
                    int dbIdx = -1;
                    for (int z = 0; z < apiKeyTotal; z++) { if (strcmp(apiKeys[z].nome, "Dropbox") == 0) { dbIdx = z; break; } }
                    if (dbIdx < 0 && apiKeyTotal < API_KEY_TOTAL) { dbIdx = apiKeyTotal; strcpy(apiKeys[dbIdx].nome, "Dropbox"); strcpy(apiKeys[dbIdx].desc, "API Dropbox (refresh token)"); apiKeyTotal++; }
                    if (dbIdx >= 0) {
                        dropbox_exchange(dropboxAuthCode, k, s, apiKeys[dbIdx].chave, 511);
                        if (apiKeys[dbIdx].chave[0]) {
                            if (dropboxAdvKey[0]) strncpy(apiKeys[dbIdx].app_id, dropboxAdvKey, 255);
                            apiKeys_salvar_raw();
                            strncpy(dropboxRefreshDisplay, apiKeys[dbIdx].chave, 1023);
                            dropboxTokenGerado = true;
                            _mkdir("data/HyperNeiva/configuracao");
                            char cwd[1024]; _getcwd(cwd, 1024);
                            char txtPath[1024]; snprintf(txtPath, sizeof(txtPath), "%s/data/HyperNeiva/configuracao/dropbox_token.txt", cwd);
                            FILE* ft = fopen(txtPath, "w");
                            if (ft) { fprintf(ft, "%s", dropboxRefreshDisplay); fclose(ft); sprintf(dropboxGenStatus + strlen(dropboxGenStatus), " + salvo em disco"); }
                            else { sprintf(dropboxGenStatus + strlen(dropboxGenStatus), " + ERRO ao salvar arquivo!"); }
                        }
                    }
                }
            } else if (dropboxTab == 2) {
                DrawText("Modulo XML Carregado", 50, curY+30, 14, (Color){150,150,150,255});
            } else {
                DrawText("Modo Manual (Access Token temporario ~4h)", 50, curY, 14, WHITE);
                curY += 25;
                DrawText("Access Token:", 50, curY, 13, WHITE);
                curY += 20;
                Rectangle rMan = { 50, curY, 500, 60 };
                DrawRectangleRec(rMan, (Color){20,30,50,240});
                DrawRectangleLinesEx(rMan, 1, (Color){60,80,120,255});
                DrawText(TextFormat("%s%s", dropboxAuthCode, dropboxEditAtivo ? "_" : ""), rMan.x+8, rMan.y+8, 13, (Color){200,200,200,255});
                if (CheckCollisionPointRec(mousePoint, rMan) && clicked) { dropboxEditAtivo = true; dropboxEditKey = false; dropboxEditSec = false; }
                curY += 70;
                Rectangle btnMan = { 50, curY, 250, 35 };
                bool hMan = CheckCollisionPointRec(mousePoint, btnMan);
                DrawRectangleRec(btnMan, hMan ? (Color){50,200,50,255} : (Color){30,100,40,200});
                DrawRectangleLinesEx(btnMan, 2, hMan ? WHITE : (Color){50,150,60,255});
                DrawText("ACESSO TEMPORARIO", btnMan.x+35, btnMan.y+9, 13, WHITE);
                if (hMan && clicked && strlen(dropboxAuthCode) > 0) {
                    int dbIdx = -1;
                    for (int z = 0; z < apiKeyTotal; z++) { if (strcmp(apiKeys[z].nome, "Dropbox") == 0) { dbIdx = z; break; } }
                    if (dbIdx < 0 && apiKeyTotal < API_KEY_TOTAL) { dbIdx = apiKeyTotal; strcpy(apiKeys[dbIdx].nome, "Dropbox"); strcpy(apiKeys[dbIdx].desc, "API Dropbox (refresh token)"); apiKeyTotal++; }
                    if (dbIdx >= 0) {
                        strncpy(apiKeys[dbIdx].chave, dropboxAuthCode, 511);
                        apiKeys_salvar_raw();
                        strncpy(dropboxRefreshDisplay, "Token manual salvo!", 1023);
                        dropboxTokenGerado = true;
                        char txtPath[1024]; sprintf(txtPath, "data/HyperNeiva/configuracao/dropbox_token.txt");
                        FILE* ft = fopen(txtPath, "w");
                        if (ft) { fprintf(ft, "%s", dropboxAuthCode); fclose(ft); }
                    }
                }
            }

            // ---- KEYBOARD INPUT ----
            if (dropboxEditAtivo) {
                int key = GetCharPressed();
                while (key > 0) {
                    int len = (int)strlen(dropboxAuthCode);
                    if (dropboxAllSelected) { dropboxAuthCode[0] = 0; dropboxAllSelected = false; len = 0; }
                    if (len < 510 && key >= 32 && key <= 126) { dropboxAuthCode[len] = (char)key; dropboxAuthCode[len+1] = 0; }
                    key = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    if (dropboxAllSelected) { dropboxAuthCode[0] = 0; dropboxAllSelected = false; }
                    else { int l = (int)strlen(dropboxAuthCode); if (l > 0) dropboxAuthCode[l-1] = 0; }
                }
                if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_A)) dropboxAllSelected = true;
                input_clipboard(dropboxAuthCode, 510, &dropboxAllSelected);
            } else if (dropboxEditKey) {
                int key = GetCharPressed();
                while (key > 0) {
                    int len = (int)strlen(dropboxAdvKey);
                    if (dropboxAllSelected) { dropboxAdvKey[0] = 0; dropboxAllSelected = false; len = 0; }
                    if (len < 254 && key >= 32 && key <= 126) { dropboxAdvKey[len] = (char)key; dropboxAdvKey[len+1] = 0; }
                    key = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    if (dropboxAllSelected) { dropboxAdvKey[0] = 0; dropboxAllSelected = false; }
                    else { int l = (int)strlen(dropboxAdvKey); if (l > 0) dropboxAdvKey[l-1] = 0; }
                }
                if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_A)) dropboxAllSelected = true;
                input_clipboard(dropboxAdvKey, 254, &dropboxAllSelected);
            } else if (dropboxEditSec) {
                int key = GetCharPressed();
                while (key > 0) {
                    int len = (int)strlen(dropboxAdvSec);
                    if (dropboxAllSelected) { dropboxAdvSec[0] = 0; dropboxAllSelected = false; len = 0; }
                    if (len < 254 && key >= 32 && key <= 126) { dropboxAdvSec[len] = (char)key; dropboxAdvSec[len+1] = 0; }
                    key = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    if (dropboxAllSelected) { dropboxAdvSec[0] = 0; dropboxAllSelected = false; }
                    else { int l = (int)strlen(dropboxAdvSec); if (l > 0) dropboxAdvSec[l-1] = 0; }
                }
                if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_A)) dropboxAllSelected = true;
                input_clipboard(dropboxAdvSec, 254, &dropboxAllSelected);
            }

            // ---- STATUS ----
            if (dropboxGenStatus[0] && dropboxTab != 2) {
                Color sc = (strncmp(dropboxGenStatus, "OK", 2) == 0) ? (Color){0,255,150,255} : (strncmp(dropboxGenStatus, "Erro", 4) == 0) ? (Color){255,100,100,255} : (Color){255,220,120,255};
                DrawText(dropboxGenStatus, 50, 300, 13, sc);
            }

            // ---- SUCCESS ----
            if (dropboxTokenGerado && dropboxTab != 2) {
                curY = 320;
                DrawRectangle(40, curY-10, 560, 130, (Color){0,180,80,30});
                DrawRectangleLines(40, curY-10, 560, 130, (Color){0,200,100,80});
                DrawText("REFRESH TOKEN GERADO:", 50, curY, 12, (Color){0,255,150,255});
                curY += 20;
                Rectangle rDisplay = { 50, curY, 540, 40 };
                DrawRectangleRec(rDisplay, (Color){0,0,0,120});
                DrawRectangleLinesEx(rDisplay, 1, (Color){0,200,100,100});
                const char* displayTxt = dropboxRefreshDisplay;
                int tw = MeasureText(displayTxt, 11);
                if (tw > 530) {
                    static float scrollOff = 0;
                    scrollOff += GetFrameTime() * 30;
                    if (scrollOff > tw + 50) scrollOff = -530;
                    BeginScissorMode(55, curY+5, 530, 30);
                    DrawText(displayTxt, (int)(55 - scrollOff), curY+11, 11, (Color){0,255,150,255});
                    EndScissorMode();
                } else {
                    DrawText(displayTxt, 55, curY+11, 11, (Color){0,255,150,255});
                }
                curY += 50;
                Rectangle btnCopy = { 50, curY, 120, 30 };
                bool hCopy = CheckCollisionPointRec(mousePoint, btnCopy);
                DrawRectangleRec(btnCopy, hCopy ? (Color){60,180,255,255} : (Color){30,80,140,200});
                DrawRectangleLinesEx(btnCopy, 1, hCopy ? WHITE : (Color){50,100,160,255});
                DrawText("COPIAR", btnCopy.x+28, btnCopy.y+7, 12, WHITE);
                if (hCopy && clicked) SetClipboardText(dropboxRefreshDisplay);

                Rectangle btnDown = { 180, curY, 140, 30 };
                bool hDown = CheckCollisionPointRec(mousePoint, btnDown);
                DrawRectangleRec(btnDown, hDown ? (Color){60,180,255,255} : (Color){30,80,140,200});
                DrawRectangleLinesEx(btnDown, 1, hDown ? WHITE : (Color){50,100,160,255});
                DrawText("BAIXAR .TXT", btnDown.x+24, btnDown.y+7, 12, WHITE);
                if (hDown && clicked) {
                    char outPath[1024];
                    sprintf(outPath, "%s\\dropbox_refresh_token.txt", "data/HyperNeiva");
                    FILE* f = fopen(outPath, "w");
                    if (f) { fprintf(f, "%s", dropboxRefreshDisplay); fclose(f); }
                }

                Rectangle btnExpl = { 330, curY, 160, 30 };
                bool hExpl = CheckCollisionPointRec(mousePoint, btnExpl);
                DrawRectangleRec(btnExpl, hExpl ? (Color){60,180,255,255} : (Color){30,80,140,200});
                DrawRectangleLinesEx(btnExpl, 1, hExpl ? WHITE : (Color){50,100,160,255});
                DrawText("ABRIR EXPLORADOR", btnExpl.x+12, btnExpl.y+7, 12, WHITE);
                if (hExpl && clicked) exec_async_silent("cmd /c start data\\HyperNeiva");
            }

            Rectangle btnCloseGen = { 50, 560, 100, 30 };
            bool hCloseGen = CheckCollisionPointRec(mousePoint, btnCloseGen);
            DrawRectangleRec(btnCloseGen, hCloseGen ? btnHover : btnNormal);
            DrawText("Voltar", 70, 568, 14, textLight);
            if (hCloseGen && clicked) { currentState = MENU_CONFIG_API; scrollY = 0; dropboxGenStatus[0] = 0; dropboxTokenGerado = false; dropboxAuthCode[0] = 0; }
        }

        int totalH = qtd * espacamento;
        int maxVisH = screenHeight - 80;
        int minScroll = maxVisH - totalH;
        if (minScroll > 0) minScroll = 0;

        bool hasScroll = (minScroll < 0);
        if (hasScroll) {
            Rectangle btnUp = { 760, 40, 20, 20 };
            Rectangle btnDown = { 760, screenHeight - 60, 20, 20 };
            
            DrawRectangleRec(btnUp, CheckCollisionPointRec(mousePoint, btnUp) ? btnHover : btnNormal);
            Vector2 u1 = { btnUp.x + 4, btnUp.y + 16 }; Vector2 u2 = { btnUp.x + 16, btnUp.y + 16 }; Vector2 u3 = { btnUp.x + 10, btnUp.y + 4 };
            DrawTriangle(u1, u2, u3, textLight);

            DrawRectangleRec(btnDown, CheckCollisionPointRec(mousePoint, btnDown) ? btnHover : btnNormal);
            Vector2 d1 = { btnDown.x + 4, btnDown.y + 4 }; Vector2 d2 = { btnDown.x + 10, btnDown.y + 16 }; Vector2 d3 = { btnDown.x + 16, btnDown.y + 4 };
            DrawTriangle(d1, d2, d3, textLight);
            
            if (CheckCollisionPointRec(mousePoint, btnUp) && holding) scrollY += 5;
            if (CheckCollisionPointRec(mousePoint, btnDown) && holding) scrollY -= 5;
            
            Rectangle scrollTrack = { 760, 60, 20, screenHeight - 120 };
            DrawRectangleRec(scrollTrack, panelDark);
            
            float proportion = (float)(-scrollY) / (float)(-minScroll);
            float thumbHeight = ((float)maxVisH / (float)totalH) * scrollTrack.height;
            if (thumbHeight < 20) thumbHeight = 20;
            
            float thumbY = scrollTrack.y + proportion * (scrollTrack.height - thumbHeight);
            Rectangle scrollThumb = { 760, thumbY, 20, thumbHeight };
            
            if (CheckCollisionPointRec(mousePoint, scrollTrack) && holding) {
                float clickY = mousePoint.y - scrollTrack.y - (thumbHeight / 2);
                float maxThumbY = scrollTrack.height - thumbHeight;
                if (maxThumbY > 0) {
                    float clickProp = clickY / maxThumbY;
                    if (clickProp < 0) clickProp = 0;
                    if (clickProp > 1) clickProp = 1;
                    scrollY = -(int)(clickProp * -minScroll);
                }
            }
            DrawRectangleRec(scrollThumb, textLight);
        }

        if (wheelMove != 0 && hasScroll) scrollY += (int)(wheelMove * 30);
        if (scrollY > 0) scrollY = 0;
        if (scrollY < minScroll) scrollY = minScroll;

        if (mousePoint.x < 450) {
            lastHoveredRadio = -1;
        }

        for (int i = 0; i < qtd; i++) {
            int yPos = 40 + (i * espacamento) + scrollY;
            if (yPos > -50 && yPos < screenHeight) {
                Rectangle btn = { 470, yPos, (hasScroll ? 280 : 310), (float)alturaBtn };
                bool hovered = CheckCollisionPointRec(mousePoint, btn);
                
                if (hovered && currentState == MENU_MIDIA_RADIO_LISTA) {
                    lastHoveredRadio = i;
                }
                
                if (hovered && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                    if (currentState == MENU_MIDIA_ARQUIVOS && i > 2) {
                        int realIdx = -1;
                        for (int j = 0; j < qtdArquivos; j++) { if (opcoes[i] == listaArquivosNomes[j]) { realIdx = j; break; } }
                        if (realIdx != -1) {
                            strcpy(targetFilePath, listaArquivosCaminhos[realIdx]);
                            currentState = MENU_ARQUIVOS_OPCOES_ARQUIVO;
                            scrollY = 0;
                        }
                    }
                }
                
                if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    pressedItemIndex = i;
                    holdTime = 0.0f;
                }
                
                if (pressedItemIndex == i && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                    if (hovered) {
                        holdTime += GetFrameTime();
                        if (holdTime >= 0.6f && !longPressTriggered) {
                            longPressTriggered = true;
                            if (currentState == MENU_MIDIA_ARQUIVOS && i > 2) {
                                int realIdx = -1;
                                for (int j = 0; j < qtdArquivos; j++) { if (opcoes[i] == listaArquivosNomes[j]) { realIdx = j; break; } }
                                if (realIdx != -1) {
                                    strcpy(targetFilePath, listaArquivosCaminhos[realIdx]);
                                    currentState = MENU_ARQUIVOS_OPCOES_ARQUIVO;
                                    scrollY = 0;
                                }
                            }
                        }
                    } else {
                        pressedItemIndex = -1;
                    }
                }
                
                if (hovered && clicked) {
                    if (currentState == MENU_MAIN) {
                        scrollY = 0;
                        if (i == 0) currentState = MENU_JOGAR; else if (i == 1) currentState = MENU_MIDIA;
                        else if (i == 2) currentState = MENU_BAIXAR; else if (i == 3) currentState = MENU_EDITAR;
                        else if (i == 4) currentState = MENU_EXPLORAR; else if (i == 5) currentState = MENU_EXTRA;
                        else if (i == 6) { initFerramentas(); ferramentasViaAbaPrincipal = true; currentState = MENU_FERRAMENTAS; }
                        else if (i == 7) currentState = MENU_CONFIGURACOES; else if (i == 8) currentState = MENU_INFORMACOES;
                    } else if (currentState == MENU_EXPLORAR) {
                        if (i == 0) { currentState = MENU_MAIN; scrollY = 0; }
                        else if (i == 1) { strcpy(caminhoArquivos, "data/HyperNeiva"); lerPastaArquivos(); currentState = MENU_MIDIA_ARQUIVOS; scrollY = 0; }
                        else if (i == 2) { char* u = getenv("USERPROFILE"); if (u) { sprintf(caminhoArquivos, "%s/Downloads", u); lerPastaArquivos(); currentState = MENU_MIDIA_ARQUIVOS; scrollY = 0; } }
                        else if (i == 3) { char* u = getenv("USERPROFILE"); if (u) { sprintf(caminhoArquivos, "%s/Documents", u); lerPastaArquivos(); currentState = MENU_MIDIA_ARQUIVOS; scrollY = 0; } }
                        else {
                            char drive[4];
                            drive[0] = opcoes[i][6]; 
                            drive[1] = ':';
                            drive[2] = '/';
                            drive[3] = '\0';
                            strcpy(caminhoArquivos, drive);
                            lerPastaArquivos();
                            currentState = MENU_MIDIA_ARQUIVOS; scrollY = 0;
                        }
                    } else if (currentState == MENU_MIDIA) {
                        scrollY = 0;
                        if (i == 0) currentState = MENU_MAIN;
                        else if (i == 1) currentState = MENU_MIDIA_RADIO_CATEGORIA;
                        else if (i == 2) { strcpy(caminhoMusicas, "data/HyperNeiva/midia/musicas"); lerPastaMusicas(); currentState = MENU_MIDIA_MUSICAS; }
                        else if (i == 3) currentState = MENU_MIDIA_VIDEOS;
                        else if (i == 4) { strcpy(caminhoImagens, "data/HyperNeiva/midia/imagens"); lerPastaImagens(); currentState = MENU_MIDIA_IMAGENS; }
                        else if (i == 6) { strcpy(caminhoLeitura, "data/HyperNeiva/midia/leitura"); lerPastaLeitura(); currentState = MENU_MIDIA_LEITURA; }
                        else if (i == 7) { strcpy(caminhoArquivos, "data/HyperNeiva/midia/arquivos"); lerPastaArquivos(); currentState = MENU_MIDIA_ARQUIVOS; }
                    } else if (currentState == MENU_MIDIA_RADIO_CATEGORIA) {
                        if (i == 0) { currentState = MENU_MIDIA; scrollY = 0; }
                        else if (i == 1) { 
                            previousStateForSearch = MENU_MIDIA_RADIO_CATEGORIA; searchTargetMode = 1;
                            globalSearchQuery[0] = '\0'; globalSearchCount = 0; currentState = MENU_PESQUISA_GLOBAL; scrollY = 0; 
                        }
                        else if (i == 2) { radio_fetch_top50(); currentState = MENU_MIDIA_RADIO_LISTA; scrollY = 0; }
                        else if (i == 3) { radio_fetch_saopaulo(); currentState = MENU_MIDIA_RADIO_LISTA; scrollY = 0; }
                        else if (i == 4) { radio_fetch_podcast(); currentState = MENU_MIDIA_RADIO_LISTA; scrollY = 0; }
                        else if (i == 5) { currentState = MENU_MIDIA_RADIO_OUTROS; scrollY = 0; }
                    } else if (currentState == MENU_MIDIA_RADIO_OUTROS) {
                        if (i == 0) { currentState = MENU_MIDIA_RADIO_CATEGORIA; scrollY = 0; }
                        else if (i == 1) { // Gênero
                            previousStateForSearch = MENU_MIDIA_RADIO_OUTROS; searchTargetMode = 2;
                            globalSearchQuery[0] = '\0'; globalSearchCount = 0; currentState = MENU_PESQUISA_GLOBAL; scrollY = 0; 
                        }
                        else if (i == 2) { // País
                            previousStateForSearch = MENU_MIDIA_RADIO_OUTROS; searchTargetMode = 3;
                            globalSearchQuery[0] = '\0'; globalSearchCount = 0; currentState = MENU_PESQUISA_GLOBAL; scrollY = 0; 
                        }
                        else if (i == 3) { // Estado
                            previousStateForSearch = MENU_MIDIA_RADIO_OUTROS; searchTargetMode = 4;
                            globalSearchQuery[0] = '\0'; globalSearchCount = 0; currentState = MENU_PESQUISA_GLOBAL; scrollY = 0; 
                        }
                        else if (i == 4) { 
                            previousStateForSearch = MENU_MIDIA_RADIO_OUTROS; searchTargetMode = 5;
                            globalSearchQuery[0] = '\0'; globalSearchCount = 0; currentState = MENU_PESQUISA_GLOBAL; scrollY = 0; 
                        }
                    } else if (currentState == MENU_MIDIA_MUSICAS) {
                        if (strcmp(opcoes[i], "Voltar") == 0) {
                            if (strcmp(caminhoMusicas, "data/HyperNeiva/midia/musicas") == 0) { currentState = MENU_MIDIA; scrollY = 0; }
                            else {
                                char* lastSlash = strrchr(caminhoMusicas, '/'); if (!lastSlash) lastSlash = strrchr(caminhoMusicas, '\\');
                                if (lastSlash) *lastSlash = '\0'; lerPastaMusicas(); scrollY = 0;
                            }
                            continue;
                        } else if (strcmp(opcoes[i], "OPÇÕES DE ÁUDIO") == 0) {
                            currentState = MENU_AUDIO_OPCOES; scrollY = 0;
                            continue;
                        } else if (strcmp(opcoes[i], "Pesquisar") == 0) {
                            previousStateForSearch = MENU_MIDIA_MUSICAS; searchTargetMode = 0;
                            globalSearchQuery[0] = '\0'; globalSearchCount = 0; currentState = MENU_PESQUISA_GLOBAL; scrollY = 0;
                            continue;
                        }
                        
                        int realIdx = -1;
                        for (int j = 0; j < qtdMusicas; j++) { if (opcoes[i] == listaMusicasNomes[j]) { realIdx = j; break; } }
                        if (realIdx == -1) continue;
                        
                        if (listaMusicasEhPasta[realIdx]) {
                            strcpy(caminhoMusicas, listaMusicasCaminhos[realIdx]); lerPastaMusicas(); scrollY = 0;
                        } else {
                            if (musicLoaded) UnloadMusicStream(currentMusic);
                            currentMusic = LoadMusicStream(listaMusicasCaminhos[realIdx]);
                            strcpy(currentMusicPath, listaMusicasCaminhos[realIdx]);
                            carregarMetadadosMusica(currentMusicPath);
                            musicLoaded = true; isPlaying = true;
                            SetMusicVolume(currentMusic, volumeGeral / 100.0f);
                            PlayMusicStream(currentMusic);
                            strncpy(musicTitle, GetFileName(listaMusicasCaminhos[realIdx]), 255);
                        }
                    } else if (currentState == MENU_MIDIA_IMAGENS) {
                        if (strcmp(opcoes[i], "Voltar") == 0) {
                            if (strcmp(caminhoImagens, "data/HyperNeiva/midia/imagens") == 0) { currentState = MENU_MIDIA; scrollY = 0; }
                            else {
                                char* lastSlash = strrchr(caminhoImagens, '/'); if (!lastSlash) lastSlash = strrchr(caminhoImagens, '\\');
                                if (lastSlash) *lastSlash = '\0'; lerPastaImagens(); scrollY = 0;
                            }
                            continue;
                        } else if (strcmp(opcoes[i], "Pesquisar") == 0) {
                            previousStateForSearch = MENU_MIDIA_IMAGENS; searchTargetMode = 0;
                            globalSearchQuery[0] = '\0'; globalSearchCount = 0; currentState = MENU_PESQUISA_GLOBAL; scrollY = 0;
                            continue;
                        }
                        
                        int realIdx = -1;
                        for (int j = 0; j < qtdImagens; j++) { if (opcoes[i] == listaImagensNomes[j]) { realIdx = j; break; } }
                        if (realIdx == -1) continue;
                        
                        if (listaImagensEhPasta[realIdx]) {
                            strcpy(caminhoImagens, listaImagensCaminhos[realIdx]); lerPastaImagens(); scrollY = 0;
                        } else {
                            if (doubleClicked && strcmp(viewerImageTitle, listaImagensNomes[realIdx]) == 0) {
                                isFullScreenImage = true;
                            } else {
                                if (viewerImageLoaded) UnloadTexture(viewerImage);
                                viewerImage = LoadTexture(listaImagensCaminhos[realIdx]);
                                viewerImageLoaded = true;
                                strncpy(viewerImageTitle, listaImagensNomes[realIdx], 255);
                            }
                        }
                    } else if (currentState == MENU_MIDIA_LEITURA) {
                        if (strcmp(opcoes[i], "Voltar") == 0) {
                            if (strcmp(caminhoLeitura, "data/HyperNeiva/midia/leitura") == 0) { currentState = MENU_MIDIA; scrollY = 0; }
                            else {
                                char* lastSlash = strrchr(caminhoLeitura, '/'); if (!lastSlash) lastSlash = strrchr(caminhoLeitura, '\\');
                                if (lastSlash) *lastSlash = '\0'; lerPastaLeitura(); scrollY = 0;
                            }
                            continue;
                        } else if (strcmp(opcoes[i], "Pesquisar") == 0) {
                            previousStateForSearch = MENU_MIDIA_LEITURA; searchTargetMode = 0;
                            globalSearchQuery[0] = '\0'; globalSearchCount = 0; currentState = MENU_PESQUISA_GLOBAL; scrollY = 0;
                            continue;
                        }
                        
                        int realIdx = -1;
                        for (int j = 0; j < qtdLeitura; j++) { if (opcoes[i] == listaLeituraNomes[j]) { realIdx = j; break; } }
                        if (realIdx == -1) continue;
                        
                        if (listaLeituraEhPasta[realIdx]) {
                            strcpy(caminhoLeitura, listaLeituraCaminhos[realIdx]); lerPastaLeitura(); scrollY = 0;
                        } else {
                            if (doubleClicked && strcmp(viewerTextTitle, listaLeituraNomes[realIdx]) == 0) {
                                isFullScreenText = true;
                            } else {
                                if (strstr(listaLeituraNomes[realIdx], ".pdf") || strstr(listaLeituraNomes[realIdx], ".PDF")) {
                                    abrirPDF(listaLeituraCaminhos[realIdx]);
                                    if (viewerText) { UnloadFileText(viewerText); viewerText = NULL; }
                                } else {
                                    fecharPDF();
                                    if (viewerText) UnloadFileText(viewerText);
                                    viewerText = LoadFileText(listaLeituraCaminhos[realIdx]);
                                    viewerTextScrollY = 0;
                                }
                                strncpy(viewerTextTitle, listaLeituraNomes[realIdx], 255);
                            }
                        }
                    } else if (currentState == MENU_MIDIA_ARQUIVOS) {
                        if (strcmp(opcoes[i], "Voltar") == 0) {
                            if (strcmp(caminhoArquivos, "data/HyperNeiva/midia/arquivos") == 0) { 
                                currentState = MENU_MIDIA; scrollY = 0; 
                            } else {
                                char* lastSlash = strrchr(caminhoArquivos, '/'); 
                                char* lastBackslash = strrchr(caminhoArquivos, '\\');
                                char* slash = (lastSlash > lastBackslash) ? lastSlash : lastBackslash;
                                
                                if (slash && slash != caminhoArquivos && *(slash - 1) != ':') {
                                    *slash = '\0'; 
                                    lerPastaArquivos(); 
                                    scrollY = 0;
                                } else {
                                    // Reached root of a drive or starting folder
                                    currentState = MENU_EXPLORAR; scrollY = 0;
                                }
                            }
                            continue;
                        } else if (strcmp(opcoes[i], "Opcoes") == 0) {
                            currentState = MENU_ARQUIVOS_OPCOES_GERAL; scrollY = 0;
                            continue;
                        } else if (strcmp(opcoes[i], "Pesquisar") == 0) {
                            previousStateForSearch = MENU_MIDIA_ARQUIVOS; searchTargetMode = 0;
                            globalSearchQuery[0] = '\0'; globalSearchCount = 0; currentState = MENU_PESQUISA_GLOBAL; scrollY = 0;
                            continue;
                        }
                        
                        int realIdx = -1;
                        for (int j = 0; j < qtdArquivos; j++) { if (opcoes[i] == listaArquivosNomes[j]) { realIdx = j; break; } }
                        if (realIdx == -1) continue;
                        
                        if (listaArquivosEhPasta[realIdx]) {
                            strcpy(caminhoArquivos, listaArquivosCaminhos[realIdx]); lerPastaArquivos(); scrollY = 0;
                        } else {
                            char* cname = listaArquivosNomes[realIdx];
                            char* cpath = listaArquivosCaminhos[realIdx];
                            
                            if (strstr(cname, ".pdf") || strstr(cname, ".PDF")) {
                                abrirPDF(cpath);
                                if (viewerText) { UnloadFileText(viewerText); viewerText = NULL; }
                                strncpy(viewerTextTitle, cname, 255);
                                currentState = MENU_MIDIA_LEITURA;
                            } else if (strstr(cname, ".txt") || strstr(cname, ".ini") || strstr(cname, ".json") || strstr(cname, ".xml")) {
                                fecharPDF();
                                abrirNotepad(cpath);
                                currentState = MENU_BLOCO_DE_NOTAS;
                            } else if (strstr(cname, ".png") || strstr(cname, ".jpg") || strstr(cname, ".bmp")) {
                                if (viewerImageLoaded) UnloadTexture(viewerImage);
                                viewerImage = LoadTexture(cpath);
                                viewerImageLoaded = true;
                                strncpy(viewerImageTitle, cname, 255);
                                currentState = MENU_MIDIA_IMAGENS;
                            } else {
                                char cmd[1024];
                                char winPath[512];
                                strcpy(winPath, cpath);
                                for(int k=0; winPath[k]; k++) if(winPath[k]=='/') winPath[k]='\\';
                                sprintf(cmd, "cmd.exe /c start \"\" \"%s\"", winPath);
                                exec_async_silent(cmd);
                            }
                        }
                    } else if (currentState == MENU_AUDIO_OPCOES) {
                        if (i == 0) {
                            if (musicLoaded) { isPlaying = !isPlaying; if (isPlaying) ResumeMusicStream(currentMusic); else PauseMusicStream(currentMusic); }
                        } else if (i == 1) {
                            if (musicLoaded) { StopMusicStream(currentMusic); isPlaying = false; strcpy(musicTitle, "PARADO"); }
                        } else if (i == 2) {
                            if (musicLoaded) {
                                int idx = encontrarIndiceMusicaAtual(currentMusicPath);
                                if (idx != -1) {
                                    int nextIdx = idx + 1; bool found = false;
                                    while (nextIdx < qtdMusicas) { if (!listaMusicasEhPasta[nextIdx]) { found = true; break; } nextIdx++; }
                                    if (found) {
                                        UnloadMusicStream(currentMusic); currentMusic = LoadMusicStream(listaMusicasCaminhos[nextIdx]);
                                        strcpy(currentMusicPath, listaMusicasCaminhos[nextIdx]); isPlaying = true;
                                        carregarMetadadosMusica(currentMusicPath);
                                        SetMusicVolume(currentMusic, volumeGeral / 100.0f); PlayMusicStream(currentMusic);
                                        strncpy(musicTitle, GetFileName(listaMusicasCaminhos[nextIdx]), 255);
                                    }
                                }
                            }
                        } else if (i == 3) {
                            if (musicLoaded) {
                                int idx = encontrarIndiceMusicaAtual(currentMusicPath);
                                if (idx != -1) {
                                    int prevIdx = idx - 1; bool found = false;
                                    while (prevIdx >= 2) { if (!listaMusicasEhPasta[prevIdx]) { found = true; break; } prevIdx--; }
                                    if (found) {
                                        UnloadMusicStream(currentMusic); currentMusic = LoadMusicStream(listaMusicasCaminhos[prevIdx]);
                                        strcpy(currentMusicPath, listaMusicasCaminhos[prevIdx]); isPlaying = true;
                                        carregarMetadadosMusica(currentMusicPath);
                                        SetMusicVolume(currentMusic, volumeGeral / 100.0f); PlayMusicStream(currentMusic);
                                        strncpy(musicTitle, GetFileName(listaMusicasCaminhos[prevIdx]), 255);
                                    }
                                }
                            }
                        } else if (i == 4) {
                            volumeGeral += 10; if (volumeGeral > 100) volumeGeral = 100;
                            if (musicLoaded) SetMusicVolume(currentMusic, volumeGeral / 100.0f);
                        } else if (i == 5) {
                            volumeGeral -= 10; if (volumeGeral < 0) volumeGeral = 0;
                            if (musicLoaded) SetMusicVolume(currentMusic, volumeGeral / 100.0f);
                        } else if (i == 6) {
                            if (musicLoaded) {
                                float t = GetMusicTimePlayed(currentMusic) + 10.0f;
                                if (t > GetMusicTimeLength(currentMusic)) t = GetMusicTimeLength(currentMusic) - 0.1f;
                                SeekMusicStream(currentMusic, t);
                            }
                        } else if (i == 7) {
                            if (musicLoaded) {
                                float t = GetMusicTimePlayed(currentMusic) - 10.0f;
                                if (t < 0.0f) t = 0.0f;
                                SeekMusicStream(currentMusic, t);
                            }
                        } else if (i == 8) {
                            modoReproducao = (modoReproducao + 1) % 5;
                        } else if (i == 9) {
                            currentState = MENU_MIDIA_MUSICAS; scrollY = 0;
                        }
                    } else if (currentState == MENU_ARQUIVOS_OPCOES_GERAL) {
                        if (i == 0) { currentState = MENU_MIDIA_ARQUIVOS; scrollY = 0; }
                        else if (i == 1) { searchTargetMode = 6; globalSearchQuery[0] = '\0'; globalSearchCount = 0; currentState = MENU_PESQUISA_GLOBAL; scrollY = 0; }
                        else if (i == 2) { searchTargetMode = 7; globalSearchQuery[0] = '\0'; globalSearchCount = 0; currentState = MENU_PESQUISA_GLOBAL; scrollY = 0; }
                        else if (i == 3) {
                            if (strlen(clipboardPath) > 0) {
                                char destPath[1024];
                                char* base = strrchr(clipboardPath, '/'); if (!base) base = strrchr(clipboardPath, '\\');
                                if (base) base++; else base = clipboardPath;
                                sprintf(destPath, "%s/%s", caminhoArquivos, base);
                                if (clipboardIsCut) { rename(clipboardPath, destPath); clipboardPath[0] = '\0'; }
                                else { copyFileReal(clipboardPath, destPath); }
                                lerPastaArquivos();
                            }
                            currentState = MENU_MIDIA_ARQUIVOS; scrollY = 0;
                        }
                    } else if (currentState == MENU_ARQUIVOS_OPCOES_ARQUIVO) {
                        if (strcmp(opcoes[i], "Voltar") == 0) { currentState = MENU_MIDIA_ARQUIVOS; scrollY = 0; }
                        else if (strcmp(opcoes[i], "Copiar") == 0) { strcpy(clipboardPath, targetFilePath); clipboardIsCut = false; currentState = MENU_MIDIA_ARQUIVOS; scrollY = 0; }
                        else if (strcmp(opcoes[i], "Recortar") == 0) { strcpy(clipboardPath, targetFilePath); clipboardIsCut = true; currentState = MENU_MIDIA_ARQUIVOS; scrollY = 0; }
                        else if (strcmp(opcoes[i], "Renomear") == 0) { searchTargetMode = 8; globalSearchQuery[0] = '\0'; globalSearchCount = 0; currentState = MENU_PESQUISA_GLOBAL; scrollY = 0; }
                        else if (strcmp(opcoes[i], "Deletar") == 0) { remove(targetFilePath); lerPastaArquivos(); currentState = MENU_MIDIA_ARQUIVOS; scrollY = 0; }
                        else if (strcmp(opcoes[i], "Extrair ZIP") == 0) {
                            extrairZip(targetFilePath, caminhoArquivos);
                            lerPastaArquivos(); currentState = MENU_MIDIA_ARQUIVOS; scrollY = 0;
                        }
                        else if (strcmp(opcoes[i], "Compactar ZIP") == 0) {
                            char zipDest[1024];
                            char* base = strrchr(targetFilePath, '/'); if (!base) base = strrchr(targetFilePath, '\\');
                            char nomeSemExt[256] = "";
                            if (base) {
                                strncpy(nomeSemExt, base + 1, 255);
                                char* dot = strrchr(nomeSemExt, '.'); if (dot) *dot = '\0';
                                char dirPath[1024]; strcpy(dirPath, targetFilePath);
                                dirPath[base - targetFilePath] = '\0';
                                sprintf(zipDest, "%s/%s.zip", dirPath, nomeSemExt);
                            } else {
                                sprintf(zipDest, "%s/%s.zip", caminhoArquivos, targetFilePath);
                            }
                            compactarZip(targetFilePath, zipDest);
                            lerPastaArquivos(); currentState = MENU_MIDIA_ARQUIVOS; scrollY = 0;
                        }
                        else if (strcmp(opcoes[i], "Usar como Fundo") == 0) {
                            definirComoBackground(targetFilePath);
                            currentState = MENU_MIDIA_ARQUIVOS; scrollY = 0;
                        }
                    } else if (currentState == MENU_MIDIA_RADIO_LISTA) {
                        if (i == 0) { currentState = MENU_MIDIA_RADIO_CATEGORIA; scrollY = 0; }
                        else {
                            if (musicLoaded) PauseMusicStream(currentMusic);
                            radio_play(radioUrls[i]);
                            
                            if (radioLogoLoaded) { UnloadTexture(radioLogo); radioLogoLoaded = false; }
                            if (FileExists("data/HyperNeiva/current_radio_logo.jpg")) {
                                radioLogo = LoadTexture("data/HyperNeiva/current_radio_logo.jpg");
                                if (radioLogo.id != 0) radioLogoLoaded = true;
                            }
                            if (!IsAudioStreamPlaying(radioStream)) PlayAudioStream(radioStream);
                        }
                    } else if (currentState == MENU_EXTRA) {
                        if (i == 0) { currentState = MENU_MAIN; scrollY = 0; }
                        else if (i == 1) {
                            initControleTeste();
                            currentState = MENU_CONTROLE_TESTE;
                            scrollY = 0;
                        }
                        else if (i == 2) {
                            initTecladoTeste();
                            currentState = MENU_TECLADO_TESTE;
                            scrollY = 0;
                        }
                        else if (i == 3) {
                            initInstrumentos();
                            currentState = MENU_INSTRUMENTOS;
                            scrollY = 0;
                        }
                        else if (i == 4) {
                            initFerramentas();
                            ferramentasViaAbaPrincipal = false;
                            currentState = MENU_FERRAMENTAS;
                            scrollY = 0;
                        }
                    } else if (currentState == MENU_CONFIGURACOES) {
                        if (i == 0) { currentState = MENU_MAIN; scrollY = 0; }
                        else if (i == 1) {
                            apiKeys_carregar();
                            currentState = MENU_CONFIG_API;
                            scrollY = 0;
                        }
                    } else if (currentState == MENU_BAIXAR) {
                        if (i == 0) { currentState = MENU_MAIN; scrollY = 0; }
                        else if (i == 1) { currentState = MENU_BAIXAR_REPOS; scrollY = 0; }
                        else if (i == 2) { currentState = MENU_BAIXAR_LINK; baixarTyping = true; baixarUrlInput[0] = 0; baixarUrlCursor = 0; scrollY = 0; }
                        else if (i == 3) { currentState = MENU_BAIXAR_LOJAS; scrollY = 0; }
                        else if (i == 4) { currentState = MENU_BAIXAR_FTP; scrollY = 0; }
                        else if (i == 5) { currentState = MENU_BAIXAR_NUVEM; scrollY = 0; }
                        else if (i == 6) { currentState = MENU_BAIXAR_MIDIA; scrollY = 0; }
                    }
                    else if (currentState == MENU_BAIXAR_MIDIA) {
                        if (i == 0) { currentState = MENU_BAIXAR; scrollY = 0; }
                        else if (i == 1) { currentState = MENU_BAIXAR_ANIME; scrollY = 0; midiaSel = 0; midiaScroll = 0; }
                        else if (i == 2) { currentState = MENU_BAIXAR_MUSICA; scrollY = 0; midiaSel = 0; midiaScroll = 0; }
                        else if (i == 3) { currentState = MENU_BAIXAR_LIVROS; scrollY = 0; midiaSel = 0; midiaScroll = 0; }
                        else if (i == 4) { currentState = MENU_BAIXAR_LEGENDAS; scrollY = 0; midiaSel = 0; midiaScroll = 0; }
                        else if (i == 5) { currentState = MENU_BAIXAR_FILMES; scrollY = 0; }
                    }
                    else if (currentState == MENU_BAIXAR_NUVEM) {
                        if (i == 0) { currentState = MENU_BAIXAR; scrollY = 0; }
                        else if (i == 1) { dropboxConectando = true; currentState = MENU_BAIXAR_DROPBOX_NAV; scrollY = 0; }
                        else if (i == 2) { gdConectando = true; currentState = MENU_BAIXAR_GOOGLE_DRIVE_NAV; scrollY = 0; }
                    }
                    else if (currentState == MENU_BAIXAR_LINK) {
                        if (i == 0) { currentState = MENU_BAIXAR; baixarTyping = false; scrollY = 0; }
                        else if (i == 1) {
                            if (strlen(baixarUrlInput) > 4) {
                                baixar_baixar_url_direta(baixarUrlInput);
                                baixarUrlInput[0] = 0; baixarUrlCursor = 0;
                                currentState = MENU_BAIXAR_FILA; scrollY = 0;
                            }
                        }
                    }
                    else if (currentState == MENU_BAIXAR_REPOS) {
                        if (i == baixarBufTotal - 1) { currentState = MENU_BAIXAR; scrollY = 0; }
                        else {
                            baixar_entrar_repositorio(baixarBuf[i].nome);
                            currentState = MENU_BAIXAR_REPOS_XML; scrollY = 0;
                        }
                    }
                    else if (currentState == MENU_BAIXAR_REPOS_XML) {
                        if (i == baixarBufTotal - 1) { currentState = MENU_BAIXAR_REPOS; scrollY = 0; }
                        else {
                            baixar_abrir_xml(baixarBuf[i].nome, baixarBuf, &baixarBufTotal);
                            currentState = MENU_BAIXAR_REPOS_GAMES; scrollY = 0;
                        }
                    }
                    else if (currentState == MENU_BAIXAR_REPOS_GAMES) {
                        if (i == baixarBufTotal - 1) { currentState = MENU_BAIXAR_REPOS_XML; scrollY = 0; }
                        else {
                            baixarGameIdx = i;
                            baixar_mostrar_links_jogo(i, baixarBuf, &baixarBufTotal);
                            currentState = MENU_BAIXAR_REPOS_LINKS; scrollY = 0;
                        }
                    }
                    else if (currentState == MENU_BAIXAR_REPOS_LINKS) {
                        if (i == baixarBufTotal - 1) { currentState = MENU_BAIXAR_REPOS_GAMES; scrollY = 0; }
                        else {
                            if (baixarBuf[i].link[0]) {
                                baixar_baixar_url_direta(baixarBuf[i].link);
                                currentState = MENU_BAIXAR_FILA; scrollY = 0;
                            }
                        }
                    }
                    else if (currentState == MENU_BAIXAR_DROPBOX) {
                        if (i == baixarBufTotal - 1) { currentState = MENU_BAIXAR; scrollY = 0; }
                        else if (i == 0) { currentState = MENU_BAIXAR_LINK; baixarTyping = true; baixarUrlInput[0] = 0; baixarUrlCursor = 0; scrollY = 0; }
                        else if (i == 1) {
                            char cmd[1024]; sprintf(cmd, "explorer \"%s\"", baixar_dir_base());
                            exec_async_silent(cmd);
                        }
                    }
                    else if (currentState == MENU_BAIXAR_LOJAS) {
                        if (i == baixarBufTotal - 1) { currentState = MENU_BAIXAR; scrollY = 0; }
                        else if (i == 0) { baixar_carregar_hbstore(); currentState = MENU_BAIXAR_HB; scrollY = 0; }
                        else if (i == 1) { baixar_carregar_apollo(); currentState = MENU_BAIXAR_APOLLO; scrollY = 0; }
                        else if (i == 2) { baixar_carregar_retroarch(); currentState = MENU_BAIXAR_RETRO; scrollY = 0; }
                    }
                    else if (currentState == MENU_BAIXAR_HB || currentState == MENU_BAIXAR_APOLLO || currentState == MENU_BAIXAR_RETRO) {
                        if (i == baixarTotal) { baixar_loja_cancelar(); currentState = MENU_BAIXAR_LOJAS; scrollY = 0; }
                        else if (i < baixarTotal) {
                            if (baixarLista[i].link[0]) {
                                baixar_adicionar_link_fila(baixarLista[i].nome, baixarLista[i].link);
                                currentState = MENU_BAIXAR_FILA; scrollY = 0;
                            }
                        }
                    }
                    else if (currentState == MENU_BAIXAR_FTP) {
                        if (i == 0) { currentState = MENU_BAIXAR; scrollY = 0; }
                        else if (i == 1) {
                            char ip[64]; baixar_obter_ip_local(ip);
                            ftpServerIdx = ftpTotalServers;
                            ftp_adicionar_servidor("Novo Servidor", ip, 21, "anonymous", "", 0, "");
                            ftpServerIdx = ftpTotalServers - 1;
                            ftpEditandoCampo = -1;
                            currentState = MENU_BAIXAR_FTP_EDITAR; scrollY = 0;
                        }
                        else {
                            int srvIdx = i - 2;
                            float btnW = (hasScroll ? 280.0f : 310.0f);
                            Rectangle edRec = { 470 + btnW - 132, yPos + 5, 62, (float)alturaBtn - 10 };
                            Rectangle opRec = { 470 + btnW - 65, yPos + 5, 62, (float)alturaBtn - 10 };
                            if (CheckCollisionPointRec(mousePoint, edRec)) {
                                ftpServerIdx = srvIdx; ftpEditandoCampo = -1;
                                currentState = MENU_BAIXAR_FTP_EDITAR; scrollY = 0;
                            } else {
                                ftpServerIdx = srvIdx;
                                ftp_listar_local("."); ftp_listar_remoto_async("/");
                                ftpLocalSel = 0; ftpRemoteSel = 0; ftpPainelAtivo = 1;
                                currentState = MENU_BAIXAR_FTP_GERENCIAR; scrollY = 0;
                            }
                        }
                    }
                    else if (currentState == MENU_BAIXAR_FTP_EDITAR) {
                        int fi = (i < qtd) ? ftpEdMap[i] : -9;
                        if (fi == -3) { currentState = MENU_BAIXAR_FTP; scrollY = 0; }
                        else if (fi == 5) {
                            ftpEditandoCampo = -1;
                            int oldTipo = ftpServers[ftpServerIdx].tipo;
                            int portaAtual = ftpServers[ftpServerIdx].port;
                            int defaultAntigo = oldTipo == 2 ? 22 : oldTipo == 1 ? 2222 : 21;
                            ftpServers[ftpServerIdx].tipo = (oldTipo + 1) % 3;
                            if (portaAtual == defaultAntigo)
                                ftpServers[ftpServerIdx].port = ftpServers[ftpServerIdx].tipo == 2 ? 22 : ftpServers[ftpServerIdx].tipo == 1 ? 2222 : 21;
                        }
                        else if (fi >= 0 && fi <= 6) {
                            ftpEditandoCampo = fi;
                            if (fi == 0) strcpy(ftpEditBuf, ftpServers[ftpServerIdx].name);
                            else if (fi == 1) strcpy(ftpEditBuf, ftpServers[ftpServerIdx].ip);
                            else if (fi == 2) sprintf(ftpEditBuf, "%d", ftpServers[ftpServerIdx].port);
                            else if (fi == 3) strcpy(ftpEditBuf, ftpServers[ftpServerIdx].user);
                            else if (fi == 4) strcpy(ftpEditBuf, ftpServers[ftpServerIdx].pass);
                            else if (fi == 6) strcpy(ftpEditBuf, ftpServers[ftpServerIdx].ssh_key);
                        }
                        else if (fi == -1) {
                            if (ftpServerIdx >= 0 && ftpServerIdx < ftpTotalServers) { ftp_salvar_servidores(); sprintf(ftpStatus, "Servidor salvo."); }
                            currentState = MENU_BAIXAR_FTP; scrollY = 0;
                        }
                        else if (fi == -2) {
                            if (ftpServerIdx >= 0 && ftpServerIdx < ftpTotalServers) ftp_deletar_servidor(ftpServerIdx);
                            currentState = MENU_BAIXAR_FTP; scrollY = 0;
                        }
                    }
                    else if (currentState == MENU_BAIXAR_FTP_GERENCIAR) {
                        if (i == 0) { currentState = MENU_BAIXAR_FTP; scrollY = 0; }
                        else if (i == 1 && ftpMenuRemotoInicio == 2) { ftp_voltar_remota(); scrollY = 0; }
                        else if (i >= ftpMenuRemotoInicio && !ftpListandoRemoto) {
                            int sIdx = i - ftpMenuRemotoInicio;
                            if (sIdx >= 0 && sIdx < ftpRemoteTotal) {
                                ftpRemoteSel = sIdx; ftpPainelAtivo = 1;
                                if (ftpRemoteList[sIdx].isDir && ftpRemoteList[sIdx].path[0]) {
                                    ftp_entrar_pasta_remota(ftpRemoteList[sIdx].name);
                                    ftpRemoteSel = 0; scrollY = 0;
                                }
                            }
                        }
                    }
                    else if (currentState == MENU_BAIXAR_FILA) {
                        if (i == 0) { currentState = MENU_BAIXAR; scrollY = 0; }
                        else if (i == 1) {
                            baixar_iniciar_fila();
                        }
                        else if (i == 2) {
                            char cmd[700];
                            sprintf(cmd, "explorer \"%s/baixado\"", baixar_dir_base());
                            exec_async_silent(cmd);
                        }
                    }
                    else {
                        if (i == 0) { currentState = MENU_MAIN; scrollY = 0; }
                    }
                }
                Color rowColor = hovered ? btnHover : btnNormal;
                bool menuSel = false, menuDir = false;
                if (currentState == MENU_BAIXAR_FTP_GERENCIAR && i >= ftpMenuRemotoInicio) {
                    int sIdx = i - ftpMenuRemotoInicio;
                    menuDir = ftpMenuIsDir[i];
                    if (sIdx >= 0 && sIdx < ftpRemoteTotal && ftpRemoteSel == sIdx && ftpPainelAtivo == 1) menuSel = true;
                }
                if (menuSel) rowColor = (Color){60,80,120,255};
                DrawRectangleRec(btn, rowColor);

                if (currentState == MENU_BAIXAR_FTP && i >= 2) {
                    Rectangle edRec = { btn.x + btn.width - 132, yPos + 5, 62, alturaBtn - 10 };
                    Rectangle opRec = { btn.x + btn.width - 65, yPos + 5, 62, alturaBtn - 10 };
                    bool edHov = CheckCollisionPointRec(mousePoint, edRec);
                    bool opHov = CheckCollisionPointRec(mousePoint, opRec);
                    float cx = edRec.x + edRec.width / 2.0f;
                    float cy = edRec.y + edRec.height / 2.0f;
                    Color pc = edHov ? (Color){130,180,255,255} : (Color){80,140,230,255};
                    DrawRectangle((int)(cx - 4), (int)(cy - 12), 8, 4, (Color){120,120,120,255});
                    DrawRectangle((int)(cx - 4), (int)(cy - 8), 8, 2, (Color){45,45,45,255});
                    DrawRectangle((int)(cx - 4), (int)(cy - 6), 8, 11, pc);
                    DrawTriangle((Vector2){cx - 4, cy + 5}, (Vector2){cx + 4, cy + 5}, (Vector2){cx, cy + 13}, pc);
                    DrawRectangleRec(opRec, opHov ? (Color){70,190,110,255} : (Color){50,110,80,255});
                    DrawText("ABRIR", (int)opRec.x + 9, (int)opRec.y + (alturaBtn-10)/4, 12, WHITE);
                }

                int tx = (int)btn.x + 20;
                int visibleWidth = (int)btn.width - 40;
                if (currentState == MENU_BAIXAR_FTP && i >= 2) visibleWidth -= 140;
                if (menuDir) {
                    int iy = (int)btn.y + (alturaBtn - 12) / 2;
                    DrawRectangle((int)btn.x + 8, iy, 7, 3, (Color){230,180,60,255});
                    DrawRectangle((int)btn.x + 8, iy + 3, 15, 9, (Color){230,180,60,255});
                    tx = (int)btn.x + 30;
                    visibleWidth -= 22;
                }
                int textWidth = MeasureText(opcoes[i], 16);
                if (textWidth <= visibleWidth) {
                    DrawText(opcoes[i], tx, (int)btn.y + (alturaBtn/4), 16, menuDir ? (Color){120,200,255,255} : textLight);
                } else {
                    float offset = 0.0f;
                    if (hovered) {
                        float speed = 70.0f;
                        float range = (float)(textWidth - visibleWidth + 20);
                        float t = (float)fmod(GetTime() * speed, range * 2.0f);
                        offset = (t < range) ? -t : -(range * 2.0f - t);
                    }
                    BeginScissorMode(tx, (int)btn.y, visibleWidth, (int)btn.height);
                    DrawText(opcoes[i], tx + (int)offset, (int)btn.y + (alturaBtn/4), 16, menuDir ? (Color){120,200,255,255} : textLight);
                    EndScissorMode();
                }
            }
        }

        if (currentState == MENU_BAIXAR_FILMES) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                bool clickedBusca = CheckCollisionPointRec(mousePoint, (Rectangle){ 20, 60, 520, 34 });
                tmdbTyping = clickedBusca;
                
                if (CheckCollisionPointRec(mousePoint, (Rectangle){ (float)screenWidth - 110, 20, 90, 30 })) { currentState = MENU_BAIXAR; scrollY = 0; }
                else if (CheckCollisionPointRec(mousePoint, (Rectangle){ (float)screenWidth - 210, 20, 90, 30 })) { tmdb_carregar_exibicao(tmdb_ultimo_caminho()); currentState = MENU_BAIXAR_FILMES_BAIXADOS; scrollY = 0; }
                else if (CheckCollisionPointRec(mousePoint, (Rectangle){ 550, 60, 90, 34 })) { if (tmdbBusca[0]) tmdb_buscar(tmdbBusca); }
                else if (!clickedBusca) {
                    int py = 150, ih = 56, pxR = 560, pwR = screenWidth - 580;
                    for (int i = 0; i < tmdbTotal; i++) {
                        int y = py + i * ih - (tmdbSel > 10 ? (tmdbSel - 10) * ih : 0);
                        if (y < py - ih || y > screenHeight - 40) continue;
                        if (CheckCollisionPointRec(mousePoint, (Rectangle){ (float)pxR, (float)y, (float)pwR, (float)ih })) {
                            tmdbSel = i;
                            if (!tmdbBaixando) tmdb_selecionar(i);
                        }
                    }
                    if (tmdbCapaLoaded && CheckCollisionPointRec(mousePoint, (Rectangle){ 20, 150, 520, (float)(screenHeight - 210) })) {
                        tmdb_mostrar_texto();
                    }
                    if (tmdbMostraTexto && !tmdbBaixando && CheckCollisionPointRec(mousePoint, (Rectangle){ 20, 318, 180, 24 })) {
                        if (tmdbSel >= 0 && tmdbSel < tmdbTotal) { tmdb_baixar_item(tmdbSel); tmdb_aguardar(); tmdb_carregar_exibicao(tmdb_ultimo_caminho()); }
                    }
                    if (tmdbMostraTexto && !tmdbBaixando && CheckCollisionPointRec(mousePoint, (Rectangle){ 210, 318, 170, 24 })) {
                        if (tmdbSel >= 0 && tmdbSel < tmdbTotal) {
                            char termo[256];
                            TmdbItem* it = &tmdbResultados[tmdbSel];
                            if (it->titulo[0]) sprintf(termo, "%s", it->titulo); else sprintf(termo, "%s", it->tituloOrig);
                            strncpy(torrentTermo, termo, 255); torrentTermo[255] = 0;
                            torrentProvedor = 2;
                            torrentSel = 0; torrentScroll = 0;
                            currentState = MENU_BAIXAR_FILMES_TORRENT;
                        }
                    }
                }
            }
        }

        if (currentState == MENU_BAIXAR_FILMES_BAIXADOS) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(mousePoint, (Rectangle){ (float)screenWidth - 110, 20, 90, 30 })) { currentState = MENU_BAIXAR_FILMES; scrollY = 0; }
            }
        }

        if (currentState == MENU_BAIXAR_FILMES_TORRENT) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(mousePoint, (Rectangle){ 20, 20, 90, 30 })) { currentState = MENU_BAIXAR_FILMES; scrollY = 0; }
                if (CheckCollisionPointRec(mousePoint, (Rectangle){ (float)(screenWidth - 110), (float)(screenHeight - 46), 90, 30 })) { currentState = MENU_BAIXAR_FILMES; scrollY = 0; }
                for (int i = 0; i < 8; i++) {
                    int bx = 100 + (i % 4) * 165;
                    int by = 56 + (i / 4) * 30;
                    if (CheckCollisionPointRec(mousePoint, (Rectangle){ (float)bx, (float)by, 158, 26 })) torrentProvedor = i;
                }
                if (CheckCollisionPointRec(mousePoint, (Rectangle){ (float)(screenWidth - 110), 20, 90, 30 })) {
                    if (!torrentBuscando) { torrentTotal = 0; torrentPagina = 0; torrentSel = 0; torrent_buscar_provedor(torrentProvedor, torrentTermo); }
                }
                int totalPag = (torrentTotal + TORRENT_POR_PAG - 1) / TORRENT_POR_PAG;
                if (totalPag < 1) totalPag = 1;
                int ini = torrentPagina * TORRENT_POR_PAG;
                int fim = ini + TORRENT_POR_PAG; if (fim > torrentTotal) fim = torrentTotal;
                int lx = 20, lw = screenWidth - 40, ly = 140, lh = 30;
                for (int i = ini; i < fim; i++) {
                    int y = ly + (i - ini) * lh;
                    if (CheckCollisionPointRec(mousePoint, (Rectangle){ (float)lx, (float)y, (float)lw, (float)lh })) {
                        torrentSel = i;
                        torrent_abrir_magnet(i);
                    }
                }
            }
        }

        if (currentState == MENU_BAIXAR_ANIME || currentState == MENU_BAIXAR_MUSICA ||
            currentState == MENU_BAIXAR_LIVROS || currentState == MENU_BAIXAR_LEGENDAS) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(mousePoint, (Rectangle){ 20, 20, 90, 30 })) { currentState = MENU_BAIXAR_MIDIA; scrollY = 0; }
                if (CheckCollisionPointRec(mousePoint, (Rectangle){ 20, 60, 520, 34 })) midiaTyping = true;
                if (CheckCollisionPointRec(mousePoint, (Rectangle){ 550, 60, 90, 34 })) {
                    if (!midiaBuscando) {
                        midiaTotal = 0;
                        void (*buscar)(const char*) = midia_buscar_anime;
                        if (currentState == MENU_BAIXAR_MUSICA) buscar = midia_buscar_musica;
                        else if (currentState == MENU_BAIXAR_LIVROS) buscar = midia_buscar_livro;
                        else if (currentState == MENU_BAIXAR_LEGENDAS) buscar = midia_buscar_legenda;
                        buscar(midiaBusca[0] ? midiaBusca : " ");
                    }
                }
                int lx = 20, lw = screenWidth - 40, ly = 150, lh = 40;
                for (int i = 0; i < midiaTotal; i++) {
                    int y = ly + i * lh - midiaScroll;
                    if (y < ly - lh || y > screenHeight - 40) continue;
                    if (CheckCollisionPointRec(mousePoint, (Rectangle){ (float)lx, (float)y, (float)lw, (float)lh })) {
                        midiaSel = i; midia_abrir(i);
                    }
                }
                if (currentState == MENU_BAIXAR_ANIME && CheckCollisionPointRec(mousePoint, (Rectangle){ (float)(screenWidth - 210), (float)(screenHeight - 46), 190, 30 })) {
                    if (!midiaBuscando) { currentState = MENU_BAIXAR_ANIME_TORRENT; midiaTotal = 0; midiaSel = 0; midiaScroll = 0; midia_buscar_anime_torrent(midiaBusca[0] ? midiaBusca : " "); }
                }
            }
        }

        if (currentState == MENU_BAIXAR_ANIME_TORRENT) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(mousePoint, (Rectangle){ 20, 20, 90, 30 })) { currentState = MENU_BAIXAR_ANIME; scrollY = 0; }
                if (CheckCollisionPointRec(mousePoint, (Rectangle){ (float)(screenWidth - 110), (float)(screenHeight - 46), 90, 30 })) { currentState = MENU_BAIXAR_ANIME; scrollY = 0; }
                int lx = 20, lw = screenWidth - 40, ly = 140, lh = 30;
                for (int i = 0; i < midiaTotal; i++) {
                    int y = ly + i * lh - midiaScroll;
                    if (y < ly - lh || y > screenHeight - 40) continue;
                    if (CheckCollisionPointRec(mousePoint, (Rectangle){ (float)lx, (float)y, (float)lw, (float)lh })) {
                        midiaSel = i; midia_abrir(i);
                    }
                }
            }
        }

        if (currentState == MENU_BAIXAR_FTP_GERENCIAR) {
            int pxL = 20, pwL = 440, py = 95, ph = screenHeight - 160, ih = 26;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (ftpServers[ftpServerIdx].tipo == 2 && CheckCollisionPointRec(mousePoint, (Rectangle){ (float)screenWidth - 210, 20, 90, 30 })) {
                    ftp_conectar_ssh(ftpServerIdx);
                }
                else if (CheckCollisionPointRec(mousePoint, (Rectangle){ (float)screenWidth - 110, 20, 90, 30 })) {
                    currentState = MENU_BAIXAR_FTP; scrollY = 0;
                }
                for (int k = 0; k < ftpLocalTotal; k++) {
                    int y = py + 6 + k * ih - (ftpLocalSel > 12 ? (ftpLocalSel - 12) * ih : 0);
                    if (y < py || y > py + ph) continue;
                    if (CheckCollisionPointRec(mousePoint, (Rectangle){ (float)pxL + 4, (float)y, (float)pwL - 8, (float)ih })) {
                        ftpLocalSel = k; ftpPainelAtivo = 0;
                    }
                }
            }
            if (!ftpTransferindo && !ftpCriandoPasta) {
                if (IsKeyPressed(KEY_TAB)) ftpPainelAtivo = ftpPainelAtivo ? 0 : 1;
                if (IsKeyPressed(KEY_DOWN)) { if (ftpPainelAtivo == 0 && ftpLocalSel < ftpLocalTotal - 1) ftpLocalSel++; else if (ftpPainelAtivo == 1 && !ftpListandoRemoto && ftpRemoteSel < ftpRemoteTotal - 1) { ftpRemoteSel++; float yRow = 40 + (float)(ftpMenuRemotoInicio + ftpRemoteSel) * espacamento + scrollY; if (yRow + alturaBtn > screenHeight - 30) scrollY -= (int)((yRow + alturaBtn) - (screenHeight - 30)); } }
                if (IsKeyPressed(KEY_UP)) { if (ftpPainelAtivo == 0 && ftpLocalSel > 0) ftpLocalSel--; else if (ftpPainelAtivo == 1 && !ftpListandoRemoto && ftpRemoteSel > 0) { ftpRemoteSel--; float yRow = 40 + (float)(ftpMenuRemotoInicio + ftpRemoteSel) * espacamento + scrollY; if (yRow < 40) scrollY += (int)(40 - yRow); } }
                if (scrollY > 0) scrollY = 0;
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_O)) {
                    if (ftpPainelAtivo == 0 && ftpLocalSel < ftpLocalTotal && ftpLocalList[ftpLocalSel].path[0] && ftpLocalList[ftpLocalSel].isDir) { ftp_entrar_pasta_local(ftpLocalList[ftpLocalSel].name); ftpLocalSel = 0; }
                    else if (ftpPainelAtivo == 1 && !ftpListandoRemoto && ftpRemoteSel < ftpRemoteTotal && ftpRemoteList[ftpRemoteSel].path[0] && ftpRemoteList[ftpRemoteSel].isDir) { ftp_entrar_pasta_remota(ftpRemoteList[ftpRemoteSel].name); ftpRemoteSel = 0; scrollY = 0; }
                }
                if (IsKeyPressed(KEY_X)) {
                    if (ftpServers[ftpServerIdx].tipo == 2) {
                        notificar("SSH: use o terminal (botao TERMINAL) para transferencias.");
                    }
                    else if (ftpPainelAtivo == 1 && ftpRemoteSel < ftpRemoteTotal && ftpRemoteList[ftpRemoteSel].path[0]) ftp_baixar_item(ftpRemoteSel);
                    else if (ftpPainelAtivo == 0 && ftpLocalSel < ftpLocalTotal && ftpLocalList[ftpLocalSel].path[0]) ftp_enviar_item(ftpLocalSel);
                }
                if (IsKeyPressed(KEY_Y)) {
                    if (ftpServers[ftpServerIdx].tipo == 2) notificar("SSH: use o terminal para criar pastas.");
                    else { ftpCriandoPasta = 1; ftpEditBuf[0] = 0; }
                }
                if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_B)) { currentState = MENU_BAIXAR_FTP; scrollY = 0; }
            }
        }

        if (currentState == MENU_BAIXAR_FILMES) {
            if (!tmdbTyping) {
                if (IsKeyPressed(KEY_B) || IsKeyPressed(KEY_ESCAPE)) { currentState = MENU_BAIXAR; scrollY = 0; }
                if (!tmdbBaixando) {
                    if (IsKeyPressed(KEY_DOWN)) { if (tmdbSel < tmdbTotal - 1) tmdbSel++; }
                    if (IsKeyPressed(KEY_UP)) { if (tmdbSel > 0) tmdbSel--; }
                    if (IsKeyPressed(KEY_ENTER)) { if (tmdbBusca[0]) tmdb_buscar(tmdbBusca); }
                    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_O)) { if (tmdbSel >= 0 && tmdbSel < tmdbTotal) tmdb_selecionar(tmdbSel); }
                    if (IsKeyPressed(KEY_C)) {
                        if (tmdbSel >= 0 && tmdbSel < tmdbTotal) { tmdb_baixar_item(tmdbSel); tmdb_aguardar(); tmdb_carregar_exibicao(tmdb_ultimo_caminho()); }
                    }
                    if (IsKeyPressed(KEY_L)) { tmdb_carregar_exibicao(tmdb_ultimo_caminho()); currentState = MENU_BAIXAR_FILMES_BAIXADOS; scrollY = 0; }
                }
            }
        }

        if (currentState == MENU_BAIXAR_FILMES_BAIXADOS) {
            static int baixadosTotalB = 0;
            static char baixadosNomesB[1000][256];
            if (IsKeyPressed(KEY_B) || IsKeyPressed(KEY_ESCAPE)) { currentState = MENU_BAIXAR_FILMES; scrollY = 0; }
            tmdb_listar_baixados(baixadosNomesB, &baixadosTotalB);
            static int baixadosSelB = 0;
            if (baixadosTotalB == 0) baixadosSelB = 0;
            if (baixadosSelB >= baixadosTotalB) baixadosSelB = baixadosTotalB - 1;
            if (IsKeyPressed(KEY_DOWN)) { if (baixadosSelB < baixadosTotalB - 1) baixadosSelB++; }
            if (IsKeyPressed(KEY_UP)) { if (baixadosSelB > 0) baixadosSelB--; }
            if (baixadosSelB >= 0 && baixadosSelB < baixadosTotalB) {
                char p[TMDB_PATH_LEN]; sprintf(p, "%s/%s", tmdb_ultimo_caminho_base(), baixadosNomesB[baixadosSelB]);
                tmdb_carregar_exibicao(p);
            }
            if (IsKeyPressed(KEY_X) && baixadosSelB >= 0 && baixadosSelB < baixadosTotalB) {
                char p[TMDB_PATH_LEN]; sprintf(p, "%s/%s", tmdb_ultimo_caminho_base(), baixadosNomesB[baixadosSelB]);
                char cmd[1024]; sprintf(cmd, "explorer \"%s\"", p);
                exec_async_silent(cmd);
            }
        }

        if (currentState == MENU_BAIXAR_FILMES_TORRENT) {
            if (IsKeyPressed(KEY_B) || IsKeyPressed(KEY_ESCAPE)) { currentState = MENU_BAIXAR_FILMES; scrollY = 0; }
            int totalPag = (torrentTotal + TORRENT_POR_PAG - 1) / TORRENT_POR_PAG;
            if (totalPag < 1) totalPag = 1;
            if (IsKeyPressed(KEY_RIGHT)) { if (torrentPagina < totalPag - 1) { torrentPagina++; torrentSel = torrentPagina * TORRENT_POR_PAG; } }
            if (IsKeyPressed(KEY_LEFT)) { if (torrentPagina > 0) { torrentPagina--; torrentSel = torrentPagina * TORRENT_POR_PAG; } }
            if (IsKeyPressed(KEY_DOWN)) { if (torrentSel < torrentTotal - 1) torrentSel++; }
            if (IsKeyPressed(KEY_UP)) { if (torrentSel > 0) torrentSel--; }
            if (IsKeyPressed(KEY_ONE)) torrentProvedor = 0;
            if (IsKeyPressed(KEY_TWO)) torrentProvedor = 1;
            if (IsKeyPressed(KEY_THREE)) torrentProvedor = 2;
            if (IsKeyPressed(KEY_FOUR)) torrentProvedor = 3;
            if (IsKeyPressed(KEY_FIVE)) torrentProvedor = 4;
            if (IsKeyPressed(KEY_SIX)) torrentProvedor = 5;
            if (IsKeyPressed(KEY_SEVEN)) torrentProvedor = 6;
            if (IsKeyPressed(KEY_EIGHT)) torrentProvedor = 7;
            if (IsKeyPressed(KEY_ENTER)) { if (torrentSel >= 0 && torrentSel < torrentTotal) torrent_abrir_magnet(torrentSel); }
            if (IsKeyPressed(KEY_D)) { if (torrentSel >= 0 && torrentSel < torrentTotal) torrent_baixar_torrent(torrentSel); }
            if (IsKeyPressed(KEY_F) && !torrentBuscando) { torrentTotal = 0; torrentPagina = 0; torrentSel = 0; torrent_buscar_provedor(torrentProvedor, torrentTermo); }
        }

        if (currentState == MENU_BAIXAR_ANIME || currentState == MENU_BAIXAR_MUSICA ||
            currentState == MENU_BAIXAR_LIVROS || currentState == MENU_BAIXAR_LEGENDAS) {
            if (midiaTyping) {
                int key = GetCharPressed();
                while (key > 0) { int l = (int)strlen(midiaBusca); if (l < 255 && key >= 32 && key <= 125) { midiaBusca[l] = (char)key; midiaBusca[l+1] = 0; } key = GetCharPressed(); }
                if (IsKeyPressed(KEY_BACKSPACE)) { int l = (int)strlen(midiaBusca); if (l > 0) midiaBusca[l-1] = 0; }
                if (IsKeyPressed(KEY_ENTER)) { midiaTyping = false; if (midiaBusca[0]) { midiaTotal = 0; void (*b)(const char*) = midia_buscar_anime; if (currentState == MENU_BAIXAR_MUSICA) b = midia_buscar_musica; else if (currentState == MENU_BAIXAR_LIVROS) b = midia_buscar_livro; else if (currentState == MENU_BAIXAR_LEGENDAS) b = midia_buscar_legenda; b(midiaBusca); } }
                if (IsKeyPressed(KEY_ESCAPE)) midiaTyping = false;
            } else {
                if (IsKeyPressed(KEY_T)) midiaTyping = true;
                if (IsKeyPressed(KEY_B) || IsKeyPressed(KEY_ESCAPE)) { currentState = MENU_BAIXAR_MIDIA; scrollY = 0; }
                if (IsKeyPressed(KEY_DOWN)) { if (midiaSel < midiaTotal - 1) midiaSel++; }
                if (IsKeyPressed(KEY_UP)) { if (midiaSel > 0) midiaSel--; }
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_O)) { if (midiaSel >= 0 && midiaSel < midiaTotal) midia_abrir(midiaSel); }
                if (IsKeyPressed(KEY_D)) { if (midiaSel >= 0 && midiaSel < midiaTotal) midia_baixar(midiaSel); }
                if (IsKeyPressed(KEY_F) && !midiaBuscando) { midiaTotal = 0; void (*b)(const char*) = midia_buscar_anime; if (currentState == MENU_BAIXAR_MUSICA) b = midia_buscar_musica; else if (currentState == MENU_BAIXAR_LIVROS) b = midia_buscar_livro; else if (currentState == MENU_BAIXAR_LEGENDAS) b = midia_buscar_legenda; b(midiaBusca[0] ? midiaBusca : " "); }
                if (currentState == MENU_BAIXAR_ANIME && IsKeyPressed(KEY_X) && !midiaBuscando) { currentState = MENU_BAIXAR_ANIME_TORRENT; midiaTotal = 0; midiaSel = 0; midiaScroll = 0; midia_buscar_anime_torrent(midiaBusca[0] ? midiaBusca : " "); }
                float wv = GetMouseWheelMove();
                if (wv != 0) midiaScroll += (int)(wv * 30);
                if (midiaScroll < 0) midiaScroll = 0;
            }
        }

        if (currentState == MENU_BAIXAR_ANIME_TORRENT) {
            if (IsKeyPressed(KEY_B) || IsKeyPressed(KEY_ESCAPE)) { currentState = MENU_BAIXAR_ANIME; scrollY = 0; }
            if (IsKeyPressed(KEY_DOWN)) { if (midiaSel < midiaTotal - 1) midiaSel++; }
            if (IsKeyPressed(KEY_UP)) { if (midiaSel > 0) midiaSel--; }
            if (IsKeyPressed(KEY_ENTER)) { if (midiaSel >= 0 && midiaSel < midiaTotal) midia_abrir(midiaSel); }
            if (IsKeyPressed(KEY_D)) { if (midiaSel >= 0 && midiaSel < midiaTotal) midia_baixar(midiaSel); }
            float wv = GetMouseWheelMove();
            if (wv != 0) midiaScroll += (int)(wv * 30);
            if (midiaScroll < 0) midiaScroll = 0;
        }

        if (currentState == MENU_BAIXAR_FILMES || currentState == MENU_BAIXAR_FILMES_BAIXADOS) {
            float wv = GetMouseWheelMove();
            if (wv != 0) tmdbSinopseScroll -= (int)(wv * 20);
        }

        if (hasScroll) {
            Rectangle track = { 760, 65, 20, screenHeight - 130 };
            float proportion = (float)maxVisH / (float)totalH; float thumbHeight = track.height * proportion; if (thumbHeight < 20) thumbHeight = 20;
            if (currentState == MENU_MIDIA_RADIO_LISTA) {
                DrawRectangleRec(track, scrollBarColor);
                float scrollPerc = (float)scrollY / (float)minScroll; float thumbY = track.y + (scrollPerc * (track.height - thumbHeight));
                Rectangle thumb = { 760, thumbY, 20, thumbHeight }; DrawRectangleRec(thumb, textLight);
            } else {
                DrawRectangleRec(track, scrollBarColor);
                float scrollPerc = (float)scrollY / (float)minScroll; float thumbY = track.y + (scrollPerc * (track.height - thumbHeight));
                Rectangle thumb = { 760, thumbY, 20, thumbHeight }; DrawRectangleRec(thumb, textLight);
            }
        }

        if (currentState == MENU_BLOCO_DE_NOTAS) {
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){ 10, 15, 30, 240 });
            char title[256]; sprintf(title, "BLOCO DE NOTAS: %s", arquivoAbertoNotepad);
            DrawText(title, 80, 20, 20, (Color){ 0, 170, 255, 255 });
            
            Rectangle btnVoltar = { 80, 50, 120, 35 };
            bool hV = CheckCollisionPointRec(mousePoint, btnVoltar);
            DrawRectangleRec(btnVoltar, hV ? (Color){ 80, 80, 80, 255 } : (Color){ 40, 40, 40, 200 });
            DrawRectangleLinesEx(btnVoltar, 2, hV ? WHITE : (Color){ 60, 60, 60, 255 });
            DrawText("VOLTAR", btnVoltar.x + 25, btnVoltar.y + 10, 16, WHITE);
            if (hV && clicked) { currentState = MENU_MIDIA_ARQUIVOS; lerPastaArquivos(); scrollY = 0; }
            
            Rectangle btnSalvar = { 220, 50, 150, 35 };
            bool hS = CheckCollisionPointRec(mousePoint, btnSalvar);
            DrawRectangleRec(btnSalvar, hS ? (Color){ 50, 200, 50, 255 } : (Color){ 30, 100, 40, 200 });
            DrawRectangleLinesEx(btnSalvar, 2, hS ? WHITE : (Color){ 50, 150, 60, 255 });
            DrawText("SALVAR", btnSalvar.x + 40, btnSalvar.y + 10, 16, WHITE);
            if (hS && clicked) { salvarNotepad(); currentState = MENU_MIDIA_ARQUIVOS; lerPastaArquivos(); scrollY = 0; }
            
            Rectangle btnNova = { 390, 50, 150, 35 };
            bool hN = CheckCollisionPointRec(mousePoint, btnNova);
            DrawRectangleRec(btnNova, hN ? (Color){ 50, 150, 255, 255 } : (Color){ 30, 45, 70, 200 });
            DrawRectangleLinesEx(btnNova, 2, hN ? WHITE : (Color){ 40, 60, 90, 255 });
            DrawText("NOVA LINHA", btnNova.x + 25, btnNova.y + 10, 16, WHITE);
            if (hN && clicked && totalLinhasNotepad < 500) {
                totalLinhasNotepad++;
                linhasNotepad[totalLinhasNotepad - 1][0] = '\0';
                scrollNotepad -= 35; 
            }
            
            int totalH = totalLinhasNotepad * 35;
            int maxVisH = screenHeight - 120;
            int minScr = maxVisH - totalH; if (minScr > 0) minScr = 0;
            if (scrollNotepad < minScr) scrollNotepad = minScr;
            if (scrollNotepad > 0) scrollNotepad = 0;
            
            float mw = GetMouseWheelMove();
            if (mw != 0) scrollNotepad += (int)(mw * 35);
            
            BeginScissorMode(80, 100, 640, maxVisH);
            for (int i = 0; i < totalLinhasNotepad; i++) {
                int py = 100 + (i * 35) + scrollNotepad;
                if (py > 50 && py < screenHeight) {
                    Rectangle lineBtn = { 80, py, 640, 30 };
                    bool hL = CheckCollisionPointRec(mousePoint, lineBtn);
                    DrawRectangleRec(lineBtn, hL ? (Color){ 50, 60, 80, 200 } : (Color){ 20, 30, 40, 150 });
                    DrawRectangleLinesEx(lineBtn, 1, hL ? (Color){ 100, 150, 200, 255 } : (Color){ 40, 50, 60, 255 });
                    
                    char num[16]; sprintf(num, "%03d:", i + 1);
                    DrawText(num, lineBtn.x + 10, lineBtn.y + 8, 16, (Color){ 100, 150, 255, 255 });
                    DrawText(linhasNotepad[i], lineBtn.x + 50, lineBtn.y + 8, 16, WHITE);
                    
                    if (hL && clicked) {
                        searchTargetMode = 9;
                        strcpy(globalSearchQuery, linhasNotepad[i]);
                        globalSearchCount = strlen(globalSearchQuery);
                        linhaSelecionadaNotepad = i;
                        currentState = MENU_PESQUISA_GLOBAL;
                        scrollY = 0;
                    }
                }
            }
            EndScissorMode();
            
            if (minScr < 0) {
                Rectangle tck = { 740, 100, 15, maxVisH }; DrawRectangleRec(tck, (Color){ 30, 30, 40, 255 });
                float pp = (float)maxVisH / (float)totalH; float tH = tck.height * pp; if (tH < 20) tH = 20;
                float sp = (float)scrollNotepad / (float)minScr; float tY = tck.y + (sp * (tck.height - tH));
                Rectangle thm = { 740, tY, 15, tH }; DrawRectangleRec(thm, WHITE);
            }
        }

        if (currentState == MENU_PESQUISA_GLOBAL) {
            // Fundo semi-transparente estilo PS4
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){ 10, 15, 30, 240 });
            
            const char* title = "Pesquisar";
            if (searchTargetMode == 0) title = "Filtrar Pasta Local";
            else if (searchTargetMode == 1) title = "Pesquisar Rádio (Nome)";
            else if (searchTargetMode == 2) title = "Pesquisar Rádio (Gênero/Tag)";
            else if (searchTargetMode == 3) title = "Pesquisar Rádio (País)";
            else if (searchTargetMode == 4) title = "Pesquisar Rádio (Estado/Região)";
            else if (searchTargetMode == 5) title = "Pesquisar Rádio (Idioma)";
            else if (searchTargetMode == 6) title = "Nome da Nova Pasta";
            else if (searchTargetMode == 7) title = "Nome do Novo Arquivo";
            else if (searchTargetMode == 8) title = "Renomear Arquivo/Pasta";
            else if (searchTargetMode == 9) title = "Editar Linha de Texto";
            
            DrawText(title, 80, 50, 24, textLight);
            
            int key = GetCharPressed();
            while (key > 0) {
                if ((key >= 32) && (key <= 125) && (globalSearchCount < 63)) {
                    globalSearchQuery[globalSearchCount] = (char)key;
                    globalSearchQuery[globalSearchCount + 1] = '\0';
                    globalSearchCount++;
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE)) {
                globalSearchCount--;
                if (globalSearchCount < 0) globalSearchCount = 0;
                globalSearchQuery[globalSearchCount] = '\0';
            }
            
            // Caixa de texto estilo console
            DrawRectangle(80, 90, 640, 50, (Color){ 20, 30, 50, 255 });
            DrawRectangleLines(80, 90, 640, 50, (Color){ 50, 150, 255, 255 });
            DrawText(globalSearchQuery, 100, 105, 24, WHITE);
            // Cursor piscando
            if ((int)(GetTime() * 2) % 2 == 0) DrawText("_", 100 + MeasureText(globalSearchQuery, 24), 105, 24, WHITE);
            
            // PS4 Virtual Keyboard Grid
            const char keys[4][12] = {
                "1234567890-",
                "QWERTYUIOP",
                "ASDFGHJKL",
                "ZXCVBNM"
            };
            
            int startY = 180;
            for (int row = 0; row < 4; row++) {
                int startX = 80 + (row * 15); // Stagger keys slightly
                int len = strlen(keys[row]);
                for (int col = 0; col < len; col++) {
                    Rectangle kBtn = { startX + (col * 50), startY + (row * 50), 45, 45 };
                    bool hK = CheckCollisionPointRec(mousePoint, kBtn);
                    DrawRectangleRec(kBtn, hK ? (Color){ 50, 150, 255, 255 } : (Color){ 30, 45, 70, 200 });
                    DrawRectangleLinesEx(kBtn, 2, hK ? WHITE : (Color){ 40, 60, 90, 255 });
                    
                    char lbl[2] = { keys[row][col], '\0' };
                    DrawText(lbl, kBtn.x + 15, kBtn.y + 12, 20, WHITE);
                    
                    if (hK && clicked && globalSearchCount < 63) {
                        globalSearchQuery[globalSearchCount] = keys[row][col];
                        globalSearchQuery[globalSearchCount + 1] = '\0';
                        globalSearchCount++;
                    }
                }
            }
            
            // Action Keys
            Rectangle kSpace = { 80, startY + 200, 240, 45 };
            bool hSpace = CheckCollisionPointRec(mousePoint, kSpace);
            DrawRectangleRec(kSpace, hSpace ? (Color){ 50, 150, 255, 255 } : (Color){ 30, 45, 70, 200 });
            DrawRectangleLinesEx(kSpace, 2, hSpace ? WHITE : (Color){ 40, 60, 90, 255 });
            DrawText("ESPACO", kSpace.x + 80, kSpace.y + 12, 20, WHITE);
            if (hSpace && clicked && globalSearchCount < 63) {
                globalSearchQuery[globalSearchCount] = ' ';
                globalSearchQuery[globalSearchCount + 1] = '\0';
                globalSearchCount++;
            }
            
            Rectangle kDel = { 330, startY + 200, 100, 45 };
            bool hDel = CheckCollisionPointRec(mousePoint, kDel);
            DrawRectangleRec(kDel, hDel ? (Color){ 200, 50, 50, 255 } : (Color){ 100, 30, 40, 200 });
            DrawRectangleLinesEx(kDel, 2, hDel ? WHITE : (Color){ 150, 50, 60, 255 });
            DrawText("DEL", kDel.x + 30, kDel.y + 12, 20, WHITE);
            if (hDel && clicked && globalSearchCount > 0) {
                globalSearchCount--;
                globalSearchQuery[globalSearchCount] = '\0';
            }
            
            Rectangle kEnter = { 440, startY + 200, 180, 45 };
            bool hEnter = CheckCollisionPointRec(mousePoint, kEnter);
            DrawRectangleRec(kEnter, hEnter ? (Color){ 50, 200, 50, 255 } : (Color){ 30, 100, 40, 200 });
            DrawRectangleLinesEx(kEnter, 2, hEnter ? WHITE : (Color){ 50, 150, 60, 255 });
            DrawText("CONFIRMAR", kEnter.x + 35, kEnter.y + 12, 20, WHITE);
            
            Rectangle btnVoltarP = { 80, startY + 260, 150, 45 };
            bool hoverVoltarP = CheckCollisionPointRec(mousePoint, btnVoltarP);
            DrawRectangleRec(btnVoltarP, hoverVoltarP ? (Color){ 80, 80, 80, 255 } : (Color){ 40, 40, 40, 200 });
            DrawRectangleLinesEx(btnVoltarP, 2, hoverVoltarP ? WHITE : (Color){ 60, 60, 60, 255 });
            DrawText("VOLTAR", btnVoltarP.x + 40, btnVoltarP.y + 12, 20, WHITE);
            if (hoverVoltarP && clicked) {
                currentState = previousStateForSearch;
                globalSearchQuery[0] = '\0'; // clear search on cancel
                globalSearchCount = 0;
            }
            
            if ((hEnter && clicked) || IsKeyPressed(KEY_ENTER)) {
                if (searchTargetMode == 0) { // Local filter OR AI Command
                    char q[256];
                    strncpy(q, globalSearchQuery, 255); q[255] = '\0';
                    for(int i=0; q[i]; i++) q[i] = tolower(q[i]);

                    bool handledAsCommand = false;
                    // Check for intent to open or go to something
                    if (strstr(q, "abra ") || strstr(q, "abrir ") || strstr(q, "ir para ") || strstr(q, "vai para ")) {
                        handledAsCommand = true;
                        if (strstr(q, "ferramenta")) { initFerramentas(); currentState = MENU_FERRAMENTAS; }
                        else if (strstr(q, "nota") || strstr(q, "bloco")) currentState = MENU_BLOCO_DE_NOTAS;
                        else if (strstr(q, "midia") || strstr(q, "mídia") || strstr(q, "música") || strstr(q, "video")) currentState = MENU_MIDIA;
                        else if (strstr(q, "baixar") || strstr(q, "download")) currentState = MENU_BAIXAR;
                        else if (strstr(q, "radio") || strstr(q, "rádio")) currentState = MENU_MIDIA_RADIO_CATEGORIA;
                        else if (strstr(q, "config")) currentState = MENU_CONFIGURACOES;
                        else if (strstr(q, "explorar") || strstr(q, "arquivo")) currentState = MENU_EXPLORAR;
                        else if (strstr(q, "chrome") || strstr(q, "google") || strstr(q, "navegador")) { exec_async_silent("cmd /c start chrome"); currentState = previousStateForSearch; }
                        else if (strstr(q, "calculadora") || strstr(q, "calc")) { exec_async_silent("cmd /c calc"); currentState = previousStateForSearch; }
                        else if (strstr(q, "notepad") || strstr(q, "bloco de notas do windows")) { exec_async_silent("cmd /c notepad"); currentState = previousStateForSearch; }
                        else handledAsCommand = false;
                    }

                    if (handledAsCommand) {
                        globalSearchQuery[0] = '\0';
                        globalSearchCount = 0;
                    } else {
                        currentState = previousStateForSearch;
                    }
                    scrollY = 0;
                } else if (searchTargetMode >= 1 && searchTargetMode <= 5 && globalSearchCount > 0) {
                    if (searchTargetMode == 1) radio_fetch_search(globalSearchQuery);
                    else if (searchTargetMode == 2) radio_fetch_tag(globalSearchQuery);
                    else if (searchTargetMode == 3) radio_fetch_country(globalSearchQuery);
                    else if (searchTargetMode == 4) radio_fetch_state(globalSearchQuery);
                    else if (searchTargetMode == 5) radio_fetch_language(globalSearchQuery);
                    
                    currentState = MENU_MIDIA_RADIO_LISTA;
                    scrollY = 0;
                } else if (searchTargetMode >= 6) {
                    if (searchTargetMode == 6) {
                        char newPath[1024]; sprintf(newPath, "%s/%s", caminhoArquivos, globalSearchQuery);
                        mkdir(newPath); 
                        lerPastaArquivos();
                        currentState = MENU_MIDIA_ARQUIVOS; scrollY = 0;
                        globalSearchQuery[0] = '\0'; globalSearchCount = 0;
                    } else if (searchTargetMode == 7) {
                        char newPath[1024]; 
                        if (strchr(globalSearchQuery, '.') != NULL) {
                            sprintf(newPath, "%s/%s", caminhoArquivos, globalSearchQuery);
                        } else {
                            sprintf(newPath, "%s/%s.txt", caminhoArquivos, globalSearchQuery);
                        }
                        FILE* f = fopen(newPath, "w"); if (f) fclose(f); 
                        lerPastaArquivos();
                        abrirNotepad(newPath);
                        currentState = MENU_BLOCO_DE_NOTAS;
                        globalSearchQuery[0] = '\0'; globalSearchCount = 0;
                    } else if (searchTargetMode == 8) {
                        char newPath[1024];
                        char* base = strrchr(targetFilePath, '/'); if (!base) base = strrchr(targetFilePath, '\\');
                        if (base) {
                            char dirPath[1024]; strcpy(dirPath, targetFilePath);
                            dirPath[base - targetFilePath] = '\0';
                            sprintf(newPath, "%s/%s", dirPath, globalSearchQuery);
                        } else {
                            sprintf(newPath, "%s/%s", caminhoArquivos, globalSearchQuery);
                        }
                        rename(targetFilePath, newPath);
                        lerPastaArquivos();
                        currentState = MENU_MIDIA_ARQUIVOS; scrollY = 0;
                        globalSearchQuery[0] = '\0'; globalSearchCount = 0;
                    } else if (searchTargetMode == 9) {
                        strncpy(linhasNotepad[linhaSelecionadaNotepad], globalSearchQuery, 255);
                        currentState = MENU_BLOCO_DE_NOTAS;
                        globalSearchQuery[0] = '\0'; globalSearchCount = 0;
                    }
                }
            }
        }

        if (isFullScreenImage && viewerImageLoaded) {
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){ 0, 0, 0, 240 });
            
            float scaleFs = 1.0f;
            if (viewerImage.width > screenWidth - 40 || viewerImage.height > screenHeight - 40) {
                float scaleX = (screenWidth - 40.0f) / viewerImage.width;
                float scaleY = (screenHeight - 40.0f) / viewerImage.height;
                scaleFs = (scaleX < scaleY) ? scaleX : scaleY;
            }
            float drawX = (screenWidth - (viewerImage.width * scaleFs)) / 2.0f;
            float drawY = (screenHeight - (viewerImage.height * scaleFs)) / 2.0f;
            
            DrawTextureEx(viewerImage, (Vector2){ drawX, drawY }, 0.0f, scaleFs, WHITE);
            
            Rectangle closeBtn = { screenWidth - 50, 10, 40, 40 };
            bool hClose = CheckCollisionPointRec(mousePoint, closeBtn);
            DrawRectangleRec(closeBtn, hClose ? RED : MAROON);
            DrawText("X", (int)closeBtn.x + 13, (int)closeBtn.y + 10, 20, WHITE);
        }

        if (isFullScreenText) {
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){ 0, 0, 0, 240 });
            
            if (pdfLoaded) {
                float scaleFs = 1.0f;
                if (pdfTexture.width > screenWidth - 40 || pdfTexture.height > screenHeight - 40) {
                    float scaleX = (screenWidth - 40.0f) / pdfTexture.width;
                    float scaleY = (screenHeight - 40.0f) / pdfTexture.height;
                    scaleFs = (scaleX < scaleY) ? scaleX : scaleY;
                }
                float drawX = (screenWidth - (pdfTexture.width * scaleFs)) / 2.0f;
                float drawY = (screenHeight - (pdfTexture.height * scaleFs)) / 2.0f;
                
                DrawTextureEx(pdfTexture, (Vector2){ drawX, drawY }, 0.0f, scaleFs, WHITE);

                Rectangle btnPrevPdf = { 50, screenHeight - 50, 100, 40 };
                bool hPrevPdf = CheckCollisionPointRec(mousePoint, btnPrevPdf);
                DrawRectangleRec(btnPrevPdf, hPrevPdf ? btnHover : btnNormal);
                DrawText("<< PAG", 65, screenHeight - 40, 16, textLight);
                if (hPrevPdf && clicked && pdfPaginaAtual > 1) {
                    carregarPaginaPDF(pdfPaginaAtual - 1);
                    clicked = false;
                }
                
                Rectangle btnNextPdf = { screenWidth - 150, screenHeight - 50, 100, 40 };
                bool hNextPdf = CheckCollisionPointRec(mousePoint, btnNextPdf);
                DrawRectangleRec(btnNextPdf, hNextPdf ? btnHover : btnNormal);
                DrawText("PAG >>", screenWidth - 135, screenHeight - 40, 16, textLight);
                if (hNextPdf && clicked && pdfPaginaAtual < pdfTotalPaginas) {
                    carregarPaginaPDF(pdfPaginaAtual + 1);
                    clicked = false;
                }
                
                char fspag[64]; sprintf(fspag, "Pagina %d de %d", pdfPaginaAtual, pdfTotalPaginas);
                DrawText(fspag, (screenWidth / 2) - 60, screenHeight - 40, 16, WHITE);

            } else if (viewerText) {
                if (wheelMove != 0) {
                    viewerTextScrollY += (int)(wheelMove * 40);
                    if (viewerTextScrollY > 0) viewerTextScrollY = 0;
                }
                BeginScissorMode(40, 40, screenWidth - 80, screenHeight - 80);
                DrawText(viewerText, 50, 50 + viewerTextScrollY, 18, textLight);
                EndScissorMode();
            }
            
            Rectangle closeBtn = { screenWidth - 50, 10, 40, 40 };
            bool hClose = CheckCollisionPointRec(mousePoint, closeBtn);
            DrawRectangleRec(closeBtn, hClose ? RED : MAROON);
            DrawText("X", (int)closeBtn.x + 13, (int)closeBtn.y + 10, 20, WHITE);
        }

        if (currentState == MENU_INSTRUMENTOS) {
            renderizarInstrumentosRaylib();
            if (IsKeyPressed(KEY_ESCAPE)) {
                fecharInstrumentos();
                currentState = MENU_EXTRA;
                scrollY = 0;
            }
        }
        
        if (currentState == MENU_CONTROLE_TESTE) {
            renderizarControleTesteRaylib();
            if (IsKeyPressed(KEY_ESCAPE)) {
                fecharControleTeste();
                currentState = MENU_EXTRA;
                scrollY = 0;
            }
        }

        if (currentState == MENU_TECLADO_TESTE) {
            renderizarTecladoTeste();
        }

        if (currentState == MENU_FERRAMENTAS && !isFullScreenImage) {
            renderizarFerramentasRaylib();
            if (IsKeyPressed(KEY_ESCAPE)) {
                if (deveFecharFerramentas()) {
                    fecharFerramentas();
                    currentState = ferramentasViaAbaPrincipal ? MENU_MAIN : MENU_EXTRA;
                    ferramentasViaAbaPrincipal = false;
                    scrollY = 0;
                }
            }
        }

        if (currentState == MENU_VIDEO) {
            VideoPlayer_Draw();
        }

        if (notifyTimer > 0) {
            notifyTimer--;
            int alpha = notifyTimer < 30 ? notifyTimer * 8 : 255;
            Color nColor = (Color){30, 30, 40, (unsigned char)alpha};
            DrawRectangle(0, screenHeight-50, screenWidth, 50, nColor);
            DrawRectangle(0, screenHeight-50, screenWidth, 2, (Color){60, 120, 255, (unsigned char)alpha});
            DrawText(notifyMsg, 20, screenHeight-35, 14, (Color){220, 220, 255, (unsigned char)alpha});
        }

        EndDrawing();
    }

    VideoPlayer_Close();
    if (viewerText) UnloadFileText(viewerText);
    fecharPDF();
    if (viewerImageLoaded) UnloadTexture(viewerImage);
    for (int i = 0; i < coverCount; i++) UnloadTexture(coverTextures[i]);
    if (musicLoaded) UnloadMusicStream(currentMusic);
    
    UnloadAudioStream(radioStream);
    UnloadAudioStream(instStream);
    fecharInstrumentos();
    fecharControleTeste();
    fecharTecladoTeste();
    fecharFerramentas();
    radio_close();
    baixar_fila_aguardar();
    baixar_loja_cancelar();
    
    CloseAudioDevice(); CloseWindow(); return 0;
}
