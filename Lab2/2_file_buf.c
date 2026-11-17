/*
 * 2_file_buf.c - SOP1 L2: cały plik <-> bufor
 *
 * Plik samodzielny. Na labie wklejasz go do swojego main.c (bez bloku DEMO).
 * Wklejanie kilku plików naraz jest bezpieczne: powtórzone include'y, ERR
 * i read_all/write_all są chronione przez #ifndef.
 * Demo:  gcc -Wall -Wextra -DDEMO -o 2_file_buf 2_file_buf.c
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

/* Kopia read_all/write_all z 1_io_all.c (opis tam). */
#ifndef SOP_IO_ALL
#define SOP_IO_ALL

ssize_t read_all(int fd, char* buf, size_t count)
{
    size_t total = 0;
    while (total < count)
    {
        ssize_t r = read(fd, buf + total, count - total);
        if (r == -1)
        {
            if (errno == EINTR) continue;
            ERR("read");
        }
        if (r == 0)  // EOF
            break;
        total += r;
    }
    return total;
}

void write_all(int fd, const char* buf, size_t count)
{
    size_t total = 0;
    while (total < count)
    {
        ssize_t w = write(fd, buf + total, count - total);
        if (w == -1)
        {
            if (errno == EINTR) continue;
            ERR("write");
        }
        total += w;
    }
}

/* Wygodny zapis napisu C (długość z strlen - tylko dla literałów i napisów
 * zakończonych '\0', nie dla bufora po read!). */
void write_str(int fd, const char* s) { write_all(fd, s, strlen(s)); }

#endif /* SOP_IO_ALL */

/* ============================================================================
 *  2_file_buf.c: cały plik <-> bufor
 * ============================================================================
 *
 *  read_file(path, buf, cap)
 *      Czyta plik do TWOJEGO bufora o pojemności `cap`.
 *      Zwraca liczbę bajtów; -1 gdy plik nie istnieje (errno == ENOENT).
 *      Jeśli plik jest większy niż cap, dostajesz pierwsze `cap` bajtów
 *      (wynik == cap => możliwe, że plik został obcięty).
 *      Bufor NIE jest zakończony '\0'. Jeśli chcesz napis:
 *          char buf[N + 1];
 *          ssize_t n = read_file(path, buf, N);
 *          buf[n] = '\0';
 *
 *  read_file_alloc(path, &size)
 *      Sam sprawdza rozmiar (fstat) i alokuje bufor malloc'iem.
 *      Dopisuje '\0' za danymi (dla wygody; nie wlicza się do size).
 *      Zwraca NULL gdy plik nie istnieje. Po użyciu: free().
 *      Działa dla zwykłych plików (dla FIFO st_size == 0 - tam czytaj w pętli).
 *
 *  write_file(path, buf, len)
 *      Tworzy plik lub nadpisuje istniejący zawartością bufora.
 *
 *  append_file(path, buf, len)
 *      Dopisuje na koniec (O_APPEND - każdy write idzie na koniec pliku).
 *
 *  FLAGI open - ściąga:
 *      O_RDONLY / O_WRONLY / O_RDWR   tryb dostępu (dokładnie jeden!)
 *      O_CREAT    utwórz, jeśli nie ma -> WYMAGA trzeciego argumentu (0644)
 *      O_EXCL     razem z O_CREAT: błąd EEXIST, jeśli plik już jest
 *      O_TRUNC    wyzeruj istniejący plik
 *      O_APPEND   zapisy zawsze na koniec
 *  Wzorzec "utwórz albo otwórz istniejący":
 *      fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0644);
 *      if (fd >= 0)             { ...nowy plik, wypełnij...  }
 *      else if (errno == EEXIST){ fd = open(path, O_RDWR);   }
 *      else ERR("open");
 *
 *  lseek(fd, off, SEEK_SET/SEEK_CUR/SEEK_END) - przesuwa kursor, zwraca nową
 *  pozycję. lseek(fd, 0, SEEK_END) = rozmiar pliku. Na FIFO/terminalu daje
 *  ESPIPE ("Illegal seek"). Zapis po lseek NADPISUJE bajty, nie wstawia.
 */
ssize_t read_file(const char* path, char* buf, size_t cap)
{
    int fd = open(path, O_RDONLY);
    if (fd == -1)
    {
        if (errno == ENOENT) return -1;
        ERR("open");
    }
    ssize_t n = read_all(fd, buf, cap);
    if (close(fd) == -1) ERR("close");
    return n;
}

char* read_file_alloc(const char* path, size_t* size)
{
    int fd = open(path, O_RDONLY);
    if (fd == -1)
    {
        if (errno == ENOENT) return NULL;
        ERR("open");
    }
    struct stat st;
    if (fstat(fd, &st) == -1) ERR("fstat");

    char* buf = malloc(st.st_size + 1);
    if (!buf) ERR("malloc");
    *size = read_all(fd, buf, st.st_size);
    buf[*size] = '\0';

    if (close(fd) == -1) ERR("close");
    return buf;
}

void write_file(const char* path, const char* buf, size_t len)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) ERR("open");
    write_all(fd, buf, len);
    if (close(fd) == -1) ERR("close");
}

void append_file(const char* path, const char* buf, size_t len)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) ERR("open");
    write_all(fd, buf, len);
    if (close(fd) == -1) ERR("close");
}

/* ---------------------------- DEMO (usuń na labie) ---------------------------- */
#ifdef DEMO
int main(int argc, char** argv)
{
    // ./2_file_buf plik  ->  kopia w kopia.out z dopisaną linią
    if (argc != 2)
    {
        fprintf(stderr, "USAGE: %s plik\n", argv[0]);
        return EXIT_FAILURE;
    }
    size_t size;
    char* data = read_file_alloc(argv[1], &size);
    if (!data)
    {
        fprintf(stderr, "%s nie istnieje\n", argv[1]);
        return EXIT_FAILURE;
    }
    printf("przeczytano %zu bajtow\n", size);
    write_file("kopia.out", data, size);
    append_file("kopia.out", "-- koniec kopii --\n", 19);
    free(data);

    char small[16];
    ssize_t n = read_file(argv[1], small, sizeof(small));
    printf("pierwsze %zd bajtow: \"%.*s\"\n", n, (int)n, small);
    return EXIT_SUCCESS;
}
#endif /* DEMO */
