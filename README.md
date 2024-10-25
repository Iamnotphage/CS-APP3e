# CS:APP3e

在此处说一下环境:

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