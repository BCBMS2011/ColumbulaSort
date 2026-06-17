// ColumbulaSort.h — 高性能基数排序  // columbina hypo selenia
#ifndef COLUMBULA_SORT_H
#define COLUMBULA_SORT_H

#include <vector>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <iterator>
#include <type_traits>
#include <cstdint>
#include <numeric>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

#ifdef __AVX2__
    #include <immintrin.h>
    #define HAS_AVX2 1
#else
    #define HAS_AVX2 0
#endif

#ifdef __GNUC__
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define PREFETCH(addr, rw, loc) __builtin_prefetch(addr, rw, loc)
    #define RESTRICT __restrict__
#else
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
    #define PREFETCH(addr, rw, loc)
    #define RESTRICT
#endif

#define CACHE_LINE 64

class ColumbulaSort {
private:
    static constexpr size_t SMALL_SORT_THRESHOLD = 8192;
    static constexpr size_t STRING_DIRECT_SORT_THRESHOLD = 50000;
    static constexpr size_t STRING_MAX_LEN_DIRECT = 32;
    template<typename T> static constexpr int bitWidth() { return sizeof(T) * 8; }
    template<typename T> using UT = typename std::make_unsigned<T>::type;

    template<typename UT>
    static void countingSort8(const UT* RESTRICT src, UT* RESTRICT dst, size_t n, int shift) {
        alignas(CACHE_LINE) int count[256];
        memset(count, 0, sizeof(count));
        size_t i = 0;
        for (; i + 4 <= n; i += 4) {
            ++count[(src[i]>>shift)&0xFF]; ++count[(src[i+1]>>shift)&0xFF];
            ++count[(src[i+2]>>shift)&0xFF]; ++count[(src[i+3]>>shift)&0xFF];
        }
        for (; i < n; ++i) ++count[(src[i]>>shift)&0xFF];
        for (int j = 1; j < 256; ++j) count[j] += count[j-1];
        for (i = n; i-- > 0;) { int d = (src[i]>>shift)&0xFF; dst[--count[d]] = src[i]; }
    }

    template<typename UT>
    static void countingSortDispatch(const UT* RESTRICT src, UT* RESTRICT dst, size_t n, int shift, int bits) {
        (void)bits;
#if HAS_AVX2
        if constexpr (sizeof(UT) == 4) {
            if (n > 1024) {
                static thread_local std::vector<uint32_t> idxBuf;
                static thread_local std::vector<int> countBuf;
                if (idxBuf.size() < n) idxBuf.resize(n);
                uint32_t* RESTRICT idx = idxBuf.data();
                size_t i = 0;
                const __m256i sv = _mm256_set1_epi32(shift);
                const __m256i mv = _mm256_set1_epi32(0xFF);
                for (; i + 8 <= n; i += 8) {
                    __m256i v = _mm256_loadu_si256((const __m256i*)(src + i));
                    v = _mm256_srlv_epi32(v, sv);
                    v = _mm256_and_si256(v, mv);
                    _mm256_storeu_si256((__m256i*)(idx + i), v);
                }
                for (; i < n; ++i) idx[i] = (src[i]>>shift)&0xFF;
                countBuf.assign(256, 0);
                int* RESTRICT cnt = countBuf.data();
                for (i = 0; i < n; ++i) ++cnt[idx[i]];
                for (int j = 1; j < 256; ++j) cnt[j] += cnt[j-1];
                for (i = n; i-- > 0;) { uint32_t d = idx[i]; dst[--cnt[d]] = src[i]; }
                return;
            }
        }
#endif
        countingSort8(src, dst, n, shift);
    }

    template<typename T>
    static void dynamicRadixSort(T* RESTRICT first, T* RESTRICT last, T* RESTRICT temp, const std::vector<int>& passes) {
        using U = UT<T>;
        U* uf = (U*)first, * ut = (U*)temp;
        size_t n = last - first; if (n == 0) return;
        U* src = uf, * dst = ut; int shift = 0;
        for (size_t i = 0; i < passes.size(); ++i) {
            countingSortDispatch(src, dst, n, shift, passes[i]);
            std::swap(src, dst); shift += passes[i];
        }
        if (src != uf) memcpy(uf, ut, n * sizeof(U));
    }

