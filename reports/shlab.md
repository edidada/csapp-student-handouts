# CSAPP Shell Lab

实现官方tsh.c七个空函数：fork/execve启动命令、quit/jobs/bg/fg、整组SIGINT/SIGTSTP转发、SIGCHLD回收与状态更新。fork前阻塞信号，父子双方setpgid；子进程还原默认信号处置及原mask后exec；父进程先登记再解屏蔽。作业表主流程访问受信号屏蔽保护，所有处理器互斥，waitfg以sigsuspend避免丢失唤醒。处理器用栈上整数格式化及write输出并保留errno，没有printf。

独立shlab.yml调用scripts/verify_shlab.sh，仅在Actions编译运行；16条原trace及原sdriver/tshref保持不变，对照实时参考输出，仅归一化动态PID。官方公开旧版参考需32位运行库；不修改账户或调用handin占位路径。

当前为首个候选，远端结果待确认，不计完成。还需补充快速退出竞争、无前台信号、PID/JID切换、进程组后代与僵尸检查。没有本地编译运行，没有新增Python文件。

来源：https://csapp.cs.cmu.edu/3e/shlab.pdf；原包哈希见SOURCE.json，官方公开通用讲义含旧期次和站点占位，不冒充当前校内实验。
