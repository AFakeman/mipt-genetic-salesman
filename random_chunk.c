#include "random_chunk.h"

#include <stddef.h>
#include <stdlib.h>

typedef struct RandomChunk {
  unsigned* chunk;
  size_t cursor;
  size_t length;
  RandomProvider* provider;
} RandomChunk;

RandomChunk* RandomChunkCreate(RandomProvider *provider) {
  RandomChunk *self = (RandomChunk*) malloc(sizeof(RandomChunk));
  self->chunk = RandomProviderPopRandom(provider);
  self->cursor = 0;
  self->length = kRandomQueueChunkSize;
  self->provider = provider;
  return self;
}

void RandomChunkDelete(RandomChunk* self) {
	free(self->chunk);
	free(self);
}

unsigned RandomChunkPopRandom(RandomChunk *self) {
	unsigned to_ret = self->chunk[self->cursor];
	if (++(self->cursor) == self->length) {
		free(self->chunk);
		self->chunk = RandomProviderPopRandom(self->provider);
		self->cursor = 0;
	}
	return to_ret;
}

size_t RandomChunkPopRandomLong(RandomChunk* self) {
	size_t result = 0;
	size_t cursor;
	for (cursor = 0; cursor < sizeof(size_t) / sizeof(unsigned); ++cursor) {
		((unsigned *) &result)[cursor] = RandomChunkPopRandom(self);
	}
	return result;
}