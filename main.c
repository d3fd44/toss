#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define TSEND 0
#define TRECV 1
#define OK    1

// propably overkill. just wanna be cool lol.
#define ERRORS(T)                                                                                                                          \
    T(ALLOC_ERR, "memory allocation failed.")                                                                                              \
    T(BAD_ARGS_ERR, "bad arguments.")                                                                                                      \
    T(CONNECTION_FAIL, "destination unreachable.")                                                                                         \
    T(FS_ERR, "fs error: error reading file.")                                                                                             \
    T(INVALID_ADDR, "specified address contains characters representing a non-valid address in the specified address family")              \
    T(INVALID_HOST, "invalid port number.")                                                                                                \
    T(SOCK_DESC_ERR, "socket error: couldn't initialize file descriptor.")                                                                 \
    T(STAT_READ_ERR, "fs error: couldn't get file stats.")                                                                                 \
    T(UNHANDLED_ERR, "wtf")                                                                                                                \
    T(UNSUPPORTED_AF, "unsupported address family (IPv4 only).")

typedef enum
{
    NO_ERR,
#define T(err, errmsg) err,
    ERRORS(T)
#undef T
} terr_t;

typedef struct
{
    uint32_t tf_name_len;
    uint64_t tf_size;
} tf_header_t;

typedef struct
{
    bool               print_usage;
    bool               mode;
    char              *file_path, *file_name;
    struct stat        file_stat;
    struct sockaddr_in dst_addr;
    terr_t             err;
} targs_t;

static terr_t terr = -1;
const char   *err_msg[] = {
#define T(err, errmsg) [err] = errmsg,
    ERRORS(T)
#undef T
};

#define TFAIL(code)                                                                                                                        \
    do                                                                                                                                     \
    {                                                                                                                                      \
        terr = (code);                                                                                                                     \
        goto exit;                                                                                                                         \
    } while (0)

// my first argument parser, dam it feels kinda tricky. inspired by raysan's simple parser:
// https://github.com/raysan5/rfxgen/blob/3185a36277226243695169da1e0d8d4aedde6f50/src/rfxgen.c#L1125
// error handling works, but sucks. i mean it is good, but the reporting sucks
void process_args(int argc, char **argv, targs_t *targs)
{
    bool  print_usage = false;
    bool  mode;
    char *inet = { 0 };
    int   host = 12345;

    char *path = { 0 };
    char *file_name = { 0 };

    if (argc == 1)
        print_usage = true;

    memset(targs, 0, sizeof *targs);
    targs->dst_addr.sin_family = AF_INET;  // IPv4 only for now

    for (int i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0))
        {
            print_usage = true;
            break;
        }
        else if ((strcmp(argv[i], "-s") == 0) || (strcmp(argv[i], "--send") == 0))
        {
            mode = TSEND;
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                switch (inet_pton(AF_INET, argv[i + 1], &targs->dst_addr.sin_addr))
                {
                    case -1:
                        targs->err = UNSUPPORTED_AF;
                        return;
                    case 0:
                        targs->err = INVALID_ADDR;
                        return;
                    case OK:
                        break;
                    default:
                        targs->err = UNHANDLED_ERR;
                        return;
                }
                i++;
            }
            else
            {
                printf("error parsing address after \"-s | --send\"\n");
                targs->err = INVALID_ADDR;
                return;
            }
        }
        else if ((strcmp(argv[i], "-r") == 0) || (strcmp(argv[i], "--receive") == 0))
        {
            mode = TRECV;
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                switch (inet_pton(AF_INET, argv[i + 1], &targs->dst_addr.sin_addr))
                {
                    case -1:
                        targs->err = UNSUPPORTED_AF;
                        return;
                    case 0:
                        targs->err = INVALID_ADDR;
                        return;
                    case OK:
                        break;
                    default:
                        targs->err = UNHANDLED_ERR;
                        return;
                }
                i++;
            }
            else
            {
                printf("error parsing address after \"-r | --receive\"\n");
                targs->err = INVALID_ADDR;
                return;
            }
        }
        else if ((strcmp(argv[i], "-p") == 0) || (strcmp(argv[i], "--port") == 0))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                char *end = NULL;
                long  value = strtol(argv[i + 1], &end, 10);

                if (errno == ERANGE || end == argv[i + 1] || *end != '\0')
                {
                    targs->err = INVALID_HOST;
                    return;
                }

                host = (unsigned short)value;
                i++;
            }
            else
            {
                printf("error parsing port number after \"-p | --port\"\n");
                targs->err = INVALID_HOST;
                return;
            }
        }
        else if (argv[i][0] != '-')
        {
            if (stat(argv[i], &targs->file_stat))
            {
                printf("%s\n", argv[i]);
                targs->err = STAT_READ_ERR;
                return;
            }
            targs->file_path = argv[i];
            targs->file_name = basename(targs->file_path);
        }
        else
        {
            printf("unknown argument: %s\n", argv[i]);
            targs->err = BAD_ARGS_ERR;
            return;
        }
    }

    targs->err = NO_ERR;
    targs->print_usage = print_usage;
    targs->dst_addr.sin_port = htons(host);
    targs->mode = mode;
}

int main(int argc, char **argv)
{
    targs_t targs;
    process_args(argc, argv, &targs);

    if (targs.err != NO_ERR)
    {
        printf("usage: toss [<options>] <path-to-file>\n"
               "options:\n"
               "    -s, --send    <destination-ip>\n"
               "    -r, --receive <bind-ip>\n"
               "    -p, --port    <port-number>\n"
               "    -h, --help\n\n");
        TFAIL(targs.err);
    }

    if (targs.print_usage == true)
    {
        // repeated
        printf("usage: toss [<options>] <path-to-file>\n"
               "options:\n"
               "    -s, --send    <destination-ip>\n"
               "    -r, --receive <bind-ip>\n"
               "    -p, --port    <port-number>\n"
               "    -h, --help\n\n");
        return 0;
    }

    char in_buf[INET_ADDRSTRLEN];

    // construct a message header
    tf_header_t file_header;

    FILE *file = fopen(targs.file_path, "r");
    if (!file)
        TFAIL(FS_ERR);

    file_header.tf_name_len = strlen(targs.file_name);
    file_header.tf_size = targs.file_stat.st_size;

    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1)
        TFAIL(SOCK_DESC_ERR);

    if (connect(sd, (struct sockaddr *)&targs.dst_addr, sizeof(targs.dst_addr)))
    {
        perror("smth\n");
        TFAIL(CONNECTION_FAIL);
    }

    printf("sending \"%s\" (%zu Bytes)...\n", targs.file_path, file_header.tf_size);

    assert(sizeof(tf_header_t) == send(sd, &file_header, sizeof(tf_header_t), 0));
    assert(file_header.tf_name_len == send(sd, targs.file_name, file_header.tf_name_len, 0));

    char buf[1024] = { 0 };

    for (size_t i = 0; i < (targs.file_stat.st_size / _Countof(buf)) + 1; i++)
    {
        int read_count = fread(buf, 1, _Countof(buf), file);
        assert(read_count == send(sd, buf, read_count, 0));
    }

    printf("closing socket fd...\n");
    fclose(file);
    close(sd);
    printf("done.\n");
    return 0;

exit:
    // this segfaulted lol (i think it's bc we might jump here before even defining file or sd).
    // idk man, my error handling looks cool but feels suck :(
    // if (file)
    //     fclose(file);
    // if (sd > 0)
    //     close(sd);

    fprintf(stderr, "exit: %s\n", err_msg[terr]);
    exit(1);
}
