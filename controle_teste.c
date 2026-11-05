#include "controle_teste.h"
#include "raylib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

// RenderTexture for drawing at 1920x1080 and scaling to screen
static RenderTexture2D testRenderTexture;
static bool testRenderTextureLoaded = false;

extern void voltarParaMenuExtra(void);

// Stick path grids
static bool trilhaL[256][256];
static bool trilhaR[256][256];
static int prevLx = 128, prevLy = 128;
static int prevRx = 128, prevRy = 128;

// Touchpad painting grid
static bool trilhaTouch[400][250];
static int prevTx = -1, prevTy = -1;

// Keyboard state history
static int lastKeysPressed[10];
static int totalKeysHistory = 0;

static void drawLineOnGridL(bool grid[256][256], int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        if (x0 >= 0 && x0 < 256 && y0 >= 0 && y0 < 256) grid[x0][y0] = true;
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void drawLineOnGridTouch(bool grid[400][250], int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        if (x0 >= 0 && x0 < 400 && y0 >= 0 && y0 < 250) grid[x0][y0] = true;
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

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

static void desenharTexto(const char* text, int size, int x, int y, uint32_t cor) {
    uint8_t a = (cor >> 24) & 0xFF;
    uint8_t r = (cor >> 16) & 0xFF;
    uint8_t g = (cor >> 8) & 0xFF;
    uint8_t b = cor & 0xFF;
    DrawText(text, x, y, size, (Color){ r, g, b, a });
}

void initControleTeste(void) {
    if (!testRenderTextureLoaded) {
        testRenderTexture = LoadRenderTexture(1920, 1080);
        testRenderTextureLoaded = true;
    }
    memset(trilhaL, 0, sizeof(trilhaL));
    memset(trilhaR, 0, sizeof(trilhaR));
    memset(trilhaTouch, 0, sizeof(trilhaTouch));
    prevLx = 128; prevLy = 128;
    prevRx = 128; prevRy = 128;
    prevTx = -1; prevTy = -1;
    totalKeysHistory = 0;
}

void fecharControleTeste(void) {
    if (testRenderTextureLoaded) {
        UnloadRenderTexture(testRenderTexture);
        testRenderTextureLoaded = false;
    }
}

void renderizarControleTesteRaylib(void) {
    if (!testRenderTextureLoaded) return;

    Vector2 mousePosRaw = GetMousePosition();
    Vector2 mousePos = {
        mousePosRaw.x * (1920.0f / (float)GetScreenWidth()),
        mousePosRaw.y * (1080.0f / (float)GetScreenHeight())
    };
    bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool holding = IsMouseButtonDown(MOUSE_LEFT_BUTTON);

    int vltX = 1600, vltY = 80, vltW = 220, vltH = 50;
    bool hoverVoltar = (mousePos.x >= vltX && mousePos.x <= vltX + vltW && mousePos.y >= vltY && mousePos.y <= vltY + vltH);
    if (hoverVoltar && clicked) {
        fecharControleTeste();
        voltarParaMenuExtra();
    }

    // Track last pressed keyboard keys
    float flx = 0.0f;
    float fly = 0.0f;
    float frx = 0.0f;
    float fry = 0.0f;
    float fl2 = -1.0f;
    float fr2 = -1.0f;

    // Read gamepad values if connected
    bool gp = IsGamepadAvailable(0);
    int lx = 128, ly = 128, rx = 128, ry = 128;
    int l2 = 0, r2 = 0;
    bool btnDpadUp = false, btnDpadDown = false, btnDpadLeft = false, btnDpadRight = false;
    bool btnL1 = false, btnR1 = false, btnL3 = false, btnR3 = false;
    bool btnStart = false, btnSelect = false;
    bool btnFaceUp = false, btnFaceDown = false, btnFaceLeft = false, btnFaceRight = false;

    if (gp) {
        flx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        fly = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        frx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X);
        fry = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y);
        fl2 = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_TRIGGER);
        fr2 = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_TRIGGER);

        // Convert axes [-1.0, 1.0] to [0, 255]
        lx = (int)((flx + 1.0f) * 127.5f);
        ly = (int)((fly + 1.0f) * 127.5f);
        rx = (int)((frx + 1.0f) * 127.5f);
        ry = (int)((fry + 1.0f) * 127.5f);
        
        // Triggers map differently depending on driver (usually [-1, 1] or [0, 1])
        l2 = (int)((fl2 + 1.0f) * 127.5f);
        r2 = (int)((fr2 + 1.0f) * 127.5f);

        // Buttons
        btnDpadUp = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_UP);
        btnDpadDown = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
        btnDpadLeft = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
        btnDpadRight = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);

        btnL1 = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
        btnR1 = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
        btnL3 = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_THUMB);
        btnR3 = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_THUMB);

        btnStart = IsGamepadButtonDown(0, GAMEPAD_BUTTON_MIDDLE_RIGHT);
        btnSelect = IsGamepadButtonDown(0, GAMEPAD_BUTTON_MIDDLE_LEFT);

        btnFaceUp = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_UP);
        btnFaceDown = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        btnFaceLeft = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
        btnFaceRight = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    }

    // L3 drift trail mapping
    if (lx >= 115 && lx <= 140 && ly >= 115 && ly <= 140) {
        memset(trilhaL, 0, sizeof(trilhaL));
        prevLx = 128; prevLy = 128;
    } else {
        drawLineOnGridL(trilhaL, prevLx, prevLy, lx, ly);
        prevLx = lx; prevLy = ly;
    }

    // R3 drift trail mapping
    if (rx >= 115 && rx <= 140 && ry >= 115 && ry <= 140) {
        memset(trilhaR, 0, sizeof(trilhaR));
        prevRx = 128; prevRy = 128;
    } else {
        drawLineOnGridL(trilhaR, prevRx, prevRy, rx, ry);
        prevRx = rx; prevRy = ry;
    }

    // Touchpad mouse canvas painting
    int tpW = 400; int tpH = 250;
    int tpX = 760; int tpY = 480;
    if (holding && mousePos.x >= tpX && mousePos.x <= tpX + tpW && mousePos.y >= tpY && mousePos.y <= tpY + tpH) {
        int tx = (int)(mousePos.x - tpX);
        int ty = (int)(mousePos.y - tpY);
        if (prevTx == -1) { prevTx = tx; prevTy = ty; }
        drawLineOnGridTouch(trilhaTouch, prevTx, prevTy, tx, ty);
        prevTx = tx; prevTy = ty;
    } else {
        prevTx = -1; prevTy = -1;
    }

    // Clear touchpad if double clicked in touchpad region or pressed Select/C
    if (IsKeyPressed(KEY_C) || btnSelect) {
        memset(trilhaTouch, 0, sizeof(trilhaTouch));
    }

    // DRAWING EVERYTHING ONTO THE 1920x1080 TEXTURE
    BeginTextureMode(testRenderTexture);
    ClearBackground(BLACK);

    desenharRetanguloAlpha(0, 0, 1920, 1080, 0xFF151515);

    desenharTexto("GERENCIADOR DE DISPOSITIVOS & CONTROLES", 40, 50, 40, 0xFF00AAFF);
    desenharTexto("Pressione [ESC] para Sair do Teste de Controle", 25, 50, 80, 0xFFAAAAAA);

    // Draw Voltar Button
    desenharRetanguloAlpha(vltX, vltY, vltW, vltH, hoverVoltar ? 0xFF00AAFF : 0xFF333333);
    DrawRectangleLines(vltX, vltY, vltW, vltH, WHITE);
    int txtW = MeasureText("VOLTAR", 18);
    desenharTexto("VOLTAR", 18, vltX + (vltW - txtW)/2, vltY + 16, 0xFFFFFFFF);

    // ==========================================
    // DISPOSITIVO DETECTADO
    // ==========================================
    if (gp) {
        char gpName[256];
        sprintf(gpName, "CONTROLE DETECTADO: %s", GetGamepadName(0));
        desenharTexto(gpName, 26, 50, 130, 0xFF00FF00);
    } else {
        desenharTexto("DISPOSITIVO: [TECLADO + MOUSE] - Nenhum comando conectado", 26, 50, 130, 0xFFEAB308);
        desenharTexto("(Conecte um comando USB/Bluetooth para testar analógicos, drift e gatilhos)", 18, 50, 160, 0xFF94A3B8);
    }

    // ==========================================
    // COLUNA 1: D-PAD & SISTEMA
    // ==========================================
    int dX = 100, dY = 240;
    desenharTexto("D-PAD / DIRECIONAIS", 30, dX, dY - 40, 0xFF00AAFF);
    desenharTexto("[ SETA CIMA ]", 35, dX, dY, (btnDpadUp || IsKeyDown(KEY_UP)) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto("[ SETA BAIXO ]", 35, dX, dY + 50, (btnDpadDown || IsKeyDown(KEY_DOWN)) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto("[ SETA ESQ ]", 35, dX, dY + 100, (btnDpadLeft || IsKeyDown(KEY_LEFT)) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto("[ SETA DIR ]", 35, dX, dY + 150, (btnDpadRight || IsKeyDown(KEY_RIGHT)) ? 0xFFFFFFFF : 0xFF555555);

    int eX = 520, eY = 240;
    desenharTexto("OMBROS & OUTROS", 30, eX, eY - 40, 0xFF00AAFF);
    desenharTexto("[ L1 / LB ]", 35, eX, eY, (btnL1 || IsKeyDown(KEY_E)) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto("[ R1 / RB ]", 35, eX, eY + 50, (btnR1 || IsKeyDown(KEY_R)) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto("[ L3 CLICK ]", 35, eX, eY + 100, btnL3 ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto("[ R3 CLICK ]", 35, eX, eY + 150, btnR3 ? 0xFFFFFFFF : 0xFF555555);

    int sX = 940, sY = 240;
    desenharTexto("BOTOES DE MENU", 30, sX, sY - 40, 0xFF00AAFF);
    desenharTexto("[ START / OPTIONS ]", 35, sX, sY, (btnStart || IsKeyDown(KEY_ENTER)) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto("[ SELECT / SHARE ]", 35, sX, sY + 50, (btnSelect || IsKeyDown(KEY_RIGHT_SHIFT)) ? 0xFFFFFFFF : 0xFF555555);

    // ==========================================
    // COLUNA 4: AÇÃO (TRI, BOLA, X, QUADRADO)
    // ==========================================
    int aX = 1450, aY = 240;
    desenharTexto("BOTOES DE ACAO", 30, aX, aY - 40, 0xFF00AAFF);
    desenharTexto("[ TRIANGULO / Y ]", 35, aX, aY, (btnFaceUp || IsKeyDown(KEY_W)) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto("[ BOLINHA / B ]", 35, aX, aY + 50, (btnFaceRight || IsKeyDown(KEY_D)) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto("[ CRUZ / X / A ]", 35, aX, aY + 100, (btnFaceDown || IsKeyDown(KEY_S)) ? 0xFFFFFFFF : 0xFF555555);
    desenharTexto("[ QUADRADO / X ]", 35, aX, aY + 150, (btnFaceLeft || IsKeyDown(KEY_A)) ? 0xFFFFFFFF : 0xFF555555);

    // ==========================================
    // PARTE 2: ANALÓGICOS (L3 / R3 DRIFT ANALYZER)
    // ==========================================
    int lCenterX = 230; int lCenterY = 660; int radius = 150;
    desenharTexto("ANALOGICO L3 (DRIFT)", 30, lCenterX - 150, lCenterY - 180, 0xFF00AAFF);
    desenharTexto("Centro Ideal: 128 | 128", 22, lCenterX - 110, lCenterY - 150, 0xFF888888);
    drawCircle(lCenterX, lCenterY, radius, (Color){ 70, 70, 70, 255 }, false);
    drawCircle(lCenterX, lCenterY, 4, WHITE, true);

    // Draw Left drift trail
    for (int tx = 0; tx < 256; tx++) {
        for (int ty = 0; ty < 256; ty++) {
            if (trilhaL[tx][ty]) {
                int px = lCenterX + ((tx - 128) * radius) / 128;
                int py = lCenterY + ((ty - 128) * radius) / 128;
                DrawRectangle(px - 2, py - 2, 4, 4, (Color){ 0, 170, 255, 120 });
            }
        }
    }
    // Draw current stick position dot
    int lxOff = ((lx - 128) * radius) / 128; 
    int lyOff = ((ly - 128) * radius) / 128;
    drawCircle(lCenterX + lxOff, lCenterY + lyOff, 15, GREEN, true);

    char txt[128];
    sprintf(txt, "L3 X: %d | Y: %d", lx, ly);
    desenharTexto(txt, 25, lCenterX - 90, lCenterY + 175, 0xFFFFFFFF);

    // Right stick
    int rCenterX = 560; int rCenterY = 660;
    desenharTexto("ANALOGICO R3 (DRIFT)", 30, rCenterX - 150, rCenterY - 180, 0xFF00AAFF);
    desenharTexto("Centro Ideal: 128 | 128", 22, rCenterX - 110, rCenterY - 150, 0xFF888888);
    drawCircle(rCenterX, rCenterY, radius, (Color){ 70, 70, 70, 255 }, false);
    drawCircle(rCenterX, rCenterY, 4, WHITE, true);

    // Draw Right drift trail
    for (int tx = 0; tx < 256; tx++) {
        for (int ty = 0; ty < 256; ty++) {
            if (trilhaR[tx][ty]) {
                int px = rCenterX + ((tx - 128) * radius) / 128;
                int py = rCenterY + ((ty - 128) * radius) / 128;
                DrawRectangle(px - 2, py - 2, 4, 4, (Color){ 0, 170, 255, 120 });
            }
        }
    }
    // Draw current stick position dot
    int rxOff = ((rx - 128) * radius) / 128; 
    int ryOff = ((ry - 128) * radius) / 128;
    drawCircle(rCenterX + rxOff, rCenterY + ryOff, 15, GREEN, true);

    sprintf(txt, "R3 X: %d | Y: %d", rx, ry);
    desenharTexto(txt, 25, rCenterX - 90, rCenterY + 175, 0xFFFFFFFF);

    // ==========================================
    // PARTE 3: TOUCHPAD / MOUSE PAINTING
    // ==========================================
    desenharTexto("PAINEL DE DESENHO (TOUCHPAD)", 30, tpX + 10, tpY - 40, 0xFF00AAFF);
    desenharTexto("(Arraste com o mouse | Aperte [C] para limpar)", 20, tpX + 10, tpY - 15, 0xFF888888);
    
    // Draw board
    DrawRectangle(tpX, tpY, tpW, tpH, (Color){ 30, 30, 30, 255 });
    DrawRectangleLines(tpX, tpY, tpW, tpH, (Color){ 100, 100, 100, 255 });

    // Draw painted trails
    for (int ty = 0; ty < tpH; ty++) {
        for (int tx = 0; tx < tpW; tx++) {
            if (trilhaTouch[tx][ty]) {
                DrawRectangle(tpX + tx - 2, tpY + ty - 2, 4, 4, (Color){ 0, 170, 255, 255 });
            }
        }
    }
    if (holding && mousePos.x >= tpX && mousePos.x <= tpX + tpW && mousePos.y >= tpY && mousePos.y <= tpY + tpH) {
        DrawCircle((int)mousePos.x, (int)mousePos.y, 8, GREEN);
    }

    // ==========================================
    // PARTE 4: MOUSE & TECLADO DETAILED STATE
    // ==========================================
    // DETAILED CONTROLLER AXES INFO
    // ==========================================
    int infoX = 1260, infoY = 480;
    desenharTexto("EIXOS DO CONTROLE (DETALHADO)", 30, infoX, infoY - 40, 0xFF00AAFF);

    char valStr[128];
    // L3 Axes
    desenharTexto("L3 ANALOGICO:", 24, infoX, infoY, 0xFFFFFFFF);
    sprintf(valStr, "Eixo X (Horizontal):  %+.5f", flx);
    desenharTexto(valStr, 22, infoX + 20, infoY + 35, 0xFFEEEEEE);
    sprintf(valStr, "Eixo Y (Vertical):    %+.5f", fly);
    desenharTexto(valStr, 22, infoX + 20, infoY + 70, 0xFFEEEEEE);

    // R3 Axes
    desenharTexto("R3 ANALOGICO:", 24, infoX, infoY + 120, 0xFFFFFFFF);
    sprintf(valStr, "Eixo X (Horizontal):  %+.5f", frx);
    desenharTexto(valStr, 22, infoX + 20, infoY + 155, 0xFFEEEEEE);
    sprintf(valStr, "Eixo Y (Vertical):    %+.5f", fry);
    desenharTexto(valStr, 22, infoX + 20, infoY + 190, 0xFFEEEEEE);

    // Trigger Axes raw values
    desenharTexto("GATILHOS RAWS:", 24, infoX, infoY + 240, 0xFFFFFFFF);
    sprintf(valStr, "Gatilho L2 (Eixo):   %+.5f", fl2);
    desenharTexto(valStr, 22, infoX + 20, infoY + 275, 0xFFEEEEEE);
    sprintf(valStr, "Gatilho R2 (Eixo):   %+.5f", fr2);
    desenharTexto(valStr, 22, infoX + 20, infoY + 310, 0xFFEEEEEE);

    // ==========================================
    // PARTE 5: GATILHOS ANALÓGICOS (L2 / R2)
    // ==========================================
    int l2X = 150, l2Y = 900;
    desenharTexto("GATILHO L2", 25, l2X, l2Y - 30, 0xFFFFFFFF);
    // Draw background track
    desenharRetanguloAlpha(l2X + 150, l2Y - 25, 400, 20, 0xFF333333);
    // Fill active trigger value
    desenharRetanguloAlpha(l2X + 150, l2Y - 25, (l2 * 400) / 255, 20, 0xFF00AAFF);
    char bufVal[32];
    sprintf(bufVal, "%d / 255 (%d%%)", l2, (l2 * 100) / 255);
    desenharTexto(bufVal, 22, l2X + 570, l2Y - 25, 0xFFFFFFFF);

    int r2X = 150, r2Y = 955;
    desenharTexto("GATILHO R2", 25, r2X, r2Y - 30, 0xFFFFFFFF);
    desenharRetanguloAlpha(r2X + 150, r2Y - 25, 400, 20, 0xFF333333);
    desenharRetanguloAlpha(r2X + 150, r2Y - 25, (r2 * 400) / 255, 20, 0xFF00AAFF);
    sprintf(bufVal, "%d / 255 (%d%%)", r2, (r2 * 100) / 255);
    desenharTexto(bufVal, 22, r2X + 570, r2Y - 25, 0xFFFFFFFF);

    EndTextureMode();

    // Render texture to screen scaled
    int sWidth = GetScreenWidth();
    int sHeight = GetScreenHeight();
    DrawTexturePro(testRenderTexture.texture, 
                  (Rectangle){ 0, 0, (float)testRenderTexture.texture.width, (float)-testRenderTexture.texture.height }, 
                  (Rectangle){ 0, 0, (float)sWidth, (float)sHeight }, 
                  (Vector2){ 0, 0 }, 0.0f, WHITE);
}
