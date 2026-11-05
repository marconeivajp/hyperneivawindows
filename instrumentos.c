#include "instrumentos.h"
#include "raylib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>   
#include <math.h>     
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

// Global/Extern variables
float audioTempoAtual = 0.0f;
char musicaAtual[256] = "PARADO";
extern void tocarMusicaNova(const char* path);

// RenderTexture for drawing at 1920x1080 and scaling to screen
RenderTexture2D instRenderTexture;
bool instRenderTextureLoaded = false;

extern void voltarParaMenuExtra(void);

// Variables and state
int modoInstrumento = 0; // 0 = Piano, 1 = Violao
int violaoMenuEstado = 0; // 0 = Principal, 1 = Opcoes, 2 = Criador, 3 = Selecionar Faixa
int cursorMenuViolao = 0;
int oitavaOffset = 0;

#define MAX_CHORDS_MUSICA 1000 

typedef struct {
    float tempo;
    char nome[16];
} AcordeTempo;

static AcordeTempo acordesCarregados[MAX_CHORDS_MUSICA];
static int totalAcordesCarregados = 0;
static bool usandoAcordesFalsos = false;

static void carregarAcordesDaMusica(const char* audioPath) {
    totalAcordesCarregados = 0;
    usandoAcordesFalsos = false;
    char txtPath[512];
    strcpy(txtPath, audioPath);

    char* dot = strrchr(txtPath, '.');
    if (dot) {
        strcpy(dot, ".txt");
    } else {
        strcat(txtPath, ".txt");
    }

    FILE* f = fopen(txtPath, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f) && totalAcordesCarregados < MAX_CHORDS_MUSICA) {
            float sec = 0.0f;
            char nome[16] = "";
            if (sscanf(line, "%f | %15s", &sec, nome) == 2) {
                acordesCarregados[totalAcordesCarregados].tempo = sec;
                strcpy(acordesCarregados[totalAcordesCarregados].nome, nome);
                totalAcordesCarregados++;
            }
        }
        fclose(f);
    }

    if (totalAcordesCarregados == 0) {
        usandoAcordesFalsos = true;
        const char* fakeChords[] = { "C", "G", "Am", "F", "Dm", "E7", "Am", "G" };
        for (int i = 0; i < 8; i++) {
            acordesCarregados[i].tempo = i * 4.0f;
            strcpy(acordesCarregados[i].nome, fakeChords[i % 8]);
            totalAcordesCarregados++;
        }
    }
}

// Custom collections database
#define MAX_COLETANEAS 10
#define GRUPOS_POR_COLETANEA 7
typedef struct {
    char nome[64];
    int grupoOrigem[GRUPOS_POR_COLETANEA][6];
    int indiceOrigem[GRUPOS_POR_COLETANEA][6];
} ColetaneaCustom;
static ColetaneaCustom coletaneasCustomizadas[MAX_COLETANEAS];
static int totalColetaneas = 0;
static ColetaneaCustom coletaneaEmEdicao;
static int grupoEdicaoAtual = 0;
static int cursorEdicaoAcorde = 0;

// Mini-explorer file navigation
static char instCaminhoAtual[512] = "data/HyperNeiva/midia/musicas";
static char instNomes[500][64];
static char instPaths[500][256];
static bool instEhPasta[500];
static int instTotalItens = 0;
static int instSel = 0;
static int instScroll = 0;

static void instListarMusicas(const char* path) {
    DIR* d = opendir(path);
    if (!d) return;

    instTotalItens = 0;

    // Check if we can show "go back" folder
    if (strcmp(path, "data/HyperNeiva/midia/musicas") != 0 && strcmp(path, "data/HyperNeiva/midia") != 0 && strcmp(path, "data/HyperNeiva") != 0) {
        strcpy(instNomes[instTotalItens], "[ .. Voltar Pasta Anterior ]");
        strcpy(instPaths[instTotalItens], "..");
        instEhPasta[instTotalItens] = true;
        instTotalItens++;
    }

    struct dirent* dir;
    while ((dir = readdir(d)) != NULL && instTotalItens < 500) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, dir->d_name);

        struct stat st;
        bool isDir = false;
        if (stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode)) {
            isDir = true;
        }

        if (isDir) {
            snprintf(instNomes[instTotalItens], 64, "[ %s ]", dir->d_name);
            strcpy(instPaths[instTotalItens], fullPath);
            instEhPasta[instTotalItens] = true;
            instTotalItens++;
        } else {
            char temp[256]; strcpy(temp, dir->d_name);
            for (int i = 0; temp[i]; i++) temp[i] = tolower(temp[i]);
            if (strstr(temp, ".wav") || strstr(temp, ".mp3")) {
                strncpy(instNomes[instTotalItens], dir->d_name, 63);
                strcpy(instPaths[instTotalItens], fullPath);
                instEhPasta[instTotalItens] = false;
                instTotalItens++;
            }
        }
    }
    closedir(d);

    // Sort folders first, then files alphabetically
    for (int i = (strcmp(instPaths[0], "..") == 0 ? 1 : 0); i < instTotalItens - 1; i++) {
        for (int j = i + 1; j < instTotalItens; j++) {
            bool swap = false;
            if (instEhPasta[i] != instEhPasta[j]) {
                if (!instEhPasta[i] && instEhPasta[j]) swap = true;
            } else {
                if (strcasecmp(instNomes[i], instNomes[j]) > 0) swap = true;
            }
            if (swap) {
                char tNome[64]; strcpy(tNome, instNomes[i]); strcpy(instNomes[i], instNomes[j]); strcpy(instNomes[j], tNome);
                char tPath[256]; strcpy(tPath, instPaths[i]); strcpy(instPaths[i], instPaths[j]); strcpy(instPaths[j], tPath);
                bool tPasta = instEhPasta[i]; instEhPasta[i] = instEhPasta[j]; instEhPasta[j] = tPasta;
            }
        }
    }
    instSel = 0;
    instScroll = 0;
}

// Piano and guitar states
static bool pianoKeysPressionadas[13] = { false };
static float pianoFases[13] = { 0.0f };
static float volumeTecla[13] = { 0.0f };
static const float pianoFreqsBase[13] = {
    261.63f, 277.18f, 293.66f, 311.13f, 329.63f, 349.23f,
    369.99f, 392.00f, 415.30f, 440.00f, 466.16f, 493.88f, 523.25f
};

#define NUM_GRUPOS_VIOLAO 7
static int acordeAtual = 0;
static int grupoAcordeAtual = 0;
static bool usandoColetaneaCustomizada = false;
static int indiceColetaneaAtual = 0;

static const char* nomeAcordes[NUM_GRUPOS_VIOLAO][6] = {
    { "C", "Dm", "Em", "F", "G", "Am" }, { "G", "Am", "Bm", "C", "D", "Em" },
    { "D", "Em", "F#m", "G", "A", "Bm" }, { "A", "Bm", "C#m", "D", "E", "F#m" },
    { "E", "F#m", "G#m", "A", "B", "C#m" }, { "F", "Gm", "Am", "Bb", "C", "Dm" },
    { "B", "C#m", "D#m", "E", "F#", "G#m" }
};

