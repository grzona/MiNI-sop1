#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

void recursive_walker(const char *path, const char *extension, long long depth)
{
    if (depth < 1) return;

    DIR *dir = opendir(path);
    if (dir == NULL) 
    {
        ERR("RECURSIVE LIST OPEN DIR");
    }

    int dfd = dirfd(dir);
    struct dirent *entry;
    struct stat buf;

    while ((entry = readdir(dir)) != NULL)
    {
        // Pomijamy kropki bezpośrednio
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // AT_SYMLINK_NOFOLLOW zapobiega wpadnięciu w pętlę dowiązań symbolicznych
        if (fstatat(dfd, entry->d_name, &buf, AT_SYMLINK_NOFOLLOW) != 0)
            continue;

        if (S_ISREG(buf.st_mode))
        {
            if (extension != NULL)
            {
                char *dot = strrchr(entry->d_name, '.');
                if (!dot || strcmp(dot, extension) != 0)
                    continue;
            }

            // Przetwarzanie pliku...
            printf("File: %s/%s\n", path, entry->d_name);
        }
        else if (S_ISDIR(buf.st_mode))
        {
            char new_path[PATH_MAX];
            size_t path_len = strlen(path);
            int needs_slash = (path_len > 0 && path[path_len - 1] != '/');

            // Formatowanie ścieżki bez dynamicznej alokacji
            int res = snprintf(new_path, sizeof(new_path), "%s%s%s", 
                               path, needs_slash ? "/" : "", entry->d_name);

            if (res < 0 || res >= (int)sizeof(new_path))
            {
                closedir(dir);
                ERR("PATH TOO LONG");
            }

            // Wywołanie rekurencyjne
            recursive_walker(new_path, extension, depth - 1);
        }
    }

    closedir(dir);
}
