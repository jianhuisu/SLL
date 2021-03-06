## RTMP推流

“视频直播”是近两年互联网产业里很火的一个版块，大大小小的视频网站、APP层出不穷，
而RTMP是目前市面上实现视频直播所采用的最主流的数据传输方式。

常规的方式是:

 1. 视频主播通过OBS等推流软件将摄像头捕捉的视频通过RTMP协议传输到指定的服务器地址
 1. 服务器将接收到的视频流以m3u8格式保存下来， 
 1. 客户端再通过拉取RTMP数据流的方式获取到视频数据并播放。

以上所描述大概就是一个基本的视频直播模型。

那么，如果想要直接在浏览器中向RTMP服务器推流又该如何实现呢？两种实现方式

 1. 浏览器内置flash支持  (chrome已经废弃对flash的支持)
 2. h5播放器支持.

直播的大致流程：

APP端调用摄像头

-》 拍摄视频 
-》 实时上传视频 
-》 服务器端获取视频并解码 
-》 存储成一小段一小段视频 
-》 服务器端进行推流 
-》 H5或者app端通过一个url拉取视频流进行播放

实际的直播和用户播放的直播会有10秒左右或者更高的延迟，这一点对于后面开发比较重要，一定要注意这个点。


直播推流常用推流传输协议
复制代码

 1. rtsp（real time streaming protocol）：实时流 传输协议，用于控制声音和图像的多媒体串流协议。由real networks和netscape提出。
基于udp协议，实时性比较好、一般用于视频会议。
 2. rtmp（real time messaging protocol）：实时消息 传输协议，adobe公司为flash播放器和服务器之间的音频、视频、数据传输开发的开放协议。
基于tcp协议，低延迟稳定性比较好，一般用于直播推送。默认端口1935.
 3. HLS（http live streaming）：苹果公司实现的基于http协议的流媒体传输协议。

流媒体服务器：负责直播流发布和转播分发功能。一般选择ngnix服务端，一款免费web服务端。
配合nginx-rtmp-module模块使用rtmp协议。

RTSP（Real Time Streaming Protocol），实时流传输协议，是TCP/IP协议体系中的一个应用层协议，由哥伦比亚大学、网景和RealNetworks公司提交的IETF RFC标准。
该协议定义了一对多应用程序如何有效地通过IP网络传送多媒体数据。
RTSP在体系结构上位于RTP和RTCP之上，它使用TCP或RTP完成数据传输。
HTTP与RTSP相比，HTTP传送HTML，而RTP传送的是多媒体数据。
HTTP请求由客户机发出，服务器作出响应；使用RTSP时，客户机和服务器都可以发出请求，即RTSP可以是双向的。

FFmpeg是一套可以用来记录、转换数字音频、视频，并能将其转化为流的开源计算机程序。

FFmpeg是一套可以用来记录、转换数字音频、视频，并能将其转化为流的开源计算机程序。在这里我只用到了它的视屏格式转换功能，将rtsp协议的视频流转成rtmp(监控本身是rtsp协议，现在这个方案虽然用了jwplay,但也只是支持rtmp，因此要用ffmpeg转码)
举个例子：
假如海康摄像头的监控地址为（rtsp://admin:12345@192.168.10.215/h264/ch1/main/av_stream）（帐户，密码，ip,端口，.....）




2 评论消息监听：

我们也通过websocket拉取评论消息，这里主要的问题在服务端压力上，有可能用户评论量很大的时候，服务器压力过大
，出现断连的情况。 也可能是用户网络断开，造成的断连。 一方面后端通过他们的优化来提高承载力，
一方面前端和后端进行配合优化。 我们每次连接websocket服务器的时候，前端会通过接口，
拿到当前承载量最小的服务器地址进行连接。 websocket如果断连了的话，是不会获得任何消息的，
所以保证功能可以使用，我们还会针对websocket进行心跳检测（检查是否断开连接）。

3 心跳 重连

因为websocket可能会存在断开连接的情况，而这时候是不会触发任何事件的，所以我们不知道它是否断开了。
那么我们设置一种消息类型，由前端发送给服务端，服务端如果返回了数据，就说明连接正常。
如果连接断开了，我们再次去请求后端接口，拿到当前承载量最小的服务器地址，进行重连。 
设置一个间隔时间比如10秒，最后一次获取到服务器的消息后，如果10秒内没再收到消息，就进行一次检查，
如果10秒内收到了，便重置这个时间。 之前的博客写过比较详细的心跳检测：《初探和实现websocket心跳重连》

4. video

关于video，总结起来我们要解决的那些问题，或者有些不能解决的问题，归根到底是一个问题：兼容。 
兼容问题又可以分为两种：标签事件的兼容问题和浏览器表现的兼容问题。

标签兼容查看手册
浏览器的表现兼容问题

比如： 在微信和QQ的内置浏览器里，播放视频会自动全屏，video标签也是永远浮在页面最上层，你根本控制不了。 
浮在最上层不只是X5浏览器，还有些手机只带的浏览器。 

视频源出现问题的表现，播放按钮的问题，都有不同。 这些都是脱离我们代码本身，浏览器的设置，所以从代码层面上我们是没法解决的。
之前出现这些问题的时候，当然我也会看下相关直播的公司的页面，看他们是怎么解决的。
比如在微信这个流量大口他们有没有实现看起来实现不了的功能。 
实际结果是，这些厂家应该是微信有合作，进行了相关定制的。 而我们本不是专门做直播的，所以没必要投入这种成本。

    <video autoplay webkit-playsinline>      
        <source src="http://ip/hls/mystream.m3u8" type="application/vnd.apple.mpegurl" />      
        <p class="warning">Your browser does not support HTML5 video.</p>   
    </video> 


## 参考资料 

https://www.cnblogs.com/gaoji/p/6872365.html
