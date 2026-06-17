# ColumbulaSort 🕊️

ColumbulaSort 是一个高性能的基数排序（Radix Sort）C++ 头文件库。单头文件，即插即用，在大多数场景下比 `std::sort` 快 2–20 倍。

全部代码由 DeepSeek-V4 生成，未经广泛验证，请慎重使用。

---

## 快速使用

```cpp
#include "ColumbulaSort.h"
#include <vector>

int main() {
    ColumbulaSort cs;

    // 整型
    std::vector<int> arr = {5, 3, 8, 1, 9, 2, 7, 4, 6, 0};
    cs.sort(arr);                     // sort(vector&)
    cs(arr);                          // operator()(vector&)

    // 浮点数
    std::vector<double> floats = {3.14, -2.71, 1.41, -0.58};
    cs.sort(floats);

    // 字符串
    std::vector<std::string> strs = {"pear", "apple", "banana", "orange"};
    cs.sort(strs);

    // 按 key 排序
    struct Point { int x, y; };
    std::vector<Point> pts = {{5,1}, {3,4}, {8,2}};
    cs.sort(pts, [](const Point& p) { return p.x; });

    return 0;
}
```

### 支持的类型

| 类型 | 说明 |
|------|------|
| `int`, `unsigned int` | 32-bit，AVX2 加速 |
| `short`, `unsigned short` | 16-bit，加速比最优 |
| `long long`, `unsigned long long` | 64-bit |
| `char`, `signed char`, `unsigned char` | 按无符号字节值排序 |
| `float`, `double` | IEEE 754 bit-level 全序 |
| `std::string` | 字典序，中短长度加速明显 |
| 自定义 + key 函数 | 支持所有算术 key 类型 |

### 编译要求

```bash
g++ -O3 -march=native -std=c++17 your_program.cpp -o your_program
```

需要支持 C++17 的编译器。`-march=native` 启用 AVX2 以获得最大性能。

---

## 性能

### 一键测试

```bash
g++ -O3 -march=native -std=c++17 test.cpp -o test
./test
```

### 典型结果（Intel CPU, AVX2）

| 数据 | 数据量 | ColumbulaSort | std::sort | 加速比 |
|------|--------|--------------|-----------|-------|
| int 随机 | 1,000,000 | **15.4 ms** | 45.3 ms | **2.9×** |
| uint 随机 | 1,000,000 | **8.6 ms** | 43.8 ms | **5.1×** |
| ushort 随机 | 1,000,000 | **2.8 ms** | 38.4 ms | **13.7×** |
| ushort 随机 | 5,000,000 | **17.8 ms** | 188.9 ms | **10.6×** |
| float 随机 | 1,000,000 | **11.8 ms** | 46.9 ms | **4.0×** |
| int 50M | 50,000,000 | **1.22 s** | 2.93 s | **2.4×** |
| int 250M | 250,000,000 | **6.42 s** | 17.96 s | **2.8×** |
| ushort 500M | 500,000,000 | **2.42 s** | 20.63 s | **8.5×** |

所有测试均为多轮中位数，确保数据准确。

---

## 算法原理

LSD 基数排序（Least Significant Digit First），每次 pass 处理 8 位，使用稳定的计数排序。

### 关键优化

- **数据自适应** — 自动检测已排序/逆序/常量数据，直接返回或 reverse
- **SIMD 加速** — AVX2 批量处理 32-bit 无符号整型的 min/max 分析和计数索引生成
- **分层计数排序** — 小量数据栈上分配、中等数据 alloca、大数据量多路径分发
- **浮点数全序** — IEEE 754 bit 模式 → 全序无符号表示，处理符号位和 NaN
- **字符串加速** — 短/中等长度字符串用 uint64_t 多 key 排序
- **线程安全缓冲区** — `thread_local` 向量复用，避免反复分配

---

## 项目结构

```
ColumbulaSort/
├── ColumbulaSort.h      # 核心头文件（单头文件库，include 即用）
├── test.cpp             # 完整测试套件
├── test_result.txt      # 运行时测试结果
├── README.md            # 本文档
├── LICENSE              # MIT License
└── .gitignore
```

---

## 许可

[MIT License](LICENSE)
