#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <streambuf>
#include <fstream>
#include <string>
#include <map>
#include <mutex>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <thread>
#include <ctime>
#include <ratio>
#include <chrono>
#include <condition_variable>
using namespace std;

//socket编程配置库
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")

#define DEFAULT_BUFFER_LEN 512

//端口号定成byf同学的学号后四位
#define DEFAULT_PORT "3499"


condition_variable my_time;
condition_variable my_name;
condition_variable my_list;
condition_variable my_message;

mutex mutex_time;
mutex mutex_name;
mutex mutex_list;
mutex mutex_message;
mutex mutex_output;

//初始化页面
void startMenu();
//客户端请求获得时间
void get_server_time(char* buffer_send, SOCKET connected_socket);
//客户端请求获得服务器名字
void get_server_name(char* buffer_send, SOCKET connected_socket);
//客户端请求获得客户端列表
void get_client_list(char* buffer_send, SOCKET connected_socket);
//客户端请求发送信息
void send_message(char* buffer_send, SOCKET connected_socket);
//客户端接收消息后，在命令行输出消息
void print_message(string response_content);
//客户端接收信息
int receive_packet(SOCKET connected_socket);
//客户端请求与服务端断开连接
void break_connection(SOCKET connected_socket);
//客户端请求退出
void dropout(SOCKET connected_socket);

int main()
{
    struct addrinfo* result = NULL;
    struct addrinfo* ptr = NULL;
    struct addrinfo hints;
    WORD wVersionRequested;
    WSADATA wsaData;
    SOCKET connected_socket = INVALID_SOCKET;
    char buffer_send[DEFAULT_BUFFER_LEN]; //要发送的信息
    ZeroMemory(buffer_send, DEFAULT_BUFFER_LEN);
    int recv_buflen = DEFAULT_BUFFER_LEN; //收到的信息
    char ip[DEFAULT_BUFFER_LEN];
    char version;

    while (1)
    {
        int option_result;
        startMenu();//初始界面
        int option;
        cin >> option;
        if (option == 2 || option == 7) //用户退出
        {
            return 0;
        }
        else if (option == 1) //连接服务器
        {
            printf("please input the server ip:\n");
            cin >> ip;
            if (ip)
                cout << "usage: " << ip << " server-name" << endl;

            //Winsock初始化
            wVersionRequested = MAKEWORD(2, 2); //希望使用的WinSock DLL的版本
            option_result = WSAStartup(wVersionRequested, &wsaData);
            if (option_result != 0)
            {
                cout << "WSAStartup failed with error: " << to_string(option_result) << endl;
                system("pause");
                continue;
            }

            //确认WinSock DLL支持版本2.2：
            if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
            {
                WSACleanup();
                printf("Invalid Winsock version!\n");
                version = getchar();
                return 0;
            }

            //构建本地地址信息
            ZeroMemory(&hints, sizeof(hints));
            hints.ai_family = AF_UNSPEC;     //地址家族
            hints.ai_socktype = SOCK_STREAM; //socket类型
            hints.ai_protocol = IPPROTO_TCP; //protocol

            //解析服务器端口和地址
            option_result = getaddrinfo(ip, DEFAULT_PORT, &hints, &result);
            if (option_result != 0)
            {
                cout << "getaddrinfo failed with error: " << to_string(option_result) << endl;
                WSACleanup();
                continue;
            }
            int flag = 0;
            for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
            {
                //创建用于连接服务器的SOCKET，使用TCP协议
                connected_socket = socket(ptr->ai_family, ptr->ai_socktype,
                    ptr->ai_protocol);
                if (connected_socket == INVALID_SOCKET)
                {
                    cout << "socket failed with error: " << to_string(WSAGetLastError()) << endl;
                    WSACleanup();
                    flag = 1;
                    break;
                }

                //连接到服务器
                option_result = connect(connected_socket, ptr->ai_addr, (int)ptr->ai_addrlen);
                if (option_result == SOCKET_ERROR)
                {
                    closesocket(connected_socket);
                    connected_socket = INVALID_SOCKET;
                    continue;
                }
                break;
            }
            if (flag == 1)
            {
                continue;
            }
            else
            {
                freeaddrinfo(result);
                if (connected_socket == INVALID_SOCKET)
                {
                    cout << "Fail to connect to the server!" << endl;
                    WSACleanup();
                    continue;
                }
                else
                {   //连接成功
                    cout << "Succeed to get connected!" << endl;
                    char* IP = NULL;
                    int port = 0;

                    //获得客户端信息
                    thread::id this_id = this_thread::get_id();
                    SOCKADDR_IN clientInfo = { 0 };
                    int addr_size = sizeof(clientInfo);

                    //获得当前ip与port
                    getpeername(connected_socket, (struct sockaddr*) & clientInfo, &addr_size);
                    IP = inet_ntoa(clientInfo.sin_addr);
                    port = clientInfo.sin_port;
                    cout << "the current IP is: " << IP << endl;
                    cout << "the current port is:" << port << endl;

                    thread(receive_packet, move(connected_socket)).detach(); //接收数据

                    while (true)
                    {
                        startMenu();
                        int ooption;
                        cin >> ooption;
                        if (ooption == 2)
                        { //请求结束连接
                            break_connection(connected_socket);
                            break;
                        }
                        else if (ooption == 3)
                        { //请求获得时间
                            get_server_time(buffer_send, connected_socket);
                            continue;
                        }
                        else if (ooption == 4)
                        { //请求获得服务器名字
                            get_server_name(buffer_send, connected_socket);
                            continue;
                        }
                        else if (ooption == 5)
                        { //请求获得客户端列表
                            get_client_list(buffer_send, connected_socket);
                            continue;
                        }
                        else if (ooption == 6)
                        { //请求向其它客户端发送信息
                            send_message(buffer_send, connected_socket);
                            continue;
                        }
                        else if (ooption == 7)
                        {//请求退出
                            dropout(connected_socket);
                            return 0;
                        }
                        else
                        {
                            continue;
                        }
                    }
                    continue;
                }
            }
        }
        //所有的请求发送都要在连接后进行
        else
        {
            cout << "You have to connect to a server first." << endl;
            continue;
        }
    }
}

