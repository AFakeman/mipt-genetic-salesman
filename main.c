#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "random_provider.h"

int main() {
	RandomProvider* provider = RandomProviderCreate();
	sleep(1);

	for(size_t i = 0; i < 10000; ++i) {
		unsigned* ptr = RandomProviderPopRandom(provider);
		printf("%p\n", ptr);
		free(ptr);
	}

	RandomProviderShutdown(provider);
	RandomProviderDelete(provider);
}