#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>

#include <arpa/inet.h>

#define MAXDGRAM 10240

int main(int argc, char **argv)
{
	int sockfd;
	struct sockaddr_in servaddr;
#define ADDRP (struct sockaddr *)&servaddr
#define ADDRSZ sizeof(servaddr)
	char *sendbuf = malloc(MAXDGRAM);
	int nb;
	int maxdgram = MAXDGRAM;

	if (getenv("MAXDGRAM")) {
		maxdgram = atoi(getenv("MAXDGRAM"));
	}
	sendbuf = malloc(maxdgram);

	if (argc < 3) {
		printf("usage:  udpcli <IP address> <port>\n");
		exit(1);
	}

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(argv[1]);
	servaddr.sin_port = htons(atoi(argv[2]));

	while ((nb = fread(sendbuf, 1, maxdgram, stdin)) > 0) {
		sendto(sockfd, sendbuf, nb, 0, ADDRP, ADDRSZ);
	}
	return 0;
}
