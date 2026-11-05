#include "baixar_google_drive_win.h"
#include "segredos.h"
#include "api_crypt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <sys/wait.h>
#endif

GdItem gdItems[GD_TOTAL];
int gdTotal = 0;
char gdCurrentPath[GD_PATH_LEN] = "";
char gdStatus[512] = "";
bool gdTokenValido = false;
bool gdConectando = false;
int gdScroll = 0;
bool gdMarcados[GD_TOTAL] = {0};
int gdRcItem = -1;
int gdRcX = 0, gdRcY = 0;
char gdAccessToken[GD_TOKEN_LEN] = "";
char gdRefreshToken[GD_TOKEN_LEN] = "";

extern char* baixar_dir_base(void);
extern void exec_silent(const char* cmd);
extern void exec_async_silent(const char* cmd);

char gdClientId[256] = "";
char gdClientSecret[256] = "";

static void gd_salvar_nas_chaves(void) {
    int idx = -1;
    for (int i = 0; i < apiKeyTotal; i++) {
        if (strcmp(apiKeys[i].nome, "Google Drive") == 0) { idx = i; break; }
    }
    if (idx == -1 && apiKeyTotal < API_KEY_TOTAL) {
        idx = apiKeyTotal++;
        strcpy(apiKeys[idx].nome, "Google Drive");
        apiKeys[idx].desc[0] = 0; apiKeys[idx].chave[0] = 0; apiKeys[idx].app_id[0] = 0;
    }
    if (idx >= 0) {
        strncpy(apiKeys[idx].app_id, gdClientId, 255);
        strncpy(apiKeys[idx].desc, gdClientSecret, 255);
        strncpy(apiKeys[idx].chave, gdRefreshToken, 511);
    }
    extern void apiKeys_salvar(void);
    apiKeys_salvar();
}

static char* gd_json_val(char* buf, const char* key) {
    char* p = strstr(buf, key); if (!p) return NULL;
    p = strchr(p, ':'); if (!p) return NULL;
    p++; while (*p == ' ' || *p == '\t') p++;
    if (*p != '\"') return NULL;
    return p + 1;
}

void gd_ler_creds_das_chaves(void) {
    if (apiKeyTotal == 0) apiKeys_carregar();
    gdClientId[0] = 0; gdClientSecret[0] = 0; gdRefreshToken[0] = 0;
    for (int i = 0; i < apiKeyTotal; i++) {
        if (strcmp(apiKeys[i].nome, "Google Drive") == 0) {
            if (apiKeys[i].app_id[0]) strncpy(gdClientId, apiKeys[i].app_id, 255);
            if (apiKeys[i].desc[0]) strncpy(gdClientSecret, apiKeys[i].desc, 255);
            if (apiKeys[i].chave[0]) strncpy(gdRefreshToken, apiKeys[i].chave, GD_TOKEN_LEN-1);
            break;
        }
    }
    strcpy(gdClientId, GOOGLE_CLIENT_ID);
    strcpy(gdClientSecret, GOOGLE_CLIENT_SECRET);
    strcpy(gdRefreshToken, "1//04gLmnrufs3YRCgYIARAAGAQSNwF-L9IroyiX9hKxFljIkHKAtPBqOue-fLQSONw6NkKMiMMDzmTnMJAyaPKnii4CNb_keFH6O60");
    gd_salvar_nas_chaves();
}

