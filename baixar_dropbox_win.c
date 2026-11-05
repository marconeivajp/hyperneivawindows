#include "baixar_dropbox_win.h"
#include "baixar_win.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <direct.h>

#include "segredos.h"

typedef struct {
    char nome[64];
    char desc[256];
    char chave[512];
    char app_id[256];
} ApiKey;

extern ApiKey apiKeys[];
extern int apiKeyTotal;

DropboxItem dropboxItems[DROPBOX_MAX_ITEMS];
int dropboxTotal = 0;
char dropboxCurrentPath[DROPBOX_PATH_LEN] = "";
char dropboxToken[2048] = "";
char dropboxStatus[256] = "Pronto.";
bool dropboxTokenValido = false;

static char g_appKey[128] = "";
static char g_appSecret[128] = "";
static char g_refreshToken[512] = "";

static int dropbox_run_cmd(const char* cmd) {
    char full[4096];
    sprintf(full, "cmd /c \"%s\"", cmd);
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(NULL, full, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ec = 0;
    GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)ec;
}

void dropbox_limpar(void) {
    dropboxTotal = 0;
    dropboxTokenValido = false;
    dropboxToken[0] = 0;
    g_appKey[0] = 0;
    g_appSecret[0] = 0;
    g_refreshToken[0] = 0;
    strcpy(dropboxCurrentPath, "");
    sprintf(dropboxStatus, "Pronto.");
}

void dropbox_iniciar(void) {
    dropboxTotal = 0;
    dropboxTokenValido = false;
    dropboxToken[0] = 0;
    g_appKey[0] = 0;
    g_appSecret[0] = 0;
    g_refreshToken[0] = 0;
    strcpy(dropboxCurrentPath, "");
    sprintf(dropboxStatus, "Inicializando...");

    strcpy(g_appKey, DROPBOX_DEFAULT_APPKEY);
    strcpy(g_appSecret, DROPBOX_DEFAULT_APPSECRET);

    char txt_token[2048] = { 0 };
    char cwd[1024]; _getcwd(cwd, 1024);
    char tokenPath[1024]; snprintf(tokenPath, sizeof(tokenPath), "%s/data/HyperNeiva/configuracao/dropbox_token.txt", cwd);
    FILE* fToken = fopen(tokenPath, "rb");
    if (fToken) {
        fseek(fToken, 0, SEEK_END); long sz = ftell(fToken); fseek(fToken, 0, SEEK_SET);
        if (sz > 0 && sz < 2047) { fread(txt_token, 1, sz, fToken); txt_token[sz] = '\0'; }
        fclose(fToken);
    }

    if (strlen(txt_token) < 15) {
        sprintf(dropboxStatus, "ERRO: Token nao encontrado! Gere token nas Configuracoes");
        return;
    }

    char refreshToken[512] = { 0 };
    bool temLabels = false;
    char txtCopy[2048]; strcpy(txtCopy, txt_token);

    char* linha = strtok(txtCopy, "\r\n");
    while (linha) {
        if (strstr(linha, "Appkey:") || strstr(linha, "App key:") || strstr(linha, "AppKey:")) {
            char* val = strchr(linha, ':');
            if (val) { while (*val == ':' || *val == ' ') val++; sscanf(val, "%s", g_appKey); }
            temLabels = true;
        } else if (strstr(linha, "App secret:") || strstr(linha, "Appsecret:") || strstr(linha, "App Secret:")) {
            char* val = strchr(linha, ':');
            if (val) { while (*val == ':' || *val == ' ') val++; sscanf(val, "%s", g_appSecret); }
            temLabels = true;
        } else if (strstr(linha, "Token refresh:") || strstr(linha, "Tokenrefresh:") || strstr(linha, "Token Refresh:")) {
            char* val = strchr(linha, ':');
            if (val) { while (*val == ':' || *val == ' ') val++; sscanf(val, "%s", refreshToken); }
            temLabels = true;
        }
        linha = strtok(NULL, "\r\n");
    }

    if (!temLabels) {
        sscanf(txt_token, "%s", refreshToken);
    }

    if (strlen(refreshToken) < 15) {
        sprintf(dropboxStatus, "ERRO: Token invalido no arquivo");
        return;
    }

    strncpy(g_refreshToken, refreshToken, sizeof(g_refreshToken) - 1);
    g_refreshToken[sizeof(g_refreshToken) - 1] = 0;

    dropbox_obter_token();
}

