#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <errno.h>

struct rpmsg_endpoint_info {
    char name[32];
    __u32 src;
    __u32 dst;
};

#define RPMSG_CREATE_EPT_IOCTL  _IOW(0xb5, 0x1, struct rpmsg_endpoint_info)

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <channel-name> [ctrl-dev]\n", argv[0]);
        return 1;
    }
    const char *chan = argv[1];
    const char *ctrldev = argc > 2 ? argv[2] : "/dev/rpmsg_ctrl3";

    int fd = open(ctrldev, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open(%s) failed: %s\n", ctrldev, strerror(errno));
        return 1;
    }

    struct rpmsg_endpoint_info info;
    memset(&info, 0, sizeof(info));
    strncpy(info.name, chan, sizeof(info.name) - 1);
    info.src = 0xFFFFFFFF;
    info.dst = 0xFFFFFFFF;

    int rc = ioctl(fd, RPMSG_CREATE_EPT_IOCTL, &info);
    if (rc < 0) {
        fprintf(stderr, "ioctl RPMSG_CREATE_EPT_IOCTL for '%s' failed: %s (errno=%d)\n", chan, strerror(errno), errno);
        close(fd);
        return 1;
    }
    printf("ioctl RPMSG_CREATE_EPT_IOCTL for '%s' succeeded (rc=%d)\n", chan, rc);
    fflush(stdout);
    if (argc > 3 && strcmp(argv[3], "hold") == 0) {
        for (;;) pause();
    }
    close(fd);
    printf("ctrl fd closed, exiting\n");
    return 0;
}
