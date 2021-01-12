#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#define INIT_RETRIES 5
void send_buffer(int fd, char* buffer, int bytes);
void print_headers();
double subtractTimeOfDay(struct timeval* begin, struct timeval* end);

void usage()
{
	fprintf(stdout, "usage: gpnetbench -p PORT -H HOST [-l SECONDS] [-t EXPERIMENT] [-f UNITS] [-P HEADERS] [-b KB] [-h]\n");
	fprintf(stdout, "where\n");
	fprintf(stdout, "       PORT is the port to connect to for the server\n");
	fprintf(stdout, "       HOST is the hostname to connect to for the server\n");
	fprintf(stdout, "       SECONDS is the number of seconds to sample the network, where the default is 60\n");
	fprintf(stdout, "       EXPERIMENT is the experiment name to run, where the default is TCP_STREAM\n");
	fprintf(stdout, "       UNITS is the output units, where the default is M megabytes\n");
	fprintf(stdout, "       HEADERS is 0 (don't) or 1 (do) display headers in the output\n");
	fprintf(stdout, "       KB is the size of the send buffer in kilobytes, where the default is 32\n");

	fprintf(stdout, "       -h shows this help message\n");
}

int main(int argc, char** argv)
{
	int socketFd;
	int retVal;
	int c;
	int displayHeaders = 1;
	char* serverPort = "0";
	int duration = 60;
	double actual_duration;
	char* hostname = NULL;
	char* sendBuffer = NULL;
	int kilobytesBufSize = 32;
	int bytesBufSize;
	struct addrinfo hints, *servinfo, *p;
	time_t start_time;
	time_t end_time;
	unsigned int buffers_sent = 0;
	double megaBytesSent;
	double megaBytesPerSecond;
	struct timeval beginTimeDetails;
	struct timeval endTimeDetails;

	while ((c = getopt (argc, argv, "p:l:b:P:H:f:t:h")) != -1)
	{
		switch (c)
		{
			case 'p':
				serverPort = optarg;
				break;
			case 'l':
				duration = atoi(optarg);
				break;
			case 'b':
				kilobytesBufSize = atoi(optarg);
				break;
			case 'P':
				displayHeaders = atoi(optarg);
				if (displayHeaders)
					displayHeaders = 1;
				break;
			case 'H':
				hostname = optarg;
				break;
			case 'f':
				// backward compat
				break;
			case 't':
				// backward compat
				break;
			case 'h':
			case '?':
			default:
				usage();
				return 1;
		}
	}

	if (!serverPort)
	{
		fprintf(stdout, "-p port not specified\n");
		usage();
		return 1;
	}
	if (!hostname)
	{
		fprintf(stdout, "-H hostname not specified\n");
		usage();
		return 1;
	}

	// validate a sensible value for duration
	if (duration < 5 || duration > 3600)
	{
		fprintf(stdout, "duration must be between 5 and 3600 seconds\n");
		return 1;
	}

	// validate a sensible value for buffer size
	if (kilobytesBufSize < 1 || kilobytesBufSize > 10240)
	{
		fprintf(stdout, "buffer size for sending must be between 1 and 10240 KB\n");
		return 1;
	}
	bytesBufSize = kilobytesBufSize * 1024;

	sendBuffer = malloc(bytesBufSize);
	memset(sendBuffer, 0, bytesBufSize);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;	/* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;    /* Any protocol - TCP implied for network use due to SOCK_STREAM */

	retVal = getaddrinfo(hostname, serverPort, &hints, &servinfo);
	if (retVal != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retVal));
		exit(1);
	}

	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		socketFd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (socketFd < 0)
		{
			fprintf(stderr, "socket call failed, trying next if available..\n");
			continue;
		}

		if (connect(socketFd, p->ai_addr, p->ai_addrlen) == -1)
		{
			perror("connect");
			close(socketFd);
			continue;
		}

		break; // successfully connected
	}

	if (p == NULL)
	{
		fprintf(stderr, "failed to connect\n");
		exit(1);
	}

	freeaddrinfo(servinfo);

	printf("Connected to server\n");

	start_time = time(NULL);
	end_time = start_time + duration;
	gettimeofday(&beginTimeDetails, NULL);
	while (time(NULL) < end_time)
	{
		send_buffer(socketFd, sendBuffer, bytesBufSize);
		buffers_sent++;
	}
	gettimeofday(&endTimeDetails, NULL);

	actual_duration = subtractTimeOfDay(&beginTimeDetails, &endTimeDetails);
	megaBytesSent = buffers_sent * (double)bytesBufSize / (1024.0*1024.0);
	megaBytesPerSecond = megaBytesSent / actual_duration;

	if (displayHeaders)
		print_headers();

	printf("0     0        %d       %.2f     %.2f\n", bytesBufSize, (double)actual_duration, megaBytesPerSecond);
	return 0;
}

void send_buffer(int fd, char* buffer, int bytes)
{
	ssize_t retval;

	while(bytes > 0)
	{
		retval = send(fd, buffer, bytes, 0);
		if (retval < 0)
		{
			perror("error on send call");
			exit(1);
		}
		if (retval > bytes)
		{
			fprintf(stdout, "unexpected large return code from send %d with only %d bytes in send buffer\n", (int)retval, bytes);
		}

		// advance the  buffer by number of bytes sent and reduce number of bytes remaining to be sent
		bytes -= retval;
		buffer += retval;
	}
}

double subtractTimeOfDay(struct timeval* begin, struct timeval* end)
{
	double seconds;

	if (end->tv_usec < begin->tv_usec)
	{
		end->tv_usec += 1000000;
		end->tv_sec -= 1;
	}

	seconds = end->tv_usec - begin->tv_usec;
	seconds /= 1000000.0;

	seconds += (end->tv_sec - begin->tv_sec);
	return seconds;
}

void print_headers()
{
	printf("               Send\n");
	printf("               Message  Elapsed\n");
	printf("               Size     Time     Throughput\n");
	printf("n/a   n/a      bytes    secs.    MBytes/sec\n");
}
