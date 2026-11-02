# CSAPP Cache Lab 实验报告

官方 CSAPP 3e student handout 原始地址和 SHA256 见 SOURCE.json。本分支 edidada/cachelab，独立 cachelab.yml 远程编译评分。

模拟器解析 I/L/S/M 记录，忽略取指；M 执行读写两次访问。按地址拆分 set/tag，用单调访问序号实现每组 LRU，优先填空行，分别统计 hit/miss/eviction，支持 verbose 与 64 位地址。内存分配和参数范围检查避免无效移位及尺寸乘法溢出。

转置函数仅使用 12 个局部 int，不使用额外数组、动态内存或递归。32 方阵以 8×8 分块，一次读满一行再写回；64 方阵以 8×8 分块中的四个 4×4 子块换位，借助 B 的最终存储区域暂存右上块，避免相隔四行的直接映射冲突。非方阵按 16×16 分块，并检查边界。

原 driver.py 只做 Python 3 兼容迁移（print、subprocess 文本模式、map 列表），不改计分逻辑和原测试。CI 执行原 test-csim 和 Valgrind/test-trans；严格要求原 AUTORESULT_STRING 为 53/53，以及 32/64/61 三种尺寸 miss 分别不超过 300/1300/2000。未提交 Autolab；以最终 Actions 运行结果为验收依据，本地不编译。
