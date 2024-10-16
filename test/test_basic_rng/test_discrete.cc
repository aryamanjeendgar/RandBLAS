// Copyright, 2024. See LICENSE for copyright holder information.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// (1) Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// (2) Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// (3) Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

#include "RandBLAS/config.h"
#include "RandBLAS/base.hh"
#include "RandBLAS/util.hh"
#include <RandBLAS/random_gen.hh>
#include <RandBLAS/exceptions.hh>
#include <RandBLAS/sparse_skops.hh>
using RandBLAS::RNGState;
using RandBLAS::weights_to_cdf;
using RandBLAS::sample_indices_iid;
using RandBLAS::sample_indices_iid_uniform;
using RandBLAS::repeated_fisher_yates;
#include "rng_common.hh"
#include "../comparison.hh"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <random>
#include <set>
#include <vector>
#include <string>
#include <limits>
#include <cstdint>
#include <numeric>
#include <tuple>
#include <cassert>
#include <gtest/gtest.h>

using std::vector;


class TestSampleIndices : public ::testing::Test
{
    protected:
    
    virtual void SetUp(){};

    virtual void TearDown(){};

    //
    // MARK: With Replacement
    //

    static void test_iid_uniform_smoke(int64_t N, int64_t k, uint32_t seed) { 
        RNGState state(seed);
        vector<int64_t> samples(k, -1);
        sample_indices_iid_uniform(N, k, samples.data(), state);
        int64_t* data = samples.data();
        for (int64_t i = 0; i < k; ++i) {
            ASSERT_LT(data[i], N);
            ASSERT_GE(data[i], 0);
        }
        return;
    }

    static void index_set_kolmogorov_smirnov_tester(
        vector<int64_t> &samples, vector<float> &true_cdf, double critical_value
    ) {
        auto N = (int64_t) true_cdf.size();
        vector<float> sample_cdf(N, 0.0);
        for (int64_t s : samples)
            sample_cdf[s] += 1;
        weights_to_cdf(N, sample_cdf.data());

        for (int i = 0; i < N; ++i) {
            float F_empirical = sample_cdf[i];
            float F_true = true_cdf[i];
            auto diff = (double) std::abs(F_empirical - F_true);
            ASSERT_LT(diff, critical_value);
        }
        return;
    }

    static void test_iid_uniform_kolmogorov_smirnov(int64_t N, double significance, int64_t num_samples, uint32_t seed) {
        using RandBLAS_StatTests::KolmogorovSmirnovConstants::critical_value_rep_mutator;
        auto critical_value = critical_value_rep_mutator(num_samples, significance);

        vector<float> true_cdf(N, 1.0);
        weights_to_cdf(N, true_cdf.data());

        RNGState state(seed);
        vector<int64_t> samples(num_samples, -1);
        sample_indices_iid_uniform(N, num_samples, samples.data(), state);

        index_set_kolmogorov_smirnov_tester(samples, true_cdf, critical_value);
        return;
    }

    static void test_iid_kolmogorov_smirnov(int64_t N, float exponent, double significance, int64_t num_samples, uint32_t seed) {
        using RandBLAS_StatTests::KolmogorovSmirnovConstants::critical_value_rep_mutator;
        auto critical_value = critical_value_rep_mutator(num_samples, significance);

        // Make the true CDF 
        vector<float> true_cdf{};
        for (int i = 0; i < N; ++i)
            true_cdf.push_back(std::pow(1.0/((float)i + 1.0), exponent));
        weights_to_cdf(N, true_cdf.data());

        RNGState state(seed);
        vector<int64_t> samples(num_samples, -1);
        sample_indices_iid(N, true_cdf.data(), num_samples, samples.data(), state);

        index_set_kolmogorov_smirnov_tester(samples, true_cdf, critical_value);
        return;
    }

