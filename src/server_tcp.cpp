#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<arpa/inet.h>
#include"threadpool.h"
#include<sys/epoll.h>

typedef struct sockinfo {
	int cfd;
}cominfo;
typedef struct poolinfo {
	ThreadPool* p;
	int fd;
	int epfd;
}poolinfo;
struct epoll_event events[1024];
struct epoll_event ev[100];
int count_event;
void communicate(void* arg);
void accept(void* arg);
char response_forbiden[15] = { '4','0','3',' ','f','o','r','b','i','d','d','e','n','\r','\n'};
char response_ok[8] = { '2','0','0',' ','o','k' ,'\r','\n'};
char response_notfound[15] = { '4','0','4',' ','N','o','t',' ','F','o','u','n','d','\r','\n' };
char content_length[15] = { 'c','o','n','t','e','n','t','-','l','e','n','g','t','h',':' };
char file[5] = { 'm','y','w','e','b' };
char end[2] = { '\r','\n' };
char welcom[11] = { 'w','e','l','c','o','m','e',' ','t','o' ,' '};
char default_p[7] = { 'd','e','f','a','u','l','t' };
char page[6] = { ' ','p','a','g','e','\n' };
char visit[6] = { 'v','i','s','i','t',' '};
struct response {
	char *line;
	char* content_length;
	char server[12] = { 's','e','r','v','e','r',':','z','l','j','\r','\n'};
	char empty[2] = { '\r','\n' };
	char *body =NULL;
};

void get_line(char* line, char* end_temp, int len,char a[])
{
	int count = 0;
	end_temp++;
	for (int i = 0; i < len; i++)
	{
		if (*end_temp != '\n' && i < 9)
			if (*(end_temp) != '\r')
				line[i] = *(end_temp++);
			else
			{
				line[i] = ' ';
				end_temp++;
			}
		else
			line[i] = a[count++];
	}
}

void get_contentlength(char *content,int length,char *len,int lensize)
{
	int count = 0,count_len=0;
	for (int i = 0; i < length; i++)
	{
		if (i < 15)
			content[i] = content_length[i];
		else if (i<15+lensize)
			content[i] = len[count_len++];
		else
		 content[i] = end[count++];
	}
}

void get_body(char* body, int len,char *a,int a_len,char b[])
{
	int count_a = 0, count_b = 0;
	for (int i = 0; i < len; i++)
	{
		if (i < 11)
			body[i] = welcom[i];
		else if (i < 11+a_len)
		{
			body[i] = a[count_a++];
		}
		else
		{
			body[i] = b[count_b++];
		}
	}
}

char* general_url(char* start_temp, char* end_temp, char* buff)
{
	int len_str = end_temp - start_temp;
	int char_length = len_str + 6;
	char* url = (char*)malloc(sizeof(char) * char_length);
	memset(url, 0, char_length);
	strncpy(url, start_temp, end_temp - start_temp);
	for (int i = len_str; i >= 0; i--)
		url[i + 5] = url[i];
	for (int i = 0; i < 5; i++)
		url[i] = file[i];
	url[char_length - 1] = '\0';
	char* ptr = strstr(url, "?");
	if (ptr != NULL)
	{
		for (int i = ptr - url; i < char_length; i++)
		{
			url[i] = '\0';
		}
	}
	return url;
}

void not_exit(struct response getre,char*end_temp,int cfd)
{
	getre.line = (char*)malloc(sizeof(char) * 24);
	get_line(getre.line, end_temp, 24, response_notfound);
	send(cfd, getre.line, 24, 0);
	free(getre.line);
	char a = { '0' };
	getre.content_length = (char*)malloc(sizeof(char) * 18);
	get_contentlength(getre.content_length, 18, &a, 1);
	send(cfd, getre.server, 12, 0);
	send(cfd, getre.content_length, 18, 0);
	send(cfd, getre.empty, 2, 0);
	free(getre.content_length);
}
void forbidden(struct response getre,char *end_temp,int cfd)
{
	getre.line = (char*)malloc(sizeof(char) * 24);
	get_line(getre.line, end_temp, 24, response_forbiden);
	send(cfd, getre.line, 24, 0);
	free(getre.line);
	char a = '0';
	getre.content_length = (char*)malloc(sizeof(char) * 18);
	get_contentlength(getre.content_length, 18, &a, 1);
	send(cfd, getre.server, 12, 0);
	send(cfd, getre.content_length, 18, 0);
	send(cfd, getre.empty, 2, 0);
	free(getre.content_length);
}

