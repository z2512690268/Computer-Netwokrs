/* Author: Bao Yifan
   Date: 2021.1.9
   SID: 3180103499
   Description: Seeo Computer Network Lab guide-7
   C++ Version: C++11
   Encoding: UTF-8
   Developing Environment: 
    OS: MacOS Catalina 10.15.5
    Compiler: Apple clang version 11.0.3 (clang-1103.0.32.62)
    Editor: VSCode
*/
#include <stdlib.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <streambuf>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <ctime>
#include <ratio>
#include <chrono>
#include <cstring>

#include <unistd.h>
//Socket 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define SERVER_NAME "BYF"
#define PORT 3499
#define BUFLEN 512
#define CONNECTIONMAX 128

struct Client
{
    char IP[30]; //字符串形式IP地址--点分十进制表示
    int port;
    int id;
    std::thread::id threadID; //可以改用pthread 估计用法类似的
    int sockfd; //客户端的socket fd 句柄
};
//此客户端结构体完整的表示了客户端的信息
//代表了服务器的客户端处理线程与客户端的通讯

std::vector<Client*> clientList; //客户端列表-这里用指针？可能是为了可以修改某些信息
int clientNum = 0;
std::mutex threadLock; //线程锁-互斥


//将std::string转换为char* 类型发送对应数据
void sendString(int clientSockfd, std::string str);
//处理客户端发来的时间请求
void getTime(int clientSockfd);
//处理客户端发来的名字请求
void getServerName(int clientSockfd);
//处理客户端发来的列表请求
void getClientList(int clientSockfd);
//判断某个id的客户端是否仍然在连接列表中！！
bool isAlive(int id);
//搜寻列表中客户端对应的套接字--根据ID在ClientList中查找-返回socket fd
int searchSocket(int id); //id -> socket fd;
//解析客户端发来的数请求数据包--recv, process -> recvBuffer
int processRequestPacket(int clientSockfd, char* recvBuffer);
//客户端处理函数--线程
void clientHandler(int clientSockfd);


int main(){

    //创建套接字
    int serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    //将套接字和IP、端口绑定
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));  //每个字节都用0填充
    serv_addr.sin_family = AF_INET;  //使用IPv4地址
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);//inet_addr("127.0.0.1");  //具体的IP地址
    serv_addr.sin_port = htons(PORT);  //端口
    bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    //进入监听状态，等待用户发起请求
    listen(serv_sock, 20);
    //接收客户端请求
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);
    printf("Begin to handle client.\n");
    while(true) 
    {
        sockaddr_in client; //客户端socket信息结构体--在accept接收的时候赋值
        unsigned int clientAddrLength = sizeof(client);
        
        int clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        //创建子线程 
        std::thread(clientHandler, std::ref(clnt_sock)).detach();
    }
    close(serv_sock);
    return 0;
}


void sendString(int clientSockfd, std::string str)
{
    char packet[BUFLEN] = {0};
    strcpy(packet, str.c_str());
    int len = str.length();
    send(clientSockfd, packet, len, 0); //发送消息
}

