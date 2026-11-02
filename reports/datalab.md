# CSAPP 3e Data Lab 实验报告

分支 `edidada/datalab`。起点为 CSAPP 官方公开 student handout，原始下载地址和 SHA256 见 SOURCE.json；本仓库是发行包导入，不能称为上游 GitHub fork，也不冒充某学校当期定制版本。

实现全部 13 个函数：仅用受限位运算表达 XOR、Tmin/Tmax、奇数位、取负、ASCII 数字判断、条件选择、带符号比较和逻辑非；howManyBits 先统一符号再按 16/8/4/2/1 位分段确定最高有效位。浮点部分按 IEEE754 单精度拆分符号、阶码和尾数，分别处理零、非规格化、正规数、溢出、无穷与 NaN；floatFloat2Int 对越界返回指定 sentinel，floatPower2 覆盖 -149/-126/127 边界。

整数部分遵循 handout 的运算符、常量、直线代码与操作数限制，不使用循环、条件分支或函数调用；浮点部分按原题允许分支。官方 dlc 检查规则和操作数上限，btest 检查数值正确性，driver.pl 综合评分，三者都不改写。driver 不带提交参数，不上传校内服务器。

独立 datalab.yml 在 GitHub Ubuntu runner 安装 32 位开发库，分别用 GCC O0/O2 构建和执行。-fwrapv 对齐实验指定的 32 位二补码模型，不宣称这些技巧适用于任意 C 实现；libm 放到链接命令末尾。CI 解析官方输出，要求 btest 和 driver 的实际得分都等于各自满分，不把进程返回 0 单独当成通过。原始 dlc/btest/driver 日志保存 artifact。本地不编译，最终结论以对应 Actions 为准。
