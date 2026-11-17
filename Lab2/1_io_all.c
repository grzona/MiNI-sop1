/*
 * 1_io_all.c - SOP1 L2: read_all / write_all - pewny odczyt i zapis n bajtów
 *
 * Plik samodzielny. Na labie wklejasz go do swojego main.c (bez bloku DEMO).
 * Wklejanie kilku plików naraz jest bezpieczne: powtórzone include'y, ERR
 * i read_all/write_all są chronione przez #ifndef.
 * Demo:  gcc -Wall -Wextra -DDEMO -o 1_io_all 1_io_all.c
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
 *  1_io_all.c: read_all / write_all
 * ============================================================================
 *
 *  PROBLEM: read(fd, buf, n) i write(fd, buf, n) mogą wrócić wcześniej:
 *    - read z potoku/FIFO oddaje tyle, ile akurat jest (np. pół wiadomości),
 *    - write do pełnego potoku zapisuje część,
 *    - oba mogą zostać przerwane sygnałem (-1, errno == EINTR).
 *
 *  read_all  - czyta, aż zbierze `count` bajtów ALBO trafi na EOF.
 *              Zwraca liczbę przeczytanych bajtów (< count oznacza EOF).
 *  write_all - pisze, aż zapisze wszystkie `count` bajtów.
 *
 *  KIEDY UŻYWAĆ:
 *    + zwykłe pliki, rekordy o stałym rozmiarze, "wczytaj wszystko do EOF"
 *    - NIE używaj read_all na deskryptorze, który właśnie zgłosił poll():
 *      read_all zablokuje się w oczekiwaniu na resztę danych i inne
 *      deskryptory przestaną być obsługiwane. Po poll robisz JEDEN read.
 */
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

/* ---------------------------- DEMO (usuń na labie) ---------------------------- */
#ifdef DEMO
int main(void)
{
    // kopiuje stdin na stdout: ./1_io_all < plik
    char buf[4096];
    ssize_t n;
    while ((n = read_all(0, buf, sizeof(buf))) > 0) write_all(1, buf, n);
    write_str(2, "koniec danych\n");
    return EXIT_SUCCESS;
}
#endif /* DEMO */