//开始界面.
void startMenu()
{
    cout << "Please input the number to choose the operation:" << endl;
    cout << "1. get connected." << endl;
    cout << "2. break the connection." << endl;
    cout << "3. get server time." << endl;
    cout << "4. get server name." << endl;
    cout << "5. get the client list." << endl;
    cout << "6. send message to others." << endl;
    cout << "7. exit." << endl;
    cout << "" << endl;
}

//客户端接收消息后输出消息.
void print_message(string output_content)
{
    //要先对output加锁，然后解锁
    mutex_output.lock(); //对互斥量加锁
    cout << output_content << endl;//在命令行输出
    mutex_output.unlock(); //对互斥量解锁
}


//客户端接收消息
int receive_packet(SOCKET connected_socket)
{
    char recv_buf[DEFAULT_BUFFER_LEN];//接受数据报
    int result;//存储是否成功接受的结果
    int recv_buflen = DEFAULT_BUFFER_LEN;//接受数据报长度
    do
    {
        ZeroMemory(recv_buf, DEFAULT_BUFFER_LEN);//全零初始化
        result = recv(connected_socket, recv_buf, recv_buflen, 0); //接收数据，接收到的数据包存在recv_buf中
        if (result > 0)                                          //接收成功，返回接收到的数据长度
        {
            int position1 = 0;//接收到的数据包类型的位置，start
            int position2 = 0;

            string recv_packet = recv_buf; //接收到的数据包
            char packet_type;              //接收到的数据包类型
            string response_content;       //接收到的数据包内容

            //得到接受数据包的类型
            position1 = recv_packet.find("#") + 1; //接收到的数据包类型的位置（start）
            packet_type = recv_packet[position1];  //接收到的数据包类型

            //接收到的数据包内容
            position1 = recv_packet.find("*") + 1;   //找到服务器回答的数据的开始位置
            position2 = recv_packet.find("#", position1); //从pos1所在位置开始往后找，找到空格为止

            //判断内容是否为空，如果为空则直接设置为空字符串
            if ((position2 - position1) != 0) { //内容不为空
                response_content = recv_packet.substr(position1, position2 - position1);
            }
            else {//内容为空
                response_content = "";
            }

            switch (packet_type) //对不同类型的数据包做不同处理
            {

            case 'N': //数据包类型为名字
                print_message(response_content);
                my_name.notify_one();
                break;
            case 'L': //数据包类型为客户端列表
                print_message(response_content);
                my_list.notify_one();//随机唤醒一个等待的线程
                break;
            case 'T': //数据包类型为时间
                print_message(response_content);//锁定，输出，解锁
                my_time.notify_one();//随机唤醒一个等待的线程
                break;
            case 'M': //数据包类型为信息
                position1 = recv_packet.find("-") + 1;//信息数据开始的位置
                char type = recv_packet[position1];//信息类型
                switch (type)
                {
                case 'Y': //表示发送消息成功
                    position1 = recv_packet.find("*") + 1;//从*开始，到空格，是接收信息一方的编号，整数
                    position2 = recv_packet.find(" ", position1);
                    int id_request;
                    id_request = stoi(recv_packet.substr(position1, position2 - position1)); //将字符串转化为数字
                    response_content = "the message has been sent to " + to_string(id_request);//输出消息发送成功的提示
                    print_message(response_content);
                    my_message.notify_one();
                    break;
                case 'N': //表示发送消息失败
                    print_message(response_content);
                    my_message.notify_one();
                    break;
                case 'M': //表示收到信息

                    //得到发送信息的客户端ID
                    position1 = recv_packet.find("*") + 1;//发送信息的客户端id以*开始，以空格结束
                    position2 = recv_packet.find(" ", position1);

                    int id_from_server;
                    id_from_server = stoi(recv_packet.substr(position1, position2 - position1));

                    //发送信息的客户端IP
                    position1 = position2 + 1;
                    position2 = recv_packet.find("\n");
                    string ip_from_server;
                    ip_from_server = recv_packet.substr(position1, position2 - position1);

                    //收到的信息，以#结束
                    position1 = position2 + 1;
                    position2 = recv_packet.find("#", position1);
                    string receive_message;
                    receive_message = recv_packet.substr(position1, position2 - position1);

                    //加锁，输出信息传送消息，并解锁
                    mutex_output.lock();
                    cout << "The content of the message from " << to_string(id_from_server) << " " << ip_from_server << " is:" << endl;
                    cout << receive_message << endl;
                    mutex_output.unlock();
                    break;
                }
            }
        }
        else if (result < 0) { //连接结束
            printf("Receipt failed with error: %d\n", WSAGetLastError());
        }
        else { //连接失败
            printf("Connection closed\n");
        }
    } while (result > 0);
    return 0;
}

