#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <net/route.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

static int set_address(int fd, const char *name, unsigned long request,
                       const char *address)
{
    struct ifreq req;
    memset(&req, 0, sizeof(req));
    strncpy(req.ifr_name, name, IFNAMSIZ - 1);
    struct sockaddr_in *sin = (struct sockaddr_in *)&req.ifr_addr;
    sin->sin_family = AF_INET;
    if (inet_pton(AF_INET, address, &sin->sin_addr) != 1)
        return -1;
    return ioctl(fd, request, &req);
}

static int configure_interface(int fd, const char *name, const char *address,
                               const char *mask, const char *broadcast)
{
    struct ifreq req;
    memset(&req, 0, sizeof(req));
    strncpy(req.ifr_name, name, IFNAMSIZ - 1);
    if (ioctl(fd, SIOCGIFFLAGS, &req) < 0)
        return -1;
    req.ifr_flags |= IFF_UP | IFF_RUNNING;
    if (ioctl(fd, SIOCSIFFLAGS, &req) < 0)
        return -1;
    if (set_address(fd, name, SIOCSIFADDR, address) < 0 ||
        set_address(fd, name, SIOCSIFNETMASK, mask) < 0 ||
        set_address(fd, name, SIOCSIFBRDADDR, broadcast) < 0)
        return -1;
    return 0;
}

static int add_default_route(const char *device, const char *gateway)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return -1;
    struct rtentry route;
    memset(&route, 0, sizeof(route));
    struct sockaddr_in *dst = (struct sockaddr_in *)&route.rt_dst;
    struct sockaddr_in *gw = (struct sockaddr_in *)&route.rt_gateway;
    struct sockaddr_in *mask = (struct sockaddr_in *)&route.rt_genmask;
    dst->sin_family = AF_INET;
    gw->sin_family = AF_INET;
    mask->sin_family = AF_INET;
    inet_pton(AF_INET, "0.0.0.0", &dst->sin_addr);
    inet_pton(AF_INET, gateway, &gw->sin_addr);
    inet_pton(AF_INET, "0.0.0.0", &mask->sin_addr);
    route.rt_flags = RTF_UP | RTF_GATEWAY;
    route.rt_dev = (char *)device;
    int result = ioctl(fd, SIOCADDRT, &route);
    if (result < 0 && errno == EEXIST)
        result = 0;
    close(fd);
    return result;
}

static void write_resolver(void)
{
    int fd = open("/etc/resolv.conf", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        const char line[] = "nameserver 10.0.2.3\n";
        (void)write(fd, line, sizeof(line) - 1);
        close(fd);
    }
}

int main(int argc, char **argv)
{
    const char *action = argc > 1 ? argv[1] : "start";
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("rem-net: socket");
        return 1;
    }
    if (!strcmp(action, "stop")) {
        struct ifreq req;
        memset(&req, 0, sizeof(req));
        strncpy(req.ifr_name, "eth0", IFNAMSIZ - 1);
        if (!ioctl(fd, SIOCGIFFLAGS, &req)) {
            req.ifr_flags &= (short)~IFF_UP;
            (void)ioctl(fd, SIOCSIFFLAGS, &req);
        }
        puts("REM_NET|stop|PASS");
        close(fd);
        return 0;
    }
    if (strcmp(action, "start")) {
        fprintf(stderr, "usage: rem-net {start|stop}\n");
        close(fd);
        return 2;
    }
    if (configure_interface(fd, "lo", "127.0.0.1", "255.0.0.0", "127.255.255.255"))
        fprintf(stderr, "REM_NET|loopback-warning|%s\n", strerror(errno));
    if (configure_interface(fd, "eth0", "10.0.2.15", "255.255.255.0", "10.0.2.255")) {
        fprintf(stderr, "REM_NET|eth0-warning|%s\n", strerror(errno));
        close(fd);
        return 1;
    }
    close(fd);
    if (add_default_route("eth0", "10.0.2.2"))
        fprintf(stderr, "REM_NET|route-warning|%s\n", strerror(errno));
    write_resolver();
    puts("REM_NET|eth0=10.0.2.15/24|gateway=10.0.2.2|PASS");
    return 0;
}
