#include <stdio.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char* argv[]) {
    int  i;
    char buf[4096];

    for (i=0; i<argc; i++) printf("%s/", argv[i]);
    printf("\n");

    while (fgets(buf, 4096, stdin)) {
        if (0 == strcmp("exit", buf)) {
            printf("Exit!!!!\n");
            return 1;
        }
        usleep(500000); /* 500ms */
        printf("%s", buf);
    }
    return 0;
}
