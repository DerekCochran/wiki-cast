/*
 * accum.c — Accumulator implementation.
 */
#include "accum.h"

#include <assert.h>
#include <stdlib.h>

#define ACCUM_INIT_CAP 32

void accum_init(Accum *a) {
	a->tokens= malloc(ACCUM_INIT_CAP * sizeof(Token *));
	assert(a->tokens);
	a->count= 0;
	a->cap= ACCUM_INIT_CAP;
}

void accum_push(Accum *a, Token *t) {
	assert(a && t);
	if(a->count >= a->cap) {
		a->cap*= 2;
		a->tokens= realloc(a->tokens, a->cap * sizeof(Token *));
		assert(a->tokens);
	}
	a->tokens[a->count++]= t;
}

Token *accum_get(const Accum *a, size_t i) {
	if(!a || i >= a->count) return NULL;
	return a->tokens[i];
}

// TODO:  Do a true free and rename this to reset
void accum_free(Accum *a) {
	if(!a) return;
	//free(a->tokens);
	//a->tokens= NULL;
	a->count= 0;
	//a->cap= 0;
}
