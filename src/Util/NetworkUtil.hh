# ifndef _NETWORK_UTIL_HH_
# define _NETWORK_UTIL_HH_

#include <cstdio>
#include <cstdlib>
#include <curses.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KRST  "\033[0m"

#define DETAILED true

int printIP(int);
int sendMsg(int sd, const char* buf,int len);
int recvMsg(int sd, char* buf,int len);

# endif