static const char* notasAcordes[NUM_GRUPOS_VIOLAO][6][6] = {
    { {"G2", "C3", "E3", "G3", "C4", "E4"}, {"A2", "D3", "A3", "D4", "F4", "A4"}, {"E2", "B2", "E3", "G3", "B3", "E4"}, {"F2", "C3", "F3", "A3", "C4", "F4"}, {"G2", "B2", "D3", "G3", "D4", "G4"}, {"E2", "A2", "E3", "A3", "C4", "E4"} },
    { {"G2", "B2", "D3", "G3", "D4", "G4"}, {"E2", "A2", "E3", "A3", "C4", "E4"}, {"F#2", "B2", "F#3", "B3", "D4", "F#4"},{"G2", "C3", "E3", "G3", "C4", "E4"}, {"A2", "D3", "A3", "D4", "F#4", "A4"},{"E2", "B2", "E3", "G3", "B3", "E4"} },
    { {"A2", "D3", "A3", "D4", "F#4", "A4"},{"E2", "B2", "E3", "G3", "B3", "E4"}, {"F#2", "C#3", "F#3", "A3", "C#4", "F#4"},{"G2", "B2", "D3", "G3", "D4", "G4"}, {"E2", "A2", "E3", "A3", "C#4", "E4"},{"F#2", "B2", "F#3", "B3", "D4", "F#4"} },
    { {"E2", "A2", "E3", "A3", "C#4", "E4"},{"F#2", "B2", "F#3", "B3", "D4", "F#4"},{"G#2", "C#3", "G#3", "C#4", "E4", "G#4"},{"A2", "D3", "A3", "D4", "F#4", "A4"}, {"E2", "B2", "E3", "G#3", "B3", "E4"},{"F#2", "C#3", "F#3", "A3", "C#4", "F#4"} },
    { {"E2", "B2", "E3", "G#3", "B3", "E4"},{"F#2", "C#3", "F#3", "A3", "C#4", "F#4"},{"G#2", "D#3", "G#3", "B3", "D#4", "G#4"},{"E2", "A2", "E3", "A3", "C#4", "E4"}, {"F#2", "B2", "F#3", "B3", "D#4", "F#4"},{"G#2", "C#3", "G#3", "C#4", "E4", "G#4"} },
    { {"F2", "C3", "F3", "A3", "C4", "F4"}, {"G2", "D3", "G3", "Bb3", "D4", "G4"}, {"E2", "A2", "E3", "A3", "C4", "E4"}, {"F2", "Bb2", "F3", "Bb3", "D4", "F4"}, {"G2", "C3", "E3", "G3", "C4", "E4"}, {"A2", "D3", "A3", "D4", "F4", "A4"} },
    { {"F#2", "B2", "F#3", "B3", "D#4", "F#4"},{"G#2", "C#3", "G#3", "C#4", "E4", "G#4"},{"A#2", "D#3", "A#3", "D#4", "F#4", "A#4"},{"E2", "B2", "E3", "G#3", "B3", "E4"}, {"F#2", "C#3", "F#3", "A#3", "C#4", "F#4"},{"G#2", "D#3", "G#3", "B3", "D#4", "G#4"} }
};

static const float frequenciasViolao[NUM_GRUPOS_VIOLAO][6][6] = {
    { {98.00f, 130.81f, 164.81f, 196.00f, 261.63f, 329.63f}, {110.00f, 146.83f, 220.00f, 293.66f, 349.23f, 440.00f}, {82.41f, 123.47f, 164.81f, 196.00f, 246.94f, 329.63f}, {87.31f, 130.81f, 174.61f, 220.00f, 261.63f, 349.23f}, {98.00f, 123.47f, 146.83f, 196.00f, 293.66f, 392.00f}, {82.41f, 110.00f, 164.81f, 220.00f, 261.63f, 329.63f} },
    { {98.00f, 123.47f, 146.83f, 196.00f, 293.66f, 392.00f}, {82.41f, 110.00f, 164.81f, 220.00f, 261.63f, 329.63f}, {92.50f, 123.47f, 185.00f, 246.94f, 293.66f, 369.99f}, {98.00f, 130.81f, 164.81f, 196.00f, 261.63f, 329.63f}, {110.00f, 146.83f, 220.00f, 293.66f, 369.99f, 440.00f},{82.41f, 123.47f, 164.81f, 196.00f, 246.94f, 329.63f} },
    { {110.00f, 146.83f, 220.00f, 293.66f, 369.99f, 440.00f},{82.41f, 123.47f, 164.81f, 196.00f, 246.94f, 329.63f}, {92.50f, 138.59f, 185.00f, 220.00f, 277.18f, 369.99f}, {98.00f, 123.47f, 146.83f, 196.00f, 293.66f, 392.00f}, {82.41f, 110.00f, 164.81f, 220.00f, 277.18f, 329.63f}, {92.50f, 123.47f, 185.00f, 246.94f, 293.66f, 369.99f} },
    { {82.41f, 110.00f, 164.81f, 220.00f, 277.18f, 329.63f}, {92.50f, 123.47f, 185.00f, 246.94f, 293.66f, 369.99f}, {103.83f, 138.59f, 207.65f, 277.18f, 329.63f, 415.30f},{110.00f, 146.83f, 220.00f, 293.66f, 369.99f, 440.00f}, {82.41f, 123.47f, 164.81f, 207.65f, 246.94f, 329.63f}, {92.50f, 138.59f, 185.00f, 220.00f, 277.18f, 369.99f} },
    { {82.41f, 123.47f, 164.81f, 207.65f, 246.94f, 329.63f}, {92.50f, 138.59f, 185.00f, 220.00f, 277.18f, 369.99f}, {103.83f, 155.56f, 207.65f, 246.94f, 311.13f, 415.30f},{82.41f, 110.00f, 164.81f, 220.00f, 277.18f, 329.63f}, {92.50f, 123.47f, 185.00f, 246.94f, 311.13f, 369.99f}, {103.83f, 138.59f, 207.65f, 277.18f, 329.63f, 415.30f} },
    { {87.31f, 130.81f, 174.61f, 220.00f, 261.63f, 349.23f}, {98.00f, 146.83f, 196.00f, 233.08f, 293.66f, 392.00f}, {82.41f, 110.00f, 164.81f, 220.00f, 261.63f, 329.63f}, {87.31f, 116.54f, 174.61f, 233.08f, 293.66f, 349.23f}, {98.00f, 130.81f, 164.81f, 196.00f, 261.63f, 349.23f}, {110.00f, 146.83f, 220.00f, 293.66f, 349.23f, 440.00f} },
    { {92.50f, 123.47f, 185.00f, 246.94f, 311.13f, 369.99f}, {103.83f, 138.59f, 207.65f, 277.18f, 329.63f, 415.30f}, {116.54f, 155.56f, 233.08f, 311.13f, 369.99f, 466.16f},{82.41f, 123.47f, 164.81f, 207.65f, 246.94f, 329.63f}, {92.50f, 138.59f, 185.00f, 233.08f, 277.18f, 369.99f}, {103.83f, 155.56f, 207.65f, 246.94f, 311.13f, 415.30f} }
};

