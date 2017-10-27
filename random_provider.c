#include "random_provider.h"

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include "queue.h"

const size_t kRandomQueueSize = 64;
const size_t kRandomQueueChunkSize = 1024;

struct RandomProvider {
  Queue queue_;
  size_t queue_size_;
  pthread_mutex_t mutex_;
  pthread_cond_t cond_consumer_;
  pthread_cond_t cond_producer_;
  atomic_int shutdown_;
  pthread_t thread_;
};

void* GenerateRandomChunk(size_t size) {
  size_t i;
  unsigned* chunk = (unsigned*)malloc(sizeof(unsigned) * kRandomQueueChunkSize);
  for (i = 0; i < kRandomQueueChunkSize; i++) {
    chunk[i] = rand();
  }
  return chunk;
}

void* RandomProviderThreadJob(void* in) {
  RandomProvider* self = (RandomProvider*)in;
  pthread_mutex_lock(&(self->mutex_));
  // printf("START\n");
  while (!atomic_load(&(self->shutdown_))) {
    while (self->queue_size_ < kRandomQueueSize) {
      QueuePush(&(self->queue_), GenerateRandomChunk(kRandomQueueChunkSize));
      ++self->queue_size_;
      pthread_mutex_unlock(&(self->mutex_));
      pthread_cond_signal(&(self->cond_consumer_));
      // Let the other threads grab a fresh chunk.
      pthread_mutex_lock(&(self->mutex_));
    }
    pthread_cond_wait(&(self->cond_producer_), &(self->mutex_));
  }
  return NULL;
}

RandomProvider* RandomProviderCreate() {
  RandomProvider* self = (RandomProvider*)malloc(sizeof(RandomProvider));
  pthread_mutex_init(&(self->mutex_), NULL);
  QueueInit(&(self->queue_));
  self->queue_size_ = 0;
  pthread_cond_init(&(self->cond_consumer_), NULL);
  pthread_cond_init(&(self->cond_producer_), NULL);
  atomic_store(&(self->shutdown_), 0);
  pthread_create(&(self->thread_), NULL, RandomProviderThreadJob, self);
  return self;
}

void RandomProviderDelete(RandomProvider* self) {
  void* queue_el;
  RandomProviderShutdown(self);
  pthread_cond_signal(&(self->cond_producer_));
  pthread_join(self->thread_, NULL);
  pthread_mutex_destroy(&(self->mutex_));
  pthread_cond_destroy(&(self->cond_producer_));
  pthread_cond_destroy(&(self->cond_consumer_));
  while ((queue_el = QueuePop(&(self->queue_)))) {
    free(queue_el);
  }
  assert(QueueEmpty(&(self->queue_)));
  QueueDestroy(&(self->queue_));
  free(self);
}

unsigned* RandomProviderPopRandom(RandomProvider* self) {
  unsigned* to_return;
  pthread_mutex_lock(&(self->mutex_));
  while (self->queue_size_ == 0) {
    pthread_cond_wait(&(self->cond_consumer_), &(self->mutex_));
  }
  to_return = (unsigned*)QueuePop(&(self->queue_));
  if (--self->queue_size_ != 0) {
    pthread_cond_signal(&(self->cond_consumer_));
  }
  if (self->queue_size_ < kRandomQueueSize / 2) {
    pthread_cond_signal(&(self->cond_producer_));
  }
  pthread_mutex_unlock(&(self->mutex_));
  return to_return;
}

void RandomProviderShutdown(RandomProvider* self) {
  atomic_store(&(self->shutdown_), 1);
  pthread_cond_signal(&(self->cond_producer_));
}