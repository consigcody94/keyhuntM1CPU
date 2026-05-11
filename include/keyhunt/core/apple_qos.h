/**
 * @file apple_qos.h
 * @brief Apple Silicon QoS (Quality of Service) thread scheduling
 *
 * Provides utilities for pinning threads to specific core types on
 * Apple Silicon M-series chips. M5 introduces a three-tier architecture:
 *   - Super Cores: Highest single-thread performance
 *   - Performance Cores: Balanced multi-thread workloads
 *   - Efficiency Cores: Background/low-priority tasks
 *
 * QoS mapping:
 *   QOS_CLASS_USER_INTERACTIVE -> Super Cores (BSGS search threads)
 *   QOS_CLASS_USER_INITIATED   -> Performance Cores (bloom filter I/O)
 *   QOS_CLASS_UTILITY          -> Efficiency Cores (progress, logging)
 *   QOS_CLASS_BACKGROUND       -> Efficiency Cores (file I/O)
 */

#ifndef KEYHUNT_CORE_APPLE_QOS_H
#define KEYHUNT_CORE_APPLE_QOS_H

#include <cstdint>
#include <cstdio>

#if defined(__APPLE__)
#include <pthread.h>
#include <sys/sysctl.h>
#include <sys/qos.h>
#endif

namespace keyhunt {
namespace core {

/**
 * @brief Thread QoS class for M5 core assignment
 */
enum class ThreadQoS {
    SUPER,        // QOS_CLASS_USER_INTERACTIVE - Super Cores
    PERFORMANCE,  // QOS_CLASS_USER_INITIATED   - Performance Cores
    UTILITY,      // QOS_CLASS_UTILITY          - Efficiency Cores
    BACKGROUND    // QOS_CLASS_BACKGROUND       - Efficiency Cores (lowest)
};

/**
 * @brief Apple Silicon topology information
 */
struct AppleSiliconTopology {
    int total_cores = 0;
    int perf_levels = 0;       // Number of performance levels (2 for M1-M4, 3 for M5)
    int super_cores = 0;       // Level 0 cores (Super on M5, Performance on M1-M4)
    int performance_cores = 0; // Level 1 cores
    int efficiency_cores = 0;  // Level 2 cores (or level 1 on M1-M4)
    bool has_three_tiers = false;

    void print() const {
        printf("\n=== Apple Silicon Topology ===\n");
        printf("Total cores:      %d\n", total_cores);
        printf("Performance levels: %d%s\n", perf_levels,
               has_three_tiers ? " (three-tier / M5)" : " (two-tier)");
        if (has_three_tiers) {
            printf("Super cores:      %d\n", super_cores);
            printf("Performance cores: %d\n", performance_cores);
            printf("Efficiency cores:  %d\n", efficiency_cores);
        } else {
            printf("Performance cores: %d\n", super_cores + performance_cores);
            printf("Efficiency cores:  %d\n", efficiency_cores);
        }
        printf("=============================\n\n");
    }
};

/**
 * @brief Detect Apple Silicon core topology
 */
inline AppleSiliconTopology detect_topology() {
    AppleSiliconTopology topo;

#if defined(__APPLE__)
    size_t size;

    // Total logical CPUs
    int ncpu = 0;
    size = sizeof(ncpu);
    if (sysctlbyname("hw.logicalcpu", &ncpu, &size, NULL, 0) == 0) {
        topo.total_cores = ncpu;
    }

    // Number of performance levels
    int nperflevels = 0;
    size = sizeof(nperflevels);
    if (sysctlbyname("hw.nperflevels", &nperflevels, &size, NULL, 0) == 0) {
        topo.perf_levels = nperflevels;
        topo.has_three_tiers = (nperflevels >= 3);
    }

    // Query per-level core counts
    // perflevel0 = highest performance tier (Super on M5, P-cores on M1-M4)
    // perflevel1 = mid tier (P-cores on M5, E-cores on M1-M4)
    // perflevel2 = lowest tier (E-cores on M5)
    int level0_cpus = 0, level1_cpus = 0, level2_cpus = 0;
    size = sizeof(int);

    sysctlbyname("hw.perflevel0.logicalcpu", &level0_cpus, &size, NULL, 0);
    sysctlbyname("hw.perflevel1.logicalcpu", &level1_cpus, &size, NULL, 0);

    if (topo.has_three_tiers) {
        sysctlbyname("hw.perflevel2.logicalcpu", &level2_cpus, &size, NULL, 0);
        topo.super_cores = level0_cpus;
        topo.performance_cores = level1_cpus;
        topo.efficiency_cores = level2_cpus;
    } else {
        topo.super_cores = 0;
        topo.performance_cores = level0_cpus;
        topo.efficiency_cores = level1_cpus;
    }
#endif

    return topo;
}

/**
 * @brief Set the QoS class for the current thread
 *
 * On M5, this influences which core tier the OS scheduler prefers:
 *   SUPER       -> QOS_CLASS_USER_INTERACTIVE (Super Cores)
 *   PERFORMANCE -> QOS_CLASS_USER_INITIATED   (Performance Cores)
 *   UTILITY     -> QOS_CLASS_UTILITY          (Efficiency Cores)
 *   BACKGROUND  -> QOS_CLASS_BACKGROUND       (Efficiency Cores)
 */
inline bool set_thread_qos(ThreadQoS qos) {
#if defined(__APPLE__)
    qos_class_t qos_class;
    switch (qos) {
        case ThreadQoS::SUPER:
            qos_class = QOS_CLASS_USER_INTERACTIVE;
            break;
        case ThreadQoS::PERFORMANCE:
            qos_class = QOS_CLASS_USER_INITIATED;
            break;
        case ThreadQoS::UTILITY:
            qos_class = QOS_CLASS_UTILITY;
            break;
        case ThreadQoS::BACKGROUND:
            qos_class = QOS_CLASS_BACKGROUND;
            break;
    }

    return pthread_set_qos_class_self_np(qos_class, 0) == 0;
#else
    (void)qos;
    return false;
#endif
}

/**
 * @brief Get recommended thread counts for each QoS tier
 */
inline void get_recommended_threads(const AppleSiliconTopology &topo,
                                     int &search_threads,
                                     int &io_threads,
                                     int &bg_threads) {
    if (topo.has_three_tiers) {
        // M5: Use Super + Performance for search, Efficiency for background
        search_threads = topo.super_cores + topo.performance_cores;
        io_threads = 1;  // One efficiency core for I/O
        bg_threads = topo.efficiency_cores > 1 ? topo.efficiency_cores - 1 : 0;
    } else {
        // M1-M4: Use Performance for search, Efficiency for background
        search_threads = topo.performance_cores;
        io_threads = 1;
        bg_threads = topo.efficiency_cores > 1 ? topo.efficiency_cores - 1 : 0;
    }
}

} // namespace core
} // namespace keyhunt

#endif // KEYHUNT_CORE_APPLE_QOS_H