static float ks_delayLines[6][2048];
static int ks_delayPtr[6] = { 0 };
static int ks_delayLength[6] = { 0 };
static volatile int ks_strumTimer[6] = { -1, -1, -1, -1, -1, -1 };

// Helper draw functions
static void desenharRetanguloAlpha(int x, int y, int w, int h, uint32_t cor) {
    uint8_t a = (cor >> 24) & 0xFF;
    uint8_t r = (cor >> 16) & 0xFF;
    uint8_t g = (cor >> 8) & 0xFF;
    uint8_t b = cor & 0xFF;
    DrawRectangle(x, y, w, h, (Color){ r, g, b, a });
}

static void desenharTexto(const char* text, int size, int x, int y, uint32_t cor) {
    uint8_t a = (cor >> 24) & 0xFF;
    uint8_t r = (cor >> 16) & 0xFF;
    uint8_t g = (cor >> 8) & 0xFF;
    uint8_t b = cor & 0xFF;
    DrawText(text, x, y, size, (Color){ r, g, b, a });
}

static void renderizarGrelhaAcordes(void) {
    int painelY = 820;
    int painelH = 260;

    if (audioTempoAtual <= 0.0f || strcmp(musicaAtual, "PARADO") == 0) {
        desenharRetanguloAlpha(0, painelY, 1920, painelH, 0xEE020617);
        desenharRetanguloAlpha(400, painelY, 4, painelH, 0xFF334155);
        desenharTexto("HYPER NEIVA STUDIO v2", 30, 50, painelY + 40, 0xFF38BDF8);
        desenharTexto("STATUS: Aguardando Faixa de Audio...", 20, 50, painelY + 80, 0xFF94A3B8);
        desenharTexto("-> Acesse o Menu de Opcoes (M/TAB) -> 'Escolher Musica' para carregar a musica e os acordes reais!", 18, 50, painelY + 120, 0xFFFFFFFF);
        return;
    }

    int pixelsPerSecond = 200;
    int centerX = 400;

    desenharRetanguloAlpha(0, painelY, 1920, painelH, 0xEE020617);

    float beatDuration = 60.0f / 120.0f;
    int startBeatIndex = (int)((audioTempoAtual - 5.0f) / beatDuration);
    int endBeatIndex = (int)((audioTempoAtual + 10.0f) / beatDuration);

    // GRELHA DE FUNDO
    for (int i = startBeatIndex; i <= endBeatIndex; i++) {
        if (i < 0) continue;
        float beatTime = i * beatDuration;
        int px = centerX + (int)((beatTime - audioTempoAtual) * pixelsPerSecond);

        if (px > -100 && px < 1920 + 100) {
            if (i % 4 == 0) {
                desenharRetanguloAlpha(px, painelY + 30, 2, 200, 0x7738BDF8);
            } else {
                desenharRetanguloAlpha(px, painelY + 50, 2, 160, 0x6694A3B8);
            }
        }
    }

    int blockH = 160;
    int blockY = painelY + 50;

    if (totalAcordesCarregados > 0) {
        for (int i = 0; i < totalAcordesCarregados; i++) {
            float startT = acordesCarregados[i].tempo;
            float endT = (i + 1 < totalAcordesCarregados) ? acordesCarregados[i + 1].tempo : (startT + 4.0f);

            int startPx = centerX + (int)((startT - audioTempoAtual) * pixelsPerSecond);
            int endPx = centerX + (int)((endT - audioTempoAtual) * pixelsPerSecond);
            int boxWidth = endPx - startPx;

            if (endPx > -100 && startPx < 1920 + 100 && boxWidth > 0) {
                bool isActive = (audioTempoAtual >= startT && audioTempoAtual < endT);
                uint32_t corBloco = isActive ? 0x6638BDF8 : 0x881E293B;
                uint32_t corTexto = isActive ? 0xFFFFFFFF : 0xFF64748B;

                desenharRetanguloAlpha(startPx, blockY, boxWidth, blockH, corBloco);
                desenharTexto(acordesCarregados[i].nome, 50, startPx + (boxWidth / 2) - 25, painelY + 155, corTexto);
            }
        }

        // BORDAS
        for (int i = 0; i < totalAcordesCarregados; i++) {
            float startT = acordesCarregados[i].tempo;
            int startPx = centerX + (int)((startT - audioTempoAtual) * pixelsPerSecond);
            if (startPx > -10 && startPx < 1920 + 10) {
                desenharRetanguloAlpha(startPx, painelY + 30, 2, 200, 0xEE38BDF8);
            }
        }
    }

    if (usandoAcordesFalsos) {
        desenharTexto("ARQUIVO (.TXT) NAO ENCONTRADO! EXIBINDO ACORDES DE TESTE", 20, centerX + 50, painelY + 20, 0xFFFFFFFF);
    } else if (totalAcordesCarregados == 0) {
        desenharTexto("ARQUIVO DE ACORDES (.TXT) NAO ENCONTRADO!", 35, centerX + 50, painelY + 100, 0xFFFFFFFF);
        desenharTexto("Crie um arquivo de texto com o mesmo nome da musica e os tempos dos acordes.", 20, centerX + 50, painelY + 150, 0xFFFFFFFF);
    }

    desenharRetanguloAlpha(centerX - 1, painelY + 20, 4, 220, 0xFF00FF00);

    const char* acordeTocando = "...";
    if (totalAcordesCarregados > 0) {
        for (int i = totalAcordesCarregados - 1; i >= 0; i--) {
            if (audioTempoAtual >= acordesCarregados[i].tempo) {
                acordeTocando = acordesCarregados[i].nome;
                break;
            }
        }
    }

    char textoAtual[128];
    sprintf(textoAtual, "ACORDE ATUAL: %s", acordeTocando);
    desenharTexto(textoAtual, 25, 50, painelY + 30, 0xFF38BDF8);
}

void initInstrumentos(void) {
    if (!instRenderTextureLoaded) {
        instRenderTexture = LoadRenderTexture(1920, 1080);
        instRenderTextureLoaded = true;
    }
    strcpy(instCaminhoAtual, "data/HyperNeiva/midia/musicas");
    instListarMusicas(instCaminhoAtual);
    modoInstrumento = 0;
    violaoMenuEstado = 0;
}

void fecharInstrumentos(void) {
    if (instRenderTextureLoaded) {
        UnloadRenderTexture(instRenderTexture);
        instRenderTextureLoaded = false;
    }
}

