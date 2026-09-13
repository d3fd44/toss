#include <arpa/inet.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

// propably overkill. just wanna be cool lol.
#define ERRORS(T)                                                                                                                          \
    T(SOCK_DESC_ERR, "socket error: couldn't initialize file descriptor.")                                                                 \
    T(CONNECTION_FAIL, "couldn't connect to the destination")                                                                              \
    T(FS_ERR, "fs error: error reading file.")                                                                                             \
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


    FILE *file = fopen(argv[2], "r");
    if (!file)
        TFAIL(FS_ERR);

    fseek(file, 0, SEEK_END);  // maybe stat does the thing?
    size_t fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    printf("file size: %zu\n", fsize);

    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1)
        TFAIL(SOCK_DESC_ERR);

    struct sockaddr_in dst = { .sin_family = AF_INET, .sin_port = htons(12345) };

    switch (inet_pton(AF_INET, "127.0.0.1", &dst.sin_addr))
    {
        case -1:  // returned when `af` (arg 1) isn't a valid address family (skip for now)
            break;
        case 0:
            TFAIL(DST_ADD_ERR);
        case 1:
            break;
        default:
            TFAIL(UNHANDLED_ERR);
    }

    int connected = connect(sd, (struct sockaddr *)&dst, sizeof(dst));

    if (connected)  // non-zero on error
        TFAIL(CONNECTION_FAIL);

    return 0;

exit:
    dprintf(2, "exit: %s\n", err_msg[terr]);
    exit(1);
}
