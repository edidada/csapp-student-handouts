# CSAPP Malloc Lab

按官方公开讲义实现mm_init/mm_malloc/mm_free/mm_realloc与mm_check，仅修改原实验mm.c。原driver、memlib、配置、Makefile及两条short trace保持不变。讲义：https://csapp.cs.cmu.edu/3e/malloclab.pdf；旧期次和站点占位不代表当前校内课程。

块头/尾包含大小与分配位，8字节对齐；空闲块payload存双向指针。显式链表first-fit，释放立即双向合并，余块足够容纳标签及链接才切分。初始/按需4096字节扩堆，临近上限退回精确大小；不使用系统内存管理调用、全局数组或结构。realloc支持NULL、零、原地缩小、邻块及堆尾增长、搬移复制、失败保留旧内容和大小溢出拒绝。mm_check核对物理分区、标签一致、对齐、无相邻空闲块、链接有效互反、无环及空闲节点一一覆盖，不进入计时路径。

远端验收：原32位Makefile驱动分别跑两条原short trace；独立32/64位ASan/UBSan测试四种邻接合并、切分复用、realloc各路径、溢出与耗尽恢复；5种子共60000随机操作，每步核对全部活跃内容、8字节对齐、堆内范围、互不重叠和mm_check。申请失败后核对原内容，释放后大块重用且不再扩堆。详细日志作为Actions工件。本地不编译或运行，无新增Python文件。

42b7f1e57dd9b07a51039c3d3e128beedb108b91对应run https://github.com/edidada/csapp-student-handouts/actions/runs/43465094832 已completed/success：两条原short、32/64位各60000随机操作（各60027次堆检查）、各19个1MiB块耗尽/内容保持/16MiB复用、六条逐步重放及兼容驱动测量均通过。最终报告提交仍须对应CI，最终证据归档langnote。原公开包只含两条短调试trace，config列出的11条完整trace未包含，不能把两条通过或其性能指数冒充完整官方评分。尚需更多真实工作负载的利用率/吞吐测量；独立压力检查不替代缺失评分trace。

另外生成small/mixed/realloc三类×两种子的六条合成trace，原driver同时测libc与本实现，保存利用率、吞吐和全部trace；这些只补公开工作负载证据，不命名或替代缺失的11条官方trace。

原driver存在plain char符号比较缺陷：memset写index低8位，随后newp[j]与index&255比较，默认signed char下ID131的0x83提升为-125而错误失败。独立重放确认该位置实际数据保持。尝试-funsigned-char还会破坏旧驱动char c=getopt的EOF判断，不能采用。最终两条原short仍按完全未改的原Makefile验证；256 ID合成trace在运行时生成驱动兼容副本，仅将这一字节比较强转unsigned char，断言原表达式恰好出现一次；原mdriver.c保持不变。该副本测量明确为兼容驱动结果，不声称原驱动在默认signed char下通过256 ID。分配器对象完全相同，未缩减ID或期望数据，另有独立重放每步检查全部payload。

该轮六合成负载：mixed两种子利用率均显示100%（原输出取整数，不代表无元数据开销），realloc为76%/74%，small均81%；吞吐分别56860/54658、19514/18921、74463/68719 Kops/s，仅该runner及原计时口径的观察值。原short1的80/100只针对12次请求的调试trace，不能宣称完整Malloc Lab得分。后续机器数值可能变化；准确最终值以归档JSON为准。

所有分配器状态仅为两个全局标量指针，team为原题必填身份结构。空闲链表头插first-fit不保证最优碎片率；无并发/非法重复free契约，也不声称生产malloc替代品。