void renderizarInstrumentosRaylib(void) {
    if (!instRenderTextureLoaded) return;

    Vector2 mousePosRaw = GetMousePosition();
    Vector2 mousePos = {
        mousePosRaw.x * (1920.0f / (float)GetScreenWidth()),
        mousePosRaw.y * (1080.0f / (float)GetScreenHeight())
    };
    bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool clickedReleased = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
    bool holding = IsMouseButtonDown(MOUSE_LEFT_BUTTON);

    int vltX = 1600, vltY = 80, vltW = 220, vltH = 50;
    bool hoverVoltar = (mousePos.x >= vltX && mousePos.x <= vltX + vltW && mousePos.y >= vltY && mousePos.y <= vltY + vltH);
    if (hoverVoltar && clicked) {
        fecharInstrumentos();
        voltarParaMenuExtra();
    }

    // Keyboard controls for Piano Scale/Instrument Toggle/Menu
    if (IsKeyPressed(KEY_I)) {
        modoInstrumento = (modoInstrumento == 0) ? 1 : 0;
    }
    if (IsKeyPressed(KEY_M) || IsKeyPressed(KEY_TAB)) {
        if (violaoMenuEstado == 0) {
            violaoMenuEstado = 1;
            cursorMenuViolao = 0;
        } else {
            violaoMenuEstado = 0;
        }
    }

    // Capture sound trigger events for mixing thread
    if (violaoMenuEstado == 0) {
        if (modoInstrumento == 0) {
            // Piano Key Mapping (A, W, S, E, D, F, T, G, Y, H, U, J, K)
            pianoKeysPressionadas[0] = IsKeyDown(KEY_A);
            pianoKeysPressionadas[1] = IsKeyDown(KEY_W);
            pianoKeysPressionadas[2] = IsKeyDown(KEY_S);
            pianoKeysPressionadas[3] = IsKeyDown(KEY_E);
            pianoKeysPressionadas[4] = IsKeyDown(KEY_D);
            pianoKeysPressionadas[5] = IsKeyDown(KEY_F);
            pianoKeysPressionadas[6] = IsKeyDown(KEY_T);
            pianoKeysPressionadas[7] = IsKeyDown(KEY_G);
            pianoKeysPressionadas[8] = IsKeyDown(KEY_Y);
            pianoKeysPressionadas[9] = IsKeyDown(KEY_H);
            pianoKeysPressionadas[10] = IsKeyDown(KEY_U);
            pianoKeysPressionadas[11] = IsKeyDown(KEY_J);
            pianoKeysPressionadas[12] = IsKeyDown(KEY_K);

            // Octave controls
            if (IsKeyPressed(KEY_Z) && oitavaOffset > -2) oitavaOffset--;
            if (IsKeyPressed(KEY_X) && oitavaOffset < 3) oitavaOffset++;

            // Mouse click support for Piano keys
            int startX = 360; int startY = 220; int whiteW = 150; int whiteH = 500; int blackW = 80; int blackH = 300;
            int whiteMap[8] = { 0, 2, 4, 5, 7, 9, 11, 12 };
            int blackMap[5] = { 1, 3, 6, 8, 10 };
            int blackOffsets[5] = { 1, 2, 4, 5, 6 };

            if (holding && mousePos.y >= startY && mousePos.y <= startY + whiteH && mousePos.x >= startX && mousePos.x <= startX + 8 * whiteW) {
                // Determine if mouse is over a black key first
                int hitKey = -1;
                if (mousePos.y <= startY + blackH) {
                    for (int i = 0; i < 5; i++) {
                        int kX = startX + (blackOffsets[i] * whiteW) - (blackW / 2);
                        if (mousePos.x >= kX && mousePos.x <= kX + blackW) {
                            hitKey = blackMap[i];
                            break;
                        }
                    }
                }
                // If not hit black key, determine white key
                if (hitKey == -1) {
                    int col = (int)((mousePos.x - startX) / whiteW);
                    if (col >= 0 && col < 8) {
                        hitKey = whiteMap[col];
                    }
                }
                if (hitKey != -1) {
                    pianoKeysPressionadas[hitKey] = true;
                }
            }
        } else {
            // Violao Controls
            // Chords selection: Keyboard 1, 2, 3, 4, 5, 6
            if (IsKeyPressed(KEY_ONE)) acordeAtual = 0;
            if (IsKeyPressed(KEY_TWO)) acordeAtual = 1;
            if (IsKeyPressed(KEY_THREE)) acordeAtual = 2;
            if (IsKeyPressed(KEY_FOUR)) acordeAtual = 3;
            if (IsKeyPressed(KEY_FIVE)) acordeAtual = 4;
            if (IsKeyPressed(KEY_SIX)) acordeAtual = 5;

            // Change chord group: LEFT/RIGHT
            if (IsKeyPressed(KEY_LEFT)) {
                grupoAcordeAtual--;
                if (grupoAcordeAtual < 0) grupoAcordeAtual = GRUPOS_POR_COLETANEA - 1;
            }
            if (IsKeyPressed(KEY_RIGHT)) {
                grupoAcordeAtual++;
                if (grupoAcordeAtual >= GRUPOS_POR_COLETANEA) grupoAcordeAtual = 0;
            }

            // Mouse click on chord boxes
            int cx = 250; int cy = 500; int radius = 180; int boxW = 120; int boxH = 60;
            int boxX[6] = { cx - boxW / 2, cx + (int)(radius * 0.866f) - boxW / 2, cx + (int)(radius * 0.866f) - boxW / 2, cx - boxW / 2, cx - (int)(radius * 0.866f) - boxW / 2, cx - (int)(radius * 0.866f) - boxW / 2 };
            int boxY[6] = { cy - radius - boxH / 2, cy - (int)(radius * 0.5f) - boxH / 2, cy + (int)(radius * 0.5f) - boxH / 2, cy + radius - boxH / 2, cy + (int)(radius * 0.5f) - boxH / 2, cy - (int)(radius * 0.5f) - boxH / 2 };

            if (clicked) {
                for (int i = 0; i < 6; i++) {
                    if (mousePos.x >= boxX[i] && mousePos.x <= boxX[i] + boxW && mousePos.y >= boxY[i] && mousePos.y <= boxY[i] + boxH) {
                        acordeAtual = i;
                        break;
                    }
                }
            }

            // Strumming: SPACE (strum down) / SHIFT (strum up)
            if (IsKeyPressed(KEY_SPACE)) {
                for (int i = 0; i < 6; i++) ks_strumTimer[i] = i * 250;
            }
            if (IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT)) {
                for (int i = 0; i < 6; i++) ks_strumTimer[5 - i] = i * 250;
            }

            // Mouse swipe strumming support
            static float lastMouseX = -1.0f;
            if (holding && mousePos.x >= 600 && mousePos.x <= 1100 && mousePos.y >= 100 && mousePos.y <= 480) {
                if (lastMouseX != -1.0f) {
                    float diffX = mousePos.x - lastMouseX;
                    if (fabsf(diffX) > 25.0f) {
                        if (diffX > 0.0f) {
                            for (int i = 0; i < 6; i++) ks_strumTimer[i] = i * 250;
                        } else {
                            for (int i = 0; i < 6; i++) ks_strumTimer[5 - i] = i * 250;
                        }
                    }
                }
                lastMouseX = mousePos.x;
            } else {
                lastMouseX = -1.0f;
            }

            // Plucking individual strings: Q, W, E, R, T, Y
            int pluckKeys[6] = { KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y };
            for (int i = 0; i < 6; i++) {
                if (IsKeyPressed(pluckKeys[i])) {
                    ks_strumTimer[i] = 1;
                }
            }
        }
    } else if (violaoMenuEstado == 1) {
        // Options Menu Navigation
        if (IsKeyPressed(KEY_UP)) {
            cursorMenuViolao--;
            if (cursorMenuViolao < 0) cursorMenuViolao = 3 + totalColetaneas;
        }
        if (IsKeyPressed(KEY_DOWN)) {
            cursorMenuViolao++;
            if (cursorMenuViolao > 3 + totalColetaneas) cursorMenuViolao = 0;
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || clicked) {
            // Click support on items
            int hitIndex = cursorMenuViolao;
            if (clicked) {
                hitIndex = -1;
                for (int i = 0; i < 4 + totalColetaneas; i++) {
                    int itemY = 300 + (i * 50);
                    if (mousePos.x >= 600 && mousePos.x <= 1200 && mousePos.y >= itemY && mousePos.y <= itemY + 40) {
                        hitIndex = i;
                        break;
                    }
                }
            }

            if (hitIndex != -1) {
                if (hitIndex == 0) violaoMenuEstado = 0;
                else if (hitIndex == 1) {
                    instListarMusicas(instCaminhoAtual);
                    violaoMenuEstado = 3;
                } else if (hitIndex == 2) {
                    if (totalColetaneas < MAX_COLETANEAS) {
                        for (int g = 0; g < GRUPOS_POR_COLETANEA; g++) {
                            for (int c = 0; c < 6; c++) { coletaneaEmEdicao.grupoOrigem[g][c] = g; coletaneaEmEdicao.indiceOrigem[g][c] = c; }
                        }
                        cursorEdicaoAcorde = 0; grupoEdicaoAtual = 0; violaoMenuEstado = 2;
                    }
                } else if (hitIndex == 3) {
                    totalColetaneas = 0; usandoColetaneaCustomizada = false; violaoMenuEstado = 0;
                } else {
                    indiceColetaneaAtual = hitIndex - 4; usandoColetaneaCustomizada = true; grupoAcordeAtual = 0; violaoMenuEstado = 0;
                }
            }
        }
    } else if (violaoMenuEstado == 2) {
        // Chord Creator Menu
        if (IsKeyPressed(KEY_UP)) {
            cursorEdicaoAcorde--; if (cursorEdicaoAcorde < 0) cursorEdicaoAcorde = 5;
        }
        if (IsKeyPressed(KEY_DOWN)) {
            cursorEdicaoAcorde++; if (cursorEdicaoAcorde > 5) cursorEdicaoAcorde = 0;
        }
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)) {
            int flatIndex = coletaneaEmEdicao.grupoOrigem[grupoEdicaoAtual][cursorEdicaoAcorde] * 6 + coletaneaEmEdicao.indiceOrigem[grupoEdicaoAtual][cursorEdicaoAcorde];
            if (IsKeyPressed(KEY_LEFT)) { flatIndex--; if (flatIndex < 0) flatIndex = 41; }
            if (IsKeyPressed(KEY_RIGHT)) { flatIndex++; if (flatIndex > 41) flatIndex = 0; }
            coletaneaEmEdicao.grupoOrigem[grupoEdicaoAtual][cursorEdicaoAcorde] = flatIndex / 6;
            coletaneaEmEdicao.indiceOrigem[grupoEdicaoAtual][cursorEdicaoAcorde] = flatIndex % 6;
        }

        // Change group being edited
        if (IsKeyPressed(KEY_PAGE_UP)) { grupoEdicaoAtual--; if (grupoEdicaoAtual < 0) grupoEdicaoAtual = GRUPOS_POR_COLETANEA - 1; }
        if (IsKeyPressed(KEY_PAGE_DOWN)) { grupoEdicaoAtual++; if (grupoEdicaoAtual >= GRUPOS_POR_COLETANEA) grupoEdicaoAtual = 0; }

        // Click support on items to edit
        if (clicked) {
            for (int i = 0; i < 6; i++) {
                int itemY = 300 + (i * 60);
                if (mousePos.x >= 600 && mousePos.x <= 1200 && mousePos.y >= itemY && mousePos.y <= itemY + 50) {
                    cursorEdicaoAcorde = i;
                    // Trigger adjustment on click
                    int flatIndex = coletaneaEmEdicao.grupoOrigem[grupoEdicaoAtual][cursorEdicaoAcorde] * 6 + coletaneaEmEdicao.indiceOrigem[grupoEdicaoAtual][cursorEdicaoAcorde];
                    flatIndex = (flatIndex + 1) % 42;
                    coletaneaEmEdicao.grupoOrigem[grupoEdicaoAtual][cursorEdicaoAcorde] = flatIndex / 6;
                    coletaneaEmEdicao.indiceOrigem[grupoEdicaoAtual][cursorEdicaoAcorde] = flatIndex % 6;
                    break;
                }
            }
            // Click to Save/Name
            if (mousePos.x >= 500 && mousePos.x <= 900 && mousePos.y >= 220 && mousePos.y <= 270) {
                char bufferNome[64];
                sprintf(bufferNome, "Minha Coletanea %d", totalColetaneas + 1);
                strcpy(coletaneaEmEdicao.nome, bufferNome);
                coletaneasCustomizadas[totalColetaneas] = coletaneaEmEdicao;
                indiceColetaneaAtual = totalColetaneas;
                totalColetaneas++;
                usandoColetaneaCustomizada = true;
                grupoAcordeAtual = 0;
                violaoMenuEstado = 0;
            }
        }

        // Save
        if (IsKeyPressed(KEY_S)) {
            char bufferNome[64];
            sprintf(bufferNome, "Minha Coletanea %d", totalColetaneas + 1);
            strcpy(coletaneaEmEdicao.nome, bufferNome);
            coletaneasCustomizadas[totalColetaneas] = coletaneaEmEdicao;
            indiceColetaneaAtual = totalColetaneas;
            totalColetaneas++;
            usandoColetaneaCustomizada = true;
            grupoAcordeAtual = 0;
            violaoMenuEstado = 0;
        }
    } else if (violaoMenuEstado == 3) {
        // File selection menu
        if (IsKeyPressed(KEY_UP)) {
            instSel--;
            if (instSel < 0) instSel = instTotalItens > 0 ? instTotalItens - 1 : 0;
            if (instSel < instScroll) instScroll = instSel;
            if (instSel > instScroll + 14) instScroll = instSel - 14;
        }
        if (IsKeyPressed(KEY_DOWN)) {
            instSel++;
            if (instSel >= instTotalItens) instSel = 0;
            if (instSel > instScroll + 14) instScroll = instSel - 14;
            if (instSel < instScroll) instScroll = instSel;
        }

        // Mouse selection
        if (clicked) {
            int maxVis = 15;
            for (int i = 0; i < maxVis && (i + instScroll) < instTotalItens; i++) {
                int idx = i + instScroll;
                int itemY = 250 + (i * 45);
                if (mousePos.x >= 400 && mousePos.x <= 1400 && mousePos.y >= itemY && mousePos.y <= itemY + 40) {
                    instSel = idx;
                    // Trigger selection
                    if (instEhPasta[instSel]) {
                        if (strcmp(instPaths[instSel], "..") == 0) {
                            char* lastSlash = strrchr(instCaminhoAtual, '/');
                            if (lastSlash && lastSlash != instCaminhoAtual) {
                                *lastSlash = '\0';
                                instListarMusicas(instCaminhoAtual);
                            }
                        } else {
                            strcpy(instCaminhoAtual, instPaths[instSel]);
                            instListarMusicas(instCaminhoAtual);
                        }
                    } else {
                        tocarMusicaNova(instPaths[instSel]);
                        carregarAcordesDaMusica(instPaths[instSel]);
                        violaoMenuEstado = 0;
                    }
                    break;
                }
            }
        }

        if (IsKeyPressed(KEY_ENTER)) {
            if (instTotalItens > 0) {
                if (instEhPasta[instSel]) {
                    if (strcmp(instPaths[instSel], "..") == 0) {
                        char* lastSlash = strrchr(instCaminhoAtual, '/');
                        if (lastSlash && lastSlash != instCaminhoAtual) {
                            *lastSlash = '\0';
                            instListarMusicas(instCaminhoAtual);
                        }
                    } else {
                        strcpy(instCaminhoAtual, instPaths[instSel]);
                        instListarMusicas(instCaminhoAtual);
                    }
                } else {
                    tocarMusicaNova(instPaths[instSel]);
                    carregarAcordesDaMusica(instPaths[instSel]);
                    violaoMenuEstado = 0;
                }
            }
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            violaoMenuEstado = 1;
        }
    }

    // DRAWING EVERYTHING ONTO THE 1920x1080 RENDER TEXTURE
    BeginTextureMode(instRenderTexture);
    ClearBackground(BLACK);

    uint32_t* dummyPixels = NULL; // We don't write pixels directly, we use Raylib draw calls!

    if (violaoMenuEstado == 0) {
        // DRAW INSTRUMENTS CANVAS
        desenharRetanguloAlpha(0, 0, 1920, 1080, 0xFF151515);

        desenharTexto("SISTEMA DE INSTRUMENTOS VIRTUAIS", 40, 50, 50, 0xFF00AAFF);
        desenharTexto("[I] Alternar Modo  |  [M/TAB] Menu Opcoes  |  [ESC] Sair", 25, 50, 100, 0xFFAAAAAA);

        // Draw Voltar Button
        desenharRetanguloAlpha(vltX, vltY, vltW, vltH, hoverVoltar ? 0xFF00AAFF : 0xFF333333);
        DrawRectangleLines(vltX, vltY, vltW, vltH, WHITE);
        int txtW = MeasureText("VOLTAR", 18);
        desenharTexto("VOLTAR", 18, vltX + (vltW - txtW)/2, vltY + 16, 0xFFFFFFFF);

        if (modoInstrumento == 0) {
            char txtOitava[128]; sprintf(txtOitava, "Escala Musical Atual: Oitava %d (Z / X para mudar)", 4 + oitavaOffset);
            desenharTexto(txtOitava, 30, 50, 150, 0xFF00FF00);

            int startX = 360; int startY = 220; int whiteW = 150; int whiteH = 500; int blackW = 80; int blackH = 300;
            int whiteMap[8] = { 0, 2, 4, 5, 7, 9, 11, 12 };
            const char* whiteLabels[8] = { "[ A ]", "[ S ]", "[ D ]", "[ F ]", "[ G ]", "[ H ]", "[ J ]", "[ K ]" };
            const char* whiteNotes[8] = { "DO", "RE", "MI", "FA", "SOL", "LA", "SI", "DO" };

            for (int i = 0; i < 8; i++) {
                int keyIndex = whiteMap[i]; bool isPressed = pianoKeysPressionadas[keyIndex];
                uint32_t color = isPressed ? 0xFF00AAFF : 0xFFEEEEEE; uint32_t borderColor = 0xFF555555;
                int kX = startX + (i * whiteW);
                
                desenharRetanguloAlpha(kX, startY, whiteW, whiteH, color);
                // Draw borders
                desenharRetanguloAlpha(kX, startY, whiteW, 2, borderColor);
                desenharRetanguloAlpha(kX, startY + whiteH - 2, whiteW, 2, borderColor);
                desenharRetanguloAlpha(kX, startY, 2, whiteH, borderColor);
                desenharRetanguloAlpha(kX + whiteW - 2, startY, 2, whiteH, borderColor);

                desenharTexto(whiteNotes[i], 30, kX + 50, startY + whiteH - 80, isPressed ? 0xFFFFFFFF : 0xFF000000);
                desenharTexto(whiteLabels[i], 20, kX + 40, startY + whiteH - 30, isPressed ? 0xFFFFFFFF : 0xFF555555);
            }

            int blackMap[5] = { 1, 3, 6, 8, 10 }; 
            int blackOffsets[5] = { 1, 2, 4, 5, 6 }; 
            const char* blackLabels[5] = { "[ W ]", "[ E ]", "[ T ]", "[ Y ]", "[ U ]" };
            
            for (int i = 0; i < 5; i++) {
                int keyIndex = blackMap[i]; bool isPressed = pianoKeysPressionadas[keyIndex];
                uint32_t color = isPressed ? 0xFF00AAFF : 0xFF222222; 
                int kX = startX + (blackOffsets[i] * whiteW) - (blackW / 2);
                
                desenharRetanguloAlpha(kX, startY, blackW, blackH, color);
                desenharTexto(blackLabels[i], 18, kX + 10, startY + blackH - 30, 0xFFFFFFFF);
            }
        } else {
            desenharTexto("MAO ESQUERDA: Clique nos Acordes ou use as teclas [1] a [6]", 25, 50, 200, 0xFF00FF00);

            char txtGrupo[128];
            if (usandoColetaneaCustomizada) {
                sprintf(txtGrupo, "[SETAS ESQ/DIR] para trocar - Custom: %s (Grupo %d/7)", coletaneasCustomizadas[indiceColetaneaAtual].nome, grupoAcordeAtual + 1);
            } else {
                sprintf(txtGrupo, "[SETAS ESQ/DIR] para trocar - Coletanea Padrao: Grupo %d/7", grupoAcordeAtual + 1);
            }
            desenharTexto(txtGrupo, 25, 50, 250, 0xFF00FFFF);

            int cx = 250; int cy = 500; int radius = 180; int boxW = 120; int boxH = 60;
            int boxX[6] = { cx - boxW / 2, cx + (int)(radius * 0.866f) - boxW / 2, cx + (int)(radius * 0.866f) - boxW / 2, cx - boxW / 2, cx - (int)(radius * 0.866f) - boxW / 2, cx - (int)(radius * 0.866f) - boxW / 2 };
            int boxY[6] = { cy - radius - boxH / 2, cy - (int)(radius * 0.5f) - boxH / 2, cy + (int)(radius * 0.5f) - boxH / 2, cy + radius - boxH / 2, cy + (int)(radius * 0.5f) - boxH / 2, cy - (int)(radius * 0.5f) - boxH / 2 };

            for (int i = 0; i < 6; i++) {
                uint32_t color = (acordeAtual == i) ? 0xFF00AAFF : 0xFF333333;
                desenharRetanguloAlpha(boxX[i], boxY[i], boxW, boxH, color);

                if (usandoColetaneaCustomizada) {
                    int gOrig = coletaneasCustomizadas[indiceColetaneaAtual].grupoOrigem[grupoAcordeAtual][i];
                    int iOrig = coletaneasCustomizadas[indiceColetaneaAtual].indiceOrigem[grupoAcordeAtual][i];
                    desenharTexto(nomeAcordes[gOrig][iOrig], 30, boxX[i] + 35, boxY[i] + 40, 0xFFFFFFFF);
                } else {
                    desenharTexto(nomeAcordes[grupoAcordeAtual][i], 30, boxX[i] + 35, boxY[i] + 40, 0xFFFFFFFF);
                }
            }

            // Chord groups table
            int gridX = 1150; int gridY = 150;
            desenharTexto("TABELA DE GRUPOS", 25, gridX, gridY - 50, 0xFF00AAFF);
            for (int g = 0; g < GRUPOS_POR_COLETANEA; g++) {
                char headerG[8]; sprintf(headerG, "G%d", g + 1);
                int cX = gridX + (g * 105); 
                int cY = gridY;
                desenharTexto(headerG, 30, cX, cY, (grupoAcordeAtual == g) ? 0xFF00FFFF : 0xFFFFFFFF);
                for (int c = 0; c < 6; c++) {
                    if (usandoColetaneaCustomizada) {
                        int gO = coletaneasCustomizadas[indiceColetaneaAtual].grupoOrigem[g][c];
                        int iO = coletaneasCustomizadas[indiceColetaneaAtual].indiceOrigem[g][c];
                        desenharTexto(nomeAcordes[gO][iO], 30, cX, cY + 45 + c * 40, 0xFFAAAAAA);
                    } else {
                        desenharTexto(nomeAcordes[g][c], 30, cX, cY + 45 + c * 40, 0xFFAAAAAA);
                    }
                }
            }

            // Right hand (Plucking/Strumming)
            int rhY = 150;
            int rhX = 650; 
            desenharTexto("MAO DIREITA: Strum e Dedilhado", 25, rhX, rhY, 0xFF00AAFF);
            desenharTexto("Use [ESPACO] para palhetar ou passe o mouse", 20, rhX, rhY + 40, 0xFFFFFFFF);

            char bufCordas[128];
            int gReal = usandoColetaneaCustomizada ? coletaneasCustomizadas[indiceColetaneaAtual].grupoOrigem[grupoAcordeAtual][acordeAtual] : grupoAcordeAtual;
            int iReal = usandoColetaneaCustomizada ? coletaneasCustomizadas[indiceColetaneaAtual].indiceOrigem[grupoAcordeAtual][acordeAtual] : acordeAtual;

            sprintf(bufCordas, "[ Q ] - Corda 6 (%s)", notasAcordes[gReal][iReal][0]); desenharTexto(bufCordas, 30, rhX, rhY + 110, 0xFFEEEEEE);
            sprintf(bufCordas, "[ W ] - Corda 5 (%s)", notasAcordes[gReal][iReal][1]); desenharTexto(bufCordas, 30, rhX, rhY + 160, 0xFFEEEEEE);
            sprintf(bufCordas, "[ E ] - Corda 4 (%s)", notasAcordes[gReal][iReal][2]); desenharTexto(bufCordas, 30, rhX, rhY + 210, 0xFFEEEEEE);
            sprintf(bufCordas, "[ R ] - Corda 3 (%s)", notasAcordes[gReal][iReal][3]); desenharTexto(bufCordas, 30, rhX, rhY + 260, 0xFFEEEEEE);
            sprintf(bufCordas, "[ T ] - Corda 2 (%s)", notasAcordes[gReal][iReal][4]); desenharTexto(bufCordas, 30, rhX, rhY + 310, 0xFFEEEEEE);
            sprintf(bufCordas, "[ Y ] - Corda 1 (%s)", notasAcordes[gReal][iReal][5]); desenharTexto(bufCordas, 30, rhX, rhY + 360, 0xFFEEEEEE);
        }

        renderizarGrelhaAcordes();
    } else {
        // Dim screen background slightly for menus
        desenharRetanguloAlpha(0, 0, 1920, 1080, 0xDD0A0A0A);

        if (violaoMenuEstado == 1) {
            desenharTexto("OPCOES DO ESTUDIO (M/TAB PARA FECHAR)", 40, 500, 200, 0xFF00AAFF);
            const char* menuOpts[] = {
                "Voltar ao Instrumento",
                "Escolher Musica de Fundo (MP3/WAV)",
                "Criar Nova Coletanea Customizada",
                "Restaurar Padrao Original"
            };
            int numBase = 4;

            for (int i = 0; i < numBase + totalColetaneas; i++) {
                uint32_t cor = (cursorMenuViolao == i) ? 0xFF00FFFF : 0xFFFFFFFF;
                if (i < numBase) {
                    desenharTexto(menuOpts[i], 30, 600, 300 + (i * 50), cor);
                } else {
                    char bufG[128]; sprintf(bufG, "Carregar: %s", coletaneasCustomizadas[i - numBase].nome);
                    desenharTexto(bufG, 30, 600, 300 + (i * 50), cor);
                }
            }
        } else if (violaoMenuEstado == 2) {
            desenharTexto("CRIADOR DE COLETANEA VIRTUAL", 40, 500, 100, 0xFF00AAFF);
            char bufG[64]; sprintf(bufG, "GRUPO ATUAL: %d DE 7 (Use [PAGE UP/DOWN] para mudar de grupo)", grupoEdicaoAtual + 1);
            desenharTexto(bufG, 30, 500, 160, 0xFF00FFFF);
            desenharTexto("[CIMA/BAIXO] Escolher Posicao | [ESQ/DIR] Mudar Acorde | [S] Salvar", 20, 500, 220, 0xFF00FF00);

            for (int i = 0; i < 6; i++) {
                uint32_t cor = (cursorEdicaoAcorde == i) ? 0xFF00FFFF : 0xFFFFFFFF;
                int gOrig = coletaneaEmEdicao.grupoOrigem[grupoEdicaoAtual][i];
                int iOrig = coletaneaEmEdicao.indiceOrigem[grupoEdicaoAtual][i];
                char bufL[128]; sprintf(bufL, "Posicao %d: < %s >", i + 1, nomeAcordes[gOrig][iOrig]);
                desenharTexto(bufL, 35, 600, 300 + (i * 60), cor);
            }
            desenharRetanguloAlpha(500, 720, 400, 50, 0xFF333333);
            desenharTexto("CLIQUE AQUI OU APERTE [S] PARA SALVAR E VOLTAR", 20, 520, 750, 0xFFFFFFFF);
        } else if (violaoMenuEstado == 3) {
            desenharTexto("SELECIONAR FAIXA DE AUDIO", 40, 400, 100, 0xFF00AAFF);
            char bufDir[512]; sprintf(bufDir, "PASTA ATUAL: %s", instCaminhoAtual);
            desenharTexto(bufDir, 20, 400, 150, 0xFFAAAAAA);
            desenharTexto("[ENTER] Carregar e Tocar Musica | [BACKSPACE] Voltar", 20, 400, 180, 0xFF00FF00);

            int maxVis = 15;
            for (int i = 0; i < maxVis && (i + instScroll) < instTotalItens; i++) {
                int idx = i + instScroll;
                uint32_t cor = (idx == instSel) ? 0xFF00FFFF : (instEhPasta[idx] ? 0xFFEAB308 : 0xFFFFFFFF);
                desenharTexto(instNomes[idx], 30, 400, 250 + (i * 45), cor);
            }

            if (instTotalItens == 0) {
                desenharTexto("Nenhuma musica MP3 ou WAV encontrada nesta pasta.", 30, 400, 250, 0xFF555555);
            }
        }
    }

    EndTextureMode();

    // DRAW THE RENDERED CANVAS ONTO REAL SCREEN SCALED DOWN
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();
    DrawTexturePro(instRenderTexture.texture, 
                  (Rectangle){ 0, 0, (float)instRenderTexture.texture.width, (float)-instRenderTexture.texture.height }, 
                  (Rectangle){ 0, 0, (float)screenWidth, (float)screenHeight }, 
                  (Vector2){ 0, 0 }, 0.0f, WHITE);
}

