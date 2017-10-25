#include "queue.h"

#include <assert.h>
#include <stdlib.h>

struct QueueNode {
  void* data;
  struct QueueNode* next;
};

typedef struct QueueNode QueueNode;

void QueueInit(Queue* self) {
  self->top_ = NULL;
  self->back_ = NULL;
}

void QueueDestroy(Queue* self) {
  if (!self->top_)
    return;
  QueueNode* cursor = self->top_->next;
  QueueNode* to_delete = self->top_;
  while (cursor != NULL) {
    free(to_delete);
    to_delete = cursor;
    cursor = cursor->next;
  }
  free(to_delete);
}

void* QueueTop(const Queue* self) {
  if (!self->top_)
    return NULL;
  else
    return self->top_->data;
}

void* QueuePop(Queue* self) {
  if (!self->top_) {
    return NULL;
  }
  void* data = self->top_->data;
  QueueNode* to_cleanup = self->top_;
  self->top_ = to_cleanup->next;
  if (!self->top_)
    self->back_ = NULL;
  free(to_cleanup);
  return data;
}

void QueuePush(Queue* self, void* data) {
  QueueNode* new_back = malloc(sizeof(QueueNode));
  assert(data);
  new_back->next = NULL;
  new_back->data = data;
  if (!self->back_)
    self->top_ = new_back;
  else
    self->back_->next = new_back;
  self->back_ = new_back;
}

int QueueEmpty(Queue* self) {
  return self->top_ == NULL;
}
