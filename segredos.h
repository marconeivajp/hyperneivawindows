/* Escolhe as chaves: segredos_local.h (fora do git) ou o exemplo vazio. */
#ifndef SEGREDOS_H
#define SEGREDOS_H
#if __has_include("segredos_local.h")
#include "segredos_local.h"
#else
#include "segredos_exemplo.h"
#endif
#endif