    static void genPasses(int tb, std::vector<int>& p) {
        p.clear(); if (tb <= 0) return;
        int k = (tb + 7) / 8;
        for (int i = 0; i < k; ++i) p.push_back(8);
    }

    template<typename T>
    static void sortFloat(T* first, T* last) {
        size_t n = last - first; if (n <= 1) return;
        if (n < SMALL_SORT_THRESHOLD) { std::sort(first, last); return; }
        using U = typename std::conditional<sizeof(T)==4, uint32_t, uint64_t>::type;
        const U sb = (sizeof(T)==4) ? 0x80000000u : 0x8000000000000000ull;
        std::vector<U> bits(n);
        for (size_t i = 0; i < n; ++i) { U b; memcpy(&b, &first[i], sizeof(T)); bits[i] = (b & sb) ? ~b : (b ^ sb); }
        std::vector<int> ps; genPasses(sizeof(U)*8, ps);
        std::vector<U> temp(n);
        U* src = bits.data(), * dst = temp.data(); int shift = 0;
        for (int b : ps) { countingSortDispatch(src, dst, n, shift, b); std::swap(src, dst); shift += b; }
        if (src != bits.data()) memcpy(bits.data(), temp.data(), n * sizeof(U));
        for (size_t i = 0; i < n; ++i) { U b = bits[i]; if (b & sb) b ^= sb; else b = ~b; memcpy(&first[i], &b, sizeof(T)); }
    }

    static void sortChar(char* first, char* last) {
        size_t n = last - first; if (n <= 1) return;
        if (n < SMALL_SORT_THRESHOLD) { std::sort(first, last, [](char a, char b){ return (unsigned char)a < (unsigned char)b; }); return; }
        alignas(CACHE_LINE) int count[256] = {0};
        for (size_t i = 0; i < n; ++i) ++count[(uint8_t)first[i]];
        for (int i = 1; i < 256; ++i) count[i] += count[i-1];
        std::vector<char> temp(n);
        for (size_t i = n; i-- > 0;) { uint8_t val = (uint8_t)first[i]; temp[--count[val]] = first[i]; }
        memcpy(first, temp.data(), n);
    }

    static void sortStrings(std::string* first, std::string* last) {
        size_t n = last - first; if (n <= 1) return;
        size_t max_len = 0; for (size_t i = 0; i < n; ++i) if (first[i].size() > max_len) max_len = first[i].size();
        if (n < STRING_DIRECT_SORT_THRESHOLD || max_len > STRING_MAX_LEN_DIRECT) { std::sort(first, last); return; }
        static thread_local std::vector<size_t> indices, tmp_idx;
        static thread_local std::vector<uint64_t> keys, tmp_keys;
        static thread_local std::vector<std::string> temp;
        if (indices.size() < n) indices.resize(n); if (temp.size() < n) temp.resize(n);
        std::iota(indices.begin(), indices.end(), 0);
        size_t ns = (max_len + 7) / 8;
        if (keys.size() < n) keys.resize(n); if (tmp_keys.size() < n) tmp_keys.resize(n); if (tmp_idx.size() < n) tmp_idx.resize(n);
        for (size_t seg = ns; seg-- > 0;) {
            uint64_t* kp = keys.data(); size_t* ip = indices.data();
            for (size_t i = 0; i < n; ++i) {
                const std::string& s = first[ip[i]]; uint64_t val = 0; size_t start = seg * 8, len = s.size();
                for (size_t b = 0; b < 8 && (start + b) < len; ++b) val |= (uint64_t)(unsigned char)s[start + b] << ((7 - b) * 8);
                kp[i] = val;
            }
            sortKeysAndIndices(kp, ip, n);
        }
        for (size_t i = 0; i < n; ++i) temp[i] = std::move(first[indices[i]]);
        for (size_t i = 0; i < n; ++i) first[i] = std::move(temp[i]);
    }

