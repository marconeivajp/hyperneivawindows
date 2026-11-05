#ifndef BAIXAR_GOOGLE_DRIVE_WIN_H
#define BAIXAR_GOOGLE_DRIVE_WIN_H

#define GD_TOTAL 4000
#define GD_NOME_LEN 256
#define GD_PATH_LEN 1024
#define GD_TOKEN_LEN 2048

typedef struct {
    char nome[GD_NOME_LEN];
    char id[GD_PATH_LEN];
    int isDir;
    long long size;
} GdItem;

extern GdItem gdItems[GD_TOTAL];
extern int gdTotal;
extern char gdCurrentPath[GD_PATH_LEN];
extern char gdStatus[512];
extern bool gdTokenValido;
extern bool gdConectando;
extern int gdScroll;
extern bool gdMarcados[GD_TOTAL];
extern int gdRcItem;
extern int gdRcX, gdRcY;
extern char gdAccessToken[GD_TOKEN_LEN];
extern char gdRefreshToken[GD_TOKEN_LEN];
extern char gdClientId[256];
extern char gdClientSecret[256];

void gd_iniciar(void);
void gd_listar(const char* folderId);
void gd_baixar(const char* fileId, const char* nome);
void gd_limpar(void);
void gd_exchange_code(const char* code);
void gd_ler_creds_das_chaves(void);
void gd_criar_pasta(const char* name, const char* parentId);
void gd_deletar(const char* fileId);

#endif
