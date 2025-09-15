#include "../include/MemoryPool.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <numeric>
#include <iomanip>
#include <random>

// 使用你的内存池命名空间和 chrono
using namespace llt_memoryPool;
using namespace std::chrono;

// 定义一个操作的类型
enum class AllocOpType { ALLOC, DEALLOC };

// 定义一个操作指令
struct AllocOp {
    AllocOpType type;
    size_t      size;  // For ALLOC
    size_t      index; // For DEALLOC
};

// --- 核心测试函数 (保持不变) ---
void worker_thread_task(const std::vector<AllocOp>& ops, bool use_mem_pool, size_t num_total_ops) {
    std::vector<std::pair<void*, size_t>> ptrs;
    ptrs.reserve(num_total_ops); // 预分配足够大的空间

    for (const auto& op : ops) {
        if (op.type == AllocOpType::ALLOC) {
            void* p;
            if (use_mem_pool) {
                p = MemoryPool::allocate(op.size);
            } else {
                p = new char[op.size];
            }
            ptrs.emplace_back(p, op.size);
        } else { // DEALLOC
            if (!ptrs.empty()) {
                // 为了简化，总是从随机位置释放
                size_t idx_to_free = op.index % ptrs.size();
                
                void* p_to_free = ptrs[idx_to_free].first;
                size_t size_to_free = ptrs[idx_to_free].second;
                
                if (use_mem_pool) {
                    MemoryPool::deallocate(p_to_free, size_to_free);
                } else {
                    delete[] static_cast<char*>(p_to_free);
                }
                // 快速删除：用最后一个元素覆盖被删除的元素
                ptrs[idx_to_free] = ptrs.back();
                ptrs.pop_back();
            }
        }
    }

    // 清理所有剩余的内存
    for (const auto& alloc_pair : ptrs) {
        if (use_mem_pool) {
            MemoryPool::deallocate(alloc_pair.first, alloc_pair.second);
        } else {
            delete[] static_cast<char*>(alloc_pair.first);
        }
    }
}

// --- 测试启动器 (使用 std::chrono) ---
void run_performance_test(const std::string& test_name, 
                          size_t num_threads, 
                          const std::vector<std::vector<AllocOp>>& workload,
                          size_t num_total_ops) {
    
    // --- 测试 Memory Pool ---
    auto start_pool = high_resolution_clock::now();
    std::vector<std::thread> pool_threads;
    for (size_t i = 0; i < num_threads; ++i) {
        pool_threads.emplace_back(worker_thread_task, std::cref(workload[i]), true, num_total_ops);
    }
    for (auto& t : pool_threads) { t.join(); }
    auto end_pool = high_resolution_clock::now();
    double pool_ms = duration_cast<microseconds>(end_pool - start_pool).count() / 1000.0;
    std::cout << test_name << "Memory Pool: " << std::fixed << std::setprecision(3) << pool_ms << " ms" << std::endl;

    // --- 测试 New/Delete ---
    auto start_sys = high_resolution_clock::now();
    std::vector<std::thread> sys_threads;
    for (size_t i = 0; i < num_threads; ++i) {
        sys_threads.emplace_back(worker_thread_task, std::cref(workload[i]), false, num_total_ops);
    }
    for (auto& t : sys_threads) { t.join(); }
    auto end_sys = high_resolution_clock::now();
    double sys_ms = duration_cast<microseconds>(end_sys - start_sys).count() / 1000.0;
    std::cout << test_name << "New/Delete:  " << std::fixed << std::setprecision(3) << sys_ms << " ms" << std::endl;
}

// --- 生成测试负载 ---
std::vector<std::vector<AllocOp>> generate_workload(size_t num_threads, size_t num_ops_per_thread) {
    std::vector<std::vector<AllocOp>> workload(num_threads);
    std::mt19937 gen(12345); // 固定种子
    std::uniform_int_distribution<size_t> size_dis(1, 32); 
    std::uniform_int_distribution<int> op_dis(0, 3);
    std::uniform_int_distribution<size_t> dealloc_idx_dis; // 用于随机释放

    for (size_t i = 0; i < num_threads; ++i) {
        workload[i].reserve(num_ops_per_thread);
        size_t current_allocs = 0;
        for (size_t j = 0; j < num_ops_per_thread; ++j) {
            if (current_allocs < 10 || op_dis(gen) > 0) { // 75% 概率分配 (前期多分配)
                workload[i].push_back({AllocOpType::ALLOC, size_dis(gen) * 8, 0});
                current_allocs++;
            } else { // 25% 概率释放
                workload[i].push_back({AllocOpType::DEALLOC, 0, dealloc_idx_dis(gen)});
                current_allocs--;
            }
        }
    }
    return workload;
}
void warmup(size_t num_threads, size_t ops_per_thread) {
    std::cout << "--- Warming up (" << num_threads << " threads) ---" << std::endl;
    auto workload = generate_workload(num_threads, ops_per_thread);
    
    // 预热内存池，不计时
    std::vector<std::thread> pool_threads;
    for (size_t i = 0; i < num_threads; ++i) 
        pool_threads.emplace_back(worker_thread_task, std::cref(workload[i]), true, ops_per_thread);
    for (auto& t : pool_threads) t.join();
    
    // 预热系统 malloc，不计时
    std::vector<std::thread> sys_threads;
    for (size_t i = 0; i < num_threads; ++i)
        sys_threads.emplace_back(worker_thread_task, std::cref(workload[i]), false, ops_per_thread);
    for (auto& t : sys_threads) t.join();

    std::cout << "Warmup complete.\n" << std::endl;
}
int main() {
    warmup(64, 10000); 
    std::cout << "Starting Fair Performance Tests on a 4-Core Machine...\n" << std::endl;
    
    const size_t TOTAL_OPS = 1000000; // 总操作数
    
    // --- 场景 1: 中等竞争 (4 线程) ---
    const size_t num_threads_medium = 4;
    std::cout << "--- Test Case 1: Medium Contention (" << num_threads_medium << " threads) ---" << std::endl;
    auto workload_medium = generate_workload(num_threads_medium, TOTAL_OPS / num_threads_medium);
    run_performance_test(" ", num_threads_medium, workload_medium, TOTAL_OPS / num_threads_medium);
    std::cout << std::endl;

    // --- 场景 2: 高竞争 (16 线程) ---
    const size_t num_threads_high = 16;
    std::cout << "--- Test Case 2: High Contention (" << num_threads_high << " threads) ---" << std::endl;
    auto workload_high = generate_workload(num_threads_high, TOTAL_OPS / num_threads_high);
    run_performance_test(" ", num_threads_high, workload_high, TOTAL_OPS / num_threads_high);
    std::cout << std::endl;

    // --- 场景 3: 极端竞争 (64 线程) ---
    const size_t num_threads_extreme = 64;
    std::cout << "--- Test Case 3: Extreme Contention (" << num_threads_extreme << " threads) ---" << std::endl;
    auto workload_extreme = generate_workload(num_threads_extreme, TOTAL_OPS / num_threads_extreme);
    run_performance_test(" ", num_threads_extreme, workload_extreme, TOTAL_OPS / num_threads_extreme);
    std::cout << std::endl;

    return 0;
}