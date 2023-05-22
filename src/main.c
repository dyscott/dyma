#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "dyma.h"
#include "dyma_utils.h"

int main(int argc, char const *argv[]) {
    char* ptr = dy_malloc(sizeof(char) * 13);

    strcpy(ptr, "Hello world!");

    printf("%s\n", ptr);

    dy_free(ptr);

    return EXIT_SUCCESS;
}
