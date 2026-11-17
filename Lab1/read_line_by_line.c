#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define ERR(source) (perror(source),\
fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),\
exit(EXIT_FAILURE))


int read_line_by_line(char *path, char* text) // odczytaj plik linia po linii, sprawdź, czy zawiera text
{
    size_t len = 0;
    ssize_t read;

    char *line= NULL;
    FILE *file = NULL;
    file = fopen(path, "r");
    if (file == NULL) ERR("Contains text");


    while ((read = getline(&line, &len, file)) != -1)
    {
        if (line == NULL) ERR("READ LINEBYLINE");
        if (strstr(line, text) != NULL)
        {
            // Co się ma dziać, gdy znajdziemy linie
        }
    }

    fclose(file);

    free(line);
    return 0;
}


int main(void) {
    printf("Hello, World!\n");
    return 0;
}