bool dropbox_obter_token(void) {
    if (!g_refreshToken[0]) {
        sprintf(dropboxStatus, "Sem refresh token");
        dropboxTokenValido = false;
        return false;
    }

    char tempDir[1024] = "";
    const char* tmp = getenv("TEMP");
    if (tmp) strcpy(tempDir, tmp);
    if (!tempDir[0]) strcpy(tempDir, "C:\\Temp");

    char tmpFile[1024];
    sprintf(tmpFile, "%s\\dropbox_token.json", tempDir);

    sprintf(dropboxStatus, "Obtendo token Dropbox...");

    char cmd[4096];
    sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s -X POST \"https://api.dropbox.com/oauth2/token\" -H \"Content-Type: application/x-www-form-urlencoded\" -d \"grant_type=refresh_token&refresh_token=%s&client_id=%s&client_secret=%s\" > \"%s\"",
            g_refreshToken, g_appKey, g_appSecret, tmpFile);

    int ec = dropbox_run_cmd(cmd);
    if (ec != 0) {
        sprintf(dropboxStatus, "Falha ao executar curl para token");
        dropboxTokenValido = false;
        return false;
    }

    FILE* f = fopen(tmpFile, "rb");
    if (!f) {
        sprintf(dropboxStatus, "Falha ao ler resposta do token");
        dropboxTokenValido = false;
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(sz + 1);
    if (!buf) { fclose(f); dropboxTokenValido = false; return false; }
    fread(buf, 1, sz, f);
    buf[sz] = 0;
    fclose(f);

    bool ok = false;
    char* tokenStart = strstr(buf, "\"access_token\": \"");
    if (tokenStart) {
        tokenStart += 17;
        char* tokenEnd = strchr(tokenStart, '\"');
        if (tokenEnd) {
            int tokenLen = (int)(tokenEnd - tokenStart);
            if (tokenLen > 2047) tokenLen = 2047;
            strncpy(dropboxToken, tokenStart, tokenLen);
            dropboxToken[tokenLen] = 0;
            dropboxTokenValido = true;
            ok = true;
            sprintf(dropboxStatus, "Token obtido com sucesso");
        }
    }

    if (!ok) {
        char* errStart = strstr(buf, "\"error_description\": \"");
        if (errStart) {
            errStart += 22;
            char* errEnd = strchr(errStart, '\"');
            if (errEnd) {
                int errLen = (int)(errEnd - errStart);
                if (errLen > 100) errLen = 100;
                char errMsg[128];
                strncpy(errMsg, errStart, errLen);
                errMsg[errLen] = 0;
                sprintf(dropboxStatus, "Erro token: %s", errMsg);
            }
        } else {
            sprintf(dropboxStatus, "Falha ao extrair token do JSON");
        }
        dropboxTokenValido = false;
    }

    free(buf);
    remove(tmpFile);
    return ok;
}

