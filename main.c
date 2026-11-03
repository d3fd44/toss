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

#define TSEND       1
#define TRECV       2
#define OK          1

#define SEND_BUFFER 1024

uint64_t htonll(uint64_t x)
{
    uint16_t one = 1;
    uint8_t  first_byte;
    memcpy(&first_byte, &one, 1);

    if (first_byte == 1)  // check endianness
        return ((uint64_t)htonl((uint32_t)x) << 32) | htonl((uint32_t)(x >> 32));

    return x;
}

#define TO_NETWORK(x) _Generic((x), uint16_t: htons, uint32_t: htonl, uint64_t: htonll)(x)

// propably overkill. just wanna be cool lol.
#define ERRORS(T)                                                                \
    T(BAD_ARG, "bad argument.")                                                  \
    T(CONNECTION_FAIL, "destination unreachable.")                               \
    T(FILE_READ_FAIL, "error reading file.")                                     \
    T(FILE_STAT_READ_FAIL, "failed to get file stats.")                          \
    T(INVALID_ADDR, "invalid IPv4 address.")                                     \
    T(INVALID_HOST, "invalid port number.")                                      \
    T(MEM_ALLOC_FAIL, "memory allocation failed.")                               \
    T(NO_DIRECTION, "please specify a transmission direction (send / receive).") \
    T(SOCK_DESC_INIT_FAIL, "failed to initialize socket descriptor.")            \
    T(UNHANDLED_ERR, "wtf")                                                      \
    T(UNSUPPORTED_AF, "unsupported address family (IPv4 only).")

#define PRINT_USAGE()                               \
    printf("usage: toss <options> <path-to-file>\n" \
           "options:\n"                             \
           "    -s, --send    <destination-ip>\n"   \
           "    -r, --receive <bind-ip>\n"          \
           "    -p, --port    <port-number>\n"      \
           "    -h, --help\n\n")

#define HEADER_MEMBERS         \
    T(uint32_t, file_name_len) \
    T(uint64_t, file_size)

typedef struct {
#define T(type, name) type name;
    HEADER_MEMBERS
#undef T
} theader_t;  // tee-header

enum {
    THEADER_SIZE = 0
#define T(type, _) +sizeof(type)
    HEADER_MEMBERS
#undef T
};

typedef enum {
    NO_ERR,
#define T(err, errmsg) err,
    ERRORS(T)
#undef T
} terr_t;

const char *err_msg[] = {
#define T(err, errmsg) [err] = errmsg,
    ERRORS(T)
#undef T
};

typedef struct {
    bool               print_usage;
    uint8_t            mode;
    char              *file_path, *file_name;
    struct stat        file_stat;
    struct sockaddr_in dst_addr;
    terr_t             err;
} targs_t;

// my first argument parser, dam it feels kinda tricky. inspired by raysan's simple parser:
// https://github.com/raysan5/rfxgen/blob/3185a36277226243695169da1e0d8d4aedde6f50/src/rfxgen.c#L1125
// error handling works, but sucks. i mean it is good, but the reporting sucks
void process_args(int argc, char **argv, targs_t *targs)
{
    bool    print_usage = false;
    int     host = 12345;
    uint8_t mode = 0;

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
                    case -1:  // impossible, since af (arg[0]) is hardcoded.
                        targs->err = UNSUPPORTED_AF;
                        return;
                    case 0:
                        printf("failed to parse '%s'.\n", argv[i + 1]);
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
                targs->err = BAD_ARG;
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
                    case -1:  // same as above
                        targs->err = UNSUPPORTED_AF;
                        return;
                    case 0:
                        printf("failed to parse '%s'.\n", argv[i + 1]);
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
                targs->err = BAD_ARG;
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
                    printf("failed to parse '%s'.\n", argv[i + 1]);
                    targs->err = INVALID_HOST;
                    return;
                }

                host = (unsigned short)value;
                i++;
            }
            else
            {
                targs->err = INVALID_HOST;
                return;
            }
        }
        else if (argv[i][0] != '-')
        {
            if (stat(argv[i], &targs->file_stat))
            {
                targs->err = FILE_STAT_READ_FAIL;
                return;
            }
            targs->file_path = argv[i];
            targs->file_name = basename(targs->file_path);
        }
        else
        {
            printf("unknown argument: %s\n", argv[i]);
            targs->err = BAD_ARG;
            return;
        }
    }

    if (mode == 0)
    {
        targs->err = NO_DIRECTION;
        return;
    }

    targs->err = NO_ERR;
    targs->print_usage = print_usage;
    targs->dst_addr.sin_port = htons(host);
    targs->mode = mode;
}

uint8_t toss_header(int socket_fd, theader_t header)
{
    uint8_t buffer[THEADER_SIZE] = { 0 };
    size_t  i = 0;

    // header serialization
#define T(type, name)                               \
    type temp_##name = TO_NETWORK(header.name);     \
    memcpy(buffer + i, &temp_##name, sizeof(type)); \
    i += sizeof(type);
    HEADER_MEMBERS
#undef T

    if (send(socket_fd, buffer, THEADER_SIZE, 0) != THEADER_SIZE)
    {
        fprintf(stderr, "error sending header\n");
        return 1;
    }

    return 0;
}

uint8_t toss_file(int sd, char *filename, FILE *file, uint64_t size)
{
    assert(strlen(filename) == send(sd, filename, strlen(filename), 0));

    char buf[SEND_BUFFER] = { 0 };

    for (size_t i = 0; i < (size / _Countof(buf)) + 1; i++)
    {
        int read_count = fread(buf, 1, _Countof(buf), file);
        assert(read_count == send(sd, buf, read_count, 0));
    }
    return 0;
}

void toss_exit(uint8_t code)
{
    fprintf(stderr, "exit: %s\n", err_msg[code]);
    exit(1);
}

int main(int argc, char **argv)
{
    targs_t targs;
    FILE   *file = NULL;
    int     sd = -1;
    process_args(argc, argv, &targs);

    if (targs.err != NO_ERR)
        toss_exit(targs.err);

    if (targs.print_usage == true)
    {
        PRINT_USAGE();
        return 0;
    }

    char in_buf[INET_ADDRSTRLEN];

    // construct a message header
    theader_t file_header;

    file = fopen(targs.file_path, "r");
    if (!file)
        toss_exit(FILE_READ_FAIL);

    file_header.file_name_len = strlen(targs.file_name);
    file_header.file_size = targs.file_stat.st_size;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1)
    {
        fclose(file);
        toss_exit(SOCK_DESC_INIT_FAIL);
    }

    if (connect(sd, (struct sockaddr *)&targs.dst_addr, sizeof(targs.dst_addr)))
    {
        fclose(file);
        toss_exit(CONNECTION_FAIL);
    }

    printf("sending \"%s\" %zu bytes + %d bytes header + %zu bytes name_len (%ld bytes total)...\n", targs.file_path,
           targs.file_stat.st_size, THEADER_SIZE, strlen(targs.file_name),
           strlen(targs.file_name) + THEADER_SIZE + targs.file_stat.st_size);

    toss_header(sd, file_header);
    toss_file(sd, targs.file_name, file, targs.file_stat.st_size);

    printf("done.\n");
    fclose(file);
    close(sd);

    return 0;
}
