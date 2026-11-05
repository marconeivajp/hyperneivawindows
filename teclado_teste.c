#include "teclado_teste.h"
#include "raylib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include <ctype.h>

// RenderTexture for drawing at 1920x1080 and scaling to screen
static RenderTexture2D kbRenderTexture;
static bool kbRenderTextureLoaded = false;

extern void voltarParaMenuExtra(void);

typedef enum {
    KB_STATE_TEST,
    KB_STATE_OPEN_EXPLORER,
    KB_STATE_SAVE_DIALOG
} KBSubState;

static KBSubState kbState = KB_STATE_TEST;

// Directory explorer variables for keyboard test saving
static char kbCaminhoAtual[512] = "data/HyperNeiva";
static char kbNomes[200][64];
static char kbPaths[200][256];
static bool kbEhPasta[200];
static int kbTotalItens = 0;
static int kbSel = 0;
static int kbScroll = 0;
static char kbSaveFilename[128] = "";

typedef struct {
    int key;
    const char* label;
    int x, y;
    int w, h;
    float flash;
    bool tested;
} KeyVisual;

// Comprehensive layout for keyboard test
static KeyVisual kbKeys[] = {
    // Row 1 (y = 0)
    { KEY_ESCAPE, "ESC", 0, 0, 85, 80, 0.0f, false },
    { KEY_ONE, "1", 95, 0, 85, 80, 0.0f, false },
    { KEY_TWO, "2", 190, 0, 85, 80, 0.0f, false },
    { KEY_THREE, "3", 285, 0, 85, 80, 0.0f, false },
    { KEY_FOUR, "4", 380, 0, 85, 80, 0.0f, false },
    { KEY_FIVE, "5", 475, 0, 85, 80, 0.0f, false },
    { KEY_SIX, "6", 570, 0, 85, 80, 0.0f, false },
    { KEY_SEVEN, "7", 665, 0, 85, 80, 0.0f, false },
    { KEY_EIGHT, "8", 760, 0, 85, 80, 0.0f, false },
    { KEY_NINE, "9", 855, 0, 85, 80, 0.0f, false },
    { KEY_ZERO, "0", 950, 0, 85, 80, 0.0f, false },
    { KEY_MINUS, "-", 1045, 0, 85, 80, 0.0f, false },
    { KEY_EQUAL, "=", 1140, 0, 85, 80, 0.0f, false },
    { KEY_BACKSPACE, "BACKSPACE", 1235, 0, 165, 80, 0.0f, false },

    // Row 2 (y = 90)
    { KEY_TAB, "TAB", 0, 90, 120, 80, 0.0f, false },
    { KEY_Q, "Q", 130, 90, 85, 80, 0.0f, false },
    { KEY_W, "W", 225, 90, 85, 80, 0.0f, false },
    { KEY_E, "E", 320, 90, 85, 80, 0.0f, false },
    { KEY_R, "R", 415, 90, 85, 80, 0.0f, false },
    { KEY_T, "T", 510, 90, 85, 80, 0.0f, false },
    { KEY_Y, "Y", 605, 90, 85, 80, 0.0f, false },
    { KEY_U, "U", 700, 90, 85, 80, 0.0f, false },
    { KEY_I, "I", 795, 90, 85, 80, 0.0f, false },
    { KEY_O, "O", 890, 90, 85, 80, 0.0f, false },
    { KEY_P, "P", 985, 90, 85, 80, 0.0f, false },
    { KEY_LEFT_BRACKET, "[", 1080, 90, 85, 80, 0.0f, false },
    { KEY_RIGHT_BRACKET, "]", 1175, 90, 85, 80, 0.0f, false },
    { KEY_ENTER, "ENTER", 1270, 90, 130, 80, 0.0f, false },

    // Row 3 (y = 180)
    { KEY_CAPS_LOCK, "CAPS LOCK", 0, 180, 145, 80, 0.0f, false },
    { KEY_A, "A", 155, 180, 85, 80, 0.0f, false },
    { KEY_S, "S", 250, 180, 85, 80, 0.0f, false },
    { KEY_D, "D", 345, 180, 85, 80, 0.0f, false },
    { KEY_F, "F", 440, 180, 85, 80, 0.0f, false },
    { KEY_G, "G", 535, 180, 85, 80, 0.0f, false },
    { KEY_H, "H", 630, 180, 85, 80, 0.0f, false },
    { KEY_J, "J", 725, 180, 85, 80, 0.0f, false },
    { KEY_K, "K", 820, 180, 85, 80, 0.0f, false },
    { KEY_L, "L", 915, 180, 85, 80, 0.0f, false },
    { KEY_SEMICOLON, ";", 1010, 180, 85, 80, 0.0f, false },
    { KEY_APOSTROPHE, "'", 1105, 180, 85, 80, 0.0f, false },
    { KEY_BACKSLASH, "\\", 1200, 180, 200, 80, 0.0f, false },

    // Row 4 (y = 270)
    { KEY_LEFT_SHIFT, "LSHIFT", 0, 270, 190, 80, 0.0f, false },
    { KEY_Z, "Z", 200, 270, 85, 80, 0.0f, false },
    { KEY_X, "X", 295, 270, 85, 80, 0.0f, false },
    { KEY_C, "C", 390, 270, 85, 80, 0.0f, false },
    { KEY_V, "V", 485, 270, 85, 80, 0.0f, false },
    { KEY_B, "B", 580, 270, 85, 80, 0.0f, false },
    { KEY_N, "N", 675, 270, 85, 80, 0.0f, false },
    { KEY_M, "M", 770, 270, 85, 80, 0.0f, false },
    { KEY_COMMA, ",", 865, 270, 85, 80, 0.0f, false },
    { KEY_PERIOD, ".", 960, 270, 85, 80, 0.0f, false },
    { KEY_SLASH, "/", 1055, 270, 85, 80, 0.0f, false },
    { KEY_RIGHT_SHIFT, "RSHIFT", 1150, 270, 145, 80, 0.0f, false },
    { KEY_UP, "UP", 1305, 270, 95, 80, 0.0f, false },

    // Row 5 (y = 360)
    { KEY_LEFT_CONTROL, "CTRL", 0, 360, 100, 80, 0.0f, false },
    { KEY_LEFT_SUPER, "WIN", 110, 360, 85, 80, 0.0f, false },
    { KEY_LEFT_ALT, "ALT", 205, 360, 85, 80, 0.0f, false },
    { KEY_SPACE, "SPACE", 300, 360, 600, 80, 0.0f, false },
    { KEY_RIGHT_ALT, "ALT GR", 910, 360, 85, 80, 0.0f, false },
    { KEY_RIGHT_CONTROL, "CTRL", 1005, 360, 100, 80, 0.0f, false },
    { KEY_LEFT, "LEFT", 1115, 360, 85, 80, 0.0f, false },
    { KEY_DOWN, "DOWN", 1210, 360, 85, 80, 0.0f, false },
    { KEY_RIGHT, "RIGHT", 1305, 360, 95, 80, 0.0f, false }
};

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