void misturarAudioPiano(int16_t* bufferAudio, size_t frames) {
    if (modoInstrumento == 0) {
        float multiplicadorEscalaPiano = powf(2.0f, (float)oitavaOffset);
        for (size_t i = 0; i < frames; i++) {
            float mixTotal = 0.0f;
            for (int k = 0; k < 13; k++) {
                if (pianoKeysPressionadas[k]) { 
                    volumeTecla[k] += 0.005f; 
                    if (volumeTecla[k] > 1.0f) volumeTecla[k] = 1.0f; 
                } else { 
                    volumeTecla[k] -= 0.002f; 
                    if (volumeTecla[k] < 0.0f) { 
                        volumeTecla[k] = 0.0f; 
                        pianoFases[k] = 0.0f; 
                    } 
                }
                if (volumeTecla[k] > 0.0f) {
                    float freqAtual = pianoFreqsBase[k] * multiplicadorEscalaPiano;
                    mixTotal += sinf(pianoFases[k]) * (12000.0f * volumeTecla[k]);
                    pianoFases[k] += (2.0f * 3.1415926535f * freqAtual) / 48000.0f;
                    if (pianoFases[k] > 2.0f * 3.1415926535f) pianoFases[k] -= 2.0f * 3.1415926535f;
                }
            }
            if (mixTotal != 0.0f) {
                int32_t mixedL = bufferAudio[i * 2] + (int32_t)mixTotal; 
                int32_t mixedR = bufferAudio[i * 2 + 1] + (int32_t)mixTotal;
                if (mixedL > 32767) mixedL = 32767; else if (mixedL < -32768) mixedL = -32768;
                if (mixedR > 32767) mixedR = 32767; else if (mixedR < -32768) mixedR = -32768;
                bufferAudio[i * 2] = (int16_t)mixedL; 
                bufferAudio[i * 2 + 1] = (int16_t)mixedR;
            }
        }
    } else {
        // Violao
        for (size_t i = 0; i < frames; i++) {
            float mixTotal = 0.0f;
            for (int k = 0; k < 6; k++) {
                if (ks_strumTimer[k] > 0) {
                    ks_strumTimer[k]--;
                    if (ks_strumTimer[k] == 0) {
                        int gReal = usandoColetaneaCustomizada ? coletaneasCustomizadas[indiceColetaneaAtual].grupoOrigem[grupoAcordeAtual][acordeAtual] : grupoAcordeAtual;
                        int iReal = usandoColetaneaCustomizada ? coletaneasCustomizadas[indiceColetaneaAtual].indiceOrigem[grupoAcordeAtual][acordeAtual] : acordeAtual;

                        int len = (int)(48000.0f / frequenciasViolao[gReal][iReal][k]);
                        if (len > 2047) len = 2047;
                        ks_delayLength[k] = len; ks_delayPtr[k] = 0;
                        for (int j = 0; j < len; j++) ks_delayLines[k][j] = ((rand() % 10000) / 5000.0f - 1.0f);
                    }
                }
                if (ks_delayLength[k] > 0) {
                    int ptr = ks_delayPtr[k]; 
                    int prevPtr = (ptr - 1 < 0) ? (ks_delayLength[k] - 1) : (ptr - 1);
                    float current = ks_delayLines[k][ptr]; 
                    float prev = ks_delayLines[k][prevPtr];
                    float newVal = 0.998f * 0.5f * (current + prev);
                    ks_delayLines[k][ptr] = newVal; 
                    ks_delayPtr[k] = (ptr + 1) % ks_delayLength[k];
                    mixTotal += newVal * 15000.0f;
                }
            }
            if (mixTotal != 0.0f) {
                int32_t mixedL = bufferAudio[i * 2] + (int32_t)mixTotal; 
                int32_t mixedR = bufferAudio[i * 2 + 1] + (int32_t)mixTotal;
                if (mixedL > 32767) mixedL = 32767; else if (mixedL < -32768) mixedL = -32768;
                if (mixedR > 32767) mixedR = 32767; else if (mixedR < -32768) mixedR = -32768;
                bufferAudio[i * 2] = (int16_t)mixedL; 
                bufferAudio[i * 2 + 1] = (int16_t)mixedR;
            }
        }
    }
}
