import os

def criarEstruturaSubpastas(path):
    os.makedirs(path, exist_ok=True)
    os.makedirs(f"{path}/artwork1", exist_ok=True)
    os.makedirs(f"{path}/artwork2", exist_ok=True)
    os.makedirs(f"{path}/artwork3", exist_ok=True)
    os.makedirs(f"{path}/background", exist_ok=True)
    os.makedirs(f"{path}/barra_de_load", exist_ok=True)
    os.makedirs(f"{path}/elementos1", exist_ok=True)
    os.makedirs(f"{path}/ponteiros", exist_ok=True)

def inicializarPastasGamesESistemas(rootMidia):
    pathBase = f"{rootMidia}/Games"
    os.makedirs(pathBase, exist_ok=True)
    
    atari = ["Atari - 2600", "Atari - 5200", "Atari - 7800", "Atari - Jaguar", "Atari - Lynx", "Atari - ST", "Atari - XEGS"]
    for c in atari: criarEstruturaSubpastas(f"{pathBase}/Atari/{c}")

    ms = ["Microsoft - Xbox", "Microsoft - Xbox 360", "Microsoft - Xbox One"]
    for c in ms: criarEstruturaSubpastas(f"{pathBase}/Microsoft/{c}")

    nin = ["Nintendo - Game Boy", "Nintendo - Game Boy Advance", "Nintendo - Game Boy Color", "Nintendo - GameCube", "Nintendo - Nintendo 3DS", "Nintendo - Nintendo 64", "Nintendo - Nintendo DS", "Nintendo - Nintendo Entertainment System", "Nintendo - Nintendo Switch", "Nintendo - Super Nintendo Entertainment System", "Nintendo - Virtual Boy", "Nintendo - Wii", "Nintendo - Wii U"]
    for c in nin: criarEstruturaSubpastas(f"{pathBase}/Nintendo/{c}")

    sega = ["Sega - 32X", "Sega - Dreamcast", "Sega - Game Gear", "Sega - Master System - Mark III", "Sega - Mega Drive - Genesis", "Sega - Sega CD", "Sega - Saturn", "Sega - SG-1000"]
    for c in sega: criarEstruturaSubpastas(f"{pathBase}/Sega/{c}")

    sony = ["Sony - PlayStation", "Sony - PlayStation 2", "Sony - PlayStation 3", "Sony - PlayStation 4", "Sony - PlayStation Portable", "Sony - PlayStation Vita"]
    for c in sony: criarEstruturaSubpastas(f"{pathBase}/Sony/{c}")

    sistemasPath = f"{rootMidia}/Sistemas"
    os.makedirs(sistemasPath, exist_ok=True)
    sistemas = ["Editar", "Midia", "Explorar", "Baixar"]
    for c in sistemas:
        os.makedirs(f"{sistemasPath}/{c}", exist_ok=True)
        criarEstruturaSubpastas(f"{sistemasPath}/{c}/Geral")

def inicializarPastas():
    dirs = [
        "data/HyperNeiva/configuracao/temporario",
        "data/HyperNeiva/configuracao/imagens/elementos",
        "data/HyperNeiva/configuracao/imagens/sistema",
        "data/HyperNeiva/configuracao/audios/elementos_sonoros",
        "data/HyperNeiva/configuracao/jogar",
        "data/HyperNeiva/configuracao/settings",
        "data/HyperNeiva/configuracao/video_imagens",
        "data/HyperNeiva/configuracao/game_e_sistema",
        "data/HyperNeiva/configuracao/repositorios/games",
        "data/HyperNeiva/configuracao/repositorios/imagens para perfil",
        "data/HyperNeiva/configuracao/repositorios/xml games",
        "data/HyperNeiva/baixado/linkdireto",
        "data/HyperNeiva/baixado/dropbox",
        "data/HyperNeiva/baixado/capas",
        "data/HyperNeiva/baixado/repositorios/games",
        "data/HyperNeiva/midia/musicas",
        "data/HyperNeiva/midia/videos/filmes",
        "data/HyperNeiva/midia/videos/series",
        "data/HyperNeiva/midia/videos/animes",
        "data/HyperNeiva/midia/videos/outros",
        "data/HyperNeiva/midia/imagens",
        "data/HyperNeiva/midia/audios/elementos sonoros",
        "data/HyperNeiva/midia/leitura/livros",
        "data/HyperNeiva/midia/leitura/mangas",
        "data/HyperNeiva/midia/arquivos",
        "data/HyperNeiva/Musicas",
        "data/retroarch/Games/Nintendo - Nintendo Entertainment System",
        "data/retroarch/Games/Nintendo - Super Nintendo Entertainment System",
        "data/retroarch/Games/Nintendo - Game Boy",
        "data/retroarch/Games/Nintendo - Game Boy Color",
        "data/retroarch/Games/Nintendo - Game Boy Advance",
        "data/retroarch/Games/Nintendo - Nintendo 64",
        "data/retroarch/Games/Sega - SG-1000",
        "data/retroarch/Games/Sega - Master System - Mark III",
        "data/retroarch/Games/Sega - Mega Drive - Genesis",
        "data/retroarch/Games/Sega - 32X",
        "data/retroarch/Games/Sega - Saturn",
        "data/retroarch/Games/Sega - Dreamcast",
        "data/retroarch/Games/Sega - Game Gear",
        "data/retroarch/Games/Sony - PlayStation",
        "data/retroarch/Games/Sony - PlayStation 2",
        "data/retroarch/Games/Sony - PlayStation Portable",
        "data/retroarch/Games/SNK - Neo Geo",
        "data/retroarch/Games/Bandai - WonderSwan Color",
        "data/retroarch/savefiles",
        "data/retroarch/savestates"
    ]
    for d in dirs: os.makedirs(d, exist_ok=True)
    
    inicializarPastasGamesESistemas("data/HyperNeiva/midia/imagens")
    inicializarPastasGamesESistemas("data/HyperNeiva/midia/videos/video")
    inicializarPastasGamesESistemas("data/HyperNeiva/midia/audios/elementos sonoros")

if __name__ == '__main__':
    inicializarPastas()
    print("Todas as pastas foram criadas com sucesso!")