void gd_exchange_code(const char* code) {
    gd_ler_creds_das_chaves();
    if (gdClientId[0] == 0) strcpy(gdClientId, "SEU_CLIENT_ID");
    if (gdClientSecret[0] == 0) strcpy(gdClientSecret, "SEU_CLIENT_SECRET");
    char tempBat[1024]; snprintf(tempBat, sizeof(tempBat), "%s/gd_exchange.bat", getenv("TEMP"));
    char outFile[1024]; snprintf(outFile, sizeof(outFile), "%s/gd_exchange_out.txt", getenv("TEMP"));
    FILE* f = fopen(tempBat, "w");
    if (!f) { strcpy(gdStatus, "Erro ao criar .bat"); return; }
    fprintf(f, "@echo off\n");
    fprintf(f, "\"C:\\Windows\\System32\\curl.exe\" -s -X POST \"https://oauth2.googleapis.com/token\" --data-urlencode \"code=%s\" --data-urlencode \"client_id=%s\" --data-urlencode \"client_secret=%s\" --data-urlencode \"redirect_uri=urn:ietf:wg:oauth:2.0:oob\" --data-urlencode \"grant_type=authorization_code\" > \"%s\"\n", code, gdClientId, gdClientSecret, outFile);
    fclose(f);
    char cmd[1100]; snprintf(cmd, sizeof(cmd), "\"%s\"", tempBat);
    exec_silent(cmd);
    FILE* out = fopen(outFile, "r");
    if (!out) { strcpy(gdStatus, "Erro ao ler resposta"); return; }
    long sz; fseek(out, 0, SEEK_END); sz = ftell(out); fseek(out, 0, SEEK_SET);
    char* buf = (char*)malloc(sz + 1); if (!buf) { fclose(out); return; }
    fread(buf, 1, sz, out); buf[sz] = 0; fclose(out);
    char* at = strstr(buf, "\"access_token\"");
    if (at) { at = strchr(at, ':') + 1; while (*at == ' ' || *at == '\t') at++; if (*at == '\"') { at++; char* end = strchr(at, '\"'); if (end) { *end = 0; strncpy(gdAccessToken, at, GD_TOKEN_LEN-1); } } }
    char* rt = strstr(buf, "\"refresh_token\"");
    if (rt) { rt = strchr(rt, ':') + 1; while (*rt == ' ' || *rt == '\t') rt++; if (*rt == '\"') { rt++; char* end = strchr(rt, '\"'); if (end) { *end = 0; strncpy(gdRefreshToken, rt, GD_TOKEN_LEN-1); } } }
    if (gdAccessToken[0]) {
        gdTokenValido = true;
        if (gdRefreshToken[0]) gd_salvar_nas_chaves();
        sprintf(gdStatus, "Token obtido!");
    } else {
        char* err = strstr(buf, "\"error\"");
        if (err) { err = strchr(err, ':') + 1; while (*err == ' ' || *err == '\t') err++; if (*err == '\"') { err++; char* eend = strchr(err, '\"'); if (eend) { *eend = 0; snprintf(gdStatus, sizeof(gdStatus), "Erro: %s", err); } } }
        else { strcpy(gdStatus, "Falha ao obter token"); }
    }
    free(buf);
}

static void gd_refresh_access_token(void) {
    if (gdRefreshToken[0] == 0) { gdTokenValido = false; return; }
    if (gdClientId[0] == 0) strcpy(gdClientId, "SEU_CLIENT_ID");
    if (gdClientSecret[0] == 0) strcpy(gdClientSecret, "SEU_CLIENT_SECRET");
    char tempBat[1024]; snprintf(tempBat, sizeof(tempBat), "%s/gd_refresh.bat", getenv("TEMP"));
    char outFile[1024]; snprintf(outFile, sizeof(outFile), "%s/gd_refresh_out.txt", getenv("TEMP"));
    FILE* f = fopen(tempBat, "w");
    if (!f) { strcpy(gdStatus, "Erro: criar bat refresh"); return; }
    fprintf(f, "@echo off\n");
    fprintf(f, "\"C:\\Windows\\System32\\curl.exe\" -s -X POST \"https://oauth2.googleapis.com/token\" --data-urlencode \"refresh_token=%s\" --data-urlencode \"client_id=%s\" --data-urlencode \"client_secret=%s\" --data-urlencode \"grant_type=refresh_token\" > \"%s\" 2>&1\n", gdRefreshToken, gdClientId, gdClientSecret, outFile);
    fclose(f);
    char cmd[1100]; snprintf(cmd, sizeof(cmd), "\"%s\"", tempBat);
    exec_silent(cmd);
    FILE* out = fopen(outFile, "r");
    if (!out) { strcpy(gdStatus, "Erro: ler saida refresh"); return; }
    long sz; fseek(out, 0, SEEK_END); sz = ftell(out); fseek(out, 0, SEEK_SET);
    char* buf = (char*)malloc(sz + 1); if (!buf) { fclose(out); return; }
    fread(buf, 1, sz, out); buf[sz] = 0; fclose(out);
    char* at = strstr(buf, "\"access_token\"");
    if (at) {
        at = strchr(at, ':') + 1; while (*at == ' ' || *at == '\t') at++;
        if (*at == '\"') { at++; char* end = strchr(at, '\"'); if (end) { *end = 0; strncpy(gdAccessToken, at, GD_TOKEN_LEN-1); } }
        gdTokenValido = true; sprintf(gdStatus, "Token OK (%.10s...)", gdAccessToken);
    }
    else {
        gdTokenValido = false;
        char* err = strstr(buf, "\"error_description\"");
        if (err) { err = strchr(err, ':') + 1; while (*err == ' ' || *err == '\t') err++; if (*err == '\"') { err++; char* eend = strchr(err, '\"'); if (eend) { *eend = 0; snprintf(gdStatus, sizeof(gdStatus), "Erro: %s", err); } } }
        else if (buf[0]) {
            char tmp[256]; int ml = strlen(buf); if (ml > 100) ml = 100;
            strncpy(tmp, buf, ml); tmp[ml] = 0;
            snprintf(gdStatus, sizeof(gdStatus), "Resp: %.100s", tmp);
        }
        else { strcpy(gdStatus, "Token invalido"); }
    }
    free(buf);
}