    static void test_iid_degenerate_distributions(uint32_t seed) {
        int64_t N = 100;
        int64_t num_samples = N*N;
        vector<int64_t> samples(num_samples, -1);
        RNGState state(seed);

        // Test case 1: distribution is nonuniform, with mass only on even elements != 10.
        vector<float> true_cdf(N, 0.0);
        for (int i = 0; i < N; i = i + 2)
            true_cdf[i] = 1.0f / ((float) i + 1.0f);
        true_cdf[10] = 0.0;
        weights_to_cdf(N, true_cdf.data());
        sample_indices_iid(N, true_cdf.data(), num_samples, samples.data(), state);
        for (auto s : samples) {
            ASSERT_FALSE(s == 10 || s % 2 == 1) << "s = " << s;
        }

        // Test case 2: distribution is trivial (a delta function),
        // and a negative weight needs to be clipped without error.
        std::fill(true_cdf.begin(), true_cdf.end(), 0.0);
        std::fill(samples.begin(), samples.end(), -1);
        true_cdf[17] = 99.0f;
        true_cdf[3]  = -std::numeric_limits<float>::epsilon()/10;
        randblas_require(true_cdf[3] < 0);
        weights_to_cdf(N, true_cdf.data());
        ASSERT_GE(true_cdf[17], 0.0f);
        sample_indices_iid(N, true_cdf.data(), num_samples, samples.data(), state);
        for (auto s : samples) {
            ASSERT_EQ(s, 17);
        }
        return;
    }

    static void test_updated_rngstates_iid_uniform() {
        RNGState seed;
        int offset = 3456;
        seed.counter.incr(offset);
        int n = 40;
        int k = 17;
        vector<int> unimportant(2*k);
        auto s1 = sample_indices_iid_uniform(n, k, unimportant.data(), seed);
        auto s2 = sample_indices_iid_uniform(n, k, unimportant.data(), s1);
        // check that counter increments are the same for the two samples of k indices.
        auto total_2call = s2.counter.v[0];
        EXPECT_EQ(total_2call-offset, 2*(s1.counter.v[0]-offset));

        // check that the counter increment for a single sample of size 2k is (a) no larger
        // than the total increment for two samples of size k, and (b) is at most one less
        // than the total increment for two samples of size k.
        auto t = sample_indices_iid_uniform(n, 2*k, unimportant.data(), seed);
        auto total_1call = t.counter.v[0];
        EXPECT_LE( total_1call, total_2call    );
        EXPECT_LE( total_2call, total_1call + 1);
    }

    static void test_updated_rngstates_iid() {
        RNGState seed;
        int offset = 8675309;
        seed.counter.incr(offset);
        int n = 29;
        int k = 13;
        vector<int> unimportant(2*k);
        vector<float> cdf(n, 1.0);
        weights_to_cdf(n, cdf.data());

        auto s1 = sample_indices_iid(n, cdf.data(), k, unimportant.data(), seed);
        auto s2 = sample_indices_iid(n, cdf.data(), k, unimportant.data(), s1);
        // check that counter increments are the same for the two samples of k indices.
        auto total_2call = s2.counter.v[0];
        EXPECT_EQ(total_2call-offset, 2*(s1.counter.v[0]-offset));

        // check that the counter increment for a single sample of size 2k is (a) no larger
        // than the total increment for two samples of size k, and (b) is at most one less
        // than the total increment for two samples of size k.
        auto t = sample_indices_iid(n, cdf.data(), 2*k, unimportant.data(), seed);
        auto total_1call = t.counter.v[0];
        EXPECT_LE( total_1call, total_2call    );
        EXPECT_LE( total_2call, total_1call + 1);
    }

    //
    // MARK: Without Replacement
    //

    static vector<float> fisher_yates_cdf(const vector<int64_t> &idxs_major, int64_t K, int64_t num_samples) {
        vector<float> empirical_cdf;

        // If K is 0, then there's nothing to count over and we should just return 1
        if (K == 0) {
            empirical_cdf.push_back(1.0);
        } else {
            // Count how many values in idxs_major are less than K across the samples
            vector<int64_t> counter(K + 1, 0);
            for (int64_t i = 0; i < K * num_samples; i += K) {
                int count = 0;
                for (int64_t j = 0; j < K; ++j) {
                    if (idxs_major[i + j] < K)
                        count += 1;
                }
                counter[count] += 1;
            }
            // Copy counter and normalize to get empirical_cdf
            empirical_cdf.resize(counter.size());
            std::copy(counter.begin(), counter.end(), empirical_cdf.begin());
            weights_to_cdf(empirical_cdf.size(), empirical_cdf.data());
        }

        return empirical_cdf;
    }

    static void fisher_yates_kolmogorov_smirnov_tester(
        const vector<int64_t> &idxs_major, vector<float> &true_cdf, double critical_value, int64_t N, int64_t K, int64_t num_samples
    ) {
        using RandBLAS_StatTests::ks_check_critval;
        // Calculate the empirical cdf and check critval
        vector<float> empirical_cdf = fisher_yates_cdf(idxs_major, K, num_samples);
        std::pair<int, double> result = ks_check_critval(true_cdf, empirical_cdf, critical_value);
        EXPECT_EQ(result.first, -1) 
            << "\nKS test failed at index " << result.first 
            << " with difference " << result.second << " and critical value " << critical_value
            << "\nTest parameters: " << "N=" << N << " " << "K=" << K << " " << "num_samples=" << num_samples << std::endl;
    }

