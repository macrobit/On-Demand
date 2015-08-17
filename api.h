#ifndef API_H
#define API_H

#define SERVFILE "/tmp/tmp-server"
#define SERVPORT 30957

#if 0
#define xprintf printf
#else
#define xprintf
#endif

extern const char* vms[];
extern const int vmsnum;

int msg_send(int sock, char* ip, int port, char* msg, int flag);
int msg_recv(int sock, char* msg, char* ip, int* port);
int getvmdst(char * ip);
int getvmcur();

#endif
