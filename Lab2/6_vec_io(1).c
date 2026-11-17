/*
 * 6_vec_io.c - SOP1 L2: readv / writev - wektorowe I/O
 *
 * Plik samodzielny. Na labie wklejasz go do swojego main.c (bez bloku DEMO).
 * Wklejanie kilku plików naraz jest bezpieczne: powtórzone include'y, ERR
 * i read_all/write_all są chronione przez #ifndef.
 * Demo:  gcc -Wall -Wextra -DDEMO -o 6_vec_io 6_vec_io.c
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
 *  6_vec_io.c: readv / writev - wektorowe I/O
 * ============================================================================
 *
 *  Jedno wywołanie systemowe obsługuje KILKA buforów naraz:
 *      struct iovec { void* iov_base; size_t iov_len; };
 *      ssize_t writev(int fd, const struct iovec* iov, int iovcnt);
 *      ssize_t readv (int fd, const struct iovec* iov, int iovcnt);
 *
 *  writev - "zbieranie": wysyła bufory po kolei, jakby były jednym ciągiem.
 *      Po co: składasz wiadomość z kawałków (nagłówek + treść + '\n') bez
 *      kopiowania do wspólnego bufora, a całość idzie jednym wywołaniem.
 *      Zapis do potoku/FIFO o łącznym rozmiarze <= PIPE_BUF (4096 na
 *      Linuksie) jest atomowy - nie przemiesza się z zapisami innych procesów.
 *  readv - "rozrzucanie": wypełnia bufory po kolei; następny dostaje dane
 *      dopiero, gdy poprzedni jest pełny.
 *      Po co: rekord o stałym układzie (np. 8 bajtów nagłówka + 56 treści)
 *      trafia od razu do osobnych zmiennych.
 *
 *  Wartość zwracana: ŁĄCZNA liczba bajtów (może być mniejsza od sumy
 *  długości - tak samo jak przy read/write), 0 = EOF (readv), -1 = błąd.
 */
/* Zapisuje "prefix" + "text" + '\n' jednym wywołaniem. Zwraca liczbę bajtów. */
ssize_t write_line_v(int fd, const char* prefix, const char* text)
{
    char nl = '\n';
    struct iovec iov[3];
    iov[0].iov_base = (void*)prefix;
    iov[0].iov_len = strlen(prefix);
    iov[1].iov_base = (void*)text;
    iov[1].iov_len = strlen(text);
    iov[2].iov_base = &nl;
    iov[2].iov_len = 1;

    ssize_t w;
    while ((w = writev(fd, iov, 3)) == -1)
    {
        if (errno != EINTR) ERR("writev");
    }
    return w;
}

/* Czyta jeden rekord złożony z dwóch pól o stałych rozmiarach.
 * Zwraca łączną liczbę bajtów (a_len + b_len = cały rekord, 0 = EOF). */
ssize_t read_record_v(int fd, char* a, size_t a_len, char* b, size_t b_len)
{
    struct iovec iov[2];
    iov[0].iov_base = a;
    iov[0].iov_len = a_len;
    iov[1].iov_base = b;
    iov[1].iov_len = b_len;

    ssize_t r;
    while ((r = readv(fd, iov, 2)) == -1)
    {
        if (errno != EINTR) ERR("readv");
    }
    return r;
}

/* ---------------------------- DEMO (usuń na labie) ---------------------------- */
#ifdef DEMO
int main(int argc, char** argv)
{
    // ./6_vec_io rekordy.bin
    if (argc != 2)
    {
        fprintf(stderr, "USAGE: %s plik\n", argv[0]);
        return EXIT_FAILURE;
    }
    int fd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) ERR("open");
    write_line_v(fd, "HDR:", "tresc pierwsza");  // 4 + 14 + 1 bajtów
    write_line_v(fd, "HDR:", "tresc druga...");

    if (lseek(fd, 0, SEEK_SET) == -1)  // wracamy na początek, żeby czytać
        ERR("lseek");

    char hdr[4], body[15];
    ssize_t r;
    while ((r = read_record_v(fd, hdr, sizeof(hdr), body, sizeof(body))) > 0)
        printf("rekord %zd B: hdr=\"%.4s\" body=\"%.14s\"\n", r, hdr, body);

    if (close(fd) == -1) ERR("close");
    return EXIT_SUCCESS;
}
#endif /* DEMO */
