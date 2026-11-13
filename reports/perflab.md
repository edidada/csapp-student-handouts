# CSAPP Performance Lab

rotate使用16×16分块、块内连续目标写入，处理不整块尾部；smooth将边界与内部拆开，内部复用三行列和，只计算新右列，保持整数求和后除9的精确语义。仅修改题目允许的kernels.c，原naive版本保留。

独立perflab.yml调用shell验证脚本：原32位Makefile/时钟/driver/config不改，原驱动三个随机种子完整执行；另以ASan/UBSan测试50尺寸、5种16位RGB模式及两内核，共500组，检查独立数学期望、源不变及目的前后哨兵。本地不编译运行，无新增Python源文件。

性能分两种口径：原config保留历史Pentium III基准分数；实际提升则对同一runner中重新执行的naive与优化CPE计算五尺寸几何平均，再取三次中位数。两内核都必须快于同机基线，不能把旧config速度倍数当同机改进。候选尚待远端结果，不计完成。

官方公开通用要求：https://csapp.cs.cmu.edu/3e/perflab.pdf，允许整数C优化，不使用内核中的汇编、外部库并行或全局图像缓存。旧期次和站点handin占位不视为当前校内实验。固定源包哈希见SOURCE.json。
