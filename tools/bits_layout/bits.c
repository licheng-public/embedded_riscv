#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    uint64_t val;
    int8_t cnt = 0; /* the number of bits */
    int8_t i = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s number \n", argv[0]);
        return 1;
    }

    val = strtoll(argv[1], NULL, 0);
    printf("0x%lx\n", val);
    if (val > UINT32_MAX) {
        cnt = 64;
    }
    else {
        cnt = 32;
    }
    printf("/");
    for (i = cnt; i > 1; i--) {
        printf("%c%c%c", '-', '-', '-');
    }
    printf("--\\\n");
    for (i = cnt; i > 0; i--) {
        printf("|%2d", i - 1);
    }
    printf("|\n");

    for (i = cnt; i > 0; i--) {
        printf("%c%c%c", '|', '-', '-');
    }
    printf("|\n");
    /* now print each bits */
    for (i = cnt; i > 0; i--) {
        if ( (val & (1 << (i - 1))) != 0 ) {
            printf("|%2d", 1);
        }
        else {
            printf("|%2d", 0);
        }
    }
    printf("|\n");
    /* end of bits print */

    printf("\\");
    for (i = cnt; i > 1; i--) {
        printf("%c%c%c", '-', '-', '-');
    }
    printf("--/\n");
    return 0;
}
