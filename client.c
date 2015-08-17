//client
#include "unp.h"
#include "api.h"

static char tmpfname[MAXLINE];
//static int tmpfd = 0;
//static int cliid = 0;

void createtmpfile() {
    int fd;

    strcpy(tmpfname, "/tmp/tmp-XXXXXX");
    fd = mkstemp(tmpfname);
    close(fd);
}

void deletetmpfile() {
    unlink(tmpfname);
}

int main(int argc, char** argv) {
    int sockfd;
    int r;
    //struct sockaddr_in servaddr;

    //bzero(&servaddr, sizeof(servaddr));
    //
    struct sockaddr_un cliaddr;
    int vmcur;

    createtmpfile();
    //printf("tmpfile: %s\n", tmpfname);
    unlink(tmpfname);
#if 0
    if (r < 0) {
        printf("Fail to create temp file");
        return 0;
    }
#endif
    sockfd = Socket(AF_LOCAL, SOCK_DGRAM, 0);

    bzero(&cliaddr, sizeof(cliaddr));   /* bind an address for us */
    cliaddr.sun_family = AF_LOCAL;
    strcpy(cliaddr.sun_path, tmpfname);

    Bind(sockfd, (SA *) &cliaddr, sizeof(cliaddr));
    vmcur = getvmcur();

    for(;;) {
        int servid;
        char msg[MAXLINE];
        char servip[MAXLINE];
        int servport;
        int total;
    //time_t t;
        char msg1[] = "t";

        total = vmsnum;
        printf("server (1~%d): ", total);
        scanf("%d", &servid);
        if (servid > total || servid < 1)
            continue;

        // trace
        printf("client at node vm%d sending request to server at vm%d\n", vmcur, servid);
        strcpy(servip, vms[servid-1]);
        servport = SERVPORT;
        msg_send(sockfd, servip, servport, msg1, 0);

        // trace
        r = msg_recv(sockfd, msg, servip, &servport);
#if 1
        if (r == -1) {
            printf("client at node vm%d: timeout on response from vm%d\n", vmcur, servid);
            // trace
            printf("client at node vm%d sending request to server at vm%d\n", vmcur, servid);
            msg_send(sockfd, servip, servport, msg1, 1);
            r = msg_recv(sockfd, msg, servip, &servport);
        }
#endif
        if (r == -1) {
            printf("client at node vm %d: timeout again on response from vm %d\n", vmcur, servid);
        } else {
            int msglen = strlen(msg);
            if (msglen && (msg[msglen-1] == '\r' || msg[msglen-1] == '\n')) {
                msg[msglen-1] = 0;
            }
            printf("client at node vm%d received from vm%d <%s>\n", vmcur, servid, msg);
        }
    }

    return 0;
}
