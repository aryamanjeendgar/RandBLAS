#include "RandBLAS/base.hh"
#include "RandBLAS/exceptions.hh"
#include "RandBLAS/random_gen.hh"
#include <RandBLAS/sparse_skops.hh>

#include <Random123/philox.h>
#include <blas.hh>
#include <lapack.hh>
#include <omp.h>

#include <iostream>
#include <stdio.h>
#include <stdexcept>
#include <string>
#include <tuple>
#include <random>

#include <math.h>
#include <typeinfo>

#define MAX(a, b) (((a) < (b)) ? (b) : (a))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

namespace RandBLAS {

    // =============================================================================
    /// WARNING: None of the following functions or overloads thereof are part of the
    /// public API
    ///
    template<SignedInteger sint_t>
    // void generateRademacherVector_r123(std::vector<int64_t> &buff, uint32_t key_seed, uint32_t ctr_seed, int64_t n) {
    void generateRademacherVector_r123(sint_t* buff, uint32_t key_seed, uint32_t ctr_seed, int64_t n) {
        typedef r123::Philox2x32 RNG;
        // std::vector<int64_t> rademacherVector(n);

        // Use OpenMP to parallelize the Rademacher vector generation
        #pragma omp parallel
        {
            // Each thread has its own RNG instance
            RNG rng;

            RNG::key_type k = {{key_seed + omp_get_thread_num()}}; // Unique key for each thread

            // Thread-local counter
            RNG::ctr_type c;

            // Parallel for loop
            #pragma omp for
            for (int i = 0; i < n; ++i) {
                // Set the counter for each random number (unique per thread)
                c[0] = ctr_seed + i; // Ensure the counter is unique across threads

                // Generate a 2x32-bit random number using the Philox generator
                RNG::ctr_type r = rng(c, k);

                // Convert the random number into a float in [0, 1) using u01fixedpt
                float randValue = r123::u01fixedpt<float>(r.v[0]);

                // Convert the float into a Rademacher entry (-1 or 1)
                buff[i] = randValue < 0.5 ? -1 : 1;
            }
        }
    }

    std::vector<int64_t> generateRademacherVector_parallel(int64_t n) {
        std::vector<int64_t> rademacherVec(n);

        #pragma omp parallel
        {
            // Each thread gets its own random number generator
            std::random_device rd;
            std::mt19937 gen(rd());
            std::bernoulli_distribution bernoulli(0.5);

            #pragma omp for
            for (int64_t i = 0; i < n; ++i) {
                rademacherVec[i] = bernoulli(gen) ? 1 : -1;
            }
        }

        return rademacherVec;
    }

    template<typename T>
    void applyDiagonalRademacher(int64_t rows, int64_t cols, T* A) {
        std::vector<int64_t> diag = generateRademacherVector_parallel(cols);

        for(int col=0; col < cols; col++) {
        if(diag[col] > 0)
            continue;
        blas::scal(rows, diag[col], &A[col * rows], 1);
    }
    }

    template<typename T, SignedInteger sint_t>
    // void applyDiagonalRademacher(int64_t rows, int64_t cols, T* A, std::vector<int64_t> &diag) {
    void applyDiagonalRademacher(int64_t rows, int64_t cols, T* A, sint_t* diag) {
        //NOTE: Only considers sketching from the left + ColMajor format as of now
        for(int col=0; col < cols; col++) {
        if(diag[col] > 0)
            continue;
        blas::scal(rows, diag[col], &A[col * rows], 1);
    }
    }

    // `applyDiagonalRademacher`should have a similar API as `sketch_general`
    // Will be called inside of `lskget`
    // B <- alpha * diag(Rademacher) * op(A) + beta * B
    // `alpha`, `beta` aren't needed and shape(A) == shape(B)
    // lda == ldb
    template<typename T, SignedInteger sint_t>
    void applyDiagonalRademacher(
        blas::Layout layout,
        blas::Op opA, // taken from `sketch_general/lskget`
        int64_t n, // everything is the same size as `op(A)`: m-by-n
        int64_t m,
        // std::vector<int64_t> &diag,
        sint_t* diag,
        const T* A, // The data matrix that won't be modified
        // int64_t lda,
        T* B // The destination matrix
        // int64_t ldb
    ) {
        // B <- alpha * diag(Rademacher) * op(A) + beta * B
        // `alpha`, `beta` aren't needed
        // && shape(A) == shape(B) && lda == ldb && shape({A | B}) = m-by-n
        //NOTE: Use `layout` and `opA` for working with RowMajor data
        int64_t lda = m;
        int64_t ldb = m;

        //NOTE: Should the copy be made inside of `applyDiagonalRademacher` or
        // should it be inside of `lskget`?
        lapack::lacpy(lapack::MatrixType::General, m, n, A, lda, B, ldb);

        applyDiagonalRademacher(m, n, B, diag);
    }


    // `permuteRowsToTop` should just take in `B` as an input
    // `B` will be passed in by the user to `sketch_general/lskget` and `permuteRowsToTop` will take in
    // the `B` that has been modified by `applyRademacherDiagonal`
    // Will also be called inside of `lskget`
    template<typename T>
    void permuteRowsToTop(int rows, int cols, std::vector<int64_t> selectedRows, T* B, int ldb) {
        //NOTE: There should be a similar `permuteColsToLeft`
        int top = 0;  // Keeps track of the topmost unselected row

        // Compute `r` internally
        // int64_t r = 100;
        // std::vector<int64_t> selectedRows = generateRademacherVector_parallel(r);

        for (int selected : selectedRows) {
            if (selected != top) {
                // Use BLAS swap to swap the entire rows
                // Swapping row 'selected' with row 'top'
                blas::swap(cols, &B[selected], ldb, &B[top], ldb);
            }
            top++;
        }
    }