static void kbListarArquivos(const char* path) {
    DIR* d = opendir(path);
    if (!d) return;

    kbTotalItens = 0;

    // Check if we can show "go back" folder
    if (strcmp(path, "data/HyperNeiva") != 0 && strcmp(path, "data") != 0) {
        strcpy(kbNomes[kbTotalItens], "[ .. Voltar Pasta Anterior ]");
        strcpy(kbPaths[kbTotalItens], "..");
        kbEhPasta[kbTotalItens] = true;
        kbTotalItens++;
    }

    struct dirent* dir;
    while ((dir = readdir(d)) != NULL && kbTotalItens < 200) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, dir->d_name);

        struct stat st;
        bool isDir = false;
        if (stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode)) {
            isDir = true;
        }

        if (isDir) {
            snprintf(kbNomes[kbTotalItens], 64, "[ %s ]", dir->d_name);
            strcpy(kbPaths[kbTotalItens], fullPath);
            kbEhPasta[kbTotalItens] = true;
            kbTotalItens++;
        } else {
            char temp[256]; strcpy(temp, dir->d_name);
            for (int i = 0; temp[i]; i++) temp[i] = tolower(temp[i]);
            if (strstr(temp, ".txt")) {
                strncpy(kbNomes[kbTotalItens], dir->d_name, 63);
                strcpy(kbPaths[kbTotalItens], fullPath);
                kbEhPasta[kbTotalItens] = false;
                kbTotalItens++;
            }
        }
    }
    closedir(d);

    // Sort folders first, then files alphabetically
    for (int i = (strcmp(kbPaths[0], "..") == 0 ? 1 : 0); i < kbTotalItens - 1; i++) {
        for (int j = i + 1; j < kbTotalItens; j++) {
            bool swap = false;
            if (kbEhPasta[i] != kbEhPasta[j]) {
                if (!kbEhPasta[i] && kbEhPasta[j]) swap = true;
            } else {
                if (strcasecmp(kbNomes[i], kbNomes[j]) > 0) swap = true;
            }
            if (swap) {
                char tNome[64]; strcpy(tNome, kbNomes[i]); strcpy(kbNomes[i], kbNomes[j]); strcpy(kbNomes[j], tNome);
                char tPath[256]; strcpy(tPath, kbPaths[i]); strcpy(kbPaths[i], kbPaths[j]); strcpy(kbPaths[j], tPath);
                bool tPasta = kbEhPasta[i]; kbEhPasta[i] = kbEhPasta[j]; kbEhPasta[j] = tPasta;
            }
        }
    }
    kbSel = 0;
    kbScroll = 0;
}

