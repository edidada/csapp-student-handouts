# CSAPP Architecture Lab：实现与验收清单

官方来源为本仓库 SOURCE.json 中归档的 CSAPP 3e archlab-handout.tar；内部 archlab/archlab.pdf 是公开通用讲义，带有 Summer 2015 示例日期及站点定制占位，不冒充当前 CMU 校内课程。基础提交806b895a35bed94853a69ae290f7dd024b5d4fd3。独立分支edidada/archlab，所有工具构建、YIS/SEQ/PIPE运行和性能测试放在GitHub Actions，本地仅阅读、编辑和静态检查。

## 必须完成的内容

1. Part A：sum.ys迭代链表求和、rsum.ys递归链表求和、copy.ys复制并求XOR。遵循参数/栈/被调用者保存寄存器约定；讲义三元素列表和复制示例之外，补空表、单元素、负值、零长度、目的区哨兵和栈恢复检查。
2. Part B：seq-full.hcl实现iaddq，覆盖取指长度/寄存器、源/目标、ALU输入、写回和条件码；原asumi、y86-code testssim、ptest原指令及TFLAGS=-i全部回归。不能只测单条加法。
3. Part C：优化ncopy.ys及pipe-full.hcl，任意长度正确，复制内容/正数计数/边界哨兵均正确；机器码小于等于1000字节。实现iaddq时同时处理PIPE数据通路、转发/冒险和条件码。
4. Part C完整验收：原sdriver/ldriver、YIS correctness.pl、PIPE correctness.pl -p、y86-code testpsim、ptest原指令及-i、check-len.pl、benchmark.pl平均CPE。读取讲义评分公式并报告实际CPE，不能将正确性通过冒充性能目标达标。
5. 原starter/test/参考不可随实现修改；旧工具在现代编译器上的兼容调整必须明确记录。运行脚本与薄YAML独立；报告须列出最终SHA、实际运行与工件证据，langnote台账在最终提交CI成功后更新。

## 当前状态

三个部分已实现，首轮1606258/run43459958006远端通过：原PartA三例均返回0xcba；SEQ/PIPE标准及iaddq回归、原y86-code、ncopy YIS/PIPE各68/68长度均通过，代码324字节，平均CPE8.30（43.9/60性能分，达到讲义<9目标，不是满分）。随后增加PartA独立边界验收，最终提交须重新运行全部检查；没有本地编译或模拟。

## 实现说明

- sum_list迭代读取链表；rsum_list递归时保存/恢复rbx；copy_block只用调用者保存临时寄存器，非正长度直接返回0。
- SEQ iaddq取10字节（寄存器字节及立即数），读取rB，ALU计算valC+valB并写回rB，更新ZF/SF/OF；不访问数据内存。
- PIPE增加相同取指和译码路径，以dstE/srcB接入已有转发及load-use控制；仅在正常状态更新条件码，保留旧指令异常/分支/返回处理。
- ncopy四路展开，先读四个值再写，隐藏load-use停顿；批量指针更新和iaddq减少常数加载；独立标量尾循环处理非4倍长度，没有按64元素硬编码，也不越界预取。
- PartA补充33组边界：空/零/负数/混合/64位溢出域，复制零或负长度、目的区前后哨兵，检查复制内容、校验和、rsp及rbx恢复。探针脚本只在远端生成和运行，不改变原测试。

## 运行与兼容边界

独立.github/workflows/archlab.yml只安装flex/bison/libfl-dev并调用scripts/verify_archlab.sh；复用归档sim.tar，不改原回归工具和参考数据。现代GCC以-fcommon兼容旧工具的公共暂定全局定义；GUIMODE/TKLIBS/TKINC为空选择官方TTY实现，不依赖过时Tcl8.5。pipefail和明确成功数量/失败关键词检查避免Perl或tee返回码掩盖测试失败。原seq/pipe各4组ptest在标准及-i模式分别执行，ncopy correctness检查返回成功数必须等于总数，代码长度明确断言<=1000；benchmark不代替correctness。

所有解答文件头含作者和自学ID及设计说明。此为官方公开自学版本，不执行讲义站点占位的handin操作，不声称校内提交或人工评分。源包哈希见SOURCE.json；最终SHA、Actions及永久工件在langnote课程结果中保存。
