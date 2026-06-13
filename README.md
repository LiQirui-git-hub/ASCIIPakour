# ASCIIPakour

> [!TIP]
> 本文代码可以通过Dev-C++ 5.11编译，不用安装外部库

## 介绍：
这是一个**3D**游戏，仅支持Windows(暂时，未来有概率改变)

同时，画面**不是用新窗口渲染**，是**命令行内渲染**

支持新版本Windows Terminal和普通命令行

|  操作系统  | 兼容性  |
| --------- | ------- |
| Windows11 | 完全兼容 |
| Windows10 | 完全兼容 |
| Windows7  | 待测存疑 |

| 键位 | 操作 |
| --------- | ------- |
w,s,a,d|前后左右移动
Space  |跳跃
Shift  |冲刺
R      |重开
Esc    |退出游戏
鼠标   |改变视角

## 许可证简释
本项目采用 [PolyForm Noncommercial License 1.0.0](LICENSE)。
- 商业用途（包括出售、付费SaaS、集成到商业产品）需获取单独授权
- 商业授权请联系：eod_ilff7k0eh@dingtalk.com
作为版权所有者，我保留未来自行商用的权利。

## 编译时加入命令：
```bash
-std=c++11 -lpthread -O3 -fopenmp
```