    static void single_test_fisher_yates_kolmogorov_smirnov(int64_t N, int64_t K, double significance, int64_t num_samples, uint32_t seed) {
        using RandBLAS_StatTests::hypergeometric_pmf_arr;
        using RandBLAS_StatTests::KolmogorovSmirnovConstants::critical_value_rep_mutator;

        auto critical_value = critical_value_rep_mutator(num_samples, significance);

        // Initialize arguments for repeated_fisher_yates
        vector<int64_t> indices(K * num_samples);
        RNGState state(seed);

        // Generate repeated Fisher-Yates in idxs_major
        state = repeated_fisher_yates(K, N, num_samples, indices.data(), state);

        // Generate the true hypergeometric cdf (get the pdf first)
        vector<float> true_cdf = hypergeometric_pmf_arr<float>(N, K, K);
        weights_to_cdf(true_cdf.size(), true_cdf.data());

        // Compute the critval and check against it
        fisher_yates_kolmogorov_smirnov_tester(indices, true_cdf, critical_value, N, K, num_samples);

        return;
    }

    static void incr_with_phase_transitions(int64_t &K, int64_t unit_bound = 10, int64_t sqrt_bound = 100) {
        if (K < unit_bound) {
            K += 1;
        } else if (K < sqrt_bound) {
            // Step in square root scale up to sqrt_bound
            K += (int64_t) std::pow(std::sqrt(K) + 1, 2);
        } else {
            // Step in log-scale after sqrt_bound
            // Log base to give 5 steps for each order of magnitude
            K *= std::pow(10.0, 0.2);
        }
        return;
    }

    static void test_fisher_yates_kolmogorov_smirnov(int64_t N, double significance, int64_t num_samples, uint32_t seed) {
        int64_t K = 0;
        while (K <= N) {
            single_test_fisher_yates_kolmogorov_smirnov(N, K, significance, num_samples, seed);
            incr_with_phase_transitions(K);
        }
    }

    static void test_updated_rngstates_fisher_yates() {
        RNGState seed;
        int offset = 306;
        seed.counter.incr(offset);
        int n = 29;
        int k = 17;
        int r1 = 1;
        int r2 = 3;
        int r_total  = r1 + r2;
        vector<int> twocall(r_total*k);
        vector<int> onecall(r_total*k);

        auto s1 = repeated_fisher_yates(k, n, r1, twocall.data(), seed);
        auto s2 = repeated_fisher_yates(k, n, r2, twocall.data() + r1*k, s1);
        auto ctr_twocall = (int) s2.counter.v[0];
        auto expect_incr = (int) std::ceil(((float)r_total/r1)*(s1.counter.v[0]-offset));
        EXPECT_EQ(ctr_twocall - offset, expect_incr);

        auto t = repeated_fisher_yates(k, n, r_total, onecall.data(), seed);
        auto ctr_onecall = t.counter.v[0];
        EXPECT_EQ( ctr_onecall, ctr_twocall );

        test::comparison::buffs_approx_equal(
            onecall.data(), twocall.data(), r_total*k, __PRETTY_FUNCTION__, __FILE__, __LINE__
        );
    }

};

TEST_F(TestSampleIndices, rngstate_updates_iid_uniform) {
    test_updated_rngstates_iid_uniform();
}

TEST_F(TestSampleIndices, rngstate_updates_iid) {
    test_updated_rngstates_iid();
}

TEST_F(TestSampleIndices, rngstate_updates_fisher_yates) {
    test_updated_rngstates_fisher_yates();
}


TEST_F(TestSampleIndices, smoke_3_x_10) {
    for (uint32_t i = 0; i < 10; ++i)
        test_iid_uniform_smoke(3, 10, i);
}

TEST_F(TestSampleIndices, smoke_10_x_3) {
    for (uint32_t i = 0; i < 10; ++i)
        test_iid_uniform_smoke(10, 3, i);
}

TEST_F(TestSampleIndices, smoke_med) {
    for (uint32_t i = 0; i < 10; ++i)
        test_iid_uniform_smoke((int) 1e6 , 6000, i);
}