void post_contlength(char* pointer,struct response getre,int cfd)
{
	char* temp_count= pointer;
	int count = 0;
	while (*temp_count++ != '\n')
		count++;
	getre.content_length = (char*)malloc(count + 1);
	char* temp = getre.content_length;
	while (*pointer != '\n')
	{
		*temp= *pointer;
		temp ++;
		pointer++;
	}
	*temp = '\n';
	send(cfd, getre.content_length, count + 1, 0);
	free(getre.content_length);
}
void post_body(char* pointer, struct response getre, int cfd,int len)
{
	getre.body = (char*)malloc(sizeof(char) * len);
	for (int i = 0; i < len; i++)
		getre.body[i] = *pointer++;
	send(cfd, getre.body, len, 0);
	free(getre.body);
}
int serverTcp() {
	// 创建监听套接字
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		perror("socket");
		return -1;
	}

	// 绑定本地 IP 和端口
	struct sockaddr_in saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(6969); // 指定端口号
	saddr.sin_addr.s_addr = INADDR_ANY; // 服务器监听所有网络接口
	int ret = bind(server_fd, (struct sockaddr*)&saddr, sizeof(saddr));
	if (ret == -1) {
		perror("bind");
		close(server_fd);
		return -1;
	}
	// 开始监听连接请求
	ret = listen(server_fd, 66); // 设置最大同时连接数为 66
	if (ret == -1) {
		perror("listen");
		close(server_fd);
		return -1;
	}
    //创建epoll实例
	int epoll_fd = epoll_create(1);//参数对其没有任何影响，但是要求大于0
	if (epoll_fd == -1)
	{
		close(server_fd);
		perror("epoll is failed");
		return -1;
	}
	ev[count_event].events = EPOLLIN;//检测读事件
	ev[count_event].data.fd = server_fd;
	int judge = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev[count_event++]);
	if (judge == -1)
	{
		perror("epoll_ctl");
		close(server_fd);
		return -1;
	}
	//创建线程池
	ThreadPool* pool = ThreadPool_Init(8, 2, 100);
	poolinfo* pthreadinfo = (poolinfo*)malloc(sizeof(poolinfo));
	pthreadinfo->p = pool;
	pthreadinfo->fd = server_fd;
	pthreadinfo->epfd = epoll_fd;
	threadpool_add(pool, accept, pthreadinfo);
	pthread_exit(NULL);
	return 0;
}



void accept(void* arg)
{
	poolinfo* acceptinfo = (poolinfo*)(arg);
	// 接受客户端连接请求
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr); // 初始化长度
	int size = sizeof(events) / sizeof(events[0]);
	while (true)
	{
		int num=epoll_wait(acceptinfo->epfd, events, size, -1);//-1代表有文件表示符就绪，返回值num是返回的就绪的文件表示符的总个数
		for (int i = 0; i < num; i++)
		{
			int num_fd = events[i].data.fd;
			if (num_fd == acceptinfo->fd)
			{
				int cfd = accept(acceptinfo->fd, (struct sockaddr*)&addr, &len);
				if (cfd == -1)
				{
					perror("accept");
				}
				else 
				{
					pthread_mutex_t mutexbusy;
					pthread_mutex_lock(&mutexbusy);
					ev[count_event].events = EPOLLIN;//检测读事件
					ev[count_event].data.fd = cfd;
					int judge = epoll_ctl(acceptinfo->epfd, EPOLL_CTL_ADD, cfd, &ev[count_event++] );
					if (judge == -1)
					{
						perror("epoll_ctl");
						close(cfd);
					}
					else
					{
						// 打印客户端 IP 地址和端口号
						char clie_Ip[INET_ADDRSTRLEN];
						printf("client ip: %s    port: %d\n", inet_ntop(AF_INET, &addr.sin_addr, clie_Ip, sizeof(clie_Ip)), ntohs(addr.sin_port));
					}
					pthread_mutex_unlock(&mutexbusy);
				}
			}
			else
			{
				int judge = epoll_ctl(acceptinfo->epfd, EPOLL_CTL_DEL, num_fd, NULL);
				if (judge == -1)
				{
					perror("epoll_ctl");
				}
				else
				{
					cominfo* communication = (cominfo*)malloc(sizeof(poolinfo));
					communication->cfd = num_fd;
					threadpool_add(acceptinfo->p, communicate, communication);
				}
			}
		}
		
	}
	close(acceptinfo->fd);
}
	