void gd_iniciar(void) {
    gd_ler_creds_das_chaves();
    if (gdRefreshToken[0]) {
        gd_refresh_access_token();
        if (!gdTokenValido) { gdConectando = false; return; }
        gd_listar("root");
    } else {
        if (gdClientId[0] == 0 || strcmp(gdClientId, "SEU_CLIENT_ID") == 0) {
            strcpy(gdStatus, "Configure a chave 'Google Drive' em Configuracoes > Chaves API");
            gdTokenValido = false; gdConectando = false; return;
        }
        char url[2048];
        snprintf(url, sizeof(url), "https://accounts.google.com/o/oauth2/v2/auth?client_id=%s&redirect_uri=urn:ietf:wg:oauth:2.0:oob&response_type=code&scope=https://www.googleapis.com/auth/drive", gdClientId);
        char cmd[2100]; snprintf(cmd, sizeof(cmd), "cmd /c start \"\" \"%s\"", url);
        exec_async_silent(cmd);
        strcpy(gdStatus, "Abra o navegador, autorize e cole o codigo aqui.");
        gdTokenValido = false; gdConectando = false;
    }
}

void gd_listar(const char* folderId) {
    if (gdAccessToken[0] == 0) { strcpy(gdStatus, "Sem token"); return; }
    sprintf(gdStatus, "Listando...");
    char outFile[1024]; snprintf(outFile, sizeof(outFile), "%s/gd_list_out.txt", getenv("TEMP"));
    char tempBat[1024]; snprintf(tempBat, sizeof(tempBat), "%s/gd_list.bat", getenv("TEMP"));
    FILE* f = fopen(tempBat, "w");
    if (!f) { strcpy(gdStatus, "Erro: criar bat list"); return; }
    char q[1024]; snprintf(q, sizeof(q), "'%s'+in+parents+and+trashed=false", folderId);
    fprintf(f, "@echo off\n");
    fprintf(f, "\"C:\\Windows\\System32\\curl.exe\" -s -H \"Authorization: Bearer %s\" \"https://www.googleapis.com/drive/v3/files?q=%s&fields=files(id,name,mimeType,size)&pageSize=1000\" > \"%s\"\n", gdAccessToken, q, outFile);
    fclose(f);
    char cmd[1100]; snprintf(cmd, sizeof(cmd), "\"%s\"", tempBat);
    exec_silent(cmd);
    FILE* out = fopen(outFile, "r");
    if (!out) { strcpy(gdStatus, "Erro saida list"); return; }
    long sz; fseek(out, 0, SEEK_END); sz = ftell(out); fseek(out, 0, SEEK_SET);
    char* buf = (char*)malloc(sz + 1); if (!buf) { fclose(out); return; }
    fread(buf, 1, sz, out); buf[sz] = 0; fclose(out);
    if (strstr(buf, "\"error\"")) {
        char* msg = gd_json_val(buf, "\"message\"");
        if (msg) { char* eend = strchr(msg, '\"'); if (eend) { *eend = 0; snprintf(gdStatus, sizeof(gdStatus), "Erro: %s", msg); } }
        else {
            char tmp[256]; int ml = strlen(buf); if (ml > 100) ml = 100;
            strncpy(tmp, buf, ml); tmp[ml] = 0;
            snprintf(gdStatus, sizeof(gdStatus), "API: %.100s", tmp);
        }
        free(buf); return;
    }
    gdTotal = 0;
    if (strcmp(folderId, "root") != 0) {
        strcpy(gdItems[gdTotal].nome, "..");
        strcpy(gdItems[gdTotal].id, ".."); gdItems[gdTotal].isDir = 1; gdItems[gdTotal].size = 0; gdTotal++;
    }
    char* pos = buf; char* fi;
    while ((fi = gd_json_val(pos, "\"id\"")) && gdTotal < GD_TOTAL) {
        char* end = strchr(fi, '\"'); if (!end) break;
        char id[GD_PATH_LEN]; int idLen = (int)(end - fi);
        if (idLen > GD_PATH_LEN-1) idLen = GD_PATH_LEN-1;
        strncpy(id, fi, idLen); id[idLen] = 0; pos = end + 1;
        char* nm = gd_json_val(pos, "\"name\"");
        char name[GD_NOME_LEN] = "";
        if (nm) { char* nend = strchr(nm, '\"'); if (nend) { int nl = (int)(nend - nm); if (nl > GD_NOME_LEN-1) nl = GD_NOME_LEN-1; strncpy(name, nm, nl); name[nl] = 0; } }
        char* mt = gd_json_val(pos, "\"mimeType\"");
        int isDir = (mt && strncmp(mt, "application/vnd.google-apps.folder", 33) == 0) ? 1 : 0;
        if (name[0]) { strcpy(gdItems[gdTotal].nome, name); strcpy(gdItems[gdTotal].id, id); gdItems[gdTotal].isDir = isDir; gdTotal++; }
        pos = end + 1;
    }
    free(buf);
    strcpy(gdCurrentPath, folderId);
    sprintf(gdStatus, "Itens: %d", gdTotal);
}

