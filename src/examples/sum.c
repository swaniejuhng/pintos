#include <stdio.h>
#include <stdlib.h>
#include "../userprog/syscall.h"
#include "../lib/user/syscall.h"


int main(int argc, char *argv[]) {
    int i, n[5];

    for(i = 0; i < 5; i++)
        n[i] = atoi(argv[i]);

    printf("%d %d\n", fibonacci(n[1]), sum_of_four_int(n[1], n[2], n[3], n[4]));
    return EXIT_SUCCESS;
}
