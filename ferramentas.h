#ifndef FERRAMENTAS_H
#define FERRAMENTAS_H

#include "raylib.h"
#include <stdbool.h>

void initFerramentas(void);
void renderizarFerramentasRaylib(void);
bool deveFecharFerramentas(void);
void fecharFerramentas(void);
void abrirTextoNoBlocoNotas(const char* filepath);

#endif
