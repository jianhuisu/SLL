#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h> // 网络地址转换
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h> // 信号集
#include <string.h>

int main(int argc, const char * argv[]) {

    int server_fd;
    struct sockaddr_in server_addr;

    /*
     参数说明
        SOCK_STREAM  tcp
        SOCK_DGRAM udp    视频 音频 （允许部分失真）

     函数介绍

     socket函数可以创建一个套接字。默认情况，内核会认为socket函数创建的套接字是
     主动套接字（active socket），它存在于一个连接的客户端。
     而服务器调用listen函数告诉内核，该套接字是被服务器而不是客户端使用的，
     即listen函数将一个主动套接字转化为监听套接字（下文以 listenfd 表示）。监听套接字可以接受来自客户端的连接请求。

     如果想要给它赋值一个地址，就必须调用bind()函数，否则就当调用connect()、listen()时系统会自动随机分配一个端口。

     */
    server_fd = socket(AF_INET,SOCK_STREAM,0);

    // 申请内存 地址清零/初始化
    memset(&server_addr,0,sizeof(server_addr));

    /*
        获取IPV4的地址信息；
     */
    server_addr.sin_family = AF_INET;

    /*
        将监听端口的整型变量从主机字节顺序转变成网络字节顺序

        字节序问题只有在读取的时候才会出现

        大端法  人类习惯的表示方式。12345 一万两千三百四十五
        小端法  计算机电路从低位开始处理效率高 五万四千三百二十一

     在设定地址之前务必对地址 进行 主机字节序到网络字节序的转换,否则会引起
     todo 网络上进行 字符串/文件传输时 是否需要进行字节序转换 ？ 我理解的是 需要 ，这样 不论数据的发送或者接收 都需要进行字节序转换

     */
    server_addr.sin_port   = htons(9100);

    /*
     如果你的服务器有多个网卡（每个网卡上有不同的IP地址），而你的服务（不管是在udp端口上侦听，还是在tcp端口上侦听），出于某种原因：
     可能是你的服务器操作系统可能随时增减IP地址，也有可能是为了省去确定服务器上有什么网络端口（网卡）的麻烦 —— 可以要在调用bind()的时候，告诉操作系统：“我需要在 yyyy 端口上侦听，所有发送到服务器的这个端口，不管是哪个网卡/哪个IP地址接收到的数据，都是我处理的。”这时候，服务器程序则在0.0.0.0这个地址上进行侦听。例如：

          回环地址 127.0.0.1 ： 80 端口
     局域网地址 172.16.225.67 : 80 端口
     外网接口 111.198.190.132 ：80 端口

     不论外部信息发送到上述三个IP中任意一个的 80端口 都可以被 server接收

     127.0.0.1是一个特殊的IP地址，表示本机地址，如果绑定到这个地址，客户端必须同时在本机运行才能连接，也就是说，外部的计算机无法连接进来。

     因为 tcp udp 都具有自己的端口号（范围均为 0-2^16-1） 所以 udp :80 是不同于 tcp:80 的地址

     所以 利用三元组（ip地址，协议，端口）就可以标识唯一的网络的进程

     */

    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* todo 为什么这里不能用 (struct sockaddr_in *) &server_addr
     Incompatible pointer types passing 'struct sockaddr_in *' to parameter of type 'const struct sockaddr *'

     由调度程序
     目标进程 发送进程

     将ip+port 与 进程的fd之间的映射关系注册到 管理进程维护的表中

     */
    if( bind(server_fd,(struct sockaddr *) &server_addr,sizeof(struct sockaddr_in) ) < 0 ){
        perror("socket bind error");
    }

    /*

     sockfd 一个已绑定未被连接的套接字描述符
     backlog 连接请求队列(queue of pending connections)的最大长度

        listen函数使用主动连接套接字变为被连接套接口，使得一个进程可以接受其它进程的请求，从而成为一个服务进程。

         (1) 执行listen 之后套接字进入被动模式。
         (2) 队列满了以后，将拒绝新的连接请求。客户端将出现连接D 错误WSAECONNREFUSED。
         (3) 在正在listen的套接字上执行listen不起作用

     */
    if(listen(server_fd,4) < 0 ){
        perror("listen op fail");
    }

    /*
        一个进程最多允许操作 1024个文件,一个fd代表一个文件
         并且 fd1 与fd2 默认分配给 stdin stdout
         所以 server fd 一般会从 3 开始

         [root@vagrant-centos65 IMserver]$>t server.c
          server start success fd is : 3
          wait connect ...

    */
    printf(" server start success fd is : %d \n",server_fd);

    // todo 如何使该进程作为daemon运行
    /*
        采用多进程的方式 可同时处理多个来自于不同客户端的 socket 连接。

        父进程进行监听,分配 子进程进行实际的 交互

         一般的，父进程在生成子进程之后会有两种情况：一是父进程继续去做别的事情，类似上面举的例子；另一是父进程啥都不做，一直在wait子进程退出。
         SIGCHLD信号就是为这第一种情况准备的，它让父进程去做别的事情，而只要父进程注册了处理该信号的函数，在子进程退出时就会调用该函数，
         在函数中wait子进程得到终止状态之后再继续做父进程的事情。

         最后，我们来明确以下二点：
         1)凡父进程不调用wait函数族获得子进程终止状态,回收子进程,子进程在退出后都会变成僵尸进程。
         2)SIGCHLD信号可以异步的通知父进程有子进程终止

        在进程树中 子进程终止后,会向父进程发送一个信号,通知父进程回收子进程的进程标示符。进程如何处理信号有三种选择。

     1）忽略该信号。有些信号表示硬件异常，例如，除以0或访问进程地址空间以外的单元等，因为这些异常产生的后果不确定，所以不推荐使用这种处理方式。

     2）按系统默认处理方式。

     3）提供一个函数，信号发生时调用这个函数，成为捕捉该信号。以实现按用户自定义的方式来处理信号。

        具体有以下几种情况

         父进程在子进程退出时仍在运行

            父进程wait() 子进程退出信号,回收子进程
            父进程提前注册 信号处理函数, 异步处理子进程
            父进程不负责对子进程进行回收，转交由内核（init进程 ，pid 为 1 ） 代为处理

        父进程先于子进程终止

            子进程退出后无进程负责回收,变成僵尸进程，占据一个进程id号

     */
    // linux 下 SIGCLD 就是 SIGCHLD
    signal(SIGCHLD,SIG_IGN);

    socklen_t client_len;
    int client_fd;
    struct sockaddr_in client_addr;
    char buffer[140];
    pid_t pid; // 子进程的 pid

    // todo 如何使 daemon 运行
    while(1){

        printf("wait connect  \n");
        client_len = sizeof(client_addr);

        /*
            主动阻塞主进程的执行 直至监听端口接收到客户端数据  唤醒主进程
            ?? client_fd 才是真正与客户端进行通讯的socket文件标示 server_fd socket文件中应该保存的是 客户端连接请求队列
            两点确定一条通信通道

            s：标识一个未连接socket
            name：指向要连接套接字的sockaddr结构体的指针
            namelen：sockaddr结构体的字节长度


         */
        if( (client_fd = accept(server_fd,(struct sockaddr *) &client_addr,&client_len)) == -1 ){
            perror("get connect from queue faild");
        }

        /*
            子进程是父进程的副本，它将获得父进程数据空间、堆、栈等资源的副本。注意，子进程持有的是上述存储空间的“副本”，这意味着父子进程间不共享这些存储空间。
            无法确定fork之后是子进程先运行还是父进程先运行，这依赖于系统的实现。所以在移植代码的时候我们不应该对此作出任何的假设
         */

        pid = fork();

        // 子进程
        if(pid == 0){

            // inet_ntoa 将一个32位网络字节序的二进制IP地址转换成相应的点分十进制的IP地址
            printf("server:get connect from %s port: %d \n",inet_ntoa(client_addr.sin_addr),client_addr.sin_port);
            send(client_fd,"hello \n",13,0);

             /*

               如果s的发送缓冲中没有数据或者数据被协议成功发送完毕后，

               recv先检查套接字s的接收缓冲区，

               1 如果s接收缓冲区中没有数据或者协议正在接收数据，那么recv就一直等待(阻塞、中断)，直到协议把数据接收完毕。当协议把数据接收完毕，recv函数就把s的接收缓 冲中的数据copy到buf中

               2 当协议把数据接收完毕,s接收缓冲区中数据已经可读,recv函数就把s的接收缓冲中的数据copy到buf中(s的接收缓冲属于系统临界资源,使用完毕后要立即释放)
                （
                      注意协议接收到的数据可能大于buf的长度，所以在这种情况下要调用几次recv函数才能把 s的接收缓冲中的数据全部copy完。
                      recv函数仅仅是copy数据，真正的接收数据是协议来完成的
                  ）
                 (如果copy不完是不是意味着不会释放这缓冲区)

                    recv函数返回 实际copy的字节数。
                    如果recv在copy时出错，那么它返回SOCKET_ERROR；
                    如果recv函数在等待协议接收数据时网络中断了，那么它返回0

               3  (这种recv()的调用方式是阻塞调用)

               todo 如何区分阻塞套接字 与 非阻塞套接字






             */

            int  continueRecv = 1; // socket 对话是否需要继续
            char buff[100];        // 应用的数据接收缓存区
            size_t  buflen;        // 缓存区最大容量

            while(continueRecv)
            {

                buflen = recv(client_fd, buff, sizeof(buff), 0);

                //  SOCKET_ERROR 未定义 这个Linux的库函数里没有? win 下使用 ?
                if(buflen < 0 ){

                    continueRecv = 0;
                    perror("SOCKET_ERROR");

                } else if(buflen == 0){

                    // 这里表示对端的socket已正常关闭.
                    continueRecv = 0;
                    perror("network outage");


                } else {

                    if(buflen != sizeof(buff)){
                        printf("数据接收完毕 \n");
                    } else {
                        printf("数据未完全接收 \n");
                    }

                    printf("receive message: %s \n",buff);

                }

            }

            close(client_fd);
            printf("child process exit \n");
            exit(0);

        }else if(pid > 0){
            // 父进程
            printf("fork once \n");
        } else {
            printf("fork fail \n");
        }


    }

}

