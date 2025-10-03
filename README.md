# 2025-2026 操作系统课程实验

这是 2025-2026 学年操作系统课程的实验代码仓库。

## 📚 实验内容

- **lab1**: 操作系统启动
- **lab2**: 物理内存管理
- **lab3**: 中断处理与异常

## 🛠️ 环境配置

详细的环境配置说明请参考 [README_SETUP.md](README_SETUP.md)

### 快速开始

1. 下载 RISC-V 工具链
2. 安装 QEMU
3. 配置环境变量
4. 开始实验

```bash
# 激活实验环境
source setup_os_env.sh

# 编译并运行实验
cd labcode/lab1
make
make qemu
```

## 📂 目录结构

```
.
├── labcode/              # 实验代码
│   ├── lab1/            # 实验1
│   ├── lab2/            # 实验2
│   └── lab3/            # 实验3
├── setup_os_env.sh      # 环境配置脚本
├── install_tools.sh     # 工具安装脚本
└── README_SETUP.md      # 详细配置指南
```

## 🚀 使用说明

### 编译实验

```bash
cd labcode/lab1
make clean
make
```

### 运行实验

```bash
make qemu
```

退出 QEMU: 按 `Ctrl+A` 然后松开，再按 `X`

## 📖 参考资料

- [rCore Tutorial](https://rcore-os.github.io/rCore-Tutorial-Book-v3/)
- [RISC-V 规范](https://riscv.org/technical/specifications/)
- [QEMU 文档](https://www.qemu.org/docs/master/)

## 📝 注意事项

- 本仓库不包含 RISC-V 工具链压缩包（文件过大）
- 请自行下载并配置工具链
- 详细配置步骤见 [README_SETUP.md](README_SETUP.md)

## 🎓 课程信息

- **课程**: 操作系统
- **学年**: 2025-2026
- **架构**: RISC-V 64

---

© 2025-2026 操作系统课程
