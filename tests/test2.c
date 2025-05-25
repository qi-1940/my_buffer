#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h> 
#define READ_LEN 2
#define CIRCUL_NUM 20

int main(){
    int fd = open("/dev/my_buffer", O_RDWR);
	char buffer[READ_LEN];
	if (-1 != fd)
	{
		int i = CIRCUL_NUM;
		while (1)
		{
			ssize_t bytes_read = read(fd, buffer, READ_LEN);
			if (bytes_read < 0) {
				perror("Failed to read from device");
				close(fd);
				exit(EXIT_FAILURE);
			}
			printf("read ended.Has read %c %c %c.\n",buffer[0],buffer[1],buffer[2]);
			sleep(rand()%3);
		}
	}
	else
	{
		printf("open failed! \n");
	}
	return 0;
}