static void kbSalvarHistorico(const char* filepath) {
    #ifdef _WIN32
    _mkdir("data");
    _mkdir("data/HyperNeiva");
    #else
    mkdir("data", 0777);
    mkdir("data/HyperNeiva", 0777);
    #endif

    FILE* f = fopen(filepath, "w");
    if (f) {
        fprintf(f, "HYPER_NEIVA_KB_TEST_V1\n");
        for (int i = 0; i < (int)(sizeof(kbKeys) / sizeof(kbKeys[0])); i++) {
            if (kbKeys[i].tested) {
                fprintf(f, "%d %s\n", kbKeys[i].key, kbKeys[i].label);
            }
        }
        fclose(f);
    }
}

static void kbAbrirHistorico(const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) return;

    char header[64];
    if (fgets(header, sizeof(header), f)) {
        if (strcmp(header, "HYPER_NEIVA_KB_TEST_V1\n") == 0) {
            // Reset current tested
            for (int i = 0; i < (int)(sizeof(kbKeys) / sizeof(kbKeys[0])); i++) {
                kbKeys[i].tested = false;
            }

            int keyVal;
            char keyLabel[64];
            while (fscanf(f, "%d %63s\n", &keyVal, keyLabel) == 2) {
                for (int i = 0; i < (int)(sizeof(kbKeys) / sizeof(kbKeys[0])); i++) {
                    if (kbKeys[i].key == keyVal) {
                        kbKeys[i].tested = true;
                        break;
                    }
                }
            }
        }
    }
    fclose(f);
}

void initTecladoTeste(void) {
    if (!kbRenderTextureLoaded) {
        kbRenderTexture = LoadRenderTexture(1920, 1080);
        kbRenderTextureLoaded = true;
    }
    kbState = KB_STATE_TEST;
    strcpy(kbCaminhoAtual, "data/HyperNeiva");
    for (int i = 0; i < (int)(sizeof(kbKeys) / sizeof(kbKeys[0])); i++) {
        kbKeys[i].flash = 0.0f;
        kbKeys[i].tested = false;
    }
}

void fecharTecladoTeste(void) {
    if (kbRenderTextureLoaded) {
        UnloadRenderTexture(kbRenderTexture);
        kbRenderTextureLoaded = false;
    }
}