//客户端请求处理线程
void clientHandler(int clientSockfd)
{
    char recvBuffer[BUFLEN]={0};
    char* IP = NULL;
    int port;

    sockaddr_in client = {0}; //客户端socket信息结构体
    int clientAddrLength =  sizeof(client);
    
    //获取线程编号--大概是C++的线程方式
    std::thread::id threadID = std::this_thread::get_id();
    
    //获取客户端socket信息结构体
    getpeername(clientSockfd, (struct sockaddr*)&client, (socklen_t*)&clientAddrLength);
    IP = inet_ntoa(client.sin_addr);//ip地址 转化为 点分十进制表示
    port = client.sin_port; //端口号
    printf("connection established: %s\n", IP);

    //Client结构体-存储client信息-用于后面列表的显示
    Client theClient;
    theClient.sockfd = clientSockfd;
    
    strcpy(theClient.IP, IP);
    theClient.port = port;
    theClient.id = clientNum++; //客户的到达顺序即client ID 
    theClient.threadID = threadID;
    
    threadLock.lock();
    clientList.push_back(&theClient);  //直接传地址
    threadLock.unlock();
    //循环调用recv() 等待客户端的请求数据包
    int ret = 0;
    bool isClose = false;
    while (!isClose) 
    {
        memset(recvBuffer, 0, BUFLEN);
        ret = recv(clientSockfd, recvBuffer, BUFLEN, 0);
        if(ret > 0) { //正常情况
            std::cout << "Packet received...\n";
            //处理T N L 类型请求 
            int flag = processRequestPacket(clientSockfd, recvBuffer);

            if (flag == 0)  //处理M类请求 发送信息
            {   //向指定客户端发送信息
                std::string str = recvBuffer;
                int pos1, pos2;
                pos1 = str.find("*") + 1;//找第一个*
                pos2 = str.find("*", pos1); //从第一个*开始找第二个*
                int dstID = std::stoi(str.substr(pos1, pos2 - pos1));//目标ID-传送的时候直接以字符的形式发送的
                pos1 = pos2 + 1;
                pos2 = str.find("#", pos1);//找到结尾的'#'
                std::string message = str.substr(pos1, pos2 - pos1);
                if (isAlive(dstID)) {
					int dstSockfd = searchSocket(dstID); // 获取目标客户端的socket
					//封装指示数据包，转发信息
					std::string response = "#M-M*";
					response += std::to_string(theClient.id);
					response += " ";
					response += theClient.IP;
					response += "\n";
					response += message;
					response += "#\0";
					sendString(dstSockfd, response);
					printf("Send a message to destination client!\n");
					//发送响应数据包，表示发送成功
					response = "#M-Y*";
					response += std::to_string(dstID);
					response += " ";
					response += message;
					response += "#\0";
					sendString(clientSockfd, response);
					printf("Send a message to start client!\n");
				}
				else {
					// 发送响应数据包，表示发送失败
					std::string response = "#M-N*Destination doesn't exist!#";
					sendString(clientSockfd, response);
					printf("Send message failed!\n");
				}

            }

        }
        else if(ret == 0)
        {   //连接中断 
            printf("connection closed!\n");
            isClose = true; 
        }
        else //error 
        { 
            std::cerr << "recv failed! close this socket";
            close(clientSockfd);
            return ;
        }
    }

    //退出循环后--一般是客户端中断连接--这里也需要关闭socket
    close(clientSockfd);
    //在列表中查找当前client-删除
    for (auto it = clientList.begin(); it != clientList.end(); it++)
    {
        if ((*it)->id == theClient.id && (*it)->IP == theClient.IP) 
        {
            threadLock.lock();
            clientList.erase(it);   //删除
            clientNum--;
            threadLock.unlock();
            break;
        }
    }
    return ;
}

int processRequestPacket(int clientSockfd, char* recvBuffer)
{
    if (recvBuffer[0] != '#')
    {
        std::cerr << "wrong packet" << std::endl;
        return -1;
    }
    char type = recvBuffer[1];
    switch (type)
    {
        case 'T':
            getTime(clientSockfd);
            break;
        case 'N':
            getServerName(clientSockfd);
            break;
        case 'L':
            getClientList(clientSockfd);
            break;
        default:
            return 0;
            break;
    }
    return 1;
}

void getTime(int sockfd) 
{
    std::string response = "#T*";
    using std::chrono::system_clock;
    //时间相关处理函数
    system_clock::time_point today = system_clock::now();
    time_t  t;
    t = system_clock::to_time_t(today);
    response += ctime(&t);
    response += "#\0";
    sendString(sockfd, response);
    printf("Time send.\n");
}

void getServerName(int sockfd)
{
    std::string response = "#N*";
	response += SERVER_NAME;
	response += "#\0";
	sendString(sockfd, response);
	printf("Server name send.\n");
    
}

void getClientList(int sockfd)
{
    std::string response = "#L*";
	threadLock.lock();
	std::vector<struct Client*>::iterator it;
	for (auto it = clientList.begin(); it != clientList.end(); it++) {
		response += std::to_string((*it)->id);
		response += ' ';
		response += (*it)->IP;
		response += '\n';
	}
	response += "#\0";
	threadLock.unlock();
	sendString(sockfd, response);
	printf("Client list send.\n");
}

bool isAlive(int id)
{
    bool flag = false;
    threadLock.lock();
    for (auto it = clientList.begin(); it != clientList.end(); it++)
    {
        if ((*it)->id == id)
        {
            flag = true;
            break;
        }
    }
    threadLock.unlock();
    return flag;
}

int searchSocket(int id)
{
    int sockfd;
    threadLock.lock();
    for (auto it = clientList.begin(); it != clientList.end(); it++)
    {
        if((*it)->id ==id) {
            sockfd = (*it)->sockfd;
            break;
        }
    }
    threadLock.unlock();
    return sockfd;
}

