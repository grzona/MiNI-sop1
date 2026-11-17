/*
 * 3_fifo.c - SOP1 L2: FIFO (łącza nazwane)
 *
 * Plik samodzielny. Na labie wklejasz go do swojego main.c (bez bloku DEMO).
 * Wklejanie kilku plików naraz jest bezpieczne: powtórzone include'y, ERR
 * i read_all/write_all są chronione przez #ifndef.
 * Demo:  gcc -Wall -Wextra -DDEMO -o 3_fifo 3_fifo.c
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
 *  3_fifo.c: FIFO (łącza nazwane)
 * ============================================================================
 *
 *  FIFO to plik specjalny: dane wpisane jednym końcem wychodzą drugim,
 *  nie są zapisywane na dysku, nie da się po nich lseek'ować.
 *
 *  is_fifo(path)       1 - istnieje i jest FIFO; 0 - nie istnieje lub inny typ
 *  make_fifo(path)     mkfifo; istniejące FIFO nie jest błędem
 *  open_fifo_read(path, nonblock)
 *  set_nonblock(fd)    włącza O_NONBLOCK na otwartym deskryptorze
 *
 *  ZACHOWANIE open na FIFO:
 *    open(O_RDONLY)              BLOKUJE, aż ktoś otworzy do zapisu
 *    open(O_WRONLY)              BLOKUJE, aż ktoś otworzy do odczytu
 *    open(O_RDONLY | O_NONBLOCK) wraca od razu
 *    open(O_WRONLY | O_NONBLOCK) bez czytelnika: błąd ENXIO
 *  Wniosek: otwierając kilka FIFO blokująco po kolei, czekasz na pisarza
 *  każdego z nich. Jeśli któregoś pisarza nie uruchomisz - program wisi
 *  w open (to nie błąd w kodzie).
 *
 *  ZACHOWANIE read na FIFO:
 *    są dane                     zwraca to, co jest (może być mniej niż chcesz)
 *    brak danych, jest pisarz    blokuje (z O_NONBLOCK: -1, errno == EAGAIN)
 *    brak danych, brak pisarzy   0 = EOF
 *
 *  ZAPIS do FIFO bez czytelnika: sygnał SIGPIPE (domyślnie zabija proces)
 *  lub - gdy SIGPIPE ignorowany - write zwraca -1 z errno == EPIPE.
 *
 *  fcntl: flagi zmieniamy ZAWSZE wzorcem "pobierz -> dodaj -> ustaw".
 *  Samo fcntl(fd, F_SETFL, O_NONBLOCK) skasowałoby pozostałe flagi.
 *
 *  Sprzątanie: unlink(path) usuwa FIFO z dysku.
 */
int is_fifo(const char* path)
{
    struct stat st;
    if (stat(path, &st) == -1)
    {
        if (errno == ENOENT) return 0;
        ERR("stat");
    }
    return S_ISFIFO(st.st_mode) ? 1 : 0;
    /* Inne makra typu: S_ISREG (zwykły plik), S_ISDIR (katalog),
     * S_ISLNK (dowiązanie - wymaga lstat), S_ISCHR, S_ISBLK, S_ISSOCK.
     * Mając już deskryptor, użyj fstat(fd, &st) zamiast stat(path, &st). */
}

void make_fifo(const char* path)
{
    if (mkfifo(path, 0666) == -1 && errno != EEXIST) ERR("mkfifo");
}

void set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) ERR("fcntl F_GETFL");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) ERR("fcntl F_SETFL");
}

int open_fifo_read(const char* path, int nonblock)
{
    int fd = open(path, O_RDONLY | (nonblock ? O_NONBLOCK : 0));
    if (fd == -1) ERR("open fifo");
    return fd;
}

/* ---------------------------- DEMO (usuń na labie) ---------------------------- */
#ifdef DEMO
int main(int argc, char** argv)
{
    // ./3_fifo sciezka  ->  tworzy FIFO (jeśli trzeba) i wypisuje wszystko, co przyjdzie
    // w drugim terminalu: echo test > sciezka
    if (argc != 2)
    {
        fprintf(stderr, "USAGE: %s fifo\n", argv[0]);
        return EXIT_FAILURE;
    }
    make_fifo(argv[1]);
    if (!is_fifo(argv[1]))
    {
        fprintf(stderr, "%s istnieje, ale nie jest FIFO\n", argv[1]);
        return EXIT_FAILURE;
    }
    int fd = open_fifo_read(argv[1], 0);  // blokuje do pojawienia się pisarza
    char buf[256];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        if (write(1, buf, n) != n) ERR("write");
    if (n == -1) ERR("read");
    if (close(fd) == -1) ERR("close");
    if (unlink(argv[1]) == -1) ERR("unlink");
    return EXIT_SUCCESS;
}
#endif /* DEMO */
