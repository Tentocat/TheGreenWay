// Copyright (c) 2015-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <bench/perf.h>

#include <assert.h>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <numeric>

void benchmark::ConsolePrinter::header()
{
    std::cout << "# Benchmark, evals, iterations, total, min, max, median" << std::endl;
}

void benchmark::ConsolePrinter::result(const State& state)
{
    auto results = state.m_elapsed_results;
    std::sort(results.begin(), results.end());

    double total = state.m_num_iters * std::accumulate(results.begin(), results.end(), 0.0);

    double front = 0;
    double back = 0;
    double median = 0;

    if (!results.empty()) {
        front = results.front();
        back = results.back();

        size_t mid 