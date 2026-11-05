#ifndef CRIAR_PASTAS_WIN_H
#define CRIAR_PASTAS_WIN_H

#include <stdio.h>
#include <direct.h>
#include <string.h>

void criarEstruturaSubpastas(const char* consolePath) {
    char buf[512];
    _mkdir(consolePath);
    snprintf(buf, sizeof(buf), "%s/artwork1", consolePath); _mkdir(buf);
    snprintf(buf, sizeof(buf), "%s/artwork2", consolePath); _mkdir(buf);
    snprintf(buf, sizeof(buf), "%s/artwork3", consolePath); _mkdir(buf);
    snprintf(buf, sizeof(buf), "%s/background", consolePath); _mkdir(buf);
    snprintf(buf, sizeof(buf), "%s/barra_de_load", consolePath); _mkdir(buf);
    snprintf(buf, sizeof(buf), "%s/elementos1", consolePath); _mkdir(buf);
    snprintf(buf, sizeof(buf), "%s/ponteiros", consolePath); _mkdir(buf);
}

void inicializarPastasGamesESistemas(const char* rootMidia) {
    char pathBase[512];
    char pathConsole[512];

    snprintf(pathBase, sizeof(pathBase), "%s/Games", rootMidia);
    _mkdir(pathBase);

    // Atari
    snprintf(pathConsole, sizeof(pathConsole), "%s/Atari", pathBase); _mkdir(pathConsole);
    const char* atari[] = { "Atari - 2600", "Atari - 5200", "Atari - 7800", "Atari - Jaguar", "Atari - Lynx", "Atari - ST", "Atari - XEGS" };
    for (int i = 0; i < 7; i++) { char p[512]; snprintf(p, sizeof(p), "%s/%s", pathConsole, atari[i]); criarEstruturaSubpastas(p); }

    // Microsoft
    snprintf(pathConsole, sizeof(pathConsole), "%s/Microsoft", pathBase); _mkdir(pathConsole);
    const char* ms[] = { "Microsoft - Xbox", "Microsoft - Xbox 360", "Microsoft - Xbox One" };
    for (int i = 0; i < 3; i++) { char p[512]; snprintf(p, sizeof(p), "%s/%s", pathConsole, ms[i]); criarEstruturaSubpastas(p); }

    // Nintendo
    snprintf(pathConsole, sizeof(pathConsole), "%s/Nintendo", pathBase); _mkdir(pathConsole);
    const char* nin[] = { 
        "Nintendo - Game Boy", "Nintendo - Game Boy Advance", "Nintendo - Game Boy Color", 
        "Nintendo - GameCube", "Nintendo - Nintendo 3DS", "Nintendo - Nintendo 64", 
        "Nintendo - Nintendo DS", "Nintendo - Nintendo Entertainment System", 
        "Nintendo - Nintendo Switch", "Nintendo - Super Nintendo Entertainment System", 
        "Nintendo - Virtual Boy", "Nintendo - Wii", "Nintendo - Wii U" 
    };
    for (int i = 0; i < 13; i++) { char p[512]; snprintf(p, sizeof(p), "%s/%s", pathConsole, nin[i]); criarEstruturaSubpastas(p); }

    // Sega
    snprintf(pathConsole, sizeof(pathConsole), "%s/Sega", pathBase); _mkdir(pathConsole);
    const char* sega[] = { 
        "Sega - 32X", "Sega - Dreamcast", "Sega - Game Gear", 
        "Sega - Master System - Mark III", "Sega - Mega Drive - Genesis", 
        "Sega - Sega CD", "Sega - Saturn", "Sega - SG-1000" 
    };
    for (int i = 0; i < 8; i++) { char p[512]; snprintf(p, sizeof(p), "%s/%s", pathConsole, sega[i]); criarEstruturaSubpastas(p); }

    // Sony
    snprintf(pathConsole, sizeof(pathConsole), "%s/Sony", pathBase); _mkdir(pathConsole);
    const char* sony[] = { 
        "Sony - PlayStation", "Sony - PlayStation 2", "Sony - PlayStation 3", 
        "Sony - PlayStation 4", "Sony - PlayStation Portable", "Sony - PlayStation Vita" 
    };
    for (int i = 0; i < 6; i++) { char p[512]; snprintf(p, sizeof(p), "%s/%s", pathConsole, sony[i]); criarEstruturaSubpastas(p); }

    // Sistemas
    snprintf(pathBase, sizeof(pathBase), "%s/Sistemas", rootMidia);
    _mkdir(pathBase);
    const char* sistemas[] = { "Editar", "Midia", "Explorar", "Baixar" };
    for (int i = 0; i < 4; i++) {
        char pS[512]; snprintf(pS, sizeof(pS), "%s/%s", pathBase, sistemas[i]); _mkdir(pS);
        char pG[512]; snprintf(pG, sizeof(pG), "%s/Geral", pS); criarEstruturaSubpastas(pG);
    }
}

