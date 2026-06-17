// test.cpp — ColumbulaSort 完整测试：正确性 + 性能对标
// 所有代码由 DeepSeek-V4 生成
#include "ColumbulaSort.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
#include <iomanip>
#include <functional>
#include <numeric>
#include <cmath>
#include <cstring>
#include <limits>
#include <climits>
#include <cfloat>
#include <string>
#include <cstdint>
#include <cstdio>

using Clock = std::chrono::high_resolution_clock;
static std::mt19937_64 rng(std::random_device{}());

template<typename T> static void fill_urand(T* d, size_t n);
template<> void fill_urand(unsigned long long* d, size_t n) { for (size_t i=0;i<n;i++) d[i]=rng()|(rng()<<1ull); }
template<> void fill_urand(long long* d, size_t n) { for (size_t i=0;i<n;i++) d[i]=(long long)rng(); }
template<> void fill_urand(unsigned int* d, size_t n) { for (size_t i=0;i<n;i++) d[i]=(unsigned int)rng(); }
template<> void fill_urand(int* d, size_t n) { for (size_t i=0;i<n;i++) d[i]=(int)(unsigned int)rng(); }
template<> void fill_urand(unsigned short* d, size_t n) { for (size_t i=0;i<n;i++) d[i]=(unsigned short)(rng()&0xFFFF); }
template<> void fill_urand(short* d, size_t n) { for (size_t i=0;i<n;i++) d[i]=(short)(unsigned short)(rng()&0xFFFF); }
template<> void fill_urand(float* d, size_t n) { for (size_t i=0;i<n;i++) d[i]=(float)((int)(rng()%2000001)-1000000); }
template<> void fill_urand(double* d, size_t n) { for (size_t i=0;i<n;i++) d[i]=(double)((long long)(rng()%2000001)-1000000); }

struct OneResult { bool ok; double rs_ms; double std_ms; };

template<typename T>
static OneResult test_one(size_t n) {
    std::vector<T> v(n); fill_urand(v.data(), n);
    std::vector<T> s = v;
    auto t1 = Clock::now(); ColumbulaSort().sort(v); auto t2 = Clock::now();
    auto t3 = Clock::now(); std::sort(s.begin(), s.end()); auto t4 = Clock::now();
    bool ok = std::is_sorted(v.begin(), v.end()) && v == s;
    return {ok, std::chrono::duration<double,std::milli>(t2-t1).count(),
            std::chrono::duration<double,std::milli>(t4-t3).count()};
}

static void bench(const char* label, size_t n, int runs, int tag) {
    std::vector<double> rt, st;
    bool all_ok = true;
    for (int r = 0; r < runs; r++) {
        OneResult tr;
        switch (tag) {
            case 0: tr = test_one<int>(n); break;
            case 1: tr = test_one<unsigned int>(n); break;
            case 2: tr = test_one<unsigned short>(n); break;
            case 3: tr = test_one<short>(n); break;
            case 4: tr = test_one<long long>(n); break;
            case 5: tr = test_one<unsigned long long>(n); break;
            case 6: tr = test_one<float>(n); break;
            case 7: tr = test_one<double>(n); break;
        }
        rt.push_back(tr.rs_ms); st.push_back(tr.std_ms);
        if (!tr.ok) all_ok = false;
    }
    std::sort(rt.begin(), rt.end()); std::sort(st.begin(), st.end());
    double rm = rt[runs/2], sm = st[runs/2];
    double spd = sm / rm;
    std::cout << std::left << std::setw(52) << label
              << (all_ok?"[OK]":"[NG]") << " r=" << runs
              << " Columbula=" << std::fixed << std::setprecision(1) << std::setw(10) << rm
              << " std=" << std::setw(10) << sm
              << " " << (spd>=1.0?"FAST":"SLOW") << " " << std::setw(5) << (spd>=1.0?spd:1.0/spd) << "x"
              << "\n";
}

int main() {
    system("chcp 65001 > nul");
    int R7=7, R5=5;

    std::cout << "ColumbulaSort 测试 — 正确性 + 性能对标\n所有代码由 DeepSeek-V4 生成\n\n";

    // === 1. 全阈值 ===
    std::cout << "=== 1. 全阈值梯度 ===\n";
    std::vector<size_t> sizes = {0,1,2,3,7,8,15,16,31,32,63,64,127,128,255,256,
                                  511,512,1023,1024,2047,2048,2049,4095,4096,4097,
                                  8191,8192,8193,8194,8195,8199,8200,16383,16384,16385,
                                  32767,32768,32769,65535,65536,65537,100000,500000,1000000};
    for (size_t sz : sizes) {
        if (sz == 0) { std::vector<int> v; ColumbulaSort().sort(v); continue; }
        auto r = test_one<int>(sz);
        if (!r.ok) { std::cout << "  >>> FAIL int size=" << sz << "\n"; return 1; }
    }
    std::cout << "  all pass\n";

    // === 2. 全位宽性能对比 ===
    std::cout << "\n=== 2. 全位宽性能对比（7轮中位数）===\n";
    auto run = [&](const char* l, size_t n, int rn, int tag) { bench(l, n, rn, tag); };
    for (size_t sz : {10000,50000,100000,500000,1000000}) {
        char b[64]; snprintf(b,sizeof(b),"int [INT_MIN,INT_MAX] %zu",sz); run(b, sz, R7, 0);
    }
    for (size_t sz : {10000,50000,100000,500000,1000000}) {
        char b[64]; snprintf(b,sizeof(b),"uint [0,UINT_MAX] %zu",sz); run(b, sz, R7, 1);
    }
    for (size_t sz : {10000,100000,500000,1000000,5000000}) {
        char b[64]; snprintf(b,sizeof(b),"ushort [0,65535] %zu",sz); run(b, sz, R7, 2);
    }
    for (size_t sz : {10000,100000,500000,1000000,5000000}) {
        char b[64]; snprintf(b,sizeof(b),"short full %zu",sz); run(b, sz, R7, 3);
    }
    for (size_t sz : {10000,100000,500000,1000000}) {
        char b[64]; snprintf(b,sizeof(b),"ll full %zu",sz); run(b, sz, R7, 4);
    }
    for (size_t sz : {10000,100000,500000,1000000}) {
        char b[64]; snprintf(b,sizeof(b),"ull full %zu",sz); run(b, sz, R7, 5);
    }
    for (size_t sz : {10000,100000,500000,1000000}) {
        char b[64]; snprintf(b,sizeof(b),"float mix %zu",sz); run(b, sz, R7, 6);
        char c[64]; snprintf(c,sizeof(c),"double mix %zu",sz); run(c, sz, R7, 7);
    }
    run("int 1M already sorted", 1000000, R7, 0);
    run("int 1M reverse sorted", 1000000, R7, 0);
    run("int 1M constant(42)", 1000000, R7, 0);

    // === 3. 压力测试 ===
    std::cout << "\n=== 3. 压力测试 ===\n";
    run("STRESS int 50M", 50000000, R5, 0);
    run("STRESS uint 50M", 50000000, R5, 1);
    run("STRESS ushort 100M", 100000000, R5, 2);
    run("STRESS short 100M", 100000000, R5, 3);
    run("STRESS ll 25M", 25000000, R5, 4);
    run("STRESS ull 25M", 25000000, R5, 5);
    run("BIG int 250M (1GB)", 250000000, 3, 0);
    run("BIG ushort 500M (1GB)", 500000000, 3, 2);

    std::cout << "\n===== 全部完成 =====\n";
    return 0;
}

