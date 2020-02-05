## [文件监控系统第二版]

### 项目概况

#### 背景

* 项目来源：[2019深度软件开发大赛](https://www.deepin.org/devcon-2019/topic)
* 项目名称：瑞星文件管控客户端

#### 运行环境

* deepin Linux x86_64 系统,理论上兼容其他Linux系统

#### 条件与限制

* 编译时，g++版本有限制（请使用最新版本）
* 区分服务端与客户端，一般运行在不同的机器上
* 正式的运行环境是N(N>=2)台计算机节点，通过有线或者无线互联，且运行服务端机器有客户端机器能直连的IP地址
* 您确保您的内核支持`fanotify`

#### 业务功能需求

* 监视和记录指定目录文件下文件事件（包括打开和关闭），当打开和关闭事件发生时，client向server发送一个文件事件信息（包括文件路径、文件句柄、和操作方式等等）
* 用户在配置了指定的监视服务器后，必须等待server对该文件进行指令后，才可根据规则执行操作（操作包括：读取、删除、重写等）
* 当监视服务器关闭或网络不通时，client拒绝任何操作，不允许任何人打开文件
* 当没有配置监视服务器或者监视服务器不存在时，根据默认权限执行操作

#### 业务功能补充需求

* 监控和记录指定目录文件是不是被打开(open)和关闭(close)状态.这两个函数必须阻塞完成服务器对终端文件的操作.基于阻塞决定利用HOOK技术劫持调用
* 客户端需要感知与服务器的连接状态,一旦连接断开,则禁止客户端打开文件


#### 业务功能注意事项
* 关于多进程同时对多文件执行不同操作的情况
* 关于服务端同时接受多个客户端请求执行的情况

#### 系统功能需求

* 监控文件系统操作,考虑在内核态实现,因为用户态容易被绕过,并不安全

#### 性能及运行需求
* 7*24小时不间断操作，如果崩掉能做到及时重启.



#### 特性

	-  采用`epoll` +`fanotify`实现在`内核态`上的监控,完成对于子目录的递归监控
	- 使用`多线程`发送文件
	- 采用 `rapidjson`来解析单配置文件
	- 日志管理
	- `Reuseport`特性
	- 对象池技术
	- 避免内存泄漏,尽可能使用RAII技术
	- ....





#### 客户端概要设计(第二版比第一版优势?)

* 第一版是通过`Ring3`中的`LD_PRELOAD`动态连接.so函数劫持.因为`Linux`系统是基于`Glibc`的,会由动态链接器负责装载和初始化,`LD_PRELOAD`是首先要加载so的路径,通过在`LD_PRELOAD`中加载.so我们所需要的同名函数即可.但是被强制覆盖`LD_PRELOAD`会很容易被绕过
* 第一版使用了消息队列并不安全,任何非root用户也可以在消息队列中读取消息,用户态标示消息队列可以通过"ipcs"指令获得,攻击者可以获取消息队列中的内容
* 第二版选择`fanotify`来实现`ring0`级别的监控,`fanotify`比`inotify`优势在于当文件被访问的时候,监控程序可以获取事件通知,还能够决定是不是允许该操作

- 客户端通过`rapidjson`进行配置文件解析
- `fanotify`并不能监控下下层目录,采用递归的方式进行注册,并注册相应的事件
- 每当有一个进程打开一个文件的时候,客户端会触发读事件,并且客户端标记引用计数+1,只有第一次打开的时候,客户端向服务器发起备份,当文件的引用技术原值,也就是所有的进程都已经关闭这个文件,这时发起恢复请求
- 采用多线程同时给服务器发送文件



#### 服务器概要设计

- 每个线程创建各自的epoll和监听套接字,使用`reuseport`特性

- 根据mac地址作为每一个用户的目录,根据包中的偏移量移动文件指针,进行写入

- ....

#### 数据结构设计

- 配置文件使用json 格式

- 通信协议:结构体

  ```
  struct data{
      char  mac[32]; //mac地址
      char  path[PATH_MAX]; // 文件路径
      char  content[8192];//文件内容
      bool  sign ; // 事件标志 open  false   or close true
      longlong   n; //文件开始偏移量 
  };
  ```

#### 功能说明

- 给终端中的文件监视工具配置受控文件路径和监视服务器地址，确保文件监视工具和监视服务器控制模块程序正常运行.
- 在正常和服务器相连中,编写测试代码,open并且读取文件,显示`It is a secret`,关闭监控程序,再次读取显示原本内容
- 监控程序在没有和服务器相连,拒绝打开文件

#### 安装和运行

- 在`Client/config`目录下找到`init.json`文件,修改相应的配置

- `sudo cp init.json /etc` 

- 分别在 Client 和Server 目录下 创建 build 目录

- 执行 `cmake ..` 和` make`

- 执行相应的程序

  #### 

  



  

  
