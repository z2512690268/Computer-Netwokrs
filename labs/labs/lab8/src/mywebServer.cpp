#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <regex>
#include <unistd.h>
//socket库
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define MAXBUFSIZE 1024    //读入的buffer的大小
const std::string rootPath = "./"; //根目录

void* clientHandler(void* cfd); //客户端处理线程

//webServer class
class  webServer
{
private: 
    int sockfd;         //服务器socket文件描述符
    sockaddr_in sin;    //服务器socket结构体
    const int MAXCLIENT = 16; //允许并发处理的最大线程数
   
public:
    webServer();
    ~webServer();
    void start(); //启动线程  
};

//main函数
int main()
{
    webServer mywebServer; //服务器class对象
    mywebServer.start();     //服务器线程--其实这里是函数调用-主线程
}

webServer::webServer()
{
    //创建socket 绑定端口 侦听
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(3499); //端口号-学号
    sin.sin_addr.s_addr = htonl(INADDR_ANY); //IP地址
    bind(sockfd, (sockaddr*)&sin, sizeof(sin)); //绑定端口
    listen(sockfd, MAXCLIENT); //进入侦听
}

//析构函数 关闭服务器线程
webServer::~webServer()
{
    close(sockfd);
}


void webServer::start() 
{
    std::cout << "begin listening\n";
    //每收到一个请求 就创建一个新线程进行处理
    while(true)
    {
        sockaddr_in client; //client socket结构体
        unsigned int length = sizeof(client);
        int connection_fd = accept(sockfd, (sockaddr*)&client, (socklen_t*)&length);
        //服务器端输出连接者的连接信息
        std::cout<<inet_ntoa(client.sin_addr)<<":"<<ntohs(client.sin_port)<<" connected.\n";
        //创建新线程 进行处理
        //pthread_t closeThread;
        //pthread_create(&closeThread, nullptr, listenQuitThread, &sockfd); //此部分单独线程监听服务器退出信号-效果较差-响应慢
        pthread_t cilentThread;
        pthread_create(&cilentThread, nullptr, clientHandler,&connection_fd);
    }
}

//客户端处理线程
void* clientHandler(void* cfd)
{   
    int clientSockfd = *(int*)cfd;  //客户端socket描述符
    char buffer[MAXBUFSIZE] = {0};  //接收buffer清空
    std::string request;            //客户请求
    while (true)
    {
        int realSize = recv(clientSockfd, buffer, MAXBUFSIZE, 0); //接收浏览器请求
        request.append(buffer, buffer + realSize); //客户请求有效数据
        // if (buffer[0] == 'P') { //POST
        //     memset(buffer, 0, MAXBUFSIZE);
        //     realSize = recv(clientSockfd, buffer, MAXBUFSIZE, 0); //POST请求数据   
        //     request.append(buffer, buffer + realSize);
        // }
        if (realSize < MAXBUFSIZE)
            break;
    }
    
    std::string reply;      //响应消息
    std::regex whitespace("\\s+");
    //利用正则表达式分隔数据
    std::vector<std::string> tokens(std::sregex_token_iterator(request.begin(), request.end(), whitespace, -1),
                                        std::sregex_token_iterator());
    std::string type = tokens[0]; //请求类型 GET 或者是 POST
    std::string path = tokens[1];
    std::string http = tokens[2];
    std::cout << "type is" << type << std::endl;
    std::cout << "path is" << path << std::endl;
    if (type == "GET")
    {
        //按照题目要求, 公布出去的目录为两部分 一部分是/login 另一部分是/image
        //事实上的的本服务器上的根是realroot-然后下面有html img txt
        std::fstream file;
        std::string realPath;
        //采用硬编码方式进行路径映射
        std::string content_type;
        bool notfound = false;  //设置page not found信号为false
        if (path == "/login/noimg.html") {
            //这里采用相对路径-题目要求绝对路径-但这样源代码在不同电脑上就不能跑了-所以这里还是采用相对路径方便验证
            realPath = "./realroot/html/noimg.html";
            content_type = "text/html";
        }
        else if (path == "/login/test.html") {
            realPath = "./realroot/html/test.html";
            content_type = "text/html";
        } 
        else if (path == "/login/test.txt") {
            realPath = "./realroot/txt/test.txt";
            content_type = "text/plain";
        }
        else if (path == "/image/logo.jpg") {
            realPath = "./realroot/img/logo.jpg";
            content_type = "image/jpeg";
        }
        else {
            notfound = true;
        }
        //对应路径正确
        if(!notfound) {
            //打开文件
            file.open(rootPath + realPath, std::ios::in | std::ios::binary);
            std::istreambuf_iterator<char> begin(file), end;
            std::string fileData(begin, end); //文件数据内容
            file.close();
            //设置对应的head和body
            std::string content_length = std::to_string(fileData.size());//字节！
            reply.append("HTTP/1.1 200 OK\n");  //还有疑惑的是-到底是\r\n还是就\n
            reply.append("Content-Type: " + content_type + "\n");
            reply.append("Content-Length: " + content_length + "\n\n");
            reply.append(fileData);
            reply.append("\n");
        }
        else {
            //404 page not found
            std::string response = "<html><body><h1>404 Page not found</h1></body></html>\n";
            reply.append("HTTP/1.1 404 Not Found\n");
            reply.append("Server: Yifan\n");
            reply.append("Content-Type: text/html\n");
            reply.append("Content-Length: " + std::to_string(response.size()) + "\n\n"); 
            reply.append(response);
            reply.append("\n");
        }
    }
    else if (type == "POST") //如果上POST请求
    {
        //解析form表单数据
        //form表单数据格式为 变量名1=变量值1&变量名2=变量值2

        std::string name = "";
        std::string passwd = "";
        std::string data = request.substr(request.find("\r\n\r\n", 0));
        //找到login的位置 找到pass的位置
        int pos_login = data.find("login");
        int pos_pwd = data.find("pass");
        //数据格式: login=namevalue&pass=passvalue
        name = data.substr(pos_login+6, pos_pwd - pos_login - 7);
        passwd = data.substr(pos_pwd + 5);
        
        //响应包的head
        reply.append("HTTP/1.1 200 OK\n");
        reply.append("Server: BaoYifan\n");
        reply.append("Content-Type: text/html\n");
        //回答结果正确
        if (name == "Yifan" && passwd == "123456" ) {
            std::string response = "<html><body>Login Succeed, Yifan!</body></html>\n";
            reply.append("Content-Length: " + std::to_string(response.size()) + "\n\n");
            reply.append(response);
            reply.append("\n");
        }
        else { //结果回答错误
            std::string response = "<html><body>failed</body></html>\n";
            reply.append("Content-Length: " + std::to_string(response.size()) + "\n\n");
            reply.append(response);
            reply.append("\n");
        }
    }
    //std::cout << "the reply is:" << reply << std::endl;
    //发送数据
    send(clientSockfd, reply.c_str(), reply.size(), 0);
    //关闭client socket
    close(clientSockfd); 
    return nullptr;
}

