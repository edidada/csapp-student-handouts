# CSAPP Architecture Lab：实现与验收清单

官方来源为本仓库 SOURCE.json 中归档的 CSAPP 3e archlab-handout.tar；内部 archlab/archlab.pdf 是公开通用讲义，带有 Summer 2015 示例日期及站点定制占位，不冒充当前 CMU 校内课程。基础提交806b895a35bed94853a69ae290f7dd024b5d4fd3。独立分支edidada/archlab，所有工具构建、YIS/SEQ/PIPE运行和性能测试放在GitHub Actions，本地仅阅读、编辑和静态检查。

## 必须完成的内容

1. Part A：sum.ys迭代链表求和、rsum.ys递归链表求和、copy.ys复制并求XOR。遵循参数/栈/被调用者保存寄存器约定；讲义三元素列表和复制示例之外，补空表、单元素、负值、零长度、目的区哨兵和栈恢复检查。
2. Part B：seq-full.hcl实现iaddq，覆盖取指长度/寄存器、源/目标、ALU输入、写回和条件码；原asumi、y86-code testssim、ptest原指令及TFLAGS=-i全部回归。不能只测单条加法。
3. Part C：优化ncopy.ys及pipe-full.hcl，任意长度正确，复制内容/正数计数/边界哨兵均正确；机器码小于等于1000字节。实现iaddq时同时处理PIPE数据通路、转发/冒险和条件码。
4. Part C完整验收：原sdriver/ldriver、YIS correctness.pl、PIPE correctness.pl -p、y86-code testpsim、ptest原指令及-i、check-len.pl、benchmark.pl平均CPE。读取讲义评分公式并报告实际CPE，不能将正确性通过冒充性能目标达标。
5. 原starter/test/参考不可随实现修改；旧工具在现代编译器上的兼容调整必须明确记录。运行脚本与薄YAML独立；报告须列出最终SHA、实际运行与工件证据，langnote台账在最终提交CI成功后更新。

## 当前状态

已读官方讲义并建立独立worktree，尚未实现或执行任何实验。下一步从原sim.tar读取HCL及驱动源码，完成三个部分实现及独立工作流。没有本地编译或模拟。
