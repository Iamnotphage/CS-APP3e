# CS:APP3e

个人推荐环境解决方案(优先级从上到下):

1. 纯血Linux主机(感觉大部分学生党不会有)
2. Windows + WSL2/Docker (推荐WSL2)
3. MacOS + Docker (Apple Silicon会有gdb问题)

**总之，需要一个x86的linux环境**

前置知识:

1. 掌握基本Linux命令
2. 了解make工具链
3. 科学上网能力
4. 折腾过WSL或者Docker

本人环境:

1. MacOS(主力) + Docker(centOS)
2. Windows(为了gdb调试) + WSL2
   
*Mac的M系列芯片暂时没有gdb，所以只能别的方法*

最推荐的当然是纯血x86的Linux主机，但是作为学生来说肯定很鸡肋

Mac要运行x86架构的程序，可以考虑用docker

在docker中拉取centOS并下载对应的工具(网路上很多教程)

# 关于Mac无法使用gdb

在docker容器中的centOS就算`yum install gdb`了也无法读取寄存器。

本人的方案是利用ssh远端调用Windows进行gdb调试

ssh配置以及内网穿透自行解决即可。