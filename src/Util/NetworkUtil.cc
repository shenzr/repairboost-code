#include "NetworkUtil.hh"

int printIP(int ip){
  unsigned char bytes[4];
  bytes[0] = ip & 0xFF;
  bytes[1] = (ip >> 8) & 0xFF;
  bytes[2] = (ip >> 16) & 0xFF;
  bytes[3] = (ip >> 24) & 0xFF;
  fprintf(stderr,"%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]); 
  return 0;
}

int sendMsg(int sd, const char* buf, int len){
  while (len > 0) {
    int retVal=send(sd, buf, len, 0);
    len -= retVal;
    buf += retVal;
  }
  return 0;
}

int recvMsg(int sd,char* buf,int len){
  if (DETAILED) fprintf(stdout,"expected to recv %d byte\n",len);
  int retVal;
  while (len > 0) {
    if ((retVal = recv(sd, buf, len, 0)) == 0) throw 0;
    len -= retVal;
    buf += retVal;
    if (DETAILED) fprintf(stdout, "  %d bytes received\n", retVal);
  }
  return 0;
}


