#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>

#define ERR(source) (perror(source),\
fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),\
exit(EXIT_FAILURE))

int parse_file(char *path) // odczytaj plik linia po linii, sprawdź, czy zawiera text
{
    size_t len = 0;
    ssize_t read;

    char *line= NULL;
    FILE *file = NULL;
    file = fopen(path, "r");
    if (file == NULL) ERR("Contains text");

    const int useful_cnt = 3;
    char *useful[useful_cnt];
    for (int i=0; i<useful_cnt; ++i) useful[i] = NULL;
    char *keywords[useful_cnt];
    keywords[0] = "author:";
    keywords[1] = "title:";
    keywords[2] = "genre:";
    // MAKSYMALNIE useful_cnt KEYWORDÓW

    while ((read = getline(&line, &len, file)) != -1)
    {
        if (line == NULL) ERR("READ LINEBYLINE");
        for (int i=0; i<useful_cnt; ++i)
        {
            char *lineptr = NULL;
            if (strlen(line) <= strlen(keywords[i])) continue; // na bank nie ma
            lineptr = strstr(line, keywords[i]);
            // logika sprawdza, czy klucz jest na początku lub czy jest poprzedzony spacją
            if (lineptr != NULL&& (lineptr == line || (lineptr > line && *(lineptr-1) == ' '))) // znaleźliśmy useful[i];
            {
                if (useful[i])
                {
                    ERR("PARSER: NIEJEDNOZNACZNOŚĆ KLUCZY!!!");
                    free(useful[i]);
                }
                useful[i] = malloc(sizeof(char) * (strlen(line) - strlen(keywords[i]) + 4));
                if (useful[i] == NULL) ERR("READING: MALLOC");
                strncpy(useful[i], lineptr+strlen(keywords[i]), strlen(line)-strlen(keywords[i]));
            }

        }
    }
    for (int i=0; i<useful_cnt; ++i)
    {
        if (useful[i] == NULL)
        {
            free(useful[i]);
            useful[i] = malloc(sizeof(char) * 12);
            if (useful[i] == NULL) ERR("READING: MALLOC");
            strcpy(useful[i], "missing!\n");
        }

        fprintf(stdout, "%s %s", keywords[i], useful[i]);
    }

    fclose(file);
    for (int i=0; i<useful_cnt; ++i) free(useful[i]);
    free(line);
    return 0;
}
