#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h> 
#define BUFFER_LEN 4
// 生成随机字母（大小写混合）
void generate_random_letters(char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        // 随机选择大小写（50%概率）
        if (rand() % 2) {
            buf[i] = 'A' + (rand() % 26);  // 大写字母
        } else {
            buf[i] = 'a' + (rand() % 26);  // 小写字母
        }
    }
}
int main(){
    int fd = open("/dev/my_buffer", O_RDWR);
	if (-1 != fd)
	{
        while (1)
        {
            //生成要写到设备中的数据
            char buffer[BUFFER_LEN];
            generate_random_letters(buffer,BUFFER_LEN);
            // 执行写入操作
            int ret = write(fd, buffer, BUFFER_LEN);
            if (ret < 0) {
                fprintf(stderr, "写入失败\n");
                close(fd);
                return EXIT_FAILURE;
            }
            printf("Has written %c %c %c %c.\n",buffer[0],buffer[1],buffer[2],buffer[3]);
            sleep(rand()%3);
        }
	}
	else
	{
		printf("open failed! \n");
	}
	return 0;
}