# CSAPP Malloc Lab

按官方公开讲义实现mm_init/mm_malloc/mm_free/mm_realloc与mm_check，仅修改原实验mm.c。原driver、memlib、配置、Makefile及两条short trace保持不变。讲义：https://csapp.cs.cmu.edu/3e/malloclab.pdf；旧期次和站点占位不代表当前校内课程。

块头/尾包含大小与分配位，8字节对齐；空闲块payload存双向指针。显式链表first-fit，释放立即双向合并，余块足够容纳标签及链接才切分。初始/按需4096字节扩堆，临近上限退回精确大小；不使用系统内存管理调用、全局数组或结构。realloc支持NULL、零、原地缩小、邻块及堆尾增长、搬移复制、失败保留旧内容和大小溢出拒绝。mm_check核对物理分区、标签一致、对齐、无相邻空闲块、链接有效互反、无环及空闲节点一一覆盖，不进入计时路径。

远端验收：原32位Makefile驱动分别跑两条原short trace；独立32/64位ASan/UBSan测试四种邻接合并、切分复用、realloc各路径、溢出与耗尽恢复；5种子共60000随机操作，每步核对全部活跃内容、8字节对齐、堆内范围、互不重叠和mm_check。申请失败后核对原内容，释放后大块重用且不再扩堆。详细日志作为Actions工件。本地不编译或运行，无新增Python文件。

当前尚待对应提交远端CI结果，不计验收完成。原公开包只含两条短调试trace，config列出的11条完整trace未包含，不能把两条通过或其性能指数冒充完整官方评分。尚需更多真实工作负载的利用率/吞吐测量；独立压力检查不替代缺失评分trace。

另外生成small/mixed/realloc三类×两种子的六条合成trace，原driver同时测libc与本实现，保存利用率、吞吐和全部trace；这些只补公开工作负载证据，不命名或替代缺失的11条官方trace。

原driver存在plain char符号比较缺陷：memset写index低8位，随后newp[j]与index&255比较，在默认signed char下，ID131的0x83提升为-125而错误失败。两条原short仍按原Makefile验证；256 ID合成trace另将原mdriver.c单独以-funsigned-char编译并链接同一分配器对象，不改驱动源码或缩减ID/期望数据。独立重放每步检查全部payload，避免依赖驱动这一历史问题。
