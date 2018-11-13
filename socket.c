#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

#define BUFSIZE 8096

struct {
    char *ext;
    char *filetype;
} extensions [] = {
    {"jpg", "image/jpeg"},
    {"jpeg","image/jpeg"},
    {"png", "image/png" },
    {"htm", "text/html" },
    {"html","text/html" },
    {0,0} };


void handle_socket(int fd)
{
    int j, file_fd, buflen, len;
    long i, ret;
    char * fstr;
    static char buffer[BUFSIZE+1];

    ret = read(fd,buffer,BUFSIZE);   /* 讀取瀏覽器要求 */
    if (ret==0||ret==-1) {
     /* 網路連線有問題，所以結束行程 */
        exit(3);
    }

    /* 程式技巧：在讀取到的字串結尾補空字元，方便後續程式判斷結尾 */
    if (ret>0&&ret<BUFSIZE)
        buffer[ret] = 0;
    else
        buffer[0] = 0;

    /* 移除換行字元 */
    for (i=0;i<ret;i++) 
        if (buffer[i]=='\r'||buffer[i]=='\n')
            buffer[i] = 0;
    
    /* 只接受 GET 命令要求 */
    if (strncmp(buffer,"GET ",4)&&strncmp(buffer,"get ",4))
        exit(3);
    
    /* 我們要把 GET /index.html HTTP/1.0 後面的 HTTP/1.0 用空字元隔開 */
    for(i=4;i<BUFSIZE;i++) {
        if(buffer[i] == ' ') {
            buffer[i] = 0;
            break;
        }
    }

    /* 檔掉回上層目錄的路徑『..』 */
    for (j=0;j<i-1;j++)
        if (buffer[j]=='.'&&buffer[j+1]=='.')
            exit(3);

    /* 當客戶端要求根目錄時讀取 index.html */
    if (!strncmp(&buffer[0],"GET /\0",6)||!strncmp(&buffer[0],"get /\0",6) )
        strcpy(buffer,"GET /index.html\0");

    /* 檢查客戶端所要求的檔案格式 */
    buflen = strlen(buffer);
    fstr = (char *)0;

    for(i=0;extensions[i].ext!=0;i++) {
        len = strlen(extensions[i].ext);
        if(!strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
            fstr = extensions[i].filetype;
            break;
        }
    }

    /* 檔案格式不支援 */
    if(fstr == 0) {
        fstr = extensions[i-1].filetype;
    }

    /* 開啟檔案 */
    if((file_fd=open(&buffer[5],O_RDONLY))==-1)
        write(fd, "Failed to open file", 19);

    /* 傳回瀏覽器成功碼 200 和內容的格式 */
    sprintf(buffer,"HTTP/1.0 200 OK\r\nContent-Type: %s\r\n\r\n", fstr);
    write(fd,buffer,strlen(buffer));


    /* 讀取檔案內容輸出到客戶端瀏覽器 */
    ret=read(file_fd, buffer, BUFSIZE);
    while (ret>0) {
        write(fd,buffer,ret);
        ret=read(file_fd, buffer, BUFSIZE);
    }
}

void select_socket(int port)
{
    printf("select socket.\n");

    int socketfd = 0,listenfd = 0,fd_max;
	
    fd_set rset, read_fds;
    FD_ZERO(&rset);
	FD_ZERO(&read_fds);

	static struct sockaddr_in cli_addr;
    static struct sockaddr_in serv_addr;
    socklen_t len;
	len  = sizeof(cli_addr) - 1;
	bzero(&serv_addr,sizeof(serv_addr));
	

    /* 開啟網路 Socket */
    if ((socketfd=socket(PF_INET, SOCK_STREAM,0))<0)
        exit(3);
    
    /* 網路連線設定 */
    serv_addr.sin_family = AF_INET;
    /* 使用任何在本機的對外 IP */
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    /* 使用 8888 Port */
    serv_addr.sin_port = htons(port);

    /* 開啟網路監聽器 */
    bind(socketfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr));
        
    /* 開始監聽網路 */
    listen(socketfd,64);

	FD_SET(socketfd, &rset);
	fd_max = socketfd;

	while(1)
    {
		read_fds = rset;
		if (select(fd_max + 1, &read_fds, NULL, NULL, NULL) == -1)
			exit(4);
        
		for (int i = 0; i <= fd_max; i++)
        {
			if(FD_ISSET(i, &read_fds))
            {
				if(i == socketfd)
                {
					listenfd = accept(socketfd, (struct sockaddr*) &cli_addr, &len);
					if (listenfd == -1)
						perror("accept");
					else
                    {
						FD_SET(listenfd, &rset);
						if(listenfd > fd_max)
							fd_max = listenfd;
					}
				}
                else
                {
					handle_socket(i);
					close(i);
        			FD_CLR(i, &rset);			
				}
			}
		}		
	}
}

void fork_socket(int port)
{
    printf("fork socket.\n");

    int pid, listenfd=0, socketfd;
    socklen_t length;
    static struct sockaddr_in cli_addr;
    static struct sockaddr_in serv_addr;
    bzero(&serv_addr,sizeof(serv_addr));

    /* 背景繼續執行 */
    //if(fork() != 0)return 0;

    /* 讓父行程不必等待子行程結束 */
    signal(SIGCLD, SIG_IGN);

    /* 開啟網路 Socket */
    if ((socketfd=socket(PF_INET, SOCK_STREAM,0))<0)
        exit(3);
    
    /* 網路連線設定 */
    serv_addr.sin_family = AF_INET;
    /* 使用任何在本機的對外 IP */
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    /* 使用 8888 Port */
    serv_addr.sin_port = htons(port);

    /* 開啟網路監聽器 */
    bind(socketfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr));
        
    /* 開始監聽網路 */
    listen(socketfd,64);

    while(1) 
    {
        length = sizeof(cli_addr);
        /* 等待客戶端連線 */
        if ((listenfd = accept(socketfd, (struct sockaddr *)&cli_addr, &length))<0)
            exit(3);

        /* 分出子行程處理要求 */
        if ((pid = fork()) < 0) {
            exit(3);
        } else {
            if (pid == 0) {  /* 子行程 */
                close(socketfd);
                handle_socket(listenfd);
                break;
            } else { /* 父行程 */
                close(listenfd);
            }
        }
    }
}

int main(int argc, char **argv)
{
	int port = 8888;
	int choose_mode = 0; 
    //0: fork
    //1: select
    if(argc!=1)
    {
        if(strcmp(argv[1],"-f") == 0)
                choose_mode = 0;
        else if(strcmp(argv[1],"-s") == 0)
                choose_mode = 1;
        else
            return 0;
    }
	
    fprintf(stderr, "Server port is : %d\n",port);
	if(choose_mode) 
        select_socket(port);
	else 
        fork_socket(port);

    return 0;
}