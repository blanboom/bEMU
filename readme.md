# bEMU: Blanboom's NES/Famicom Emulator

这是一个具有基本功能的 NES 模拟器，能够运行部分 NES 软件和游戏。

完成本程序仅仅是为了加深自己对计算机底层工作原理的理解。如果需要一个稳定、兼容性强的 NES 模拟器，建议参考其他项目。

## 主要功能与使用方法

**1\. 显示 NES ROM 信息**

```
bEMU -i rom_file.nes
```

**2\. 反汇编器**

```
bEMU -d rom_file.nes
```

**3\. 运行模拟器**

```
bEMU -r rom_file.nes
```

**4\. 显示调试信息**

在运行模拟器的过程中，按下 Ctrl+T, 即可显示 CPU、PPU 寄存器中的数值等信息。

该功能通过 SIGINFO 信号实现，所以只适合基于 BSD 的操作系统，包括 OS X. 在 Linux 中可考虑换用 SIGUSR1.

## 编译方法

**1\. 安装 Allegro**

本程序使用 Allegro 进行图像的显示、以及按键信息的获取。在编译之前，首先需要安装 Allegro. 

在 OS X 操作系统下，可使用 Homebrew 安装:

```
brew install allegro
```

**2\. 指定 Allegro 所在位置**

编辑 `CMakeLists.txt`，在 `include_directories` 和 `link_directories` 中指定 Allegro 头文件和库文件所在路径。如果使用 Homebrew 安装，默认路径为 `/usr/local/inclue` 和 `/usr/local/lib`.

**3\. 开始编译**

```
cmake CMakeLists.txt
make
```

## 感谢

本程序参考和使用了下列项目中的代码：

- **LiteNES**: https://github.com/NJUOS/LiteNES
- **nesemu2**: https://github.com/holodnak/nesemu2
- **mwillsey/NES**: https://github.com/mwillsey/NES
