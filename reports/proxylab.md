# CSAPP Proxy Lab

官方公开要求：https://csapp.cs.cmu.edu/3e/proxylab.pdf。实现GET HTTP/1.0转发、并发及有界近似LRU缓存；讲义旧期次/站点占位不代表当前校内评分。

每连接detached线程，使用原RIO无进程退出包装的读写接口；忽略SIGPIPE，连接/读写错误在当前事务回收。解析绝对http URL、默认80/显式端口、路径缺省及query，额外支持origin-form和括号IPv6。固定HTTP/1.0、User-Agent及两种Connection: close，保留客户端Host及普通附加头，移除hop-by-hop头与Connection命名字段；有界请求/字段长度，拒绝重复Host、GET消息体及传输编码歧义。响应按实际字节数转发，不用字符串长度处理二进制内容。

缓存按完整响应字节计，单对象102400、总量1049000。读写锁允许多个读者同时复制数据，随后释放锁再写慢客户端；原子使用时间近似LRU，写者独占插入/淘汰，同键并发miss去重。缓存键包含实际连接origin host/port、请求目标和全部转发头，避免Host override或请求变体碰撞。完整EOF、200、Content-Length一致才可缓存；过大、截断、transfer-coded、private/no-store/Vary:*及凭证/Cookie/Range请求绕过缓存。不宣称完整HTTP缓存freshness协议或HTTPS CONNECT/POST支持。

独立proxylab.yml仅调用shell；原driver.sh、Tiny源码、Makefile与csapp库保持不变，现有nop-server.py仅更新Python3解释器路径。清空NO_PROXY，防止curl绕过本机proxy导致假通过；构建原Tiny与proxy，严格断言Basic40/Concurrency15/Cache15及70总分。额外socket fixture分别验证普通和ASan/UBSan版本的二进制、请求头、默认80、错误恢复、32并发与阻塞origin隔离、96缓存并发读、变体/port key隔离、102400边界、过大/截断/私密不缓存、容量/LRU、客户端RST与origin离线缓存。Python仅嵌入shell，未新增Python文件；本地不编译或运行。

7378d537146b0cb6f2ef8c758a4c5855bed49152对应run https://github.com/edidada/csapp-student-handouts/actions/runs/43467716091 已completed/success。原Basic40/Concurrency15/Cache15，总70/70；普通和ASan/UBSan两种构建各18组独立检查通过。每种包含32并发（另有一个阻塞origin）、96并发cache命中、40客户端RST、500次cache请求及空闲FD/线程恢复，原对象边界和LRU期望未放宽。sanitizer日志无AddressSanitizer或UBSan runtime error；服务以SIGTERM结束，不将此称为退出时LSan完整堆泄漏证明。最终报告提交仍需对应CI，最终SHA/工件以langnote永久证据为准。

默认80端口以GitHub临时runner上的单次sudo origin helper验证，proxy始终普通用户运行，不改账户、capability或sysctl；现有nop-server仅换解释器。额外头/二进制期望与原参考不改。旧csapp库本身有-Wextra警告，单独以原-Wall加sanitizer构建，新增proxy继续-Wextra -Werror；未修改库来消除这些警告。错误响应后只做有界非阻塞排空，避免未读尾部导致TCP reset吞掉错误状态。FD/线程恢复观察覆盖在途事务/10秒socket超时，并非将忙碌瞬间资源当泄漏。

原库阻塞getaddrinfo/connect仍可能受系统解析/连接超时影响；每连接线程并非生产环境有界线程池。测试不宣称完整HTTP/1.1、TLS、认证代理、freshness重验证、所有RFC异常/IPv6网络或生产负载验收。
