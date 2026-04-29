#include <stdio.h>
#include <stdlib.h>

static int has_crlf(FILE *fp) {
    int c;
    int prev = 0;

    while ((c = fgetc(fp)) != EOF) {
        if (prev == '\r' && c == '\n') {
            return 1;
        }
        prev = c;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        return 2;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    int result = has_crlf(fp);
    fclose(fp);

    if (result) {
        printf("CRLF detected: %s\n", argv[1]);
        return 0;
    }

    printf("LF only: %s\n", argv[1]);
    return 0;
}
