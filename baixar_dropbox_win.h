#ifndef BAIXAR_DROPBOX_WIN_H
#define BAIXAR_DROPBOX_WIN_H

#include <stdbool.h>

#define DROPBOX_MAX_ITEMS 2000
#define DROPBOX_PATH_LEN 1024

typedef struct {
    char nome[256];
    char path[DROPBOX_PATH_LEN];
    int isDir;
} DropboxItem;

extern DropboxItem dropboxItems[DROPBOX_MAX_ITEMS];
extern int dropboxTotal;
extern char dropboxCurrentPath[DROPBOX_PATH_LEN];
extern char dropboxToken[2048];
extern char dropboxStatus[256];
extern bool dropboxTokenValido;

void dropbox_iniciar(void);
bool dropbox_obter_token(void);
void dropbox_listar(const char* path);
void dropbox_baixar(const char* remotePath, const char* nomeArquivo);
void dropbox_deletar(const char* remotePath);
void dropbox_renomear(const char* remotePath, const char* novoNome);
void dropbox_criar_pasta(const char* remotePath);
void dropbox_limpar(void);

#endif
