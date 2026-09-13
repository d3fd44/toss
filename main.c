#include <arpa/inet.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

// propably overkill. just wanna be cool lol.
#define ERRORS(T)                                                                                                                          \
    T(SD_ERR, "socket error: couldn't initialize file descriptor.")                                                                        \
    T(FS_ERR, "fs error: error reading file.")                                                                                             \
    T(AL_ERR, "memory allocation failed.")                                                                                                 \
    T(BD_ARG, "bad arguments.")                                                                                                            \
    T(AD_ERR, "address error")                                                                                                             \
    T(CONNECTION_FAILED, "couldn't connect to the destination")

typedef enum
{
#define T(err, errmsg) err,
    ERRORS(T)
#undef T
      NO_ERR = -1

} throw_err_t;

static throw_err_t terr = -1;
const char        *err_msg[] = {
#define T(err, errmsg) [err] = errmsg,
    ERRORS(T)
#undef T
};

#define THROW_FAIL(code)                                                                                                                   \
    do                                                                                                                                     \
    {                                                                                                                                      \
        terr = (code);                                                                                                                     \
        goto exit;                                                                                                                         \
    } while (0)

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("usage: throw <dest-ip> <file>\n");
        THROW_FAIL(BD_ARG);
    }

    FILE *file = fopen(argv[2], "r");
    if (!file)
        THROW_FAIL(FS_ERR);

    fseek(file, 0, SEEK_END);
    size_t fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    printf("file size: %zu\n", fsize);

    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1)
        THROW_FAIL(SD_ERR);

    struct sockaddr_in dst = { .sin_family = AF_INET, .sin_port = htons(12345) };

    if (inet_pton(AF_INET, "127.0.0.1", &dst.sin_addr) == -1)  // TODO: 0 means string missing somethign (man 3 inet_pton)
        THROW_FAIL(AD_ERR);

    int connected = connect(sd, (struct sockaddr *)&dst, sizeof(dst));

    if (connected)  // non-zero on error
        THROW_FAIL(CONNECTION_FAILED);

    return 0;

exit:
    dprintf(2, "exit: %s\n", err_msg[terr]);
    exit(1);
}
