/*
 * 4_poll_fifo.c - SOP1 L2: poll na zestawie FIFO z callbackami
 *
 * Plik samodzielny. Na labie wklejasz go do swojego main.c (bez bloku DEMO).
 * Wklejanie kilku plików naraz jest bezpieczne: powtórzone include'y, ERR
 * i read_all/write_all są chronione przez #ifndef.
 * Demo:  gcc -Wall -Wextra -DDEMO -o 4_poll_fifo 4_poll_fifo.c
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
 *  4_poll_fifo.c: poll na zestawie FIFO z callbackami
 * ============================================================================
 *
 *  CO TO ROBI:
 *    Obserwuje do FIFO_ARRAY_SIZE łączy naraz. Każde łącze ma własny bufor.
 *    Funkcja sama skleja dane w pełne linie (zakończone '\n') i dla każdej
 *    woła twój callback. Ty piszesz tylko "co zrobić z linią".
 *
 *  UŻYCIE:
 *      struct fifo_set set;
 *      fifo_set_open(&set, paths, n);             // otwiera n łączy
 *      fifo_set_run(&set, 3000, on_line, on_eof, on_timeout, &moj_stan);
 *      // wraca, gdy wszystkie łącza dojdą do EOF (są już zamknięte)
 *
 *  CALLBACKI (on_eof i on_timeout mogą być NULL):
 *    on_line(idx, path, line, len, ctx)
 *        `line` to napis C BEZ '\n' (jest zakończony '\0'), ważny tylko
 *        w trakcie wywołania - jeśli chcesz go zachować, skopiuj.
 *    on_eof(idx, path, ctx)
 *        pisarz zamknął łącze; deskryptor jest już zamknięty.
 *    on_timeout(ctx)
 *        przez timeout_ms żadne łącze nic nie przysłało.
 *    `ctx` to dowolny wskaźnik przekazywany bez zmian - sposób na dostęp do
 *    własnych danych (np. deskryptora pliku wynikowego) bez zmiennych
 *    globalnych.
 *
 *  PRZYPADKI BRZEGOWE:
 *    - Linia dłuższa niż FIFO_LINE_MAX: on_line dostaje pierwsze
 *      FIFO_LINE_MAX bajtów, reszta do najbliższego '\n' jest pomijana.
 *    - Dane bez '\n' tuż przed EOF: on_line dostaje je jako ostatnią linię.
 *
 *  JAK DZIAŁA poll - ściąga:
 *      struct pollfd { int fd; short events; short revents; };
 *      int poll(struct pollfd* fds, nfds_t nfds, int timeout_ms);
 *    events  - ustawiasz TY: na co czekasz (POLLIN = dane do odczytu)
 *    revents - ustawia JĄDRO: co zaszło (maska bitowa, sprawdzaj przez &)
 *    timeout - w MILISEKUNDACH; -1 = czekaj bez końca; 0 = nie czekaj
 *    wynik   - >0: liczba gotowych deskryptorów; 0: timeout; -1: błąd
 *    fd < 0  - wpis jest ignorowany (tak "wyłączamy" zakończone łącze)
 *
 *    Flagi w revents:
 *      POLLIN    są dane
 *      POLLHUP   pisarz zamknął łącze (może przyjść SAMO, bez POLLIN!)
 *      POLLERR   błąd na deskryptorze
 *      POLLNVAL  deskryptor nieotwarty (close bez fd = -1) - błąd w programie
 *    POLLHUP/POLLERR/POLLNVAL jądro zgłasza zawsze, bez zamawiania w events.
 *
 *  PUŁAPKI (każda z nich kończy się zawieszeniem albo aktywnym czekaniem):
 *    1) Reagowanie tylko na POLLIN - po zamknięciu łącza przychodzi samo
 *       POLLHUP, nie robisz read, nie widzisz EOF, poll wraca w kółko.
 *       => read rób przy JAKIMKOLWIEK revents != 0.
 *    2) Czytanie w pętli "do końca" po poll - blokuje na jednym łączu.
 *       => JEDEN read na jedno zgłoszenie gotowości.
 *    3) Zamykanie na samo POLLHUP - w łączu mogą jeszcze być dane.
 *       => zamykaj dopiero, gdy read zwróci 0.
 *    4) close bez ustawienia fd = -1 => POLLNVAL w kółko.
 *    5) Jeden wspólny bufor na wszystkie łącza - fragmenty wiadomości
 *       z różnych łączy się wymieszają. => bufor i licznik PER ŁĄCZE,
 *       żyjące między wywołaniami poll.
 *    6) nfds to LICZBA wpisów w tablicy, nie "największy fd".
 */
