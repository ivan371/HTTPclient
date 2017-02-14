#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

#define BUF 10
#define LENHTTP 7
#define LENHTTPS 8

struct link {
	char* domain;
	char* filename;
};

struct link parseconfig(char* name);
int gethttp(int headsocket, int in, FILE* fout, int offset);
int parsehead();
void find_location();
int find_filename(char* way);

int main(int argc, char* argv[]) {
	if(argc < 2) {
		fprintf(stderr, "not 2 argc");
		exit(1);
	}
	struct link httplink = parseconfig(argv[1]);
	//printf("%s\n", httplink.domain);
	//printf("%s\n", httplink.filename);
	struct sockaddr_in stSockAddr;
	int result;
	int headsocket = socket(AF_INET, SOCK_STREAM, 0);
	int mainsocket = socket(AF_INET, SOCK_STREAM, 0);
	struct hostent* host = gethostbyname(httplink.domain);
	if(host == 0) {
		fprintf(stderr, "Uncorrectable adress\n");
		exit(-1);
	}
	if (headsocket == -1 || mainsocket == -1) {
		fprintf(stderr, "Unable to create socket\n");
		exit(-1);
	}

	memset(&stSockAddr, 0, sizeof (stSockAddr));

	stSockAddr.sin_family = AF_INET;
	stSockAddr.sin_port = htons(80);

	result = inet_pton(PF_INET, inet_ntoa(*(struct in_addr *)host->h_addr_list[0]), &stSockAddr.sin_addr);

	if (result < 0) {
		fprintf(stderr, "Uncorrectable adress");
		close(headsocket);
		exit(1);
	} else if (!result) {
		fprintf(stderr, "Uncorrectable ip");
		close(headsocket);
		exit(1);
	}
	//fprintf(stderr, "%s\n", inet_ntoa(*(struct in_addr *)host->h_addr_list[0]));
	if (connect(headsocket, (struct sockaddr*) &stSockAddr, sizeof (stSockAddr)) == -1) {
		fprintf(stderr, "Connection error");
		close(headsocket);
		exit(1);
	}

	//fprintf(stderr, "Успешно подключились к серверу\n");
	int in = open("head.conf", O_RDONLY);
	FILE* fout=fopen("result.html", "w");
	int size = gethttp(headsocket, in, fout, 0);
	//printf("size: %d\n", size);
	close(in);
	fclose(fout);
	shutdown(headsocket, SHUT_RDWR);
	close(headsocket);

	if(!parsehead()) {
		exit(1);
	}

	if (connect(mainsocket, (struct sockaddr*) &stSockAddr, sizeof (stSockAddr)) == -1) {
		fprintf(stderr, "Connection error");
		close(headsocket);
		exit(1);
	}
	int input = open("config.conf", O_RDONLY);
	FILE* foutput=fopen(httplink.filename, "w");
	gethttp(mainsocket, input, foutput, size);
	close(input);
	fclose(foutput);

	shutdown(mainsocket, SHUT_RDWR);
	close(mainsocket);

	return 0;
}

struct link parseconfig(char* name) {
	int len = strlen(name);
	struct link httplink;
	char http[LENHTTP] = "http://";
	char https[LENHTTPS] = "https://";
	if(!strncmp(https, name, LENHTTPS)) {
		fprintf(stderr, "Sorry, our client works only with http\n");
		exit(1);
	}
	if(strncmp(http, name, LENHTTP)) {
		fprintf(stderr, "Uncorrectable adress.\nYou http adress must begin with http\n");
		exit(1);
	}
	int i;
	FILE * fin = fopen("config.conf", "w");
	FILE * fhead = fopen("head.conf", "w");
	for(i = LENHTTP; i < len && name[i] != '/'; i++);
	int slash = find_filename(name + i + 1);
	char* filename;
	char* index = "index.html";
	if(slash == 0) {
		filename = index;
	}
	else {
		filename = name + i + slash + 2;
	}
	//printf("%s\n", filename);
	char* domain = (char*)calloc(i - LENHTTP, sizeof(char));
	strncpy(domain, name + LENHTTP, i - LENHTTP);
	if (name[i] == '/') {
		//printf("domain: %s \nfile adress: %s \n", domain, name + i + 1);
		fprintf(fin, "GET /%s HTTP/1.1\r\nHost: %s \r\nConnection: close\r\n\r\n", name + i + 1, domain);
		fprintf(fhead, "HEAD /%s HTTP/1.1\r\nHost: %s \r\nConnection: close\r\n\r\n", name + i + 1, domain);
	}
	else {
		//printf("domain: %s \nfile adress: \n", domain);
		fprintf(fin, "GET / HTTP/1.1\r\nHost: %s \r\nConnection: close \r\n\r\n", domain);
		fprintf(fhead, "HEAD / HTTP/1.1\r\nHost: %s \r\nConnection: close \r\n\r\n", domain);
	}
	fclose(fin);
	fclose(fhead);
	httplink.domain = domain;
	httplink.filename = filename;
	return httplink;
}

int gethttp(int headsocket, int in, FILE* fout, int offset) {
	char* str = (char*)calloc(BUF, sizeof(char));
	int res = 0;
	int nbytes = BUF;
	while(nbytes != 0)
	{
		nbytes = read (in, str, BUF);
		if(nbytes == 0)
			break;
		res = write(headsocket, str, nbytes);
		if(res == -1)
		{
			fprintf(stderr, "Write error");
			close(headsocket);
			close(in);
			fclose(fout);
			exit(-1);
		}
	}
	//fprintf(stderr, "Запрос отправлен\n");
	res = BUF;
	int size = 0;
	if(offset != 0) {
		char* tmp = (char*)calloc(offset, sizeof(char));
		read(headsocket, tmp, offset);
		free(tmp);
	}
	while(res != 0)
	{
		res = read(headsocket, str, BUF);
		if(res == -1)
		{
			fprintf(stderr, "Read error");
			close(headsocket);
			close(in);
			fclose(fout);
			exit(-1);
		}
		size += res;
		fwrite(str, sizeof(char), res, fout);
	}
	//fprintf(stderr, "Запрос принят\n");
	return size;
}

int parsehead() {
	FILE* head = fopen("result.html", "r");
	char* http = (char*)calloc(10, sizeof(char));
	int request;
	char* status = (char*)calloc(10, sizeof(char));
	char myhttp[] = "HTTP/1.1";
	int result = 1;
	fscanf(head, "%s\n", http);
	fscanf(head, "%d\n", &request);
	fscanf(head, "%s\n", status);
	fclose(head);
	if(!strcmp(http, "")) {
		printf("Server doesn't support http or doesn't work\n");
		result = 0;
	} else if(strcmp(http, myhttp)) {
		printf("Warning! this version of http is not 1.1!\n");
	}
	switch (request / 100) {
		case 3:
			printf("You must redirect to another location:\n");
			result = 0;
			find_location();
		break;
		case 4:
			printf("Client error\n");
			result = 0;
		break;
		case 5:
			printf("Server error\n");
			result = 0;
		break;
	}
	free(status);
	free(http);
	return result;
}

void find_location() {
	FILE* head = fopen("result.html", "r");
	char* str = (char*)calloc(100, sizeof(char));
	char location[] = "Location:";
	while (strcmp(str, location) && fscanf(head, "%s", str));
	fscanf(head, "%s", str);
	printf("%s\n", str);
	free(str);
	fclose(head);
}

int find_filename(char* way) {
	int len = strlen(way);
	int lastslash = 0;
	int i = 0;
	for(i = 0; i < len; i++) {
		if(way[i] == '/')
			lastslash = i;
	}
	return lastslash;
}