void communicate(void*arg)
{
	cominfo* communicate_info = (cominfo*)(arg);
	// 循环接收数据
	while (1) {
		char buff[1024];
		char get_compare[4] = { 0 };
		int len_judge=recv(communicate_info->cfd, get_compare, sizeof(get_compare), 0);
		if (len_judge == 0)
		{
			printf("link has breaked\n");
			break;
		}
		char temp_char = get_compare[3];
		get_compare[3] = '\0';
		
		if (strcmp("GET\0", get_compare)==0)
		{
			get_compare[3] = temp_char;
			printf("%s", get_compare);
			int len = recv(communicate_info->cfd, buff, sizeof(buff), 0);
			printf("%s", buff);
			if (len > 0)
			{
				char* start_temp = buff;
				char* end_temp = strstr(buff, " HTTP");
				char* url = general_url(start_temp, end_temp, buff);
				//处理回复
				struct response getre = {};
					int judge = access(url, 0);//判断文件是否存在
					if (judge == 0)
					{
						if (strstr(url, "/security/") != NULL)
						{
							forbidden(getre, end_temp, communicate_info->cfd);
						}
						else if ((strcmp("myweb/", url) == 0))
						{
							getre.line = (char*)malloc(sizeof(char) * 17);
							get_line(getre.line, end_temp, 17,response_ok);
							send(communicate_info->cfd, getre.line, 17, 0);
							free(getre.line);
							getre.body = (char*)malloc(sizeof(char) * 24);
							get_body(getre.body, 24, default_p, sizeof(default_p), page);
							char len[2] = { '2','4' };
							getre.content_length = (char*)malloc(sizeof(char) * 19);
							get_contentlength(getre.content_length, 19, len, 2);
							send(communicate_info->cfd, getre.server, 12, 0);
							send(communicate_info->cfd, getre.content_length, 19, 0);
							send(communicate_info->cfd, getre.empty, 2, 0);
							send(communicate_info->cfd, getre.body, 24, 0);
							free(getre.content_length);
							free(getre.body);
						}
						else
						{
							getre.line = (char*)malloc(sizeof(char) * 17);
							get_line(getre.line, end_temp, 17, response_ok);
							send(communicate_info->cfd, getre.line, 17, 0);
							free(getre.line);
							int count_end = 0;
							while (url[++count_end] != '\0');
							int count_front=count_end;
							while (url[--count_front] != '/');
							int count_total = count_end - count_front - 1;
							char* filetype = (char*)malloc(sizeof(char) *(count_total+1) );
							for (int i = 0; i < count_total; i++)
							{
								filetype[i] = url[count_front + i + 1];
							}
							filetype[count_total] ='\n';
							int lenbody = count_total + 18;
							getre.body = (char*)malloc(sizeof(char) * lenbody);
							get_body(getre.body, lenbody, visit, sizeof(visit), filetype);
							int tempbody = lenbody;
							int count_body = 0;
							while (tempbody != 0)
							{
								count_body += 1;
								tempbody=tempbody / 10;
							}
							tempbody = lenbody;
							int index_lenbody = count_body - 1;
							char* len_body = (char*)malloc(sizeof(char) * count_body);
							while (tempbody != 0)
							{
								len_body[index_lenbody--] = char(tempbody % 10 + 48);
								tempbody /= 10;
							}
							getre.content_length = (char*)malloc(sizeof(char) * (17+count_body));
							get_contentlength(getre.content_length,17+count_body , len_body, count_body);
							send(communicate_info->cfd, getre.server, 12, 0);
							send(communicate_info->cfd, getre.content_length, 17+count_body, 0);
							send(communicate_info->cfd, getre.empty, 2, 0);
							send(communicate_info->cfd, getre.body, lenbody, 0);
							free(getre.content_length);
							free(getre.body);
							free(len_body);
							free(filetype);
						}
					}
					else
					{
						not_exit(getre, end_temp, communicate_info->cfd);
					}
					free(url);
	
			}
			else if (len == 0)
			{
				printf("link has breaked\n");
				break;
			}
			else
			{
				perror("receive");
				break;
			}
		}
		//post解析
		else
		{
			get_compare[3] = temp_char;
			printf("%s ", get_compare);
			int len = recv(communicate_info->cfd, buff, sizeof(buff), 0);
			buff[len] = '\n';
			for (int i = 0; i <=len; i++)
				printf("%c", buff[i]);
			if (len > 0)
			{
				char* start_temp = buff+1;
				char* end_temp = strstr(buff, " HTTP");
				char* url = general_url(start_temp, end_temp, buff);
				//处理回复
				struct response getre = {};
				int judge = access(url, 0);//判断文件是否存在
				if (judge == 0)
				{
					if (strstr(url, "/security/") != NULL)
					{
						forbidden(getre, end_temp, communicate_info->cfd);
					}
					else 
					{
						getre.line = (char*)malloc(sizeof(char) * 17);
						get_line(getre.line, end_temp, 17, response_ok);
						send(communicate_info->cfd, getre.line, 17, 0);
						free(getre.line);
						char* pointer = strstr(buff, "Content-Length:");
						send(communicate_info->cfd, getre.server, 12, 0);
						post_contlength(pointer, getre, communicate_info->cfd);
						send(communicate_info->cfd, getre.empty, 2, 0);
						pointer = strstr(buff, "\r\n\r\n");
						pointer = pointer + 4;
						int gap = (buff + len) - pointer;
						post_body(pointer, getre, communicate_info->cfd, gap);
					}
				}
				else
				{
					not_exit(getre, end_temp, communicate_info->cfd);
				}
				free(url);

			}
			else if (len == 0)
			{
				printf("link has breaked\n");
				break;
			}
			else
			{
				perror("receive");
				break;
			}
		}
	}
	close(communicate_info->cfd);
}

