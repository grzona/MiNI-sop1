#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#define ERR(source) (perror(source),\
fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),\
exit(EXIT_FAILURE))


int
main(int argc, char *argv[])
{

    char ** arguments;
    arguments = malloc(argc * sizeof(char *));
    if (arguments == NULL) ERR("failed to allocate arguments");
    int cnt = 0;
    int some_int=0;
    long long some_ll=0;
    int opt;

    while ((opt = getopt(argc, argv, "n:t:")) != -1) {
        switch (opt) {
            case 'n': // jeśli po n nastąpić string
                arguments[cnt] = malloc(strlen(optarg) + 1);
                if (arguments == NULL) ERR("failed to allocate arguments[i]");
                strcpy(arguments[cnt], optarg);
                cnt++;
                break;
            case 't': // jeśli trzeba usstawić inta na konkretną wartość
                errno=0;
                some_int = atoi(optarg);
                some_ll = strtol(optarg, NULL, 10);
                if (errno != 0) ERR("failed to convert number to int");
                break;
            default: /* '?' */
                ERR("OPTION ERORR");
        }
    }

    for (int i = 0; i < cnt; i++)
    {
        /* TUTAJ COMMITUJEMY KOD, CO ZROBIĆ Z POSZCZEGÓLNYMI arguments[i]] */
        free(arguments[i]);
    }
    free(arguments);


    /* Other code omitted */

    exit(EXIT_SUCCESS);
}
