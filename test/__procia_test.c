#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


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
            printf("exit\n");
            perror("===>__procia_test EXIT\n");
            return 1;
        }
        usleep(500000); /* 500ms */
        printf("%s", buf);
    }
    perror("===>__procia_test RETURN\n");
    return 0;
}