#define FIFO_ARRAY_SIZE 5
#define FIFO_LINE_MAX 255

typedef void (*fifo_line_cb)(int idx, const char* path, const char* line, size_t len, void* ctx);
typedef void (*fifo_eof_cb)(int idx, const char* path, void* ctx);
typedef void (*fifo_timeout_cb)(void* ctx);

struct fifo_set
{
    int count;                                     // ile wpisów jest używanych
    int active;                                    // ile łączy jeszcze otwartych
    struct pollfd pfd[FIFO_ARRAY_SIZE];            // tablica dla poll
    const char* path[FIFO_ARRAY_SIZE];             // ścieżki (do komunikatów)
    char buf[FIFO_ARRAY_SIZE][FIFO_LINE_MAX + 1];  // niedokończona linia (+1 na '\0')
    size_t len[FIFO_ARRAY_SIZE];                   // ile bajtów w buforze
    int skip[FIFO_ARRAY_SIZE];                     // 1 = pomijamy resztę za długiej linii
};

/* Otwiera `count` łączy do odczytu (blokująco - czeka na pisarzy).
 * Zwraca 0, albo -(i+1), jeśli paths[i] nie istnieje / nie jest FIFO
 * (wtedy nic nie zostaje otwarte). */
int fifo_set_open(struct fifo_set* set, char** paths, int count)
{
    if (count < 1 || count > FIFO_ARRAY_SIZE)
    {
        fprintf(stderr, "fifo_set_open: count musi byc z zakresu 1..%d\n", FIFO_ARRAY_SIZE);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < count; ++i)  // najpierw sprawdzamy wszystkie
    {
        struct stat st;
        if (stat(paths[i], &st) == -1)
        {
            if (errno == ENOENT) return -(i + 1);
            ERR("stat");
        }
        if (!S_ISFIFO(st.st_mode)) return -(i + 1);
    }

    memset(set, 0, sizeof(*set));
    set->count = count;
    set->active = count;
    for (int i = 0; i < FIFO_ARRAY_SIZE; ++i) set->pfd[i].fd = -1;  // nieużywane wpisy poll zignoruje

    for (int i = 0; i < count; ++i)
    {
        set->path[i] = paths[i];
        set->pfd[i].fd = open(paths[i], O_RDONLY);
        if (set->pfd[i].fd == -1) ERR("open fifo");
        set->pfd[i].events = POLLIN;
    }
    return 0;
}

/* Wyciąga z bufora łącza i wszystkie pełne linie i oddaje je callbackowi. */
static void fifo_set_feed(struct fifo_set* set, int i, fifo_line_cb on_line, void* ctx)
{
    char* buf = set->buf[i];
    size_t start = 0;  // początek pierwszej nieobsłużonej linii
    char* nl;

    while ((nl = memchr(buf + start, '\n', set->len[i] - start)) != NULL)
    {
        if (set->skip[i])
            set->skip[i] = 0;  // to był ogon za długiej linii - koniec pomijania
        else
        {
            *nl = '\0';
            on_line(i, set->path[i], buf + start, nl - (buf + start), ctx);
        }
        start = (nl - buf) + 1;
    }

    // niedokończony fragment przesuwamy na początek bufora
    set->len[i] -= start;
    memmove(buf, buf + start, set->len[i]);

    // bufor pełny, a '\n' nie ma: linia za długa
    if (set->len[i] == FIFO_LINE_MAX)
    {
        if (!set->skip[i])
        {
            buf[FIFO_LINE_MAX] = '\0';
            on_line(i, set->path[i], buf, FIFO_LINE_MAX, ctx);
            set->skip[i] = 1;
        }
        set->len[i] = 0;
    }
}

