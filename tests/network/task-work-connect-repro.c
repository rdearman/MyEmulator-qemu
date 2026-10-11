#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <net/route.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
static int set_addr(int fd, const char *name, unsigned long req, const char *ip) {
 struct ifreq q; struct sockaddr_in a; memset(&q,0,sizeof q); strncpy(q.ifr_name,name,IFNAMSIZ-1);
 memset(&a,0,sizeof a); a.sin_family=AF_INET; inet_pton(AF_INET,ip,&a.sin_addr); memcpy(&q.ifr_addr,&a,sizeof a);
 return ioctl(fd,req,&q);
}
int main(void) {
 int fd=socket(AF_INET,SOCK_DGRAM,0); if(fd<0){perror("socket");return 1;}
 struct ifreq q; memset(&q,0,sizeof q);strncpy(q.ifr_name,"eth0",IFNAMSIZ-1);
 if(ioctl(fd,SIOCGIFFLAGS,&q)<0){printf("INTERFACE=FAIL errno=%d\n",errno);}else {
  printf("INTERFACE=PASS flags=%x\n",q.ifr_flags); q.ifr_flags|=IFF_UP|IFF_RUNNING;
  if(ioctl(fd,SIOCSIFFLAGS,&q)<0)printf("LINK_UP=FAIL errno=%d\n",errno);else puts("LINK_UP=PASS");
  if(set_addr(fd,"eth0",SIOCSIFADDR,"10.0.2.15")<0)printf("ADDR=FAIL errno=%d\n",errno);else puts("ADDR=PASS 10.0.2.15");
  if(set_addr(fd,"eth0",SIOCSIFNETMASK,"255.255.255.0")<0)printf("MASK=FAIL errno=%d\n",errno);else puts("MASK=PASS /24");
 }
 struct rtentry r; memset(&r,0,sizeof r); struct sockaddr_in *dst=(void*)&r.rt_dst,*gw=(void*)&r.rt_gateway,*mask=(void*)&r.rt_genmask;
 dst->sin_family=gw->sin_family=mask->sin_family=AF_INET; inet_pton(AF_INET,"0.0.0.0",&dst->sin_addr);inet_pton(AF_INET,"10.0.2.2",&gw->sin_addr);inet_pton(AF_INET,"0.0.0.0",&mask->sin_addr);r.rt_flags=RTF_UP|RTF_GATEWAY;r.rt_dev="eth0";
 if(ioctl(fd,SIOCADDRT,&r)<0)printf("DEFAULT_ROUTE=FAIL errno=%d %s\n",errno,strerror(errno));else puts("DEFAULT_ROUTE=PASS"); close(fd);
 int rx=socket(AF_INET,SOCK_DGRAM,0),tx=socket(AF_INET,SOCK_DGRAM,0); struct sockaddr_in l={.sin_family=AF_INET,.sin_port=0};inet_pton(AF_INET,"127.0.0.1",&l.sin_addr);
 int local=0; if(rx>=0&&tx>=0&&bind(rx,(void*)&l,sizeof l)==0){socklen_t n=sizeof l;getsockname(rx,(void*)&l,&n);sendto(tx,"x",1,0,(void*)&l,sizeof l);struct pollfd p={rx,POLLIN,0};if(poll(&p,1,1500)>0){char b;local=recv(rx,&b,1,0)==1&&b=='x';}}
 printf("LOCALHOST_UDP=%s\n",local?"PASS":"FAIL");
 struct addrinfo h={.ai_family=AF_INET,.ai_socktype=SOCK_STREAM},*ai=0;int gr=getaddrinfo("example.com",0,&h,&ai);printf("DNS_EXAMPLE_COM=%s gai=%d\n",gr==0?"PASS":"FAIL",gr);if(ai)freeaddrinfo(ai);
 int s=socket(AF_INET,SOCK_STREAM,0),ext=0,err=0;if(s>=0){fcntl(s,F_SETFL,O_NONBLOCK);struct sockaddr_in a={.sin_family=AF_INET,.sin_port=htons(80)};inet_pton(AF_INET,"1.1.1.1",&a.sin_addr);int c=connect(s,(void*)&a,sizeof a);if(c==0)ext=1;else if(errno==EINPROGRESS){struct pollfd p={s,POLLOUT,0};if(poll(&p,1,4000)>0){socklen_t n=sizeof err;getsockopt(s,SOL_SOCKET,SO_ERROR,&err,&n);ext=err==0;}}else err=errno;}
 printf("EXTERNAL_TCP_1.1.1.1:80=%s errno=%d\n",ext?"PASS":"FAIL",err);return 0;
}
