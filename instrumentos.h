#ifndef INSTRUMENTOS_H
#define INSTRUMENTOS_H

#include <stdint.h>
#include <stddef.h>

void initInstrumentos(void);
void renderizarInstrumentosRaylib(void);
void misturarAudioPiano(int16_t* bufferAudio, size_t frames);
void fecharInstrumentos(void);

extern float audioTempoAtual;
extern char musicaAtual[256];

#endif