/* Pętla główna: wraca, gdy wszystkie łącza dojdą do EOF. */
void fifo_set_run(struct fifo_set* set, int timeout_ms, fifo_line_cb on_line, fifo_eof_cb on_eof,
                  fifo_timeout_cb on_timeout, void* ctx)
{
    while (set->active > 0)
    {
        int ready = poll(set->pfd, set->count, timeout_ms);
        if (ready == -1)
        {
            if (errno == EINTR)  // przerwane sygnałem - to nie błąd
                continue;
            ERR("poll");
        }
        if (ready == 0)  // timeout
        {
            if (on_timeout) on_timeout(ctx);
            continue;
        }

        for (int i = 0; i < set->count; ++i)
        {
            short re = set->pfd[i].revents;
            if (set->pfd[i].fd < 0 || re == 0) continue;
            if (re & POLLNVAL) ERR("poll: POLLNVAL");

            // POLLIN, POLLHUP albo POLLERR: JEDEN read, jego wynik mówi resztę
            ssize_t r = read(set->pfd[i].fd, set->buf[i] + set->len[i], FIFO_LINE_MAX - set->len[i]);
            if (r == -1)
            {
                if (errno == EINTR || errno == EAGAIN) continue;
                ERR("read");
            }

            if (r == 0)  // EOF
            {
                if (set->len[i] > 0 && !set->skip[i])  // ostatnia linia bez '\n'
                {
                    set->buf[i][set->len[i]] = '\0';
                    on_line(i, set->path[i], set->buf[i], set->len[i], ctx);
                }
                if (close(set->pfd[i].fd) == -1) ERR("close");
                set->pfd[i].fd = -1;  // poll przestaje obserwować
                set->active--;
                if (on_eof) on_eof(i, set->path[i], ctx);
                continue;
            }

            set->len[i] += r;
            fifo_set_feed(set, i, on_line, ctx);
        }
    }
}

/* ---------------------------- DEMO (usuń na labie) ---------------------------- */
#ifdef DEMO
static void demo_on_line(int idx, const char* path, const char* line, size_t len, void* ctx)
{
    int* lines = ctx;  // ctx = wskaźnik na nasz licznik linii
    (*lines)++;
    printf("[%d:%s] (%zu B) %s\n", idx, path, len, line);
}

static void demo_on_eof(int idx, const char* path, void* ctx)
{
    (void)ctx;
    printf("[%d:%s] koniec\n", idx, path);
}

static void demo_on_timeout(void* ctx)
{
    (void)ctx;
    printf("cisza...\n");
}

int main(int argc, char** argv)
{
    // mkfifo a b && ./4_poll_fifo a b
    // w innych terminalach: cat > a   oraz   cat > b   (Ctrl+D kończy)
    int n = argc - 1;
    if (n < 1 || n > FIFO_ARRAY_SIZE)
    {
        fprintf(stderr, "USAGE: %s fifo1 [... fifo%d]\n", argv[0], FIFO_ARRAY_SIZE);
        return EXIT_FAILURE;
    }
    struct fifo_set set;
    int res = fifo_set_open(&set, argv + 1, n);
    if (res < 0)
    {
        fprintf(stderr, "%s nie jest FIFO\n", argv[-res]);
        return EXIT_FAILURE;
    }
    int lines = 0;
    fifo_set_run(&set, 3000, demo_on_line, demo_on_eof, demo_on_timeout, &lines);
    printf("razem linii: %d\n", lines);
    return EXIT_SUCCESS;
}
#endif /* DEMO */