    template<typename KeyType>
    static void countingSortWithIndex(const KeyType* RESTRICT sk, KeyType* RESTRICT dk,
                                      const size_t* RESTRICT si, size_t* RESTRICT di, size_t n, int shift) {
        alignas(CACHE_LINE) int count[256] = {0};
        for (size_t i = 0; i < n; ++i) ++count[(sk[i]>>shift)&0xFF];
        for (int i = 1; i < 256; ++i) count[i] += count[i-1];
        for (size_t i = n; i-- > 0;) { int d = (sk[i]>>shift)&0xFF; size_t p = --count[d]; dk[p]=sk[i]; di[p]=si[i]; }
    }

    template<typename KeyType>
    static void sortKeysAndIndices(KeyType* keys, size_t* indices, size_t n) {
        if (n <= 1) return;
        if (n < SMALL_SORT_THRESHOLD) {
            std::vector<size_t> order(n); std::iota(order.begin(), order.end(), 0);
            std::sort(order.begin(), order.end(), [&](size_t a, size_t b){ return keys[a] < keys[b]; });
            std::vector<KeyType> nk(n); std::vector<size_t> ni(n);
            for (size_t i = 0; i < n; ++i) { nk[i] = keys[order[i]]; ni[i] = indices[order[i]]; }
            memcpy(keys, nk.data(), n * sizeof(KeyType)); memcpy(indices, ni.data(), n * sizeof(size_t));
            return;
        }
        std::vector<int> ps; genPasses(bitWidth<KeyType>(), ps);
        std::vector<KeyType> tmpK(n); std::vector<size_t> tmpI(n);
        KeyType* sk = keys, * dk = tmpK.data(); size_t* si = indices, * di = tmpI.data(); int shift = 0;
        for (int b : ps) { countingSortWithIndex(sk, dk, si, di, n, shift); std::swap(sk, dk); std::swap(si, di); shift += b; }
        if (sk != keys) { memcpy(keys, tmpK.data(), n * sizeof(KeyType)); memcpy(indices, tmpI.data(), n * sizeof(size_t)); }
    }

    template<typename K> static constexpr auto to_unsigned_key(K key) {
        using U = UT<K>;
        if constexpr (std::is_floating_point_v<K>) {
            using U2 = typename std::conditional<sizeof(K)==4, uint32_t, uint64_t>::type;
            U2 bits; memcpy(&bits, &key, sizeof(K)); const U2 sb = U2(1) << (sizeof(K)*8-1);
            return (bits & sb) ? ~bits : (bits ^ sb);
        } else if constexpr (std::is_signed_v<K>) {
            return static_cast<U>(key) ^ (U(1) << (sizeof(K)*8-1));
        } else { return static_cast<U>(key); }
    }

    // 多路归并
    template<typename T>
    static void multiMerge(T** chunks, size_t* sizes, T* output, int k) {
        using Item = std::pair<T, int>;
        auto cmp = [](const Item& a, const Item& b) { return a.first > b.first; };
        std::vector<Item> heap; std::vector<size_t> pos(k, 0);
        heap.reserve(k);
        for (int i = 0; i < k; ++i) if (pos[i] < sizes[i]) heap.emplace_back(chunks[i][pos[i]++], i);
        std::make_heap(heap.begin(), heap.end(), cmp);
        size_t oi = 0;
        while (!heap.empty()) {
            std::pop_heap(heap.begin(), heap.end(), cmp);
            auto [val, idx] = heap.back(); heap.pop_back();
            output[oi++] = val;
            if (pos[idx] < sizes[idx]) { heap.emplace_back(chunks[idx][pos[idx]++], idx); std::push_heap(heap.begin(), heap.end(), cmp); }
        }
    }

public:
    // ========== 单线程排序（原有接口）==========
    template<typename T>
    void sort(T* first, T* last) const {
        size_t n = last - first;
        if (UNLIKELY(n <= 1)) return;
        static_assert(std::is_integral<T>::value, "RadixSort only supports integral types");
        if (n < SMALL_SORT_THRESHOLD) { std::sort(first, last); return; }

        T minv = first[0], maxv = first[0];
        bool inc = true, dec = true;
        for (size_t i = 1; i < n; ++i) {
            T v = first[i]; if (v < minv) minv = v; if (v > maxv) maxv = v;
            if (inc && v < first[i-1]) inc = false; if (dec && v > first[i-1]) dec = false;
            if (!inc && !dec) { for (++i; i < n; ++i) { T t = first[i]; if (t < minv) minv = t; if (t > maxv) maxv = t; } break; }
        }
        if (inc) return; if (dec) { std::reverse(first, last); return; }

        using U = UT<T>;
        if constexpr (std::is_signed_v<T>) {
            constexpr U sb = U(1) << (bitWidth<T>() - 1);
            for (size_t i = 0; i < n; ++i) ((U*)first)[i] ^= sb;
            std::vector<int> ps; genPasses(bitWidth<T>(), ps);
            std::vector<T> temp(n);
            dynamicRadixSort(first, last, temp.data(), ps);
            for (size_t i = 0; i < n; ++i) ((U*)first)[i] ^= sb;
        } else {
            U ur = U(maxv) - U(minv); if (ur == 0) return;
            int ib = 0; while (ur) { ++ib; ur >>= 1; }
            std::vector<int> ps; genPasses(ib, ps);
            std::vector<T> temp(n);
            dynamicRadixSort(first, last, temp.data(), ps);
        }
    }

