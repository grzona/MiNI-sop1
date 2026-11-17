/*
 * 5_mmap_file.c - SOP1 L2: mmap - plik jako tablica bajtów
 *
 * Plik samodzielny. Na labie wklejasz go do swojego main.c (bez bloku DEMO).
 * Wklejanie kilku plików naraz jest bezpieczne: powtórzone include'y, ERR
 * i read_all/write_all są chronione przez #ifndef.
 * Demo:  gcc -Wall -Wextra -DDEMO -o 5_mmap_file 5_mmap_file.c
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#ifndef ERR
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#endif

/* ============================================================================
 *  5_mmap_file.c: mmap - plik jako tablica bajtów
 * ============================================================================
 *
 *  map_file(path, &size, writable)
 *      Otwiera plik, mapuje CAŁY i zamyka deskryptor (mapa działa dalej).
 *      writable == 1: zmiany w tablicy trafiają do pliku (MAP_SHARED).
 *      writable == 0: tylko odczyt; zapis do tablicy = SIGSEGV.
 *  unmap_file(p, size)
 *      msync (wymuszenie zapisu na dysk) + munmap.
 *
 *  Jeśli masz już otwarty deskryptor (np. z O_RDWR), mapuj go bezpośrednio:
 *      char* p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
 *      if (p == MAP_FAILED) ERR("mmap");
 *
 *  ARGUMENTY mmap(addr, length, prot, flags, fd, offset):
 *      addr    NULL - adres wybiera jądro
 *      length  ile bajtów (> 0; pusty plik => EINVAL)
 *      prot    PROT_READ | PROT_WRITE - musi pasować do trybu open
 *              (PROT_WRITE + MAP_SHARED wymaga O_RDWR, nie O_WRONLY)
 *      flags   MAP_SHARED  - zapis do tablicy zmienia plik
 *              MAP_PRIVATE - prywatna kopia, plik bez zmian
 *      offset  od którego bajtu pliku; wielokrotność rozmiaru strony (zwykle 0)
 *
 *  PUŁAPKI:
 *    1) Błąd to MAP_FAILED, czyli (void*)-1, a NIE NULL.
 *    2) MAP_PRIVATE zamiast MAP_SHARED: program "działa", plik bez zmian.
 *    3) Mapa nie kończy się '\0': strlen/strstr/printf("%s") wyjdą poza nią.
 *       Używaj memcmp/memchr/memmem z jawną długością, a do wypisania
 *       printf("%.*s", (int)len, ptr) lub write(1, ptr, len).
 *    4) Rozmiar jest stały - przez mapę nie da się wydłużyć pliku. Żeby
 *       zmapować większy obszar, najpierw ftruncate(fd, nowy_rozmiar).
 *    5) Nie wpisuj '\0' do mapy "żeby łatwiej wypisać" - zmienisz plik.
 *    6) munmap dostaje ten sam wskaźnik i długość co mmap.
 *
 *  Pozycja kursora pliku (lseek) nie ma dla mmap znaczenia.
 */
char* map_file(const char* path, size_t* size, int writable)
{
    int fd = open(path, writable ? O_RDWR : O_RDONLY);
    if (fd == -1) ERR("open");

    struct stat st;
    if (fstat(fd, &st) == -1) ERR("fstat");
    if (st.st_size == 0)
    {
        fprintf(stderr, "map_file: %s jest pusty\n", path);
        exit(EXIT_FAILURE);
    }
    *size = st.st_size;

    char* p = mmap(NULL, *size, PROT_READ | (writable ? PROT_WRITE : 0), MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) ERR("mmap");
    if (close(fd) == -1) ERR("close");
    return p;
}

void unmap_file(char* p, size_t size)
{
    if (msync(p, size, MS_SYNC) == -1) ERR("msync");
    if (munmap(p, size) == -1) ERR("munmap");
}

/* Przykład pracy na mapie: zamiana wszystkich wystąpień `from` na `to`
 * (napisy RÓWNEJ długości - tylko wtedy da się to zrobić w miejscu).
 * Zwraca liczbę zamian. */
int replace_in_place(char* p, size_t size, const char* from, const char* to)
{
    size_t n = strlen(from);
    if (n == 0 || n != strlen(to)) return -1;

    int count = 0;
    size_t i = 0;
    while (i + n <= size)  // ostatnia pozycja startowa: size - n
    {
        if (memcmp(p + i, from, n) == 0)
        {
            memcpy(p + i, to, n);
            count++;
            i += n;
        }
        else
            i++;
    }
    return count;
}

/* ---------------------------- DEMO (usuń na labie) ---------------------------- */
#ifdef DEMO
int main(int argc, char** argv)
{
    // ./5_mmap_file plik stare nowe   (napisy równej długości)
    if (argc != 4)
    {
        fprintf(stderr, "USAGE: %s plik z na\n", argv[0]);
        return EXIT_FAILURE;
    }
    size_t size;
    char* p = map_file(argv[1], &size, 1);
    int n = replace_in_place(p, size, argv[2], argv[3]);
    if (n < 0)
        fprintf(stderr, "napisy musza miec rowna, niezerowa dlugosc\n");
    else
        printf("zamian: %d\n", n);

    // pierwsza linia bez końcowych spacji: liczymy długość, nie wstawiamy '\0'
    char* nl = memchr(p, '\n', size);
    size_t len = nl ? (size_t)(nl - p) : size;
    while (len > 0 && p[len - 1] == ' ') len--;
    printf("pierwsza linia: \"%.*s\"\n", (int)len, p);

    unmap_file(p, size);
    return EXIT_SUCCESS;
}
#endif /* DEMO */
