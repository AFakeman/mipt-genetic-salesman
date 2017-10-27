#include "graph.h"

typedef struct PathData {
	size_t iterations;
	double time;
	int* best_path;
} ShortestPathData;

int ShortestPath(const graph_t* graph,
                  size_t thread_count,
                  size_t population_size,
                  size_t same_fitness_for,
                  ShortestPathData *return_data);