#include "ferramentas.h"
#include "raylib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <pthread.h>
#ifdef _WIN32
#include <direct.h>
extern int __stdcall Beep(unsigned int, unsigned int);
extern void exec_silent(const char* cmd);
extern void exec_async_silent(const char* cmd);
extern bool grav_start_recording(const char* outputFile);
extern bool grav_screen_start_recording(const char* outputFile);
extern bool grav_video_start_recording(const char* outputFile);
extern bool grav_video_screen_start_recording(const char* outputFile);
extern bool grav_concat_mp4(const char* part1, const char* part2, const char* out);
extern int grav_video_preview_w(void);
extern int grav_video_preview_h(void);
extern int grav_video_preview_get(unsigned char* outBuf, int bufSize);
extern int grav_video_preview_active(void);
extern void grav_get_exe_dir(char* out, int maxLen);
extern void tocarMusicaNova(const char* path);
extern void abrirVideoNoApp(const char* path);
extern void abrirImagemNoApp(const char* path);
extern bool grav_capture_screen_to_png(const char* outputPath, bool activeWindowOnly);
extern bool grav_capture_camera_to_png(const char* outputPath);
extern bool grav_print_preview_start(int source);
extern void grav_print_preview_stop(void);
extern void grav_stop_recording();
extern bool grav_is_recording();
extern long grav_temp_file_size(const char* path);
extern void grav_pause_recording();
extern void grav_resume_recording();
extern void grav_set_mic_mute(bool muted);
extern void grav_kill_orphans();
extern bool grav_finish_recording(const char* rawPath, const char* finalPath);
extern const char* grav_last_error_msg();
#else
#include <sys/types.h>
#endif

#include "gemini_ia.h"

// RenderTexture for drawing at 1920x1080 and scaling to screen
static RenderTexture2D toolsRenderTexture;
static bool toolsRenderTextureLoaded = false;

extern void voltarParaMenuExtra(void);

// Sub-modes
typedef enum {
    TOOL_MENU,
    TOOL_CALCULADORA,
    TOOL_NOTEPAD,
    TOOL_TIMER,
    TOOL_GRAVADOR,
    TOOL_CONTATOS,
    TOOL_CAIXA,
	TOOL_TERMINAL,
	TOOL_IA,
	TOOL_QRCODE
} ToolState;

// ===== QR / CODIGO DE BARRAS / WHATSAPP / LEITOR =====
#define QR_MAX_LISTA 80
static int qrTab = 0; // 0=QR, 1=Barras, 2=WhatsApp, 3=Ler
static char qrDir[512] = "";
static char qrInput[1024] = "";
static bool qrFoco = false;
static char brDados[256] = "";
static char brLabel[128] = "";
static int brFoco = 0; // 0 nenhum, 1 dados, 2 label
static char wpFone[32] = "";
static char wpMsg[1024] = "";
static int wpFoco = 0; // 0 nenhum, 1 fone, 2 msg
static char wpLink[1600] = "";
static bool wpTemLink = false;
static Texture2D qrTex = { 0 };
static bool qrTexOk = false;
static char qrLastPng[512] = "";
static char qrMsg[512] = "";
static bool qrMsgErro = false;
// Leitor
static int lerModo = 0; // 0 idle, 1 camera rodando, 2 imagem
static bool lerPrevOn = false;
static Texture2D lerCamTex = { 0 };
static bool lerCamTexOk = false;
static double lerCamLastLoad = 0.0;
static char lerResultado[4096] = "";
static bool lerTemResult = false;
static volatile bool scanRunning = false;
static volatile bool scanStopReq = true;
static pthread_t scanTid;
static bool scanTidValid = false;
static char scanPng[512] = "";
static char scanView[512] = "";
static char scanOut[512] = "";
static char lerPathIn[512] = "";
static bool lerFocoPath = false;
static char lerLista[QR_MAX_LISTA][260];
static int lerTotal = 0;
static int lerScroll = 0;

static ToolState toolMode = TOOL_MENU;
static int menuSelection = 0; // 0 = Calculadora, 1 = Bloco de Notas, 2 = Gravador, 3 = Relógio

typedef enum {
    NOTEPAD_EDIT,
    NOTEPAD_OPEN_EXPLORER,
    NOTEPAD_SAVE_DIALOG
} NotepadSubState;

static NotepadSubState notepadState = NOTEPAD_EDIT;

// File selector variables for Notepad
static char noteCaminhoAtual[512] = "data/HyperNeiva/midia/leitura";
static char noteNomes[200][64];
static char notePaths[200][256];
static bool noteEhPasta[200];
static int noteTotalItens = 0;
static int noteSel = 0;
static int noteScroll = 0;
#define MAX_GRAVACOES 128
static char g_gravDir[512] = "data/HyperNeiva/gravacoes/";
static char g_gravTemp[512] = "data/HyperNeiva/gravacoes/grav_temp.raw";
static char g_gravTelaTemp[512] = "data/HyperNeiva/gravacoes/grav_tela_temp.mp4";
static char g_gravVideoTemp[512] = "data/HyperNeiva/gravacoes/grav_video_temp.mp4";
#define GRAV_DIR g_gravDir
#define GRAV_TEMP_RAW g_gravTemp
#define GRAV_TELA_TEMP g_gravTelaTemp
#define GRAV_VIDEO_TEMP g_gravVideoTemp
#define GRAV_PREVIEW_W 320
#define GRAV_PREVIEW_H 240
#define GRAV_PREVIEW_FRAME (GRAV_PREVIEW_W * GRAV_PREVIEW_H * 4)
static Texture2D gravPreviewTex;
static bool gravPreviewTexLoaded = false;
static Texture2D gravIconMic;
static Texture2D gravIconCam;
static Texture2D gravIconTela;
static Texture2D gravIconPause;
static Texture2D gravIconPlay;
static Texture2D gravIconMicOff;
static Texture2D gravIconDelete;
static Texture2D gravIconBack;
static bool gravIconsLoaded = false;
static bool gravMicMuted = false;
static bool gravVideoHasScreen = false;
static int gravDeleteIdx = -1;
static int gravState = 0; // 0=idle, 1=naming-before, 2=recording, 3=paused, 4=error, 5=naming-after
static int gravTipo = 0;  // 0=audio, 1=video, 2=tela, 3=print/foto
static int printFonte = 0; // 0=camera, 1=tela
static int printModo = 0;  // 0=tela inteira, 1=janela ativa
static bool printPreviewOn = false;
static bool gravShowMenu = true;
static char gravErrorMsg[256];
static double gravErrorTimer;
static char gravNomeInput[128];
static char gravList[MAX_GRAVACOES][260];
static int gravTotal = 0;
static int gravScroll = 0;
static double gravRecStartTime;
static long gravLastSize = 0;
static double gravLastSizeTime = 0;
static double gravLastGrowthTime = 0;
static char saveFilename[128] = "";

// IA / GEMINI STATE
#define IA_MAX_LISTA 300
static int iaTab = 0; // 0=Texto, 1=Visao, 2=Gerar Imagem, 3=Audio->Texto, 4=Texto->Audio
static int iaLastTab = -1;
static char iaPrompt[4096];
static int iaPromptLen = 0;
static bool iaFocoPrompt = false;
static bool iaFocoPrompt2 = false;
static char iaPrompt2[4096];
static int iaPrompt2Len = 0;
static char iaResposta[16384];
static bool iaTemResposta = false;
static int iaScroll = 0;
static bool iaOcupado = false;
static char iaStatus[256];
static char iaErro[512];
static bool iaTemErro = false;
static int iaModeloIdx = 0;
static int iaImagemModelo = 0;
static int iaAspecto = 0;
static int iaVozIdx = 0;
static int iaAudioSel = -1;
static int iaImagemSel = -1;
static int iaArquivoScroll = 0;
static char iaAudioList[IA_MAX_LISTA][512];
static int iaAudioTotal = 0;
static char iaImagemList[IA_MAX_LISTA][512];
static int iaImagemTotal = 0;
static bool iaMostrarImagem = false;
static Texture2D iaResTex = { 0 };
static bool iaResTexLoaded = false;
static GeminiResposta iaResp;
static int iaRespTipo = -1;
static bool iaMicGravando = false;
static double iaMicInicio = 0.0;
static char iaMicRaw[512] = "";
static char iaMicWav[512] = "";

static const char* iaModelosTexto[] = { "gemini-3.5-flash", "gemini-3.5-flash-lite", "gemini-3.1-flash-lite" };
static const char* iaModelosImagem[] = { "gemini-3.1-flash-image", "gemini-3-pro-image-preview" };
static const char* iaAspectos[] = { "1:1", "16:9", "9:16", "3:4", "4:3" };
static const char* iaVozes[] = { "Kore", "Puck", "Zephyr", "Aoede", "Leda", "Charon" };

// CALCULADORA STATE & LOGIC
static char calcBuffer[128] = "";
static char calcDisplay[128] = "0";
static double calcPrevValue = 0.0;
static char calcOp = '\0';
static bool calcResetOnNext = false;

static double applyOp(double a, double b, char op) {
    switch (op) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/': return (b != 0.0) ? (a / b) : 0.0;
        default: return b;
    }
}

static void handleCalcInput(const char* val) {
    if (calcResetOnNext) {
        calcDisplay[0] = '\0';
        calcResetOnNext = false;
    }
    if (strcmp(calcDisplay, "0") == 0 && strcmp(val, ".") != 0) {
        calcDisplay[0] = '\0';
    }
    if (strlen(calcDisplay) < 20) {
        strcat(calcDisplay, val);
    }
}

static void handleCalcOp(char op) {
    if (calcDisplay[0] != '\0' && !calcResetOnNext) {
        double current = atof(calcDisplay);
        if (calcOp != '\0') {
            calcPrevValue = applyOp(calcPrevValue, current, calcOp);
        } else {
            calcPrevValue = current;
        }
        calcOp = op;
        sprintf(calcBuffer, "%g %c", calcPrevValue, calcOp);
        sprintf(calcDisplay, "%g", calcPrevValue);
        calcResetOnNext = true;
    } else {
        calcOp = op;
        sprintf(calcBuffer, "%g %c", calcPrevValue, calcOp);
    }
}

static void handleCalcEqual(void) {
    if (calcOp != '\0' && calcDisplay[0] != '\0') {
        double current = atof(calcDisplay);
        double result = applyOp(calcPrevValue, current, calcOp);
        sprintf(calcBuffer, "%g %c %g =", calcPrevValue, calcOp, current);
        sprintf(calcDisplay, "%g", result);
        calcPrevValue = result;
        calcOp = '\0';
        calcResetOnNext = true;
    }
}

static void handleCalcClear(void) {
    calcDisplay[0] = '0';
    calcDisplay[1] = '\0';
    calcBuffer[0] = '\0';
    calcPrevValue = 0.0;
    calcOp = '\0';
    calcResetOnNext = false;
}

static void handleCalcBackspace(void) {
    int len = strlen(calcDisplay);
    if (len > 1) {
        calcDisplay[len - 1] = '\0';
    } else {
        calcDisplay[0] = '0';
        calcDisplay[1] = '\0';
    }
}

// BLOCO DE NOTAS STATE & LOGIC
static char notepadText[8192] = "";
static int notepadTextLen = 0;
static float backspaceTimer = 0.0f;
static int notepadScrollY = 0;

static void noteListarArquivos(const char* path) {
    DIR* d = opendir(path);
    if (!d) return;
    noteTotalItens = 0;

    if (strcmp(path, "data/HyperNeiva/midia/leitura") != 0 &&
        strcmp(path, "data/HyperNeiva/midia") != 0 &&
        strcmp(path, "data/HyperNeiva") != 0 &&
        strcmp(path, "data") != 0) {
        strcpy(noteNomes[noteTotalItens], "[ .. Voltar Pasta Anterior ]");
        strcpy(notePaths[noteTotalItens], "..");
        noteEhPasta[noteTotalItens] = true;
        noteTotalItens++;
    }

    struct dirent* dir;
    while ((dir = readdir(d)) != NULL && noteTotalItens < 200) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, dir->d_name);
        struct stat st;
        bool isDir = false;
        if (stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode)) {
            isDir = true;
        }
        if (isDir) {
            snprintf(noteNomes[noteTotalItens], 64, "[ %s ]", dir->d_name);
            strcpy(notePaths[noteTotalItens], fullPath);
            noteEhPasta[noteTotalItens] = true;
            noteTotalItens++;
        } else {
            char temp[256]; strcpy(temp, dir->d_name);
            for (int i = 0; temp[i]; i++) temp[i] = tolower(temp[i]);
            if (strstr(temp, ".txt")) {
                strncpy(noteNomes[noteTotalItens], dir->d_name, 63);
                strcpy(notePaths[noteTotalItens], fullPath);
                noteEhPasta[noteTotalItens] = false;
                noteTotalItens++;
            }
        }
    }
    closedir(d);

    for (int i = (noteTotalItens > 0 && strcmp(notePaths[0], "..") == 0 ? 1 : 0); i < noteTotalItens - 1; i++) {
        for (int j = i + 1; j < noteTotalItens; j++) {
            bool swap = false;
            if (noteEhPasta[i] != noteEhPasta[j]) {
                if (!noteEhPasta[i] && noteEhPasta[j]) swap = true;
            } else {
                if (strcasecmp(noteNomes[i], noteNomes[j]) > 0) swap = true;
            }
            if (swap) {
                char tNome[64]; strcpy(tNome, noteNomes[i]); strcpy(noteNomes[i], noteNomes[j]); strcpy(noteNomes[j], tNome);
                char tPath[256]; strcpy(tPath, notePaths[i]); strcpy(notePaths[i], notePaths[j]); strcpy(notePaths[j], tPath);
                bool tPasta = noteEhPasta[i]; noteEhPasta[i] = noteEhPasta[j]; noteEhPasta[j] = tPasta;
            }
        }
    }
    noteSel = 0;
    noteScroll = 0;
}

static void carregarNotasFerramenta(void) {
    FILE* f = fopen("data/HyperNeiva/notas_ferramenta.txt", "r");
    void abrirTextoNoBlocoNotas(const char* filepath) {
    initFerramentas();
    toolMode = TOOL_NOTEPAD;
    notepadState = NOTEPAD_EDIT;

    FILE* f = fopen(filepath, "r");
    if (f) {
        size_t readBytes = fread(notepadText, 1, sizeof(notepadText) - 1, f);
        notepadText[readBytes] = '\0';
        notepadTextLen = (int)readBytes;
        fclose(f);
    }
}

void ferramentas_abrir_ia(void) {
    initFerramentas();
    toolMode = TOOL_IA;
}

void ferramentas_abrir_calculadora(void) {
    initFerramentas();
    toolMode = TOOL_CALCULADORA;
}

void ferramentas_abrir_terminal(void) {
    initFerramentas();
    toolMode = TOOL_TERMINAL;
}
    if (f) {
        size_t readBytes = fread(notepadText, 1, sizeof(notepadText) - 1, f);
        notepadText[readBytes] = '\0';
        notepadTextLen = (int)readBytes;
        fclose(f);
    } else {
        notepadText[0] = '\0';
        notepadTextLen = 0;
    }
}

static void salvarNotasFerramenta(void) {
    #ifdef _WIN32
    _mkdir("data");
    _mkdir("data/HyperNeiva");
    #else
    mkdir("data", 0777);
    mkdir("data/HyperNeiva", 0777);
    #endif
    FILE* f = fopen("data/HyperNeiva/notas_ferramenta.txt", "w");
    if (f) {
        fwrite(notepadText, 1, strlen(notepadText), f);
        fclose(f);
    }
}

// CRONOMETRO & CONTAGEM REGRESSIVA STATE
static int timerTab = 0; // 0 = Relogio, 1 = Cronometro, 2 = Timer, 3 = Alarme

typedef enum {
    TIMER_STATE_NORMAL,
    TIMER_STATE_OPEN_EXPLORER,
    TIMER_STATE_SAVE_DIALOG,
    TIMER_STATE_EVENTO_EDIT
} TimerSubState;

static TimerSubState timerState = TIMER_STATE_NORMAL;
static char timerCaminhoAtual[512] = "data/HyperNeiva";
static char timerNomes[200][64];
static char timerPaths[200][256];
static bool timerEhPasta[200];
static int timerTotalItens = 0;
static int timerSel = 0;
static int timerScroll = 0;
static char timerSaveFilename[128] = "";

static double chronoElapsed = 0.0;
static double chronoStartTime = 0.0;
static bool chronoRunning = false;
static double chronoLaps[5] = {0};
static int chronoLapCount = 0;

static double timerRemaining = 0.0;
static double timerTotalDuration = 0.0;
static double timerStartTime = 0.0;
static bool timerRunning = false;
static int timerEditField = 0; // 0=horas 1=min 2=seg (edicao por setas)
static float alarmFlashTimer = 0.0f;

// ALARME STATE (lista de alarmes)
#define MAX_ALARMES 30
typedef struct {
    int hora;
    int minuto;
    int segundo;
    bool ativo;
    bool disparado;
} AlarmeItem;
static AlarmeItem alarmes[MAX_ALARMES];
static int alarmeTotal = 0;
static int alarmeSel = 0;
static int alarmeEditField = 0; // 0=hora 1=min 2=seg (edicao por setas)
static bool alarmeEditando = false;
static int alarmUltimoSegundo = -1;
static float alarmRingTimer = 0.0f;
static int alarmeTocando = -1;
static float alarmBeepTimer = 0.0f;
// Calendario: mes/ano exibido (navegacao)
static int calViewMes = -1; // 1..12 (-1 = mes atual)
static int calViewAno = -1;
// Dia selecionado no calendario (para eventos/tarefas)
static int calSelDia = -1, calSelMes = -1, calSelAno = -1;

// ===== CLIMA (Open-Meteo + ip-api) =====
static char weatherCidade[64] = "";
static char weatherTemp[32] = "";
static char weatherDesc[64] = "Carregando clima...";
static int weatherCode = -1;
static bool weatherIsDay = true;
static bool weatherSol = false;
static bool weatherCarregado = false;
static bool weatherCarregando = false;
static pthread_t weatherThread;
// Detalhes do clima
static float weatherVento = 0;
static int weatherUmidade = 0;
static float weatherSensacao = 0;
static float weatherPressao = 0;
static int weatherChuva = 0;
static char weatherSunrise[32] = "";
static char weatherSunset[32] = "";
// Clima manual
static int weatherModo = 0; // 0 = auto IP, 1 = manual
static double weatherLatM = 0, weatherLonM = 0;
static char weatherCidadeManual[64] = "";
static char weatherStatus[64] = "";
static bool weatherEditando = false;
static char weatherBusca[64] = "";

// ===== EVENTOS / TAREFAS / AGENDAS =====
#define MAX_EVENTOS 300
#define MAX_NOTIF_OPCOES 6
static const int notifMinutos[MAX_NOTIF_OPCOES] = { 1440, 720, 60, 30, 10, 0 };
static const char* notifNomes[MAX_NOTIF_OPCOES] = { "1 dia antes", "12 horas antes", "1 hora antes", "30 min antes", "10 min antes", "No horario" };

#define MAX_CORES 10
static const uint32_t paletaCores[MAX_CORES] = {
    0xFFE53935, 0xFFD81B60, 0xFF8E24AA, 0xFF5C6BC0, 0xFF2196F3,
    0xFF009688, 0xFF43A047, 0xFFFBC02D, 0xFFFB8C00, 0xFF9E9E9E };
static const char* paletaNomes[MAX_CORES] = {
    "Vermelho","Rosa","Roxo","Indigo","Azul","Verde agua","Verde","Amarelo","Laranja","Cinza" };

#define MAX_REP 5
static const char* repNomes[MAX_REP] = { "Nunca", "Todo dia", "Todo mes", "Todo ano", "Personalizado" };

typedef struct {
    int ano, mes, dia;
    int hora, minuto, segundo;
    int anoF, mesF, diaF;
    int horaF, minutoF, segundoF;
    int cor;     // indice na paleta
    int rep;     // 0 nenhum,1 diario,2 mensal,3 anual,4 personalizado
    int repDias; // intervalo em dias (personalizado)
    char nome[128];
    char desc[256];
    char emails[256];
    char phones[256];
    int notifMask;
    int tipo; // 0 = evento, 1 = tarefa
} EventoItem;
static EventoItem eventos[MAX_EVENTOS];
static int eventosTotal = 0;
static unsigned char eventoNotificado[MAX_EVENTOS];
static float eventoBannerTimer = 0.0f;
static char eventoBannerMsg[256] = "";

// Agendas (conjuntos de calendarios/tarefas)
#define MAX_AGENDAS 30
typedef struct { int id; char nome[64]; } AgendaItem;
static AgendaItem agendas[MAX_AGENDAS];
static int agendasTotal = 0;
static int agendaAtual = 0;
static char agendaArquivo[200] = "";

// Estado do formulario de evento
typedef enum {
    EVT_NOME=0, EVT_DESC=1, EVT_EMAILS=2, EVT_PHONES=3,
    EVT_HORA=4, EVT_HORAF=5, EVT_COR=6, EVT_REP=7, EVT_REPDIAS=8, EVT_NOTIF=9, EVT_TIPO=10
} EvtField;
static int evtEditIndex = -1; // -1 = novo
static char evtNome[128] = "", evtDesc[256] = "", evtEmails[256] = "", evtPhones[256] = "";
static int evtHora = 0, evtMinuto = 0;
static int evtDiaF = 0, evtMesF = 0, evtAnoF = 0, evtHoraF = 0, evtMinF = 0;
static int evtCor = 0, evtRep = 0, evtRepDias = 1;
static int evtTipo = 0; // 0 = evento, 1 = tarefa
static int evtNotifMask = 0;
static int evtField = 0;
static int evtNotifSel = 0;
static int evtHoraField = 0;
static int evtHoraFField = 0;
static bool evtEditando = false;
static bool agendaEditando = false;
static char agendaNovoNome[64] = "";

// ===== CONTATOS =====
#define MAX_CONTATOS 500
#define MAX_CAMPC 16
typedef struct {
    char nome[128];
    char telefone[64];
    char emails[256];
    char tags[256];          // nomes separados por ','
    char cl[MAX_CAMPC][64];  // rotulos dos campos personalizados (ligados ao catalogo)
    char cv[MAX_CAMPC][128]; // valores
    time_t adicionado;
} ContactItem;
static ContactItem contatos[MAX_CONTATOS];
static int contatosTotal = 0;
static int contatoSel = -1;
static int contatoEditIndex = -1; // -1 = novo
static int contatoField = 0;      // 0 Nome,1 Telefone,2 E-mails,3..(3+campoTotal-1) valores
static int contatoSort = 0;       // 0 alfabetico, 1 data, 2 tag, 3 campo
static bool contatoEditando = false;
static char contatoFiltroTag[64] = "";
static bool contatoFiltroEdit = false;
static int contatoScroll = 0;
static int contatoEditScroll = 0;
static int contatoOrd[MAX_CONTATOS];
static ContactItem contatoBuf;    // buffer de edicao
// Catalogos globais (definicoes reutilizaveis)
#define MAX_CAT 64
static char tagCatalog[MAX_CAT][64];
static int tagTotal = 0;
static char campoCatalog[MAX_CAT][64];
static int campoTotal = 0;
// Gerenciador de catalogo (overlay)
static int gerenciaModo = 0; // 0 nenhum, 1 tags, 2 campos
static int gerSel = 0;
static bool gerEditando = false;
static int gerEditIdx = -1;
static char gerNome[64] = "";

// ===== CAIXA DE ENTRADA =====
#define MAX_MSG 500
typedef struct {
    int tipo; // 0 email, 1 whatsapp, 2 sms, 3 telegram
    char de[128];
    char assunto[128];
    char texto[600];
    time_t recebido;
    bool lido;
} MsgItem;
static MsgItem msgs[MAX_MSG];
static int msgsTotal = 0;
static int msgSel = -1;
static int msgEditIndex = -1; // -1 = novo
static int msgField = 0;
static int msgTipoSel = 0;
static int msgFiltroTipo = -1; // -1 todos, 0..3
static bool msgEditando = false;
static char mDe[128] = "", mAss[128] = "", mTxt[600] = "";
static int msgScroll = 0;
static int msgViewIndex = -1;
static bool msgCompondo = false;

// ===== TERMINAL =====
#define MAX_TERM_LINES 200
#define MAX_TERM_CMD 512
static char termInput[MAX_TERM_CMD] = "";
static char termLines[MAX_TERM_LINES][256];
static int termLinesTotal = 0;
static int termScroll = 0;
static char termHist[MAX_TERM_LINES][MAX_TERM_CMD];
static int termHistTotal = 0;
static int termHistSel = -1;
static bool termOptOpen = false;
static double termCaret = 0.0;

static void ferramentas_term_append(const char* line) {
    if (termLinesTotal >= MAX_TERM_LINES) {
        for (int i = 0; i < MAX_TERM_LINES - 1; i++) strcpy(termLines[i], termLines[i + 1]);
        termLinesTotal = MAX_TERM_LINES - 1;
    }
    strncpy(termLines[termLinesTotal], line, 255); termLines[termLinesTotal][255] = 0;
    termLinesTotal++;
    termScroll = termLinesTotal;
}
static void ferramentas_term_run(const char* cmd) {
    ferramentas_term_append(termInput[0] ? termInput : cmd);
    char c[512];
    strncpy(c, cmd, 511); c[511] = 0;
    // remover espacos iniciais
    int p = 0; while (c[p] == ' ') p++;
    if (strcmp(c + p, "cls") == 0 || strcmp(c + p, "clear") == 0) { termLinesTotal = 0; termScroll = 0; return; }
    if (strncmp(c + p, "opencode", 8) == 0 && (c[p + 8] == 0 || c[p + 8] == ' ')) {
        const char* query = c + p + 8;
        while (*query == ' ') query++;
        if (*query == 0) {
            ferramentas_term_append("Abrindo opencode (TUI)...");
            exec_async_silent("cmd /c start \"opencode\" opencode");
            return;
        }
        char tmpFile[256];
        const char* tmpDir = getenv("TEMP"); if (!tmpDir) tmpDir = "C:\\temp";
        snprintf(tmpFile, sizeof(tmpFile), "%s\\opencode_term_output.txt", tmpDir);
        char fullCmd[4200];
        snprintf(fullCmd, sizeof(fullCmd), "opencode run \"%s\" > \"%s\" 2>&1", query, tmpFile);
        ferramentas_term_append("Processando...");
        exec_silent(fullCmd);
        FILE* ft = fopen(tmpFile, "r");
        if (ft) {
            char line[256];
            while (fgets(line, sizeof(line), ft)) {
                int l = strlen(line); while (l > 0 && (line[l-1]=='\n'||line[l-1]=='\r')) line[--l]=0;
                ferramentas_term_append(line);
            }
            fclose(ft);
        } else {
            ferramentas_term_append("Erro: nao foi possivel executar opencode");
        }
        remove(tmpFile);
        return;
    }
    if (strncmp(c + p, "help", 4) == 0 && (c[p + 4] == 0 || c[p + 4] == ' ')) {
        ferramentas_term_append("Comandos: opencode, cls, echo <txt>, date, ver, dir, help");
        return;
    }
    // executar via popen e capturar saida
    FILE* fp = popen(c, "r");
    if (!fp) { ferramentas_term_append("Erro ao executar comando."); return; }
    char buf[256];
    int any = 0;
    while (fgets(buf, sizeof(buf), fp)) {
        int l = strlen(buf); while (l > 0 && (buf[l - 1] == '\n' || buf[l - 1] == '\r')) buf[--l] = 0;
        ferramentas_term_append(buf); any = 1;
    }
    pclose(fp);
    if (!any) ferramentas_term_append("(sem saida)");
}

static void timerListarArquivos(const char* path) {
    DIR* d = opendir(path);
    if (!d) return;

    timerTotalItens = 0;

    if (strcmp(path, "data/HyperNeiva") != 0 && strcmp(path, "data") != 0) {
        strcpy(timerNomes[timerTotalItens], "[ .. Voltar Pasta Anterior ]");
        strcpy(timerPaths[timerTotalItens], "..");
        timerEhPasta[timerTotalItens] = true;
        timerTotalItens++;
    }

    struct dirent* dir;
    while ((dir = readdir(d)) != NULL && timerTotalItens < 200) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, dir->d_name);

        struct stat st;
        bool isDir = false;
        if (stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode)) {
            isDir = true;
        }

        if (isDir) {
            snprintf(timerNomes[timerTotalItens], 64, "[ %s ]", dir->d_name);
            strcpy(timerPaths[timerTotalItens], fullPath);
            timerEhPasta[timerTotalItens] = true;
            timerTotalItens++;
        } else {
            char temp[256]; strcpy(temp, dir->d_name);
            for (int i = 0; temp[i]; i++) temp[i] = tolower(temp[i]);
            if (strstr(temp, ".txt")) {
                strncpy(timerNomes[timerTotalItens], dir->d_name, 63);
                strcpy(timerPaths[timerTotalItens], fullPath);
                timerEhPasta[timerTotalItens] = false;
                timerTotalItens++;
            }
        }
    }
    closedir(d);

    for (int i = (timerTotalItens > 0 && strcmp(timerPaths[0], "..") == 0 ? 1 : 0); i < timerTotalItens - 1; i++) {
        for (int j = i + 1; j < timerTotalItens; j++) {
            bool swap = false;
            if (timerEhPasta[i] != timerEhPasta[j]) {
                if (!timerEhPasta[i] && timerEhPasta[j]) swap = true;
            } else {
                if (strcasecmp(timerNomes[i], timerNomes[j]) > 0) swap = true;
            }
            if (swap) {
                char tNome[64]; strcpy(tNome, timerNomes[i]); strcpy(timerNomes[i], timerNomes[j]); strcpy(timerNomes[j], tNome);
                char tPath[256]; strcpy(tPath, timerPaths[i]); strcpy(timerPaths[i], timerPaths[j]); strcpy(timerPaths[j], tPath);
                bool tPasta = timerEhPasta[i]; timerEhPasta[i] = timerEhPasta[j]; timerEhPasta[j] = tPasta;
            }
        }
    }
    timerSel = 0;
    timerScroll = 0;
}

static void timerSalvarLaps(const char* filepath) {
    #ifdef _WIN32
    _mkdir("data");
    _mkdir("data/HyperNeiva");
    #else
    mkdir("data", 0777);
    mkdir("data/HyperNeiva", 0777);
    #endif

    FILE* f = fopen(filepath, "w");
    if (f) {
        fprintf(f, "HYPER_NEIVA_CHRONO_LAPS_V1\n");
        fprintf(f, "ELAPSED: %f\n", chronoElapsed);
        fprintf(f, "LAPCOUNT: %d\n", chronoLapCount);
        for (int i = 0; i < chronoLapCount; i++) {
            fprintf(f, "LAP %d: %f\n", i, chronoLaps[i]);
        }
        fclose(f);
    }
}

static void timerAbrirLaps(const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) return;

    char header[64];
    if (fgets(header, sizeof(header), f)) {
        // Strip newline
        char* nl = strchr(header, '\n');
        if (nl) *nl = '\0';
        nl = strchr(header, '\r');
        if (nl) *nl = '\0';

        if (strcmp(header, "HYPER_NEIVA_CHRONO_LAPS_V1") == 0) {
            double elapsed = 0.0;
            int lapCount = 0;
            if (fscanf(f, "ELAPSED: %lf\n", &elapsed) == 1) {
                chronoElapsed = elapsed;
            }
            if (fscanf(f, "LAPCOUNT: %d\n", &lapCount) == 1) {
                chronoLapCount = lapCount;
                if (chronoLapCount > 5) chronoLapCount = 5;
            }
            for (int i = 0; i < chronoLapCount; i++) {
                int dummy;
                double val;
                if (fscanf(f, "LAP %d: %lf\n", &dummy, &val) == 2) {
                    chronoLaps[i] = val;
                }
            }
        }
    }
    fclose(f);
}

// Helpers
static void drawCircle(int cx, int cy, int r, Color color, bool fill) {
    if (fill) {
        DrawCircle(cx, cy, r, color);
    } else {
        DrawCircleLines(cx, cy, r, color);
    }
}

static void desenharRetanguloAlpha(int x, int y, int w, int h, uint32_t cor) {
    uint8_t a = (cor >> 24) & 0xFF;
    uint8_t r = (cor >> 16) & 0xFF;
    uint8_t g = (cor >> 8) & 0xFF;
    uint8_t b = cor & 0xFF;
    DrawRectangle(x, y, w, h, (Color){ r, g, b, a });
}

static int medirTexto(const char* text, int size) {
    return MeasureText(text, size);
}

static void desenharTexto(const char* text, int size, int x, int y, uint32_t cor) {
    uint8_t a = (cor >> 24) & 0xFF;
    uint8_t r = (cor >> 16) & 0xFF;
    uint8_t g = (cor >> 8) & 0xFF;
    uint8_t b = cor & 0xFF;
    DrawText(text, x, y, size, (Color){ r, g, b, a });
}

static void desenharPreviewVideo(int x, int y, int w, int h) {
    static unsigned char pixels[GRAV_PREVIEW_FRAME];
    if (!grav_video_preview_active()) return;
    if (grav_video_preview_get(pixels, GRAV_PREVIEW_FRAME)) {
        if (!gravPreviewTexLoaded) {
            Image img = { 0 };
            img.width = GRAV_PREVIEW_W;
            img.height = GRAV_PREVIEW_H;
            img.mipmaps = 1;
            img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            img.data = pixels;
            gravPreviewTex = LoadTextureFromImage(img);
            gravPreviewTexLoaded = true;
        } else {
            UpdateTexture(gravPreviewTex, pixels);
        }
    }
    if (gravPreviewTexLoaded) {
        DrawTexturePro(gravPreviewTex,
            (Rectangle){ 0, 0, (float)GRAV_PREVIEW_W, (float)GRAV_PREVIEW_H },
            (Rectangle){ (float)x, (float)y, (float)w, (float)h },
            (Vector2){ 0, 0 }, 0.0f, WHITE);
        DrawRectangleLines(x, y, w, h, (Color){255,80,80,255});
    }
}

static void gravSalvarAtual(void) {
    if (strlen(gravNomeInput) == 0) return;
    if (gravTipo == 0) {
        char finalPath[260];
        snprintf(finalPath, sizeof(finalPath), "%s%s.wav", GRAV_DIR, gravNomeInput);
        if (grav_finish_recording(GRAV_TEMP_RAW, finalPath)) {
            remove(GRAV_TEMP_RAW);
            if (gravTotal < MAX_GRAVACOES) strcpy(gravList[gravTotal++], finalPath);
            gravState = 0;
        } else {
            snprintf(gravErrorMsg, sizeof(gravErrorMsg), "Nao foi possivel salvar o arquivo. Gravacao vazia ou falhou.");
            gravErrorTimer = GetTime();
            gravState = 4;
        }
    } else {
        char finalPath[260];
        snprintf(finalPath, sizeof(finalPath), "%s%s.mp4", GRAV_DIR, gravNomeInput);
        remove(finalPath);
        const char* temp = (gravTipo == 1) ? GRAV_VIDEO_TEMP : GRAV_TELA_TEMP;
        if (rename(temp, finalPath) == 0) {
            if (gravTotal < MAX_GRAVACOES) strcpy(gravList[gravTotal++], finalPath);
            gravState = 0;
        } else {
            snprintf(gravErrorMsg, sizeof(gravErrorMsg), "Nao foi possivel salvar o arquivo.");
            gravErrorTimer = GetTime();
            gravState = 4;
        }
    }
}

static void gravDesenharIcone(Texture2D ic, int x, int y, int sz, Color tint, bool hover, Color borda) {
    desenharRetanguloAlpha(x, y, sz, sz, hover ? 0xFF2A3A4A : 0xFF1E2024);
    DrawRectangleLines(x, y, sz, sz, borda);
    if (ic.id != 0) {
        DrawTexturePro(ic, (Rectangle){0, 0, (float)ic.width, (float)ic.height},
            (Rectangle){(float)x, (float)y, (float)sz, (float)sz}, (Vector2){0, 0}, 0.0f, tint);
    }
}

static void desenharBotaoVoltarIcone(int x, int y, int w, int h, bool hover) {
    desenharRetanguloAlpha(x, y, w, h, hover ? 0xFF00AAFF : 0xFF333333);
    DrawRectangleLines(x, y, w, h, WHITE);
    if (gravIconBack.id != 0) {
        int sz = (h - 16) < 28 ? (h - 16) : 28;
        if (sz < 18) sz = 18;
        DrawTexturePro(gravIconBack, (Rectangle){0, 0, (float)gravIconBack.width, (float)gravIconBack.height},
            (Rectangle){(float)(x + (w - sz)/2), (float)(y + (h - sz)/2), (float)sz, (float)sz}, (Vector2){0, 0}, 0.0f, WHITE);
    }
}

void ferramentas_carregar_eventos(void);
static void limpar_pipe(char* dst, const char* src, int sz);
static void ferramentas_carregar_agendas(void);
static void ferramentas_carregar_clima_cfg(void);
static void ferramentas_aplicar_clima(double t, int wc, int id, const char* cidade);
static void ferramentas_carregar_contatos(void);
static void ferramentas_salvar_contatos(void);
static void ferramentas_carregar_catalogos(void);
static void ferramentas_salvar_tags(void);
static void ferramentas_salvar_campos(void);
static void ferramentas_carregar_caixa(void);
static void ferramentas_salvar_caixa(void);
static void qr_scan_stop(void);
static void qr_listar_imagens(void);

void initFerramentas(void) {
    if (!toolsRenderTextureLoaded) {
        toolsRenderTexture = LoadRenderTexture(1920, 1080);
        toolsRenderTextureLoaded = true;
    }
    char gravExeDir[512];
    grav_get_exe_dir(gravExeDir, sizeof(gravExeDir));
    snprintf(g_gravDir, sizeof(g_gravDir), "%s\\data\\HyperNeiva\\gravacoes\\", gravExeDir);
    snprintf(g_gravTemp, sizeof(g_gravTemp), "%s\\data\\HyperNeiva\\gravacoes\\grav_temp.raw", gravExeDir);
    snprintf(g_gravTelaTemp, sizeof(g_gravTelaTemp), "%s\\data\\HyperNeiva\\gravacoes\\grav_tela_temp.mp4", gravExeDir);
    snprintf(g_gravVideoTemp, sizeof(g_gravVideoTemp), "%s\\data\\HyperNeiva\\gravacoes\\grav_video_temp.mp4", gravExeDir);
    {
        char qrExeDir[512];
        grav_get_exe_dir(qrExeDir, sizeof(qrExeDir));
        snprintf(qrDir, sizeof(qrDir), "%s\\data\\HyperNeiva\\qrcodes\\", qrExeDir);
    }
    qr_scan_stop();
    qrTab = 0; qrFoco = false; brFoco = 0; wpFoco = 0;
    qrInput[0] = '\0'; brDados[0] = '\0'; brLabel[0] = '\0';
    wpFone[0] = '\0'; wpMsg[0] = '\0'; wpLink[0] = '\0'; wpTemLink = false;
    lerModo = 0; lerFocoPath = false; lerPathIn[0] = '\0';
    lerResultado[0] = '\0'; lerTemResult = false; lerScroll = 0;
    if (qrTexOk) { UnloadTexture(qrTex); qrTexOk = false; }
    if (lerCamTexOk) { UnloadTexture(lerCamTex); lerCamTexOk = false; }
    qrLastPng[0] = '\0'; qrMsg[0] = '\0';
    qr_listar_imagens();
    gravShowMenu = true;
    toolMode = TOOL_MENU;
    menuSelection = 0;

    handleCalcClear();
    carregarNotasFerramenta();
    notepadScrollY = 0;
    notepadState = NOTEPAD_EDIT;
    saveFilename[0] = '\0';
    strcpy(noteCaminhoAtual, "data/HyperNeiva/midia/leitura");

    // Reset timer states
    chronoElapsed = 0.0;
    chronoRunning = false;
    chronoLapCount = 0;
    memset(chronoLaps, 0, sizeof(chronoLaps));

    timerRemaining = 0.0;
    timerTotalDuration = 0.0;
    timerRunning = false;
    alarmFlashTimer = 0.0f;
    timerState = TIMER_STATE_NORMAL;
    strcpy(timerCaminhoAtual, "data/HyperNeiva");
    timerSaveFilename[0] = '\0';
    ferramentas_carregar_clima_cfg();
    ferramentas_carregar_agendas();
    ferramentas_carregar_eventos();
    ferramentas_carregar_catalogos();
    ferramentas_carregar_contatos();
    ferramentas_carregar_caixa();
    weatherCarregado = false; weatherCarregando = false; weatherTemp[0] = 0;
    strcpy(weatherDesc, "Carregando clima...");

    // Gravador init
    grav_kill_orphans();
    gravState = 0;
    gravTotal = 0;
    gravScroll = 0;
    gravVideoHasScreen = false;
    gravNomeInput[0] = 0;
    #ifdef _WIN32
    _mkdir(GRAV_DIR);
    #else
    mkdir(GRAV_DIR, 0777);
    #endif
    DIR* d = opendir(GRAV_DIR);
    if (d) {
        struct dirent* de;
        while ((de = readdir(d)) != NULL && gravTotal < MAX_GRAVACOES) {
            const char* ext = strrchr(de->d_name, '.');
            if (ext && (strcmp(ext, ".wav") == 0 || strcmp(ext, ".mp3") == 0 || strcmp(ext, ".aac") == 0 ||
                        strcmp(ext, ".ogg") == 0 || strcmp(ext, ".flac") == 0 || strcmp(ext, ".mp4") == 0 ||
                        strcmp(ext, ".avi") == 0 || strcmp(ext, ".mkv") == 0 || strcmp(ext, ".mov") == 0 || strcmp(ext, ".webm") == 0 ||
                        strcmp(ext, ".png") == 0 || strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0 || strcmp(ext, ".bmp") == 0)) {
                snprintf(gravList[gravTotal], sizeof(gravList[0]), "%s%s", GRAV_DIR, de->d_name);
                gravTotal++;
            }
        }
        closedir(d);
    }
    if (!gravIconsLoaded) {
        if (FileExists("assets/images/grav_mic.png")) gravIconMic = LoadTexture("assets/images/grav_mic.png");
        if (FileExists("assets/images/grav_camera.png")) gravIconCam = LoadTexture("assets/images/grav_camera.png");
        if (FileExists("assets/images/grav_tela.png")) gravIconTela = LoadTexture("assets/images/grav_tela.png");
        if (FileExists("assets/images/grav_pause.png")) gravIconPause = LoadTexture("assets/images/grav_pause.png");
        if (FileExists("assets/images/grav_play.png")) gravIconPlay = LoadTexture("assets/images/grav_play.png");
        if (FileExists("assets/images/grav_mic_off.png")) gravIconMicOff = LoadTexture("assets/images/grav_mic_off.png");
        if (FileExists("assets/images/grav_delete.png")) gravIconDelete = LoadTexture("assets/images/grav_delete.png");
        if (FileExists("assets/images/grav_back.png")) gravIconBack = LoadTexture("assets/images/grav_back.png");
        gravIconsLoaded = true;
    }
}

void fecharFerramentas(void) {
    salvarNotasFerramenta();
    grav_stop_recording();
    if (gravPreviewTexLoaded) {
        UnloadTexture(gravPreviewTex);
        gravPreviewTexLoaded = false;
    }
    if (gravIconsLoaded) {
        UnloadTexture(gravIconMic);
        UnloadTexture(gravIconCam);
        UnloadTexture(gravIconTela);
        UnloadTexture(gravIconPause);
        UnloadTexture(gravIconPlay);
        UnloadTexture(gravIconMicOff);
        UnloadTexture(gravIconDelete);
        UnloadTexture(gravIconBack);
        gravIconsLoaded = false;
    }
    if (toolsRenderTextureLoaded) {
        UnloadRenderTexture(toolsRenderTexture);
        toolsRenderTextureLoaded = false;
    }
}

bool deveFecharFerramentas(void) {
    if (toolMode == TOOL_MENU) {
        return true;
    } else {
        if (toolMode == TOOL_NOTEPAD) {
            salvarNotasFerramenta();
        }
        toolMode = TOOL_MENU;
        return false;
    }
}

// =================== CLIMA ===================
static int ferramentas_exec_curl(const char* cmd) {
    char full[4200]; sprintf(full, "cmd /c \"%s\"", cmd);
    exec_silent(full);
    return 0;
}

static double ferramentas_parse_double(const char* json, const char* chave) {
    char pat[64]; sprintf(pat, "\"%s\"", chave);
    const char* p = strstr(json, pat);
    if (!p) return -999.0;
    p += strlen(pat);
    while (*p == ' ' || *p == ':') p++;
    return atof(p);
}

static void ferramentas_json_str(const char* json, const char* chave, char* out, int sz) {
    char pat[64]; sprintf(pat, "\"%s\"", chave);
    const char* p = strstr(json, pat);
    if (!p) { out[0] = 0; return; }
    p += strlen(pat);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') { out[0] = 0; return; }
    p++; int o = 0;
    while (*p && *p != '"' && o < sz - 1) out[o++] = *p++;
    out[o] = 0;
}

static void ferramentas_extrair_hora(const char* src, char* out) {
    const char* t = strchr(src, 'T');
    if (!t) { strncpy(out, src, 31); out[31] = 0; return; }
    t++;
    int o = 0;
    while (*t && *t != '"' && o < 31) out[o++] = *t++;
    out[o] = 0;
}

static void ferramentas_mapear_clima(int code, bool isDay, char* desc, bool* sol) {
    *sol = false;
    switch (code) {
        case 0: strcpy(desc, "Ceu limpo"); *sol = isDay; break;
        case 1: strcpy(desc, "Predom. limpo"); *sol = isDay; break;
        case 2: strcpy(desc, "Parcial. nublado"); break;
        case 3: strcpy(desc, "Nublado"); break;
        case 45: case 48: strcpy(desc, "Neblina"); break;
        case 51: case 53: case 55: strcpy(desc, "Garoa"); break;
        case 56: case 57: strcpy(desc, "Garoa gelada"); break;
        case 61: case 63: case 65: strcpy(desc, "Chuva"); break;
        case 66: case 67: strcpy(desc, "Chuva congel."); break;
        case 71: case 73: case 75: strcpy(desc, "Neve"); break;
        case 77: strcpy(desc, "Neve granular"); break;
        case 80: case 81: case 82: strcpy(desc, "Pancadas"); break;
        case 85: case 86: strcpy(desc, "Pancadas neve"); break;
        case 95: strcpy(desc, "Tempestade"); break;
        case 96: case 99: strcpy(desc, "Tempestade c/ gelo"); break;
        default: strcpy(desc, "Desconhecido");
    }
}

static void* ferramentas_thread_clima(void* arg) {
    double la = 0, lo = 0; char city[64] = {0};
    bool manual = (weatherModo == 1 && (weatherLatM != 0 || weatherLonM != 0));
    if (manual) {
        la = weatherLatM; lo = weatherLonM; strcpy(city, weatherCidadeManual);
    } else {
        char cmd[2048];
        sprintf(cmd, "curl -s \"http://ip-api.com/json/\" > data/HyperNeiva/clima_ip.json");
        ferramentas_exec_curl(cmd);
        FILE* f = fopen("data/HyperNeiva/clima_ip.json", "rb");
        if (f) {
            fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
            char* b = (char*)malloc(sz + 1);
            if (b) {
                fread(b, 1, sz, f); b[sz] = 0;
                la = ferramentas_parse_double(b, "lat");
                lo = ferramentas_parse_double(b, "lon");
                ferramentas_json_str(b, "city", city, sizeof(city));
                free(b);
            }
            fclose(f);
        }
    }
    if (la != 0 || lo != 0) {
        char url[1024]; char lats[32], lons[32];
        sprintf(lats, "%g", la); sprintf(lons, "%g", lo);
        sprintf(url, "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m,surface_pressure,is_day&daily=sunrise,sunset,precipitation_probability_max", lats, lons);
        char cmd2[2048];
        sprintf(cmd2, "curl -s \"%s\" > data/HyperNeiva/clima.json", url);
        ferramentas_exec_curl(cmd2);
        FILE* f = fopen("data/HyperNeiva/clima.json", "rb");
        if (f) {
            fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
            char* b = (char*)malloc(sz + 1);
            if (b) {
                fread(b, 1, sz, f); b[sz] = 0;
                double t = ferramentas_parse_double(b, "temperature_2m");
                int wc = (int)ferramentas_parse_double(b, "weather_code");
                int id = (int)ferramentas_parse_double(b, "is_day");
                ferramentas_aplicar_clima(t, wc, id, city);
                weatherUmidade = (int)ferramentas_parse_double(b, "relative_humidity_2m");
                weatherSensacao = ferramentas_parse_double(b, "apparent_temperature");
                weatherVento = ferramentas_parse_double(b, "wind_speed_10m");
                weatherPressao = ferramentas_parse_double(b, "surface_pressure");
                weatherChuva = (int)ferramentas_parse_double(b, "precipitation_probability_max");
                char sr[64], ss[64];
                ferramentas_json_str(b, "sunrise", sr, sizeof(sr));
                ferramentas_json_str(b, "sunset", ss, sizeof(ss));
                ferramentas_extrair_hora(sr, weatherSunrise);
                ferramentas_extrair_hora(ss, weatherSunset);
                free(b);
            }
            fclose(f);
        }
    }
    weatherCarregado = true; weatherCarregando = false;
    if (weatherTemp[0] == 0) strcpy(weatherDesc, "Clima indisponivel");
    weatherStatus[0] = 0;
    return NULL;
}

// =================== CONTATOS ===================
static int ferramentas_ci_cmp(const char* a, const char* b) {
    for (int i = 0; a[i] && b[i]; i++) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return ca - cb;
    }
    if (a[0] == 0 && b[0] != 0) return -1;
    if (b[0] == 0 && a[0] != 0) return 1;
    return 0;
}

static void ferramentas_carregar_contatos(void) {
    contatosTotal = 0;
    FILE* f = fopen("data/HyperNeiva/contatos.txt", "r");
    if (!f) return;
    char line[4096];
    int cap = 4 + 2 * MAX_CAMPC + 2;
    while (fgets(line, sizeof(line), f) && contatosTotal < MAX_CONTATOS) {
        char* parts[128]; int pi = 0; char* p = line; char* start = p;
        for (; *p && pi < cap; p++) {
            if (*p == '|') { *p = 0; parts[pi++] = start; start = p + 1; }
        }
        if (pi < 4) continue;
        ContactItem* c = &contatos[contatosTotal];
        limpar_pipe(c->nome, parts[0], sizeof(c->nome));
        limpar_pipe(c->telefone, parts[1], sizeof(c->telefone));
        limpar_pipe(c->emails, parts[2], sizeof(c->emails));
        limpar_pipe(c->tags, parts[3], sizeof(c->tags));
        for (int k = 0; k < MAX_CAMPC; k++) {
            int bi = 4 + 2 * k;
            if (bi + 1 < pi) {
                limpar_pipe(c->cl[k], parts[bi], 64);
                limpar_pipe(c->cv[k], parts[bi + 1], 128);
            }
        }
        c->adicionado = (pi >= 5) ? (time_t)atoll(parts[pi - 1]) : 0;
        contatosTotal++;
    }
    fclose(f);
}

static void ferramentas_salvar_contatos(void) {
    FILE* f = fopen("data/HyperNeiva/contatos.txt", "w");
    if (!f) return;
    for (int i = 0; i < contatosTotal; i++) {
        ContactItem* c = &contatos[i];
        char n[128], t[64], e[256], tg[256], cl[MAX_CAMPC][64], cv[MAX_CAMPC][128];
        limpar_pipe(n, c->nome, sizeof(n));
        limpar_pipe(t, c->telefone, sizeof(t));
        limpar_pipe(e, c->emails, sizeof(e));
        limpar_pipe(tg, c->tags, sizeof(tg));
        for (int k = 0; k < MAX_CAMPC; k++) { limpar_pipe(cl[k], c->cl[k], 64); limpar_pipe(cv[k], c->cv[k], 128); }
        fprintf(f, "%s|%s|%s|%s", n, t, e, tg);
        for (int k = 0; k < MAX_CAMPC; k++) fprintf(f, "|%s|%s", cl[k], cv[k]);
        fprintf(f, "|%s|%lld\n", c->nome, (long long)c->adicionado);
    }
    fclose(f);
}

// ---------- Helpers de tags/campos em contatos ----------
static int contato_slot_label(ContactItem* c, const char* label) {
    for (int k = 0; k < MAX_CAMPC; k++) {
        if (c->cl[k][0] && strcmp(c->cl[k], label) == 0) return k;
    }
    return -1;
}
static void contato_set_campo(ContactItem* c, const char* label, const char* valor) {
    int s = contato_slot_label(c, label);
    if (s < 0) { for (int k = 0; k < MAX_CAMPC; k++) if (!c->cl[k][0]) { s = k; break; } }
    if (s < 0) return;
    strcpy(c->cl[s], label);
    strncpy(c->cv[s], valor, sizeof(c->cv[s]) - 1); c->cv[s][sizeof(c->cv[s]) - 1] = 0;
}
static bool contato_tag_tem(ContactItem* c, const char* tag) {
    char* p = c->tags;
    while (*p) {
        while (*p == ' ' || *p == ',') p++;
        char* ini = p;
        while (*p && *p != ',' && *p != ' ' && *p != '\n' && *p != '\r') p++;
        int len = p - ini;
        if (len == (int)strlen(tag) && strncmp(ini, tag, len) == 0) return true;
    }
    return false;
}
static void contato_tag_add(ContactItem* c, const char* tag) {
    if (contato_tag_tem(c, tag)) return;
    int l = strlen(c->tags);
    if (l > 0 && l < (int)sizeof(c->tags) - 2) { c->tags[l] = ','; c->tags[l + 1] = 0; l++; }
    strncat(c->tags, tag, sizeof(c->tags) - l - 1);
}
static void contato_tag_del(ContactItem* c, const char* tag) {
    char out[256]; out[0] = 0;
    char* p = c->tags;
    bool first = true;
    while (*p) {
        while (*p == ' ' || *p == ',') p++;
        char* ini = p;
        while (*p && *p != ',' && *p != ' ' && *p != '\n' && *p != '\r') p++;
        int len = p - ini;
        if (len == (int)strlen(tag) && strncmp(ini, tag, len) == 0) continue;
        if (!first && strlen(out) < (int)sizeof(out) - 2) strcat(out, ",");
        if (strlen(out) < (int)sizeof(out) - len - 1) { strncat(out, ini, len); out[strlen(out) + len] = 0; }
        first = false;
    }
    strcpy(c->tags, out);
}

// ---------- Catalogos globais (tags e campos) ----------
static void ferramentas_carregar_catalogos(void) {
    tagTotal = 0; campoTotal = 0;
    FILE* ft = fopen("data/HyperNeiva/tags.txt", "r");
    if (ft) {
        char line[128];
        while (fgets(line, sizeof(line), ft) && tagTotal < MAX_CAT) {
            int l = strlen(line); while (l > 0 && (line[l - 1] == '\n' || line[l - 1] == '\r' || line[l - 1] == ' ')) line[--l] = 0;
            if (line[0]) { strncpy(tagCatalog[tagTotal], line, 63); tagCatalog[tagTotal][63] = 0; tagTotal++; }
        }
        fclose(ft);
    }
    FILE* fc = fopen("data/HyperNeiva/campos.txt", "r");
    if (fc) {
        char line[128];
        while (fgets(line, sizeof(line), fc) && campoTotal < MAX_CAT) {
            int l = strlen(line); while (l > 0 && (line[l - 1] == '\n' || line[l - 1] == '\r' || line[l - 1] == ' ')) line[--l] = 0;
            if (line[0]) { strncpy(campoCatalog[campoTotal], line, 63); campoCatalog[campoTotal][63] = 0; campoTotal++; }
        }
        fclose(fc);
    }
}
static void ferramentas_salvar_tags(void) {
    FILE* f = fopen("data/HyperNeiva/tags.txt", "w");
    if (!f) return;
    for (int i = 0; i < tagTotal; i++) fprintf(f, "%s\n", tagCatalog[i]);
    fclose(f);
}
static void ferramentas_salvar_campos(void) {
    FILE* f = fopen("data/HyperNeiva/campos.txt", "w");
    if (!f) return;
    for (int i = 0; i < campoTotal; i++) fprintf(f, "%s\n", campoCatalog[i]);
    fclose(f);
}
static void ferramentas_tag_renomear(const char* old, const char* nw) {
    for (int i = 0; i < contatosTotal; i++) {
        if (contato_tag_tem(&contatos[i], old)) {
            contato_tag_del(&contatos[i], old);
            contato_tag_add(&contatos[i], nw);
        }
    }
    if (strcmp(contatoFiltroTag, old) == 0) strcpy(contatoFiltroTag, nw);
    ferramentas_salvar_contatos();
}
static void ferramentas_tag_excluir(const char* tag) {
    for (int i = 0; i < contatosTotal; i++) contato_tag_del(&contatos[i], tag);
    if (strcmp(contatoFiltroTag, tag) == 0) contatoFiltroTag[0] = 0;
    ferramentas_salvar_contatos();
}
static void ferramentas_campo_renomear(const char* old, const char* nw) {
    for (int i = 0; i < contatosTotal; i++)
        for (int k = 0; k < MAX_CAMPC; k++)
            if (strcmp(contatos[i].cl[k], old) == 0) strcpy(contatos[i].cl[k], nw);
    ferramentas_salvar_contatos();
}
static void ferramentas_campo_excluir(const char* label) {
    for (int i = 0; i < contatosTotal; i++)
        for (int k = 0; k < MAX_CAMPC; k++)
            if (strcmp(contatos[i].cl[k], label) == 0) { contatos[i].cl[k][0] = 0; contatos[i].cv[k][0] = 0; }
    ferramentas_salvar_contatos();
}

static int ferramentas_contatos_ordem(int* ord) {
    int n = 0;
    for (int i = 0; i < contatosTotal; i++) {
        if (contatoFiltroTag[0] && !strstr(contatos[i].tags, contatoFiltroTag)) continue;
        ord[n++] = i;
    }
    for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++) {
        bool sw = false;
        if (contatoSort == 0) sw = ferramentas_ci_cmp(contatos[ord[a]].nome, contatos[ord[b]].nome) > 0;
        else if (contatoSort == 1) sw = contatos[ord[a]].adicionado < contatos[ord[b]].adicionado;
        else if (contatoSort == 2) sw = ferramentas_ci_cmp(contatos[ord[a]].tags, contatos[ord[b]].tags) > 0;
        else sw = ferramentas_ci_cmp(contatos[ord[a]].cv[0], contatos[ord[b]].cv[0]) > 0;
        if (sw) { int t = ord[a]; ord[a] = ord[b]; ord[b] = t; }
    }
    return n;
}

// =================== CAIXA DE ENTRADA ===================
static void ferramentas_carregar_caixa(void) {
    msgsTotal = 0;
    FILE* f = fopen("data/HyperNeiva/caixa.txt", "r");
    if (!f) return;
    char line[2048];
    while (fgets(line, sizeof(line), f) && msgsTotal < MAX_MSG) {
        MsgItem* m = &msgs[msgsTotal];
        int tipo, lido; long long rec;
        char de[128], ass[128], txt[600];
        if (sscanf(line, "%d|%lld|%d|", &tipo, &rec, &lido) != 3) continue;
        char* p = line; int cnt = 0;
        while (*p && cnt < 3) { if (*p == '|') cnt++; p++; }
        if (*p == 0) continue;
        p++;
        char* parts[3]; int pi = 0; char* q = p; char* start = q;
        for (; *q && pi < 3; q++) { if (*q == '|') { *q = 0; parts[pi++] = start; start = q + 1; } }
        if (pi < 3) continue;
        parts[pi] = start;
        limpar_pipe(m->de, parts[0], sizeof(m->de));
        limpar_pipe(m->assunto, parts[1], sizeof(m->assunto));
        limpar_pipe(m->texto, parts[2], sizeof(m->texto));
        m->tipo = tipo; m->recebido = (time_t)rec; m->lido = lido != 0;
        msgsTotal++;
    }
    fclose(f);
}

static void ferramentas_salvar_caixa(void) {
    FILE* f = fopen("data/HyperNeiva/caixa.txt", "w");
    if (!f) return;
    for (int i = 0; i < msgsTotal; i++) {
        MsgItem* m = &msgs[i];
        char de[128], ass[128], txt[600];
        limpar_pipe(de, m->de, sizeof(de));
        limpar_pipe(ass, m->assunto, sizeof(ass));
        limpar_pipe(txt, m->texto, sizeof(txt));
        fprintf(f, "%d|%lld|%d|%s|%s|%s\n", m->tipo, (long long)m->recebido, m->lido ? 1 : 0, de, ass, txt);
    }
    fclose(f);
}

static void ferramentas_iniciar_clima(void) {
    if (weatherCarregado || weatherCarregando) return;
    weatherCarregando = true;
    pthread_create(&weatherThread, NULL, ferramentas_thread_clima, NULL);
}

// =================== EVENTOS / AGENDAS ===================
static void limpar_pipe(char* dst, const char* src, int sz) {
    int o = 0;
    for (int i = 0; src[i] && o < sz - 1; i++) {
        if (src[i] == '|' || src[i] == '\n' || src[i] == '\r') dst[o++] = ' ';
        else dst[o++] = src[i];
    }
    dst[o] = 0;
}

static Color evtCorColor(int i) {
    if (i < 0 || i >= MAX_CORES) i = 0;
    uint32_t c = paletaCores[i];
    return (Color){ (uint8_t)((c >> 16) & 0xFF), (uint8_t)((c >> 8) & 0xFF), (uint8_t)(c & 0xFF), 255 };
}

static void ferramentas_urlenc(char* out, const char* in) {
    int o = 0;
    for (int i = 0; in[i] && o < 250; i++) {
        if (in[i] == ' ') { out[o++] = '%'; out[o++] = '2'; out[o++] = '0'; }
        else out[o++] = in[i];
    }
    out[o] = 0;
}

void ferramentas_carregar_eventos(void);

static void ferramentas_carregar_agendas(void) {
    #ifdef _WIN32
    _mkdir("data/HyperNeiva/agendas");
    #else
    mkdir("data/HyperNeiva/agendas", 0777);
    #endif
    agendasTotal = 0;
    FILE* fi = fopen("data/HyperNeiva/agendas/index.txt", "r");
    if (fi) {
        char line[200];
        while (fgets(line, sizeof(line), fi) && agendasTotal < MAX_AGENDAS) {
            int id; char nm[64];
            if (sscanf(line, "%d|%63[^\n]", &id, nm) == 2) {
                agendas[agendasTotal].id = id; limpar_pipe(agendas[agendasTotal].nome, nm, 64); agendasTotal++;
            }
        }
        fclose(fi);
    }
    if (agendasTotal == 0) { agendas[0].id = 1; strcpy(agendas[0].nome, "Principal"); agendasTotal = 1; }
    if (agendaAtual >= agendasTotal) agendaAtual = 0;
    sprintf(agendaArquivo, "data/HyperNeiva/agendas/agenda_%d.txt", agendas[agendaAtual].id);
}

static void ferramentas_salvar_agenda_index(void) {
    FILE* fi = fopen("data/HyperNeiva/agendas/index.txt", "w");
    if (fi) {
        for (int i = 0; i < agendasTotal; i++) fprintf(fi, "%d|%s\n", agendas[i].id, agendas[i].nome);
        fclose(fi);
    }
}

static void ferramentas_trocar_agenda(int idx) {
    if (idx < 0 || idx >= agendasTotal) return;
    agendaAtual = idx;
    sprintf(agendaArquivo, "data/HyperNeiva/agendas/agenda_%d.txt", agendas[idx].id);
    ferramentas_carregar_eventos();
}

void ferramentas_carregar_eventos(void) {
    eventosTotal = 0;
    if (!agendaArquivo[0]) sprintf(agendaArquivo, "data/HyperNeiva/agendas/agenda_1.txt", 1);
    FILE* f = fopen(agendaArquivo, "r");
    if (!f) return;
    char line[1024];
    while (fgets(line, sizeof(line), f) && eventosTotal < MAX_EVENTOS) {
        int v[17];
        int n = sscanf(line, "%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|",
            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8], &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15], &v[16]);
        if (n != 17) continue;
        char* p = line; int cnt = 0;
        while (*p && cnt < 17) { if (*p == '|') cnt++; p++; }
        if (*p == 0) continue;
        p++;
        char* parts[4]; int pi = 0; char* q = p; char* start = q;
        for (; *q && pi < 4; q++) { if (*q == '|') { *q = 0; parts[pi++] = start; start = q + 1; } }
        if (pi < 2) continue;
        parts[pi] = start;
        EventoItem* e = &eventos[eventosTotal];
        e->ano = v[0]; e->mes = v[1]; e->dia = v[2];
        e->hora = v[3]; e->minuto = v[4]; e->segundo = v[5];
        e->anoF = v[6]; e->mesF = v[7]; e->diaF = v[8];
        e->horaF = v[9]; e->minutoF = v[10]; e->segundoF = v[11];
        e->cor = v[12]; e->rep = v[13]; e->repDias = v[14]; e->notifMask = v[15]; e->tipo = v[16];
        limpar_pipe(e->nome, parts[0], sizeof(e->nome));
        limpar_pipe(e->desc, parts[1], sizeof(e->desc));
        limpar_pipe(e->emails, parts[2] ? parts[2] : "", sizeof(e->emails));
        limpar_pipe(e->phones, parts[3] ? parts[3] : "", sizeof(e->phones));
        eventosTotal++;
    }
    fclose(f);
}

static void ferramentas_salvar_eventos(void) {
    FILE* f = fopen(agendaArquivo, "w");
    if (!f) return;
    for (int i = 0; i < eventosTotal; i++) {
        EventoItem* e = &eventos[i];
        char n[128], d[256], em[256], ph[256];
        limpar_pipe(n, e->nome, sizeof(n));
        limpar_pipe(d, e->desc, sizeof(d));
        limpar_pipe(em, e->emails, sizeof(em));
        limpar_pipe(ph, e->phones, sizeof(ph));
        fprintf(f, "%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%s|%s|%s|%s\n",
            e->ano, e->mes, e->dia, e->hora, e->minuto, e->segundo,
            e->anoF, e->mesF, e->diaF, e->horaF, e->minutoF, e->segundoF,
            e->cor, e->rep, e->repDias, e->notifMask, e->tipo, n, d, em, ph);
    }
    fclose(f);
}

static int ferramentas_contar_eventos_dia(int d, int m, int a) {
    int c = 0;
    for (int i = 0; i < eventosTotal; i++)
        if (eventos[i].dia == d && eventos[i].mes == m && eventos[i].ano == a) c++;
    return c;
}

static time_t ferramentas_ocorr(EventoItem* e, time_t now) {
    struct tm et; memset(&et, 0, sizeof(et));
    et.tm_year = e->ano - 1900; et.tm_mon = e->mes - 1; et.tm_mday = e->dia;
    et.tm_hour = e->hora; et.tm_min = e->minuto; et.tm_sec = e->segundo; et.tm_isdst = -1;
    time_t base = mktime(&et);
    if (base == (time_t)-1) return base;
    if (e->rep == 0) return base;
    if (e->rep == 1) {
        struct tm* nw = localtime(&now); struct tm o = *nw;
        o.tm_hour = e->hora; o.tm_min = e->minuto; o.tm_sec = e->segundo; o.tm_isdst = -1;
        time_t r = mktime(&o); if (r < now - 90) { o.tm_mday++; r = mktime(&o); } return r;
    }
    if (e->rep == 2) {
        struct tm* nw = localtime(&now); struct tm o = *nw;
        o.tm_mday = e->dia; o.tm_hour = e->hora; o.tm_min = e->minuto; o.tm_sec = e->segundo; o.tm_isdst = -1;
        time_t r = mktime(&o); if (r < now - 90) { o.tm_mon++; if (o.tm_mon > 11) { o.tm_mon = 0; o.tm_year++; } r = mktime(&o); } return r;
    }
    if (e->rep == 3) {
        struct tm* nw = localtime(&now); struct tm o = *nw;
        o.tm_mon = e->mes - 1; o.tm_mday = e->dia; o.tm_hour = e->hora; o.tm_min = e->minuto; o.tm_sec = e->segundo; o.tm_isdst = -1;
        time_t r = mktime(&o); if (r < now - 90) { o.tm_year++; r = mktime(&o); } return r;
    }
    if (e->rep == 4) {
        int N = e->repDias > 0 ? e->repDias : 1;
        double dd = difftime(now, base) / 86400.0;
        long k = (long)(dd / (double)N);
        time_t r = base + (time_t)(k * N) * 86400;
        if (r < now - 90) r += (time_t)N * 86400;
        return r;
    }
    return base;
}

static void ferramentas_verificar_notificacoes(void) {
    time_t now = time(NULL);
    for (int i = 0; i < eventosTotal; i++) {
        EventoItem* e = &eventos[i];
        time_t ev = ferramentas_ocorr(e, now);
        if (ev == (time_t)-1) continue;
        for (int k = 0; k < MAX_NOTIF_OPCOES; k++) {
            if (!(e->notifMask & (1 << k))) continue;
            if (eventoNotificado[i] & (1 << k)) continue;
            time_t trig = ev - (time_t)notifMinutos[k] * 60;
            double diff = difftime(now, trig);
            if (diff >= 0 && diff < 120) {
                eventoNotificado[i] |= (1 << k);
                char msg[256];
                sprintf(msg, "Lembrete: %s (%s)", e->nome, notifNomes[k]);
                strncpy(eventoBannerMsg, msg, sizeof(eventoBannerMsg) - 1);
                eventoBannerMsg[sizeof(eventoBannerMsg) - 1] = 0;
                eventoBannerTimer = 12.0f;
            }
        }
    }
}

// =================== CLIMA (manual + auto) ===================
static void ferramentas_salvar_clima_cfg(void) {
    FILE* f = fopen("data/HyperNeiva/clima_cfg.txt", "w");
    if (f) { fprintf(f, "%d|%63s|%g|%g\n", weatherModo, weatherCidadeManual, weatherLatM, weatherLonM); fclose(f); }
}
static void ferramentas_carregar_clima_cfg(void) {
    FILE* f = fopen("data/HyperNeiva/clima_cfg.txt", "r");
    if (f) {
        int m; char c[64]; double la, lo;
        if (fscanf(f, "%d|%63[^|]|%lf|%lf", &m, c, &la, &lo) == 4) {
            weatherModo = m; strcpy(weatherCidadeManual, c); weatherLatM = la; weatherLonM = lo;
        }
        fclose(f);
    }
}
static void ferramentas_aplicar_clima(double t, int wc, int id, const char* cidade) {
    sprintf(weatherTemp, "%.0f", t); weatherCode = wc; weatherIsDay = (id == 1);
    ferramentas_mapear_clima(wc, weatherIsDay, weatherDesc, &weatherSol);
    if (cidade && cidade[0]) strcpy(weatherCidade, cidade);
}
static void ferramentas_buscar_local(const char* nome) {
    char enc[256]; ferramentas_urlenc(enc, nome);
    char cmd[2048];
    sprintf(cmd, "curl -s \"https://geocoding-api.open-meteo.com/v1/search?name=%s&count=1&language=pt\" > data/HyperNeiva/clima_geo.json", enc);
    ferramentas_exec_curl(cmd);
    FILE* f = fopen("data/HyperNeiva/clima_geo.json", "rb");
    if (f) {
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        char* b = (char*)malloc(sz + 1);
        if (b) {
            fread(b, 1, sz, f); b[sz] = 0;
            double la = ferramentas_parse_double(b, "latitude");
            double lo = ferramentas_parse_double(b, "longitude");
            char nm[64]; ferramentas_json_str(b, "name", nm, sizeof(nm));
            if (la != 0 || lo != 0) {
                weatherModo = 1; weatherLatM = la; weatherLonM = lo;
                if (nm[0]) strcpy(weatherCidadeManual, nm); else strcpy(weatherCidadeManual, nome);
                ferramentas_salvar_clima_cfg();
                weatherCarregado = false; weatherCarregando = false;
                strcpy(weatherStatus, "Buscando clima...");
            } else strcpy(weatherStatus, "Cidade nao encontrada");
            free(b);
        }
        fclose(f);
    } else strcpy(weatherStatus, "Erro ao buscar cidade");
}

// =================== IA / GEMINI (HELPERS) ===================
static bool ia_ext_audio(const char* ext) {
    return (strcmp(ext, ".wav") == 0 || strcmp(ext, ".mp3") == 0 || strcmp(ext, ".aac") == 0 ||
            strcmp(ext, ".ogg") == 0 || strcmp(ext, ".flac") == 0 || strcmp(ext, ".m4a") == 0 ||
            strcmp(ext, ".mp4") == 0 || strcmp(ext, ".webm") == 0);
}
static bool ia_ext_imagem(const char* ext) {
    return (strcmp(ext, ".png") == 0 || strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0 ||
            strcmp(ext, ".bmp") == 0 || strcmp(ext, ".webp") == 0);
}
static void ia_refresh_audio_list(void) {
    iaAudioTotal = 0;
    DIR* d = opendir(GRAV_DIR);
    if (d) {
        struct dirent* de;
        while ((de = readdir(d)) != NULL && iaAudioTotal < IA_MAX_LISTA) {
            const char* ext = strrchr(de->d_name, '.');
            if (ext && ia_ext_audio(ext)) {
                snprintf(iaAudioList[iaAudioTotal], sizeof(iaAudioList[0]), "%s%s", GRAV_DIR, de->d_name);
                iaAudioTotal++;
            }
        }
        closedir(d);
    }
    if (iaAudioSel >= iaAudioTotal) iaAudioSel = -1;
    if (iaArquivoScroll > iaAudioTotal) iaArquivoScroll = 0;
}
static void ia_refresh_imagem_list(void) {
    iaImagemTotal = 0;
    DIR* d = opendir(GRAV_DIR);
    if (d) {
        struct dirent* de;
        while ((de = readdir(d)) != NULL && iaImagemTotal < IA_MAX_LISTA) {
            const char* ext = strrchr(de->d_name, '.');
            if (ext && ia_ext_imagem(ext)) {
                snprintf(iaImagemList[iaImagemTotal], sizeof(iaImagemList[0]), "%s%s", GRAV_DIR, de->d_name);
                iaImagemTotal++;
            }
        }
        closedir(d);
    }
    if (iaImagemSel >= iaImagemTotal) iaImagemSel = -1;
    if (iaArquivoScroll > iaImagemTotal) iaArquivoScroll = 0;
}
static void ia_poll(void) {
    if (iaOcupado && !gemini_busy()) {
        iaOcupado = false;
        if (iaResp.ok) {
            strncpy(iaResposta, iaResp.texto, sizeof(iaResposta) - 1);
            iaResposta[sizeof(iaResposta) - 1] = 0;
            iaTemResposta = true;
            iaTemErro = false;
            if (iaResp.temMedia && iaResp.mediaPath[0]) {
                if (iaRespTipo == 1) {
                    if (iaResTexLoaded) { UnloadTexture(iaResTex); iaResTexLoaded = false; }
                    if (FileExists(iaResp.mediaPath)) {
                        iaResTex = LoadTexture(iaResp.mediaPath);
                        iaResTexLoaded = true;
                        iaMostrarImagem = true;
                    }
                } else if (iaRespTipo == 3) {
                    tocarMusicaNova(iaResp.mediaPath);
                }
            }
        } else {
            iaTemErro = true;
            iaTemResposta = false;
            strncpy(iaErro, iaResp.texto, sizeof(iaErro) - 1);
            iaErro[sizeof(iaErro) - 1] = 0;
        }
    }
}

static const char* ia_basename(const char* p) {
    if (!p) return "";
    const char* s = strrchr(p, '/');
    const char* s2 = strrchr(p, '\\');
    if (s2 > s) s = s2;
    return s ? s + 1 : p;
}

#define IA_LINHAS_MAX 200
static char iaLinhasBuf[IA_LINHAS_MAX][260];
static int iaLinhasTotal = 0;

static void ia_quebrar_linhas(const char* text, int size, int maxW) {
    iaLinhasTotal = 0;
    if (!text || !text[0]) return;
    char linha[520];
    int li = 0;
    linha[0] = 0;
    int i = 0, n = (int)strlen(text);
    while (i < n && iaLinhasTotal < IA_LINHAS_MAX) {
        if (text[i] == '\n') {
            if (li > 0) { snprintf(iaLinhasBuf[iaLinhasTotal++], sizeof(iaLinhasBuf[0]), "%s", linha); linha[0] = 0; li = 0; }
            i++;
            continue;
        }
        if (text[i] == ' ') {
            while (i < n && text[i] == ' ') i++;
            continue;
        }
        int j = i;
        char pal[520]; int pl = 0;
        while (j < n && text[j] != ' ' && text[j] != '\n' && pl < 500) pal[pl++] = text[j++];
        pal[pl] = 0;
        if (li > 0) {
            char tmp[1100];
            memcpy(tmp, linha, li);
            tmp[li] = ' ';
            memcpy(tmp + li + 1, pal, pl);
            tmp[li + 1 + pl] = 0;
            if (medirTexto(tmp, size) <= maxW) {
                linha[li] = ' ';
                memcpy(linha + li + 1, pal, pl);
                li += 1 + pl;
                linha[li] = 0;
                i = j;
                continue;
            }
            snprintf(iaLinhasBuf[iaLinhasTotal++], sizeof(iaLinhasBuf[0]), "%s", linha);
            linha[0] = 0; li = 0;
        }
        memcpy(linha, pal, pl); li = pl; linha[li] = 0;
        i = j;
    }
    if (li > 0 && iaLinhasTotal < IA_LINHAS_MAX) snprintf(iaLinhasBuf[iaLinhasTotal++], sizeof(iaLinhasBuf[0]), "%s", linha);
}

static void ia_desenhar_linhas(int x, int y, int h, uint32_t col, int scroll, int size) {
    int yy = y;
    int step = size + 6;
    for (int i = scroll; i < iaLinhasTotal; i++) {
        if (yy > y + h - size - 4) break;
        desenharTexto(iaLinhasBuf[i], size, x, yy, col);
        yy += step;
    }
}

static void ia_desenhar_painel(int px, int py, int pw, int ph, const char* titulo,
                                const char* textoVazio, Vector2 mousePos, bool clicked) {
    desenharRetanguloAlpha(px, py, pw, ph, 0x0A0A0C);
    DrawRectangleLines(px, py, pw, ph, (Color){ 51, 51, 51, 255 });
    desenharTexto(titulo, 24, px + 20, py + 16, 0xFF00AAFF);

    if (iaTemResposta || iaTemErro) {
        int btnW = 110, btnH = 34;
        int btnX = px + pw - 130;
        int btnY = py + 10;
        bool hovC = (mousePos.x >= btnX && mousePos.x <= btnX + btnW && mousePos.y >= btnY && mousePos.y <= btnY + btnH);
        desenharRetanguloAlpha(btnX, btnY, btnW, btnH, hovC ? 0xFF00AAFF : 0xFF2A3A44);
        DrawRectangleLines(btnX, btnY, btnW, btnH, (Color){0, 255, 255, 255});
        desenharTexto("COPIAR", 18, btnX + (btnW - medirTexto("COPIAR", 18)) / 2, btnY + 8, 0xFFFFFFFF);
        if (hovC && clicked) {
            SetClipboardText(iaTemErro ? iaErro : iaResposta);
        }
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_C)) {
            SetClipboardText(iaTemErro ? iaErro : iaResposta);
        }
    }

    if (iaOcupado) {
        desenharTexto("PROCESSANDO...", 26, px + 20, py + 56, 0xFFFFCC00);
        desenharTexto("Aguarde, a IA esta trabalhando.", 20, px + 20, py + 92, 0xFF888888);
    } else if (iaTemErro) {
        desenharTexto("ERRO", 24, px + 20, py + 56, 0xFFFF5555);
        ia_quebrar_linhas(iaErro, 20, pw - 40);
        ia_desenhar_linhas(px + 20, py + 90, ph - 110, 0xFFFF8888, iaScroll, 20);
    } else if (iaTemResposta) {
        ia_quebrar_linhas(iaResposta, 20, pw - 40);
        ia_desenhar_linhas(px + 20, py + 56, ph - 76, 0xFFEEEEEE, iaScroll, 20);
    } else {
        desenharTexto(textoVazio, 22, px + 20, py + 56, 0xFF666666);
    }
}



// ===================== QR / CODIGO DE BARRAS =====================
extern void __stdcall Sleep(unsigned int dwMilliseconds);

static void qr_set_msg(const char* msg, bool erro) {
    snprintf(qrMsg, sizeof(qrMsg), "%s", msg);
    qrMsgErro = erro;
}

static void qr_ensure_dir(void) {
    if (qrDir[0]) _mkdir(qrDir);
}

static bool qr_find_exe(const char* exe, char* out, int maxOut) {
    snprintf(out, maxOut, "C:\\msys64\\mingw64\\bin\\%s", exe);
    FILE* f = fopen(out, "rb");
    if (f) { fclose(f); return true; }
    snprintf(out, maxOut, "%s", exe);
    f = fopen(out, "rb");
    if (f) { fclose(f); return true; }
    return false;
}

static void qr_mostrar_preview(const char* pngPath) {
    Image img = LoadImage(pngPath);
    if (!img.data) { qr_set_msg("Falha ao carregar preview.", true); return; }
    if (qrTexOk) UnloadTexture(qrTex);
    qrTex = LoadTextureFromImage(img);
    UnloadImage(img);
    qrTexOk = true;
}

// modo: 0=QR Code, 1=CODE128
static bool qr_exec_zint(int modo, const char* dados, const char* label, const char* outPng) {
    char zin[512], cmd[4200], inp[560];
    if (!dados || !dados[0]) { qr_set_msg("Digite o conteudo primeiro.", true); return false; }
    if (!qr_find_exe("zint.exe", zin, sizeof(zin))) { qr_set_msg("zint.exe nao encontrado em C:\\msys64\\mingw64\\bin.", true); return false; }
    qr_ensure_dir();
    snprintf(inp, sizeof(inp), "%s_in.txt", outPng);
    FILE* f = fopen(inp, "wb");
    if (!f) { qr_set_msg("Falha ao criar arquivo temporario.", true); return false; }
    fwrite(dados, 1, strlen(dados), f);
    fclose(f);
    if (modo == 0)
        snprintf(cmd, sizeof(cmd), "\"%s\" -b QRCODE --scale=6 --secure=4 -o \"%s\" -i \"%s\"", zin, outPng, inp);
    else
        snprintf(cmd, sizeof(cmd), "\"%s\" -b CODE128 --height=110 --scale=3 --notext -o \"%s\" -i \"%s\"", zin, outPng, inp);
    exec_silent(cmd);
    remove(inp);
    f = fopen(outPng, "rb");
    if (!f) { qr_set_msg("Falha ao gerar codigo (conteudo muito longo?).", true); return false; }
    fclose(f);
    if (modo == 1 && label && label[0]) {
        Image img = LoadImage(outPng);
        if (img.data) {
            ImageResizeCanvas(&img, img.width, img.height + 52, 0, 0, WHITE);
            int lw = MeasureText(label, 26);
            int lx = (img.width - lw) / 2;
            if (lx < 4) lx = 4;
            ImageDrawText(&img, label, lx, img.height - 44, 26, BLACK);
            ExportImage(img, outPng);
            UnloadImage(img);
        }
    }
    snprintf(qrLastPng, sizeof(qrLastPng), "%s", outPng);
    return true;
}

static void qr_urlencode(const char* in, char* out, int maxLen) {
    static const char hex[] = "0123456789ABCDEF";
    int o = 0;
    for (int i = 0; in[i] && o < maxLen - 4; i++) {
        unsigned char c = (unsigned char)in[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out[o++] = (char)c;
        } else {
            out[o++] = '%';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 0x0F];
        }
    }
    out[o] = '\0';
}

static void* qr_scan_thread(void* arg) {
    (void)arg;
    char zb[512], cmd[1400];
    if (!qr_find_exe("zbarimg.exe", zb, sizeof(zb))) {
        snprintf(lerResultado, sizeof(lerResultado), "zbarimg.exe nao encontrado.");
        lerTemResult = true;
        scanRunning = false;
        return NULL;
    }
    while (!scanStopReq) {
        if (grav_capture_camera_to_png(scanPng)) {
            snprintf(cmd, sizeof(cmd), "\"%s\" -q \"%s\" > \"%s\" 2>&1", zb, scanPng, scanOut);
            exec_silent(cmd);
            FILE* f = fopen(scanOut, "r");
            if (f) {
                char line[1024];
                int got = 0;
                lerResultado[0] = '\0';
                while (fgets(line, sizeof(line), f)) {
                    size_t l = strlen(line);
                    while (l && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = 0;
                    if (!l) continue;
                    got = 1;
                    if (strlen(lerResultado) + l + 2 < sizeof(lerResultado)) {
                        strcat(lerResultado, line);
                        strcat(lerResultado, "\n");
                    }
                }
                fclose(f);
                if (got) { lerTemResult = true; break; }
            }
        }
        Sleep(300);
    }
    scanRunning = false;
    return NULL;
}

static void qr_scan_start_camera(void) {
    if (scanRunning) return;
    qr_ensure_dir();
    snprintf(scanPng, sizeof(scanPng), "%s_cam.png", qrDir);
    snprintf(scanView, sizeof(scanView), "%s_camview.png", qrDir);
    snprintf(scanOut, sizeof(scanOut), "%s_scan.txt", qrDir);
    lerResultado[0] = '\0';
    lerTemResult = false;
    remove(scanView);
    if (!grav_print_preview_start(0)) { qr_set_msg("Falha ao abrir a camera.", true); return; }
    lerPrevOn = true;
    lerModo = 1;
    scanStopReq = false;
    scanRunning = true;
    if (pthread_create(&scanTid, NULL, qr_scan_thread, NULL) == 0) scanTidValid = true;
    else { scanRunning = false; scanStopReq = true; }
}

static void qr_scan_stop(void) {
    scanStopReq = true;
    if (scanTidValid) { pthread_join(scanTid, NULL); scanTidValid = false; }
    if (lerPrevOn) { grav_print_preview_stop(); lerPrevOn = false; }
    scanRunning = false;
    if (lerModo == 1) lerModo = 0;
}

static void qr_decode_imagem(const char* path) {
    char zb[512], cmd[1600], out[560];
    if (!path || !path[0]) { qr_set_msg("Informe o caminho da imagem.", true); return; }
    if (!qr_find_exe("zbarimg.exe", zb, sizeof(zb))) { qr_set_msg("zbarimg.exe nao encontrado.", true); return; }
    qr_ensure_dir();
    snprintf(out, sizeof(out), "%s_imgscan.txt", qrDir);
    snprintf(cmd, sizeof(cmd), "\"%s\" -q \"%s\" > \"%s\" 2>&1", zb, path, out);
    exec_silent(cmd);
    lerResultado[0] = '\0';
    bool got = false;
    FILE* f = fopen(out, "r");
    if (f) {
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            size_t l = strlen(line);
            while (l && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = 0;
            if (!l) continue;
            got = true;
            if (strlen(lerResultado) + l + 2 < sizeof(lerResultado)) { strcat(lerResultado, line); strcat(lerResultado, "\n"); }
        }
        fclose(f);
    }
    remove(out);
    lerTemResult = got;
    lerModo = 2;
    if (got) qr_set_msg("", false);
    else qr_set_msg("Nenhum codigo encontrado na imagem.", true);
}

static void qr_listar_imagens(void) {
    lerTotal = 0;
    DIR* d = opendir(g_gravDir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) != NULL && lerTotal < QR_MAX_LISTA) {
        const char* n = e->d_name;
        size_t nl = strlen(n);
        if (nl < 5 || nl > 250) continue;
        const char* ext = n + nl - 4;
        bool ok = (_stricmp(ext, ".png") == 0) || (_stricmp(ext, ".jpg") == 0) ||
                  (_stricmp(ext, ".bmp") == 0) || (_stricmp(ext, ".gif") == 0);
        if (!ok) ok = (_stricmp(n + nl - 5, ".jpeg") == 0);
        if (!ok) continue;
        snprintf(lerLista[lerTotal], sizeof(lerLista[0]), "%s%s", g_gravDir, n);
        lerTotal++;
    }
    closedir(d);
    if (lerScroll > lerTotal) lerScroll = 0;
}

static void qr_gerar_whatsapp(void) {
    char fone[32];
    int j = 0;
    for (int i = 0; wpFone[i] && j < 31; i++)
        if (wpFone[i] >= '0' && wpFone[i] <= '9') fone[j++] = wpFone[i];
    fone[j] = '\0';
    if (j < 10) { qr_set_msg("Numero invalido. Use DDI+DDD+numero (ex: 5511999998888).", true); return; }
    char enc[2048];
    qr_urlencode(wpMsg, enc, sizeof(enc));
    if (enc[0]) snprintf(wpLink, sizeof(wpLink), "https://wa.me/%s?text=%s", fone, enc);
    else snprintf(wpLink, sizeof(wpLink), "https://wa.me/%s", fone);
    wpTemLink = true;
    char png[560];
    snprintf(png, sizeof(png), "%s_prev.png", qrDir);
    if (qr_exec_zint(0, wpLink, NULL, png)) {
        qr_mostrar_preview(png);
        qr_set_msg("QR Code do WhatsApp gerado!", false);
    }
}

static void qr_salvar_png(void) {
    if (!qrLastPng[0] || !qrTexOk) { qr_set_msg("Gere um codigo primeiro.", true); return; }
    qr_ensure_dir();
    time_t t = time(NULL);
    struct tm* lt = localtime(&t);
    char dest[640];
    snprintf(dest, sizeof(dest), "%scodigo_%04d%02d%02d_%02d%02d%02d.png",
             qrDir, lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
             lt->tm_hour, lt->tm_min, lt->tm_sec);
    FILE* s = fopen(qrLastPng, "rb");
    if (!s) { qr_set_msg("Arquivo gerado nao existe mais.", true); return; }
    FILE* d2 = fopen(dest, "wb");
    if (!d2) { fclose(s); qr_set_msg("Falha ao salvar arquivo.", true); return; }
    char buf[8192];
    size_t r;
    while ((r = fread(buf, 1, sizeof(buf), s)) > 0) fwrite(buf, 1, r, d2);
    fclose(s);
    fclose(d2);
    qr_set_msg(dest, false);
}

static void qr_abrir_link(void) {
    if (!wpTemLink || !wpLink[0]) { qr_set_msg("Gere o link primeiro.", true); return; }
    char cmd[1700];
    snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", wpLink);
    exec_async_silent(cmd);
}

// Entrada de texto do campo focado (teclado + colar Ctrl+V)
static void qr_input_texto(char* buf, int cap) {
    int ch = GetCharPressed();
    while (ch > 0) {
        if (ch >= 32 && ch <= 255 && (int)strlen(buf) < cap - 1) {
            int l = (int)strlen(buf);
            buf[l] = (char)ch;
            buf[l + 1] = '\0';
        }
        ch = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && strlen(buf) > 0)
        buf[strlen(buf) - 1] = '\0';
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V)) {
        const char* cp = GetClipboardText();
        if (cp) {
            int l = (int)strlen(buf);
            for (int i = 0; cp[i] && l < cap - 1; i++)
                if ((unsigned char)cp[i] >= 32) buf[l++] = cp[i];
            buf[l] = '\0';
        }
    }
}

// Desenha texto cortando caracteres que ultrapassem maxW
static void qr_desenha_linha_clip(const char* s, int size, int x, int y, int maxW, uint32_t cor) {
    char tmp[1024];
    int n = 0;
    while (s[n] && n < 1023) {
        tmp[n] = s[n];
        tmp[n + 1] = '\0';
        if (medirTexto(tmp, size) > maxW) { tmp[n] = '\0'; break; }
        n++;
    }
    desenharTexto(tmp, size, x, y, cor);
}

void renderizarFerramentasRaylib(void) {
    if (!toolsRenderTextureLoaded) return;

    Vector2 mousePosRaw = GetMousePosition();
    Vector2 mousePos = {
        mousePosRaw.x * (1920.0f / (float)GetScreenWidth()),
        mousePosRaw.y * (1080.0f / (float)GetScreenHeight())
    };
    bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool holding = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    float wheelMove = GetMouseWheelMove();
    int txtW = 0;

    // ==========================================
    // UPDATE SECTION BY SUB-MODE
    // ==========================================
    if (toolMode == TOOL_MENU) {
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_LEFT)) {
            menuSelection = (menuSelection - 1 + 9) % 9;
        }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_RIGHT)) {
            menuSelection = (menuSelection + 1) % 9;
        }
        if (IsKeyPressed(KEY_ENTER)) {
            if (menuSelection == 0) toolMode = TOOL_CALCULADORA;
            else if (menuSelection == 1) toolMode = TOOL_NOTEPAD;
            else if (menuSelection == 2) toolMode = TOOL_TIMER;
            else if (menuSelection == 3) toolMode = TOOL_GRAVADOR;
            else if (menuSelection == 4) toolMode = TOOL_CONTATOS;
            else if (menuSelection == 5) toolMode = TOOL_CAIXA;
            else if (menuSelection == 6) toolMode = TOOL_TERMINAL;
            else if (menuSelection == 7) toolMode = TOOL_IA;
            else if (menuSelection == 8) toolMode = TOOL_QRCODE;
        }
    }
    else if (toolMode == TOOL_QRCODE) {
        // Abas
        for (int i = 0; i < 4; i++) {
            int tx = 100 + i * 450, ty = 170, tw = 430, th = 55;
            bool h = CheckCollisionPointRec(mousePos, (Rectangle){ (float)tx, (float)ty, (float)tw, (float)th });
            if (h && clicked) { qrTab = i; qrFoco = false; brFoco = 0; wpFoco = 0; lerFocoPath = false; }
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (scanRunning || lerPrevOn) qr_scan_stop();
            else toolMode = TOOL_MENU;
        }
        if (qrTab == 0) {
            int ix = 100, iy = 295, iw = 1100, ih = 54;
            bool hIn = CheckCollisionPointRec(mousePos, (Rectangle){ (float)ix, (float)iy, (float)iw, (float)ih });
            if (hIn && clicked) qrFoco = true;
            else if (clicked && !CheckCollisionPointRec(mousePos, (Rectangle){ 1240.0f, 295.0f, 180.0f, 54.0f })) qrFoco = false;
            if (qrFoco) qr_input_texto(qrInput, (int)sizeof(qrInput));
            bool hGerar = CheckCollisionPointRec(mousePos, (Rectangle){ 1240.0f, 295.0f, 180.0f, 54.0f });
            if ((hGerar && clicked) || (qrFoco && IsKeyPressed(KEY_ENTER))) {
                char png[560];
                snprintf(png, sizeof(png), "%s_prev.png", qrDir);
                if (qr_exec_zint(0, qrInput, NULL, png)) {
                    qr_mostrar_preview(png);
                    qr_set_msg("QR Code gerado!", false);
                }
            }
            bool hSalvar = CheckCollisionPointRec(mousePos, (Rectangle){ 760.0f, 850.0f, 190.0f, 48.0f });
            if (hSalvar && clicked) qr_salvar_png();
        }
        else if (qrTab == 1) {
            Rectangle rDados = { 100, 295, 700, 54 };
            Rectangle rLabel = { 100, 385, 700, 50 };
            Rectangle rGerar = { 840, 295, 200, 54 };
            Rectangle rSalvar = { 560, 850, 190, 48 };
            if (clicked) {
                if (CheckCollisionPointRec(mousePos, rDados)) brFoco = 1;
                else if (CheckCollisionPointRec(mousePos, rLabel)) brFoco = 2;
                else if (!CheckCollisionPointRec(mousePos, rGerar)) brFoco = 0;
            }
            if (brFoco == 1) qr_input_texto(brDados, (int)sizeof(brDados));
            else if (brFoco == 2) qr_input_texto(brLabel, (int)sizeof(brLabel));
            bool hGerar = CheckCollisionPointRec(mousePos, rGerar);
            if ((hGerar && clicked) || (brFoco == 1 && IsKeyPressed(KEY_ENTER))) {
                char png[560];
                snprintf(png, sizeof(png), "%s_prev.png", qrDir);
                if (qr_exec_zint(1, brDados, brLabel, png)) {
                    qr_mostrar_preview(png);
                    qr_set_msg("Etiqueta de codigo de barras gerada!", false);
                }
            }
            if (CheckCollisionPointRec(mousePos, rSalvar) && clicked) qr_salvar_png();
        }
        else if (qrTab == 2) {
            Rectangle rFone = { 100, 295, 420, 54 };
            Rectangle rMsg = { 560, 295, 860, 54 };
            Rectangle rGerar = { 100, 385, 320, 54 };
            Rectangle rAbrir = { 450, 385, 320, 54 };
            Rectangle rSalvar = { 760, 930, 190, 48 };
            if (clicked) {
                if (CheckCollisionPointRec(mousePos, rFone)) wpFoco = 1;
                else if (CheckCollisionPointRec(mousePos, rMsg)) wpFoco = 2;
                else if (!CheckCollisionPointRec(mousePos, rGerar) && !CheckCollisionPointRec(mousePos, rAbrir)) wpFoco = 0;
            }
            if (wpFoco == 1) qr_input_texto(wpFone, (int)sizeof(wpFone));
            else if (wpFoco == 2) qr_input_texto(wpMsg, (int)sizeof(wpMsg));
            if (CheckCollisionPointRec(mousePos, rGerar) && clicked) qr_gerar_whatsapp();
            if (CheckCollisionPointRec(mousePos, rAbrir) && clicked) qr_abrir_link();
            if (CheckCollisionPointRec(mousePos, rSalvar) && clicked) qr_salvar_png();
        }
        else { // LER CODIGO
            Rectangle rCam = { 100, 280, 340, 54 };
            Rectangle rPath = { 100, 400, 520, 50 };
            Rectangle rLerImg = { 640, 400, 160, 50 };
            if (CheckCollisionPointRec(mousePos, rCam) && clicked) {
                if (scanRunning || lerPrevOn) qr_scan_stop();
                else qr_scan_start_camera();
            }
            if (clicked) lerFocoPath = CheckCollisionPointRec(mousePos, rPath);
            if (lerFocoPath) qr_input_texto(lerPathIn, (int)sizeof(lerPathIn));
            if (CheckCollisionPointRec(mousePos, rLerImg) && clicked) {
                qr_decode_imagem(lerPathIn);
            }
            // Lista de imagens das gravacoes
            int listY = 590, rowH = 44;
            int vis = 7;
            if (wheelMove != 0) { lerScroll -= (int)(wheelMove * 2); if (lerScroll < 0) lerScroll = 0; int lmax = (lerTotal > 7) ? lerTotal - 7 : 0; if (lerScroll > lmax) lerScroll = lmax; }
            for (int i = 0; i < vis && (i + lerScroll) < lerTotal; i++) {
                int ry = listY + i * rowH;
                bool h = CheckCollisionPointRec(mousePos, (Rectangle){ 100.0f, (float)ry, 700.0f, (float)rowH - 4 });
                if (h && clicked) qr_decode_imagem(lerLista[i + lerScroll]);
            }
            // Atualiza textura da camera (~3 fps)
            if (lerPrevOn && (GetTime() - lerCamLastLoad) > 0.35) {
                lerCamLastLoad = GetTime();
                if (grav_capture_camera_to_png(scanView)) {
                    Image img = LoadImage(scanView);
                    if (img.data) {
                        if (lerCamTexOk) UnloadTexture(lerCamTex);
                        lerCamTex = LoadTextureFromImage(img);
                        UnloadImage(img);
                        lerCamTexOk = true;
                    }
                }
            }
            // Copiar resultado
            Rectangle rCop = { 1420, 910, 180, 46 };
            if (CheckCollisionPointRec(mousePos, rCop) && clicked && lerTemResult) {
                SetClipboardText(lerResultado);
                qr_set_msg("Resultado copiado!", false);
            }
        }
    }
    else if (toolMode == TOOL_CALCULADORA) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            toolMode = TOOL_MENU;
        }

        int charKey = GetCharPressed();
        while (charKey > 0) {
            if (charKey >= '0' && charKey <= '9') {
                char valStr[2] = { (char)charKey, '\0' };
                handleCalcInput(valStr);
            }
            else if (charKey == '.') {
                handleCalcInput(".");
            }
            else if (charKey == '+') handleCalcOp('+');
            else if (charKey == '-') handleCalcOp('-');
            else if (charKey == '*') handleCalcOp('*');
            else if (charKey == '/') handleCalcOp('/');
            charKey = GetCharPressed();
        }

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_EQUAL)) {
            handleCalcEqual();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            handleCalcBackspace();
        }
        if (IsKeyPressed(KEY_C) || IsKeyPressed(KEY_ESCAPE)) {
            handleCalcClear();
        }
    }
    else if (toolMode == TOOL_NOTEPAD) {
        if (notepadState == NOTEPAD_EDIT) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                salvarNotasFerramenta();
                toolMode = TOOL_MENU;
            }

            int charKey = GetCharPressed();
            while (charKey > 0) {
                if (charKey >= 32 && charKey <= 255 && notepadTextLen < (int)sizeof(notepadText) - 2) {
                    notepadText[notepadTextLen++] = (char)charKey;
                    notepadText[notepadTextLen] = '\0';
                }
                charKey = GetCharPressed();
            }

            if (IsKeyPressed(KEY_ENTER) && notepadTextLen < (int)sizeof(notepadText) - 2) {
                notepadText[notepadTextLen++] = '\n';
                notepadText[notepadTextLen] = '\0';
            }

            if (IsKeyDown(KEY_BACKSPACE)) {
                backspaceTimer += GetFrameTime();
                if (backspaceTimer > 0.08f) {
                    if (notepadTextLen > 0) {
                        notepadText[--notepadTextLen] = '\0';
                    }
                    backspaceTimer = 0.0f;
                }
            } else {
                backspaceTimer = 0.5f;
            }
            if (IsKeyPressed(KEY_BACKSPACE)) {
                if (notepadTextLen > 0) {
                    notepadText[--notepadTextLen] = '\0';
                }
            }

            if (wheelMove != 0) {
                notepadScrollY += (int)(wheelMove * 35);
                if (notepadScrollY > 0) notepadScrollY = 0;
            }
        }
        else if (notepadState == NOTEPAD_OPEN_EXPLORER) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                notepadState = NOTEPAD_EDIT;
            }

            if (wheelMove != 0) {
                noteScroll -= (int)(wheelMove * 2);
                if (noteScroll > noteTotalItens - 12) noteScroll = noteTotalItens - 12;
                if (noteScroll < 0) noteScroll = 0;
            }

            if (IsKeyPressed(KEY_UP)) {
                noteSel--;
                if (noteSel < 0) noteSel = noteTotalItens > 0 ? noteTotalItens - 1 : 0;
                if (noteSel < noteScroll) noteScroll = noteSel;
                if (noteSel > noteScroll + 11) noteScroll = noteSel - 11;
            }
            if (IsKeyPressed(KEY_DOWN)) {
                noteSel++;
                if (noteSel >= noteTotalItens) noteSel = 0;
                if (noteSel > noteScroll + 11) noteScroll = noteSel - 11;
                if (noteSel < noteScroll) noteScroll = noteSel;
            }
            if (IsKeyPressed(KEY_ENTER)) {
                if (noteTotalItens > 0) {
                    if (noteEhPasta[noteSel]) {
                        if (strcmp(notePaths[noteSel], "..") == 0) {
                            char* lastSlash = strrchr(noteCaminhoAtual, '/');
                            if (!lastSlash) lastSlash = strrchr(noteCaminhoAtual, '\\');
                            if (lastSlash && lastSlash != noteCaminhoAtual) {
                                *lastSlash = '\0';
                                noteListarArquivos(noteCaminhoAtual);
                            }
                        } else {
                            strcpy(noteCaminhoAtual, notePaths[noteSel]);
                            noteListarArquivos(noteCaminhoAtual);
                        }
                    } else {
                        // Load notepad file
                        FILE* f = fopen(notePaths[noteSel], "r");
                        if (f) {
                            size_t readBytes = fread(notepadText, 1, sizeof(notepadText) - 1, f);
                            notepadText[readBytes] = '\0';
                            notepadTextLen = (int)readBytes;
                            fclose(f);
                        }
                        notepadState = NOTEPAD_EDIT;
                    }
                }
            }
            if (IsKeyPressed(KEY_BACKSPACE)) {
                char* lastSlash = strrchr(noteCaminhoAtual, '/');
                if (!lastSlash) lastSlash = strrchr(noteCaminhoAtual, '\\');
                if (lastSlash && lastSlash != noteCaminhoAtual) {
                    *lastSlash = '\0';
                    noteListarArquivos(noteCaminhoAtual);
                }
            }
        }
        else if (notepadState == NOTEPAD_SAVE_DIALOG) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                notepadState = NOTEPAD_EDIT;
            }

            if (wheelMove != 0) {
                noteScroll -= (int)(wheelMove * 2);
                if (noteScroll > noteTotalItens - 12) noteScroll = noteTotalItens - 12;
                if (noteScroll < 0) noteScroll = 0;
            }

            int charKey = GetCharPressed();
            while (charKey > 0) {
                if (((charKey >= 'a' && charKey <= 'z') || (charKey >= 'A' && charKey <= 'Z') ||
                     (charKey >= '0' && charKey <= '9') || charKey == '_' || charKey == '-' || charKey == '.' || charKey == ' ') &&
                    strlen(saveFilename) < 60) {
                    int sl = strlen(saveFilename);
                    saveFilename[sl] = (char)charKey;
                    saveFilename[sl + 1] = '\0';
                }
                charKey = GetCharPressed();
            }

            if (IsKeyPressed(KEY_BACKSPACE)) {
                int sl = strlen(saveFilename);
                if (sl > 0) saveFilename[sl - 1] = '\0';
            }

            if (IsKeyPressed(KEY_ENTER)) {
                if (strlen(saveFilename) > 0) {
                    char fullPath[1024];
                    if (!strstr(saveFilename, ".txt")) {
                        sprintf(fullPath, "%s/%s.txt", noteCaminhoAtual, saveFilename);
                    } else {
                        sprintf(fullPath, "%s/%s", noteCaminhoAtual, saveFilename);
                    }

                    #ifdef _WIN32
                    _mkdir("data");
                    _mkdir("data/HyperNeiva");
                    _mkdir("data/HyperNeiva/midia");
                    _mkdir("data/HyperNeiva/midia/leitura");
                    #else
                    mkdir("data", 0777);
                    mkdir("data/HyperNeiva", 0777);
                    mkdir("data/HyperNeiva/midia", 0777);
                    mkdir("data/HyperNeiva/midia/leitura", 0777);
                    #endif

                    FILE* f = fopen(fullPath, "w");
                    if (f) {
                        fwrite(notepadText, 1, strlen(notepadText), f);
                        fclose(f);
                    }
                    notepadState = NOTEPAD_EDIT;
                }
            }
        }
    }
      else if (toolMode == TOOL_GRAVADOR) {
          int bx = 180, by = 170, bw = 500, bh = 55;
          bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

          int vltX = 1600, vltY = 80, vltW = 60, vltH = 50;
          bool hoverVoltar = CheckCollisionPointRec(mousePos, (Rectangle){vltX, vltY, vltW, vltH});
          if (hoverVoltar && clicked) {
              if (gravShowMenu) {
                  toolMode = TOOL_MENU;
              } else if (gravState == 0) {
                  if (printPreviewOn || grav_video_preview_active()) { grav_print_preview_stop(); printPreviewOn = false; }
                  gravShowMenu = true;
              } else if (gravState == 2 || gravState == 3) {
                  grav_stop_recording();
                  if (gravTipo == 0) remove(GRAV_TEMP_RAW);
                  else if (gravTipo == 1) remove(GRAV_VIDEO_TEMP);
                  else remove(GRAV_TELA_TEMP);
                  gravState = 0;
                  gravDeleteIdx = -1;
              } else if (gravState == 4) {
                  gravState = 0;
              } else if (gravState == 5) {
                  if (gravTipo == 0) remove(GRAV_TEMP_RAW);
                  else if (gravTipo == 1) remove(GRAV_VIDEO_TEMP);
                  else remove(GRAV_TELA_TEMP);
                  gravState = 0;
              }
          }

         if (gravShowMenu) {
             int mw = 420, mh = 90, mx = 1220, my = 250, mg = 30;
             bool hA = CheckCollisionPointRec(mousePos, (Rectangle){mx, my, mw, mh});
             bool hV = CheckCollisionPointRec(mousePos, (Rectangle){mx, my + mh + mg, mw, mh});
             bool hT = CheckCollisionPointRec(mousePos, (Rectangle){mx, my + 2 * (mh + mg), mw, mh});
             bool hP = CheckCollisionPointRec(mousePos, (Rectangle){mx, my + 3 * (mh + mg), mw, mh});
             if (hA && clicked) { gravTipo = 0; gravShowMenu = false; gravState = 0; }
             else if (hV && clicked) { gravTipo = 1; gravShowMenu = false; gravState = 0; }
             else if (hT && clicked) { gravTipo = 2; gravShowMenu = false; gravState = 0; }
             else if (hP && clicked) { gravTipo = 3; printFonte = 0; printModo = 0; gravShowMenu = false; gravState = 0; }
             if (IsKeyPressed(KEY_ESCAPE)) toolMode = TOOL_MENU;
         }
         else if (gravTipo == 0) {
             if (gravState == 0) {
                 bool hoverBtn = CheckCollisionPointRec(mousePos, (Rectangle){bx, by, bw, bh});
                 if (hoverBtn && clicked) {
                     remove(GRAV_TEMP_RAW);
                     gravLastSize = 0;
                     gravLastSizeTime = GetTime();
                     gravLastGrowthTime = GetTime();
                     if (grav_start_recording(GRAV_TEMP_RAW)) {
                         gravState = 2;
                         gravRecStartTime = GetTime();
                         gravNomeInput[0] = 0;
                     } else {
                         snprintf(gravErrorMsg, sizeof(gravErrorMsg), "%s", grav_last_error_msg());
                         if (strlen(gravErrorMsg) == 0) snprintf(gravErrorMsg, sizeof(gravErrorMsg), "Erro ao iniciar gravacao. Verifique se o ffmpeg esta instalado e o microfone conectado.");
                         gravErrorTimer = GetTime();
                         gravState = 4;
                     }
                 }
             }
              else if (gravState == 2 || gravState == 3) {
                  int iconSz = 50, gap = 16;
                  bool hoverPausa = CheckCollisionPointRec(mousePos, (Rectangle){bx, by, iconSz, iconSz});
                  bool hoverMic = CheckCollisionPointRec(mousePos, (Rectangle){bx + iconSz + gap, by, iconSz, iconSz});
                  bool hoverTela = CheckCollisionPointRec(mousePos, (Rectangle){bx + 2*(iconSz+gap), by, iconSz, iconSz});
                  bool hoverParar = CheckCollisionPointRec(mousePos, (Rectangle){bx + 3*(iconSz+gap) + 10, by, 240, bh});
                  if (hoverMic && clicked) {
                      gravMicMuted = !gravMicMuted;
                      grav_set_mic_mute(gravMicMuted);
                  }
                  if (hoverTela && clicked && gravTipo == 2) {
                      if (gravState == 2) { grav_pause_recording(); gravState = 3; }
                      else { grav_resume_recording(); gravLastGrowthTime = GetTime(); gravState = 2; }
                  }
                  if (gravState == 2) {
                      if (hoverPausa && clicked) { grav_pause_recording(); gravState = 3; }
                      if (hoverParar && clicked) {
                          grav_stop_recording();
                          gravState = 5;
                          gravNomeInput[0] = 0;
                      }
                      double now = GetTime();
                      if (gravState == 2 && now - gravLastSizeTime > 1.0) {
                          gravLastSizeTime = now;
                          long sz = grav_temp_file_size(GRAV_TEMP_RAW);
                          if (sz > gravLastSize) gravLastGrowthTime = now;
                          if (!grav_is_recording()) {
                              grav_stop_recording();
                              snprintf(gravErrorMsg, sizeof(gravErrorMsg), "Gravacao falhou. Verifique se o microfone esta conectado e disponivel.");
                              gravErrorTimer = GetTime();
                              gravState = 4;
                          } else if (now - gravLastGrowthTime > 10.0) {
                              grav_stop_recording();
                              snprintf(gravErrorMsg, sizeof(gravErrorMsg), "Gravacao nao esta gravando. Verifique se o microfone esta disponivel.");
                              gravErrorTimer = GetTime();
                              gravState = 4;
                          }
                          gravLastSize = sz;
                      }
                  } else {
                      if (hoverPausa && clicked) { grav_resume_recording(); gravLastGrowthTime = GetTime(); gravState = 2; }
                      if (hoverParar && clicked) {
                          grav_stop_recording();
                          gravState = 5;
                          gravNomeInput[0] = 0;
                      }
                  }
                 if (IsKeyPressed(KEY_ESCAPE)) {
                     grav_stop_recording();
                     remove(GRAV_TEMP_RAW);
                     gravState = 0;
                     toolMode = TOOL_MENU;
                 }
             }
             else if (gravState == 5) {
                 int ch = GetCharPressed();
                 while (ch > 0) {
                     if (ch >= 32 && ch <= 255 && strlen(gravNomeInput) < 100) {
                         int l = strlen(gravNomeInput);
                         gravNomeInput[l] = (char)ch;
                         gravNomeInput[l+1] = 0;
                     }
                     ch = GetCharPressed();
                 }
                  if (IsKeyPressed(KEY_BACKSPACE) && strlen(gravNomeInput) > 0)
                      gravNomeInput[strlen(gravNomeInput)-1] = 0;
                  bool hoverSalvar = CheckCollisionPointRec(mousePos, (Rectangle){bx, by + bh + 20, 160, 45});
                  if (IsKeyPressed(KEY_ENTER) || (hoverSalvar && clicked)) {
                      gravSalvarAtual();
                  }
                  if (IsKeyPressed(KEY_ESCAPE)) {
                      remove(GRAV_TEMP_RAW);
                      gravState = 0;
                  }
              }
          }
           else if (gravTipo == 1) {
               if (gravState == 0) {
                   bool hoverBtn = CheckCollisionPointRec(mousePos, (Rectangle){bx, by, bw, bh});
                   if (hoverBtn && clicked) {
                       remove(GRAV_VIDEO_TEMP);
                       gravLastSize = 0;
                       gravLastSizeTime = GetTime();
                       gravLastGrowthTime = GetTime();
                       if (grav_video_start_recording(GRAV_VIDEO_TEMP)) {
                           gravState = 2;
                           gravRecStartTime = GetTime();
                           gravNomeInput[0] = 0;
                       } else {
                           snprintf(gravErrorMsg, sizeof(gravErrorMsg), "%s", grav_last_error_msg());
                           if (strlen(gravErrorMsg) == 0) snprintf(gravErrorMsg, sizeof(gravErrorMsg), "Erro ao iniciar gravacao de video. Verifique se a webcam esta disponivel.");
                           gravErrorTimer = GetTime();
                           gravState = 4;
                       }
                   }
               }
               else if (gravState == 2 || gravState == 3) {
                   int iconSz = 50, gap = 16;
                   bool hoverPausa = CheckCollisionPointRec(mousePos, (Rectangle){bx, by, iconSz, iconSz});
                   bool hoverMic = CheckCollisionPointRec(mousePos, (Rectangle){bx + iconSz + gap, by, iconSz, iconSz});
                   bool hoverTela = CheckCollisionPointRec(mousePos, (Rectangle){bx + 2*(iconSz+gap), by, iconSz, iconSz});
                   bool hoverParar = CheckCollisionPointRec(mousePos, (Rectangle){bx + 3*(iconSz+gap) + 10, by, 240, bh});
                   if (hoverMic && clicked) {
                       gravMicMuted = !gravMicMuted;
                       grav_set_mic_mute(gravMicMuted);
                   }
                   if (hoverTela && clicked) {
                       gravVideoHasScreen = !gravVideoHasScreen;
                       grav_stop_recording();
                       char part1[512];
                       snprintf(part1, sizeof(part1), "%s_p1.mp4", GRAV_VIDEO_TEMP);
                       rename(GRAV_VIDEO_TEMP, part1);
                       if (gravVideoHasScreen) {
                           grav_video_screen_start_recording(GRAV_VIDEO_TEMP);
                       } else {
                           grav_video_start_recording(GRAV_VIDEO_TEMP);
                       }
                       gravState = 2;
                   }
                   if (gravState == 2) {
                       if (hoverPausa && clicked) { grav_pause_recording(); gravState = 3; }
                       if (hoverParar && clicked) {
                           grav_stop_recording();
                           char part1[512], combined[512];
                           snprintf(part1, sizeof(part1), "%s_p1.mp4", GRAV_VIDEO_TEMP);
                           snprintf(combined, sizeof(combined), "%s_comb.mp4", GRAV_VIDEO_TEMP);
                           FILE* f = fopen(part1, "rb");
                           if (f) {
                               fclose(f);
                               grav_concat_mp4(part1, GRAV_VIDEO_TEMP, combined);
                               remove(part1);
                               remove(GRAV_VIDEO_TEMP);
                               rename(combined, GRAV_VIDEO_TEMP);
                           }
                           gravState = 5;
                           gravNomeInput[0] = 0;
                       }
                        double now = GetTime();
                        if (gravState == 2 && now - gravLastSizeTime > 1.0) {
                            gravLastSizeTime = now;
                            long sz = grav_temp_file_size(GRAV_VIDEO_TEMP);
                           if (sz > gravLastSize) gravLastGrowthTime = now;
                           if (!grav_is_recording()) {
                               grav_stop_recording();
                               snprintf(gravErrorMsg, sizeof(gravErrorMsg), "Gravacao de video falhou. Verifique a webcam.");
                               gravErrorTimer = GetTime();
                               gravState = 4;
                           } else if (now - gravLastGrowthTime > 15.0) {
                               grav_stop_recording();
                               snprintf(gravErrorMsg, sizeof(gravErrorMsg), "Gravacao de video nao esta gravando.");
                               gravErrorTimer = GetTime();
                               gravState = 4;
                           }
                           gravLastSize = sz;
                       }
                   } else {
                       if (hoverPausa && clicked) { grav_resume_recording(); gravLastGrowthTime = GetTime(); gravState = 2; }
                       if (hoverParar && clicked) {
                           grav_stop_recording();
                           char part1[512], combined[512];
                           snprintf(part1, sizeof(part1), "%s_p1.mp4", GRAV_VIDEO_TEMP);
                           snprintf(combined, sizeof(combined), "%s_comb.mp4", GRAV_VIDEO_TEMP);
                           FILE* f = fopen(part1, "rb");
                           if (f) {
                               fclose(f);
                               grav_concat_mp4(part1, GRAV_VIDEO_TEMP, combined);
                               remove(part1);
                               remove(GRAV_VIDEO_TEMP);
                               rename(combined, GRAV_VIDEO_TEMP);
                           }
                           gravState = 5;
                           gravNomeInput[0] = 0;
                       }
                   }
                   if (IsKeyPressed(KEY_ESCAPE)) {
                       grav_stop_recording();
                       remove(GRAV_VIDEO_TEMP);
                       char part1[512];
                       snprintf(part1, sizeof(part1), "%s_p1.mp4", GRAV_VIDEO_TEMP);
                       remove(part1);
                       gravState = 0;
                       toolMode = TOOL_MENU;
                   }
              }
              else if (gravState == 5) {
                  int ch = GetCharPressed();
                  while (ch > 0) {
                      if (ch >= 32 && ch <= 255 && strlen(gravNomeInput) < 100) {
                          int l = strlen(gravNomeInput);
                          gravNomeInput[l] = (char)ch;
                          gravNomeInput[l+1] = 0;
                      }
                      ch = GetCharPressed();
                  }
                  if (IsKeyPressed(KEY_BACKSPACE) && strlen(gravNomeInput) > 0)
                      gravNomeInput[strlen(gravNomeInput)-1] = 0;
                  bool hoverSalvar = CheckCollisionPointRec(mousePos, (Rectangle){bx, by + bh + 20, 160, 45});
                  if (IsKeyPressed(KEY_ENTER) || (hoverSalvar && clicked)) {
                      gravSalvarAtual();
                  }
                   if (IsKeyPressed(KEY_ESCAPE)) {
                       remove(GRAV_VIDEO_TEMP);
                       char part1[512];
                       snprintf(part1, sizeof(part1), "%s_p1.mp4", GRAV_VIDEO_TEMP);
                       remove(part1);
                       gravState = 0;
                   }
              }
          }
          else if (gravTipo == 2) {
             if (gravState == 0) {
                 bool hoverBtn = CheckCollisionPointRec(mousePos, (Rectangle){bx, by, bw, bh});
                 if (hoverBtn && clicked) {
                     remove(GRAV_TELA_TEMP);
                     gravLastSize = 0;
                     gravLastSizeTime = GetTime();
                     gravLastGrowthTime = GetTime();
                     if (grav_screen_start_recording(GRAV_TELA_TEMP)) {
                         gravState = 2;
                         gravRecStartTime = GetTime();
                         gravNomeInput[0] = 0;
                     } else {
                         snprintf(gravErrorMsg, sizeof(gravErrorMsg), "%s", grav_last_error_msg());
                         if (strlen(gravErrorMsg) == 0) snprintf(gravErrorMsg, sizeof(gravErrorMsg), "Erro ao iniciar gravacao de tela.");
                         gravErrorTimer = GetTime();
                         gravState = 4;
                     }
                 }
             }
              else if (gravState == 2 || gravState == 3) {
                  int iconSz = 50, gap = 16;
                  bool hoverPausa = CheckCollisionPointRec(mousePos, (Rectangle){bx, by, iconSz, iconSz});
                  bool hoverMic = CheckCollisionPointRec(mousePos, (Rectangle){bx + iconSz + gap, by, iconSz, iconSz});
                  bool hoverTela = CheckCollisionPointRec(mousePos, (Rectangle){bx + 2*(iconSz+gap), by, iconSz, iconSz});
                  bool hoverParar = CheckCollisionPointRec(mousePos, (Rectangle){bx + 3*(iconSz+gap) + 10, by, 240, bh});
                  if (hoverMic && clicked) {
                      gravMicMuted = !gravMicMuted;
                      grav_set_mic_mute(gravMicMuted);
                  }
                  if (hoverTela && clicked && gravTipo == 2) {
                      if (gravState == 2) { grav_pause_recording(); gravState = 3; }
                      else { grav_resume_recording(); gravLastGrowthTime = GetTime(); gravState = 2; }
                  }
                  if (gravState == 2) {
                      if (hoverPausa && clicked) { grav_pause_recording(); gravState = 3; }
                      if (hoverParar && clicked) {
                          grav_stop_recording();
                          gravState = 5;
                          gravNomeInput[0] = 0;
                      }
                       double now = GetTime();
                       if (gravState == 2 && now - gravLastSizeTime > 1.0) {
                           gravLastSizeTime = now;
                           long sz = grav_temp_file_size(GRAV_TELA_TEMP);
                          if (sz > gravLastSize) gravLastGrowthTime = now;
                          if (!grav_is_recording()) {
                              grav_stop_recording();
                              snprintf(gravErrorMsg, sizeof(gravErrorMsg), "Gravacao de tela falhou. Tente novamente.");
                              gravErrorTimer = GetTime();
                              gravState = 4;
                          } else if (now - gravLastGrowthTime > 15.0) {
                              grav_stop_recording();
                              snprintf(gravErrorMsg, sizeof(gravErrorMsg), "Gravacao de tela nao esta gravando.");
                              gravErrorTimer = GetTime();
                              gravState = 4;
                          }
                          gravLastSize = sz;
                      }
                  } else {
                      if (hoverPausa && clicked) { grav_resume_recording(); gravLastGrowthTime = GetTime(); gravState = 2; }
                      if (hoverParar && clicked) {
                          grav_stop_recording();
                          gravState = 5;
                          gravNomeInput[0] = 0;
                      }
                  }
                 if (IsKeyPressed(KEY_ESCAPE)) {
                     grav_stop_recording();
                     remove(GRAV_TELA_TEMP);
                     gravState = 0;
                     toolMode = TOOL_MENU;
                 }
             }
              else if (gravState == 5) {
                  int ch = GetCharPressed();
                  while (ch > 0) {
                      if (ch >= 32 && ch <= 255 && strlen(gravNomeInput) < 100) {
                          int l = strlen(gravNomeInput);
                          gravNomeInput[l] = (char)ch;
                          gravNomeInput[l+1] = 0;
                      }
                      ch = GetCharPressed();
                  }
                   if (IsKeyPressed(KEY_BACKSPACE) && strlen(gravNomeInput) > 0)
                       gravNomeInput[strlen(gravNomeInput)-1] = 0;
                   bool hoverSalvar = CheckCollisionPointRec(mousePos, (Rectangle){bx, by + bh + 20, 160, 45});
                   if (IsKeyPressed(KEY_ENTER) || (hoverSalvar && clicked)) {
                       gravSalvarAtual();
                   }
                   if (IsKeyPressed(KEY_ESCAPE)) {
                       remove(GRAV_TELA_TEMP);
                       gravState = 0;
                   }
             }
         }
           else if (gravTipo == 3) {
               if (gravState == 0 && !gravShowMenu) {
                   if (!printPreviewOn && !grav_video_preview_active()) {
                       if (grav_print_preview_start(printFonte)) printPreviewOn = true;
                       else {
                           snprintf(gravErrorMsg, sizeof(gravErrorMsg), "%s", grav_last_error_msg());
                           if (strlen(gravErrorMsg) == 0) snprintf(gravErrorMsg, sizeof(gravErrorMsg), "Erro ao iniciar preview.");
                           gravErrorTimer = GetTime();
                           gravState = 4;
                       }
                   }
                   int fx = 190, fy = 700, fw = 250, fh = 60;
                   bool hCam = CheckCollisionPointRec(mousePos, (Rectangle){fx, fy, fw, fh});
                   bool hTela = CheckCollisionPointRec(mousePos, (Rectangle){fx + fw + 20, fy, fw, fh});
                   if (hCam && clicked && printFonte != 0) {
                       printFonte = 0;
                       grav_print_preview_stop();
                       printPreviewOn = false;
                       if (grav_print_preview_start(0)) printPreviewOn = true;
                   }
                   if (hTela && clicked && printFonte != 1) {
                       printFonte = 1;
                       grav_print_preview_stop();
                       printPreviewOn = false;
                       if (grav_print_preview_start(1)) printPreviewOn = true;
                   }
                   if (printFonte == 1) {
                       int my2 = fy + fh + 60;
                       bool hInt = CheckCollisionPointRec(mousePos, (Rectangle){fx, my2, fw, fh});
                       bool hJan = CheckCollisionPointRec(mousePos, (Rectangle){fx + fw + 20, my2, fw, fh});
                       if (hInt && clicked) printModo = 0;
                       if (hJan && clicked) printModo = 1;
                   }
                   int capY = (printFonte == 1) ? fy + fh + 60 + fh + 30 : fy + fh + 50;
                   int cbW = 360, cbH = 70;
                   bool hCap = CheckCollisionPointRec(mousePos, (Rectangle){fx, capY, cbW, cbH});
                   if (hCap && clicked) {
                       time_t t = time(NULL);
                       struct tm* tmv = localtime(&t);
                       char fname[300];
                       snprintf(fname, sizeof(fname), "%sfoto_%04d%02d%02d_%02d%02d%02d.png", GRAV_DIR,
                           tmv->tm_year + 1900, tmv->tm_mon + 1, tmv->tm_mday, tmv->tm_hour, tmv->tm_min, tmv->tm_sec);
                       bool ok = (printFonte == 1) ? grav_capture_screen_to_png(fname, printModo == 1)
                                                   : grav_capture_camera_to_png(fname);
                       if (ok) {
                           if (gravTotal < MAX_GRAVACOES) strcpy(gravList[gravTotal++], fname);
                       } else {
                           snprintf(gravErrorMsg, sizeof(gravErrorMsg), "Nao foi possivel capturar a imagem. Verifique camera/fonte.");
                           gravErrorTimer = GetTime();
                           gravState = 4;
                       }
                   }
               }
               if (gravState == 4) {
                   if (clicked || IsKeyPressed(KEY_ESCAPE)) {
                       gravState = 0;
                       if (printPreviewOn || grav_video_preview_active()) { grav_print_preview_stop(); printPreviewOn = false; }
                   }
               }
               if (gravShowMenu || gravState != 0) {
                   if (printPreviewOn || grav_video_preview_active()) { grav_print_preview_stop(); printPreviewOn = false; }
               }
           }

         if (gravState == 0 && !gravShowMenu) {
             int listX = 1080, listY = 170, rowH = 50, listW = 720;
             int visRows = 15;
             int maxScroll = gravTotal > visRows ? gravTotal - visRows : 0;
             if (gravScroll > maxScroll) gravScroll = maxScroll;
             if (gravScroll < 0) gravScroll = 0;
             if (wheelMove != 0 && CheckCollisionPointRec(mousePos, (Rectangle){listX - 10, listY - 35, listW + 20, 800})) {
                 gravScroll -= (int)wheelMove;
                 if (gravScroll > maxScroll) gravScroll = maxScroll;
                 if (gravScroll < 0) gravScroll = 0;
             }
              for (int i = 0; i < gravTotal; i++) {
                  int ry = listY + i * rowH - gravScroll * rowH;
                  if (ry + 44 < listY - 35 || ry > listY - 35 + 800) continue;
                  int delX = listX + listW - 100, delW = 80, delH = 40;
                  int playX = delX - 90, playW = 80, playH = 40;
                  bool hPlay = CheckCollisionPointRec(mousePos, (Rectangle){playX, ry + 2, playW, playH});
                  bool hDel = CheckCollisionPointRec(mousePos, (Rectangle){delX, ry + 2, delW, delH});
                  if (hDel && clicked) {
                      gravDeleteIdx = i;
                  }
                   if (hPlay && clicked && gravDeleteIdx < 0) {
                       if (printPreviewOn || grav_video_preview_active()) { grav_print_preview_stop(); printPreviewOn = false; }
                       const char* ext = strrchr(gravList[i], '.');
                       if (ext && (strcmp(ext, ".mp4") == 0 || strcmp(ext, ".avi") == 0 ||
                                  strcmp(ext, ".mkv") == 0 || strcmp(ext, ".mov") == 0 || strcmp(ext, ".webm") == 0)) {
                           abrirVideoNoApp(gravList[i]);
                       } else if (ext && (strcmp(ext, ".png") == 0 || strcmp(ext, ".jpg") == 0 ||
                                          strcmp(ext, ".jpeg") == 0 || strcmp(ext, ".bmp") == 0)) {
                           abrirImagemNoApp(gravList[i]);
                       } else {
                           tocarMusicaNova(gravList[i]);
                       }
                   }
              }
              if (gravDeleteIdx >= 0) {
                  int dw = 560, dh = 220;
                  int dx = 1920/2 - dw/2, dy = 1080/2 - dh/2;
                  bool hSim = CheckCollisionPointRec(mousePos, (Rectangle){dx + 40, dy + dh - 80, 200, 55});
                  bool hNao = CheckCollisionPointRec(mousePos, (Rectangle){dx + 320, dy + dh - 80, 200, 55});
                  if (IsKeyPressed(KEY_ESCAPE)) { gravDeleteIdx = -1; }
                  if (hSim && clicked) {
                      remove(gravList[gravDeleteIdx]);
                      for (int k = gravDeleteIdx; k < gravTotal - 1; k++) strcpy(gravList[k], gravList[k+1]);
                      gravTotal--;
                      gravDeleteIdx = -1;
                  } else if (hNao && clicked) {
                      gravDeleteIdx = -1;
                  }
              }
          }

          if (gravState == 4) {
              if (GetTime() - gravErrorTimer > 5.0) { gravState = 0; }
              if (clicked || IsKeyPressed(KEY_ESCAPE)) { gravState = 0; }
          }

          if (!gravShowMenu && gravState == 0 && IsKeyPressed(KEY_ESCAPE)) {
              if (printPreviewOn || grav_video_preview_active()) { grav_print_preview_stop(); printPreviewOn = false; }
              toolMode = TOOL_MENU;
          }
     }

    else if (toolMode == TOOL_CONTATOS) {
        // ============ Gerenciador de catalogo (tags/campos) ============
        if (gerenciaModo != 0) {
            if (IsKeyPressed(KEY_ESCAPE)) { gerenciaModo = 0; gerEditando = false; gerEditIdx = -1; gerNome[0] = 0; }
            bool isTag = (gerenciaModo == 1);
            int* tot = isTag ? &tagTotal : &campoTotal;
            int n = *tot;
            if (n == 0) gerSel = 0;
            if (IsKeyPressed(KEY_UP)) gerSel = (gerSel - 1 + (n > 0 ? n : 1)) % (n > 0 ? n : 1);
            if (IsKeyPressed(KEY_DOWN)) gerSel = (gerSel + 1) % (n > 0 ? n : 1);
            int ly = 200, rh = 50, gap = 10;
            if (gerEditando) {
                int ch = GetCharPressed();
                while (ch > 0) { if (ch >= 32 && ch <= 255 && strlen(gerNome) < 63) { int l = strlen(gerNome); gerNome[l] = (char)ch; gerNome[l + 1] = 0; } ch = GetCharPressed(); }
                if (IsKeyPressed(KEY_BACKSPACE) && strlen(gerNome) > 0) gerNome[strlen(gerNome) - 1] = 0;
                if (IsKeyPressed(KEY_ENTER)) {
                    if (gerEditIdx >= 0 && gerEditIdx < n && gerNome[0]) {
                        char* nm = isTag ? tagCatalog[gerEditIdx] : campoCatalog[gerEditIdx];
                        if (isTag) ferramentas_tag_renomear(nm, gerNome); else ferramentas_campo_renomear(nm, gerNome);
                        strcpy(nm, gerNome);
                        if (isTag) ferramentas_salvar_tags(); else ferramentas_salvar_campos();
                    }
                    gerEditando = false; gerEditIdx = -1; gerNome[0] = 0;
                }
            } else {
                for (int i = 0; i < n; i++) {
                    int ry = ly + i * (rh + gap);
                    int rbX = 1100, reX = 1320, rdX = 1540, bw = 180;
                    bool hb = (mousePos.x >= rbX && mousePos.x <= rbX + bw && mousePos.y >= ry && mousePos.y <= ry + rh);
                    bool he = (mousePos.x >= reX && mousePos.x <= reX + bw && mousePos.y >= ry && mousePos.y <= ry + rh);
                    bool hd = (mousePos.x >= rdX && mousePos.x <= rdX + bw && mousePos.y >= ry && mousePos.y <= ry + rh);
                    if (hb && clicked) { gerEditIdx = i; strcpy(gerNome, isTag ? tagCatalog[i] : campoCatalog[i]); gerEditando = true; }
                    if (he && clicked) {
                        char* nm = isTag ? tagCatalog[i] : campoCatalog[i];
                        if (isTag) ferramentas_tag_excluir(nm); else ferramentas_campo_excluir(nm);
                        for (int j = i; j < n - 1; j++) strcpy(isTag ? tagCatalog[j] : campoCatalog[j], isTag ? tagCatalog[j + 1] : campoCatalog[j + 1]);
                        (*tot)--;
                        if (isTag) ferramentas_salvar_tags(); else ferramentas_salvar_campos();
                        if (gerSel >= *tot) gerSel = *tot - 1;
                    }
                }
                int nvX = 1100, nvY = ly + n * (rh + gap) + 10, nvW = 180, nvH = rh;
                bool hnv = (mousePos.x >= nvX && mousePos.x <= nvX + nvW && mousePos.y >= nvY && mousePos.y <= nvY + nvH);
                if (hnv && clicked && *tot < MAX_CAT) {
                    char* nm = isTag ? tagCatalog[*tot] : campoCatalog[*tot];
                    strcpy(nm, "Novo"); (*tot)++;
                    if (isTag) ferramentas_salvar_tags(); else ferramentas_salvar_campos();
                    gerEditIdx = *tot - 1; strcpy(gerNome, nm); gerEditando = true;
                }
            }
            return;
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (contatoEditando) contatoEditando = false;
            else { toolMode = TOOL_MENU; }
        }
        if (!contatoEditando) {
            int nVis = ferramentas_contatos_ordem(contatoOrd);
            if (contatoSel >= nVis) contatoSel = nVis - 1;
            if (contatoSel < 0 && nVis > 0) contatoSel = 0;

            // Sort buttons
            const char* sorts[4] = { "Ordenar: Alfabetico", "Ordenar: Data", "Ordenar: Tag", "Ordenar: Campo" };
            int sbX = 100, sbY = 200, sbW = 300, sbH = 45;
            for (int si = 0; si < 4; si++) {
                int bx = sbX + si * (sbW + 15);
                bool h = (mousePos.x >= bx && mousePos.x <= bx + sbW && mousePos.y >= sbY && mousePos.y <= sbY + sbH);
                if (h && clicked) { contatoSort = si; ferramentas_contatos_ordem(contatoOrd); }
            }
            // Tag filter input
            int tfX = 100, tfY = 260, tfW = 615, tfH = 40;
            bool htf = (mousePos.x >= tfX && mousePos.x <= tfX + tfW && mousePos.y >= tfY && mousePos.y <= tfY + tfH);
            if (htf && clicked) contatoFiltroEdit = true;
            else if (clicked) contatoFiltroEdit = false;
            if (contatoFiltroEdit) {
                int ch = GetCharPressed();
                while (ch > 0) { if (ch >= 32 && ch <= 255 && strlen(contatoFiltroTag) < 63) { int l = strlen(contatoFiltroTag); contatoFiltroTag[l] = (char)ch; contatoFiltroTag[l + 1] = 0; } ch = GetCharPressed(); }
                if (IsKeyPressed(KEY_BACKSPACE) && strlen(contatoFiltroTag) > 0) contatoFiltroTag[strlen(contatoFiltroTag) - 1] = 0;
                if (IsKeyPressed(KEY_ENTER)) contatoFiltroEdit = false;
            }
            // Novo button
            int nvX = 1700, nvY = 200, nvW = 200, nvH = 45;
            bool hnv = (mousePos.x >= nvX && mousePos.x <= nvX + nvW && mousePos.y >= nvY && mousePos.y <= nvY + nvH);
            if (hnv && clicked && contatosTotal < MAX_CONTATOS) {
                contatoEditIndex = -1;
                memset(&contatoBuf, 0, sizeof(contatoBuf));
                strcpy(contatoBuf.nome, "Novo Contato");
                contatoBuf.adicionado = (time_t)time(NULL);
                contatoField = 0; contatoEditando = true;
            }
            // List
            if (wheelMove != 0) { contatoScroll -= (int)(wheelMove * 2); if (contatoScroll < 0) contatoScroll = 0; }
            int listX = 100, listY = 320, rowH = 60;
            for (int vi = 0; vi < nVis; vi++) {
                int ry = listY + vi * rowH - contatoScroll;
                if (ry < listY - rowH || ry > 1080) continue;
                bool h = (mousePos.x >= listX && mousePos.x <= listX + 1720 && mousePos.y >= ry && mousePos.y <= ry + rowH - 6);
                if (h && clicked) {
                    contatoSel = vi;
                    contatoEditIndex = contatoOrd[vi];
                    contatoBuf = contatos[contatoEditIndex];
                    contatoField = 0; contatoEditando = true;
                }
            }
            if (IsKeyPressed(KEY_UP) && nVis > 0) { contatoSel = (contatoSel - 1 + nVis) % nVis; contatoScroll = contatoSel * rowH - 500; if (contatoScroll < 0) contatoScroll = 0; }
            if (IsKeyPressed(KEY_DOWN) && nVis > 0) { contatoSel = (contatoSel + 1) % nVis; contatoScroll = contatoSel * rowH - 500; if (contatoScroll < 0) contatoScroll = 0; }
            if (IsKeyPressed(KEY_ENTER) && nVis > 0) {
                contatoEditIndex = contatoOrd[contatoSel];
                contatoBuf = contatos[contatoEditIndex];
                contatoField = 0; contatoEditando = true;
            }
        }
        else {
            int nCampos = campoTotal;
            int maxF = 3 + nCampos;
            if (IsKeyPressed(KEY_TAB)) contatoField = (contatoField + 1) % (maxF > 0 ? maxF : 1);
            if (IsKeyPressed(KEY_DOWN)) contatoField = (contatoField + 1) % (maxF > 0 ? maxF : 1);
            if (IsKeyPressed(KEY_UP)) contatoField = (contatoField + (maxF > 0 ? maxF : 1) - 1) % (maxF > 0 ? maxF : 1);
            if (wheelMove != 0) { contatoEditScroll -= (int)(wheelMove * 2); if (contatoEditScroll < 0) contatoEditScroll = 0; }

            int yy = 170 - contatoEditScroll;
            int bh = 46, gp = 8;
            for (int i = 0; i < 3; i++) {
                if (mousePos.x >= 100 && mousePos.x <= 1820 && mousePos.y >= yy && mousePos.y <= yy + bh) { if (clicked) contatoField = i; }
                yy += bh + gp;
            }
            yy += 14; yy += 30;
            for (int i = 0; i < tagTotal; i++) {
                if (mousePos.x >= 100 && mousePos.x <= 1820 && mousePos.y >= yy && mousePos.y <= yy + bh) {
                    if (clicked) {
                        if (contato_tag_tem(&contatoBuf, tagCatalog[i])) contato_tag_del(&contatoBuf, tagCatalog[i]);
                        else contato_tag_add(&contatoBuf, tagCatalog[i]);
                    }
                }
                yy += bh + gp;
            }
            yy += 14; yy += 30;
            for (int i = 0; i < nCampos; i++) {
                if (mousePos.x >= 360 && mousePos.x <= 1820 && mousePos.y >= yy && mousePos.y <= yy + bh) { if (clicked) contatoField = 3 + i; }
                yy += bh + gp;
            }

            char* cur = NULL; int cap = 0;
            if (contatoField == 0) { cur = contatoBuf.nome; cap = 127; }
            else if (contatoField == 1) { cur = contatoBuf.telefone; cap = 63; }
            else if (contatoField == 2) { cur = contatoBuf.emails; cap = 255; }
            else if (contatoField >= 3 && contatoField < 3 + nCampos) {
                int cf = contatoField - 3;
                int s = contato_slot_label(&contatoBuf, campoCatalog[cf]);
                if (s < 0) { for (int k = 0; k < MAX_CAMPC; k++) if (!contatoBuf.cl[k][0]) { s = k; break; } if (s >= 0) { strcpy(contatoBuf.cl[s], campoCatalog[cf]); contatoBuf.cv[s][0] = 0; } }
                if (s >= 0) { cur = contatoBuf.cv[s]; cap = 127; }
            }
            if (cur) {
                int ch = GetCharPressed();
                while (ch > 0) { if (ch >= 32 && ch <= 255 && strlen(cur) < cap) { int l = strlen(cur); cur[l] = (char)ch; cur[l + 1] = 0; } ch = GetCharPressed(); }
                if (IsKeyPressed(KEY_BACKSPACE) && strlen(cur) > 0) cur[strlen(cur) - 1] = 0;
            }

            int gtx = 1180, gty = 80, gtw = 200, gth = 45;
            if (mousePos.x >= gtx && mousePos.x <= gtx + gtw && mousePos.y >= gty && mousePos.y <= gty + gth) { if (clicked) gerenciaModo = 1; }
            int gcx = 1400, gcy = 80, gcw = 200, gch = 45;
            if (mousePos.x >= gcx && mousePos.x <= gcx + gcw && mousePos.y >= gcy && mousePos.y <= gcy + gch) { if (clicked) gerenciaModo = 2; }

            int svX = 1300, svY = 1000, svW = 200, svH = 50;
            if (mousePos.x >= svX && mousePos.x <= svX + svW && mousePos.y >= svY && mousePos.y <= svY + svH) {
                if (clicked) {
                    ContactItem* c;
                    if (contatoEditIndex == -1) { if (contatosTotal < MAX_CONTATOS) { c = &contatos[contatosTotal]; contatosTotal++; } else c = &contatos[0]; }
                    else c = &contatos[contatoEditIndex];
                    *c = contatoBuf;
                    ferramentas_salvar_contatos(); contatoEditando = false;
                }
            }
            int cxX = 1520, cxY = 1000, cxW = 200, cxH = 50;
            if (mousePos.x >= cxX && mousePos.x <= cxX + cxW && mousePos.y >= cxY && mousePos.y <= cxY + cxH) { if (clicked) contatoEditando = false; }
            int dlX = 100, dlY = 1000, dlW = 200, dlH = 50;
            if (mousePos.x >= dlX && mousePos.x <= dlX + dlW && mousePos.y >= dlY && mousePos.y <= dlY + dlH) {
                if (clicked && contatoEditIndex >= 0 && contatoEditIndex < contatosTotal) {
                    for (int i = contatoEditIndex; i < contatosTotal - 1; i++) contatos[i] = contatos[i + 1];
                    contatosTotal--; ferramentas_salvar_contatos(); contatoEditando = false;
                }
            }
        }
    }
    else if (toolMode == TOOL_CAIXA) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (msgCompondo) msgCompondo = false;
            else if (msgViewIndex >= 0) msgViewIndex = -1;
            else toolMode = TOOL_MENU;
        }
        if (msgViewIndex >= 0) {
            if (msgViewIndex < msgsTotal && !msgs[msgViewIndex].lido) { msgs[msgViewIndex].lido = true; ferramentas_salvar_caixa(); }
            int bX = 100, bY = 1000, bW = 200, bH = 50;
            bool hb = (mousePos.x >= bX && mousePos.x <= bX + bW && mousePos.y >= bY && mousePos.y <= bY + bH);
            if (hb && clicked) msgViewIndex = -1;
        }
        else if (!msgCompondo) {
            const char* tipos[5] = { "Todos", "E-mail", "WhatsApp", "SMS", "Telegram" };
            int fbX = 100, fbY = 200, fbW = 200, fbH = 45;
            for (int ti = 0; ti < 5; ti++) {
                int bx = fbX + ti * (fbW + 15);
                bool h = (mousePos.x >= bx && mousePos.x <= bx + fbW && mousePos.y >= fbY && mousePos.y <= fbY + fbH);
                if (h && clicked) { msgFiltroTipo = ti - 1; msgScroll = 0; msgSel = -1; }
            }
            int nvX = 1700, nvY = 200, nvW = 200, nvH = 45;
            bool hnv = (mousePos.x >= nvX && mousePos.x <= nvX + nvW && mousePos.y >= nvY && mousePos.y <= nvY + nvH);
            if (hnv && clicked && msgsTotal < MAX_MSG) {
                mDe[0] = 0; strcpy(mDe, "Eu"); mAss[0] = 0; mTxt[0] = 0; msgTipoSel = 0;
                msgEditIndex = -1; msgField = 0; msgCompondo = true;
            }
            int nVis = 0;
            int visIdx[500];
            for (int i = 0; i < msgsTotal; i++) { if (msgFiltroTipo != -1 && msgs[i].tipo != msgFiltroTipo) continue; visIdx[nVis++] = i; }
            if (msgSel >= nVis) msgSel = nVis - 1;
            if (msgSel < 0 && nVis > 0) msgSel = 0;
            if (wheelMove != 0) { msgScroll -= (int)(wheelMove * 2); if (msgScroll < 0) msgScroll = 0; }
            int listX = 100, listY = 320, rowH = 60;
            for (int vi = 0; vi < nVis; vi++) {
                int i = visIdx[vi];
                int ry = listY + vi * rowH - msgScroll;
                if (ry < listY - rowH || ry > 1080) continue;
                bool h = (mousePos.x >= listX && mousePos.x <= listX + 1720 && mousePos.y >= ry && mousePos.y <= ry + rowH - 6);
                if (h && clicked) { msgViewIndex = i; }
            }
            if (IsKeyPressed(KEY_UP) && nVis > 0) { msgSel = (msgSel - 1 + nVis) % nVis; msgScroll = msgSel * rowH - 500; if (msgScroll < 0) msgScroll = 0; }
            if (IsKeyPressed(KEY_DOWN) && nVis > 0) { msgSel = (msgSel + 1) % nVis; msgScroll = msgSel * rowH - 500; if (msgScroll < 0) msgScroll = 0; }
            if (IsKeyPressed(KEY_ENTER) && nVis > 0) msgViewIndex = visIdx[msgSel];
        }
        else {
            if (IsKeyPressed(KEY_TAB)) msgField = (msgField + 1) % 4;
            if (IsKeyPressed(KEY_DOWN)) msgField = (msgField + 1) % 4;
            if (IsKeyPressed(KEY_UP)) msgField = (msgField + 3) % 4;
            // Selecionar campo com o mouse
            {
                int fy = 200;
                int rys[4] = { fy, fy + 70, fy + 140, fy + (fy + 3 * 70) };
                int hhs[4] = { 62, 62, 62, 260 };
                for (int i = 0; i < 4; i++) {
                    if (mousePos.x >= 100 && mousePos.x <= 1820 && mousePos.y >= rys[i] && mousePos.y <= rys[i] + hhs[i] - 8) {
                        if (clicked) msgField = i;
                    }
                }
            }
            if (msgField == 0) {
                if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) msgTipoSel = (msgTipoSel + 1) % 4;
            }
            else {
                char* cur = NULL; int cap = 0;
                if (msgField == 1) { cur = mDe; cap = 127; }
                else if (msgField == 2) { cur = mAss; cap = 127; }
                else if (msgField == 3) { cur = mTxt; cap = 599; }
                if (cur) {
                    int ch = GetCharPressed();
                    while (ch > 0) { if (ch >= 32 && ch <= 255 && strlen(cur) < cap) { int l = strlen(cur); cur[l] = (char)ch; cur[l + 1] = 0; } ch = GetCharPressed(); }
                    if (IsKeyPressed(KEY_BACKSPACE) && strlen(cur) > 0) cur[strlen(cur) - 1] = 0;
                }
            }
            int svX = 1300, svY = 1000, svW = 200, svH = 50;
            bool hsv = (mousePos.x >= svX && mousePos.x <= svX + svW && mousePos.y >= svY && mousePos.y <= svY + svH);
            if (hsv && clicked) {
                MsgItem* m;
                if (msgEditIndex == -1) { if (msgsTotal < MAX_MSG) { m = &msgs[msgsTotal]; msgsTotal++; } else m = &msgs[0]; }
                else m = &msgs[msgEditIndex];
                m->tipo = msgTipoSel; strcpy(m->de, mDe); strcpy(m->assunto, mAss); strcpy(m->texto, mTxt);
                m->recebido = (time_t)time(NULL); m->lido = true;
                ferramentas_salvar_caixa(); msgCompondo = false;
            }
            int cxX = 1520, cxY = 1000, cxW = 200, cxH = 50;
            bool hcx = (mousePos.x >= cxX && mousePos.x <= cxX + cxW && mousePos.y >= cxY && mousePos.y <= cxY + cxH);
            if (hcx && clicked) msgCompondo = false;
        }
    }
    else if (toolMode == TOOL_TERMINAL) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (termOptOpen) termOptOpen = false;
            else toolMode = TOOL_MENU;
        }
        // Painel de opcoes (comandos rapidos)
        int opX = 1300, opY = 130, opW = 600, opH = 400;
        bool hop = (mousePos.x >= 1180 && mousePos.x <= 1380 && mousePos.y >= 80 && mousePos.y <= 125);
        if (hop && clicked) termOptOpen = !termOptOpen;
        if (termOptOpen) {
            const char* rapidos[8] = { "opencode", "help", "cls", "echo Ola", "date", "ver", "dir", "clear" };
            for (int i = 0; i < 8; i++) {
                int bx = opX, by = opY + 20 + i * 46, bw = opW, bh = 40;
                bool h = (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
                if (h && clicked) {
                    if (i == 0) { strcpy(termInput, "opencode"); }
                    else { strcpy(termInput, rapidos[i]); }
                    termCaret = 0;
                    ferramentas_term_run(termInput);
                    if (termInput[0]) {
                        if (termHistTotal < MAX_TERM_LINES) { strncpy(termHist[termHistTotal], termInput, MAX_TERM_CMD - 1); termHist[termHistTotal][MAX_TERM_CMD - 1] = 0; termHistTotal++; }
                        termHistSel = termHistTotal;
                    }
                    termInput[0] = 0;
                    termOptOpen = false;
                }
            }
        }
        // Entrada de texto
        int inX = 100, inY = 1000, inW = 1720, inH = 50;
        bool hin = (mousePos.x >= inX && mousePos.x <= inX + inW && mousePos.y >= inY && mousePos.y <= inY + inH);
        int ch = GetCharPressed();
        // backspace funciona mesmo sem foco (terminal sempre ativo)
        while (ch > 0) {
            if (ch >= 32 && ch <= 255 && strlen(termInput) < MAX_TERM_CMD - 1) { int l = strlen(termInput); termInput[l] = (char)ch; termInput[l + 1] = 0; }
            ch = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && strlen(termInput) > 0) termInput[strlen(termInput) - 1] = 0;
        if (IsKeyPressed(KEY_ENTER)) {
            if (termInput[0]) {
                ferramentas_term_run(termInput);
                if (termHistTotal < MAX_TERM_LINES) { strncpy(termHist[termHistTotal], termInput, MAX_TERM_CMD - 1); termHist[termHistTotal][MAX_TERM_CMD - 1] = 0; termHistTotal++; }
                termHistSel = termHistTotal;
                termInput[0] = 0;
            }
        }
        if (IsKeyPressed(KEY_UP)) {
            if (termHistTotal > 0) { termHistSel--; if (termHistSel < 0) termHistSel = 0; strcpy(termInput, termHist[termHistSel]); }
        }
        if (IsKeyPressed(KEY_DOWN)) {
            if (termHistSel < termHistTotal - 1) { termHistSel++; strcpy(termInput, termHist[termHistSel]); }
            else { termHistSel = termHistTotal; termInput[0] = 0; }
        }
        if (wheelMove != 0) { termScroll -= (int)(wheelMove * 4); if (termScroll < 0) termScroll = 0; if (termScroll > termLinesTotal) termScroll = termLinesTotal; }
    }
    else if (toolMode == TOOL_IA) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (iaMicGravando) { iaMicGravando = false; grav_stop_recording(); }
            toolMode = TOOL_MENU;
        }
        ia_poll();

        // Abas
        for (int t = 0; t < 5; t++) {
            int tx = 60 + t * 370, ty = 120, tw = 350, th = 62;
            bool ht = (mousePos.x >= tx && mousePos.x <= tx + tw && mousePos.y >= ty && mousePos.y <= ty + th);
            if (ht && clicked) iaTab = t;
        }
        if (iaTab != iaLastTab) {
            if (iaMicGravando) { iaMicGravando = false; grav_stop_recording(); }
            iaLastTab = iaTab;
            iaTemResposta = false; iaTemErro = false; iaErro[0] = 0; iaScroll = 0; iaMostrarImagem = false;
            if (iaResTexLoaded) { UnloadTexture(iaResTex); iaResTexLoaded = false; }
            if (iaTab == 1) ia_refresh_imagem_list();
            if (iaTab == 2) ia_refresh_imagem_list();
            if (iaTab == 3) ia_refresh_audio_list();
        }

        // Entrada de texto do prompt (abas 0,1,2 usam iaPrompt; aba 4 usa iaPrompt2)
        if (iaTab != 3) {
            int ch = GetCharPressed();
            while (ch > 0) {
                if (ch >= 32 && ch <= 255) {
                    if (iaTab == 4 && iaPrompt2Len < (int)sizeof(iaPrompt2) - 1) {
                        iaPrompt2[iaPrompt2Len] = (char)ch; iaPrompt2[iaPrompt2Len + 1] = 0; iaPrompt2Len++;
                    } else if (iaTab != 4 && iaPromptLen < (int)sizeof(iaPrompt) - 1) {
                        iaPrompt[iaPromptLen] = (char)ch; iaPrompt[iaPromptLen + 1] = 0; iaPromptLen++;
                    }
                }
                ch = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE)) {
                if (iaTab == 4 && iaPrompt2Len > 0) { iaPrompt2Len--; iaPrompt2[iaPrompt2Len] = 0; }
                else if (iaTab != 4 && iaPromptLen > 0) { iaPromptLen--; iaPrompt[iaPromptLen] = 0; }
            }
        }

        // Tab 0: Texto
        if (iaTab == 0) {
            for (int i = 0; i < 3; i++) {
                int bx = 110 + i * 320, by = 220, bw = 300, bh = 44;
                bool h = (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
                if (h && clicked) iaModeloIdx = i;
            }
            bool hg = (mousePos.x >= 60 && mousePos.x <= 360 && mousePos.y >= 440 && mousePos.y <= 496);
            if ((hg && clicked) || IsKeyPressed(KEY_ENTER)) {
                if (!iaOcupado && iaPromptLen > 0) {
                    iaOcupado = true; iaTemResposta = false; iaTemErro = false; iaErro[0] = 0; iaScroll = 0;
                    iaRespTipo = 0;
                    gemini_gerar_texto_async(iaPrompt, iaModelosTexto[iaModeloIdx], NULL, 0, NULL, &iaResp);
                }
            }
            if (wheelMove != 0 && mousePos.x >= 60 && mousePos.x <= 1860 && mousePos.y >= 530 && mousePos.y <= 1030) {
                iaScroll -= (int)(wheelMove * 4); if (iaScroll < 0) iaScroll = 0;
            }
        }
        // Tab 1: Visao
        else if (iaTab == 1) {
            for (int i = 0; i < 3; i++) {
                int bx = 110 + i * 320, by = 220, bw = 300, bh = 44;
                bool h = (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
                if (h && clicked) iaModeloIdx = i;
            }
            int listX = 60, listY = 280, listW = 680, rowH = 46, vis = 13;
            for (int i = 0; i < vis && (iaArquivoScroll + i) < iaImagemTotal; i++) {
                int idx = iaArquivoScroll + i;
                int ry = listY + i * rowH;
                bool h = (mousePos.x >= listX && mousePos.x <= listX + listW && mousePos.y >= ry && mousePos.y <= ry + rowH);
                if (h && clicked) iaImagemSel = idx;
            }
            if (wheelMove != 0 && mousePos.x >= listX && mousePos.x <= listX + listW && mousePos.y >= listY && mousePos.y <= listY + vis * rowH) {
                iaArquivoScroll -= (int)wheelMove; if (iaArquivoScroll < 0) iaArquivoScroll = 0;
                if (iaArquivoScroll > iaImagemTotal - vis) { iaArquivoScroll = iaImagemTotal > vis ? iaImagemTotal - vis : 0; }
            }
            bool hg = (mousePos.x >= 780 && mousePos.x <= 1080 && mousePos.y >= 470 && mousePos.y <= 526);
            if (hg && clicked) {
                if (!iaOcupado && iaPromptLen > 0 && iaImagemSel >= 0) {
                    const char* medias[1];
                    medias[0] = iaImagemList[iaImagemSel];
                    iaOcupado = true; iaTemResposta = false; iaTemErro = false; iaErro[0] = 0; iaScroll = 0;
                    iaRespTipo = 0;
                    gemini_gerar_texto_async(iaPrompt, iaModelosTexto[iaModeloIdx], medias, 1, NULL, &iaResp);
                }
            }
            if (wheelMove != 0 && mousePos.x >= 780 && mousePos.x <= 1860 && mousePos.y >= 550 && mousePos.y <= 1030) {
                iaScroll -= (int)(wheelMove * 4); if (iaScroll < 0) iaScroll = 0;
            }
        }
        // Tab 2: Gerar Imagem
        else if (iaTab == 2) {
            for (int i = 0; i < 1; i++) {
                int bx = 110 + i * 320, by = 220, bw = 300, bh = 44;
                bool h = (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
                if (h && clicked) iaImagemModelo = i;
            }
            for (int i = 0; i < 5; i++) {
                int bx = 110 + i * 240, by = 280, bw = 220, bh = 44;
                bool h = (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
                if (h && clicked) iaAspecto = i;
            }
            bool hg = (mousePos.x >= 60 && mousePos.x <= 360 && mousePos.y >= 480 && mousePos.y <= 536);
            if ((hg && clicked) || IsKeyPressed(KEY_ENTER)) {
                if (!iaOcupado && iaPromptLen > 0) {
                    iaOcupado = true; iaTemResposta = false; iaTemErro = false; iaErro[0] = 0; iaScroll = 0;
                    iaMostrarImagem = false;
                    if (iaResTexLoaded) { UnloadTexture(iaResTex); iaResTexLoaded = false; }
                    char outPng[512];
                    time_t tt = time(NULL);
                    struct tm* tmv = localtime(&tt);
                    snprintf(outPng, sizeof(outPng), "%sia_%04d%02d%02d_%02d%02d%02d.png", GRAV_DIR,
                        tmv->tm_year + 1900, tmv->tm_mon + 1, tmv->tm_mday, tmv->tm_hour, tmv->tm_min, tmv->tm_sec);
                    iaRespTipo = 1;
                    gemini_gerar_imagem_async(iaPrompt, iaModelosImagem[iaImagemModelo], iaAspectos[iaAspecto], outPng, &iaResp);
                }
            }
        }
        // Tab 3: Audio -> Texto
        else if (iaTab == 3) {
            for (int i = 0; i < 3; i++) {
                int bx = 110 + i * 320, by = 220, bw = 300, bh = 44;
                bool h = (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
                if (h && clicked) iaModeloIdx = i;
            }
            int listX = 60, listY = 290, listW = 1100, rowH = 46, vis = 15;
            for (int i = 0; i < vis && (iaArquivoScroll + i) < iaAudioTotal; i++) {
                int idx = iaArquivoScroll + i;
                int ry = listY + i * rowH;
                bool h = (mousePos.x >= listX && mousePos.x <= listX + listW && mousePos.y >= ry && mousePos.y <= ry + rowH);
                if (h && clicked) iaAudioSel = idx;
            }
            if (wheelMove != 0 && mousePos.x >= listX && mousePos.x <= listX + listW && mousePos.y >= listY && mousePos.y <= listY + vis * rowH) {
                iaArquivoScroll -= (int)wheelMove; if (iaArquivoScroll < 0) iaArquivoScroll = 0;
                if (iaArquivoScroll > iaAudioTotal - vis) { iaArquivoScroll = iaAudioTotal > vis ? iaAudioTotal - vis : 0; }
            }
            bool hg = (mousePos.x >= 1220 && mousePos.x <= 1560 && mousePos.y >= 210 && mousePos.y <= 266);
            if (hg && clicked) {
                if (!iaOcupado && iaAudioSel >= 0) {
                    iaOcupado = true; iaTemResposta = false; iaTemErro = false; iaErro[0] = 0; iaScroll = 0;
                    iaRespTipo = 2;
                    gemini_audio_para_texto_async(iaAudioList[iaAudioSel], iaModelosTexto[iaModeloIdx], &iaResp);
                }
            }
            bool hm = (mousePos.x >= 1580 && mousePos.x <= 1860 && mousePos.y >= 210 && mousePos.y <= 266);
            if (hm && clicked) {
                if (iaMicGravando) {
                    iaMicGravando = false;
                    grav_stop_recording();
                    if (iaMicRaw[0] && grav_finish_recording(iaMicRaw, iaMicWav)) {
                        iaOcupado = true; iaTemResposta = false; iaTemErro = false; iaErro[0] = 0; iaScroll = 0;
                        iaRespTipo = 2;
                        ia_refresh_audio_list();
                        if (iaAudioTotal > 0) iaAudioSel = iaAudioTotal - 1;
                        gemini_audio_para_texto_async(iaMicWav, iaModelosTexto[iaModeloIdx], &iaResp);
                    } else {
                        iaTemErro = true;
                        iaTemResposta = false;
                        const char* mlerr = grav_last_error_msg();
                        if (mlerr && mlerr[0]) snprintf(iaErro, sizeof(iaErro), "%s", mlerr);
                        else snprintf(iaErro, sizeof(iaErro), "Falha ao gravar audio (gravacao vazia). Tente falar mais perto do microfone.");
                    }
                } else if (!iaOcupado) {
                    time_t tt = time(NULL);
                    struct tm* tmv = localtime(&tt);
                    snprintf(iaMicRaw, sizeof(iaMicRaw), "%sgm_%04d%02d%02d_%02d%02d%02d.raw", GRAV_DIR,
                        tmv->tm_year + 1900, tmv->tm_mon + 1, tmv->tm_mday, tmv->tm_hour, tmv->tm_min, tmv->tm_sec);
                    snprintf(iaMicWav, sizeof(iaMicWav), "%sgm_%04d%02d%02d_%02d%02d%02d.wav", GRAV_DIR,
                        tmv->tm_year + 1900, tmv->tm_mon + 1, tmv->tm_mday, tmv->tm_hour, tmv->tm_min, tmv->tm_sec);
                    if (grav_start_recording(iaMicRaw)) {
                        iaMicGravando = true;
                        iaMicInicio = GetTime();
                        iaTemErro = false; iaTemResposta = false; iaErro[0] = 0;
                    } else {
                        iaTemErro = true;
                        iaTemResposta = false;
                        snprintf(iaErro, sizeof(iaErro), "%s", grav_last_error_msg());
                    }
                }
            }
            if (wheelMove != 0 && mousePos.x >= 1220 && mousePos.x <= 1860 && mousePos.y >= 290 && mousePos.y <= 1030) {
                iaScroll -= (int)(wheelMove * 4); if (iaScroll < 0) iaScroll = 0;
            }
        }
        // Tab 4: Texto -> Audio
        else if (iaTab == 4) {
            for (int i = 0; i < 6; i++) {
                int bx = 110 + (i % 3) * 320, by = 210 + (i / 3) * 60, bw = 300, bh = 44;
                bool h = (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
                if (h && clicked) iaVozIdx = i;
            }
            bool hg = (mousePos.x >= 60 && mousePos.x <= 420 && mousePos.y >= 460 && mousePos.y <= 516);
            if ((hg && clicked) || IsKeyPressed(KEY_ENTER)) {
                if (!iaOcupado && iaPrompt2Len > 0) {
                    iaOcupado = true; iaTemResposta = false; iaTemErro = false; iaErro[0] = 0; iaScroll = 0;
                    char outMp3[512];
                    time_t tt2 = time(NULL);
                    struct tm* tmv2 = localtime(&tt2);
                    snprintf(outMp3, sizeof(outMp3), "%svoz_%04d%02d%02d_%02d%02d%02d.wav", GRAV_DIR,
                        tmv2->tm_year + 1900, tmv2->tm_mon + 1, tmv2->tm_mday, tmv2->tm_hour, tmv2->tm_min, tmv2->tm_sec);
                    iaRespTipo = 3;
                    gemini_texto_para_audio_async(iaPrompt2, iaVozes[iaVozIdx], outMp3, &iaResp);
                }
            }
        }
    }

    // ==========================================
    // DRAWING EVERYTHING ONTO 1920x1080 CANVAS
    // ==========================================
    BeginTextureMode(toolsRenderTexture);
    ClearBackground(BLACK);

    desenharRetanguloAlpha(0, 0, 1920, 1080, 0xFF151515);

    if (toolMode == TOOL_MENU) {
        desenharTexto("FERRAMENTAS E UTILITARIOS", 45, 100, 100, 0xFF00AAFF);
        desenharTexto("Selecione um acessorio para comecar - Use as SETAS / ENTER ou clique", 22, 100, 160, 0xFFAAAAAA);
        desenharTexto("[ESC] para voltar ao Menu Extra", 20, 100, 200, 0xFFEAB308);

        // Voltar Button
        int vltX = 1600, vltY = 80, vltW = 220, vltH = 50;
        bool hoverVoltar = (mousePos.x >= vltX && mousePos.x <= vltX + vltW && mousePos.y >= vltY && mousePos.y <= vltY + vltH);
        if (hoverVoltar && clicked) {
            fecharFerramentas();
            voltarParaMenuExtra();
        }
        desenharBotaoVoltarIcone(vltX, vltY, vltW, vltH, hoverVoltar);

        // Four Card Buttons
        int cardW = 370;
        int cardH = 480;
        int cardY = 290;
        int cardG = 50;
        int startX = 150;

        for (int i = 0; i < 4; i++) {
            int cx = startX + i * (cardW + cardG);
            bool hovered = (mousePos.x >= cx && mousePos.x <= cx + cardW && mousePos.y >= cardY && mousePos.y <= cardY + cardH);
            bool selected = (menuSelection == i);

            if (hovered && clicked) {
                menuSelection = i;
                if (i == 0) toolMode = TOOL_CALCULADORA;
                else if (i == 1) toolMode = TOOL_NOTEPAD;
                else if (i == 2) toolMode = TOOL_TIMER;
                else if (i == 3) toolMode = TOOL_GRAVADOR;
            }

            uint32_t cardColor = selected ? 0x6600AAFF : (hovered ? 0x44FFFFFF : 0x33FFFFFF);
            uint32_t borderColor = selected ? 0xFF00FFFF : (hovered ? 0xFF88FFFF : 0xFF00CCFF);

            desenharRetanguloAlpha(cx, cardY, cardW, cardH, cardColor);
            DrawRectangleLines(cx, cardY, cardW, cardH, (Color){ (borderColor >> 16) & 0xFF, (borderColor >> 8) & 0xFF, borderColor & 0xFF, 255 });

            if (i == 0) {
                desenharTexto("CALCULADORA", 32, cx + 50, cardY + 50, selected ? 0xFF00AAFF : 0xFFFFFFFF);
                desenharTexto("Aritmetica e Digitos", 20, cx + 50, cardY + 100, 0xFF888888);
                // Draw miniature calculator graphic
                int cGridX = cx + 80; int cGridY = cardY + 180;
                for (int r = 0; r < 4; r++) {
                    for (int c = 0; c < 4; c++) {
                        desenharRetanguloAlpha(cGridX + c * 75, cGridY + r * 60, 65, 50, 0x44FFFFFF);
                        DrawRectangleLines(cGridX + c * 75, cGridY + r * 60, 65, 50, GRAY);
                    }
                }
            } 
            else if (i == 1) {
                desenharTexto("BLOCO DE NOTAS", 32, cx + 50, cardY + 50, selected ? 0xFF00AAFF : 0xFFFFFFFF);
                desenharTexto("Editor de Notas Simples", 20, cx + 50, cardY + 100, 0xFF888888);
                // Draw notepad lines
                int lX = cx + 60, lY = cardY + 180;
                for (int l = 0; l < 6; l++) {
                    desenharRetanguloAlpha(lX, lY + l * 40, cardW - 120, 4, 0x33FFFFFF);
                }
            } 
             else if (i == 2) {
                 desenharTexto("RELOGIO", 32, cx + 50, cardY + 50, selected ? 0xFF00AAFF : 0xFFFFFFFF);
                 desenharTexto("Relogio, Cronometro, Timer, Alarme", 20, cx + 50, cardY + 100, 0xFF888888);
                 // Draw clock outline
                 int tX = cx + 230; int tY = cardY + 300;
                 drawCircle(tX, tY, 110, (Color){ 70, 70, 70, 255 }, false);
                 drawCircle(tX, tY, 106, (Color){ 120, 120, 120, 255 }, false);
                 drawCircle(tX, tY, 6, WHITE, true);
                 DrawLine(tX, tY, tX + 70, tY - 40, WHITE);
                 DrawLine(tX, tY, tX - 10, tY - 80, RED);
             }
             else if (i == 3) {
                 desenharTexto("GRAVADOR", 30, cx + 50, cardY + 50, selected ? 0xFF00AAFF : 0xFFFFFFFF);
                 desenharTexto("Audio, Video, Tela", 20, cx + 50, cardY + 100, 0xFF888888);
                 int grX = cx + 140, grY = cardY + 350;
                 drawCircle(grX, grY, 35, (Color){ 70, 70, 70, 255 }, false);
                 drawCircle(grX, grY, 30, (Color){ 120, 120, 120, 255 }, false);
                 drawCircle(grX, grY, 6, RED, true);
                 DrawLine(grX, grY, grX + 40, grY - 25, WHITE);
                 DrawLine(grX, grY, grX - 30, grY - 15, WHITE);
             }
        }
        // Segunda fileira: Contatos, Caixa de Entrada, Terminal, IA e QR/Codigos
        int cardW2 = 300, cardG2 = 45;
        int cardH2 = 200, cardY2 = cardY + cardH + 40;
         for (int i = 0; i < 5; i++) {
             int cx = startX + i * (cardW2 + cardG2);
             bool hovered = (mousePos.x >= cx && mousePos.x <= cx + cardW2 && mousePos.y >= cardY2 && mousePos.y <= cardY2 + cardH2);
             bool selected2 = (menuSelection == 4 + i);
            if (hovered && clicked) {
                if (i == 0) toolMode = TOOL_CONTATOS;
                else if (i == 1) toolMode = TOOL_CAIXA;
                else if (i == 2) toolMode = TOOL_TERMINAL;
                else if (i == 3) toolMode = TOOL_IA;
                else if (i == 4) toolMode = TOOL_QRCODE;
            }
              uint32_t cardColor = selected2 ? 0x6600AAFF : (hovered ? 0x44FFFFFF : 0x33FFFFFF);
              uint32_t borderColor = selected2 ? 0xFF00FFFF : (hovered ? 0xFF88FFFF : 0xFF00CCFF);
            desenharRetanguloAlpha(cx, cardY2, cardW2, cardH2, cardColor);
            DrawRectangleLines(cx, cardY2, cardW2, cardH2, (Color){ (borderColor >> 16) & 0xFF, (borderColor >> 8) & 0xFF, borderColor & 0xFF, 255 });
            if (i == 0) {
                desenharTexto("CONTATOS", 26, cx + 30, cardY2 + 40, 0xFFFFFFFF);
                desenharTexto("Nome, telefone, e-mails,", 18, cx + 30, cardY2 + 90, 0xFF888888);
                desenharTexto("campos e tags", 18, cx + 30, cardY2 + 114, 0xFF888888);
                drawCircle(cx + 235, cardY2 + 110, 32, (Color){ 0, 170, 255, 180 }, false);
                desenharTexto("@", 36, cx + 223, cardY2 + 92, 0xFF00AAFF);
            } else if (i == 1) {
                desenharTexto("CAIXA DE ENTRADA", 24, cx + 30, cardY2 + 40, 0xFFFFFFFF);
                desenharTexto("E-mail, WhatsApp, SMS,", 18, cx + 30, cardY2 + 90, 0xFF888888);
                desenharTexto("Telegram", 18, cx + 30, cardY2 + 114, 0xFF888888);
                DrawRectangle(cx + 215, cardY2 + 90, 62, 46, (Color){ 40, 40, 45, 255 });
                DrawRectangleLines(cx + 215, cardY2 + 90, 62, 46, (Color){ 120, 120, 130, 255 });
            } else if (i == 2) {
                desenharTexto("TERMINAL", 26, cx + 30, cardY2 + 40, 0xFFFFFFFF);
                desenharTexto("Comandos, abrir", 18, cx + 30, cardY2 + 90, 0xFF888888);
                desenharTexto("opencode e mais", 18, cx + 30, cardY2 + 114, 0xFF888888);
                DrawRectangle(cx + 30, cardY2 + 148, 240, 34, (Color){ 20, 20, 24, 255 });
                DrawRectangleLines(cx + 30, cardY2 + 148, 240, 34, (Color){ 0, 170, 90, 255 });
                desenharTexto(">_", 24, cx + 44, cardY2 + 155, 0xFF00C878);
            } else if (i == 3) {
                desenharTexto("IA / GEMINI", 26, cx + 30, cardY2 + 40, 0xFFFFFFFF);
                desenharTexto("Texto, visao, imagens,", 18, cx + 30, cardY2 + 90, 0xFF888888);
                desenharTexto("audio e voz", 18, cx + 30, cardY2 + 114, 0xFF888888);
                desenharRetanguloAlpha(cx + 30, cardY2 + 138, 180, 42, 0xFF0A3F52);
                DrawRectangleLines(cx + 30, cardY2 + 138, 180, 42, (Color){ 0, 170, 255, 255 });
                desenharTexto("✦ IA", 22, cx + 85, cardY2 + 149, 0xFF00CCFF);
            } else {
                desenharTexto("QR / COD. BARRAS", 24, cx + 30, cardY2 + 40, 0xFFFFFFFF);
                desenharTexto("QR Code, etiquetas,", 18, cx + 30, cardY2 + 90, 0xFF888888);
                desenharTexto("WhatsApp e leitor", 18, cx + 30, cardY2 + 114, 0xFF888888);
                int qx = cx + 205, qy = cardY2 + 80, qs = 13;
                for (int r = 0; r < 5; r++) {
                    for (int c = 0; c < 5; c++) {
                        bool fill = ((r * 7 + c * 3) % 4 != 1);
                        if (r == 2 && c == 2) fill = true;
                        if (fill) desenharRetanguloAlpha(qx + c * qs, qy + r * qs, qs - 2, qs - 2, 0xFFFFFFFF);
                    }
                }
            }
        }
    } 
    else if (toolMode == TOOL_CALCULADORA) {
        desenharTexto("CALCULADORA INTEGRA", 38, 100, 60, 0xFF00AAFF);
        desenharTexto("[ESC] Menu Anterior  |  Suporta teclado numerico e operadores", 20, 100, 110, 0xFFAAAAAA);

        // Voltar Button
        int vltX = 1600, vltY = 80, vltW = 220, vltH = 50;
        bool hoverVoltar = (mousePos.x >= vltX && mousePos.x <= vltX + vltW && mousePos.y >= vltY && mousePos.y <= vltY + vltH);
        if (hoverVoltar && clicked) {
            toolMode = TOOL_MENU;
        }
        desenharBotaoVoltarIcone(vltX, vltY, vltW, vltH, hoverVoltar);

        int calcX = 660;
        int calcY = 200;
        int calcW = 600;
        int calcH = 750;

        desenharRetanguloAlpha(calcX, calcY, calcW, calcH, 0xFF22252D);
        DrawRectangleLines(calcX, calcY, calcW, calcH, (Color){ 80, 80, 80, 255 });

        // LCD display screen
        int displayY = calcY + 30;
        int displayH = 150;
        desenharRetanguloAlpha(calcX + 30, displayY, calcW - 60, displayH, 0xFF17181A);
        DrawRectangleLines(calcX + 30, displayY, calcW - 60, displayH, (Color){ 50, 50, 50, 255 });

        // Draw calcBuffer and calcDisplay
        int bufW = medirTexto(calcBuffer, 20);
        desenharTexto(calcBuffer, 20, calcX + calcW - 50 - bufW, displayY + 20, 0xFF888888);

        int dispW = medirTexto(calcDisplay, 45);
        desenharTexto(calcDisplay, 45, calcX + calcW - 50 - dispW, displayY + 70, 0xFFFFFFFF);

        // Grid buttons layout
        const char* btnLabels[5][4] = {
            { "C", "<-", "/", "*" },
            { "7", "8", "9", "-" },
            { "4", "5", "6", "+" },
            { "1", "2", "3", "." },
            { "0", "=", "", "" }
        };

        int startX = calcX + 45;
        int startY = calcY + 210;
        int btnW = 120;
        int btnH = 80;
        int gapX = 10;
        int gapY = 10;

        for (int r = 0; r < 5; r++) {
            for (int c = 0; c < 4; c++) {
                const char* lbl = btnLabels[r][c];
                if (strlen(lbl) == 0) continue;

                int bx = startX + c * (btnW + gapX);
                int by = startY + r * (btnH + gapY);
                int bw = btnW;
                int bh = btnH;

                if (strcmp(lbl, "0") == 0) {
                    bw = btnW * 2 + gapX;
                } else if (strcmp(lbl, "=") == 0 && r == 4) {
                    // Make = double width on last row
                    bx = startX + 2 * (btnW + gapX);
                    bw = btnW * 2 + gapX;
                } else if (r == 4 && c >= 2) {
                    continue; // skipped because of double width
                }

                bool hBtn = (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
                if (hBtn && clicked) {
                    if (strcmp(lbl, "C") == 0) handleCalcClear();
                    else if (strcmp(lbl, "<-") == 0) handleCalcBackspace();
                    else if (strcmp(lbl, "+") == 0) handleCalcOp('+');
                    else if (strcmp(lbl, "-") == 0) handleCalcOp('-');
                    else if (strcmp(lbl, "*") == 0) handleCalcOp('*');
                    else if (strcmp(lbl, "/") == 0) handleCalcOp('/');
                    else if (strcmp(lbl, "=") == 0) handleCalcEqual();
                    else if (strcmp(lbl, ".") == 0) handleCalcInput(".");
                    else handleCalcInput(lbl);
                }

                uint32_t btnCor = 0xAA2D313C;
                uint32_t textCor = 0xFFFFFFFF;
                if (strcmp(lbl, "C") == 0 || strcmp(lbl, "<-") == 0) {
                    btnCor = hBtn ? 0xFFF87171 : 0xAA4B3438;
                } else if (strcmp(lbl, "+") == 0 || strcmp(lbl, "-") == 0 || strcmp(lbl, "*") == 0 || strcmp(lbl, "/") == 0 || strcmp(lbl, "=") == 0) {
                    btnCor = hBtn ? 0xFF00AAFF : 0xAA2B4052;
                } else {
                    btnCor = hBtn ? 0xAA454B59 : 0xAA2D313C;
                }

                desenharRetanguloAlpha(bx, by, bw, bh, btnCor);
                DrawRectangleLines(bx, by, bw, bh, (Color){ 80, 80, 80, 255 });

                int lblW = medirTexto(lbl, 30);
             desenharTexto(lbl, 30, bx + (bw - lblW)/2, by + (bh - 30)/2, textCor);
             }
         }
     }
      else if (toolMode == TOOL_QRCODE) {
        desenharTexto("QR E CODIGO DE BARRAS", 38, 100, 60, 0xFF00AAFF);
        desenharTexto("[ESC] Voltar  |  Gere QR Codes, etiquetas de barras e links de WhatsApp, ou leia codigos com a camera/imagem", 20, 100, 110, 0xFFAAAAAA);

        int vltX = 1600, vltY = 80, vltW = 220, vltH = 50;
        bool hoverVoltar = CheckCollisionPointRec(mousePos, (Rectangle){ (float)vltX, (float)vltY, (float)vltW, (float)vltH });
        if (hoverVoltar && clicked) { qr_scan_stop(); toolMode = TOOL_MENU; }
        desenharBotaoVoltarIcone(vltX, vltY, vltW, vltH, hoverVoltar);

        const char* tabNomes[4] = { "QR CODE", "CODIGO DE BARRAS", "WHATSAPP", "LER CODIGO" };
        for (int i = 0; i < 4; i++) {
            int tx = 100 + i * 450, ty = 170, tw = 430, th = 55;
            uint32_t bg = (qrTab == i) ? 0xFF0A3F52 : 0xFF1E2024;
            desenharRetanguloAlpha(tx, ty, tw, th, bg);
            DrawRectangleLines(tx, ty, tw, th, (qrTab == i) ? (Color){ 0, 170, 255, 255 } : (Color){ 90, 90, 90, 255 });
            int lw2 = medirTexto(tabNomes[i], 24);
            desenharTexto(tabNomes[i], 24, tx + (tw - lw2) / 2, ty + 15, (qrTab == i) ? 0xFF00CCFF : 0xFFCCCCCC);
        }

        if (qrMsg[0]) desenharTexto(qrMsg, 20, 100, 366, qrMsgErro ? 0xFFFF5555 : 0xFF00C878);

        if (qrTab == 0) {
            desenharTexto("Conteudo (URL ou texto):", 22, 100, 265, 0xFFCCCCCC);
            desenharRetanguloAlpha(100, 295, 1100, 54, 0xFF101418);
            DrawRectangleLines(100, 295, 1100, 54, qrFoco ? (Color){ 0, 170, 255, 255 } : (Color){ 90, 90, 90, 255 });
            char mostra[1100];
            snprintf(mostra, sizeof(mostra), "%s%s", qrInput, (qrFoco && ((int)(GetTime() * 2) % 2 == 0)) ? "|" : "");
            qr_desenha_linha_clip(mostra, 24, 112, 308, 1070, 0xFFFFFFFF);

            bool hG = CheckCollisionPointRec(mousePos, (Rectangle){ 1240.0f, 295.0f, 180.0f, 54.0f });
            desenharRetanguloAlpha(1240, 295, 180, 54, hG ? 0xFF00AAFF : 0xFF2B4052);
            DrawRectangleLines(1240, 295, 180, 54, (Color){ 0, 170, 255, 255 });
            int lg = medirTexto("GERAR", 26);
            desenharTexto("GERAR", 26, 1240 + (180 - lg) / 2, 309, 0xFFFFFFFF);

            if (qrTexOk) {
                desenharRetanguloAlpha(760, 420, 400, 400, 0xFFFFFFFF);
                float swq = (float)qrTex.width, shq = (float)qrTex.height;
                float esc = 380.0f / (swq > shq ? swq : shq);
                if (esc > 1.0f) esc = 1.0f;
                float dwq = swq * esc, dhq = shq * esc;
                DrawTexturePro(qrTex,
                    (Rectangle){ 0, 0, swq, shq },
                    (Rectangle){ 760 + (400.0f - dwq) / 2, 420 + (400.0f - dhq) / 2, dwq, dhq },
                    (Vector2){ 0, 0 }, 0.0f, WHITE);
                bool hS = CheckCollisionPointRec(mousePos, (Rectangle){ 760.0f, 850.0f, 190.0f, 48.0f });
                desenharRetanguloAlpha(760, 850, 190, 48, hS ? 0xFF00AAFF : 0xFF2B4052);
                DrawRectangleLines(760, 850, 190, 48, (Color){ 0, 170, 255, 255 });
                int ls = medirTexto("SALVAR PNG", 20);
                desenharTexto("SALVAR PNG", 20, 760 + (190 - ls) / 2, 863, 0xFFFFFFFF);
            } else {
                desenharTexto("Como usar:", 24, 100, 430, 0xFF00AAFF);
                desenharTexto("1. Digite ou cole a URL/texto no campo acima", 20, 100, 480, 0xFF999999);
                desenharTexto("2. Clique em GERAR para criar o QR Code", 20, 100, 512, 0xFF999999);
                desenharTexto("3. SALVAR PNG guarda em data\\HyperNeiva\\qrcodes", 20, 100, 544, 0xFF999999);
                desenharTexto("Dica: Ctrl+V cola da area de transferencia", 18, 100, 600, 0xFF666666);
            }
        }
        else if (qrTab == 1) {
            desenharTexto("Dados do codigo de barras (CODE128):", 22, 100, 265, 0xFFCCCCCC);
            desenharRetanguloAlpha(100, 295, 700, 54, 0xFF101418);
            DrawRectangleLines(100, 295, 700, 54, brFoco == 1 ? (Color){ 0, 170, 255, 255 } : (Color){ 90, 90, 90, 255 });
            char mostra[280];
            snprintf(mostra, sizeof(mostra), "%s%s", brDados, (brFoco == 1 && ((int)(GetTime() * 2) % 2 == 0)) ? "|" : "");
            qr_desenha_linha_clip(mostra, 24, 112, 308, 670, 0xFFFFFFFF);

            bool hG = CheckCollisionPointRec(mousePos, (Rectangle){ 840.0f, 295.0f, 220.0f, 54.0f });
            desenharRetanguloAlpha(840, 295, 220, 54, hG ? 0xFF00AAFF : 0xFF2B4052);
            DrawRectangleLines(840, 295, 220, 54, (Color){ 0, 170, 255, 255 });
            int lg = medirTexto("GERAR ETIQUETA", 22);
            desenharTexto("GERAR ETIQUETA", 22, 840 + (220 - lg) / 2, 311, 0xFFFFFFFF);

            desenharTexto("Texto da etiqueta (opcional):", 22, 100, 360, 0xFFCCCCCC);
            desenharRetanguloAlpha(100, 388, 700, 50, 0xFF101418);
            DrawRectangleLines(100, 388, 700, 50, brFoco == 2 ? (Color){ 0, 170, 255, 255 } : (Color){ 90, 90, 90, 255 });
            char mostra2[150];
            snprintf(mostra2, sizeof(mostra2), "%s%s", brLabel, (brFoco == 2 && ((int)(GetTime() * 2) % 2 == 0)) ? "|" : "");
            qr_desenha_linha_clip(mostra2, 22, 112, 400, 670, 0xFFDDDDDD);

            if (qrTexOk) {
                desenharRetanguloAlpha(560, 500, 800, 330, 0xFFFFFFFF);
                float swq = (float)qrTex.width, shq = (float)qrTex.height;
                float esc = 780.0f / swq;
                if (310.0f / shq < esc) esc = 310.0f / shq;
                if (esc > 1.0f) esc = 1.0f;
                float dwq = swq * esc, dhq = shq * esc;
                DrawTexturePro(qrTex,
                    (Rectangle){ 0, 0, swq, shq },
                    (Rectangle){ 560 + (800.0f - dwq) / 2, 500 + (330.0f - dhq) / 2, dwq, dhq },
                    (Vector2){ 0, 0 }, 0.0f, WHITE);
                bool hS = CheckCollisionPointRec(mousePos, (Rectangle){ 560.0f, 850.0f, 190.0f, 48.0f });
                desenharRetanguloAlpha(560, 850, 190, 48, hS ? 0xFF00AAFF : 0xFF2B4052);
                DrawRectangleLines(560, 850, 190, 48, (Color){ 0, 170, 255, 255 });
                int ls = medirTexto("SALVAR PNG", 20);
                desenharTexto("SALVAR PNG", 20, 560 + (190 - ls) / 2, 863, 0xFFFFFFFF);
            } else {
                desenharTexto("Padrao CODE128 aceita letras, numeros e simbolos.", 20, 100, 520, 0xFF999999);
                desenharTexto("O texto da etiqueta aparece abaixo das barras.", 20, 100, 552, 0xFF999999);
            }
        }
        else if (qrTab == 2) {
            desenharTexto("Numero (DDI+DDD+numero):", 22, 100, 265, 0xFFCCCCCC);
            desenharRetanguloAlpha(100, 295, 420, 54, 0xFF101418);
            DrawRectangleLines(100, 295, 420, 54, wpFoco == 1 ? (Color){ 0, 170, 255, 255 } : (Color){ 90, 90, 90, 255 });
            char mf[40];
            snprintf(mf, sizeof(mf), "%s%s", wpFone, (wpFoco == 1 && ((int)(GetTime() * 2) % 2 == 0)) ? "|" : "");
            desenharTexto(mf, 24, 112, 308, 0xFFFFFFFF);

            desenharTexto("Mensagem (opcional):", 22, 560, 265, 0xFFCCCCCC);
            desenharRetanguloAlpha(560, 295, 860, 54, 0xFF101418);
            DrawRectangleLines(560, 295, 860, 54, wpFoco == 2 ? (Color){ 0, 170, 255, 255 } : (Color){ 90, 90, 90, 255 });
            char mm[900];
            snprintf(mm, sizeof(mm), "%s%s", wpMsg, (wpFoco == 2 && ((int)(GetTime() * 2) % 2 == 0)) ? "|" : "");
            qr_desenha_linha_clip(mm, 24, 572, 308, 830, 0xFFFFFFFF);

            bool hGer = CheckCollisionPointRec(mousePos, (Rectangle){ 100.0f, 385.0f, 320.0f, 54.0f });
            desenharRetanguloAlpha(100, 385, 320, 54, hGer ? 0xFF00AA55 : 0xFF1E4034);
            DrawRectangleLines(100, 385, 320, 54, (Color){ 0, 200, 120, 255 });
            int lgr = medirTexto("GERAR LINK E QR", 24);
            desenharTexto("GERAR LINK E QR", 24, 100 + (320 - lgr) / 2, 400, 0xFFFFFFFF);

            bool hAbr = CheckCollisionPointRec(mousePos, (Rectangle){ 450.0f, 385.0f, 320.0f, 54.0f });
            desenharRetanguloAlpha(450, 385, 320, 54, hAbr ? 0xFF00AAFF : 0xFF2B4052);
            DrawRectangleLines(450, 385, 320, 54, wpTemLink ? (Color){ 0, 170, 255, 255 } : (Color){ 70, 70, 70, 255 });
            int lab = medirTexto("ABRIR NO NAVEGADOR", 22);
            desenharTexto("ABRIR NO NAVEGADOR", 22, 450 + (320 - lab) / 2, 400, wpTemLink ? 0xFFFFFFFF : 0xFF777777);

            if (wpTemLink) {
                char curto[104];
                snprintf(curto, sizeof(curto), "%s", wpLink);
                if (strlen(wpLink) > 96) strcpy(curto + 93, "...");
                desenharTexto(curto, 20, 100, 462, 0xFF00C878);
            }

            if (qrTexOk) {
                desenharRetanguloAlpha(760, 520, 400, 400, 0xFFFFFFFF);
                float swq = (float)qrTex.width, shq = (float)qrTex.height;
                float esc = 380.0f / (swq > shq ? swq : shq);
                if (esc > 1.0f) esc = 1.0f;
                float dwq = swq * esc, dhq = shq * esc;
                DrawTexturePro(qrTex,
                    (Rectangle){ 0, 0, swq, shq },
                    (Rectangle){ 760 + (400.0f - dwq) / 2, 520 + (400.0f - dhq) / 2, dwq, dhq },
                    (Vector2){ 0, 0 }, 0.0f, WHITE);
                bool hS = CheckCollisionPointRec(mousePos, (Rectangle){ 760.0f, 930.0f, 190.0f, 48.0f });
                desenharRetanguloAlpha(760, 930, 190, 48, hS ? 0xFF00AAFF : 0xFF2B4052);
                DrawRectangleLines(760, 930, 190, 48, (Color){ 0, 170, 255, 255 });
                int ls = medirTexto("SALVAR PNG", 20);
                desenharTexto("SALVAR PNG", 20, 760 + (190 - ls) / 2, 943, 0xFFFFFFFF);
            }
        }
        else { // LER CODIGO
            bool camAtiva = scanRunning || lerPrevOn;
            bool hCam = CheckCollisionPointRec(mousePos, (Rectangle){ 100.0f, 280.0f, 340.0f, 54.0f });
            uint32_t corCam = camAtiva ? (hCam ? 0xFFFF7777 : 0xFF7A3030) : (hCam ? 0xFF00AAFF : 0xFF2B4052);
            desenharRetanguloAlpha(100, 280, 340, 54, corCam);
            DrawRectangleLines(100, 280, 340, 54, camAtiva ? (Color){ 255, 120, 120, 255 } : (Color){ 0, 170, 255, 255 });
            const char* lblCam = camAtiva ? "PARAR CAMERA" : "ABRIR CAMERA";
            int lc = medirTexto(lblCam, 24);
            desenharTexto(lblCam, 24, 100 + (340 - lc) / 2, 295, 0xFFFFFFFF);
            if (scanRunning && !lerTemResult && ((int)(GetTime() * 2) % 2 == 0))
                desenharTexto("Aponte para o codigo... procurando", 20, 460, 297, 0xFFFFCC00);
            else if (lerTemResult)
                desenharTexto("Codigo detectado!", 20, 460, 297, 0xFF00C878);

            desenharTexto("Ou leia de uma imagem:", 22, 100, 372, 0xFFCCCCCC);
            desenharRetanguloAlpha(100, 400, 520, 50, 0xFF101418);
            DrawRectangleLines(100, 400, 520, 50, lerFocoPath ? (Color){ 0, 170, 255, 255 } : (Color){ 90, 90, 90, 255 });
            char mp[530];
            snprintf(mp, sizeof(mp), "%s%s", lerPathIn, (lerFocoPath && ((int)(GetTime() * 2) % 2 == 0)) ? "|" : "");
            qr_desenha_linha_clip(mp, 20, 112, 413, 490, 0xFFDDDDDD);
            bool hLI = CheckCollisionPointRec(mousePos, (Rectangle){ 640.0f, 400.0f, 160.0f, 50.0f });
            desenharRetanguloAlpha(640, 400, 160, 50, hLI ? 0xFF00AAFF : 0xFF2B4052);
            DrawRectangleLines(640, 400, 160, 50, (Color){ 0, 170, 255, 255 });
            int ll = medirTexto("LER IMAGEM", 20);
            desenharTexto("LER IMAGEM", 20, 640 + (160 - ll) / 2, 413, 0xFFFFFFFF);

            desenharTexto("Imagens em gravacoes (clique para ler):", 22, 100, 555, 0xFFCCCCCC);
            int listY = 590, rowH = 44, vis = 7;
            for (int i = 0; i < vis && (i + lerScroll) < lerTotal; i++) {
                int idx = i + lerScroll;
                int ry = listY + i * rowH;
                bool h = CheckCollisionPointRec(mousePos, (Rectangle){ 100.0f, (float)ry, 700.0f, (float)rowH - 4 });
                desenharRetanguloAlpha(100, ry, 700, rowH - 4, h ? 0x5500AAFF : 0x22FFFFFF);
                const char* nome = strrchr(lerLista[idx], '\\');
                nome = nome ? nome + 1 : lerLista[idx];
                qr_desenha_linha_clip(nome, 20, 115, ry + 9, 670, 0xFFDDDDDD);
            }
            if (lerTotal == 0)
                desenharTexto("(nenhuma imagem encontrada)", 20, 115, 598, 0xFF666666);

            // Camera
            desenharRetanguloAlpha(900, 280, 480, 360, 0xFF0A0E12);
            DrawRectangleLines(900, 280, 480, 360, scanRunning ? (Color){ 255, 200, 0, 255 } : (Color){ 90, 90, 90, 255 });
            if (lerCamTexOk) {
                float swc = (float)lerCamTex.width, shc = (float)lerCamTex.height;
                float esc = 460.0f / swc;
                if (340.0f / shc < esc) esc = 340.0f / shc;
                float dwc = swc * esc, dhc = shc * esc;
                DrawTexturePro(lerCamTex,
                    (Rectangle){ 0, 0, swc, shc },
                    (Rectangle){ 900 + (480.0f - dwc) / 2, 280 + (360.0f - dhc) / 2, dwc, dhc },
                    (Vector2){ 0, 0 }, 0.0f, WHITE);
            } else if (!scanRunning) {
                desenharTexto("Camera desligada", 22, 1050, 445, 0xFF888888);
            }

            // Resultado
            desenharRetanguloAlpha(900, 670, 880, 300, 0xFF101418);
            DrawRectangleLines(900, 670, 880, 300, (Color){ 90, 90, 90, 255 });
            desenharTexto(scanRunning && !lerTemResult ? "PROCURANDO CODIGO..." : "RESULTADO:", 22, 915, 682, (scanRunning && !lerTemResult) ? 0xFFFFCC00 : 0xFF00AAFF);
            if (lerTemResult) {
                const char* p = lerResultado;
                int ly2 = 720;
                while (*p && ly2 < 940) {
                    const char* nl = strchr(p, '\n');
                    int len = nl ? (int)(nl - p) : (int)strlen(p);
                    if (len > 1023) len = 1023;
                    char linha[1024];
                    memcpy(linha, p, len);
                    linha[len] = '\0';
                    qr_desenha_linha_clip(linha, 22, 915, ly2, 850, 0xFFEEEEEE);
                    ly2 += 28;
                    p += len;
                    if (*p == '\n') p++;
                }
                bool hCop = CheckCollisionPointRec(mousePos, (Rectangle){ 1420.0f, 910.0f, 180.0f, 46.0f });
                desenharRetanguloAlpha(1420, 910, 180, 46, hCop ? 0xFF00AAFF : 0xFF2B4052);
                DrawRectangleLines(1420, 910, 180, 46, (Color){ 0, 170, 255, 255 });
                int lcop = medirTexto("COPIAR", 20);
                desenharTexto("COPIAR", 20, 1420 + (180 - lcop) / 2, 923, 0xFFFFFFFF);
            } else if (!scanRunning) {
                desenharTexto("Nenhum codigo lido ainda.", 20, 915, 725, 0xFF666666);
            }
        }
      }
       else if (toolMode == TOOL_GRAVADOR) {
            desenharTexto("GRAVADOR E PRINT", 38, 100, 60, 0xFF00AAFF);
            desenharTexto("[ESC] Menu  |  Grave audio (microfone), video (webcam), tela, ou capture fotos (camera/tela) e gerencie seus arquivos", 20, 100, 110, 0xFFAAAAAA);
           int bx = 180, by = 170, bw = 500, bh = 55;

           int vltX = 1600, vltY = 80, vltW = 60, vltH = 50;
           bool hoverVoltar = CheckCollisionPointRec(mousePos, (Rectangle){vltX, vltY, vltW, vltH});
           desenharBotaoVoltarIcone(vltX, vltY, vltW, vltH, hoverVoltar);
       if (gravShowMenu) {
           int mw = 420, mh = 90, mx = 1220, my = 250, mg = 30;
              desenharRetanguloAlpha(mx - 20, my - 40, mw + 40, 4 * (mh + mg) + 50, 0xFF14161C);
              DrawRectangleLines(mx - 20, my - 40, mw + 40, 4 * (mh + mg) + 50, (Color){70,70,70,255});
              desenharTexto("ESCOLHA O TIPO DE GRAVACAO", 26, mx, my - 30, 0xFF00AAFF);
              const char* ops[4] = { "GRAVAR AUDIO (MICROFONE)", "GRAVAR VIDEO (WEBCAM)", "GRAVAR TELA (TELA + AUDIO)", "PRINT / FOTO (CAMERA OU TELA)" };
              for (int i = 0; i < 4; i++) {
                  int oy = my + i * (mh + mg);
                  desenharRetanguloAlpha(mx, oy, mw, mh, 0xFF1E2024);
                  DrawRectangleLines(mx, oy, mw, mh, (Color){100,150,255,255});
                  Texture2D ic = (i == 0) ? gravIconMic : (i == 1) ? gravIconCam : (i == 2) ? gravIconTela : gravIconCam;
                  if (ic.id != 0) {
                      DrawTexturePro(ic, (Rectangle){0,0,(float)ic.width,(float)ic.height},
                          (Rectangle){mx + 18, oy + (mh - 64)/2, 64, 64}, (Vector2){0,0}, 0.0f, WHITE);
                  }
                  int tw = medirTexto(ops[i], 24);
                  desenharTexto(ops[i], 24, mx + 100 + (mw - 110 - tw)/2, oy + (mh - 24)/2, 0xFFFFFFFF);
              }
          }
            else if (gravState == 4) {
               desenharRetanguloAlpha(bx, by, bw, bh, 0xFF442222);
               DrawRectangleLines(bx, by, bw, bh, (Color){255,80,80,255});
               desenharTexto(gravErrorMsg, 22, bx + 10, by + (bh - 22)/2, 0xFFFF8888);
               desenharTexto("(Clique ou ESC para fechar)", 18, bx, by + 85, 0xFF888888);
            }
            else if (gravState == 0) {
               if (gravTipo == 3) {
                   int fx = 190, fy = 700, fw = 250, fh = 60;
                   desenharTexto("FONTE:", 22, fx, fy - 32, 0xFFAAAAAA);
                   desenharRetanguloAlpha(fx, fy, fw, fh, (printFonte == 0) ? 0xFF224488 : 0xFF1E2024);
                   DrawRectangleLines(fx, fy, fw, fh, (printFonte == 0) ? (Color){100,200,255,255} : (Color){70,70,70,255});
                   int t1w = medirTexto("CAMERA", 20);
                   desenharTexto("CAMERA", 20, fx + (fw - t1w)/2, fy + (fh - 20)/2, 0xFFFFFFFF);
                   desenharRetanguloAlpha(fx + fw + 20, fy, fw, fh, (printFonte == 1) ? 0xFF224488 : 0xFF1E2024);
                   DrawRectangleLines(fx + fw + 20, fy, fw, fh, (printFonte == 1) ? (Color){100,200,255,255} : (Color){70,70,70,255});
                   t1w = medirTexto("TELA", 20);
                   desenharTexto("TELA", 20, fx + fw + 20 + (fw - t1w)/2, fy + (fh - 20)/2, 0xFFFFFFFF);
                   if (printFonte == 1) {
                       int my2 = fy + fh + 60;
                       desenharTexto("MODO:", 22, fx, my2 - 32, 0xFFAAAAAA);
                       desenharRetanguloAlpha(fx, my2, fw, fh, (printModo == 0) ? 0xFF224488 : 0xFF1E2024);
                       DrawRectangleLines(fx, my2, fw, fh, (printModo == 0) ? (Color){100,200,255,255} : (Color){70,70,70,255});
                       t1w = medirTexto("TELA INTEIRA", 18);
                       desenharTexto("TELA INTEIRA", 18, fx + (fw - t1w)/2, my2 + (fh - 18)/2, 0xFFFFFFFF);
                       desenharRetanguloAlpha(fx + fw + 20, my2, fw, fh, (printModo == 1) ? 0xFF224488 : 0xFF1E2024);
                       DrawRectangleLines(fx + fw + 20, my2, fw, fh, (printModo == 1) ? (Color){100,200,255,255} : (Color){70,70,70,255});
                       t1w = medirTexto("JANELA ATIVA", 18);
                       desenharTexto("JANELA ATIVA", 18, fx + fw + 20 + (fw - t1w)/2, my2 + (fh - 18)/2, 0xFFFFFFFF);
                   }
                   int capY = (printFonte == 1) ? fy + fh + 60 + fh + 30 : fy + fh + 50;
                   int cbW = 360, cbH = 70;
                   bool hCap = CheckCollisionPointRec(mousePos, (Rectangle){fx, capY, cbW, cbH});
                   desenharRetanguloAlpha(fx, capY, cbW, cbH, hCap ? 0xFF338833 : 0xFF225522);
                   DrawRectangleLines(fx, capY, cbW, cbH, (Color){120,255,150,255});
                   int ctw = medirTexto("CAPTURAR FOTO / PRINT", 22);
                   desenharTexto("CAPTURAR FOTO / PRINT", 22, fx + (cbW - ctw)/2, capY + (cbH - 22)/2, 0xFFFFFFFF);
                   desenharTexto("A foto sera salva em gravacoes/ e aparecera na lista. (ESC ou VOLTAR para sair)", 18, fx, capY + cbH + 20, 0xFFAAAAAA);
                   desenharPreviewVideo(bx + 10, by + 130, 480, 360);
               } else {
                   const char* lbl = (gravTipo == 2) ? "GRAVAR TELA" : (gravTipo == 1) ? "GRAVAR VIDEO" : "GRAVAR AUDIO";
                   desenharRetanguloAlpha(bx, by, bw, bh, 0xFF334455);
                   DrawRectangleLines(bx, by, bw, bh, (Color){200,200,100,255});
                   int tw = medirTexto(lbl, 24);
                   desenharTexto(lbl, 24, bx + (bw - tw)/2, by + (bh - 24)/2, 0xFFFFFFFF);
               }
           }
           else if (gravState == 5) {
                char hintInfo[160];
                snprintf(hintInfo, sizeof(hintInfo), "Gravacao concluida! Digite o nome do arquivo (%s) e pressione ENTER:", (gravTipo == 0) ? ".wav" : ".mp4");
                desenharTexto(hintInfo, 20, bx, by - 30, 0xFFCCCCCC);
               desenharRetanguloAlpha(bx, by, bw, bh, 0xFF222222);
               DrawRectangleLines(bx, by, bw, bh, (Color){100,200,255,255});
               char disp[128];
               snprintf(disp, sizeof(disp), "%s_", gravNomeInput);
               int tw = medirTexto(disp, 22);
               desenharTexto(disp, 22, bx + 10, by + (bh - 22)/2, 0xFFFFFFFF);
                desenharTexto("[ESC] Descartar gravacao", 18, bx + bw + 20, by + (bh - 18)/2, 0xFFAAAAAA);
                int saveW = 160, saveH = 45;
                bool hoverSalvar = CheckCollisionPointRec(mousePos, (Rectangle){bx, by + bh + 20, saveW, saveH});
                desenharRetanguloAlpha(bx, by + bh + 20, saveW, saveH, hoverSalvar ? 0xFF228833 : 0xFF116622);
                DrawRectangleLines(bx, by + bh + 20, saveW, saveH, (Color){100,255,120,255});
                int stw = medirTexto("SALVAR", 20);
                desenharTexto("SALVAR", 20, bx + (saveW - stw)/2, by + bh + 20 + (saveH - 20)/2, 0xFFFFFFFF);
            }
          else if (gravState == 2) {
              int iconSz = 50, gap = 16;
              bool hoverPausa = CheckCollisionPointRec(mousePos, (Rectangle){bx, by, iconSz, iconSz});
              bool hoverMic = CheckCollisionPointRec(mousePos, (Rectangle){bx + iconSz + gap, by, iconSz, iconSz});
              bool hoverTela = CheckCollisionPointRec(mousePos, (Rectangle){bx + 2*(iconSz+gap), by, iconSz, iconSz});
              bool hoverParar = CheckCollisionPointRec(mousePos, (Rectangle){bx + 3*(iconSz+gap) + 10, by, 240, bh});
              gravDesenharIcone(gravIconPause, bx, by, iconSz, WHITE, hoverPausa, (Color){255,200,100,255});
              Texture2D micIc = gravMicMuted ? gravIconMicOff : gravIconMic;
              Color micTint = gravMicMuted ? (Color){255,90,90,255} : WHITE;
              gravDesenharIcone(micIc, bx + iconSz + gap, by, iconSz, micTint, hoverMic, (Color){150,150,150,255});
               bool telaAtiva = (gravTipo == 2 || (gravTipo == 1 && gravVideoHasScreen));
               Color telaTint = telaAtiva ? WHITE : (Color){110,110,110,255};
               gravDesenharIcone(gravIconTela, bx + 2*(iconSz+gap), by, iconSz, telaTint, hoverTela, (Color){150,150,150,255});
              desenharRetanguloAlpha(bx + 3*(iconSz+gap) + 10, by, 240, bh, 0xFF882222);
              DrawRectangleLines(bx + 3*(iconSz+gap) + 10, by, 240, bh, (Color){255,100,100,255});
              int tw = medirTexto("PARAR", 24);
              desenharTexto("PARAR", 24, bx + 3*(iconSz+gap) + 10 + (240 - tw)/2, by + (bh - 24)/2, 0xFFFFFFFF);
              double elapsed = GetTime() - gravRecStartTime;
              int mins = (int)elapsed / 60;
              int secs = (int)elapsed % 60;
              char timeStr[64];
               snprintf(timeStr, sizeof(timeStr), "%s: %02d:%02d", (gravTipo == 2) ? "GRAVANDO TELA" : (gravTipo == 1) ? "GRAVANDO VIDEO" : "GRAVANDO", mins, secs);
               desenharTexto(timeStr, 28, bx, by + 80, 0xFF44FF44);
               desenharTexto(gravMicMuted ? "MIC MUTADO" : "MIC ATIVO", 20, bx + 330, by + 84, gravMicMuted ? 0xFFFF8888 : 0xFF88FF88);
               if (gravTipo == 1 || gravTipo == 2) desenharPreviewVideo(bx + 10, by + 130, 480, 360);
           }
          else if (gravState == 3) {
              int iconSz = 50, gap = 16;
              bool hoverPausa = CheckCollisionPointRec(mousePos, (Rectangle){bx, by, iconSz, iconSz});
              bool hoverMic = CheckCollisionPointRec(mousePos, (Rectangle){bx + iconSz + gap, by, iconSz, iconSz});
              bool hoverTela = CheckCollisionPointRec(mousePos, (Rectangle){bx + 2*(iconSz+gap), by, iconSz, iconSz});
              bool hoverParar = CheckCollisionPointRec(mousePos, (Rectangle){bx + 3*(iconSz+gap) + 10, by, 240, bh});
              gravDesenharIcone(gravIconPlay, bx, by, iconSz, WHITE, hoverPausa, (Color){100,255,100,255});
              Texture2D micIc = gravMicMuted ? gravIconMicOff : gravIconMic;
              Color micTint = gravMicMuted ? (Color){255,90,90,255} : WHITE;
              gravDesenharIcone(micIc, bx + iconSz + gap, by, iconSz, micTint, hoverMic, (Color){150,150,150,255});
               bool telaAtiva = (gravTipo == 2 || (gravTipo == 1 && gravVideoHasScreen));
               Color telaTint = telaAtiva ? (Color){255,170,0,255} : (Color){110,110,110,255};
               gravDesenharIcone(gravIconTela, bx + 2*(iconSz+gap), by, iconSz, telaTint, hoverTela, (Color){150,150,150,255});
              desenharRetanguloAlpha(bx + 3*(iconSz+gap) + 10, by, 240, bh, 0xFF882222);
              DrawRectangleLines(bx + 3*(iconSz+gap) + 10, by, 240, bh, (Color){255,100,100,255});
              int tw = medirTexto("PARAR", 24);
              desenharTexto("PARAR", 24, bx + 3*(iconSz+gap) + 10 + (240 - tw)/2, by + (bh - 24)/2, 0xFFFFFFFF);
               desenharTexto((gravTipo == 2) ? "GRAVACAO DE TELA PAUSADA" : (gravTipo == 1) ? "GRAVACAO DE VIDEO PAUSADA" : "GRAVACAO PAUSADA", 28, bx, by + 80, 0xFFFFAA00);
               desenharTexto(gravMicMuted ? "MIC MUTADO" : "MIC ATIVO", 20, bx + 330, by + 84, gravMicMuted ? 0xFFFF8888 : 0xFF88FF88);
               if (gravTipo == 1 || gravTipo == 2) desenharPreviewVideo(bx + 10, by + 130, 480, 360);
          }

           if (!gravShowMenu) {
           desenharTexto("Gravacoes:", 26, 1080, 130, 0xFF00AAFF);
           int listX = 1080, listY = 170, rowH = 50, listW = 720;
           int boxTop = listY - 35, boxBottom = boxTop + 800;
           int visRows = (boxBottom - boxTop) / rowH;
           int maxScroll = gravTotal > visRows ? gravTotal - visRows : 0;
           if (gravScroll > maxScroll) gravScroll = maxScroll;
           if (gravScroll < 0) gravScroll = 0;
          desenharRetanguloAlpha(listX - 10, listY - 35, listW + 20, 800, 0xFF111318);
          if (gravTotal == 0) {
              desenharTexto("(nenhuma gravacao)", 22, listX, listY, 0xFF666666);
          }
          for (int i = 0; i < gravTotal; i++) {
              int ry = listY + i * rowH - gravScroll * rowH;
              if (ry + 44 < boxTop || ry > boxBottom) continue;
              desenharRetanguloAlpha(listX, ry, listW, 44, 0xFF1E2024);
              DrawRectangleLines(listX, ry, listW, 44, (Color){60,60,60,255});
              const char* fname = strrchr(gravList[i], '/');
              if (!fname) fname = strrchr(gravList[i], '\\');
              if (!fname) fname = gravList[i]; else fname++;
              desenharTexto(fname, 22, listX + 10, ry + 11, 0xFFFFFFFF);
              int delX = listX + listW - 100, delW = 80;
              int playX = delX - 90, playW = 80;
              desenharRetanguloAlpha(playX, ry + 2, playW, 40, 0xFF224488);
              DrawRectangleLines(playX, ry + 2, playW, 40, (Color){100,150,255,255});
              int tw2 = medirTexto("TOCAR", 20);
              desenharTexto("TOCAR", 20, playX + (playW - tw2)/2, ry + 11, 0xFFFFFFFF);
              desenharRetanguloAlpha(delX, ry + 2, delW, 40, 0xFF662222);
              DrawRectangleLines(delX, ry + 2, delW, 40, (Color){255,120,120,255});
              if (gravIconDelete.id != 0) {
                  DrawTexturePro(gravIconDelete, (Rectangle){0,0,(float)gravIconDelete.width,(float)gravIconDelete.height},
                      (Rectangle){(float)(delX + 22), (float)(ry + 9), 36, 36}, (Vector2){0,0}, 0.0f, WHITE);
              }
           }
           if (gravDeleteIdx >= 0) {
               int dw = 560, dh = 220;
               int dx = 1920/2 - dw/2, dy = 1080/2 - dh/2;
               desenharRetanguloAlpha(0, 0, 1920, 1080, 0xAA000000);
               desenharRetanguloAlpha(dx, dy, dw, dh, 0xFF222233);
               DrawRectangleLines(dx, dy, dw, dh, (Color){255,255,255,255});
               desenharTexto("Tem certeza que deseja excluir?", 26, dx + 40, dy + 40, 0xFFFFAA00);
               desenharTexto("(ESC cancela)", 20, dx + 40, dy + 80, 0xFF888888);
               bool hSim = CheckCollisionPointRec(mousePos, (Rectangle){dx + 40, dy + dh - 80, 200, 55});
               bool hNao = CheckCollisionPointRec(mousePos, (Rectangle){dx + 320, dy + dh - 80, 200, 55});
               desenharRetanguloAlpha(dx + 40, dy + dh - 80, 200, 55, hSim ? 0xFF883333 : 0xFF552222);
               DrawRectangleLines(dx + 40, dy + dh - 80, 200, 55, (Color){255,120,120,255});
               int tw3 = medirTexto("EXCLUIR", 22);
               desenharTexto("EXCLUIR", 22, dx + 40 + (200 - tw3)/2, dy + dh - 80 + (55 - 22)/2, 0xFFFFFFFF);
               desenharRetanguloAlpha(dx + 320, dy + dh - 80, 200, 55, hNao ? 0xFF335577 : 0xFF223344);
               DrawRectangleLines(dx + 320, dy + dh - 80, 200, 55, (Color){150,180,220,255});
               tw3 = medirTexto("CANCELAR", 22);
               desenharTexto("CANCELAR", 22, dx + 320 + (200 - tw3)/2, dy + dh - 80 + (55 - 22)/2, 0xFFFFFFFF);
           }
           if (gravTotal > visRows) {
               int sbX = listX + listW + 8, sbTop = boxTop + 12;
               int trackH = 800 - 24;
               int thumbH = (trackH * visRows) / gravTotal;
               if (thumbH < 24) thumbH = 24;
               int thumbY = sbTop + (trackH - thumbH) * gravScroll / maxScroll;
               DrawRectangle(sbX, sbTop, 6, trackH, (Color){60,60,60,255});
               DrawRectangle(sbX, thumbY, 6, thumbH, (Color){140,150,170,255});
           }
           }
           if (gravPreviewTexLoaded && !((gravTipo == 1 || gravTipo == 2) && (gravState == 2 || gravState == 3)) && !(gravTipo == 3 && gravState == 0)) {
               UnloadTexture(gravPreviewTex);
               gravPreviewTexLoaded = false;
           }
      }
      else if (toolMode == TOOL_NOTEPAD) {
        if (notepadState == NOTEPAD_EDIT) {
            desenharTexto("BLOCO DE NOTAS DO AUDIO STUDIO", 38, 100, 60, 0xFF00AAFF);
            desenharTexto("[ESC] Salvar & Menu Anterior  |  Digite livremente pelo teclado  |  Roda do mouse rola o texto", 20, 100, 110, 0xFFAAAAAA);

            int btnW = 180;
            int btnH = 50;
            int btnY = 80;

            // Abrir
            int abrX = 1100;
            bool hoverAbr = (mousePos.x >= abrX && mousePos.x <= abrX + btnW && mousePos.y >= btnY && mousePos.y <= btnY + btnH);
            if (hoverAbr && clicked) {
                notepadState = NOTEPAD_OPEN_EXPLORER;
                noteListarArquivos(noteCaminhoAtual);
            }
            desenharRetanguloAlpha(abrX, btnY, btnW, btnH, hoverAbr ? 0xFF00AAFF : 0xFF333333);
            DrawRectangleLines(abrX, btnY, btnW, btnH, WHITE);
            int txtW = medirTexto("ABRIR", 18);
            desenharTexto("ABRIR", 18, abrX + (btnW - txtW)/2, btnY + 16, 0xFFFFFFFF);

            // Salvar
            int salX = 1300;
            bool hoverSal = (mousePos.x >= salX && mousePos.x <= salX + btnW && mousePos.y >= btnY && mousePos.y <= btnY + btnH);
            if (hoverSal && clicked) {
                notepadState = NOTEPAD_SAVE_DIALOG;
                strcpy(saveFilename, "");
                noteListarArquivos(noteCaminhoAtual);
            }
            desenharRetanguloAlpha(salX, btnY, btnW, btnH, hoverSal ? 0xFF00AAFF : 0xFF333333);
            DrawRectangleLines(salX, btnY, btnW, btnH, WHITE);
            txtW = medirTexto("SALVAR", 18);
            desenharTexto("SALVAR", 18, salX + (btnW - txtW)/2, btnY + 16, 0xFFFFFFFF);

            // Voltar
            int vltX = 1500;
            bool hoverVlt = (mousePos.x >= vltX && mousePos.x <= vltX + btnW && mousePos.y >= btnY && mousePos.y <= btnY + btnH);
            if (hoverVlt && clicked) {
                salvarNotasFerramenta();
                toolMode = TOOL_MENU;
            }
            desenharBotaoVoltarIcone(vltX, btnY, btnW, btnH, hoverVlt);

            int boxX = 100;
            int boxY = 160;
            int boxW = 1720;
            int boxH = 820;

            desenharRetanguloAlpha(boxX, boxY, boxW, boxH, 0xFF1E2024);
            DrawRectangleLines(boxX, boxY, boxW, boxH, (Color){ 80, 80, 80, 255 });

            // Begin scissor
            BeginScissorMode(boxX + 20, boxY + 20, boxW - 40, boxH - 40);

            // Print lines
            int lineY = boxY + 30 + notepadScrollY;
            char lineBuf[256];
            int lineBufIdx = 0;
            int totalTextLen = strlen(notepadText);

            for (int i = 0; i <= totalTextLen; i++) {
                char c = notepadText[i];
                if (c == '\n' || c == '\0' || lineBufIdx >= 250) {
                    lineBuf[lineBufIdx] = '\0';
                    desenharTexto(lineBuf, 25, boxX + 40, lineY, 0xFFFFFFFF);
                    lineY += 35;
                    lineBufIdx = 0;
                } else {
                    lineBuf[lineBufIdx++] = c;
                }
            }

            // Blinking cursor
            static float cursorBlink = 0.0f;
            cursorBlink += GetFrameTime();
            if (fmod(cursorBlink, 1.0f) < 0.5f) {
                int cursorX = boxX + 40;
                int lastLineLen = 0;
                for (int k = totalTextLen - 1; k >= 0; k--) {
                    if (notepadText[k] == '\n') break;
                    lastLineLen++;
                }
                char lastLineText[256] = "";
                if (lastLineLen > 0 && lastLineLen < 255) {
                    strncpy(lastLineText, notepadText + (totalTextLen - lastLineLen), lastLineLen);
                    lastLineText[lastLineLen] = '\0';
                    cursorX += medirTexto(lastLineText, 25);
                }
                desenharRetanguloAlpha(cursorX, lineY - 35, 12, 28, 0xFF00AAFF);
            }

            EndScissorMode();

            // Scroll bar
            int scrollBarH = 60;
            int scrollBarY = boxY + 20;
            desenharRetanguloAlpha(boxX + boxW - 15, boxY + 20, 8, boxH - 40, 0x44FFFFFF);
            desenharRetanguloAlpha(boxX + boxW - 15, scrollBarY, 8, scrollBarH, 0xFF00AAFF);
        }
        else if (notepadState == NOTEPAD_OPEN_EXPLORER) {
            desenharTexto("ABRIR NOTA - SELECIONE UM ARQUIVO", 38, 100, 60, 0xFF00AAFF);
            char pathLabel[1024];
            sprintf(pathLabel, "Pasta: %s  |  [ESC] Cancelar  |  Clique no arquivo para carregar", noteCaminhoAtual);
            desenharTexto(pathLabel, 18, 100, 110, 0xFFAAAAAA);

            // Cancel button
            int btnY = 80;
            int btnW = 180;
            int btnH = 50;
            int vltX = 1500;
            bool hoverVlt = (mousePos.x >= vltX && mousePos.x <= vltX + btnW && mousePos.y >= btnY && mousePos.y <= btnY + btnH);
            if (hoverVlt && clicked) {
                notepadState = NOTEPAD_EDIT;
            }
            desenharRetanguloAlpha(vltX, btnY, btnW, btnH, hoverVlt ? 0xFF00AAFF : 0xFF333333);
            DrawRectangleLines(vltX, btnY, btnW, btnH, WHITE);
            int txtW = medirTexto("CANCELAR", 18);
            desenharTexto("CANCELAR", 18, vltX + (btnW - txtW)/2, btnY + 16, 0xFFFFFFFF);

            int boxX = 100;
            int boxY = 160;
            int boxW = 1720;
            int boxH = 820;

            desenharRetanguloAlpha(boxX, boxY, boxW, boxH, 0xFF1E2024);
            DrawRectangleLines(boxX, boxY, boxW, boxH, (Color){ 80, 80, 80, 255 });

            int listY = boxY + 40;
            int itemsPerPage = 16;
            for (int i = noteScroll; i < noteScroll + itemsPerPage && i < noteTotalItens; i++) {
                int itemY = listY + (i - noteScroll) * 45;
                bool isSelected = (noteSel == i);
                bool isHovered = (mousePos.x >= boxX + 50 && mousePos.x <= boxX + boxW - 100 && mousePos.y >= itemY - 5 && mousePos.y <= itemY + 30);

                if (isHovered && clicked) {
                    noteSel = i;
                    if (noteEhPasta[i]) {
                        if (strcmp(notePaths[i], "..") == 0) {
                            char* lastSlash = strrchr(noteCaminhoAtual, '/');
                            if (!lastSlash) lastSlash = strrchr(noteCaminhoAtual, '\\');
                            if (lastSlash && lastSlash != noteCaminhoAtual) {
                                *lastSlash = '\0';
                                noteListarArquivos(noteCaminhoAtual);
                            }
                        } else {
                            strcpy(noteCaminhoAtual, notePaths[i]);
                            noteListarArquivos(noteCaminhoAtual);
                        }
                    } else {
                        // Load note file
                        FILE* f = fopen(notePaths[i], "r");
                        if (f) {
                            size_t readBytes = fread(notepadText, 1, sizeof(notepadText) - 1, f);
                            notepadText[readBytes] = '\0';
                            notepadTextLen = (int)readBytes;
                            fclose(f);
                        }
                        notepadState = NOTEPAD_EDIT;
                    }
                }

                uint32_t textCor = isSelected ? 0xFF00AAFF : (isHovered ? 0xFFFFFFFF : 0xFFBBBBBB);
                if (noteEhPasta[i]) {
                    desenharTexto(noteNomes[i], 22, boxX + 60, itemY, textCor);
                } else {
                    desenharTexto(noteNomes[i], 22, boxX + 80, itemY, textCor);
                    DrawRectangle(boxX + 60, itemY + 6, 8, 8, (Color){ 120, 120, 120, 255 });
                }
            }

            if (noteTotalItens == 0) {
                desenharTexto("Nenhum arquivo de texto (.txt) encontrado nesta pasta.", 24, boxX + 60, boxY + 60, 0xFF888888);
            }
        }
        else if (notepadState == NOTEPAD_SAVE_DIALOG) {
            desenharTexto("SALVAR NOTA - SELECIONE A PASTA DESTINO", 38, 100, 60, 0xFF00AAFF);
            char pathLabel[1024];
            sprintf(pathLabel, "Pasta: %s  |  [ESC] Cancelar  |  Clique nas pastas na lista para navegar", noteCaminhoAtual);
            desenharTexto(pathLabel, 18, 100, 110, 0xFFAAAAAA);

            // Left explorer panel
            int boxX = 100;
            int boxY = 160;
            int boxW = 1100;
            int boxH = 820;

            desenharRetanguloAlpha(boxX, boxY, boxW, boxH, 0xFF1E2024);
            DrawRectangleLines(boxX, boxY, boxW, boxH, (Color){ 80, 80, 80, 255 });

            int listY = boxY + 40;
            int itemsPerPage = 16;
            for (int i = noteScroll; i < noteScroll + itemsPerPage && i < noteTotalItens; i++) {
                int itemY = listY + (i - noteScroll) * 45;
                bool isSelected = (noteSel == i);
                bool isHovered = (mousePos.x >= boxX + 50 && mousePos.x <= boxX + boxW - 50 && mousePos.y >= itemY - 5 && mousePos.y <= itemY + 30);

                if (isHovered && clicked) {
                    noteSel = i;
                    if (noteEhPasta[i]) {
                        if (strcmp(notePaths[i], "..") == 0) {
                            char* lastSlash = strrchr(noteCaminhoAtual, '/');
                            if (!lastSlash) lastSlash = strrchr(noteCaminhoAtual, '\\');
                            if (lastSlash && lastSlash != noteCaminhoAtual) {
                                *lastSlash = '\0';
                                noteListarArquivos(noteCaminhoAtual);
                            }
                        } else {
                            strcpy(noteCaminhoAtual, notePaths[i]);
                            noteListarArquivos(noteCaminhoAtual);
                        }
                    } else {
                        // Clicked file, fill filename
                        char tempName[64];
                        strcpy(tempName, noteNomes[i]);
                        char* ext = strstr(tempName, ".txt");
                        if (ext) *ext = '\0';
                        strncpy(saveFilename, tempName, sizeof(saveFilename) - 1);
                        saveFilename[sizeof(saveFilename) - 1] = '\0';
                    }
                }

                uint32_t textCor = isSelected ? 0xFF00AAFF : (isHovered ? 0xFFFFFFFF : 0xFFBBBBBB);
                if (noteEhPasta[i]) {
                    desenharTexto(noteNomes[i], 22, boxX + 60, itemY, textCor);
                } else {
                    desenharTexto(noteNomes[i], 22, boxX + 80, itemY, textCor);
                    DrawRectangle(boxX + 60, itemY + 6, 8, 8, (Color){ 120, 120, 120, 255 });
                }
            }

            // Right Save Card
            int cardX = boxX + boxW + 40;
            int cardY = boxY;
            int cardW = 580;
            int cardH = 500;

            desenharRetanguloAlpha(cardX, cardY, cardW, cardH, 0xFF2D3139);
            DrawRectangleLines(cardX, cardY, cardW, cardH, (Color){ 100, 100, 100, 255 });

            desenharTexto("SALVAR ARQUIVO", 25, cardX + 30, cardY + 30, 0xFF00AAFF);
            desenharTexto("Nome do arquivo abaixo:", 18, cardX + 30, cardY + 100, 0xFFFFFFFF);

            int inpX = cardX + 30;
            int inpY = cardY + 140;
            int inpW = cardW - 60;
            int inpH = 55;
            desenharRetanguloAlpha(inpX, inpY, inpW, inpH, 0xFF151515);
            DrawRectangleLines(inpX, inpY, inpW, inpH, (Color){ 80, 80, 80, 255 });

            // Render typed name
            char dispName[128];
            if (strlen(saveFilename) == 0) {
                strcpy(dispName, "nome_do_arquivo");
                desenharTexto(dispName, 20, inpX + 15, inpY + 18, 0xFF555555);
            } else {
                sprintf(dispName, "%s.txt", saveFilename);
                desenharTexto(dispName, 20, inpX + 15, inpY + 18, 0xFF00FF00);
            }

            // Blinking cursor
            static float timerCursorBlink = 0.0f;
            timerCursorBlink += GetFrameTime();
            if (fmod(timerCursorBlink, 1.0f) < 0.5f) {
                int cursorOffset = strlen(saveFilename) == 0 ? medirTexto("nome_do_arquivo", 20) : medirTexto(dispName, 20);
                desenharRetanguloAlpha(inpX + 15 + cursorOffset + 5, inpY + 12, 4, 30, 0xFF00AAFF);
            }

            // Confirm & Cancel Buttons
            int btnW = 200;
            int btnH = 50;
            int cnfX = cardX + 40;
            int cncX = cardX + cardW - btnW - 40;
            int btnY2 = cardY + 240;

            // Confirm
            bool hoverConfirm = (mousePos.x >= cnfX && mousePos.x <= cnfX + btnW && mousePos.y >= btnY2 && mousePos.y <= btnY2 + btnH);
            bool canConfirm = (strlen(saveFilename) > 0);
            if (hoverConfirm && clicked && canConfirm) {
                char fullPath[1024];
                if (!strstr(saveFilename, ".txt")) {
                    sprintf(fullPath, "%s/%s.txt", noteCaminhoAtual, saveFilename);
                } else {
                    sprintf(fullPath, "%s/%s", noteCaminhoAtual, saveFilename);
                }

                #ifdef _WIN32
                _mkdir("data");
                _mkdir("data/HyperNeiva");
                _mkdir("data/HyperNeiva/midia");
                _mkdir("data/HyperNeiva/midia/leitura");
                #else
                mkdir("data", 0777);
                mkdir("data/HyperNeiva", 0777);
                mkdir("data/HyperNeiva/midia", 0777);
                mkdir("data/HyperNeiva/midia/leitura", 0777);
                #endif

                FILE* f = fopen(fullPath, "w");
                if (f) {
                    fwrite(notepadText, 1, strlen(notepadText), f);
                    fclose(f);
                }
                notepadState = NOTEPAD_EDIT;
            }
            desenharRetanguloAlpha(cnfX, btnY2, btnW, btnH, canConfirm ? (hoverConfirm ? 0xFF00AAFF : 0xFF333333) : 0xFF222222);
            DrawRectangleLines(cnfX, btnY2, btnW, btnH, canConfirm ? WHITE : (Color){ 60, 60, 60, 255 });
            txtW = medirTexto("CONFIRMAR", 16);
            desenharTexto("CONFIRMAR", 16, cnfX + (btnW - txtW)/2, btnY2 + 16, canConfirm ? 0xFFFFFFFF : 0xFF555555);

            // Cancel
            bool hoverCancel = (mousePos.x >= cncX && mousePos.x <= cncX + btnW && mousePos.y >= btnY2 && mousePos.y <= btnY2 + btnH);
            if (hoverCancel && clicked) {
                notepadState = NOTEPAD_EDIT;
            }
            desenharRetanguloAlpha(cncX, btnY2, btnW, btnH, hoverCancel ? 0xFF00AAFF : 0xFF333333);
            DrawRectangleLines(cncX, btnY2, btnW, btnH, WHITE);
            txtW = medirTexto("CANCELAR", 16);
            desenharTexto("CANCELAR", 16, cncX + (btnW - txtW)/2, btnY2 + 16, 0xFFFFFFFF);
        }
    }
     else if (toolMode == TOOL_TIMER) {
         desenharTexto("RELOGIO & TEMPO", 38, 100, 60, 0xFF00AAFF);
        desenharTexto("[ESC] Menu Anterior  |  [TAB] Alternar Abas (Relogio / Cronometro / Timer / Alarme)", 20, 100, 110, 0xFFAAAAAA);

        // Voltar Button
        int vltX = 1600, vltY = 80, vltW = 220, vltH = 50;
        bool hoverVoltar = (mousePos.x >= vltX && mousePos.x <= vltX + vltW && mousePos.y >= vltY && mousePos.y <= vltY + vltH);
        if (hoverVoltar && clicked) {
            toolMode = TOOL_MENU;
        }
        desenharBotaoVoltarIcone(vltX, vltY, vltW, vltH, hoverVoltar);

        // Draw Tabs (4)
        int tY = 160, tW = 415, tH = 50, tGap = 20;
        const char* tabLabels[4] = { "1. RELOGIO", "2. CRONOMETRO", "3. TIMER", "4. ALARME" };
        for (int ti = 0; ti < 4; ti++) {
            int tx = 100 + ti * (tW + tGap);
            bool hoverT = (mousePos.x >= tx && mousePos.x <= tx + tW && mousePos.y >= tY && mousePos.y <= tY + tH);
            if (hoverT && clicked) timerTab = ti;
            desenharRetanguloAlpha(tx, tY, tW, tH, (timerTab == ti) ? 0xFF00AAFF : 0xFF333333);
            desenharTexto(tabLabels[ti], 20, tx + 40, tY + 15, (timerTab == ti) ? 0xFF000000 : 0xFFFFFFFF);
        }

        // Main Board Casing
        int bX = 100, bY = 230, bW = 1720, bH = 750;
        
        uint32_t boardColor = 0xFF1E2024;
        if (alarmFlashTimer > 0.0f) {
            if (fmod(alarmFlashTimer, 0.4f) < 0.2f) {
                boardColor = 0xFF881111;
            }
        }
        
        desenharRetanguloAlpha(bX, bY, bW, bH, boardColor);
        DrawRectangleLines(bX, bY, bW, bH, (Color){ 80, 80, 80, 255 });

        if (timerState == TIMER_STATE_NORMAL) {
            if (timerTab == 0) {
                // ===== RELOGIO (CLOCK) =====
                time_t t = time(NULL);
                struct tm* tm = localtime(&t);
                char clockStr[64], dateStr[128], dowStr[64];
                sprintf(clockStr, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
                const char* meses[] = { "Janeiro","Fevereiro","Marco","Abril","Maio","Junho","Julho","Agosto","Setembro","Outubro","Novembro","Dezembro" };
                const char* dias[] = { "Domingo","Segunda","Terca","Quarta","Quinta","Sexta","Sabado" };
                sprintf(dateStr, "%02d de %s de %d", tm->tm_mday, meses[tm->tm_mon], tm->tm_year + 1900);
                sprintf(dowStr, "%s", dias[tm->tm_wday]);

                if (calSelDia == -1) { calSelDia = tm->tm_mday; calSelMes = tm->tm_mon + 1; calSelAno = tm->tm_year + 1900; }
                int leftCx = bX + 400;
                int cw = medirTexto(clockStr, 130);
                desenharTexto(clockStr, 130, leftCx - cw/2, bY + 130, 0xFFFFFFFF);

                // ===== CLIMA (lado da hora) =====
                int wIconY = bY + 310;
                if (weatherSol) {
                    DrawCircle(leftCx, wIconY + 20, 26, (Color){ 255, 215, 0, 255 });
                    for (int rr = 0; rr < 8; rr++) {
                        float ang = rr * 3.14159f / 4.0f;
                        DrawLine(leftCx + (int)(cosf(ang)*36), wIconY + 20 + (int)(sinf(ang)*36),
                                 leftCx + (int)(cosf(ang)*52), wIconY + 20 + (int)(sinf(ang)*52), (Color){ 255, 215, 0, 255 });
                    }
                } else {
                    DrawCircle(leftCx - 14, wIconY + 24, 20, (Color){ 180, 180, 190, 255 });
                    DrawCircle(leftCx + 8, wIconY + 16, 24, (Color){ 200, 200, 210, 255 });
                    DrawCircle(leftCx + 26, wIconY + 26, 18, (Color){ 180, 180, 190, 255 });
                }
                char wTemp[64];
                if (weatherTemp[0]) sprintf(wTemp, "%s C", weatherTemp);
                else strcpy(wTemp, "--");
                int wtw = medirTexto(wTemp, 40);
                desenharTexto(wTemp, 40, leftCx + 70, wIconY, 0xFFFFFF00);
                int wdw = medirTexto(weatherDesc, 22);
                desenharTexto(weatherDesc, 22, leftCx - wdw/2, wIconY + 56, 0xFFAAAAAA);
                if (weatherCidade[0]) {
                    int cw2 = medirTexto(weatherCidade, 22);
                    desenharTexto(weatherCidade, 22, leftCx - cw2/2, wIconY + 86, 0xFF00AAFF);
                }

                // Botao MUDAR LOCAL (clima manual)
                int mwX = leftCx - 90, mwY = wIconY + 116, mwW = 180, mwH = 34;
                bool hMw = (mousePos.x>=mwX && mousePos.x<=mwX+mwW && mousePos.y>=mwY && mousePos.y<=mwY+mwH);
                if (hMw && clicked) { weatherEditando = true; weatherBusca[0] = 0; }
                desenharRetanguloAlpha(mwX, mwY, mwW, mwH, hMw?0xFF00AAFF:0xAA333333); DrawRectangleLines(mwX,mwY,mwW,mwH,WHITE);
                desenharTexto("MUDAR LOCAL", 16, mwX + (mwW - medirTexto("MUDAR LOCAL",16))/2, mwY + 8, 0xFFFFFFFF);
                if (weatherEditando) {
                    int iwX = leftCx - 150, iwY = mwY + 42, iwW = 300, iwH = 36;
                    desenharRetanguloAlpha(iwX, iwY, iwW, iwH, 0xFF151515); DrawRectangleLines(iwX, iwY, iwW, iwH, (Color){ 0, 170, 255, 255 });
                    desenharTexto(weatherBusca, 20, iwX + 10, iwY + 8, 0xFFFFFFFF);
                    if (fmod(GetTime(), 1.0f) < 0.5f) desenharRetanguloAlpha(iwX + 10 + medirTexto(weatherBusca, 20) + 4, iwY + 6, 3, 24, 0xFF00AAFF);
                    desenharTexto("ENTER=buscar  ESC=cancelar", 14, iwX, iwY + 42, 0xFF888888);
                }
                if (weatherStatus[0]) desenharTexto(weatherStatus, 16, leftCx - medirTexto(weatherStatus,16)/2, mwY + 42, 0xFFEAB308);

                // Detalhes do clima (painel a direita do icone)
                {
                    int dx = bX + 600, dy = bY + 285, lh = 26;
                    char buf[64];
                    if (weatherCarregado) {
                        sprintf(buf, "Vento: %.0f km/h", weatherVento); desenharTexto(buf, 20, dx, dy, 0xFFCCCCCC); dy += lh;
                        sprintf(buf, "Umidade: %d%%", weatherUmidade); desenharTexto(buf, 20, dx, dy, 0xFFCCCCCC); dy += lh;
                        sprintf(buf, "Sensacao: %.0f C", weatherSensacao); desenharTexto(buf, 20, dx, dy, 0xFFCCCCCC); dy += lh;
                        sprintf(buf, "Pressao: %.0f hPa", weatherPressao); desenharTexto(buf, 20, dx, dy, 0xFFCCCCCC); dy += lh;
                        sprintf(buf, "Chuva: %d%%", weatherChuva); desenharTexto(buf, 20, dx, dy, 0xFFCCCCCC); dy += lh;
                        if (weatherSunrise[0]) { sprintf(buf, "Nascer: %s", weatherSunrise); desenharTexto(buf, 20, dx, dy, 0xFFCCCCCC); dy += lh; }
                        if (weatherSunset[0]) { sprintf(buf, "Por do sol: %s", weatherSunset); desenharTexto(buf, 20, dx, dy, 0xFFCCCCCC); dy += lh; }
                    }
                }

                int dw = medirTexto(dateStr, 36);
                desenharTexto(dateStr, 36, leftCx - dw/2, bY + 470, 0xFF00AAFF);
                int ww = medirTexto(dowStr, 32);
                desenharTexto(dowStr, 32, leftCx - ww/2, bY + 525, 0xFFAAAAAA);

                // Status dos alarmes
                {
                    int ativos = 0; int proxH = 99, proxM = 99, proxS = 99;
                    for (int i = 0; i < alarmeTotal; i++) if (alarmes[i].ativo) {
                        ativos++;
                        int secI = alarmes[i].hora * 3600 + alarmes[i].minuto * 60 + alarmes[i].segundo;
                        int secP = proxH * 3600 + proxM * 60 + proxS;
                        if (secI < secP || proxH == 99) { proxH = alarmes[i].hora; proxM = alarmes[i].minuto; proxS = alarmes[i].segundo; }
                    }
                    char aStr[128];
                    if (ativos > 0) {
                        if (proxH < 99) sprintf(aStr, "ALARMES ATIVOS: %d  |  PROXIMO: %02d:%02d:%02d", ativos, proxH, proxM, proxS);
                        else sprintf(aStr, "ALARMES ATIVOS: %d", ativos);
                    } else {
                        sprintf(aStr, "ALARME: nenhum ativo");
                    }
                    int aw = medirTexto(aStr, 28);
                    desenharTexto(aStr, 28, leftCx - aw/2, bY + 575, ativos > 0 ? 0xFFEAB308 : 0xFF64748B);
                }

                // ===== SELETOR DE AGENDA =====
                {
                    int ax = bX + 40, ay = bY + 620, pbW = 34, pbH = 30;
                    desenharTexto("AGENDA:", 20, ax, ay + 4, 0xFFAAAAAA);
                    int bx = ax + 90;
                    bool hP = (mousePos.x>=bx && mousePos.x<=bx+pbW && mousePos.y>=ay && mousePos.y<=ay+pbH);
                    if (hP && clicked) ferramentas_trocar_agenda((agendaAtual + agendasTotal - 1) % agendasTotal);
                    desenharRetanguloAlpha(bx, ay, pbW, pbH, hP?0xFF00AAFF:0xAA333333); DrawRectangleLines(bx, ay, pbW, pbH, WHITE);
                    desenharTexto("<", 20, bx + 10, ay + 6, 0xFFFFFFFF);
                    int nmX = bx + pbW + 8; int nmW = medirTexto(agendas[agendaAtual].nome, 22);
                    desenharTexto(agendas[agendaAtual].nome, 22, nmX, ay + 4, 0xFF00AAFF);
                    int nbX = nmX + nmW + 10; bool hN = (mousePos.x>=nbX && mousePos.x<=nbX+pbW && mousePos.y>=ay && mousePos.y<=ay+pbH);
                    if (hN && clicked) ferramentas_trocar_agenda((agendaAtual + 1) % agendasTotal);
                    desenharRetanguloAlpha(nbX, ay, pbW, pbH, hN?0xFF00AAFF:0xAA333333); DrawRectangleLines(nbX, ay, pbW, pbH, WHITE);
                    desenharTexto(">", 20, nbX + 10, ay + 6, 0xFFFFFFFF);
                    int vX = nbX + pbW + 10; bool hV = (mousePos.x>=vX && mousePos.x<=vX+90 && mousePos.y>=ay && mousePos.y<=ay+pbH);
                    if (hV && clicked) { agendaEditando = true; agendaNovoNome[0] = 0; }
                    desenharRetanguloAlpha(vX, ay, 90, pbH, hV?0xFF00AAFF:0xAA00AA00); DrawRectangleLines(vX, ay, 90, pbH, WHITE);
                    desenharTexto("+ NOVA", 18, vX + 15, ay + 6, 0xFFFFFFFF);
                    if (agendaEditando) {
                        int ix = vX + 100, iw = 220, ih = pbH;
                        desenharRetanguloAlpha(ix, ay, iw, ih, 0xFF151515); DrawRectangleLines(ix, ay, iw, ih, (Color){ 0, 170, 255, 255 });
                        desenharTexto(agendaNovoNome, 18, ix + 8, ay + 7, 0xFFFFFFFF);
                        if (fmod(GetTime(), 1.0f) < 0.5f) desenharRetanguloAlpha(ix + 8 + medirTexto(agendaNovoNome, 18) + 4, ay + 5, 3, 20, 0xFF00AAFF);
                    }
                }

                // ===== CALENDARIO (estilo Windows, com navegacao) =====
                if (calViewMes == -1) { calViewMes = tm->tm_mon + 1; calViewAno = tm->tm_year + 1900; }
                int calX = bX + 930, calY = bY + 20, calW = 880, calH = 400;
                desenharRetanguloAlpha(calX, calY, calW, calH, 0xFF222222);
                DrawRectangleLines(calX, calY, calW, calH, (Color){ 80, 80, 80, 255 });

                // Botoes de navegacao (ano/mes)
                int navY = calY + 12, navH = 40, navW = 50;
                int bAYm = calX + 15;
                bool hAYm = (mousePos.x>=bAYm && mousePos.x<=bAYm+navW && mousePos.y>=navY && mousePos.y<=navY+navH);
                if (hAYm && clicked) { calViewAno--; }
                desenharRetanguloAlpha(bAYm, navY, navW, navH, hAYm?0xFF00AAFF:0xAA333333); DrawRectangleLines(bAYm,navY,navW,navH,WHITE);
                desenharTexto("<<", 22, bAYm + navW/2 - 11, navY + 9, 0xFFFFFFFF);
                int bMm = calX + 70;
                bool hMm = (mousePos.x>=bMm && mousePos.x<=bMm+navW && mousePos.y>=navY && mousePos.y<=navY+navH);
                if (hMm && clicked) { calViewMes--; if (calViewMes < 1) { calViewMes = 12; calViewAno--; } }
                desenharRetanguloAlpha(bMm, navY, navW, navH, hMm?0xFF00AAFF:0xAA333333); DrawRectangleLines(bMm,navY,navW,navH,WHITE);
                desenharTexto("<", 26, bMm + navW/2 - 7, navY + 7, 0xFFFFFFFF);
                int bMp = calX + calW - 120;
                bool hMp = (mousePos.x>=bMp && mousePos.x<=bMp+navW && mousePos.y>=navY && mousePos.y<=navY+navH);
                if (hMp && clicked) { calViewMes++; if (calViewMes > 12) { calViewMes = 1; calViewAno++; } }
                desenharRetanguloAlpha(bMp, navY, navW, navH, hMp?0xFF00AAFF:0xAA333333); DrawRectangleLines(bMp,navY,navW,navH,WHITE);
                desenharTexto(">", 26, bMp + navW/2 - 7, navY + 7, 0xFFFFFFFF);
                int bAYp = calX + calW - 65;
                bool hAYp = (mousePos.x>=bAYp && mousePos.x<=bAYp+navW && mousePos.y>=navY && mousePos.y<=navY+navH);
                if (hAYp && clicked) { calViewAno++; }
                desenharRetanguloAlpha(bAYp, navY, navW, navH, hAYp?0xFF00AAFF:0xAA333333); DrawRectangleLines(bAYp,navY,navW,navH,WHITE);
                desenharTexto(">>", 22, bAYp + navW/2 - 11, navY + 9, 0xFFFFFFFF);

                int y = calViewAno;
                int m = calViewMes;
                int dim;
                if (m == 2) dim = ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 29 : 28;
                else if (m == 4 || m == 6 || m == 9 || m == 11) dim = 30;
                else dim = 31;
                struct tm first; memset(&first, 0, sizeof(first));
                first.tm_year = y - 1900; first.tm_mon = m - 1; first.tm_mday = 1; first.tm_hour = 12;
                mktime(&first);
                int firstDow = first.tm_wday;
                bool ehAtual = (m == tm->tm_mon + 1 && y == tm->tm_year + 1900);

                char mesTit[64]; sprintf(mesTit, "%s %d", meses[m - 1], y);
                int mtW = medirTexto(mesTit, 30);
                desenharTexto(mesTit, 30, calX + (calW - mtW)/2, navY + 4, 0xFF00AAFF);

                const char* hdr[7] = { "D","S","T","Q","Q","S","S" };
                int cellW = calW / 7;
                for (int c = 0; c < 7; c++) {
                    int hw = medirTexto(hdr[c], 20);
                    desenharTexto(hdr[c], 20, calX + c * cellW + (cellW - hw)/2, calY + 70, 0xFFAAAAAA);
                }
                for (int d = 1; d <= dim; d++) {
                    int pos = firstDow + (d - 1);
                    int r = pos / 7, c = pos % 7;
                    int cx = calX + c * cellW + cellW / 2;
                    int cy = calY + 100 + r * 44;
                    int cellX = calX + c * cellW;
                    bool selDia = (calSelDia == d && calSelMes == m && calSelAno == y);
                    bool temEv = ferramentas_contar_eventos_dia(d, m, y) > 0;
                    if (selDia) desenharRetanguloAlpha(cellX + 6, cy - 18, cellW - 12, 38, 0xFF00AAFF);
                    else if (ehAtual && d == tm->tm_mday) DrawCircle(cx, cy + 15, 22, (Color){ 0, 170, 255, 90 });
                    if (temEv) DrawCircle(cx + cellW/2 - 14, cy + 30, 4, (Color){ 255, 215, 0, 255 });
                    char dStr[8]; sprintf(dStr, "%d", d);
                    int dw2 = medirTexto(dStr, 24);
                    uint32_t col = selDia ? 0xFF000000 : ((ehAtual && d == tm->tm_mday) ? 0xFFFFFFFF : 0xFFDDDDDD);
                    desenharTexto(dStr, 24, cx - dw2/2, cy, col);
                    if (clicked && mousePos.x >= cellX && mousePos.x <= cellX + cellW && mousePos.y >= cy - 18 && mousePos.y <= cy + 20) {
                        calSelDia = d; calSelMes = m; calSelAno = y;
                    }
                }

                // Botao HOJE
                int hojeBW = 130, hojeBH = 36; int hojeX = calX + 15, hojeY = calY + calH - hojeBH - 10;
                bool hHoje = (mousePos.x>=hojeX && mousePos.x<=hojeX+hojeBW && mousePos.y>=hojeY && mousePos.y<=hojeY+hojeBH);
                if (hHoje && clicked) { calViewMes = tm->tm_mon + 1; calViewAno = tm->tm_year + 1900; calSelDia = tm->tm_mday; calSelMes = tm->tm_mon + 1; calSelAno = tm->tm_year + 1900; }
                desenharRetanguloAlpha(hojeX, hojeY, hojeBW, hojeBH, hHoje?0xFF00AAFF:0xAA333333); DrawRectangleLines(hojeX,hojeY,hojeBW,hojeBH,WHITE);
                int hjW = medirTexto("HOJE", 20); desenharTexto("HOJE", 20, hojeX + (hojeBW - hjW)/2, hojeY + 8, 0xFFFFFFFF);

                // ===== LISTA DE TAREFAS / EVENTOS DO DIA SELECIONADO =====
                int tasksX = calX, tasksY = calY + calH + 15, tasksW = calW, tasksH = (bY + bH) - tasksY - 15;
                desenharRetanguloAlpha(tasksX, tasksY, tasksW, tasksH, 0xFF1A1A1A);
                DrawRectangleLines(tasksX, tasksY, tasksW, tasksH, (Color){ 80, 80, 80, 255 });
                char titT[128]; sprintf(titT, "TAREFAS / EVENTOS - %02d/%02d/%04d", calSelDia, calSelMes, calSelAno);
                desenharTexto(titT, 22, tasksX + 20, tasksY + 14, 0xFF00AAFF);
                // Botao + (adicionar)
                int plusW = 44, plusH = 36, plusX = tasksX + tasksW - plusW - 15, plusY = tasksY + 8;
                bool hPlus = (mousePos.x>=plusX && mousePos.x<=plusX+plusW && mousePos.y>=plusY && mousePos.y<=plusY+plusH);
                if (hPlus && clicked) {
                    evtEditIndex = -1; evtNome[0]=0; evtDesc[0]=0; evtEmails[0]=0; evtPhones[0]=0;
                    evtHora = 0; evtMinuto = 0; evtNotifMask = 0; evtField = 0; evtNotifSel = 0; evtHoraField = 0;
                    evtDiaF = calSelDia; evtMesF = calSelMes; evtAnoF = calSelAno; evtHoraF = 0; evtMinF = 0;
                    evtCor = 0; evtRep = 0; evtRepDias = 1; evtHoraFField = 0; evtTipo = 0;
                    timerState = TIMER_STATE_EVENTO_EDIT;
                }
                desenharRetanguloAlpha(plusX, plusY, plusW, plusH, hPlus?0xFF00AAFF:0xAA00AA00); DrawRectangleLines(plusX,plusY,plusW,plusH,WHITE);
                desenharTexto("+", 30, plusX + plusW/2 - 9, plusY + 3, 0xFFFFFFFF);
                // Lista
                int ly = tasksY + 58;
                bool algumEv = false;
                for (int i = 0; i < eventosTotal; i++) {
                    if (eventos[i].dia != calSelDia || eventos[i].mes != calSelMes || eventos[i].ano != calSelAno) continue;
                    if (ly > tasksY + tasksH - 34) break;
                    algumEv = true;
                    char lin[300]; sprintf(lin, "%02d:%02d - %s", eventos[i].hora, eventos[i].minuto, eventos[i].nome);
                    int lx0 = tasksX + 20, lx1 = tasksX + tasksW - 70;
                    bool hl = (mousePos.x>=lx0 && mousePos.x<=lx1 && mousePos.y>=ly-4 && mousePos.y<=ly+26);
                    if (hl && clicked) {
                        evtEditIndex = i; strcpy(evtNome, eventos[i].nome); strcpy(evtDesc, eventos[i].desc);
                        strcpy(evtEmails, eventos[i].emails); strcpy(evtPhones, eventos[i].phones);
                        evtHora = eventos[i].hora; evtMinuto = eventos[i].minuto; evtNotifMask = eventos[i].notifMask;
                        evtDiaF = eventos[i].diaF; evtMesF = eventos[i].mesF; evtAnoF = eventos[i].anoF;
                        evtHoraF = eventos[i].horaF; evtMinF = eventos[i].minutoF;
                        evtCor = eventos[i].cor; evtRep = eventos[i].rep; evtRepDias = eventos[i].repDias; evtTipo = eventos[i].tipo;
                        evtField = 0; evtNotifSel = 0; evtHoraField = 0; evtHoraFField = 0; timerState = TIMER_STATE_EVENTO_EDIT;
                    }
                    Color cc = evtCorColor(eventos[i].cor);
                    DrawRectangle(lx0, ly + 2, 14, 14, cc);
                    if (eventos[i].horaF == 0 && eventos[i].minutoF == 0 && eventos[i].diaF == eventos[i].dia && eventos[i].mesF == eventos[i].mes && eventos[i].anoF == eventos[i].ano)
                        sprintf(lin, "%s%02d:%02d - %s", eventos[i].tipo == 1 ? "(T) " : "", eventos[i].hora, eventos[i].minuto, eventos[i].nome);
                    else
                        sprintf(lin, "%s%02d:%02d - %02d:%02d  %s", eventos[i].tipo == 1 ? "(T) " : "", eventos[i].hora, eventos[i].minuto, eventos[i].horaF, eventos[i].minutoF, eventos[i].nome);
                    desenharTexto(lin, 22, lx0 + 24, ly, hl ? 0xFF00AAFF : 0xFFFFFFFF);
                    // Botao excluir (X)
                    int exW = 30, exH = 26, exX = tasksX + tasksW - 45, exY = ly - 2;
                    bool hEx = (mousePos.x>=exX && mousePos.x<=exX+exW && mousePos.y>=exY && mousePos.y<=exY+exH);
                    if (hEx && clicked) {
                        for (int k = i; k < eventosTotal - 1; k++) eventos[k] = eventos[k+1];
                        eventosTotal--; ferramentas_salvar_eventos();
                    }
                    desenharRetanguloAlpha(exX, exY, exW, exH, hEx?0xFF00AAFF:0xAAFF0000); DrawRectangleLines(exX,exY,exW,exH,WHITE);
                    desenharTexto("X", 18, exX + 9, exY + 4, 0xFFFFFFFF);
                    ly += 34;
                }
                if (!algumEv) desenharTexto("Nenhum evento. Clique em + para adicionar.", 18, tasksX + 30, tasksY + 58, 0xFF64748B);
            }
            else if (timerTab == 1) {
                int digitsY = bY + 180;
                int hours = (int)(chronoElapsed / 3600.0);
                int minutes = (int)((chronoElapsed - hours * 3600.0) / 60.0);
                int seconds = (int)(chronoElapsed - hours * 3600.0 - minutes * 60.0);
                double fract = chronoElapsed - floor(chronoElapsed);
                int ms = (int)(fract * 1000.0);

                char chronoStr[128];
                sprintf(chronoStr, "%02d:%02d:%02d.%03d", hours, minutes, seconds, ms);
                
                int timeW = medirTexto(chronoStr, 120);
                desenharTexto(chronoStr, 120, bX + (bW - timeW)/2 - 150, digitsY, 0xFFFFFFFF);

                // Control Buttons
                int btnY = bY + 380;
                int btnW = 200, btnH = 70;
                int startBtnX = bX + 220;
                int lapBtnX = startBtnX + 240;
                int resetBtnX = lapBtnX + 240;

                // Start/Pause Button
                bool hStart = (mousePos.x >= startBtnX && mousePos.x <= startBtnX + btnW && mousePos.y >= btnY && mousePos.y <= btnY + btnH);
                if (hStart && clicked) {
                    if (chronoRunning) {
                        chronoRunning = false;
                    } else {
                        chronoRunning = true;
                        chronoStartTime = GetTime() - chronoElapsed;
                    }
                }
                desenharRetanguloAlpha(startBtnX, btnY, btnW, btnH, hStart ? 0xFF00AAFF : (chronoRunning ? 0xAAFF0000 : 0xAA00FF00));
                DrawRectangleLines(startBtnX, btnY, btnW, btnH, WHITE);
                const char* startLabel = chronoRunning ? "PAUSAR" : "INICIAR";
                int labelW = medirTexto(startLabel, 24);
                desenharTexto(startLabel, 24, startBtnX + (btnW - labelW)/2, btnY + 22, 0xFFFFFFFF);

                // Lap Button
                bool hLap = (mousePos.x >= lapBtnX && mousePos.x <= lapBtnX + btnW && mousePos.y >= btnY && mousePos.y <= btnY + btnH);
                if (hLap && clicked && chronoRunning) {
                    for (int l = 4; l > 0; l--) {
                        chronoLaps[l] = chronoLaps[l - 1];
                    }
                    chronoLaps[0] = chronoElapsed;
                    if (chronoLapCount < 5) chronoLapCount++;
                }
                desenharRetanguloAlpha(lapBtnX, btnY, btnW, btnH, hLap ? 0xFFEAB308 : 0xAA555555);
                DrawRectangleLines(lapBtnX, btnY, btnW, btnH, WHITE);
                labelW = medirTexto("VOLTA", 24);
                desenharTexto("VOLTA", 24, lapBtnX + (btnW - labelW)/2, btnY + 22, 0xFFFFFFFF);

                // Reset Button
                bool hReset = (mousePos.x >= resetBtnX && mousePos.x <= resetBtnX + btnW && mousePos.y >= btnY && mousePos.y <= btnY + btnH);
                if (hReset && clicked) {
                    chronoRunning = false;
                    chronoElapsed = 0.0;
                    chronoLapCount = 0;
                    memset(chronoLaps, 0, sizeof(chronoLaps));
                }
                desenharRetanguloAlpha(resetBtnX, btnY, btnW, btnH, hReset ? 0xFF888888 : 0xAA333333);
                DrawRectangleLines(resetBtnX, btnY, btnW, btnH, WHITE);
                labelW = medirTexto("ZERAR", 24);
                desenharTexto("ZERAR", 24, resetBtnX + (btnW - labelW)/2, btnY + 22, 0xFFFFFFFF);

                // Display Lap History (Right Panel)
                int lapPanelX = bX + 1150;
                int lapPanelY = bY + 120;
                desenharTexto("TEMPOS PARCIAIS (LAPS):", 25, lapPanelX, lapPanelY, 0xFF00AAFF);
                for (int l = 0; l < chronoLapCount; l++) {
                    int lHours = (int)(chronoLaps[l] / 3600.0);
                    int lMinutes = (int)((chronoLaps[l] - lHours * 3600.0) / 60.0);
                    int lSeconds = (int)(chronoLaps[l] - lHours * 3600.0 - lMinutes * 60.0);
                    double lFract = chronoLaps[l] - floor(chronoLaps[l]);
                    int lMs = (int)(lFract * 1000.0);

                    char lapStr[128];
                    sprintf(lapStr, "Volta %d:   %02d:%02d:%02d.%03d", l + 1, lHours, lMinutes, lSeconds, lMs);
                    desenharTexto(lapStr, 22, lapPanelX, lapPanelY + 60 + l * 45, 0xFFFFFFFF);
                }
                if (chronoLapCount == 0) {
                    desenharTexto("Nenhuma volta registrada.", 20, lapPanelX, lapPanelY + 60, 0xFF64748B);
                }

                // Salvar / Abrir laps
                int lapSaveX = lapPanelX;
                int lapSaveY = bY + 480;
                int lapBtnW = 200, lapBtnH = 50;

                bool hLapSave = (mousePos.x >= lapSaveX && mousePos.x <= lapSaveX + lapBtnW && mousePos.y >= lapSaveY && mousePos.y <= lapSaveY + lapBtnH);
                if (hLapSave && clicked) {
                    timerState = TIMER_STATE_SAVE_DIALOG;
                    timerSaveFilename[0] = '\0';
                    timerListarArquivos(timerCaminhoAtual);
                }
                desenharRetanguloAlpha(lapSaveX, lapSaveY, lapBtnW, lapBtnH, hLapSave ? 0xFF00AAFF : 0xFF333333);
                DrawRectangleLines(lapSaveX, lapSaveY, lapBtnW, lapBtnH, WHITE);
                txtW = medirTexto("SALVAR TEMPOS", 16);
                desenharTexto("SALVAR TEMPOS", 16, lapSaveX + (lapBtnW - txtW)/2, lapSaveY + 16, 0xFFFFFFFF);

                int lapOpenX = lapPanelX + 220;
                bool hLapOpen = (mousePos.x >= lapOpenX && mousePos.x <= lapOpenX + lapBtnW && mousePos.y >= lapSaveY && mousePos.y <= lapSaveY + lapBtnH);
                if (hLapOpen && clicked) {
                    timerState = TIMER_STATE_OPEN_EXPLORER;
                    timerListarArquivos(timerCaminhoAtual);
                }
                desenharRetanguloAlpha(lapOpenX, lapSaveY, lapBtnW, lapBtnH, hLapOpen ? 0xFF00AAFF : 0xFF333333);
                DrawRectangleLines(lapOpenX, lapSaveY, lapBtnW, lapBtnH, WHITE);
                txtW = medirTexto("ABRIR TEMPOS", 16);
                desenharTexto("ABRIR TEMPOS", 16, lapOpenX + (lapBtnW - txtW)/2, lapSaveY + 16, 0xFFFFFFFF);
            } 
            else if (timerTab == 2) {
                int digitsY = bY + 180;
                int hours = (int)(timerRemaining / 3600.0);
                int minutes = (int)((timerRemaining - hours * 3600.0) / 60.0);
                int seconds = (int)(timerRemaining - hours * 3600.0 - minutes * 60.0);

                char timerStr[128];
                sprintf(timerStr, "%02d:%02d:%02d", hours, minutes, seconds);

                // Display segmentado com campo selecionado (setas cima/baixo editam)
                int dispX = bX + (bW) / 2 - 360;
                int segW = 120, sepW = 40;
                int colH = (timerEditField == 0) ? 0xFF00AAFF : 0xFFFFFFFF;
                int colM = (timerEditField == 1) ? 0xFF00AAFF : 0xFFFFFFFF;
                int colS = (timerEditField == 2) ? 0xFF00AAFF : 0xFFFFFFFF;
                char hh[8], mm[8], ss[8];
                sprintf(hh, "%02d", hours); sprintf(mm, "%02d", minutes); sprintf(ss, "%02d", seconds);
                desenharTexto(hh, 120, dispX, digitsY, (timerRunning ? 0xFF64748B : colH));
                desenharTexto(":", 120, dispX + segW, digitsY, 0xFFFFFFFF);
                desenharTexto(mm, 120, dispX + segW + sepW, digitsY, (timerRunning ? 0xFF64748B : colM));
                desenharTexto(":", 120, dispX + 2 * segW + sepW, digitsY, 0xFFFFFFFF);
                desenharTexto(ss, 120, dispX + 2 * segW + 2 * sepW, digitsY, (timerRunning ? 0xFF64748B : colS));
                int timeW = medirTexto(timerStr, 120);
                // Clique no display seleciona o campo
                if (!timerRunning && clicked) {
                    int cx = mousePos.x;
                    if (cx >= dispX && cx < dispX + segW) timerEditField = 0;
                    else if (cx >= dispX + segW + sepW && cx < dispX + 2 * segW + sepW) timerEditField = 1;
                    else if (cx >= dispX + 2 * segW + 2 * sepW && cx < dispX + 3 * segW + 2 * sepW) timerEditField = 2;
                }
                desenharTexto("Use SETAS <- -> p/ campo e CIMA/BAIXO p/ ajustar", 18, dispX, digitsY + 70, 0xFF64748B);

                // Setas visuais (▲/▼) para cada campo (hora/min/seg)
                if (!timerRunning) {
                    int fldSeg[3]; fldSeg[0] = dispX; fldSeg[1] = dispX + segW + sepW; fldSeg[2] = dispX + 2 * segW + 2 * sepW;
                    for (int f = 0; f < 3; f++) {
                        int cx = fldSeg[f] + segW / 2;
                        int upX = cx - 22, upY = digitsY - 70, arS = 44, arH = 38;
                        int dnX = cx - 22, dnY = digitsY + 150;
                        bool hUp = (mousePos.x >= upX && mousePos.x <= upX + arS && mousePos.y >= upY && mousePos.y <= upY + arH);
                        bool hDn = (mousePos.x >= dnX && mousePos.x <= dnX + arS && mousePos.y >= dnY && mousePos.y <= dnY + arH);
                        if (hUp && clicked) {
                            int h2 = hours, m2 = minutes, s2 = seconds;
                            if (f == 0) h2++; else if (f == 1) m2++; else s2++;
                            if (h2 > 99) h2 = 0; if (m2 > 59) m2 = 0; if (s2 > 59) s2 = 0;
                            timerRemaining = h2 * 3600.0 + m2 * 60.0 + s2; timerTotalDuration = timerRemaining;
                        }
                        if (hDn && clicked) {
                            int h2 = hours, m2 = minutes, s2 = seconds;
                            if (f == 0) h2--; else if (f == 1) m2--; else s2--;
                            if (h2 < 0) h2 = 0; if (m2 < 0) m2 = 0; if (s2 < 0) s2 = 0;
                            timerRemaining = h2 * 3600.0 + m2 * 60.0 + s2; timerTotalDuration = timerRemaining;
                        }
                        // desenho seta cima
                        desenharRetanguloAlpha(upX, upY, arS, arH, hUp ? 0xFF00AAFF : 0xAA333333);
                        DrawRectangleLines(upX, upY, arS, arH, WHITE);
                        DrawTriangle((Vector2){ (float)cx, (float)(upY + 8) }, (Vector2){ (float)upX + 6, (float)(upY + arH - 6) }, (Vector2){ (float)upX + arS - 6, (float)(upY + arH - 6) }, WHITE);
                        // desenho seta baixo
                        desenharRetanguloAlpha(dnX, dnY, arS, arH, hDn ? 0xFF00AAFF : 0xAA333333);
                        DrawRectangleLines(dnX, dnY, arS, arH, WHITE);
                        DrawTriangle((Vector2){ (float)upX + 6, (float)dnY + 8 }, (Vector2){ (float)upX + arS - 6, (float)dnY + 8 }, (Vector2){ (float)cx, (float)(dnY + arH - 6) }, WHITE);
                    }
                }

                // Adjustment buttons
                int adjX = bX + 1100;
                int adjY = bY + 100;
                desenharTexto("AJUSTAR TEMPO:", 25, adjX, adjY, 0xFF00AAFF);

                typedef struct { const char* name; double secDiff; } Adjuster;
                Adjuster adjusters[] = {
                    { "+1 Hora", 3600.0 }, { "-1 Hora", -3600.0 },
                    { "+1 Min", 60.0 },    { "-1 Min", -60.0 },
                    { "+10 Seg", 10.0 },   { "-10 Seg", -10.0 }
                };

                for (int a = 0; a < 6; a++) {
                    int btnAX = adjX + (a % 2) * 190;
                    int btnAY = adjY + 50 + (a / 2) * 60;
                    int bAW = 170, bAH = 50;

                    bool hAdj = (mousePos.x >= btnAX && mousePos.x <= btnAX + bAW && mousePos.y >= btnAY && mousePos.y <= btnAY + bAH);
                    if (hAdj && clicked && !timerRunning) {
                        timerRemaining += adjusters[a].secDiff;
                        if (timerRemaining < 0.0) timerRemaining = 0.0;
                        timerTotalDuration = timerRemaining;
                    }

                    desenharRetanguloAlpha(btnAX, btnAY, bAW, bAH, timerRunning ? 0x22FFFFFF : (hAdj ? 0xFF00AAFF : 0xAA333333));
                    DrawRectangleLines(btnAX, btnAY, bAW, bAH, WHITE);
                    int lblW = medirTexto(adjusters[a].name, 20);
                    desenharTexto(adjusters[a].name, 20, btnAX + (bAW - lblW)/2, btnAY + 15, timerRunning ? 0xFF64748B : 0xFFFFFFFF);
                }

                // Progress circle border
                int ringX = bX + (bW - timeW)/2 - 150 + timeW / 2;
                int ringY = digitsY + 70;
                DrawCircleLines(ringX, ringY, 190, DARKGRAY);
                if (timerTotalDuration > 0.0) {
                    float proportion = (float)(timerRemaining / timerTotalDuration);
                    DrawCircleSector((Vector2){ (float)ringX, (float)ringY }, 190, -90.0f, -90.0f + 360.0f * proportion, 60, (Color){ 0, 170, 255, 60 });
                }

                int btnY = bY + 520;
                int btnW = 180, btnH = 70;
                int startBtnX = bX + 220;
                int pauseBtnX = startBtnX + 200;
                int stopBtnX = pauseBtnX + 200;

                // INICIAR
                bool hStart = (mousePos.x >= startBtnX && mousePos.x <= startBtnX + btnW && mousePos.y >= btnY && mousePos.y <= btnY + btnH);
                if (hStart && clicked && !timerRunning && timerRemaining > 0.0) {
                    timerRunning = true;
                    timerStartTime = GetTime() + timerRemaining;
                }
                desenharRetanguloAlpha(startBtnX, btnY, btnW, btnH, (timerRemaining <= 0.0 || timerRunning) ? 0x22FFFFFF : (hStart ? 0xFF00AAFF : 0xAA00FF00));
                DrawRectangleLines(startBtnX, btnY, btnW, btnH, (timerRemaining <= 0.0 || timerRunning) ? GRAY : WHITE);
                int labelW = medirTexto("INICIAR", 24);
                desenharTexto("INICIAR", 24, startBtnX + (btnW - labelW)/2, btnY + 22, (timerRemaining <= 0.0 || timerRunning) ? 0xFF888888 : 0xFFFFFFFF);

                // PAUSAR
                bool hPause = (mousePos.x >= pauseBtnX && mousePos.x <= pauseBtnX + btnW && mousePos.y >= btnY && mousePos.y <= btnY + btnH);
                if (hPause && clicked && timerRunning) {
                    timerRunning = false;
                    timerRemaining = timerStartTime - GetTime();
                }
                desenharRetanguloAlpha(pauseBtnX, btnY, btnW, btnH, !timerRunning ? 0x22FFFFFF : (hPause ? 0xFF00AAFF : 0xAAEAB308));
                DrawRectangleLines(pauseBtnX, btnY, btnW, btnH, !timerRunning ? GRAY : WHITE);
                labelW = medirTexto("PAUSAR", 24);
                desenharTexto("PAUSAR", 24, pauseBtnX + (btnW - labelW)/2, btnY + 22, !timerRunning ? 0xFF888888 : 0xFFFFFFFF);

                // PARAR (reset)
                bool hStop = (mousePos.x >= stopBtnX && mousePos.x <= stopBtnX + btnW && mousePos.y >= btnY && mousePos.y <= btnY + btnH);
                if (hStop && clicked) {
                    timerRunning = false;
                    timerRemaining = 0.0;
                    timerTotalDuration = 0.0;
                }
                desenharRetanguloAlpha(stopBtnX, btnY, btnW, btnH, hStop ? 0xFF888888 : 0xAAFF0000);
                DrawRectangleLines(stopBtnX, btnY, btnW, btnH, WHITE);
                labelW = medirTexto("PARAR", 24);
                desenharTexto("PARAR", 24, stopBtnX + (btnW - labelW)/2, btnY + 22, 0xFFFFFFFF);

                if (alarmFlashTimer > 0.0f) {
                    desenharTexto("!!! TEMPO ESGOTADO !!!", 30, bX + 220, btnY - 70, 0xFFF87171);
                }
            }
            else {
                // ===== GERENCIADOR DE ALARMES (lista) =====
                time_t ta = time(NULL);
                struct tm* tma = localtime(&ta);
                char nowStr[64];
                sprintf(nowStr, "%02d:%02d:%02d", tma->tm_hour, tma->tm_min, tma->tm_sec);
                int nw = medirTexto(nowStr, 90);
                desenharTexto(nowStr, 90, bX + (bW - nw)/2, bY + 40, 0xFFFFFFFF);

                // Painel da lista
                int listX = bX + 40, listY = bY + 170, listW = 760, listH = 470;
                desenharRetanguloAlpha(listX, listY, listW, listH, 0xFF222222);
                DrawRectangleLines(listX, listY, listW, listH, (Color){ 80, 80, 80, 255 });
                desenharTexto("ALARMES", 24, listX + 20, listY + 12, 0xFF00AAFF);
                int rowH = 46;
                int vis = (listH - 50) / rowH;
                for (int i = 0; i < alarmeTotal && i < vis; i++) {
                    int ry = listY + 50 + i * rowH;
                    bool sel = (i == alarmeSel);
                    if (sel) desenharRetanguloAlpha(listX + 8, ry, listW - 16, rowH - 6, 0xFF00AAFF);
                    char linha[64]; sprintf(linha, "%02d:%02d:%02d  %s", alarmes[i].hora, alarmes[i].minuto, alarmes[i].segundo, alarmes[i].ativo ? "ATIVO" : "desl");
                    desenharTexto(linha, 26, listX + 24, ry + 8, sel ? 0xFF000000 : (alarmes[i].ativo ? 0xFFFFFFFF : 0xFF888888));
                    if (!alarmeEditando && clicked && mousePos.x >= listX && mousePos.x <= listX + listW && mousePos.y >= ry && mousePos.y <= ry + rowH) {
                        alarmeSel = i;
                    }
                }
                if (alarmeTotal == 0) desenharTexto("Nenhum alarme. Clique em NOVO.", 20, listX + 24, listY + 60, 0xFF64748B);

                // Editor (quando editando) ou info do selecionado
                if (alarmeEditando && alarmeTotal > 0 && alarmeSel < alarmeTotal) {
                    int edX = bX + 820, edY = bY + 250;
                    AlarmeItem* al = &alarmes[alarmeSel];
                    char hh[8], mm[8], ss[8]; sprintf(hh, "%02d", al->hora); sprintf(mm, "%02d", al->minuto); sprintf(ss, "%02d", al->segundo);
                    int segW2 = 110, sepW2 = 34;
                    desenharTexto("EDITAR HORA", 24, edX, edY - 50, 0xFF00AAFF);
                    int colH = (alarmeEditField == 0) ? 0xFF00AAFF : 0xFFFFFFFF;
                    int colM = (alarmeEditField == 1) ? 0xFF00AAFF : 0xFFFFFFFF;
                    int colS = (alarmeEditField == 2) ? 0xFF00AAFF : 0xFFFFFFFF;
                    desenharTexto(hh, 100, edX, edY, colH);
                    desenharTexto(":", 100, edX + segW2, edY, 0xFFFFFFFF);
                    desenharTexto(mm, 100, edX + segW2 + sepW2, edY, colM);
                    desenharTexto(":", 100, edX + 2 * segW2 + sepW2, edY, 0xFFFFFFFF);
                    desenharTexto(ss, 100, edX + 2 * segW2 + 2 * sepW2, edY, colS);
                    int fldSeg[3]; fldSeg[0] = edX; fldSeg[1] = edX + segW2 + sepW2; fldSeg[2] = edX + 2 * segW2 + 2 * sepW2;
                    for (int f = 0; f < 3; f++) {
                        int cx = fldSeg[f] + segW2 / 2;
                        int upX = cx - 22, upY = edY - 70, arS = 44, arH2 = 38;
                        int dnX = cx - 22, dnY = edY + 150;
                        bool hUp = (mousePos.x >= upX && mousePos.x <= upX + arS && mousePos.y >= upY && mousePos.y <= upY + arH2);
                        bool hDn = (mousePos.x >= dnX && mousePos.x <= dnX + arS && mousePos.y >= dnY && mousePos.y <= dnY + arH2);
                        if (hUp && clicked) { if (f == 0) { al->hora++; if (al->hora > 23) al->hora = 0; } else if (f == 1) { al->minuto++; if (al->minuto > 59) al->minuto = 0; } else { al->segundo++; if (al->segundo > 59) al->segundo = 0; } }
                        if (hDn && clicked) { if (f == 0) { al->hora--; if (al->hora < 0) al->hora = 23; } else if (f == 1) { al->minuto--; if (al->minuto < 0) al->minuto = 59; } else { al->segundo--; if (al->segundo < 0) al->segundo = 59; } }
                        desenharRetanguloAlpha(upX, upY, arS, arH2, hUp ? 0xFF00AAFF : 0xAA333333); DrawRectangleLines(upX, upY, arS, arH2, WHITE);
                        DrawTriangle((Vector2){ (float)cx, (float)(upY + 8) }, (Vector2){ (float)upX + 6, (float)(upY + arH2 - 6) }, (Vector2){ (float)upX + arS - 6, (float)(upY + arH2 - 6) }, WHITE);
                        desenharRetanguloAlpha(dnX, dnY, arS, arH2, hDn ? 0xFF00AAFF : 0xAA333333); DrawRectangleLines(dnX, dnY, arS, arH2, WHITE);
                        DrawTriangle((Vector2){ (float)upX + 6, (float)dnY + 8 }, (Vector2){ (float)upX + arS - 6, (float)dnY + 8 }, (Vector2){ (float)cx, (float)(dnY + arH2 - 6) }, WHITE);
                    }
                    desenharTexto("Use TAB/SETAS <- -> p/ campo e CIMA/BAIXO p/ ajustar", 16, edX, edY + 210, 0xFF64748B);
                } else if (alarmeTotal > 0) {
                    AlarmeItem* al = &alarmes[alarmeSel < alarmeTotal ? alarmeSel : 0];
                    char info[64]; sprintf(info, "Selecionado: %02d:%02d:%02d  %s", al->hora, al->minuto, al->segundo, al->ativo ? "ATIVO" : "desativado");
                    desenharTexto(info, 28, bX + 820, bY + 270, 0xFFAAAAAA);
                }

                // Botoes
                int btnY = bY + 660, btnW = 200, btnH = 60;
                int bx0 = bX + 40;
                bool hN = (mousePos.x >= bx0 && mousePos.x <= bx0 + btnW && mousePos.y >= btnY && mousePos.y <= btnY + btnH);
                if (hN && clicked && alarmeTotal < MAX_ALARMES) {
                    alarmes[alarmeTotal].hora = 0; alarmes[alarmeTotal].minuto = 0; alarmes[alarmeTotal].segundo = 0; alarmes[alarmeTotal].ativo = true; alarmes[alarmeTotal].disparado = false;
                    alarmeTotal++; alarmeSel = alarmeTotal - 1; alarmeEditando = true; alarmeEditField = 0;
                }
                desenharRetanguloAlpha(bx0, btnY, btnW, btnH, hN ? 0xFF00AAFF : 0xAA00FF00); DrawRectangleLines(bx0, btnY, btnW, btnH, WHITE);
                int lw = medirTexto("NOVO", 22); desenharTexto("NOVO", 22, bx0 + (btnW - lw) / 2, btnY + 18, 0xFFFFFFFF);

                int bx1 = bx0 + 220;
                bool hE = (mousePos.x >= bx1 && mousePos.x <= bx1 + btnW && mousePos.y >= btnY && mousePos.y <= btnY + btnH);
                if (hE && clicked && alarmeTotal > 0) { alarmeEditando = !alarmeEditando; alarmeEditField = 0; }
                desenharRetanguloAlpha(bx1, btnY, btnW, btnH, hE ? 0xFF00AAFF : 0xAA333333); DrawRectangleLines(bx1, btnY, btnW, btnH, WHITE);
                lw = medirTexto(alarmeEditando ? "OK" : "EDITAR", 22); desenharTexto(alarmeEditando ? "OK" : "EDITAR", 22, bx1 + (btnW - lw) / 2, btnY + 18, 0xFFFFFFFF);

                int bx2 = bx1 + 220;
                bool hT = (mousePos.x >= bx2 && mousePos.x <= bx2 + btnW && mousePos.y >= btnY && mousePos.y <= btnY + btnH);
                if (hT && clicked && alarmeTotal > 0 && alarmeSel < alarmeTotal) { alarmes[alarmeSel].ativo = !alarmes[alarmeSel].ativo; alarmes[alarmeSel].disparado = false; }
                desenharRetanguloAlpha(bx2, btnY, btnW, btnH, hT ? 0xFF00AAFF : (alarmeTotal > 0 && alarmes[alarmeSel].ativo ? 0xAA00FF00 : 0xAAFF0000)); DrawRectangleLines(bx2, btnY, btnW, btnH, WHITE);
                lw = medirTexto("LIG/DESL", 22); desenharTexto("LIG/DESL", 22, bx2 + (btnW - lw) / 2, btnY + 18, 0xFFFFFFFF);

                int bx3 = bx2 + 220;
                bool hX = (mousePos.x >= bx3 && mousePos.x <= bx3 + btnW && mousePos.y >= btnY && mousePos.y <= btnY + btnH);
                if (hX && clicked && alarmeTotal > 0 && alarmeSel < alarmeTotal) {
                    for (int i = alarmeSel; i < alarmeTotal - 1; i++) alarmes[i] = alarmes[i + 1];
                    alarmeTotal--; if (alarmeSel >= alarmeTotal) alarmeSel = alarmeTotal - 1; if (alarmeSel < 0) alarmeSel = 0;
                    alarmeEditando = false;
                }
                desenharRetanguloAlpha(bx3, btnY, btnW, btnH, hX ? 0xFF00AAFF : 0xAAFF0000); DrawRectangleLines(bx3, btnY, btnW, btnH, WHITE);
                lw = medirTexto("EXCLUIR", 22); desenharTexto("EXCLUIR", 22, bx3 + (btnW - lw) / 2, btnY + 18, 0xFFFFFFFF);

                if (alarmRingTimer > 0.0f) {
                    desenharTexto("!!! ALARME !!!", 50, bX + (bW - medirTexto("!!! ALARME !!!", 50)) / 2, bY + 90, 0xFFF87171);
                }
            }
        }
        else if (timerState == TIMER_STATE_OPEN_EXPLORER) {
            desenharTexto("ABRIR HISTORICO - SELECIONE UM ARQUIVO", 30, bX + 50, bY + 40, 0xFF00AAFF);
            char pathLabel[1024];
            sprintf(pathLabel, "Pasta: %s  |  [ESC] Cancelar  |  Clique no arquivo para carregar", timerCaminhoAtual);
            desenharTexto(pathLabel, 18, bX + 50, bY + 80, 0xFFAAAAAA);

            // Cancel button
            int cncX = bX + bW - 220;
            int cncY = bY + 40;
            int cncW = 170, cncH = 45;
            bool hCancel = (mousePos.x >= cncX && mousePos.x <= cncX + cncW && mousePos.y >= cncY && mousePos.y <= cncY + cncH);
            if (hCancel && clicked) {
                timerState = TIMER_STATE_NORMAL;
            }
            desenharRetanguloAlpha(cncX, cncY, cncW, cncH, hCancel ? 0xFF00AAFF : 0xFF333333);
            DrawRectangleLines(cncX, cncY, cncW, cncH, WHITE);
            int txtW = medirTexto("CANCELAR", 16);
            desenharTexto("CANCELAR", 16, cncX + (cncW - txtW)/2, cncY + 14, 0xFFFFFFFF);

            int listY = bY + 150;
            int itemsPerPage = 12;
            for (int i = timerScroll; i < timerScroll + itemsPerPage && i < timerTotalItens; i++) {
                int itemY = listY + (i - timerScroll) * 45;
                bool isSelected = (timerSel == i);
                bool isHovered = (mousePos.x >= bX + 50 && mousePos.x <= bX + bW - 100 && mousePos.y >= itemY - 5 && mousePos.y <= itemY + 30);

                if (isHovered && clicked) {
                    timerSel = i;
                    if (timerEhPasta[i]) {
                        if (strcmp(timerPaths[i], "..") == 0) {
                            char* lastSlash = strrchr(timerCaminhoAtual, '/');
                            if (!lastSlash) lastSlash = strrchr(timerCaminhoAtual, '\\');
                            if (lastSlash && lastSlash != timerCaminhoAtual) {
                                *lastSlash = '\0';
                                timerListarArquivos(timerCaminhoAtual);
                            }
                        } else {
                            strcpy(timerCaminhoAtual, timerPaths[i]);
                            timerListarArquivos(timerCaminhoAtual);
                        }
                    } else {
                        timerAbrirLaps(timerPaths[i]);
                        timerState = TIMER_STATE_NORMAL;
                    }
                }

                uint32_t textCor = isSelected ? 0xFF00AAFF : (isHovered ? 0xFFFFFFFF : 0xFFBBBBBB);
                if (timerEhPasta[i]) {
                    desenharTexto(timerNomes[i], 22, bX + 60, itemY, textCor);
                } else {
                    desenharTexto(timerNomes[i], 22, bX + 80, itemY, textCor);
                    DrawRectangle(bX + 60, itemY + 6, 8, 8, (Color){ 120, 120, 120, 255 });
                }
            }
            if (timerTotalItens == 0) {
                desenharTexto("Nenhum arquivo de tempos (.txt) encontrado.", 22, bX + 60, listY, 0xFF888888);
            }
        }
        else if (timerState == TIMER_STATE_SAVE_DIALOG) {
            desenharTexto("SALVAR TEMPOS - SELECIONE A PASTA DESTINO", 30, bX + 50, bY + 40, 0xFF00AAFF);
            char pathLabel[1024];
            sprintf(pathLabel, "Pasta: %s  |  [ESC] Cancelar  |  Clique nas pastas na lista para navegar", timerCaminhoAtual);
            desenharTexto(pathLabel, 18, bX + 50, bY + 80, 0xFFAAAAAA);

            // Left explorer panel
            int expW = 1000;
            int listY = bY + 140;
            int itemsPerPage = 12;
            for (int i = timerScroll; i < timerScroll + itemsPerPage && i < timerTotalItens; i++) {
                int itemY = listY + (i - timerScroll) * 45;
                bool isSelected = (timerSel == i);
                bool isHovered = (mousePos.x >= bX + 50 && mousePos.x <= bX + expW - 50 && mousePos.y >= itemY - 5 && mousePos.y <= itemY + 30);

                if (isHovered && clicked) {
                    timerSel = i;
                    if (timerEhPasta[i]) {
                        if (strcmp(timerPaths[i], "..") == 0) {
                            char* lastSlash = strrchr(timerCaminhoAtual, '/');
                            if (!lastSlash) lastSlash = strrchr(timerCaminhoAtual, '\\');
                            if (lastSlash && lastSlash != timerCaminhoAtual) {
                                *lastSlash = '\0';
                                timerListarArquivos(timerCaminhoAtual);
                            }
                        } else {
                            strcpy(timerCaminhoAtual, timerPaths[i]);
                            timerListarArquivos(timerCaminhoAtual);
                        }
                    } else {
                        char tempName[64];
                        strcpy(tempName, timerNomes[i]);
                        char* ext = strstr(tempName, ".txt");
                        if (ext) *ext = '\0';
                        strncpy(timerSaveFilename, tempName, sizeof(timerSaveFilename) - 1);
                        timerSaveFilename[sizeof(timerSaveFilename) - 1] = '\0';
                    }
                }

                uint32_t textCor = isSelected ? 0xFF00AAFF : (isHovered ? 0xFFFFFFFF : 0xFFBBBBBB);
                if (timerEhPasta[i]) {
                    desenharTexto(timerNomes[i], 22, bX + 60, itemY, textCor);
                } else {
                    desenharTexto(timerNomes[i], 22, bX + 80, itemY, textCor);
                    DrawRectangle(bX + 60, itemY + 6, 8, 8, (Color){ 120, 120, 120, 255 });
                }
            }

            // Right Save Card
            int cardX = bX + 1100;
            int cardY = bY + 120;
            int cardW = 520;
            int cardH = 500;

            desenharRetanguloAlpha(cardX, cardY, cardW, cardH, 0xFF2D3139);
            DrawRectangleLines(cardX, cardY, cardW, cardH, (Color){ 100, 100, 100, 255 });

            desenharTexto("SALVAR TEMPOS", 25, cardX + 30, cardY + 30, 0xFF00AAFF);
            desenharTexto("Nome do arquivo abaixo:", 18, cardX + 30, cardY + 100, 0xFFFFFFFF);

            int inpX = cardX + 30;
            int inpY = cardY + 140;
            int inpW = cardW - 60;
            int inpH = 55;
            desenharRetanguloAlpha(inpX, inpY, inpW, inpH, 0xFF151515);
            DrawRectangleLines(inpX, inpY, inpW, inpH, (Color){ 80, 80, 80, 255 });

            char dispName[128];
            if (strlen(timerSaveFilename) == 0) {
                strcpy(dispName, "tempos_cronometro");
                desenharTexto(dispName, 20, inpX + 15, inpY + 18, 0xFF555555);
            } else {
                sprintf(dispName, "%s.txt", timerSaveFilename);
                desenharTexto(dispName, 20, inpX + 15, inpY + 18, 0xFF00FF00);
            }

            // Blinking cursor
            static float timerCursorBlink = 0.0f;
            timerCursorBlink += GetFrameTime();
            if (fmod(timerCursorBlink, 1.0f) < 0.5f) {
                int cursorOffset = strlen(timerSaveFilename) == 0 ? medirTexto("tempos_cronometro", 20) : medirTexto(dispName, 20);
                desenharRetanguloAlpha(inpX + 15 + cursorOffset + 5, inpY + 12, 4, 30, 0xFF00AAFF);
            }

            // Buttons
            int btnW = 180;
            int btnH = 50;
            int cnfX = cardX + 40;
            int cncX = cardX + cardW - btnW - 40;
            int btnY2 = cardY + 240;

            // Confirm
            bool hoverConfirm = (mousePos.x >= cnfX && mousePos.x <= cnfX + btnW && mousePos.y >= btnY2 && mousePos.y <= btnY2 + btnH);
            bool canConfirm = (strlen(timerSaveFilename) > 0);
            if (hoverConfirm && clicked && canConfirm) {
                char fullPath[1024];
                if (!strstr(timerSaveFilename, ".txt")) {
                    sprintf(fullPath, "%s/%s.txt", timerCaminhoAtual, timerSaveFilename);
                } else {
                    sprintf(fullPath, "%s/%s", timerCaminhoAtual, timerSaveFilename);
                }
                timerSalvarLaps(fullPath);
                timerState = TIMER_STATE_NORMAL;
            }
            desenharRetanguloAlpha(cnfX, btnY2, btnW, btnH, canConfirm ? (hoverConfirm ? 0xFF00AAFF : 0xFF333333) : 0xFF222222);
            DrawRectangleLines(cnfX, btnY2, btnW, btnH, canConfirm ? WHITE : (Color){ 60, 60, 60, 255 });
            txtW = medirTexto("CONFIRMAR", 16);
            desenharTexto("CONFIRMAR", 16, cnfX + (btnW - txtW)/2, btnY2 + 16, canConfirm ? 0xFFFFFFFF : 0xFF555555);

            // Cancel
            bool hoverCancel = (mousePos.x >= cncX && mousePos.x <= cncX + btnW && mousePos.y >= btnY2 && mousePos.y <= btnY2 + btnH);
            if (hoverCancel && clicked) {
                timerState = TIMER_STATE_NORMAL;
            }
            desenharRetanguloAlpha(cncX, btnY2, btnW, btnH, hoverCancel ? 0xFF00AAFF : 0xFF333333);
            DrawRectangleLines(cncX, btnY2, btnW, btnH, WHITE);
            txtW = medirTexto("CANCELAR", 16);
            desenharTexto("CANCELAR", 16, cncX + (btnW - txtW)/2, btnY2 + 16, 0xFFFFFFFF);
        }
        else if (timerState == TIMER_STATE_EVENTO_EDIT) {
            int px = 350, py = 50, pw = 1220, ph = 1010;
            desenharRetanguloAlpha(px, py, pw, ph, 0xEE1E2024);
            DrawRectangleLines(px, py, pw, ph, (Color){ 120, 120, 120, 255 });
            char tit[128];
            if (evtEditIndex >= 0) sprintf(tit, "EDITAR EVENTO - %02d/%02d/%04d", calSelDia, calSelMes, calSelAno);
            else sprintf(tit, "NOVO EVENTO - %02d/%02d/%04d", calSelDia, calSelMes, calSelAno);
            desenharTexto(tit, 30, px + 30, py + 20, 0xFF00AAFF);

            int fx = px + 40, fw = pw - 80, fy = py + 80, fh = 40, gap = 18;
            static float evCur = 0.0f; evCur += GetFrameTime();
            // TIPO (Evento / Tarefa)
            desenharTexto("TIPO:", 20, fx, fy - 2, evtField == EVT_TIPO ? 0xFF00AAFF : 0xFFAAAAAA);
            {
                int b1w = 200, b1h = 40;
                int b1x = fx;
                bool hb1 = (mousePos.x>=b1x && mousePos.x<=b1x+b1w && mousePos.y>=fy+20 && mousePos.y<=fy+20+b1h);
                if (hb1 && clicked) { evtTipo = 0; evtField = EVT_TIPO; }
                desenharRetanguloAlpha(b1x, fy+20, b1w, b1h, evtTipo==0 ? 0xFF00AAFF : (evtField==EVT_TIPO?0x33114455:0xFF222222));
                DrawRectangleLines(b1x, fy+20, b1w, b1h, evtField==EVT_TIPO?(Color){0,170,255,255}:(Color){80,80,80,255});
                desenharTexto("EVENTO", 22, b1x + b1w/2 - medirTexto("EVENTO",22)/2, fy+30, evtTipo==0?0xFF000000:0xFFFFFFFF);
                int b2x = b1x + b1w + 16;
                bool hb2 = (mousePos.x>=b2x && mousePos.x<=b2x+b1w && mousePos.y>=fy+20 && mousePos.y<=fy+20+b1h);
                if (hb2 && clicked) { evtTipo = 1; evtField = EVT_TIPO; }
                desenharRetanguloAlpha(b2x, fy+20, b1w, b1h, evtTipo==1 ? 0xFF00AAFF : (evtField==EVT_TIPO?0x33114455:0xFF222222));
                DrawRectangleLines(b2x, fy+20, b1w, b1h, evtField==EVT_TIPO?(Color){0,170,255,255}:(Color){80,80,80,255});
                desenharTexto("TAREFA", 22, b2x + b1w/2 - medirTexto("TAREFA",22)/2, fy+30, evtTipo==1?0xFF000000:0xFFFFFFFF);
            }
            fy += 40 + gap + 14;
            // NOME
            desenharTexto("NOME:", 20, fx, fy - 2, evtField == EVT_NOME ? 0xFF00AAFF : 0xFFAAAAAA);
            desenharRetanguloAlpha(fx, fy + 20, fw, fh, evtField == EVT_NOME ? 0x33114455 : 0xFF151515);
            DrawRectangleLines(fx, fy + 20, fw, fh, evtField == EVT_NOME ? (Color){ 0, 170, 255, 255 } : (Color){ 80, 80, 80, 255 });
            desenharTexto(evtNome, 22, fx + 15, fy + 34, 0xFFFFFFFF);
            if (evtField == EVT_NOME && fmod(evCur, 1.0f) < 0.5f) desenharRetanguloAlpha(fx + 15 + medirTexto(evtNome, 22) + 5, fy + 30, 4, 28, 0xFF00AAFF);
            fy += fh + gap + 14;
            // DESCRICAO
            desenharTexto("DESCRICAO:", 20, fx, fy - 2, evtField == EVT_DESC ? 0xFF00AAFF : 0xFFAAAAAA);
            desenharRetanguloAlpha(fx, fy + 20, fw, fh, evtField == EVT_DESC ? 0x33114455 : 0xFF151515);
            DrawRectangleLines(fx, fy + 20, fw, fh, evtField == EVT_DESC ? (Color){ 0, 170, 255, 255 } : (Color){ 80, 80, 80, 255 });
            desenharTexto(evtDesc, 22, fx + 15, fy + 34, 0xFFFFFFFF);
            if (evtField == EVT_DESC && fmod(evCur, 1.0f) < 0.5f) desenharRetanguloAlpha(fx + 15 + medirTexto(evtDesc, 22) + 5, fy + 30, 4, 28, 0xFF00AAFF);
            fy += fh + gap + 14;
            // EMAILS
            desenharTexto("EMAILS (separados por ;):", 20, fx, fy - 2, evtField == EVT_EMAILS ? 0xFF00AAFF : 0xFFAAAAAA);
            desenharRetanguloAlpha(fx, fy + 20, fw, fh, evtField == EVT_EMAILS ? 0x33114455 : 0xFF151515);
            DrawRectangleLines(fx, fy + 20, fw, fh, evtField == EVT_EMAILS ? (Color){ 0, 170, 255, 255 } : (Color){ 80, 80, 80, 255 });
            desenharTexto(evtEmails, 22, fx + 15, fy + 34, 0xFFFFFFFF);
            if (evtField == EVT_EMAILS && fmod(evCur, 1.0f) < 0.5f) desenharRetanguloAlpha(fx + 15 + medirTexto(evtEmails, 22) + 5, fy + 30, 4, 28, 0xFF00AAFF);
            fy += fh + gap + 14;
            // TELEFONES
            desenharTexto("TELEFONES (separados por ;):", 20, fx, fy - 2, evtField == EVT_PHONES ? 0xFF00AAFF : 0xFFAAAAAA);
            desenharRetanguloAlpha(fx, fy + 20, fw, fh, evtField == EVT_PHONES ? 0x33114455 : 0xFF151515);
            DrawRectangleLines(fx, fy + 20, fw, fh, evtField == EVT_PHONES ? (Color){ 0, 170, 255, 255 } : (Color){ 80, 80, 80, 255 });
            desenharTexto(evtPhones, 22, fx + 15, fy + 34, 0xFFFFFFFF);
            if (evtField == EVT_PHONES && fmod(evCur, 1.0f) < 0.5f) desenharRetanguloAlpha(fx + 15 + medirTexto(evtPhones, 22) + 5, fy + 30, 4, 28, 0xFF00AAFF);
            fy += fh + gap + 14;
            // HORA e TERMINO (mesma linha)
            desenharTexto("HORARIO:", 18, fx, fy - 2, evtField == EVT_HORA ? 0xFF00AAFF : 0xFFAAAAAA);
            char hh[8], mm[8]; sprintf(hh, "%02d", evtHora); sprintf(mm, "%02d", evtMinuto);
            desenharRetanguloAlpha(fx, fy + 16, 120, 40, evtField == EVT_HORA && evtHoraField == 0 ? 0x33114455 : 0xFF151515);
            DrawRectangleLines(fx, fy+16, 120, 40, evtField==EVT_HORA&&evtHoraField==0?(Color){0,170,255,255}:(Color){80,80,80,255});
            desenharTexto(hh, 26, fx+38, fy+24, 0xFFFFFFFF);
            desenharTexto(":", 26, fx+120, fy+24, 0xFFFFFFFF);
            desenharRetanguloAlpha(fx+140, fy+16, 120, 40, evtField==EVT_HORA&&evtHoraField==1?0x33114455:0xFF151515);
            DrawRectangleLines(fx+140, fy+16, 120, 40, evtField==EVT_HORA&&evtHoraField==1?(Color){0,170,255,255}:(Color){80,80,80,255});
            desenharTexto(mm, 26, fx+178, fy+24, 0xFFFFFFFF);
            int tx2 = fx + 470;
            desenharTexto("TERMINO (data H:M):", 16, tx2, fy - 2, evtField == EVT_HORAF ? 0xFF00AAFF : 0xFFAAAAAA);
            {
                char dS[8], mS[8], aS[8], hS[8], miS[8];
                sprintf(dS, "%02d", evtDiaF); sprintf(mS, "%02d", evtMesF); sprintf(aS, "%04d", evtAnoF);
                sprintf(hS, "%02d", evtHoraF); sprintf(miS, "%02d", evtMinF);
                int segW = 44; int txx = tx2;
                int boxes[5][3];
                desenharRetanguloAlpha(txx, fy+16, segW, 40, evtField==EVT_HORAF&&evtHoraFField==0?0x33114455:0xFF151515);
                DrawRectangleLines(txx, fy+16, segW, 40, evtField==EVT_HORAF&&evtHoraFField==0?(Color){0,170,255,255}:(Color){80,80,80,255});
                desenharTexto(dS, 22, txx+10, fy+24, 0xFFFFFFFF); boxes[0][0]=txx; boxes[0][1]=fy+16; boxes[0][2]=segW; txx+=segW+4;
                desenharTexto("/", 20, txx, fy+24, 0xFF888888); txx+=12;
                desenharRetanguloAlpha(txx, fy+16, segW, 40, evtField==EVT_HORAF&&evtHoraFField==1?0x33114455:0xFF151515);
                DrawRectangleLines(txx, fy+16, segW, 40, evtField==EVT_HORAF&&evtHoraFField==1?(Color){0,170,255,255}:(Color){80,80,80,255});
                desenharTexto(mS, 22, txx+10, fy+24, 0xFFFFFFFF); boxes[1][0]=txx; boxes[1][1]=fy+16; boxes[1][2]=segW; txx+=segW+4;
                desenharTexto("/", 20, txx, fy+24, 0xFF888888); txx+=12;
                desenharRetanguloAlpha(txx, fy+16, 66, 40, evtField==EVT_HORAF&&evtHoraFField==2?0x33114455:0xFF151515);
                DrawRectangleLines(txx, fy+16, 66, 40, evtField==EVT_HORAF&&evtHoraFField==2?(Color){0,170,255,255}:(Color){80,80,80,255});
                desenharTexto(aS, 20, txx+6, fy+26, 0xFFFFFFFF); boxes[2][0]=txx; boxes[2][1]=fy+16; boxes[2][2]=66; txx+=66+8;
                desenharRetanguloAlpha(txx, fy+16, segW, 40, evtField==EVT_HORAF&&evtHoraFField==3?0x33114455:0xFF151515);
                DrawRectangleLines(txx, fy+16, segW, 40, evtField==EVT_HORAF&&evtHoraFField==3?(Color){0,170,255,255}:(Color){80,80,80,255});
                desenharTexto(hS, 22, txx+10, fy+24, 0xFFFFFFFF); boxes[3][0]=txx; boxes[3][1]=fy+16; boxes[3][2]=segW; txx+=segW+4;
                desenharTexto(":", 22, txx, fy+24, 0xFF888888); txx+=6;
                desenharRetanguloAlpha(txx, fy+16, segW, 40, evtField==EVT_HORAF&&evtHoraFField==4?0x33114455:0xFF151515);
                DrawRectangleLines(txx, fy+16, segW, 40, evtField==EVT_HORAF&&evtHoraFField==4?(Color){0,170,255,255}:(Color){80,80,80,255});
                desenharTexto(miS, 22, txx+10, fy+24, 0xFFFFFFFF); boxes[4][0]=txx; boxes[4][1]=fy+16; boxes[4][2]=segW;
                if (clicked) for (int bI = 0; bI < 5; bI++)
                    if (mousePos.x>=boxes[bI][0] && mousePos.x<=boxes[bI][0]+boxes[bI][2] && mousePos.y>=boxes[bI][1] && mousePos.y<=boxes[bI][1]+40) { evtField = EVT_HORAF; evtHoraFField = bI; }
            }
            fy += 40 + gap + 14;
            // COR
            desenharTexto("COR:", 18, fx, fy - 2, evtField == EVT_COR ? 0xFF00AAFF : 0xFFAAAAAA);
            { int sx = fx + 60, sw = 40, sgap = 6;
              for (int c = 0; c < MAX_CORES; c++) { int ry = fy + 12; Color cc = evtCorColor(c);
                DrawRectangle(sx, ry, sw, sw, cc);
                if (evtCor == c) DrawRectangleLines(sx - 2, ry - 2, sw + 4, sw + 4, (Color){ 0, 170, 255, 255 });
                if (clicked && mousePos.x>=sx && mousePos.x<=sx+sw && mousePos.y>=ry && mousePos.y<=ry+sw) { evtCor = c; evtField = EVT_COR; }
                sx += sw + sgap; } }
            fy += 40 + gap + 14;
            // REPETIR (linha unica)
            desenharTexto("REPETIR:", 18, fx, fy - 2, evtField == EVT_REP ? 0xFF00AAFF : 0xFFAAAAAA);
            {
                int rbx = fx + 90; bool hRp = (mousePos.x>=rbx && mousePos.x<=rbx+26 && mousePos.y>=fy-4 && mousePos.y<=fy+28);
                if (hRp && clicked) { evtRep = (evtRep + MAX_REP - 1) % MAX_REP; evtField = EVT_REP; }
                desenharRetanguloAlpha(rbx, fy, 26, 26, hRp?0xFF00AAFF:0xAA333333); DrawRectangleLines(rbx, fy, 26, 26, WHITE); desenharTexto("<", 18, rbx+7, fy+4, 0xFFFFFFFF);
                int rnmX = rbx + 34; desenharTexto(repNomes[evtRep], 22, rnmX, fy+3, 0xFF00FF00);
                int rnbX = rnmX + medirTexto(repNomes[evtRep], 22) + 6; bool hRn = (mousePos.x>=rnbX && mousePos.x<=rnbX+26 && mousePos.y>=fy-4 && mousePos.y<=fy+28);
                if (hRn && clicked) { evtRep = (evtRep + 1) % MAX_REP; evtField = EVT_REP; }
                desenharRetanguloAlpha(rnbX, fy, 26, 26, hRn?0xFF00AAFF:0xAA333333); DrawRectangleLines(rnbX, fy, 26, 26, WHITE); desenharTexto(">", 18, rnbX+7, fy+4, 0xFFFFFFFF);
                if (evtRep == 4) {
                    int dX = rnbX + 40; desenharTexto("a cada", 16, dX, fy+4, 0xFFAAAAAA);
                    int dvX = dX + 56; bool hDp = (mousePos.x>=dvX && mousePos.x<=dvX+22 && mousePos.y>=fy-4 && mousePos.y<=fy+28);
                    if (hDp && clicked) { evtRepDias = (evtRepDias + 364) % 365 + 1; evtField = EVT_REPDIAS; }
                    desenharRetanguloAlpha(dvX, fy, 22, 26, hDp?0xFF00AAFF:0xAA333333); DrawRectangleLines(dvX, fy, 22, 26, WHITE); desenharTexto("-", 18, dvX+6, fy+4, 0xFFFFFFFF);
                    int dvnX = dvX + 26; char rds[12]; sprintf(rds, "%d", evtRepDias); desenharTexto(rds, 22, dvnX, fy+3, 0xFFFFFFFF);
                    int dvn2 = dvnX + medirTexto(rds, 22) + 4; bool hDn = (mousePos.x>=dvn2 && mousePos.x<=dvn2+22 && mousePos.y>=fy-4 && mousePos.y<=fy+28);
                    if (hDn && clicked) { evtRepDias = (evtRepDias % 365) + 1; evtField = EVT_REPDIAS; }
                    desenharRetanguloAlpha(dvn2, fy, 22, 26, hDn?0xFF00AAFF:0xAA333333); DrawRectangleLines(dvn2, fy, 22, 26, WHITE); desenharTexto("+", 18, dvn2+6, fy+4, 0xFFFFFFFF);
                    desenharTexto("dias", 16, dvn2 + 26, fy+4, 0xFFAAAAAA);
                }
            }
            fy += 40 + gap + 14;
            // NOTIF
            desenharTexto("NOTIFICAR (ENTER/ESPACO marca, SETAS navegam):", 18, fx, fy - 2, evtField == EVT_NOTIF ? 0xFF00AAFF : 0xFFAAAAAA);
            for (int k = 0; k < MAX_NOTIF_OPCOES; k++) {
                int ny = fy + 14 + k * 26;
                bool sel = (evtField == EVT_NOTIF && evtNotifSel == k);
                bool on = (evtNotifMask & (1 << k));
                if (sel) desenharRetanguloAlpha(fx, ny, fw, 22, 0x33114455);
                desenharTexto(sel ? ">" : " ", 16, fx + 5, ny + 2, 0xFF00AAFF);
                desenharTexto(on ? "[x]" : "[ ]", 18, fx + 26, ny + 1, on ? 0xFF00FF00 : 0xFF888888);
                desenharTexto(notifNomes[k], 18, fx + 74, ny + 1, 0xFFFFFFFF);
            }
            fy += 14 + MAX_NOTIF_OPCOES * 26 + gap;
            // AGENDA (seletor)
            desenharTexto("AGENDA:", 18, fx, fy - 2, 0xFFAAAAAA);
            {
                int agX = fx + 80; int pbx = agX; bool hPb = (mousePos.x>=pbx && mousePos.x<=pbx+26 && mousePos.y>=fy-4 && mousePos.y<=fy+26);
                if (hPb && clicked) ferramentas_trocar_agenda((agendaAtual + agendasTotal - 1) % agendasTotal);
                desenharRetanguloAlpha(pbx, fy, 26, 24, hPb?0xFF00AAFF:0xAA333333); DrawRectangleLines(pbx, fy, 26, 24, WHITE); desenharTexto("<", 18, pbx+7, fy+3, 0xFFFFFFFF);
                int nmX = pbx + 34; int nmW = medirTexto(agendas[agendaAtual].nome, 22); desenharTexto(agendas[agendaAtual].nome, 22, nmX, fy+2, 0xFF00AAFF);
                int nbx = nmX + nmW + 6; bool hNb = (mousePos.x>=nbx && mousePos.x<=nbx+26 && mousePos.y>=fy-4 && mousePos.y<=fy+26);
                if (hNb && clicked) ferramentas_trocar_agenda((agendaAtual + 1) % agendasTotal);
                desenharRetanguloAlpha(nbx, fy, 26, 24, hNb?0xFF00AAFF:0xAA333333); DrawRectangleLines(nbx, fy, 26, 24, WHITE); desenharTexto(">", 18, nbx+7, fy+3, 0xFFFFFFFF);
            }
            fy += 30 + gap;
            int bbY = py + ph - 70, sbW = 220, sbH = 50, sbX = px + 40;
            bool hSav = (mousePos.x >= sbX && mousePos.x <= sbX + sbW && mousePos.y >= bbY && mousePos.y <= bbY + sbH);
            if (hSav && clicked) {
                if (evtNome[0]) {
                    if (evtDiaF == 0) { evtDiaF = calSelDia; evtMesF = calSelMes; evtAnoF = calSelAno; evtHoraF = 0; evtMinF = 0; }
                    if (evtEditIndex >= 0) {
                        EventoItem* e = &eventos[evtEditIndex];
                        strcpy(e->nome, evtNome); strcpy(e->desc, evtDesc); strcpy(e->emails, evtEmails); strcpy(e->phones, evtPhones);
                        e->hora = evtHora; e->minuto = evtMinuto; e->segundo = 0; e->ano = calSelAno; e->mes = calSelMes; e->dia = calSelDia; e->notifMask = evtNotifMask;
                        e->anoF = evtAnoF; e->mesF = evtMesF; e->diaF = evtDiaF; e->horaF = evtHoraF; e->minutoF = evtMinF; e->segundoF = 0;
                        e->cor = evtCor; e->rep = evtRep; e->repDias = evtRepDias; e->tipo = evtTipo;
                    } else if (eventosTotal < MAX_EVENTOS) {
                        EventoItem* e = &eventos[eventosTotal];
                        e->ano = calSelAno; e->mes = calSelMes; e->dia = calSelDia;
                        e->hora = evtHora; e->minuto = evtMinuto; e->segundo = 0;
                        e->anoF = evtAnoF; e->mesF = evtMesF; e->diaF = evtDiaF; e->horaF = evtHoraF; e->minutoF = evtMinF; e->segundoF = 0;
                        e->cor = evtCor; e->rep = evtRep; e->repDias = evtRepDias; e->tipo = evtTipo;
                        strcpy(e->nome, evtNome); strcpy(e->desc, evtDesc); strcpy(e->emails, evtEmails); strcpy(e->phones, evtPhones);
                        e->notifMask = evtNotifMask;
                        eventosTotal++;
                    }
                    ferramentas_salvar_eventos();
                    timerState = TIMER_STATE_NORMAL;
                }
            }
            desenharRetanguloAlpha(sbX, bbY, sbW, sbH, hSav ? 0xFF00AAFF : 0xAA00AA00); DrawRectangleLines(sbX, bbY, sbW, sbH, WHITE);
            desenharTexto("SALVAR", 20, sbX + sbW/2 - 30, bbY + 15, 0xFFFFFFFF);
            int cbW = 220, cbX = px + pw - 40 - cbW;
            bool hCan = (mousePos.x >= cbX && mousePos.x <= cbX + cbW && mousePos.y >= bbY && mousePos.y <= bbY + sbH);
            if (hCan && clicked) timerState = TIMER_STATE_NORMAL;
            desenharRetanguloAlpha(cbX, bbY, cbW, sbH, hCan ? 0xFF00AAFF : 0xAAFF0000); DrawRectangleLines(cbX, bbY, cbW, sbH, WHITE);
            desenharTexto("CANCELAR", 20, cbX + cbW/2 - 40, bbY + 15, 0xFFFFFFFF);
        }
    }

    // Overlay do alarme tocando (+10 MIN / PARAR)
    if (alarmRingTimer > 0.0f) {
        int ow = 900, oh = 420;
        int ox = (1920 - ow) / 2, oy = (1080 - oh) / 2;
        desenharRetanguloAlpha(ox, oy, ow, oh, 0xEE111111);
        DrawRectangleLines(ox, oy, ow, oh, (Color){ 255, 80, 80, 255 });
        desenharTexto("!!! ALARME !!!", 60, ox + (ow - medirTexto("!!! ALARME !!!", 60))/2, oy + 50, 0xFFF87171);
        char quem[128];
        if (alarmeTocando >= 0 && alarmeTocando < alarmeTotal) {
            sprintf(quem, "Hora: %02d:%02d:%02d", alarmes[alarmeTocando].hora, alarmes[alarmeTocando].minuto, alarmes[alarmeTocando].segundo);
        } else {
            strcpy(quem, "Hora programada");
        }
        desenharTexto(quem, 36, ox + (ow - medirTexto(quem, 36))/2, oy + 150, 0xFFFFFFFF);

        int bW2 = 320, bH2 = 90; int bY2 = oy + 260;
        int sX = ox + 70, pX = ox + ow - 70 - bW2;
        bool hS = (mousePos.x>=sX && mousePos.x<=sX+bW2 && mousePos.y>=bY2 && mousePos.y<=bY2+bH2);
        if (hS && clicked) {
            if (alarmeTocando >= 0 && alarmeTocando < alarmeTotal) {
                alarmes[alarmeTocando].minuto += 10;
                if (alarmes[alarmeTocando].minuto > 59) { alarmes[alarmeTocando].minuto -= 60; alarmes[alarmeTocando].hora = (alarmes[alarmeTocando].hora + 1) % 24; }
                alarmes[alarmeTocando].disparado = false;
            }
            alarmRingTimer = 0.0f; alarmeTocando = -1;
        }
        desenharRetanguloAlpha(sX, bY2, bW2, bH2, hS?0xFF00AAFF:0xAA00AAFF); DrawRectangleLines(sX,bY2,bW2,bH2,WHITE);
        int sW = medirTexto("+ 10 MIN", 30); desenharTexto("+ 10 MIN", 30, sX + (bW2 - sW)/2, bY2 + 30, 0xFFFFFFFF);

        bool hP = (mousePos.x>=pX && mousePos.x<=pX+bW2 && mousePos.y>=bY2 && mousePos.y<=bY2+bH2);
        if (hP && clicked) { alarmRingTimer = 0.0f; alarmeTocando = -1; }
        desenharRetanguloAlpha(pX, bY2, bW2, bH2, hP?0xFF00AAFF:0xAAFF3333); DrawRectangleLines(pX,bY2,bW2,bH2,WHITE);
        int pW = medirTexto("PARAR", 30); desenharTexto("PARAR", 30, pX + (bW2 - pW)/2, bY2 + 30, 0xFFFFFFFF);
    }

    // Banner de notificacao de eventos
    if (eventoBannerTimer > 0.0f) {
        eventoBannerTimer -= GetFrameTime();
        int bw = 620, bh = 90, bx = (1920 - bw)/2, by = 40;
        desenharRetanguloAlpha(bx, by, bw, bh, 0xEE222200);
        DrawRectangleLines(bx, by, bw, bh, (Color){ 255, 200, 0, 255 });
        desenharTexto("LEMBRETE", 22, bx + 20, by + 12, 0xFFFFD000);
        desenharTexto(eventoBannerMsg, 24, bx + 20, by + 45, 0xFFFFFFFF);
    }

    // =================== CONTATOS ===================
    #define C32(c) ((Color){ (uint8_t)(((c) >> 16) & 0xFF), (uint8_t)(((c) >> 8) & 0xFF), (uint8_t)((c) & 0xFF), 255 })
    else if (toolMode == TOOL_CONTATOS) {
        desenharTexto("CONTATOS", 38, 100, 60, 0xFF00AAFF);
        desenharTexto("[ESC] Menu Anterior  |  [NOVO] adiciona contato", 20, 100, 110, 0xFFAAAAAA);

        int vltX = 1600, vltY = 80, vltW = 220, vltH = 50;
        bool hoverVoltar = (mousePos.x >= vltX && mousePos.x <= vltX + vltW && mousePos.y >= vltY && mousePos.y <= vltY + vltH);
        if (hoverVoltar && clicked) { if (contatoEditando) contatoEditando = false; else toolMode = TOOL_MENU; }
        desenharBotaoVoltarIcone(vltX, vltY, vltW, vltH, hoverVoltar);

        // ---- Gerenciador de catalogo (overlay) ----
        if (gerenciaModo != 0) {
            desenharRetanguloAlpha(0, 0, 1920, 1080, 0xCC000000);
            bool isTag = (gerenciaModo == 1);
            int n = isTag ? tagTotal : campoTotal;
            desenharTexto(isTag ? "GERENCIAR TAGS" : "GERENCIAR CAMPOS", 36, 100, 130, 0xFF00AAFF);
            desenharTexto("[ESC] fechar  |  clique em RENOMEAR/EXCLUIR  |  + NOVO cria", 20, 100, 175, 0xFFAAAAAA);
            int ly = 220, rh = 50, gap = 10;
            for (int i = 0; i < n; i++) {
                char* nm = isTag ? tagCatalog[i] : campoCatalog[i];
                int ry = ly + i * (rh + gap);
                desenharRetanguloAlpha(300, ry, 760, rh, (i == gerSel) ? 0x2233AAFF : 0x0CFFFFFF);
                DrawRectangleLines(300, ry, 760, rh, C32((i == gerSel) ? 0xFF00AAFF : 0xFF333333));
                desenharTexto(nm, 20, 320, ry + 16, 0xFFFFFFFF);
                int rbX = 1100, reX = 1320, bw = 200;
                bool hb = (mousePos.x >= rbX && mousePos.x <= rbX + bw && mousePos.y >= ry && mousePos.y <= ry + rh);
                bool he = (mousePos.x >= reX && mousePos.x <= reX + bw && mousePos.y >= ry && mousePos.y <= ry + rh);
                desenharRetanguloAlpha(rbX, ry, bw, rh, hb ? 0xFF00AAFF : 0xFF333333); DrawRectangleLines(rbX, ry, bw, rh, WHITE);
                desenharTexto("RENOMEAR", 18, rbX + 45, ry + 16, 0xFFFFFFFF);
                desenharRetanguloAlpha(reX, ry, bw, rh, he ? 0xFFAA3333 : 0xFF333333); DrawRectangleLines(reX, ry, bw, rh, WHITE);
                desenharTexto("EXCLUIR", 18, reX + 50, ry + 16, 0xFFFFFFFF);
            }
            int boxY = 940;
            desenharRetanguloAlpha(300, boxY, 760, 45, gerEditando ? 0xFF444444 : 0xFF222222);
            DrawRectangleLines(300, boxY, 760, 45, C32(gerEditando ? 0xFF00AAFF : 0xFF555555));
            desenharTexto(gerEditando ? gerNome : "(nome - digite e ENTER para aplicar)", 18, 315, boxY + 13, gerEditando ? 0xFFFFFFFF : 0xFF888888);
            int nvX = 1100, nvY = boxY, nvW = 200, nvH = 45;
            bool hnv = (mousePos.x >= nvX && mousePos.x <= nvX + nvW && mousePos.y >= nvY && mousePos.y <= nvY + nvH);
            desenharRetanguloAlpha(nvX, nvY, nvW, nvH, hnv ? 0xFF00AAFF : 0xFF333333); DrawRectangleLines(nvX, nvY, nvW, nvH, WHITE);
            desenharTexto("+ NOVO", 18, nvX + 55, nvY + 14, 0xFFFFFFFF);
        }
        // ---- Lista de contatos ----
        else if (!contatoEditando) {
            const char* sorts[4] = { "Ordenar: Alfabetico", "Ordenar: Data", "Ordenar: Tag", "Ordenar: Campo" };
            int sbX = 100, sbY = 200, sbW = 300, sbH = 45;
            for (int si = 0; si < 4; si++) {
                int bx = sbX + si * (sbW + 15);
                bool h = (mousePos.x >= bx && mousePos.x <= bx + sbW && mousePos.y >= sbY && mousePos.y <= sbY + sbH);
                desenharRetanguloAlpha(bx, sbY, sbW, sbH, (si == contatoSort) ? 0xFF00AAFF : (h ? 0xFF444444 : 0xFF222222));
                DrawRectangleLines(bx, sbY, sbW, sbH, C32((si == contatoSort) ? 0xFF00AAFF : (h ? 0xFF888888 : 0xFF555555)));
                desenharTexto(sorts[si], 18, bx + 14, sbY + 12, (si == contatoSort) ? 0xFF000000 : 0xFFFFFFFF);
            }
            int tfX = 100, tfY = 260, tfW = 615, tfH = 40;
            bool htf = (mousePos.x >= tfX && mousePos.x <= tfX + tfW && mousePos.y >= tfY && mousePos.y <= tfY + tfH);
            desenharRetanguloAlpha(tfX, tfY, tfW, tfH, htf ? 0xFF444444 : 0xFF222222);
            DrawRectangleLines(tfX, tfY, tfW, tfH, C32(contatoFiltroEdit ? 0xFF00AAFF : 0xFF555555));
            char filtrotxt[80];
            snprintf(filtrotxt, sizeof(filtrotxt), "Filtrar por tag: %s", contatoFiltroTag);
            desenharTexto(filtrotxt, 18, tfX + 10, tfY + 10, contatoFiltroTag[0] ? 0xFFFFFFFF : 0xFF888888);

            int nvX = 1700, nvY = 200, nvW = 200, nvH = 45;
            bool hnv = (mousePos.x >= nvX && mousePos.x <= nvX + nvW && mousePos.y >= nvY && mousePos.y <= nvY + nvH);
            desenharRetanguloAlpha(nvX, nvY, nvW, nvH, hnv ? 0xFF00AAFF : 0xFF333333);
            DrawRectangleLines(nvX, nvY, nvW, nvH, WHITE);
            desenharTexto("+ NOVO", 20, nvX + 50, nvY + 12, 0xFFFFFFFF);

            int nVis = ferramentas_contatos_ordem(contatoOrd);
            int listX = 100, listY = 320, rowH = 60;
            if (nVis == 0) desenharTexto("Nenhum contato. Clique em + NOVO para adicionar.", 22, 120, 360, 0xFF888888);
            for (int vi = 0; vi < nVis; vi++) {
                int ry = listY + vi * rowH - contatoScroll;
                if (ry < listY - rowH || ry > 1080) continue;
                int idx = contatoOrd[vi];
                ContactItem* c = &contatos[idx];
                bool sel = (vi == contatoSel);
                bool h = (mousePos.x >= listX && mousePos.x <= listX + 1720 && mousePos.y >= ry && mousePos.y <= ry + rowH - 6);
                desenharRetanguloAlpha(listX, ry, 1720, rowH - 6, sel ? 0x33FFFFFF : (h ? 0x18FFFFFF : 0x0CFFFFFF));
                DrawRectangleLines(listX, ry, 1720, rowH - 6, C32(sel ? 0xFF00AAFF : (h ? 0xFF888888 : 0xFF333333)));
                desenharTexto(c->nome[0] ? c->nome : "(sem nome)", 24, listX + 20, ry + 8, 0xFFFFFFFF);
                desenharTexto(c->telefone, 18, listX + 20, ry + 36, 0xFFAAAAAA);
                if (c->tags[0]) desenharTexto(c->tags, 16, listX + 600, ry + 38, 0xFF66CCFF);
                char adic[64]; struct tm* at = localtime(&c->adicionado);
                if (at) strftime(adic, sizeof(adic), "Adic: %d/%m/%y %H:%M", at); else adic[0] = 0;
                desenharTexto(adic, 14, listX + 1300, ry + 38, 0xFF777777);
            }
        }
        // ---- Editor de contato ----
        else {
            int yy = 170 - contatoEditScroll;
            int bh = 46, gp = 8;
            const char* basLabels[3] = { "Nome", "Telefone", "E-mails" };
            char* basVals[3] = { contatoBuf.nome, contatoBuf.telefone, contatoBuf.emails };
            for (int i = 0; i < 3; i++) {
                bool active = (contatoField == i);
                desenharRetanguloAlpha(100, yy, 1720, bh, active ? 0x22FFFFFF : 0x0CFFFFFF);
                DrawRectangleLines(100, yy, 1720, bh, C32(active ? 0xFF00AAFF : 0xFF333333));
                desenharTexto(basLabels[i], 18, 120, yy + 14, active ? 0xFF00AAFF : 0xFF888888);
                desenharTexto(basVals[i], 20, 360, yy + 14, 0xFFFFFFFF);
                yy += bh + gp;
            }
            yy += 14;
            desenharTexto("TAGS  (clique para marcar/desmarcar)", 20, 100, yy, 0xFF00AAFF); yy += 30;
            for (int i = 0; i < tagTotal; i++) {
                bool on = contato_tag_tem(&contatoBuf, tagCatalog[i]);
                desenharRetanguloAlpha(100, yy, 1720, bh, on ? 0x2233AAFF : 0x0CFFFFFF);
                DrawRectangleLines(100, yy, 1720, bh, C32(on ? 0xFF00AAFF : 0xFF333333));
                desenharTexto(on ? "[X]" : "[ ]", 20, 120, yy + 14, on ? 0xFF00AAFF : 0xFFFFFFFF);
                desenharTexto(tagCatalog[i], 20, 180, yy + 14, 0xFFFFFFFF);
                yy += bh + gp;
            }
            yy += 14;
            desenharTexto("CAMPOS PERSONALIZADOS", 20, 100, yy, 0xFF00AAFF); yy += 30;
            for (int i = 0; i < campoTotal; i++) {
                int s = contato_slot_label(&contatoBuf, campoCatalog[i]);
                bool active = (contatoField == 3 + i);
                desenharRetanguloAlpha(100, yy, 1720, bh, active ? 0x22FFFFFF : 0x0CFFFFFF);
                DrawRectangleLines(100, yy, 1720, bh, C32(active ? 0xFF00AAFF : 0xFF333333));
                desenharTexto(campoCatalog[i], 18, 120, yy + 14, 0xFF888888);
                desenharTexto(s >= 0 ? contatoBuf.cv[s] : "", 20, 360, yy + 14, 0xFFFFFFFF);
                yy += bh + gp;
            }
            int gtx = 1180, gty = 80, gtw = 200, gth = 45;
            bool hgt = (mousePos.x >= gtx && mousePos.x <= gtx + gtw && mousePos.y >= gty && mousePos.y <= gty + gth);
            desenharRetanguloAlpha(gtx, gty, gtw, gth, hgt ? 0xFF00AAFF : 0xFF333333); DrawRectangleLines(gtx, gty, gtw, gth, WHITE);
            desenharTexto("GER. TAGS", 18, gtx + 45, gty + 14, 0xFFFFFFFF);
            int gcx = 1400, gcy = 80, gcw = 200, gch = 45;
            bool hgc = (mousePos.x >= gcx && mousePos.x <= gcx + gcw && mousePos.y >= gcy && mousePos.y <= gcy + gch);
            desenharRetanguloAlpha(gcx, gcy, gcw, gch, hgc ? 0xFF00AAFF : 0xFF333333); DrawRectangleLines(gcx, gcy, gcw, gch, WHITE);
            desenharTexto("GER. CAMPOS", 16, gcx + 38, gcy + 14, 0xFFFFFFFF);
            desenharTexto("[TAB] proximo campo  |  GER. criar/editar/excluir tags e campos", 16, 100, 945, 0xFF777777);
            int svX = 1300, svY = 1000, svW = 200, svH = 50;
            bool hsv = (mousePos.x >= svX && mousePos.x <= svX + svW && mousePos.y >= svY && mousePos.y <= svY + svH);
            desenharRetanguloAlpha(svX, svY, svW, svH, hsv ? 0xFF00AAFF : 0xFF333333);
            DrawRectangleLines(svX, svY, svW, svH, WHITE);
            desenharTexto("SALVAR", 20, svX + 60, svY + 16, 0xFFFFFFFF);
            int cxX = 1520, cxY = 1000, cxW = 200, cxH = 50;
            bool hcx = (mousePos.x >= cxX && mousePos.x <= cxX + cxW && mousePos.y >= cxY && mousePos.y <= cxY + cxH);
            desenharRetanguloAlpha(cxX, cxY, cxW, cxH, hcx ? 0xFFAA3333 : 0xFF333333);
            DrawRectangleLines(cxX, cxY, cxW, cxH, WHITE);
            desenharTexto("CANCELAR", 18, cxX + 45, cxY + 16, 0xFFFFFFFF);
            int dlX = 100, dlY = 1000, dlW = 200, dlH = 50;
            bool hdl = (mousePos.x >= dlX && mousePos.x <= dlX + dlW && mousePos.y >= dlY && mousePos.y <= dlY + dlH);
            desenharRetanguloAlpha(dlX, dlY, dlW, dlH, hdl ? 0xFFAA3333 : 0xFF333333);
            DrawRectangleLines(dlX, dlY, dlW, dlH, WHITE);
            desenharTexto("EXCLUIR", 18, dlX + 45, dlY + 16, 0xFFFFFFFF);
        }
    }
    // =================== CAIXA DE ENTRADA ===================
    else if (toolMode == TOOL_CAIXA) {
        desenharTexto("CAIXA DE ENTRADA", 38, 100, 60, 0xFF00AAFF);
        desenharTexto("[ESC] Menu Anterior", 20, 100, 110, 0xFFAAAAAA);

        int vltX = 1600, vltY = 80, vltW = 220, vltH = 50;
        bool hoverVoltar = (mousePos.x >= vltX && mousePos.x <= vltX + vltW && mousePos.y >= vltY && mousePos.y <= vltY + vltH);
        if (hoverVoltar && clicked) {
            if (msgCompondo) msgCompondo = false;
            else if (msgViewIndex >= 0) msgViewIndex = -1;
            else toolMode = TOOL_MENU;
        }
        desenharBotaoVoltarIcone(vltX, vltY, vltW, vltH, hoverVoltar);

        const char* msgTipoNome[4] = { "E-mail", "WhatsApp", "SMS", "Telegram" };

        if (msgViewIndex >= 0 && msgViewIndex < msgsTotal) {
            MsgItem* m = &msgs[msgViewIndex];
            desenharTexto(msgTipoNome[m->tipo], 26, 100, 180, 0xFF00AAFF);
            desenharTexto("De:", 22, 100, 230, 0xFFAAAAAA);
            desenharTexto(m->de, 22, 160, 230, 0xFFFFFFFF);
            desenharTexto("Assunto:", 22, 100, 270, 0xFFAAAAAA);
            desenharTexto(m->assunto, 22, 200, 270, 0xFFFFFFFF);
            char qd[64]; struct tm* mt = localtime(&m->recebido);
            if (mt) strftime(qd, sizeof(qd), "%d/%m/%Y %H:%M", mt); else qd[0] = 0;
            desenharTexto(qd, 18, 100, 310, 0xFF888888);
            // caixa de texto
            desenharRetanguloAlpha(100, 360, 1720, 560, 0x0CFFFFFF);
            DrawRectangleLines(100, 360, 1720, 560, C32(0xFF333333));
            // quebra de linha simples
            char buf[700]; strncpy(buf, m->texto, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
            int ty = 380; int cols = 90;
            char linha[120];
            int start = 0, len = strlen(buf);
            while (start < len) {
                int end = start + cols;
                if (end > len) end = len;
                int br = end;
                for (int p = start; p < end; p++) if (buf[p] == '\n') { br = p; break; }
                if (br == end && end < len) { for (int p = end; p > start; p--) if (buf[p] == ' ') { br = p; break; } }
                int l = 0;
                for (int p = start; p < br && l < 119; p++) linha[l++] = buf[p];
                linha[l] = 0;
                desenharTexto(linha, 20, 120, ty, 0xFFFFFFFF);
                ty += 28;
                start = (buf[br] == '\n' || buf[br] == ' ') ? br + 1 : br;
                if (ty > 900) break;
            }
            int bX = 100, bY = 1000, bW = 200, bH = 50;
            bool hb = (mousePos.x >= bX && mousePos.x <= bX + bW && mousePos.y >= bY && mousePos.y <= bY + bH);
            desenharRetanguloAlpha(bX, bY, bW, bH, hb ? 0xFF00AAFF : 0xFF333333);
            DrawRectangleLines(bX, bY, bW, bH, WHITE);
            desenharTexto("VOLTAR", 18, bX + 55, bY + 16, 0xFFFFFFFF);
        }
        else if (!msgCompondo) {
            const char* tipos[5] = { "Todos", "E-mail", "WhatsApp", "SMS", "Telegram" };
            int fbX = 100, fbY = 200, fbW = 200, fbH = 45;
            for (int ti = 0; ti < 5; ti++) {
                int bx = fbX + ti * (fbW + 15);
                bool h = (mousePos.x >= bx && mousePos.x <= bx + fbW && mousePos.y >= fbY && mousePos.y <= fbY + fbH);
                bool on = ((ti == 0 && msgFiltroTipo == -1) || (ti > 0 && msgFiltroTipo == ti - 1));
                desenharRetanguloAlpha(bx, fbY, fbW, fbH, on ? 0xFF00AAFF : (h ? 0xFF444444 : 0xFF222222));
                DrawRectangleLines(bx, fbY, fbW, fbH, C32(on ? 0xFF00AAFF : (h ? 0xFF888888 : 0xFF555555)));
                desenharTexto(tipos[ti], 18, bx + 30, fbY + 12, on ? 0xFF000000 : 0xFFFFFFFF);
            }
            int nvX = 1700, nvY = 200, nvW = 200, nvH = 45;
            bool hnv = (mousePos.x >= nvX && mousePos.x <= nvX + nvW && mousePos.y >= nvY && mousePos.y <= nvY + nvH);
            desenharRetanguloAlpha(nvX, nvY, nvW, nvH, hnv ? 0xFF00AAFF : 0xFF333333);
            DrawRectangleLines(nvX, nvY, nvW, nvH, WHITE);
            desenharTexto("+ NOVA", 20, nvX + 55, nvY + 12, 0xFFFFFFFF);

            int nVis = 0; int visIdx[500];
            for (int i = 0; i < msgsTotal; i++) { if (msgFiltroTipo != -1 && msgs[i].tipo != msgFiltroTipo) continue; visIdx[nVis++] = i; }
            int listX = 100, listY = 320, rowH = 60;
            if (nVis == 0) desenharTexto("Caixa vazia. Clique em + NOVA para criar uma mensagem.", 22, 120, 360, 0xFF888888);
            for (int vi = 0; vi < nVis; vi++) {
                int i = visIdx[vi];
                int ry = listY + vi * rowH - msgScroll;
                if (ry < listY - rowH || ry > 1080) continue;
                MsgItem* m = &msgs[i];
                bool sel = (vi == msgSel);
                bool h = (mousePos.x >= listX && mousePos.x <= listX + 1720 && mousePos.y >= ry && mousePos.y <= ry + rowH - 6);
                desenharRetanguloAlpha(listX, ry, 1720, rowH - 6, sel ? 0x33FFFFFF : (h ? 0x18FFFFFF : 0x0CFFFFFF));
                DrawRectangleLines(listX, ry, 1720, rowH - 6, C32(sel ? 0xFF00AAFF : (h ? 0xFF888888 : 0xFF333333)));
                if (!m->lido) { desenharRetanguloAlpha(listX + 6, ry + 22, 12, 12, 0xFFFF3300); DrawRectangleLines(listX + 6, ry + 22, 12, 12, C32(0xFFFFAA00)); }
                desenharTexto(msgTipoNome[m->tipo], 18, listX + 30, ry + 8, 0xFF00AAFF);
                desenharTexto(m->de, 22, listX + 150, ry + 8, m->lido ? 0xFFCCCCCC : 0xFFFFFFFF);
                desenharTexto(m->assunto, 18, listX + 150, ry + 36, 0xFFAAAAAA);
                char qd[48]; struct tm* mt = localtime(&m->recebido);
                if (mt) strftime(qd, sizeof(qd), "%d/%m %H:%M", mt); else qd[0] = 0;
                desenharTexto(qd, 16, listX + 1450, ry + 20, 0xFF777777);
            }
        }
        else {
            const char* labels[4] = { "Tipo (ENTER/TAB troca)", "De", "Assunto", "Texto" };
            char* vals[4]; vals[0] = (char*)msgTipoNome[msgTipoSel]; vals[1] = mDe; vals[2] = mAss; vals[3] = mTxt;
            int fy = 200;
            for (int i = 0; i < 4; i++) {
                int ry = fy + (i == 3 ? (fy + 3 * 70) : i * 70);
                int hh = (i == 3) ? 260 : 62;
                bool active = (i == msgField);
                desenharRetanguloAlpha(100, ry, 1720, hh - 8, active ? 0x22FFFFFF : 0x0CFFFFFF);
                DrawRectangleLines(100, ry, 1720, hh - 8, C32(active ? 0xFF00AAFF : 0xFF333333));
                desenharTexto(labels[i], 18, 120, ry + 6, active ? 0xFF00AAFF : 0xFF888888);
                if (i == 3) {
                    char buf[620]; strncpy(buf, mTxt, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
                    int ty = ry + 34; int cols = 110; int start = 0, len = strlen(buf);
                    while (start < len && ty < ry + hh - 14) {
                        int end = start + cols; if (end > len) end = len;
                        char linha[130]; int l = 0;
                        for (int p = start; p < end && l < 129; p++) linha[l++] = buf[p];
                        linha[l] = 0;
                        desenharTexto(linha, 18, 120, ty, 0xFFFFFFFF); ty += 24;
                        start = end;
                    }
                }
                else desenharTexto(vals[i], 20, 120, ry + 36, 0xFFFFFFFF);
            }
            int svX = 1300, svY = 1000, svW = 200, svH = 50;
            bool hsv = (mousePos.x >= svX && mousePos.x <= svX + svW && mousePos.y >= svY && mousePos.y <= svY + svH);
            desenharRetanguloAlpha(svX, svY, svW, svH, hsv ? 0xFF00AAFF : 0xFF333333);
            DrawRectangleLines(svX, svY, svW, svH, WHITE);
            desenharTexto("SALVAR", 20, svX + 60, svY + 16, 0xFFFFFFFF);
            int cxX = 1520, cxY = 1000, cxW = 200, cxH = 50;
            bool hcx = (mousePos.x >= cxX && mousePos.x <= cxX + cxW && mousePos.y >= cxY && mousePos.y <= cxY + cxH);
            desenharRetanguloAlpha(cxX, cxY, cxW, cxH, hcx ? 0xFFAA3333 : 0xFF333333);
            DrawRectangleLines(cxX, cxY, cxW, cxH, WHITE);
            desenharTexto("CANCELAR", 18, cxX + 45, cxY + 16, 0xFFFFFFFF);
        }
    }
    // =================== TERMINAL ===================
    else if (toolMode == TOOL_TERMINAL) {
        desenharTexto("TERMINAL", 38, 100, 60, 0xFF00AAFF);
        desenharTexto("[ESC] Menu Anterior  |  digite um comando e ENTER  |  ↑/↓ historico", 20, 100, 110, 0xFFAAAAAA);

        int vltX = 1600, vltY = 80, vltW = 220, vltH = 50;
        bool hoverVoltar = (mousePos.x >= vltX && mousePos.x <= vltX + vltW && mousePos.y >= vltY && mousePos.y <= vltY + vltH);
        if (hoverVoltar && clicked) toolMode = TOOL_MENU;
        desenharBotaoVoltarIcone(vltX, vltY, vltW, vltH, hoverVoltar);

        // Botao OPCOES
        int opX = 1180, opY = 80, opW = 200, opH = 45;
        bool hop = (mousePos.x >= opX && mousePos.x <= opX + opW && mousePos.y >= opY && mousePos.y <= opY + opH);
        desenharRetanguloAlpha(opX, opY, opW, opH, hop ? 0xFF00AAFF : 0xFF333333);
        DrawRectangleLines(opX, opY, opW, opH, WHITE);
        desenharTexto("OPCOES", 20, opX + 55, opY + 13, 0xFFFFFFFF);

        // Area de saida
        int outX = 100, outY = 150, outW = 1720, outH = 820;
        desenharRetanguloAlpha(outX, outY, outW, outH, 0x0A0A0C);
        DrawRectangleLines(outX, outY, outW, outH, C32(0xFF333333));
        int lineH = 26;
        int visible = outH / lineH;
        int start = termScroll - visible; if (start < 0) start = 0;
        for (int i = start; i < termLinesTotal && i < termScroll; i++) {
            int ry = outY + 10 + (i - start) * lineH;
            if (ry > outY + outH - lineH) break;
            uint32_t col = 0xFFCCFFCC;
            if (strncmp(termLines[i], "Erro", 4) == 0) col = 0xFFFF5555;
            else if (strncmp(termLines[i], "Abrindo", 7) == 0) col = 0xFF55FF99;
            desenharTexto(termLines[i], 18, outX + 12, ry, col);
        }
        // Barra de rolagem simples
        if (termLinesTotal > visible) {
            int sbH = (visible * outH) / termLinesTotal;
            int sbY = outY + ((termScroll - visible) * (outH - sbH)) / (termLinesTotal - visible);
            desenharRetanguloAlpha(outX + outW - 8, sbY, 5, sbH, 0xFF555555);
        }

        // Linha de entrada
        int inX = 100, inY = 1000, inW = 1720, inH = 50;
        desenharRetanguloAlpha(inX, inY, inW, inH, 0x111111);
        DrawRectangleLines(inX, inY, inW, inH, C32(0xFF00AAFF));
        termCaret += GetFrameTime();
        char prompt[600];
        snprintf(prompt, sizeof(prompt), "> %s", termInput);
        desenharTexto(prompt, 22, inX + 14, inY + 14, 0xFF00FF99);
        if (((int)(termCaret * 2)) % 2 == 0) {
            int cw = medirTexto(prompt, 22);
            DrawRectangle(inX + 14 + cw, inY + 12, 12, 26, (Color){ 0, 255, 153, 255 });
        }

        // Painel de opcoes (comandos rapidos)
        if (termOptOpen) {
            int px = 1300, py = 130, pw = 600, ph = 400;
            desenharRetanguloAlpha(0, 0, 1920, 1080, 0x66000000);
            desenharRetanguloAlpha(px, py, pw, ph, 0xEE1A1A1F);
            DrawRectangleLines(px, py, pw, ph, C32(0xFF00AAFF));
            desenharTexto("COMANDOS RAPIDOS", 22, px + 20, py + 16, 0xFF00AAFF);
            const char* rapidos[8] = { "opencode", "help", "cls", "echo Ola", "date", "ver", "dir", "clear" };
            for (int i = 0; i < 8; i++) {
                int bx = px, by = py + 20 + i * 46, bw = pw, bh = 40;
                bool h = (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
                desenharRetanguloAlpha(bx + 10, by, bw - 20, bh, h ? 0xFF00AAFF : 0xFF2A2A30);
                DrawRectangleLines(bx + 10, by, bw - 20, bh, C32(h ? 0xFF00AAFF : 0xFF444444));
                desenharTexto(rapidos[i], 20, bx + 26, by + 10, h ? 0xFF000000 : 0xFFFFFFFF);
            }
        }
    }
    else if (toolMode == TOOL_IA) {
        desenharTexto("IA / GEMINI", 38, 100, 60, 0xFF00AAFF);
        desenharTexto("[ESC] Menu Anterior  |  Requer chave Gemini em Configuracoes > Chaves API", 20, 100, 108, 0xFFAAAAAA);

        int vltX = 1600, vltY = 80, vltW = 220, vltH = 50;
        bool hoverVoltar = (mousePos.x >= vltX && mousePos.x <= vltX + vltW && mousePos.y >= vltY && mousePos.y <= vltY + vltH);
        if (hoverVoltar && clicked) toolMode = TOOL_MENU;
        desenharBotaoVoltarIcone(vltX, vltY, vltW, vltH, hoverVoltar);

        const char* abas[5] = { "TEXTO", "VISAO", "GERAR IMAGEM", "AUDIO -> TEXTO", "TEXTO -> AUDIO" };
        for (int t = 0; t < 5; t++) {
            int tx = 60 + t * 370, ty = 130, tw = 350, th = 58;
            bool sel = (iaTab == t);
            bool hov = (mousePos.x >= tx && mousePos.x <= tx + tw && mousePos.y >= ty && mousePos.y <= ty + th);
            desenharRetanguloAlpha(tx, ty, tw, th, sel ? 0xFF00AAFF : (hov ? 0xFF2A3A44 : 0xFF1E1E24));
            DrawRectangleLines(tx, ty, tw, th, C32(sel ? 0xFF00FFFF : (hov ? 0xFF88FFFF : 0xFF444444)));
            desenharTexto(abas[t], 24, tx + (tw - medirTexto(abas[t], 24)) / 2, ty + 15, sel ? 0xFF000000 : 0xFFFFFFFF);
        }
        bool cursorBlink = (fmod(GetTime(), 1.0f) < 0.5f);

        if (iaTab == 0) {
            for (int i = 0; i < 3; i++) {
                int bx = 110 + i * 320, by = 220, bw = 300, bh = 44;
                bool sel = (iaModeloIdx == i);
                bool hov = (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
                desenharRetanguloAlpha(bx, by, bw, bh, sel ? 0xFF00AAFF : (hov ? 0xFF2A3A44 : 0xFF1E1E24));
                DrawRectangleLines(bx, by, bw, bh, C32(sel ? 0xFF00FFFF : 0xFF444444));
                desenharTexto(iaModelosTexto[i], 20, bx + 20, by + 12, sel ? 0xFF000000 : 0xFFFFFFFF);
            }
            desenharTexto("PROMPT:", 22, 60, 262, 0xFF00AAFF);
            desenharRetanguloAlpha(60, 290, 1800, 130, 0x111111);
            DrawRectangleLines(60, 290, 1800, 130, C32(0xFF00AAFF));
            ia_quebrar_linhas(iaPrompt, 20, 1760);
            ia_desenhar_linhas(74, 300, 112, 0xFFEEEEEE, 0, 20);
            int cw = medirTexto(iaPrompt, 20);
            if (cursorBlink) DrawRectangle(74 + cw, 298, 12, 24, (Color){ 0, 170, 255, 255 });

            bool hg = (mousePos.x >= 60 && mousePos.x <= 360 && mousePos.y >= 440 && mousePos.y <= 496);
            desenharRetanguloAlpha(60, 440, 300, 56, hg ? 0xFF00CC88 : (iaOcupado ? 0xFF333333 : 0xFF008855));
            DrawRectangleLines(60, 440, 300, 56, C32(0xFF00FFAA));
            desenharTexto("GERAR", 26, 170, 456, iaOcupado ? 0xFF888888 : 0xFFFFFFFF);

            ia_desenhar_painel(60, 520, 1800, 500, "RESPOSTA", "Digite uma pergunta e clique em GERAR.", mousePos, clicked);
        }
        else if (iaTab == 1) {
            for (int i = 0; i < 3; i++) {
                int bx = 110 + i * 320, by = 220, bw = 300, bh = 44;
                bool sel = (iaModeloIdx == i);
                bool hov = (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
                desenharRetanguloAlpha(bx, by, bw, bh, sel ? 0xFF00AAFF : (hov ? 0xFF2A3A44 : 0xFF1E1E24));
                DrawRectangleLines(bx, by, bw, bh, C32(sel ? 0xFF00FFFF : 0xFF444444));
                desenharTexto(iaModelosTexto[i], 20, bx + 20, by + 12, sel ? 0xFF000000 : 0xFFFFFFFF);
            }
            desenharTexto("SELECIONE UMA IMAGEM:", 22, 60, 262, 0xFF00AAFF);
            desenharRetanguloAlpha(60, 290, 680, 13 * 46, 0x0A0A0C);
            DrawRectangleLines(60, 290, 680, 13 * 46, C32(0xFF333333));
            for (int i = 0; i < 13 && (iaArquivoScroll + i) < iaImagemTotal; i++) {
                int idx = iaArquivoScroll + i;
                int ry = 290 + i * 46;
                bool sel = (iaImagemSel == idx);
                bool hov = (mousePos.x >= 60 && mousePos.x <= 740 && mousePos.y >= ry && mousePos.y <= ry + 46);
                if (sel) desenharRetanguloAlpha(62, ry + 2, 676, 42, 0xFF00AAFF);
                else if (hov) desenharRetanguloAlpha(62, ry + 2, 676, 42, 0x222A33);
                desenharTexto(ia_basename(iaImagemList[idx]), 18, 76, ry + 12, sel ? 0xFF000000 : 0xFFCCCCCC);
            }
            desenharTexto("PROMPT (pergunte sobre a imagem):", 22, 780, 262, 0xFF00AAFF);
            desenharRetanguloAlpha(780, 290, 1080, 160, 0x111111);
            DrawRectangleLines(780, 290, 1080, 160, C32(0xFF00AAFF));
            ia_quebrar_linhas(iaPrompt, 20, 1040);
            ia_desenhar_linhas(794, 300, 142, 0xFFEEEEEE, 0, 20);
            int cw2 = medirTexto(iaPrompt, 20);
            if (cursorBlink) DrawRectangle(794 + cw2, 298, 12, 24, (Color){ 0, 170, 255, 255 });

            bool hg = (mousePos.x >= 780 && mousePos.x <= 1080 && mousePos.y >= 470 && mousePos.y <= 526);
            bool pode = (!iaOcupado && iaPromptLen > 0 && iaImagemSel >= 0);
            desenharRetanguloAlpha(780, 470, 300, 56, hg && pode ? 0xFF00CC88 : (iaOcupado ? 0xFF333333 : 0xFF008855));
            DrawRectangleLines(780, 470, 300, 56, C32(pode ? 0xFF00FFAA : 0xFF555555));
            desenharTexto("ANALISAR IMAGEM", 20, 800, 486, iaOcupado ? 0xFF888888 : 0xFFFFFFFF);

            ia_desenhar_painel(780, 550, 1080, 470, "RESPOSTA", "Selecione uma imagem, digite uma pergunta e clique em ANALISAR IMAGEM.", mousePos, clicked);
        }
        else if (iaTab == 2) {
            for (int i = 0; i < 2; i++) {
                int bx = 110 + i * 320, by = 220, bw = 300, bh = 44;
                bool sel = (iaImagemModelo == i);
                bool hov = (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
                desenharRetanguloAlpha(bx, by, bw, bh, sel ? 0xFF00AAFF : (hov ? 0xFF2A3A44 : 0xFF1E1E24));
                DrawRectangleLines(bx, by, bw, bh, C32(sel ? 0xFF00FFFF : 0xFF444444));
                desenharTexto(iaModelosImagem[i], 20, bx + 20, by + 12, sel ? 0xFF000000 : 0xFFFFFFFF);
            }
            desenharTexto("PROPORCAO:", 22, 60, 262, 0xFF00AAFF);
            for (int i = 0; i < 5; i++) {
                int bx = 110 + i * 240, by = 290, bw = 220, bh = 44;
                bool sel = (iaAspecto == i);
                bool hov = (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
                desenharRetanguloAlpha(bx, by, bw, bh, sel ? 0xFF00AAFF : (hov ? 0xFF2A3A44 : 0xFF1E1E24));
                DrawRectangleLines(bx, by, bw, bh, C32(sel ? 0xFF00FFFF : 0xFF444444));
                desenharTexto(iaAspectos[i], 20, bx + (bw - medirTexto(iaAspectos[i], 20)) / 2, by + 12, sel ? 0xFF000000 : 0xFFFFFFFF);
            }
            desenharTexto("PROMPT:", 22, 60, 352, 0xFF00AAFF);
            desenharRetanguloAlpha(60, 380, 1200, 120, 0x111111);
            DrawRectangleLines(60, 380, 1200, 120, C32(0xFF00AAFF));
            ia_quebrar_linhas(iaPrompt, 20, 1160);
            ia_desenhar_linhas(74, 390, 102, 0xFFEEEEEE, 0, 20);
            int cw3 = medirTexto(iaPrompt, 20);
            if (cursorBlink) DrawRectangle(74 + cw3, 388, 12, 24, (Color){ 0, 170, 255, 255 });

            bool hg = (mousePos.x >= 60 && mousePos.x <= 360 && mousePos.y >= 480 && mousePos.y <= 536);
            desenharRetanguloAlpha(60, 480, 300, 56, hg ? 0xFF00CC88 : (iaOcupado ? 0xFF333333 : 0xFF008855));
            DrawRectangleLines(60, 480, 300, 56, C32(0xFF00FFAA));
            desenharTexto("GERAR IMAGEM", 22, 90, 496, iaOcupado ? 0xFF888888 : 0xFFFFFFFF);

            int px = 1330, py = 270, pw = 540, ph = 720;
            desenharRetanguloAlpha(px, py, pw, ph, 0x0A0A0C);
            DrawRectangleLines(px, py, pw, ph, C32(0xFF333333));
            desenharTexto("IMAGEM GERADA", 24, px + 20, py + 16, 0xFF00AAFF);
            if (iaTemErro || iaTemResposta) {
                int btnW = 110, btnH = 34;
                int btnX = px + pw - 130;
                int btnY = py + 10;
                bool hovC = (mousePos.x >= btnX && mousePos.x <= btnX + btnW && mousePos.y >= btnY && mousePos.y <= btnY + btnH);
                desenharRetanguloAlpha(btnX, btnY, btnW, btnH, hovC ? 0xFF00AAFF : 0xFF2A3A44);
                DrawRectangleLines(btnX, btnY, btnW, btnH, (Color){0, 255, 255, 255});
                desenharTexto("COPIAR", 18, btnX + (btnW - medirTexto("COPIAR", 18)) / 2, btnY + 8, 0xFFFFFFFF);
                if (hovC && clicked) SetClipboardText(iaTemErro ? iaErro : iaResposta);
                if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_C)) SetClipboardText(iaTemErro ? iaErro : iaResposta);
            }
            if (iaOcupado) {
                desenharTexto("Gerando imagem...", 24, px + 20, py + 60, 0xFFFFCC00);
            } else if (iaTemErro) {
                ia_quebrar_linhas(iaErro, 18, pw - 40);
                ia_desenhar_linhas(px + 20, py + 60, ph - 80, 0xFFFF8888, 0, 18);
            } else if (iaMostrarImagem && iaResTexLoaded) {
                float tw = (float)iaResTex.width, th = (float)iaResTex.height;
                if (tw > 0 && th > 0) {
                    float boxW = (float)(pw - 40), boxH = (float)(ph - 90);
                    float scale = (tw > boxW || th > boxH) ? fminf(boxW / tw, boxH / th) : 1.0f;
                    float dw = tw * scale, dh = th * scale;
                    DrawTexturePro(iaResTex, (Rectangle){ 0, 0, tw, th },
                        (Rectangle){ px + (pw - dw) / 2, py + 36, dw, dh }, (Vector2){ 0, 0 }, 0, WHITE);
                    desenharTexto(iaResp.mediaPath, 16, px + 20, py + ph - 34, 0xFF888888);
                }
            } else {
                desenharTexto("Digite uma descricao e clique em GERAR IMAGEM.", 20, px + 20, py + 60, 0xFF666666);
            }
        }
        else if (iaTab == 3) {
            for (int i = 0; i < 3; i++) {
                int bx = 110 + i * 320, by = 220, bw = 300, bh = 44;
                bool sel = (iaModeloIdx == i);
                bool hov = (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
                desenharRetanguloAlpha(bx, by, bw, bh, sel ? 0xFF00AAFF : (hov ? 0xFF2A3A44 : 0xFF1E1E24));
                DrawRectangleLines(bx, by, bw, bh, C32(sel ? 0xFF00FFFF : 0xFF444444));
                desenharTexto(iaModelosTexto[i], 20, bx + 20, by + 12, sel ? 0xFF000000 : 0xFFFFFFFF);
            }
            bool hg = (mousePos.x >= 1220 && mousePos.x <= 1560 && mousePos.y >= 210 && mousePos.y <= 266);
            bool pode = (!iaOcupado && iaAudioSel >= 0);
            desenharRetanguloAlpha(1220, 210, 340, 56, hg && pode ? 0xFF00CC88 : (iaOcupado ? 0xFF333333 : 0xFF008855));
            DrawRectangleLines(1220, 210, 340, 56, C32(pode ? 0xFF00FFAA : 0xFF555555));
            desenharTexto("TRANSCREVER AUDIO", 20, 1240, 226, iaOcupado ? 0xFF888888 : 0xFFFFFFFF);

            bool hm = (mousePos.x >= 1580 && mousePos.x <= 1860 && mousePos.y >= 210 && mousePos.y <= 266);
            desenharRetanguloAlpha(1580, 210, 280, 56, (hm && !iaOcupado) ? 0xFFCC4444 : (iaMicGravando ? 0xFF992222 : (iaOcupado ? 0xFF333333 : 0xFF008855)));
            DrawRectangleLines(1580, 210, 280, 56, C32(iaMicGravando ? 0xFFFF5555 : 0xFF00FFAA));
            int mx = 1606, my = 238;
            Color micIc = iaMicGravando ? (Color){ 255, 90, 90, 255 } : (Color){ 255, 255, 255, 255 };
            DrawRectangleRounded((Rectangle){ (float)mx - 9, (float)my - 18, 18, 26 }, 0.5f, 8, micIc);
            DrawCircle(mx, my - 22, 11, micIc);
            DrawCircle(mx, my - 22, 7, (Color){ 20, 20, 20, 255 });
            DrawRectangle(mx - 9, my + 10, 18, 3, micIc);
            DrawRectangle(mx - 14, my + 13, 28, 3, micIc);
            desenharTexto(iaMicGravando ? "PARAR" : "GRAVAR", 20, 1640, 226, iaOcupado ? 0xFF888888 : 0xFFFFFFFF);
            if (iaMicGravando) {
                int seg = (int)(GetTime() - iaMicInicio);
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "GRAVANDO... %02d:%02d", seg / 60, seg % 60);
                desenharTexto(tmp, 20, 1220, 262, 0xFFFF5555);
            }

            desenharTexto("SELECIONE UM AUDIO:", 22, 60, 262, 0xFF00AAFF);
            desenharRetanguloAlpha(60, 290, 1100, 15 * 46, 0x0A0A0C);
            DrawRectangleLines(60, 290, 1100, 15 * 46, C32(0xFF333333));
            for (int i = 0; i < 15 && (iaArquivoScroll + i) < iaAudioTotal; i++) {
                int idx = iaArquivoScroll + i;
                int ry = 290 + i * 46;
                bool sel = (iaAudioSel == idx);
                bool hov = (mousePos.x >= 60 && mousePos.x <= 1160 && mousePos.y >= ry && mousePos.y <= ry + 46);
                if (sel) desenharRetanguloAlpha(62, ry + 2, 1096, 42, 0xFF00AAFF);
                else if (hov) desenharRetanguloAlpha(62, ry + 2, 1096, 42, 0x222A33);
                desenharTexto(ia_basename(iaAudioList[idx]), 18, 76, ry + 12, sel ? 0xFF000000 : 0xFFCCCCCC);
            }
            const char* iaMicPlaceholder = iaMicGravando ? "Gravando... fale agora e clique em PARAR." : "Selecione um audio e clique em TRANSCREVER.";
            ia_desenhar_painel(1220, 290, 640, 720, "TRANSCRICAO", iaMicPlaceholder, mousePos, clicked);
        }
        else if (iaTab == 4) {
            desenharTexto("VOZ:", 22, 60, 178, 0xFF00AAFF);
            for (int i = 0; i < 6; i++) {
                int bx = 110 + (i % 3) * 320, by = 210 + (i / 3) * 60, bw = 300, bh = 44;
                bool sel = (iaVozIdx == i);
                bool hov = (mousePos.x >= bx && mousePos.x <= bx + bw && mousePos.y >= by && mousePos.y <= by + bh);
                desenharRetanguloAlpha(bx, by, bw, bh, sel ? 0xFF00AAFF : (hov ? 0xFF2A3A44 : 0xFF1E1E24));
                DrawRectangleLines(bx, by, bw, bh, C32(sel ? 0xFF00FFFF : 0xFF444444));
                desenharTexto(iaVozes[i], 20, bx + 20, by + 12, sel ? 0xFF000000 : 0xFFFFFFFF);
            }
            desenharTexto("TEXTO PARA LER:", 22, 60, 342, 0xFF00AAFF);
            desenharRetanguloAlpha(60, 370, 1800, 120, 0x111111);
            DrawRectangleLines(60, 370, 1800, 120, C32(0xFF00AAFF));
            ia_quebrar_linhas(iaPrompt2, 20, 1760);
            ia_desenhar_linhas(74, 380, 102, 0xFFEEEEEE, 0, 20);
            int cw4 = medirTexto(iaPrompt2, 20);
            if (cursorBlink) DrawRectangle(74 + cw4, 378, 12, 24, (Color){ 0, 170, 255, 255 });

            bool hg = (mousePos.x >= 60 && mousePos.x <= 420 && mousePos.y >= 460 && mousePos.y <= 516);
            desenharRetanguloAlpha(60, 460, 360, 56, hg ? 0xFF00CC88 : (iaOcupado ? 0xFF333333 : 0xFF008855));
            DrawRectangleLines(60, 460, 360, 56, C32(0xFF00FFAA));
            desenharTexto("GERAR AUDIO", 22, 155, 476, iaOcupado ? 0xFF888888 : 0xFFFFFFFF);

            ia_desenhar_painel(60, 540, 1800, 460, "STATUS", "Digite um texto, escolha a voz e clique em GERAR AUDIO.", mousePos, clicked);
        }
    }

    EndTextureMode();

    // Render scaled to screen
    int sWidth = GetScreenWidth();
    int sHeight = GetScreenHeight();
    DrawTexturePro(toolsRenderTexture.texture, 
                  (Rectangle){ 0, 0, (float)toolsRenderTexture.texture.width, (float)-toolsRenderTexture.texture.height }, 
                  (Rectangle){ 0, 0, (float)sWidth, (float)sHeight }, 
                  (Vector2){ 0, 0 }, 0.0f, WHITE);
}

void abrirTextoNoBlocoNotas(const char* filepath) {
    initFerramentas();
    toolMode = TOOL_NOTEPAD;
    notepadState = NOTEPAD_EDIT;

    FILE* f = fopen(filepath, "r");
    if (f) {
        size_t readBytes = fread(notepadText, 1, sizeof(notepadText) - 1, f);
        notepadText[readBytes] = '\0';
        notepadTextLen = (int)readBytes;
        fclose(f);
    }
}
