#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define INVALID_AF -1
#define BAD_ADDR   0

// propably overkill. just wanna be cool lol.
#define ERRORS(T)                                                                                                                          \
    T(SOCK_DESC_ERR, "socket error: couldn't initialize file descriptor.")                                                                 \
    T(CONNECTION_FAIL, "destination unreachable.")                                                                                         \
    T(FS_ERR, "fs error: error reading file.")                                                                                             \
    T(STAT_READ_ERR, "fs error: couldn't get file stats.")                                                                                 \
    T(ALLOC_ERR, "memory allocation failed.")                                                                                              \
    T(BAD_ARGS_ERR, "bad arguments.")                                                                                                      \
    T(DST_ADD_ERR, "specified address contains characters representing a non-valid address in the specified address family")               \
    T(UNHANDLED_ERR, "wtf")

typedef enum
{
#define T(err, errmsg) err,
    ERRORS(T)
#undef T
      NO_ERR = -1
} terr_t;

typedef struct
{
    uint32_t tf_name_len;
    uint64_t tf_size;
} tf_header_t;

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

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("usage: toss <dest-ip> <file>\n");
        TFAIL(BAD_ARGS_ERR);
    }

    struct stat file_stat;
    tf_header_t file_header;

    if (stat(argv[2], &file_stat))
        TFAIL(STAT_READ_ERR);

    FILE *file = fopen(argv[2], "r");
    if (!file)
        TFAIL(FS_ERR);

    file_header.tf_name_len = strlen(argv[2]);
    file_header.tf_size = file_stat.st_size;

    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1)
        TFAIL(SOCK_DESC_ERR);

    struct sockaddr_in dst_addr = { .sin_family = AF_INET, .sin_port = htons(12345) };

    switch (inet_pton(AF_INET, "127.0.0.1", &dst_addr.sin_addr))
    {
        case INVALID_AF:  // returned when `af` (arg 1) isn't a valid address family (skip for now)
            break;
        case BAD_ADDR:
            TFAIL(DST_ADD_ERR);
        case 1:
            break;
        default:
            TFAIL(UNHANDLED_ERR);
    }

    if (connect(sd, (struct sockaddr *)&dst_addr, sizeof(dst_addr)))
        TFAIL(CONNECTION_FAIL);

    printf("sending \"%s\" (%zu Bytes)...\n", argv[2], file_header.tf_size);

    assert(sizeof(tf_header_t) == send(sd, &file_header, sizeof(tf_header_t), 0));
    assert(file_header.tf_name_len == send(sd, argv[2], file_header.tf_name_len, 0));

    char buf[1024] = { 0 };

    for (size_t i = 0; i < (file_stat.st_size / _Countof(buf)) + 1; i++)
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
    if (sd > 0)
        close(sd);
    if (file)
        fclose(file);

    fprintf(stderr, "exit: %s\n", err_msg[terr]);
    exit(1);
}