void inicializarPastas() {
    _mkdir("data");
    _mkdir("data/HyperNeiva");
    _mkdir("data/HyperNeiva/configuracao");
    _mkdir("data/HyperNeiva/configuracao/temporario");
    _mkdir("data/HyperNeiva/configuracao/imagens");
    _mkdir("data/HyperNeiva/configuracao/imagens/elementos");
    _mkdir("data/HyperNeiva/configuracao/imagens/sistema");
    _mkdir("data/HyperNeiva/configuracao/audios");
    _mkdir("data/HyperNeiva/configuracao/audios/elementos_sonoros");
    _mkdir("data/HyperNeiva/configuracao/jogar");
    _mkdir("data/HyperNeiva/configuracao/settings");
    _mkdir("data/HyperNeiva/configuracao/video_imagens");
    _mkdir("data/HyperNeiva/configuracao/game_e_sistema");
    _mkdir("data/HyperNeiva/configuracao/repositorios");
    _mkdir("data/HyperNeiva/configuracao/repositorios/games");
    _mkdir("data/HyperNeiva/configuracao/repositorios/imagens para perfil");
    _mkdir("data/HyperNeiva/configuracao/repositorios/xml games");
    _mkdir("data/HyperNeiva/baixado");
    _mkdir("data/HyperNeiva/baixado/linkdireto");
    _mkdir("data/HyperNeiva/baixado/dropbox");
    _mkdir("data/HyperNeiva/baixado/capas");
    _mkdir("data/HyperNeiva/baixado/repositorios");
    _mkdir("data/HyperNeiva/baixado/repositorios/games");
    _mkdir("data/HyperNeiva/midia");
    _mkdir("data/HyperNeiva/midia/musicas");
    _mkdir("data/HyperNeiva/midia/videos");
    _mkdir("data/HyperNeiva/midia/imagens");
    _mkdir("data/HyperNeiva/midia/audios");
    _mkdir("data/HyperNeiva/midia/leitura");
    _mkdir("data/HyperNeiva/midia/arquivos");

    inicializarPastasGamesESistemas("data/HyperNeiva/midia/imagens");
    _mkdir("data/HyperNeiva/midia/videos/video");
    inicializarPastasGamesESistemas("data/HyperNeiva/midia/videos/video");
    _mkdir("data/HyperNeiva/midia/audios/elementos sonoros");
    inicializarPastasGamesESistemas("data/HyperNeiva/midia/audios/elementos sonoros");

    _mkdir("data/HyperNeiva/midia/videos/filmes");
    _mkdir("data/HyperNeiva/midia/videos/series");
    _mkdir("data/HyperNeiva/midia/videos/animes");
    _mkdir("data/HyperNeiva/midia/videos/outros");
    _mkdir("data/HyperNeiva/midia/leitura/livros");
    _mkdir("data/HyperNeiva/midia/leitura/mangas");
    _mkdir("data/HyperNeiva/Musicas");

    _mkdir("data/retroarch");
    _mkdir("data/retroarch/Games");
    _mkdir("data/retroarch/savefiles");
    _mkdir("data/retroarch/savestates");

    _mkdir("data/retroarch/Games/Nintendo - Nintendo Entertainment System");
    _mkdir("data/retroarch/Games/Nintendo - Super Nintendo Entertainment System");
    _mkdir("data/retroarch/Games/Nintendo - Game Boy");
    _mkdir("data/retroarch/Games/Nintendo - Game Boy Color");
    _mkdir("data/retroarch/Games/Nintendo - Game Boy Advance");
    _mkdir("data/retroarch/Games/Nintendo - Nintendo 64");

    _mkdir("data/retroarch/Games/Sega - SG-1000");
    _mkdir("data/retroarch/Games/Sega - Master System - Mark III");
    _mkdir("data/retroarch/Games/Sega - Mega Drive - Genesis");
    _mkdir("data/retroarch/Games/Sega - 32X");
    _mkdir("data/retroarch/Games/Sega - Saturn");
    _mkdir("data/retroarch/Games/Sega - Dreamcast");
    _mkdir("data/retroarch/Games/Sega - Game Gear");

    _mkdir("data/retroarch/Games/Sony - PlayStation");
    _mkdir("data/retroarch/Games/Sony - PlayStation 2");
    _mkdir("data/retroarch/Games/Sony - PlayStation Portable");

    _mkdir("data/retroarch/Games/SNK - Neo Geo");
    _mkdir("data/retroarch/Games/Bandai - WonderSwan Color");
}

#endif