    enum class TrigDistName: char {
        Fourier = 'F',

        // ---------------------------------------------------------------------------

        Hadamard = 'H'
    };

    struct TrigDist {
        const int64_t n_rows;
        const int64_t n_cols;

        int64_t dim_short;
        int64_t dim_long;

        const TrigDistName family;

        TrigDist(
            int64_t n_rows,
            int64_t n_cols,
            TrigDistName tn = TrigDistName::Hadamard
        ) : n_rows(n_rows), n_cols(n_cols), family(tn) {
            dim_short = MIN(n_rows, n_cols);
            dim_long = MAX(n_rows, n_cols);
}
};

    template<typename T, typename RNG = r123::Philox4x32, SignedInteger sint_t = int64_t>
    struct TrigSkOp {
        using generator = RNG;
        using state_type = RNGState<RNG>;
        using buffer_type = T;

        //TODO: Where should the logic for deciding the size of `H` to use go?
        // Since that will be accompanied by padding of (DA) maybe it should
        // go inside of `lskget`?
        const int64_t n_rows;
        const int64_t n_cols;
        int64_t dim_short;
        int64_t dim_long;
        // int64_t n_sampled;

        const TrigDist dist;


        const RNGState<RNG> seed_state;
        RNGState<RNG> next_state;

        const blas::Layout layout;
        const bool sketchFromLeft = true;
        bool known_filled = false;

        sint_t* DiagonalRademacher = nullptr;
        sint_t* SampledRows = nullptr;

        TrigSkOp(
            TrigDist dist,
            RNGState<RNG> const &state,
            blas::Layout layout,
            bool known_filled = false
        ) : n_rows(dist.n_rows), n_cols(dist.n_cols), dist(dist), seed_state(state), known_filled(known_filled), dim_short(dist.dim_short), dim_long(dist.dim_long), layout(layout){
            // Memory for Rademacher diagonal gets allocated here
            // Number of rows/cols to be sampled gets decided here
            // i.e. `n_sampled` gets set
            if(sketchFromLeft)
                DiagonalRademacher = new sint_t[n_rows];
            else
                DiagonalRademacher = new sint_t[n_cols];

            SampledRows = new sint_t[dim_short];

            //TODO: Logic to compute the number of samples that we require `r`
            // this can be shown to depend on the maximal row norm of the data matrix
            //NOTE: Do not have access to this in the data-oblivious regime --- how
            // do we get access?
};

        TrigSkOp(
            TrigDistName family,
            int64_t n_rows,
            int64_t n_cols,
            uint32_t key
        ) : n_rows(n_rows), n_cols(n_cols), dist(TrigDist{n_rows, n_cols, family}), seed_state() {};

        //TODO: Write a proper deconstructor
        //Free up DiagonalRademacher && SampledRows
        ~TrigSkOp();
};

template <typename T, typename RNG, SignedInteger sint_t>
TrigSkOp<T, RNG, sint_t>::~TrigSkOp() {
    delete [] this->DiagonalRademacher;
    delete [] this->SampledRows;
};

template<typename T, typename RNG, SignedInteger sint_t>
RandBLAS::RNGState<RNG> fill_trig(
    TrigSkOp<T, RNG, sint_t> &Tr
) {
    /**
     * Will do the work of filling in the diagonal Rademacher entries
     * and selecting the rows/cols to be sampled
     */
    auto [ctr, key] = Tr.seed_state;

    // Fill in the Rademacher diagonal
    if(Tr.sketchFromLeft)
        generateRademacherVector_r123(Tr.DiagonalRademacher, key[0], ctr[0], Tr.n_rows);
    else
        generateRademacherVector_r123(Tr.DiagonalRademacher, key[0], ctr[0], Tr.n_cols);

    //NOTE: Select the rows/cols to be sampled --- use the `repeated_fisher_yates` function
    int64_t r = Tr.dim_short;
    int64_t d = Tr.dim_long;

    std::vector<sint_t> idxs_minor(r); // Placeholder
    std::vector<T> vals(r); // Placeholder

    Tr.next_state = RandBLAS::repeated_fisher_yates<T, RNG, sint_t>(
        Tr.seed_state,
        r,         // Number of samples (vec_nnz)
        d,         // Total number of elements (dim_major)
        1,         // Single sample round (dim_minor)
        Tr.SampledRows,  // Holds the required output
        idxs_minor.data(),  // Placeholder
        vals.data()         // Placeholder
    );
    Tr.known_filled = true;
    return Tr.next_state;
}
}


namespace RandBLAS::trig {

// Performs the actual application of the fast trigonometric transform
// Only called after calling `fill_trig`
template <typename T, typename RNG>
inline void lskget(
    blas::Layout layout,
    blas::Op opS,
    blas::Op opA,
    int64_t d, // B is d-by-n
    int64_t n, // op(A) is m-by-n
    int64_t m, // op(S) is d-by-m
    T alpha,
    TrigSkOp<T, RNG> &Tr,
    int64_t ro_s,
    int64_t co_s,
    const T* A, // data-matrix
    int64_t lda,
    T beta,
    T* B, // output matrix
    int64_t ldb
) {
    if (!Tr.known_filled)
        fill_trig(Tr);

    // Applying the diagonal transformation
    applyDiagonalRademacher(layout, opA, n, m, Tr.DiagonalRademacher, A, B);
    // applyDiagonalRademacher(m, n, A, Tr.DiagonalRademacher);

    //TODO: Apply the Hadamard transform

    //... and finally permute the rows
    // permuteRowsToTop(m, n, Tr.SampledRows, B, ldb);
}
}