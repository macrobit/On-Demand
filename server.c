// server
#include "unp.h"
#include "api.h"

// todo: time

//static int tmpfd = 0;
//static int cliid = 0;
//static int servport = 99;
#if 0
void gethostinfo() {
    //get_hw_addrs()
}

int createtmpfile() {
    strcpy(tmpfname, "tmp-XXXXXX");
    tmpfd = mkstemp(tmpfname);
    return tmpfd;
}

void deletetmpfile() {
    if (tmpfd)
        unlink(tmpfname);
}
#endif
int main(int argc, char** argv) {
    int sockfd;
	int vmcur;
    int r;

    struct sockaddr_un cliaddr, servaddr;

    sockfd = Socket(AF_LOCAL, SOCK_DGRAM, 0);

    unlink(SERVFILE);
    bzero(&servaddr, sizeof(servaddr));   /* bind an address for us */
    servaddr.sun_family = AF_LOCAL;
    strcpy(servaddr.sun_path, SERVFILE);

    Bind(sockfd, (SA *) &servaddr, sizeof(servaddr));
	vmcur = getvmcur();

    for(;;) {
        int servid;
        char msg[MAXLINE];
        char cliip[MAXLINE];
        char* msgout;
        int cliport;
		int vmdst;
        time_t t;

        r = msg_recv(sockfd, msg, cliip, &cliport);
        if (r == -1)
            continue;

		vmdst = getvmdst(cliip);
        // trace
        t = time(NULL);
        msgout = ctime(&t);
		printf("server at node vm%d responding to request from vm%d\n", vmcur, vmdst);
        msg_send(sockfd, cliip, cliport, msgout, 0);
    }

    return 0;
}
