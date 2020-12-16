// Copyright 2020 Igor Rukhovich
#include "../../modules/task_3/rukhovich_i_radix_sort_batcher/radix_sort_batcher.h"
#include <mpi.h>

double RandomDouble::Next() {
    static RandomDouble rand = RandomDouble();
    return rand.dist_(rand.gen_);
}

RandomDouble::RandomDouble() : gen_(std::random_device()()), dist_(-1e3, 1e3) {}

std::vector<double> random_double_array(size_t size) {
    std::vector<double> arr(size);

    for (size_t i = 0; i < size; ++i) {
        arr[i] = RandomDouble::Next();
    }

    return arr;
}

void bitwise_sort_mod(BitsetIt first, BitsetIt last, size_t bit, bool positive = true) {
    auto le = first, ri = last;
    if (le >= ri || le + 1 == ri) return;

    while (le < last && ((*le)[bit] != positive)) ++le;
    if (le == last) {
        if (bit == 0u) return;
        bitwise_sort_mod(first, last, bit - 1u, positive);
        return;
    }

    --ri;
    while (le < ri) {
        while (le < ri && ((*ri)[bit] == positive)) --ri;
        if (le == ri) break;

        std::swap(*le, *ri);
        while (le < ri && ((*le)[bit] != positive)) ++le;
    }

    if (bit == 0u) return;
    bitwise_sort_mod(first, le, bit - 1u, positive);
    bitwise_sort_mod(le, last, bit - 1u, positive);
}

void bitwise_sort(BitsetIt first, BitsetIt last) {
    auto le = first, ri = last;
    if (le >= ri || le + 1 == ri) return;

    while (le < last && (*le)[63u]) ++le;
    if (le == last) {
        bitwise_sort_mod(first, last, 62u, false);
        return;
    }

    --ri;
    while (le < ri) {
        while (le < ri && !(*ri)[63u]) --ri;
        if (le == ri) break;

        std::swap(*le, *ri);
        while (le < ri && (*le)[63u]) ++le;
    }

    bitwise_sort_mod(first, le, 62u, false);
    bitwise_sort_mod(le, last, 62u, true);
}

template<> void radix_sort(DoubleIt first, DoubleIt last) {
    if (first >= last) return;

    std::vector<std::bitset<64>> bits(last - first);
    auto it = first;
    for (size_t i = 0u; i < bits.size(); ++it, ++i) {
        bits[i] = reinterpret_cast<uint64_t &>(*it);
    }

    bitwise_sort(bits.begin(), bits.end());

    it = first;
    for (size_t i = 0; i < bits.size(); ++it, ++i) {
        uint64_t num = bits[i].to_ullong();
        *it = reinterpret_cast<double &>(num);
    }
}

void odd_even_merge(DoubleIt first, DoubleIt last) {
    size_t size = last - first, half = size / 2u;
    if (size < 2u) return;
    if (size == 2u) {
        if (*first > *(first + 1u)) {
            std::swap(*first, *(first + 1u));
        }
        return;
    }
    std::vector<double> arr_cpy(size);

    for (size_t i = 0u; i < half; ++i) {
        arr_cpy[i << 1u] = *(first + i);
        arr_cpy[(i << 1u) + 1u] = *(first + i + half);
    }

    odd_even_merge(arr_cpy.begin(), arr_cpy.begin() + half);
    odd_even_merge(arr_cpy.begin() + half, arr_cpy.begin() + size);

    for (size_t i = 0u; i < half; ++i) {
        *(first + i) = arr_cpy[i << 1u];
        *(first + i + half) = arr_cpy[(i << 1u) + 1u];
    }
}

template<> void par_radix_sort_batcher(DoubleIt first, DoubleIt last) {
    int num_proc, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Status status;

    uint64_t size = (rank == 0) ? static_cast<uint64_t>(last - first) : 0u;
    MPI_Bcast(&size, 1, MPI_UNSIGNED_LONG_LONG, 0, MPI_COMM_WORLD);

    std::vector<int> sendcounts(num_proc, size / num_proc), displs(num_proc);
    sendcounts.back() += size % num_proc;
    for (size_t i = 0; i < static_cast<size_t>(size); ++i) {
        displs[i] = (size / num_proc) * i;
    }

    size_t pow = 0;
    while (1u << pow < sendcounts[rank]) ++pow;
    std::vector<double> arr_part(1u << pow, HUGE_VAL);

    MPI_Scatterv(&*first, sendcounts.data(), displs.data(), MPI_DOUBLE,
                 arr_part.data(), sendcounts[rank], MPI_DOUBLE,
                 0, MPI_COMM_WORLD);

    radix_sort(arr_part.begin(), arr_part.end());

    int thread_power = 1;
    while (true) {
        if (rank % (thread_power << 1) == 0) {
            if (num_proc < rank + thread_power) {
                ++thread_power;
                continue;
            }
            size_t cur_size = arr_part.size();
            arr_part.resize(cur_size << 1u);
            MPI_Recv(arr_part.data() + cur_size, cur_size, MPI_DOUBLE,
                     rank + thread_power, 0, MPI_COMM_WORLD, &status);
            odd_even_merge(arr_part.begin(), arr_part.end());
            ++thread_power;
        } else {
            MPI_Send(arr_part.data(), arr_part.size(), MPI_DOUBLE,
                     rank - thread_power, 0, MPI_COMM_WORLD);
            break;
        }
    }

    if (rank == 0) {
        for (size_t i = 0; i < size; ++i) {
            *(first + i) = arr_part[i];
        }
    }
}