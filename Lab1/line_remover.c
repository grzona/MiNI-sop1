#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define ERR(source) (perror(source),\
fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),\
exit(EXIT_FAILURE))


int remove_line(char *path, char* text) // usuń linię zawierającą text
{
    size_t len = 0;
    ssize_t read;

    char *line= NULL;
    FILE *file = NULL;
    file = fopen(path, "r");
    if (file == NULL) ERR("Contains text");

    char *newpath = malloc(sizeof(char) * strlen(path) + 4);
    if (newpath == NULL) ERR("remove file");
    strcpy(newpath, path);
    strcat(newpath, "T");

    FILE *new_file = fopen(newpath, "a+");
    if (new_file == NULL) ERR("REMOVING FILE");

    while ((read = getline(&line, &len, file)) != -1)
    {
        if (line == NULL) ERR("CANNOT REWRITE");
        if (strstr(line, text) == NULL) // tej linii nie usuwamy
        {
            fprintf(new_file, "%s\n", line); // i przepisujemy do nowego pliku
        }else
        {
            // Co się ma dziać, jeśli usuniemy linie
        }
    }

    fclose(file);
    remove(path);
    rename(newpath, path);

    free(newpath);
    free(line);
    return 0;
}


int main(void) {
    printf("Hello, World!\n");
    return 0;
}
