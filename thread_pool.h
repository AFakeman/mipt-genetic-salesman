#include <pthread.h>
#include <stdatomic.h>

#include "queue.h"

typedef struct {
  size_t thread_count_;
  pthread_t* threads_;
  atomic_int shutdown_;
  atomic_int done_;
  Queue queue_;
  size_t task_count_;
  pthread_mutex_t queue_mutex_;
  pthread_cond_t queue_condvar_;
} ThreadPool;

typedef struct ThreadTask {
  void* data;
  atomic_int pending_;
  struct ThreadTask* dep_;
  void (*task)(void*);
} ThreadTask;

// Create an empty thread pool
void ThreadPoolInit(ThreadPool* self, size_t thread_count);

void ThreadPoolDestroy(ThreadPool* self);

// Start processing tasks.
void ThreadPoolStart(ThreadPool* self);

// Shut the pool down and stop accepting new tasks.
void ThreadPoolShutdown(ThreadPool* self);

// Reset the pool after it has been joined to reuse it.
void ThreadPoolReset(ThreadPool* self);

// Create a task for the pool.
void ThreadPoolCreateTask(ThreadTask* task, void* data, void (*func)(void*));

// Set a |dep| as depending on |parent|, meaning
// that the |parent| will add |dep| to the queue once completed.
// Dependant to parent is one-to-many relationship, meaning
// that dependant shall be added only once all related parents
// are finished. All dependencies should be set up before adding
// any parent task to pool, and two calls for one parent is not
// allowed.
void ThreadPoolSetDependant(ThreadTask* parent, ThreadTask* dep);

// Add a task to the pool if it is still working.
// This transfers ownership of the task to the pool.
void ThreadPoolAddTask(ThreadPool* self, ThreadTask* task);

// Wait for all the threads to stop.
void ThreadPoolJoin(ThreadPool* self);