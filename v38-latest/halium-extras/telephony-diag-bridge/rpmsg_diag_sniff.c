#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

int main(int argc, char **argv) {
    const char *dev = argc > 1 ? argv[1] : "/dev/rpmsg0";
    int fd = open(dev, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "open(%s) failed: %s\n", dev, strerror(errno));
        return 1;
    }
    fprintf(stderr, "opened %s, reading (Ctrl-C or wait for timeout to stop)...\n", dev);
    unsigned char buf[4096];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            fprintf(stderr, "read() failed: %s\n", strerror(errno));
            break;
        }
        if (n == 0) {
            fprintf(stderr, "EOF\n");
            break;
        }
        time_t now = time(NULL);
        printf("--- %ld bytes at %ld ---\n", (long)n, (long)now);
        for (ssize_t i = 0; i < n; i++) {
            printf("%02x ", buf[i]);
            if ((i + 1) % 16 == 0) printf("\n");
        }
        printf("\n");
        fflush(stdout);
    }
    close(fd);
    return 0;
}