    void sort(float* first, float* last) const { sortFloat(first, last); }
    void sort(double* first, double* last) const { sortFloat(first, last); }
    void sort(char* first, char* last) const { sortChar(first, last); }
    void sort(signed char* first, signed char* last) const { sortChar((char*)first, (char*)last); }
    void sort(unsigned char* first, unsigned char* last) const { sortChar((char*)first, (char*)last); }
    void sort(std::string* first, std::string* last) const { sortStrings(first, last); }
    void sort(std::vector<std::string>& v) const { sort(v.data(), v.data() + v.size()); }

    template<typename RandomIt> void sort(RandomIt first, RandomIt last) const {
        using VT = typename std::iterator_traits<RandomIt>::value_type;
        if constexpr (std::is_integral_v<VT> || std::is_floating_point_v<VT>)
            sort((VT*)&*first, (VT*)&*first + std::distance(first, last));
        else if constexpr (std::is_same_v<VT, std::string>) sort(&*first, &*first + std::distance(first, last));
        else std::sort(first, last);
    }
    template<typename T> void sort(std::vector<T>& v) const { sort(v.data(), v.data() + v.size()); }

    template<typename RandomIt, typename KeyFunc> void sort(RandomIt first, RandomIt last, KeyFunc key) const {
        using T = typename std::iterator_traits<RandomIt>::value_type;
        using KeyType = std::invoke_result_t<KeyFunc, T>;
        static_assert(std::is_arithmetic_v<KeyType>);
        size_t n = std::distance(first, last); if (n <= 1) return;
        if (n < SMALL_SORT_THRESHOLD) { std::sort(first, last, [&](const T& a, const T& b){ return key(a) < key(b); }); return; }
        using UKey = decltype(to_unsigned_key(std::declval<KeyType>()));
        std::vector<UKey> keys(n); std::vector<size_t> indices(n);
        for (size_t i = 0; i < n; ++i) { indices[i] = i; keys[i] = to_unsigned_key(key(*(first + i))); }
        sortKeysAndIndices(keys.data(), indices.data(), n);
        std::vector<T> temp(n);
        for (size_t i = 0; i < n; ++i) temp[i] = std::move(*(first + indices[i]));
        for (size_t i = 0; i < n; ++i) *(first + i) = std::move(temp[i]);
    }
    template<typename T, typename KeyFunc> void sort(std::vector<T>& v, KeyFunc key) const { sort(v.begin(), v.end(), key); }

    template<typename T> void operator()(std::vector<T>& v) const { sort(v); }
    template<typename RandomIt> void operator()(RandomIt first, RandomIt last) const { sort(first, last); }
    template<typename T, typename KeyFunc> void operator()(std::vector<T>& v, KeyFunc key) const { sort(v, key); }
    template<typename RandomIt, typename KeyFunc> void operator()(RandomIt first, RandomIt last, KeyFunc key) const { sort(first, last, key); }
};

#endif



