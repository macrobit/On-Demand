#include <string.h>

#include "api.h"
#include "odr.h"
#include "hw_addrs.h"

#if 0
const char* vms[] = {"192.168.56.102", "192.168.1.2", "192.168.1.3" };
const int vmsnum = 3;
#else
const char* vms[] = {"130.245.156.21", "130.245.156.22", "130.245.156.23", "130.245.156.24", "130.245.156.25", "130.245.156.26", "130.245.156.27", "130.245.156.28", "130.245.156.29", "130.245.156.20" };
const int vmsnum = 10;
#endif

int getvmdst(char * ip) {
    int i;
    for (i=0;i<vmsnum;i++) {
        if (!strcmp(ip, vms[i]))
            return i+1;
    }

    return 1;
}

int getvmcur() {
    static int vmcur = 0;
    if (vmcur)
        return vmcur;

    struct hwa_info* head = NULL;
    struct hwa_info* p;
    struct sockaddr_in* ifi_addr;
    struct in_addr addr;
    int i;

    if (!head)
        head = Get_hw_addrs();

    if (!head)
        return 1;

    p = head;
    while (p) {
        for (i=0;i<vmsnum;i++) {
            Inet_pton(AF_INET, vms[i], &addr);
            ifi_addr = (struct sockaddr_in*)p->ip_addr;
            if (ifi_addr->sin_addr.s_addr == addr.s_addr) {
                vmcur = i+1;
                return vmcur;
            }
        }

        p = p->hwa_next;
    }    

    return 1;
}

int msg_send(int sock, char* ip, int port, char* msgin, int flag) {
    struct sockaddr_un servaddr;
    struct msg_odr* msg;
    int msglen = 0;
    int sendlen = 0;

    if (!sock)
        return 0;

    if (!ip || strlen(ip) >= MAXIPLEN)
        return 0;

    bzero(&servaddr, sizeof(servaddr)); /* fill in server's address */
    servaddr.sun_family = AF_LOCAL;
    strcpy(servaddr.sun_path, ODRFILE);

    msglen = strlen(msgin);
    sendlen = sizeof(struct msg_odr)+msglen;
    msg = (struct msg_odr*)malloc(sendlen);
    if (!msg)
        return 0;

    memset(msg, 0, sendlen);

    msg->type = TYPE_PAYLOAD;
    if (flag)
        msg->type |= F_FORCE;

    strcpy(msg->ipdst, ip);
    msg->portdst = port;

    msg->msglen = msglen;
    memcpy(msg->msg, msgin, msglen);

    //printf("msg_send: %s\n", msgin);
    
    Sendto(sock, msg, sendlen, 0, (SA*)&servaddr, sizeof(servaddr));

    free(msg);
    return 1;
}

int msg_recv(int sock, char* msgout, char* ip, int* port) {
    struct msg_odr* msg;
    struct sockaddr_un cliaddr;
    struct timeval tv;
    char buf[512];
    int clilen;
    int len;
    int maxfdp1;
    int nready;
    fd_set allset;
    fd_set rset;

    FD_ZERO(&allset);
    FD_SET(sock, &allset);
    maxfdp1 = sock+1;

    for (;;) {
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        rset = allset;
        nready = select(maxfdp1, &rset, NULL, NULL, &tv);
        if (nready < 0) {
            if (errno == EINTR)
                continue;        /* back to for() */
            else
                err_sys("select error");
        } else if (nready == 0) {
            return -1;
        }

        if (FD_ISSET(sock, &rset)) {
            clilen = sizeof(struct sockaddr_un);
            len = Recvfrom(sock, buf, sizeof(buf), 0, (SA*)&cliaddr, &clilen);
            if (!len)
                return 0;

            msg = (struct msg_odr*)buf; 
            if (ip)
                strcpy(ip, msg->ipsrc);

            if (port)
                *port = msg->portsrc;

            if (msgout && msg->msglen) {
                memcpy(msgout, msg->msg, msg->msglen);
                msgout[msg->msglen] = 0;
                xprintf("msg_recv: %s\n", msgout);
            }

            return 1;
        }
    }

    return 0;
}
