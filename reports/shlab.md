# CSAPP Shell Lab

实现官方tsh.c七个空函数：fork/execve启动命令、quit/jobs/bg/fg、整组SIGINT/SIGTSTP转发、SIGCHLD回收与状态更新。fork前阻塞信号，父子双方setpgid；子进程还原默认信号处置及原mask后exec；父进程先登记再解屏蔽。作业表主流程访问受信号屏蔽保护，所有处理器互斥，waitfg以sigsuspend避免丢失唤醒。处理器用栈上整数格式化及write输出并保留errno，没有printf。

独立shlab.yml调用scripts/verify_shlab.sh，仅在Actions编译运行；16条原trace及原sdriver/tshref保持不变，对照实时参考输出，仅归一化动态PID。官方公开旧版参考需32位运行库；不修改账户或调用handin占位路径。

首轮16条原trace中14条一致，14/15仅缺失命令的句点与实际tshref不同；已去掉句点，未放宽比较。注意公开讲义示例的文本与实际参考二进制存在这一差异，以实时参考为准。最终提交必须同时通过16条原trace及下面独立检查，不能将候选代码存在当成验收通过。没有本地编译运行，没有新增Python文件。

## 独立进程验收

scripts/verify_shlab_jobs.sh在远端运行10组检查：无前台时SIGINT/SIGTSTP不影响shell；150个快速前台退出；100个分批后台退出且/proc直接子进程列表清空；停止列表与bg PID；mysplit整个进程组及后代同时停止；bg JID恢复父子；fg PID后中断整组且独立后台任务仍活着；fg JID及直接子进程回收；非法作业参数不破坏shell；quit正常退出。输入输出同步用明确回显标记及限时/proc状态观察，不以固定长sleep冒充进程状态证据。所有探针创建的进程组在finally中清理。

## 并发与范围

主流程作业列表读写和打印期间屏蔽信号并在恢复前flush，处理器用全信号mask串行化，保留errno；所有waitpid仅在SIGCHLD处理器循环WNOHANG/WUNTRACED/WCONTINUED回收。信号终止和停止信息使用栈上格式化/write，原SIGQUIT处理器也改用write/_exit。前台等待在屏蔽状态检查，再sigsuspend原mask，避免忙等和丢失唤醒。子进程独立进程组，恢复默认处置后才解除mask，父子均setpgid防调度竞态。

只实现本题要求的外部路径和四个内建命令，不额外扩展管道或重定向；原parseline的单引号规则保留。不是完整POSIX shell，也不声称校内handin/私有评分。原sdriver、trace01-16和参考二进制以Git基线806b895a35bed94853a69ae290f7dd024b5d4fd3检查未改；原包SHA256见SOURCE.json。Actions日志、逐条参考/实际输出、diff、traces.json和独立进程日志/JSON均上传；最终SHA与运行由langnote台账永久记录。

来源：https://csapp.cs.cmu.edu/3e/shlab.pdf；原包哈希见SOURCE.json，官方公开通用讲义含旧期次和站点占位，不冒充当前校内实验。