void dropbox_listar(const char* path) {
    dropboxTotal = 0;

    if (!dropboxTokenValido) {
        if (!dropbox_obter_token()) return;
    }

    char safePath[DROPBOX_PATH_LEN];
    if (!path || !path[0]) strcpy(safePath, "");
    else strcpy(safePath, path);

    char tempDir[1024] = "";
    const char* tmp = getenv("TEMP");
    if (tmp) strcpy(tempDir, tmp);
    if (!tempDir[0]) strcpy(tempDir, "C:\\Temp");

    char tmpFile[1024];
    sprintf(tmpFile, "%s\\dropbox_list.json", tempDir);

    sprintf(dropboxStatus, "Listando pasta...");

    char cmd[4096];
    sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s -X POST \"https://api.dropboxapi.com/2/files/list_folder\" -H \"Authorization: Bearer %s\" -H \"Content-Type: application/json\" -d \"{\"\"path\"\":\"\"%s\"\"}\" > \"%s\"",
            dropboxToken, safePath, tmpFile);

    dropbox_run_cmd(cmd);

    FILE* f = fopen(tmpFile, "rb");
    if (!f) {
        sprintf(dropboxStatus, "Falha ao ler listagem");
        return;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(sz + 1);
    if (!buf) { fclose(f); return; }
    fread(buf, 1, sz, f);
    buf[sz] = 0;
    fclose(f);

    char* errSummary = strstr(buf, "\"error_summary\": \"");
    if (errSummary) {
        errSummary += 18;
        char* errEnd = strchr(errSummary, '\"');
        if (errEnd) {
            int errLen = (int)(errEnd - errSummary);
            if (errLen > 100) errLen = 100;
            char errMsg[128];
            strncpy(errMsg, errSummary, errLen);
            errMsg[errLen] = 0;
            sprintf(dropboxStatus, "API erro: %s", errMsg);
        }
        free(buf);
        remove(tmpFile);
        return;
    }

    char* p = buf;
    while ((p = strstr(p, "\".tag\":\"")) && dropboxTotal < DROPBOX_MAX_ITEMS) {
        p += 8;
        bool isFolder = (strncmp(p, "folder", 6) == 0);

        char* namePtr = strstr(p, "\"name\":\"");
        if (!namePtr) break;
        namePtr += 8;
        char* nameEnd = strchr(namePtr, '\"');
        if (!nameEnd) break;

        char* pathPtr = strstr(nameEnd, "\"path_display\":\"");
        if (!pathPtr) break;
        pathPtr += 16;
        char* pathEnd = strchr(pathPtr, '\"');
        if (!pathEnd) break;

        int nameLen = (int)(nameEnd - namePtr);
        int pathLen = (int)(pathEnd - pathPtr);
        if (nameLen > 255) nameLen = 255;
        if (pathLen > DROPBOX_PATH_LEN - 1) pathLen = DROPBOX_PATH_LEN - 1;

        strncpy(dropboxItems[dropboxTotal].nome, namePtr, nameLen);
        dropboxItems[dropboxTotal].nome[nameLen] = 0;
        strncpy(dropboxItems[dropboxTotal].path, pathPtr, pathLen);
        dropboxItems[dropboxTotal].path[pathLen] = 0;
        dropboxItems[dropboxTotal].isDir = isFolder ? 1 : 0;
        dropboxTotal++;

        p = pathEnd;
    }

    free(buf);
    remove(tmpFile);

    if (dropboxTotal == 0) {
        strcpy(dropboxItems[0].nome, "Pasta Vazia");
        dropboxItems[0].path[0] = 0;
        dropboxItems[0].isDir = 0;
        dropboxTotal = 1;
    }

    strcpy(dropboxCurrentPath, safePath);
    sprintf(dropboxStatus, "%d itens carregados", dropboxTotal);
}

void dropbox_baixar(const char* remotePath, const char* nomeArquivo) {
    if (!dropboxTokenValido) {
        if (!dropbox_obter_token()) return;
    }

    const char* base = baixar_dir_base();
    char destDir[1024];
    sprintf(destDir, "%s/dropbox", base);
    _mkdir(destDir);

    char destFile[1024];
    sprintf(destFile, "%s/%s", destDir, nomeArquivo);

    sprintf(dropboxStatus, "Baixando %s...", nomeArquivo);

    char cmd[4096];
    sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s -X POST \"https://content.dropboxapi.com/2/files/download\" -H \"Authorization: Bearer %s\" -H \"Dropbox-API-Arg: {\"\"path\"\":\"\"%s\"\"}\" -o \"%s\"",
            dropboxToken, remotePath, destFile);

    int ec = dropbox_run_cmd(cmd);

    if (ec == 0) {
        sprintf(dropboxStatus, "Download concluido: %s", nomeArquivo);
    } else {
        sprintf(dropboxStatus, "Erro ao baixar %s", nomeArquivo);
    }
}

