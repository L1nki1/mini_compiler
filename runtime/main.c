#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

extern int64_t compiled_fn(int64_t arg);

int main(void) {
    int64_t input = 7;
    int64_t result = compiled_fn(input);
    printf("%" PRId64 "\n", result);
    return 0;
}