void renderizarTecladoTeste(void) {
    if (!kbRenderTextureLoaded) return;

    Vector2 mousePosRaw = GetMousePosition();
    Vector2 mousePos = {
        mousePosRaw.x * (1920.0f / (float)GetScreenWidth()),
        mousePosRaw.y * (1080.0f / (float)GetScreenHeight())
    };
    bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    float wheelMove = GetMouseWheelMove();

    // ==========================================
    // UPDATE SECTION
    // ==========================================
    if (kbState == KB_STATE_TEST) {
        // Update keyboard keys states
        float dt = GetFrameTime();
        for (int i = 0; i < (int)(sizeof(kbKeys) / sizeof(kbKeys[0])); i++) {
            if (IsKeyPressed(kbKeys[i].key)) {
                kbKeys[i].flash = 1.0f;
                kbKeys[i].tested = true;
            }
            if (kbKeys[i].flash > 0.0f) {
                kbKeys[i].flash -= dt * 4.0f;
                if (kbKeys[i].flash < 0.0f) kbKeys[i].flash = 0.0f;
            }
        }
    }
    else if (kbState == KB_STATE_OPEN_EXPLORER) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            kbState = KB_STATE_TEST;
        }

        // Scroll
        if (wheelMove != 0) {
            kbScroll -= (int)(wheelMove * 2);
            if (kbScroll > kbTotalItens - 18) kbScroll = kbTotalItens - 18;
            if (kbScroll < 0) kbScroll = 0;
        }

        // Keyboard arrows
        if (IsKeyPressed(KEY_UP)) {
            kbSel--;
            if (kbSel < 0) kbSel = kbTotalItens > 0 ? kbTotalItens - 1 : 0;
            if (kbSel < kbScroll) kbScroll = kbSel;
            if (kbSel > kbScroll + 17) kbScroll = kbSel - 17;
        }
        if (IsKeyPressed(KEY_DOWN)) {
            kbSel++;
            if (kbSel >= kbTotalItens) kbSel = 0;
            if (kbSel > kbScroll + 17) kbScroll = kbSel - 17;
            if (kbSel < kbScroll) kbScroll = kbSel;
        }
        if (IsKeyPressed(KEY_ENTER)) {
            if (kbTotalItens > 0) {
                if (kbEhPasta[kbSel]) {
                    if (strcmp(kbPaths[kbSel], "..") == 0) {
                        char* lastSlash = strrchr(kbCaminhoAtual, '/');
                        if (!lastSlash) lastSlash = strrchr(kbCaminhoAtual, '\\');
                        if (lastSlash && lastSlash != kbCaminhoAtual) {
                            *lastSlash = '\0';
                            kbListarArquivos(kbCaminhoAtual);
                        }
                    } else {
                        strcpy(kbCaminhoAtual, kbPaths[kbSel]);
                        kbListarArquivos(kbCaminhoAtual);
                    }
                } else {
                    kbAbrirHistorico(kbPaths[kbSel]);
                    kbState = KB_STATE_TEST;
                }
            }
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            char* lastSlash = strrchr(kbCaminhoAtual, '/');
            if (!lastSlash) lastSlash = strrchr(kbCaminhoAtual, '\\');
            if (lastSlash && lastSlash != kbCaminhoAtual) {
                *lastSlash = '\0';
                kbListarArquivos(kbCaminhoAtual);
            }
        }

        // Mouse click navigation
        if (clicked) {
            int boxX = 100;
            int boxY = 160;
            int boxW = 1720;
            int listY = boxY + 40;
            int itemsPerPage = 18;
            for (int i = kbScroll; i < kbScroll + itemsPerPage && i < kbTotalItens; i++) {
                int itemY = listY + (i - kbScroll) * 40;
                bool isHovered = (mousePos.x >= boxX + 40 && mousePos.x <= boxX + boxW - 80 && mousePos.y >= itemY - 5 && mousePos.y <= itemY + 30);
                if (isHovered) {
                    kbSel = i;
                    if (kbEhPasta[i]) {
                        if (strcmp(kbPaths[i], "..") == 0) {
                            char* lastSlash = strrchr(kbCaminhoAtual, '/');
                            if (!lastSlash) lastSlash = strrchr(kbCaminhoAtual, '\\');
                            if (lastSlash && lastSlash != kbCaminhoAtual) {
                                *lastSlash = '\0';
                                kbListarArquivos(kbCaminhoAtual);
                            }
                        } else {
                            strcpy(kbCaminhoAtual, kbPaths[i]);
                            kbListarArquivos(kbCaminhoAtual);
                        }
                    } else {
                        kbAbrirHistorico(kbPaths[i]);
                        kbState = KB_STATE_TEST;
                    }
                    break;
                }
            }
        }
    }
    else if (kbState == KB_STATE_SAVE_DIALOG) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            kbState = KB_STATE_TEST;
        }

        // Scroll
        if (wheelMove != 0) {
            kbScroll -= (int)(wheelMove * 2);
            if (kbScroll > kbTotalItens - 18) kbScroll = kbTotalItens - 18;
            if (kbScroll < 0) kbScroll = 0;
        }

        // Keyboard filename typing
        int charKey = GetCharPressed();
        while (charKey > 0) {
            if (((charKey >= 'a' && charKey <= 'z') || (charKey >= 'A' && charKey <= 'Z') ||
                 (charKey >= '0' && charKey <= '9') || charKey == '_' || charKey == '-' || charKey == '.' || charKey == ' ') &&
                strlen(kbSaveFilename) < 60) {
                int sl = strlen(kbSaveFilename);
                kbSaveFilename[sl] = (char)charKey;
                kbSaveFilename[sl + 1] = '\0';
            }
            charKey = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE)) {
            int sl = strlen(kbSaveFilename);
            if (sl > 0) kbSaveFilename[sl - 1] = '\0';
        }

        // Mouse click navigation or file select
        if (clicked) {
            int boxX = 100;
            int boxY = 160;
            int boxW = 1150;
            int listY = boxY + 40;
            int itemsPerPage = 18;
            for (int i = kbScroll; i < kbScroll + itemsPerPage && i < kbTotalItens; i++) {
                int itemY = listY + (i - kbScroll) * 40;
                bool isHovered = (mousePos.x >= boxX + 40 && mousePos.x <= boxX + boxW - 80 && mousePos.y >= itemY - 5 && mousePos.y <= itemY + 30);
                if (isHovered) {
                    kbSel = i;
                    if (kbEhPasta[i]) {
                        if (strcmp(kbPaths[i], "..") == 0) {
                            char* lastSlash = strrchr(kbCaminhoAtual, '/');
                            if (!lastSlash) lastSlash = strrchr(kbCaminhoAtual, '\\');
                            if (lastSlash && lastSlash != kbCaminhoAtual) {
                                *lastSlash = '\0';
                                kbListarArquivos(kbCaminhoAtual);
                            }
                        } else {
                            strcpy(kbCaminhoAtual, kbPaths[i]);
                            kbListarArquivos(kbCaminhoAtual);
                        }
                    } else {
                        // Copy filename without .txt
                        char tempName[64];
                        strcpy(tempName, kbNomes[i]);
                        char* ext = strstr(tempName, ".txt");
                        if (ext) *ext = '\0';
                        strncpy(kbSaveFilename, tempName, sizeof(kbSaveFilename) - 1);
                        kbSaveFilename[sizeof(kbSaveFilename) - 1] = '\0';
                    }
                    break;
                }
            }
        }

        if (IsKeyPressed(KEY_ENTER)) {
            if (strlen(kbSaveFilename) > 0) {
                char fullPath[1024];
                if (!strstr(kbSaveFilename, ".txt")) {
                    sprintf(fullPath, "%s/%s.txt", kbCaminhoAtual, kbSaveFilename);
                } else {
                    sprintf(fullPath, "%s/%s", kbCaminhoAtual, kbSaveFilename);
                }
                kbSalvarHistorico(fullPath);
                kbState = KB_STATE_TEST;
            }
        }
    }

    // DRAWING EVERYTHING ONTO 1920x1080 CANVAS
    BeginTextureMode(kbRenderTexture);
    ClearBackground(BLACK);

    desenharRetanguloAlpha(0, 0, 1920, 1080, 0xFF151515);

    if (kbState == KB_STATE_TEST) {
        desenharTexto("TESTE DE TECLADO INTERATIVO", 45, 100, 80, 0xFF00AAFF);
        desenharTexto("Pressione as teclas para validar. Teclas pressionadas acendem em verde e piscam no clique.", 22, 100, 140, 0xFFAAAAAA);
        desenharTexto("Teclas ja testadas permanecem pintadas em verde escuro.", 22, 100, 175, 0xFFAAAAAA);
        desenharTexto("[ESC] para acender a tecla ESC  |  Clique em VOLTAR para sair", 20, 100, 220, 0xFFEAB308);

        // DRAW CONTROL BUTTONS (VOLTAR, RESET, ABRIR, SALVAR)
        int rstX = 1600;
        int rstW = 220;
        int rstH = 50;

        // Voltar Button
        int vltY = 80;
        bool hoverVoltar = (mousePos.x >= rstX && mousePos.x <= rstX + rstW && mousePos.y >= vltY && mousePos.y <= vltY + rstH);
        if (hoverVoltar && clicked) {
            fecharTecladoTeste();
            voltarParaMenuExtra();
        }
        desenharRetanguloAlpha(rstX, vltY, rstW, rstH, hoverVoltar ? 0xFF00AAFF : 0xFF333333);
        DrawRectangleLines(rstX, vltY, rstW, rstH, WHITE);
        int txtW = MeasureText("VOLTAR", 18);
        desenharTexto("VOLTAR", 18, rstX + (rstW - txtW)/2, vltY + 16, 0xFFFFFFFF);

        // Reset Button
        int rstY = 150;
        bool hoverReset = (mousePos.x >= rstX && mousePos.x <= rstX + rstW && mousePos.y >= rstY && mousePos.y <= rstY + rstH);
        if (hoverReset && clicked) {
            for (int i = 0; i < (int)(sizeof(kbKeys) / sizeof(kbKeys[0])); i++) {
                kbKeys[i].tested = false;
                kbKeys[i].flash = 0.0f;
            }
        }
        desenharRetanguloAlpha(rstX, rstY, rstW, rstH, hoverReset ? 0xFF00AAFF : 0xFF333333);
        DrawRectangleLines(rstX, rstY, rstW, rstH, WHITE);
        txtW = MeasureText("LIMPAR HISTORICO", 18);
        desenharTexto("LIMPAR HISTORICO", 18, rstX + (rstW - txtW)/2, rstY + 16, 0xFFFFFFFF);

        // Abrir Button
        int abrY = 220;
        bool hoverAbrir = (mousePos.x >= rstX && mousePos.x <= rstX + rstW && mousePos.y >= abrY && mousePos.y <= abrY + rstH);
        if (hoverAbrir && clicked) {
            kbState = KB_STATE_OPEN_EXPLORER;
            kbListarArquivos(kbCaminhoAtual);
        }
        desenharRetanguloAlpha(rstX, abrY, rstW, rstH, hoverAbrir ? 0xFF00AAFF : 0xFF333333);
        DrawRectangleLines(rstX, abrY, rstW, rstH, WHITE);
        txtW = MeasureText("ABRIR HISTORICO", 18);
        desenharTexto("ABRIR HISTORICO", 18, rstX + (rstW - txtW)/2, abrY + 16, 0xFFFFFFFF);

        // Salvar Button
        int salY = 290;
        bool hoverSalvar = (mousePos.x >= rstX && mousePos.x <= rstX + rstW && mousePos.y >= salY && mousePos.y <= salY + rstH);
        if (hoverSalvar && clicked) {
            kbState = KB_STATE_SAVE_DIALOG;
            kbListarArquivos(kbCaminhoAtual);
            kbSaveFilename[0] = '\0';
        }
        desenharRetanguloAlpha(rstX, salY, rstW, rstH, hoverSalvar ? 0xFF00AAFF : 0xFF333333);
        DrawRectangleLines(rstX, salY, rstW, rstH, WHITE);
        txtW = MeasureText("SALVAR HISTORICO", 18);
        desenharTexto("SALVAR HISTORICO", 18, rstX + (rstW - txtW)/2, salY + 16, 0xFFFFFFFF);

        // DRAW THE MECHANICAL KEYBOARD LAYOUT
        int kbX = 260;
        int kbY = 380;

        // Draw keyboard casing frame
        desenharRetanguloAlpha(kbX - 20, kbY - 20, 1440, 480, 0xFF2D3139);
        DrawRectangleLines(kbX - 20, kbY - 20, 1440, 480, (Color){ 80, 80, 80, 255 });

        for (int i = 0; i < (int)(sizeof(kbKeys) / sizeof(kbKeys[0])); i++) {
            uint32_t baseColor = 0xFF22252D; // Standard default grey
            
            if (IsKeyDown(kbKeys[i].key)) {
                baseColor = 0xFF17CF17; // Bright active green
            } else if (kbKeys[i].tested) {
                baseColor = 0xFF166534; // Permanent darker tested green!
            }

            // Blend with white flash at moment of press
            float f = kbKeys[i].flash;
            uint8_t br = (baseColor >> 16) & 0xFF;
            uint8_t bg = (baseColor >> 8) & 0xFF;
            uint8_t bb = baseColor & 0xFF;

            uint8_t r = (uint8_t)(br * (1.0f - f) + 255 * f);
            uint8_t g = (uint8_t)(bg * (1.0f - f) + 255 * f);
            uint8_t b = (uint8_t)(bb * (1.0f - f) + 255 * f);

            uint32_t finalColor = (0xFF << 24) | (r << 16) | (g << 8) | b;

            // Draw key body
            desenharRetanguloAlpha(kbX + kbKeys[i].x, kbY + kbKeys[i].y, kbKeys[i].w, kbKeys[i].h, finalColor);
            DrawRectangleLines(kbX + kbKeys[i].x, kbY + kbKeys[i].y, kbKeys[i].w, kbKeys[i].h, (Color){ 80, 80, 80, 255 });

            // Draw key label centered
            int labelSize = (strlen(kbKeys[i].label) > 3) ? 14 : 22; // shrink font for longer labels like BACKSPACE
            int lblW = MeasureText(kbKeys[i].label, labelSize);
            int lblX = kbX + kbKeys[i].x + (kbKeys[i].w - lblW) / 2;
            int lblY = kbY + kbKeys[i].y + (kbKeys[i].h - labelSize) / 2;
            desenharTexto(kbKeys[i].label, labelSize, lblX, lblY, 0xFFFFFFFF);
        }
    }
    else if (kbState == KB_STATE_OPEN_EXPLORER) {
        desenharTexto("ABRIR HISTORICO - SELECIONE UM ARQUIVO", 38, 100, 60, 0xFF00AAFF);
        char pathLabel[1024];
        sprintf(pathLabel, "Pasta: %s  |  [ENTER] Carregar  |  [ESC] Cancelar  |  Clique no arquivo para carregar", kbCaminhoAtual);
        desenharTexto(pathLabel, 20, 100, 110, 0xFFAAAAAA);

        // Cancel button
        int btnY = 60;
        int btnW = 180;
        int btnH = 50;
        int vltX = 1500;
        bool hoverVlt = (mousePos.x >= vltX && mousePos.x <= vltX + btnW && mousePos.y >= btnY && mousePos.y <= btnY + btnH);
        if (hoverVlt && clicked) {
            kbState = KB_STATE_TEST;
        }
        desenharRetanguloAlpha(vltX, btnY, btnW, btnH, hoverVlt ? 0xFF00AAFF : 0xFF333333);
        DrawRectangleLines(vltX, btnY, btnW, btnH, WHITE);
        int txtW = MeasureText("CANCELAR", 18);
        desenharTexto("CANCELAR", 18, vltX + (btnW - txtW)/2, btnY + 16, 0xFFFFFFFF);

        int boxX = 100;
        int boxY = 160;
        int boxW = 1720;
        int boxH = 820;

        desenharRetanguloAlpha(boxX, boxY, boxW, boxH, 0xFF1E2024);
        DrawRectangleLines(boxX, boxY, boxW, boxH, (Color){ 80, 80, 80, 255 });

        // Render list of files
        int listY = boxY + 40;
        int itemsPerPage = 18;
        for (int i = kbScroll; i < kbScroll + itemsPerPage && i < kbTotalItens; i++) {
            int itemY = listY + (i - kbScroll) * 40;
            bool isSelected = (kbSel == i);
            bool isHovered = (mousePos.x >= boxX + 40 && mousePos.x <= boxX + boxW - 80 && mousePos.y >= itemY - 5 && mousePos.y <= itemY + 30);

            uint32_t textCor = isSelected ? 0xFF00AAFF : (isHovered ? 0xFFFFFFFF : 0xFFBBBBBB);
            if (kbEhPasta[i]) {
                desenharTexto(kbNomes[i], 22, boxX + 50, itemY, textCor);
            } else {
                desenharTexto(kbNomes[i], 22, boxX + 70, itemY, textCor);
                DrawRectangle(boxX + 50, itemY + 6, 8, 8, (Color){ 120, 120, 120, 255 });
            }
        }

        if (kbTotalItens == 0) {
            desenharTexto("Nenhum arquivo de historico (.txt) encontrado.", 24, boxX + 60, boxY + 60, 0xFF888888);
        }
    }
    else if (kbState == KB_STATE_SAVE_DIALOG) {
        desenharTexto("SALVAR HISTORICO - SELECIONE A PASTA", 38, 100, 60, 0xFF00AAFF);
        char pathLabel[1024];
        sprintf(pathLabel, "Pasta: %s  |  [ENTER] Confirmar  |  [ESC] Cancelar  |  Navegue pelas pastas na lista", kbCaminhoAtual);
        desenharTexto(pathLabel, 20, 100, 110, 0xFFAAAAAA);

        // Left Panel Casing
        int boxX = 100;
        int boxY = 160;
        int boxW = 1150;
        int boxH = 820;

        desenharRetanguloAlpha(boxX, boxY, boxW, boxH, 0xFF1E2024);
        DrawRectangleLines(boxX, boxY, boxW, boxH, (Color){ 80, 80, 80, 255 });

        // Render list of files/folders
        int listY = boxY + 40;
        int itemsPerPage = 18;
        for (int i = kbScroll; i < kbScroll + itemsPerPage && i < kbTotalItens; i++) {
            int itemY = listY + (i - kbScroll) * 40;
            bool isSelected = (kbSel == i);
            bool isHovered = (mousePos.x >= boxX + 40 && mousePos.x <= boxX + boxW - 80 && mousePos.y >= itemY - 5 && mousePos.y <= itemY + 30);

            uint32_t textCor = isSelected ? 0xFF00AAFF : (isHovered ? 0xFFFFFFFF : 0xFFBBBBBB);
            if (kbEhPasta[i]) {
                desenharTexto(kbNomes[i], 22, boxX + 50, itemY, textCor);
            } else {
                desenharTexto(kbNomes[i], 22, boxX + 70, itemY, textCor);
                DrawRectangle(boxX + 50, itemY + 6, 8, 8, (Color){ 120, 120, 120, 255 });
            }
        }

        if (kbTotalItens == 0) {
            desenharTexto("Nenhuma pasta encontrada.", 24, boxX + 60, boxY + 60, 0xFF888888);
        }

        // Right Panel Save card
        int cardX = 1300;
        int cardY = 160;
        int cardW = 520;
        int cardH = 820;

        desenharRetanguloAlpha(cardX, cardY, cardW, cardH, 0xFF2D3139);
        DrawRectangleLines(cardX, cardY, cardW, cardH, (Color){ 100, 100, 100, 255 });

        desenharTexto("SALVAR HISTORICO", 28, cardX + 30, cardY + 40, 0xFF00AAFF);
        desenharTexto("Nome do arquivo abaixo:", 18, cardX + 30, cardY + 140, 0xFFFFFFFF);

        int inpX = cardX + 30;
        int inpY = cardY + 180;
        int inpW = cardW - 60;
        int inpH = 60;
        desenharRetanguloAlpha(inpX, inpY, inpW, inpH, 0xFF151515);
        DrawRectangleLines(inpX, inpY, inpW, inpH, (Color){ 80, 80, 80, 255 });

        char dispName[128];
        if (strlen(kbSaveFilename) == 0) {
            strcpy(dispName, "historico_teclado");
            desenharTexto(dispName, 20, inpX + 15, inpY + 20, 0xFF555555);
        } else {
            sprintf(dispName, "%s.txt", kbSaveFilename);
            desenharTexto(dispName, 20, inpX + 15, inpY + 20, 0xFF00FF00);
        }

        // Blinking cursor
        static float saveCursorBlink = 0.0f;
        saveCursorBlink += GetFrameTime();
        if (fmod(saveCursorBlink, 1.0f) < 0.5f) {
            int cursorOffset = strlen(kbSaveFilename) == 0 ? MeasureText("historico_teclado", 20) : MeasureText(dispName, 20);
            desenharRetanguloAlpha(inpX + 15 + cursorOffset + 5, inpY + 15, 4, 30, 0xFF00AAFF);
        }

        // Buttons
        int btnW = 200;
        int btnH = 60;
        int cnfX = cardX + 40;
        int cncX = cardX + cardW - btnW - 40;
        int btnY2 = cardY + 300;

        // Confirm
        bool hoverConfirm = (mousePos.x >= cnfX && mousePos.x <= cnfX + btnW && mousePos.y >= btnY2 && mousePos.y <= btnY2 + btnH);
        bool canConfirm = (strlen(kbSaveFilename) > 0);
        if (hoverConfirm && clicked && canConfirm) {
            char fullPath[1024];
            if (!strstr(kbSaveFilename, ".txt")) {
                sprintf(fullPath, "%s/%s.txt", kbCaminhoAtual, kbSaveFilename);
            } else {
                sprintf(fullPath, "%s/%s", kbCaminhoAtual, kbSaveFilename);
            }
            kbSalvarHistorico(fullPath);
            kbState = KB_STATE_TEST;
        }
        desenharRetanguloAlpha(cnfX, btnY2, btnW, btnH, canConfirm ? (hoverConfirm ? 0xFF00AAFF : 0xFF333333) : 0xFF222222);
        DrawRectangleLines(cnfX, btnY2, btnW, btnH, canConfirm ? WHITE : (Color){ 60, 60, 60, 255 });
        int txtW = MeasureText("CONFIRMAR", 18);
        desenharTexto("CONFIRMAR", 18, cnfX + (btnW - txtW)/2, btnY2 + 20, canConfirm ? 0xFFFFFFFF : 0xFF555555);

        // Cancel
        bool hoverCancel = (mousePos.x >= cncX && mousePos.x <= cncX + btnW && mousePos.y >= btnY2 && mousePos.y <= btnY2 + btnH);
        if (hoverCancel && clicked) {
            kbState = KB_STATE_TEST;
        }
        desenharRetanguloAlpha(cncX, btnY2, btnW, btnH, hoverCancel ? 0xFF00AAFF : 0xFF333333);
        DrawRectangleLines(cncX, btnY2, btnW, btnH, WHITE);
        txtW = MeasureText("CANCELAR", 18);
        desenharTexto("CANCELAR", 18, cncX + (btnW - txtW)/2, btnY2 + 20, 0xFFFFFFFF);
    }

    EndTextureMode();

    // Render scaled to real screen
    int sWidth = GetScreenWidth();
    int sHeight = GetScreenHeight();
    DrawTexturePro(kbRenderTexture.texture, 
                  (Rectangle){ 0, 0, (float)kbRenderTexture.texture.width, (float)-kbRenderTexture.texture.height }, 
                  (Rectangle){ 0, 0, (float)sWidth, (float)sHeight }, 
                  (Vector2){ 0, 0 }, 0.0f, WHITE);
}