//关闭连接
void break_connection(SOCKET connected_socket)
{
    int shutdown_ret;
    shutdown_ret = shutdown(connected_socket, SD_SEND);//关闭该socket，在recv端加入接收到的消息，send端接收到应答即可shutdown
    if (shutdown_ret == SOCKET_ERROR)//如果关闭出错
    {
        cout << "Shutdown failed with error: " << to_string(WSAGetLastError()) << endl;
        closesocket(connected_socket);//假设没有写这句话，服务器端会一直处于CLOSE_WAIT状态
        WSACleanup();//最后做实际清除工作
    }
}

//得到服务器时间
void get_server_time(char* send_buffer, SOCKET connected_socket)
{
    string packet = "#T#";
    memcpy(send_buffer, packet.c_str(), packet.size());//拷贝packet里的东西到send_buffer中
    int time_result;
    //time_result = send(connected_socket, send_buffer, (int)strlen(send_buffer), 0);//在已建立连接的套接字上发送数据
    for(int i = 0; i < 100;i++)
	{
		time_result = send(connected_socket, send_buffer, (int)strlen(send_buffer), 0); //得到一次时间则发送100次请求 包含上一次
		Sleep(500); 
	 } 
    unique_lock<mutex> lck(mutex_time);//表示独占所有权
    my_time.wait(lck);//tcp释放连接的四次挥手后的主动关闭连接方的状态

}

//得到服务器名
void get_server_name(char* send_buffer, SOCKET connected_socket)
{
    string packet = "#N#";
    memcpy(send_buffer, packet.c_str(), packet.size());
    int server_result;
    server_result = send(connected_socket, send_buffer, (int)strlen(send_buffer), 0);
    unique_lock<mutex> lck(mutex_name);
    my_name.wait(lck);
}

//得到客户端列表.
void get_client_list(char* buffer_send, SOCKET connected_socket)
{
    string packet = "#L#";
    memcpy(buffer_send, packet.c_str(), packet.size());
    int list_result;
    list_result = send(connected_socket, buffer_send, (int)strlen(buffer_send), 0);
    unique_lock<mutex> lck(mutex_list);
    my_list.wait(lck);
}

//客户端发送消息.
void send_message(char* buffer_send, SOCKET connected_socket)
{
    //得到目标客户端id
    cout << "please input the destination id:" << endl;
    char buffer[1024] = "\0";
    int dst_server_id = 0;
    cin >> dst_server_id;
    cin.getline(buffer, 1024);

    //得到待发送消息
    cout << "please input the message that you want to send:" << endl;
    cin.getline(buffer, 1024);
    string send_message;
    send_message = buffer;

    ///封装数据包
    string send_packet = "#M*" + to_string(dst_server_id) + "*" + send_message + "#";
    ZeroMemory(buffer_send, DEFAULT_BUFFER_LEN);
    memcpy(buffer_send, send_packet.c_str(), send_packet.size());
    int send_result;
    send_result = send(connected_socket, buffer_send, (int)strlen(buffer_send), 0);
    unique_lock<mutex> lck(mutex_message);
    my_message.wait(lck);
}

//退出连接.
void dropout(SOCKET connected_socket)
{
    int shutdown_result = shutdown(connected_socket, SD_SEND);
    if (shutdown_result == SOCKET_ERROR)
    {
        cout << "shutdown failed with error: " << to_string(WSAGetLastError()) << endl;
        closesocket(connected_socket);
        WSACleanup();
    }
}