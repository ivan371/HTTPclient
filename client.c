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
int gethttp(int i32SocketFD, int in, FILE* fout, int offset);
int parsehead();
void find_location();
int find_filename(char* way);

int main(int argc, char* argv[]) {
	if(argc < 2) {
			fprintf(stderr, "not 2 argc");
	}
	struct link httplink = parseconfig(argv[1]);
	//printf("%s\n", httplink.domain);
	//printf("%s\n", httplink.filename);
	struct sockaddr_in stSockAddr;
	int i32Res;
	int i32SocketFD = socket(AF_INET, SOCK_STREAM, 0);
	int mainsocket = socket(AF_INET, SOCK_STREAM, 0);
	struct hostent* host = gethostbyname(httplink.domain);
	if(host == 0) {
			fprintf(stderr, "Uncorrectable adress\n");
			exit(-1);
	}
	if (i32SocketFD == -1 || mainsocket == -1) {
			fprintf(stderr, "Unable to create socket\n");
			exit(-1);	}

	memset(&stSockAddr, 0, sizeof (stSockAddr));

	stSockAddr.sin_family = AF_INET;
	stSockAddr.sin_port = htons(80);

	i32Res = inet_pton(PF_INET, inet_ntoa(*(struct in_addr *)host->h_addr_list[0]), &stSockAddr.sin_addr);

	if (i32Res < 0) {
		fprintf(stderr, "Uncorrectable adress");
		close(i32SocketFD);
		return EXIT_FAILURE;
	} else if (!i32Res) {
		fprintf(stderr, "Uncorrectable ip");
		close(i32SocketFD);
		return EXIT_FAILURE;
	}
	//fprintf(stderr, "%s\n", inet_ntoa(*(struct in_addr *)host->h_addr_list[0]));
	if (connect(i32SocketFD, (struct sockaddr*) &stSockAddr, sizeof (stSockAddr)) == -1) {
		perror("Ошибка: соединения");
		close(i32SocketFD);
		return EXIT_FAILURE;
	}

	//fprintf(stderr, "Успешно подключились к серверу\n");
	int in = open("head.conf", O_RDONLY);
	FILE* fout=fopen("result.html", "w");
	int size = gethttp(i32SocketFD, in, fout, 0);
	//printf("size: %d\n", size);
	close(in);
	fclose(fout);
	shutdown(i32SocketFD, SHUT_RDWR);
	close(i32SocketFD);

	if(!parsehead()) {
			exit(1);
	}

	if (connect(mainsocket, (struct sockaddr*) &stSockAddr, sizeof (stSockAddr)) == -1) {
		perror("Ошибка: соединения");
		close(i32SocketFD);
		return EXIT_FAILURE;
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

int gethttp(int i32SocketFD, int in, FILE* fout, int offset) {
	char* str = (char*)calloc(BUF, sizeof(char));
	int res = 0;
	int nBytes = BUF;
	while(nBytes != 0)
	{
		nBytes = read (in, str, BUF);
		if(nBytes == 0)
				break;
		res = write(i32SocketFD, str, nBytes);
		if(res == -1)
		{
				perror("Ошибка записи");
				close(i32SocketFD);
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
			read(i32SocketFD, tmp, offset);
			free(tmp);
	}
	while(res != 0)
	{
			res = read(i32SocketFD, str, BUF);
			if(res == -1)
			{
					perror("Ошибка чтения");
					close(i32SocketFD);
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
			printf("Server doesn't support http!\n");
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
