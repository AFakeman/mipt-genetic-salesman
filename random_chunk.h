#include "random_provider.h"

typedef struct RandomChunk RandomChunk;

// Create a random chunk associated with this random provider.
// Does not assume responsibility for it.
RandomChunk* RandomChunkCreate(RandomProvider* provider);
void RandomChunkDelete(RandomChunk* self);

unsigned RandomChunkPopRandom(RandomChunk* self);

size_t RandomChunkPopRandomLong(RandomChunk* self);