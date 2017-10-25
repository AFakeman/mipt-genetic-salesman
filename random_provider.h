#include <stddef.h>

struct RandomProvider;

struct RandomChunk;

// How many |unsigned|'s are there in a chunk.
extern const size_t kRandomQueueChunkSize;

typedef struct RandomProvider RandomProvider;

RandomProvider* RandomProviderCreate();
void RandomProviderDelete(RandomProvider* self);

// Shut down the provider, stopping new blocks from
// creating.
void RandomProviderShutdown(RandomProvider* self);

// Pop an array of |kRandomQueueChunkSize| size
// (transferring ownership to caller).
// If the queue is empty, blocks until completion.
unsigned* RandomProviderPopRandom(RandomProvider* self);