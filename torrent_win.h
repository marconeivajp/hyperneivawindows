#ifndef TORRENT_WIN_H
#define TORRENT_WIN_H

#include <stdbool.h>

#define TORRENT_MAX 60
#define TORRENT_PATH_LEN 1024

typedef struct {
    char nome[256];
    char magnet[1024];
    char tamanho[64];
    char seeds[32];
    int seedsNum;
    int ehYTS;
} TorrentItem;

extern TorrentItem torrentResultados[TORRENT_MAX];
extern int torrentTotal;
extern char torrentStatus[256];
extern bool torrentBuscando;
extern char torrentTermo[256];

void torrent_init(void);
void torrent_buscar(const char* termo);
void torrent_buscar_provedor(int provedor, const char* termo);
void torrent_aguardar(void);
void torrent_abrir_magnet(int idx);
void torrent_baixar_torrent(int idx);

#endif
