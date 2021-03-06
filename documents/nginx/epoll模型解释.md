## epoll IO模型

epoll是Linux内核为处理大批量文件描述符而作了改进的poll，是Linux下多路复用IO接口select/poll的增强版本，
它能显著提高程序在大量并发连接中只有少量活跃的情况下的系统CPU利用率。（更确切的描述:注：epoll被认为是linux下性能最好的多路io就绪通知方法）

>多路复用:多路复用是指两个或多个用户共享公用信道的一种机制。通过多路复用技术,多个终端能共享一条高速信道,从而达到节省信道资源的目的.
>这里指的是IO多路复用.IO multiplexing(IO多路复用)IO多路复用,有些地方称之为event driven IO(事件驱动IO)。它的好处在于单个进程可以处理多个网络IO请求.

另一点原因就是获取事件的时候，它无须遍历整个被侦听的描述符集，只要遍历那些被内核IO事件异步唤醒而加入Ready队列的描述符集合就行了。
epoll除了提供select/poll那种IO事件的水平触发（Level Triggered）外，还提供了边缘触发（Edge Triggered），
这就使得用户空间程序有可能缓存IO状态，减少epoll_wait/epoll_pwait的调用，提高应用程序效率。

select 最不能忍受的是一个进程所打开的FD是有一定限制的，由FD_SETSIZE设置，默认值是1024。对于那些需要支持的上万连接数目的IM服务器来说显然太少了。
这时候你一是可以选择修改这个宏然后重新编译服务器代码，不过资料也同时指出这样会带来网络效率的下降，
二是可以选择多进程的解决方案（传统的Apache方案），不过虽然linux上面创建进程的代价比较小，但仍旧是不可忽视的，加上进程间数据同步远比不上线程间同步的高效，
所以也不是一种完美的方案。不过 epoll则没有这个限制，它所支持的FD上限是最大可以打开文件的数目，这个数字一般远大于2048,
举个例子，在1GB内存的机器上大约是10万左右，具体数目可以cat /proc/sys/fs/file-max查看，一般来说这个数目和系统内存关系很大。

IO效率不随FD数目增加而线性下降

传统的select/poll另一个致命弱点就是当你拥有一个很大的socket集合，不过由于网络延时，任一时间只有部分的socket是“活跃”的，但是select/poll每次调用都会线性扫描全部的集合，
导致效率呈现线性下降。但是epoll不存在这个问题，它只会对“活跃”的socket进行操作---这是因为在内核实现中epoll是根据每个fd上面的callback函数实现的。
那么，只有“活跃”的socket才会主动的去调用 callback函数，其他idle状态socket则不会，在这点上，epoll实现了一个“伪”AIO，因为这时候推动力在os内核。

在一些benchmark中，如果所有的socket基本上都是活跃的---比如一个高速LAN环境，epoll并不比select/poll有什么效率，相反，
如果过多使用epoll_ctl,效率相比还有稍微的下降。但是一旦使用idle connections模拟WAN环境，epoll的效率就远在select/poll之上了。

总结:poll是服务主动扫描socket链接集合  而epoll是单个链接主动调用服务. 如何定义什么是活跃的IO连接呢？(活跃的io链接  连接上有数据在传递).

上文说到的:多进程模型与多线程模型各有利弊,多线程模型线程间通讯较多进程高效.但是没有多进程健壮.
线程执行开销小，但不利于资源的管理和保护；而进程正相反。

一个思考题:为什么端口数量会有限制呢

在TCP、UDP协议的开头，会分别有16位来存储源端口号和目标端口号，所以端口个数是

    2^16-1=65535

2的16次方减去1个符号位=65535.由ip协议中的头字段长度限制导致.

进程需要一个fd来标识来自客户端的链接 而不是使用一个端口来标识一个客户端链接,端口用来标识客户端可以。使用端口标识连接很容易因为端口数量限制而不能维护大量链接而造成资源浪费 所以使用fd来替换

1. select （能监控数量有限，不能告诉用户程序具体哪个连接有数据）

    1. select目前几乎在所有的平台上支持，其良好跨平台支持也是它的一个优点

    2. select的一个缺点在于单个进程能够监视的文件描述符的数量存在最大限制，在Linux上一般为1024

    3. select监控socket连接时不能准确告诉用户是哪个，比如：现在用socket监控10000链接，如果其中有一个链接有数据了，select就会告诉用户程序，你有socket来数据了，那样就只能自己循环10000次判断哪个活跃

    2. poll（和select一样，仅仅去除了最大监控数量）

    1. poll和select在本质上没有多大差别，但是poll没有最大文件描述符数量的限制

    2. 可以理解为poll是一个过渡阶段，大家也都不用他

    3. epoll (不仅没有最大监控数量限制，还能告诉用户程序哪个连接有活跃)

   注：epoll被认为是linux下性能最好的多路io就绪通知方法

    1. epoll直到Linux2.6（centos6以后）才出现了由内核直接支持

Epoll没有最大文件描述符数量限制

3. epoll最重要的优点是他可以直接告诉用户程序哪一个，比如现在用epoll去监控10000个socket链接，交给内核去监测，现在有一个连接有数据了，在有有一个连接有数据了，epoll会直接通知用户程序哪个连接有数据了

epoll能实现高并发原理

 1. epoll() 中内核则维护一个链表，epoll_wait 直接检查链表是不是空就知道是否有文件描述符准备好了。
 1. 在内核实现中 epoll 是根据每个 sockfd 上面的与设备驱动程序建立起来的回调函数实现的。
 1. 某个 sockfd 上的事件发生时，与它对应的回调函数就会被调用，把这个 sockfd 加入链表。
 1. epoll上面链表中获取文件描述，这里使用内存映射（mmap）技术， 避免了复制大量文件描述符带来的开销

内存映射（mmap）：内存映射文件，是由一个文件到一块内存的映射，将不必再对文件执行I/O操作。（0copy技术.内核态与用户态共用一块内存）