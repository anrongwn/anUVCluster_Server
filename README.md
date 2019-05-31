# anUVCluster_Server
基于libuv 的tcp 进程集群服务


# anUVMaster 主进程负责监听tcp server 端口，并根据CPU数量启动从服务进程，进行连接负载匀衡。
基于 uv_spawn 功能启动从服务进程，master 服务进程 通过 stdio 标准管道 发送收到的
socket connect handle 到slave 服务进程。

#anUVSlave 从服务进程负责收发 master进程发来的socket 数据。处理构架与anUVServer 服务
类似。