TEST_F(TestSampleIndices, smoke_big) {
    int64_t huge_N = std::numeric_limits<int64_t>::max() / 2;
    for (uint32_t i = 0; i < 10; ++i)
        test_iid_uniform_smoke(huge_N, 1000, i);
}

TEST_F(TestSampleIndices, support_of_degenerate_distributions) {
    for (uint32_t i = 789; i < 799; ++i)
        test_iid_degenerate_distributions(i);
}

TEST_F(TestSampleIndices, iid_uniform_ks_generous) {
    double s = 1e-6;
    test_iid_uniform_kolmogorov_smirnov(100,     s, 100000, 0);
    test_iid_uniform_kolmogorov_smirnov(10000,   s, 1000,   0);
    test_iid_uniform_kolmogorov_smirnov(1000000, s, 1000,   0);
}

TEST_F(TestSampleIndices, iid_uniform_ks_moderate) {
    float s = 1e-4;
    test_iid_uniform_kolmogorov_smirnov(100,     s, 100000, 0);
    test_iid_uniform_kolmogorov_smirnov(10000,   s, 1000,   0);
    test_iid_uniform_kolmogorov_smirnov(1000000, s, 1000,   0);
}

TEST_F(TestSampleIndices, iid_uniform_ks_skeptical) {
    float s = 1e-2;
    test_iid_uniform_kolmogorov_smirnov(100,     s, 100000, 0);
    test_iid_uniform_kolmogorov_smirnov(10000,   s, 1000,   0);
    test_iid_uniform_kolmogorov_smirnov(1000000, s, 1000,   0);
}


TEST_F(TestSampleIndices, iid_ks_generous) {
    double s = 1e-6;
    test_iid_kolmogorov_smirnov(100,      1, s, 100000, 0);
    test_iid_kolmogorov_smirnov(10000,    1, s, 1000,   0);
    test_iid_kolmogorov_smirnov(1000000,  1, s, 1000,   0);
    test_iid_kolmogorov_smirnov(100,      3, s, 100000, 0);
    test_iid_kolmogorov_smirnov(10000,    3, s, 1000,   0);
    test_iid_kolmogorov_smirnov(1000000,  3, s, 1000,   0);
}

TEST_F(TestSampleIndices, iid_ks_moderate) {
    float s = 1e-4;
    test_iid_kolmogorov_smirnov(100,      1, s, 100000, 0);
    test_iid_kolmogorov_smirnov(10000,    1, s, 1000,   0);
    test_iid_kolmogorov_smirnov(1000000,  1, s, 1000,   0);
    test_iid_kolmogorov_smirnov(100,      3, s, 100000, 0);
    test_iid_kolmogorov_smirnov(10000,    3, s, 1000,   0);
    test_iid_kolmogorov_smirnov(1000000,  3, s, 1000,   0);
}

TEST_F(TestSampleIndices, iid_ks_skeptical) {
    float s = 1e-2;
    test_iid_kolmogorov_smirnov(100,      1, s, 100000, 0);
    test_iid_kolmogorov_smirnov(10000,    1, s, 1000,   0);
    test_iid_kolmogorov_smirnov(1000000,  1, s, 1000,   0);
    test_iid_kolmogorov_smirnov(100,      3, s, 100000, 0);
    test_iid_kolmogorov_smirnov(10000,    3, s, 1000,   0);
    test_iid_kolmogorov_smirnov(1000000,  3, s, 1000,   0);
}


TEST_F(TestSampleIndices, fisher_yates_ks_generous)
{
    double s = 1e-6;
    test_fisher_yates_kolmogorov_smirnov(10, s, 10000, 0);
    test_fisher_yates_kolmogorov_smirnov(100, s, 10000, 0);
    test_fisher_yates_kolmogorov_smirnov(1000, s, 1000, 0);
}

TEST_F(TestSampleIndices, fisher_yates_ks_moderate)
{
    double s = 1e-4;
    test_fisher_yates_kolmogorov_smirnov(10, s, 10000, 0);
    test_fisher_yates_kolmogorov_smirnov(100, s, 10000, 0);
    test_fisher_yates_kolmogorov_smirnov(1000, s, 1000, 0);
}

TEST_F(TestSampleIndices, fisher_yates_ks_skeptical)
{
    double s = 1e-2;
    test_fisher_yates_kolmogorov_smirnov(10, s, 10000, 0);
    test_fisher_yates_kolmogorov_smirnov(100, s, 10000, 0);
    test_fisher_yates_kolmogorov_smirnov(1000, s, 1000, 0);
}