void dropbox_deletar(const char* remotePath) {
    if (!dropboxTokenValido) { if (!dropbox_obter_token()) return; }
    char tempDir[1024] = "";
    const char* tmp = getenv("TEMP");
    if (tmp) strcpy(tempDir, tmp);
    if (!tempDir[0]) strcpy(tempDir, "C:\\Temp");
    char tmpFile[1024]; sprintf(tmpFile, "%s\\dropbox_del.json", tempDir);
    sprintf(dropboxStatus, "Deletando %s...", remotePath);
    char cmd[4096];
    sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s -X POST \"https://api.dropboxapi.com/2/files/delete_v2\" -H \"Authorization: Bearer %s\" -H \"Content-Type: application/json\" -d \"{\"\"path\"\":\"\"%s\"\"}\" > \"%s\"",
            dropboxToken, remotePath, tmpFile);
    int ec = dropbox_run_cmd(cmd);
    if (ec == 0) sprintf(dropboxStatus, "Deletado: %s", remotePath);
    else sprintf(dropboxStatus, "Erro ao deletar %s", remotePath);
    remove(tmpFile);
}

void dropbox_renomear(const char* remotePath, const char* novoNome) {
    if (!dropboxTokenValido) { if (!dropbox_obter_token()) return; }
    char parent[1024]; strcpy(parent, remotePath);
    char* lastSlash = strrchr(parent, '/');
    char newPath[1024];
    if (lastSlash) {
        if (lastSlash == parent) snprintf(newPath, sizeof(newPath), "/%s", novoNome);
        else { *lastSlash = 0; snprintf(newPath, sizeof(newPath), "%s/%s", parent, novoNome); }
    } else snprintf(newPath, sizeof(newPath), "/%s", novoNome);
    char tempDir[1024] = "";
    const char* tmp = getenv("TEMP");
    if (tmp) strcpy(tempDir, tmp);
    if (!tempDir[0]) strcpy(tempDir, "C:\\Temp");
    char tmpFile[1024]; sprintf(tmpFile, "%s\\dropbox_ren.json", tempDir);
    sprintf(dropboxStatus, "Renomeando...");
    char cmd[4096];
    sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s -X POST \"https://api.dropboxapi.com/2/files/move_v2\" -H \"Authorization: Bearer %s\" -H \"Content-Type: application/json\" -d \"{\"\"from_path\"\":\"\"%s\"\",\"\"to_path\"\":\"\"%s\"\"}\" > \"%s\"",
            dropboxToken, remotePath, newPath, tmpFile);
    int ec = dropbox_run_cmd(cmd);
    if (ec == 0) sprintf(dropboxStatus, "Renomeado para: %s", novoNome);
    else sprintf(dropboxStatus, "Erro ao renomear %s", remotePath);
    remove(tmpFile);
}

void dropbox_criar_pasta(const char* remotePath) {
    if (!dropboxTokenValido) { if (!dropbox_obter_token()) return; }
    char tempDir[1024] = "";
    const char* tmp = getenv("TEMP");
    if (tmp) strcpy(tempDir, tmp);
    if (!tempDir[0]) strcpy(tempDir, "C:\\Temp");
    char tmpFile[1024]; sprintf(tmpFile, "%s\\dropbox_mkdir.json", tempDir);
    sprintf(dropboxStatus, "Criando pasta...");
    char cmd[4096];
    sprintf(cmd, "\"C:\\Windows\\System32\\curl.exe\" -s -X POST \"https://api.dropboxapi.com/2/files/create_folder_v2\" -H \"Authorization: Bearer %s\" -H \"Content-Type: application/json\" -d \"{\"\"path\"\":\"\"%s\"\"}\" > \"%s\"",
            dropboxToken, remotePath, tmpFile);
    int ec = dropbox_run_cmd(cmd);
    if (ec == 0) sprintf(dropboxStatus, "Pasta criada: %s", remotePath);
    else sprintf(dropboxStatus, "Erro ao criar pasta %s", remotePath);
    remove(tmpFile);
}