void gd_baixar(const char* fileId, const char* nome) {
    if (gdAccessToken[0] == 0 || !fileId || !nome) return;
    char outFile[1024]; snprintf(outFile, sizeof(outFile), "%s/baixado/%s", baixar_dir_base(), nome);
    char tempBat[1024]; snprintf(tempBat, sizeof(tempBat), "%s/gd_dl.bat", getenv("TEMP"));
    FILE* f = fopen(tempBat, "w");
    if (!f) return;
    fprintf(f, "@echo off\n");
    char dir[1024]; strcpy(dir, outFile); char* lastSlash = strrchr(dir, '/'); if (!lastSlash) lastSlash = strrchr(dir, '\\');
    if (lastSlash) { *lastSlash = 0; _mkdir(dir); }
    fprintf(f, "\"C:\\Windows\\System32\\curl.exe\" -s -L -H \"Authorization: Bearer %s\" \"https://www.googleapis.com/drive/v3/files/%s?alt=media\" -o \"%s\"\n", gdAccessToken, fileId, outFile);
    fclose(f);
    char cmd[1100]; snprintf(cmd, sizeof(cmd), "\"%s\"", tempBat);
    exec_async_silent(cmd);
}

void gd_limpar(void) {
    gdTotal = 0; gdCurrentPath[0] = 0; gdScroll = 0; gdRcItem = -1;
    for (int i = 0; i < GD_TOTAL; i++) gdMarcados[i] = false;
}

void gd_criar_pasta(const char* name, const char* parentId) {
    if (gdAccessToken[0] == 0 || !name || name[0] == 0) return;
    char tempBat[1024]; snprintf(tempBat, sizeof(tempBat), "%s/gd_mkdir.bat", getenv("TEMP"));
    char outFile[1024]; snprintf(outFile, sizeof(outFile), "%s/gd_mkdir_out.txt", getenv("TEMP"));
    char jsonFile[1024]; snprintf(jsonFile, sizeof(jsonFile), "%s/gd_mkdir.json", getenv("TEMP"));
    FILE* f = fopen(tempBat, "w");
    if (!f) return;
    const char* p = (parentId && parentId[0]) ? parentId : "root";
    fprintf(f, "@echo off\n");
    fprintf(f, "echo {\"name\\\":\\\"%%s\\\",\\\"mimeType\\\":\\\"application/vnd.google-apps.folder\\\",\\\"parents\\\":[\\\"%%s\\\"]} > \"%s\"\n", jsonFile, name, p);
    fprintf(f, "\"C:\\Windows\\System32\\curl.exe\" -s -X POST -H \"Authorization: Bearer %s\" -H \"Content-Type: application/json\" -d @\"%s\" \"https://www.googleapis.com/drive/v3/files\" > \"%s\"\n", gdAccessToken, jsonFile, outFile);
    fclose(f);
    char cmd[1100]; snprintf(cmd, sizeof(cmd), "\"%s\"", tempBat);
    exec_silent(cmd);
}

void gd_deletar(const char* fileId) {
    if (gdAccessToken[0] == 0 || !fileId || fileId[0] == 0) return;
    char tempBat[1024]; snprintf(tempBat, sizeof(tempBat), "%s/gd_del.bat", getenv("TEMP"));
    char outFile[1024]; snprintf(outFile, sizeof(outFile), "%s/gd_del_out.txt", getenv("TEMP"));
    FILE* f = fopen(tempBat, "w");
    if (!f) return;
    fprintf(f, "@echo off\n");
    fprintf(f, "\"C:\\Windows\\System32\\curl.exe\" -s -X DELETE -H \"Authorization: Bearer %s\" \"https://www.googleapis.com/drive/v3/files/%s\" > \"%s\"\n", gdAccessToken, fileId, outFile);
    fclose(f);
    char cmd[1100]; snprintf(cmd, sizeof(cmd), "\"%s\"", tempBat);
    exec_silent(cmd);
}
