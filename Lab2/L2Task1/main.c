#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define LINE_SIZE 64
#define LINENO 32
#define RECIPE_SIZE 2048
#define TOWER_SIZE 5
#define GRANNY_TIMEOUT 3000
#define MSG_BUF 256  // babcia wysyła maks. 127 bajtów + '\n'

// zapis całego bufora (write może zapisać mniej, niż poprosiliśmy)
static void write_all(int fd, const char* data, size_t len)
{
    while (len > 0)
    {
        ssize_t w = write(fd, data, len);
        if (w == -1)
        {
            if (errno == EINTR)
                continue;
            ERR("WRITE");
        }
        data += w;
        len -= w;
    }
}

static void write_str(int fd, const char* s) { write_all(fd, s, strlen(s)); }

// otwiera istniejący przepis albo tworzy nowy (32 puste linie)
static int open_recipe(const char* path)
{
    int fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0666);
    if (fd >= 0)
    {  // plik nie istniał - wypełniamy go
        char empty_line[LINE_SIZE];
        memset(empty_line, ' ', LINE_SIZE - 1);
        empty_line[LINE_SIZE - 1] = '\n';
        for (int i = 0; i < LINENO; ++i)
            write_all(fd, empty_line, LINE_SIZE);
    }
    else if (errno == EEXIST)
    {  // plik istnieje
        fd = open(path, O_RDWR);
        if (fd == -1)
            ERR("FAILED TO OPEN FILE");
    }
    else
        ERR("CANNOT OPEN FILE");

    off_t fsize = lseek(fd, 0, SEEK_END);
    if (fsize == -1)
        ERR("LSEEK");
    if (fsize != RECIPE_SIZE)
    {
        write_str(2, "Ta ksiega to nie przepis!\n");
        exit(EXIT_FAILURE);
    }
    return fd;
}

// obsługa jednej pełnej wiadomości (bez '\n')
static void handle_message(int rfd, const char* msg, size_t msg_len)
{
    if (msg_len < 3 || !isdigit((unsigned char)msg[0]) || !isdigit((unsigned char)msg[1]) || msg[2] != ':' ||
        (msg[0] - '0') * 10 + (msg[1] - '0') >= LINENO)
    {
        write_str(2, "Babcia sie zagadala\n");
        return;
    }

    int msg_no = (msg[0] - '0') * 10 + (msg[1] - '0');

    char output[LINE_SIZE];
    size_t text_len = msg_len - 3;
    if (text_len > LINE_SIZE - 1)
        text_len = LINE_SIZE - 1;  // obcięcie do 63 znaków
    memcpy(output, msg + 3, text_len);
    memset(output + text_len, ' ', LINE_SIZE - 1 - text_len);
    output[LINE_SIZE - 1] = '\n';

    if (lseek(rfd, (off_t)LINE_SIZE * msg_no, SEEK_SET) == -1)
        ERR("LSEEK");
    write_all(rfd, output, LINE_SIZE);
}

int main(int argc, char** argv)
{
    if (argc < 3 || argc - 2 > TOWER_SIZE)
    {
        fprintf(stderr, "USAGE: %s recipe tower1 [tower2 ... tower%d]\n", argv[0], TOWER_SIZE);
        exit(EXIT_FAILURE);
    }

    int rfd = open_recipe(argv[1]);

    int towno = argc - 2;
    char* tower_path[TOWER_SIZE];
    struct pollfd poll_fds[TOWER_SIZE];

    // stan każdej wieży: bufor na niedokończoną wiadomość i liczba bajtów w nim
    char buffer[TOWER_SIZE][MSG_BUF];
    size_t buf_len[TOWER_SIZE] = {0};

    for (int i = 0; i < towno; ++i)
    {
        tower_path[i] = argv[i + 2];

        struct stat st;
        int exists = 1;
        if (stat(tower_path[i], &st) == -1)
        {
            if (errno != ENOENT)
                ERR("CANNOT SCAN A FIFO");
            exists = 0;
        }
        if (!exists || !S_ISFIFO(st.st_mode))
        {
            write_str(2, "W wiezy ");
            write_str(2, tower_path[i]);
            write_str(2, " nie ma babci\n");
            exit(EXIT_FAILURE);
        }

        poll_fds[i].fd = open(tower_path[i], O_RDONLY);
        if (poll_fds[i].fd == -1)
            ERR("CANNOT OPEN A FIFO");
        poll_fds[i].events = POLLIN;
    }

    int towers_left = towno;
    while (towers_left > 0)
    {
        int poll_ret = poll(poll_fds, towno, GRANNY_TIMEOUT);
        if (poll_ret == -1)
        {
            if (errno == EINTR)
                continue;
            ERR("POLL");
        }
        if (poll_ret == 0)
        {
            write_str(1, "Babcie dolewaja herbatki...\n");
            continue;
        }

        for (int t = 0; t < towno; ++t)
        {
            short re = poll_fds[t].revents;
            if (poll_fds[t].fd < 0 || re == 0)
                continue;
            if (re & POLLNVAL)
                ERR("POLLNVAL");

            // POLLIN, POLLHUP lub POLLERR - jeden read, resztę powie nam jego wynik
            char* buf = buffer[t];
            ssize_t r = read(poll_fds[t].fd, buf + buf_len[t], MSG_BUF - buf_len[t]);
            if (r == -1)
            {
                if (errno == EINTR)
                    continue;
                ERR("READ");
            }

            if (r == 0)
            {  // EOF - babcia skończyła
                if (close(poll_fds[t].fd) == -1)
                    ERR("FAILED TO CLOSE TFD");
                poll_fds[t].fd = -1;
                towers_left--;

                write_str(1, "Babcia z wiezy ");
                write_str(1, tower_path[t]);
                write_str(1, " przestala dyktowac\n");
                continue;
            }

            buf_len[t] += r;

            // przetwarzamy wszystkie pełne wiadomości w buforze tej wieży
            char* nl;
            while ((nl = memchr(buf, '\n', buf_len[t])) != NULL)
            {
                size_t msg_len = nl - buf;
                handle_message(rfd, buf, msg_len);

                size_t consumed = msg_len + 1;
                memmove(buf, buf + consumed, buf_len[t] - consumed);
                buf_len[t] -= consumed;
            }

            // bufor pełny, a końca wiadomości nie ma - wiadomość za długa, odrzucamy
            if (buf_len[t] == MSG_BUF)
            {
                write_str(2, "Babcia sie zagadala\n");
                buf_len[t] = 0;
            }
        }
    }

    char *recipe_mptr = mmap(NULL, RECIPE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, rfd, 0);
    if (recipe_mptr == NULL) ERR("MMAP");

    char *sptr, *movptr=recipe_mptr;
    int jcnt=0;

    while ((sptr = strstr(movptr, "raw~berries")) != NULL)
    {
        *(sptr+2) = 's';
        *(sptr+3) = 'p';
        movptr = sptr+10;
        jcnt++;
    }
    movptr=recipe_mptr;


    char empty_line[LINE_SIZE];
    memset(empty_line, ' ', LINE_SIZE - 1);
    empty_line[LINE_SIZE - 1] = '\n';

    int empty_cnt=0;
    while ((sptr = strstr(movptr, empty_line)) != NULL)
    {
        empty_cnt++;
        movptr= sptr + LINE_SIZE;
    }

    char msg[] = "Brudy księżniczki Joanny wyczyszczone: "

    munmap(recipe_mptr, RECIPE_SIZE);
    if (close(rfd) == -1)
        ERR("FAILED TO CLOSE RFD");

    return 0;
}
