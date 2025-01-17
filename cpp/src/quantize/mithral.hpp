//
//  mithral.hpp
//
//  Created by DB on 2019-10-28
//  Copyright (c) 2019 DB. All rights reserved.
//
#ifndef __MITHRAL_HPP
#define __MITHRAL_HPP

#include <assert.h>
#include <stdint.h>
#include <sys/types.h>
#include <cmath>
#include <type_traits>
#include <limits>
#include <iostream>
#include "immintrin.h"
#include <vector>
#include <algorithm>

// #define MITHRAL_USE_BOLT_SAFE_SCAN // way slower, but exact sum of uint8s

// #define BLAZE  //needed for Pybind11
#ifdef BLAZE
    #include "src/utils/avx_utils.hpp"
    #include "src/utils/eigen_utils.hpp"
    #ifdef MITHRAL_USE_BOLT_SAFE_SCAN
        #include "src/quantize/bolt.hpp"
    #endif
#else
    #include "avx_utils.hpp"
    #include "eigen_utils.hpp"
    #ifdef MITHRAL_USE_BOLT_SAFE_SCAN
        #include "bolt.hpp"
    #endif
#endif

// Testing pybind
int sub(int i, int j);
int add(int i, int j);

// ================================================================ in cpp
// these should be the only functions you have to call

// ------------------------ encoding

void mithral_encode(
    const float* X, int64_t nrows, int ncols,
    const uint32_t* splitdims, const int8_t* all_splitvals,
    const float* scales, const float* offsets, int ncodebooks, uint8_t* out);

// version with int16 data
void mithral_encode(const int16_t* X, int64_t nrows, int ncols,
    const uint32_t* splitdims, const int8_t* all_splitvals,
    const uint8_t* shifts, const int16_t* offsets,
    int ncodebooks, uint8_t* out);

// version with int8 data
void mithral_encode(const int8_t* X, int64_t nrows, int ncols,
    const uint32_t* splitdims, const int8_t* all_splitvals,
    int ncodebooks, uint8_t* out);

// wrapper for int8 version that can deal with scales and offsets provided
void mithral_encode(const int8_t* X, int64_t nrows, int ncols,
    const uint32_t* splitdims, const int8_t* all_splitvals,
    const void* shifts_unused, const void* offsets_unused,
    int ncodebooks, uint8_t* out);

void zip_bolt_colmajor(const uint8_t* codes_in, int64_t nrows,
                       uint32_t ncodebooks, uint8_t* codes_out);

// ------------------------ lut creation

void mithral_lut_dense(const float* Q, int nrows, int ncols, int ncodebooks,
    const float* centroids, float& out_offset_sum, float& out_scale,
    float*__restrict__ tmp_lut_f32, uint8_t* out);


void mithral_lut_sparse(const float* Q, int nrows, int ncols, int ncodebooks,
    const float* centroids, const int* idxs, int nnz_per_centroid,
    float& out_offset_sum, float& out_scale,
    float*__restrict__ tmp_lut_f32, uint8_t* out);

// ------------------------ scan

//have to make the copy since mithral_scan_in_chunks in anon namespace
void mithral_scan(const uint8_t* codes, int64_t nblocks, int ncodebooks,
                  int noutputs, const uint8_t* luts, uint8_t* dists_out);

void mithral_scan(const uint8_t* codes, int64_t nblocks, int ncodebooks,
                  int noutputs, const uint8_t* luts, uint16_t* dists_out);

//Only for testing codes and luts correct
void mithral_scan_test(const uint8_t* codes, int n, int ncodebooks, int m,
                       float offset, float scale,
                       const uint8_t* luts, uint16_t* float_dists_out);
//closer to how mithral called
void mithral_scan_test(const uint8_t* codes, int n, int ncodebooks, int m,
                       float offset, float scale,
                       const uint8_t* luts, uint8_t* float_dists_out);
//// How Mithral called
//void mithral_scan_test_zipped(const uint8_t* codes, int n, int ncodebooks, int m,
//                       float offset, float scale,
//                       const uint8_t* luts, uint16_t* float_dists_out);
/* if zipped codes first 
*/
template<typename OutType=uint16_t>
void mithral_scan_test_zipped(const uint8_t* codes, int n, int ncodebooks, int m,
                       float offset, float scale,
                       const uint8_t* luts, OutType* float_dists_out, int use_nrows=-1) 
{ 
  use_nrows = use_nrows > 0 ? use_nrows : n;
  for (int i = 0; i < m; i++) {
    const uint8_t* lut = luts + i * ncodebooks * 16;
    for (int j = 0; j < use_nrows; j++) {
      uint32_t dist = 0; //narrowed automatically
      //uint8_t dist = 0; //mimics output type being uint8
      for (int code_ix = 0; code_ix < ncodebooks/2; code_ix ++) {
        auto code_byte = codes[j + code_ix*n];
        auto code0 = code_byte & 0x0F;
        auto code1 = (code_byte >> 4) & 0x0F; 
        auto d0 = lut[code0 + 32*code_ix];
        auto d1 = lut[code1 + 32*code_ix + 16];
        dist += d0;
        //saturating addition
        if (dist < d0) {
            dist=-1;
        }
        dist += d1;
        if (dist < d1) {
            dist=-1; 
        }
      }
      float_dists_out[i*n + j] = ((dist/scale) + offset);
    }
  }
}

// ------------------------ wrapper

template<class InputT> struct mithral_input_type_traits {};
template<> struct mithral_input_type_traits<float> {
    using encoding_scales_type = float;
    using encoding_offsets_type = float;
    //using output_type = uint16_t;  //Change output type here
    using output_type = uint8_t; //R^2 is worse but ordering is about the same
};
template<> struct mithral_input_type_traits<int16_t> {
    using encoding_scales_type = uint8_t;
    using encoding_offsets_type = int16_t;
    using output_type = uint16_t;
};
template<> struct mithral_input_type_traits<int8_t> {
    using encoding_scales_type = uint8_t;    // doesn't matter; unused
    using encoding_offsets_type = uint8_t;  // doesn't matter; unused
    using output_type = uint16_t;
};

template<class InputT>
struct mithral_amm {
    using traits = mithral_input_type_traits<InputT>;
    using scale_t = typename traits::encoding_scales_type;
    using offset_t = typename traits::encoding_offsets_type;
    using output_t = typename traits::output_type;
    static constexpr int scan_block_nrows = 32;
    static constexpr int lut_sz = 16;

    // NxD matrix @ DxM matrix
    mithral_amm(int N, int D, int M, int ncodebooks, const float* centroids,
                // for encoding
                const uint32_t* splitdims, const int8_t* splitvals,
                const scale_t* encode_scales, const offset_t* encode_offsets,
                // for lut creation
                const int* idxs, int nnz_per_centroid):
                
        nsplits_per_codebook(4),
        total_nsplits(ncodebooks * nsplits_per_codebook),

        N(N), D(D), M(M), ncodebooks(ncodebooks), centroids(centroids),
        splitdims(splitdims), splitvals(splitvals),
        encode_scales(encode_scales), encode_offsets(encode_offsets),
        idxs(idxs), nnz_per_centroid(nnz_per_centroid),
        tmp_codes(N, ncodebooks), codes(N, ncodebooks),
        tmp_luts_f32(M, ncodebooks * lut_sz), luts(M, ncodebooks * lut_sz),
        out_mat(N, M)
    {
        luts.setRandom();  // so profiling without LUT creation isn't undefined
    }
    
    void resize(int new_N, int new_M) {
        N = new_N;
        M = new_M;
        tmp_codes.resize(N, ncodebooks);
        codes.resize(N, ncodebooks);
        tmp_luts_f32.resize(M, ncodebooks * lut_sz);
        luts.resize(M, ncodebooks * lut_sz); 
        out_mat.resize(N,M);
    }

    void encode(const InputT* X) {
        // TODO add strides to these funcs so that we can pad number
        // of rows, so scan can rely on nrows being a multiple of 32
        // std::cout << "splitdims: " << splitdims[0] << "\nsplitval: " << splitvals[0] << "\nencode_scales: " << encode_scales[0]  << "\nencodeoffsets: " << encode_offsets[0] << "\n" << std::endl;
        mithral_encode(
            X, N, D, splitdims, splitvals, encode_scales,
            encode_offsets, ncodebooks, tmp_codes.data());
        zip_bolt_colmajor(tmp_codes.data(), N, ncodebooks, codes.data());
    }

    void mithral_encode_only(const InputT* X) {
        mithral_encode(
            X, N, D, splitdims, splitvals, encode_scales,
            encode_offsets, ncodebooks, tmp_codes.data());
        codes = tmp_codes; 
    }    
    
    void zip_bolt_colmajor_only () {
        //only for if copied codes into c++ from python
        zip_bolt_colmajor(tmp_codes.data(), N, ncodebooks, codes.data());
    }

    void lut(const float* Q) {
        if (nnz_per_centroid > 0) { //always positive in mithral_amm_task constructor; profile_amm_old.hpp would change to negative for some tests
            mithral_lut_sparse(Q, M, D, ncodebooks, centroids,
                idxs, nnz_per_centroid, out_offset_sum, out_scale,
                tmp_luts_f32.data(), luts.data());
        } else {
            mithral_lut_dense(Q, M, D, ncodebooks, centroids,
                out_offset_sum, out_scale, tmp_luts_f32.data(), luts.data());
        }
    }

    void scan() {
        auto nblocks = N / scan_block_nrows;
        #ifdef MITHRAL_USE_BOLT_SAFE_SCAN
            bolt_scan(codes.data(), nblocks, ncodebooks, M,
                      luts.data(), out_mat.data());
        #else
            mithral_scan(codes.data(), nblocks, ncodebooks, M,
                         luts.data(), out_mat.data()); 
        #endif
    }
 
    void scan_test() {
        mithral_scan_test((const uint8_t*)codes.data(), N, ncodebooks,  M,
                    out_offset_sum, out_scale,
                    (const uint8_t*)luts.data(), (uint16_t*)out_mat.data());
        
    }
    void scan_test_zipped () {
        mithral_scan_test_zipped<output_t>((const uint8_t*)codes.data(), N, ncodebooks,  M,
                    out_offset_sum, out_scale,
                    (const uint8_t*)luts.data(), out_mat.data());
    }
    
    //size params
    const int nsplits_per_codebook;
    const int total_nsplits;

    // ctor params
    int N;
    int D;
    int M;
    int ncodebooks;
    const float* centroids;
    const uint32_t* splitdims;
    const int8_t* splitvals;
    const scale_t* encode_scales;
    const offset_t* encode_offsets;
    const int* idxs; 
    int nnz_per_centroid;

    // storage for intermediate values
    ColMatrix<uint8_t> tmp_codes;
    ColMatrix<uint8_t> codes;
    RowMatrix<float> tmp_luts_f32;
    RowMatrix<uint8_t> luts;

    // outputs
    float out_offset_sum;
    float out_scale;
    ColMatrix<output_t> out_mat;
};

// ------------------------ just for profiling

void dense_lut_f32_fused(const float* Q, int nrows, int ncols, int ncodebooks,
    const float* centroids, float*__restrict__ out_offsets, float& out_offset_sum,
    float& out_scale, float*__restrict__ out);

void sparse_lut_f32(const float* Q, int nrows, int ncols, int ncodebooks,
                    const float* centroids,
                    const int* idxs, int nnz_per_centroid, float* out);

// ================================================================ here
// TODO ideally these should all be in the cpp file; for now they're still
// here because this makes it easy to profile different template params

namespace {

// https://godbolt.org/z/BMx6D7 (also includes zip2_4b_colmajor to compare)
// inline void zip_bolt_colmajor_v2(const uint8_t* codes_in, int64_t nrows,
//
// # tested matches Python ncodebooks=32 random X/Q
// c=np.copy(task.amm.tmp_codes)
// out=np.copy(c)
// task.amm.zip_bolt_colmajor_only()
// for i in range(ncodebooks//2):
//   x=np.bitwise_or(c[:,2*i], 16*c[:,2*i+1])%256
//   out[:,i]=x
// print(list(map(np.unique, np.where((out!=task.amm.codes)[:, :ncodebooks//2]))))
// assert(np.all((out==task.amm.codes)[:, :ncodebooks//2])) @#only front 1/2 changed
template<int NReadColsAtOnce=2>
inline void zip_bolt_colmajor(const uint8_t* codes_in, int64_t nrows,
                              uint32_t ncodebooks, uint8_t* codes_out)
{
    static constexpr int in_block_sz = 32;
    static constexpr int simd_vec_sz = 32;
    static constexpr int ncols_in_per_group = NReadColsAtOnce;
    // static constexpr int ncols_in_per_group = 16;
    // static constexpr int ncols_in_per_group = 4;
    // static constexpr int ncols_in_per_group = 2;
    static constexpr int ncols_in_per_col_out = 2;
    static constexpr int ncols_out_per_group =
        ncols_in_per_group / ncols_in_per_col_out;
    // static constexpr int ncols_in_per_group = 2;
    // static constexpr int ncols_out_per_group = 1;
    // static constexpr int chunk_sz = 4096;  // one page
    static constexpr int chunk_sz = 2048;  // half a page
    // static constexpr int chunk_sz = 1024;  // quarter of a page
    // static constexpr int chunk_sz = 512;  // quarter of a page
    // static constexpr int chunk_sz = 256;
    // static constexpr int chunk_sz = 128;
    // static constexpr int chunk_sz = 64;
    assert(ncodebooks % ncols_in_per_group == 0);
    assert(nrows % in_block_sz == 0);
    int ncolgroups = ncodebooks / ncols_in_per_group;

    // int chunk_sz = MAX(256, 4096 / (ncodebooks / 2));
    // int chunk_sz = 4096 / ncodebooks;
    auto nchunks = (nrows + chunk_sz - 1) / chunk_sz;

    auto in_col_stride = nrows;
    // auto out_stride = simd_vec_sz;

    // uint8_t* in_col_ptrs[ncols_in_per_group];
    // const uint8_t* in_col_ptrs[ncols_in_per_col_out];
    const uint8_t* in_col_ptrs[ncols_out_per_group];
    uint8_t* out_ptrs[ncols_out_per_group];

    for (int chunk = 0; chunk < nchunks; chunk++) {
        auto nrows_done_so_far = chunk * chunk_sz;
        int64_t nblocks = chunk_sz / in_block_sz;
        if (chunk == nchunks - 1) {
            auto N = MIN(chunk_sz, nrows - nrows_done_so_far);
            nblocks = N / in_block_sz;
        }

        for (int g = 0; g < ncolgroups; g++) {
            // initialize col starts
            for (int gg = 0; gg < ncols_out_per_group; gg++) {
                auto initial_col_in = (g * ncols_in_per_group) + (2 * gg);
                auto col_out = initial_col_in / 2;
                in_col_ptrs[gg] = codes_in + (initial_col_in * in_col_stride);
                out_ptrs[gg] = codes_out + (col_out * in_col_stride); 
            }
            // for each block
            // #pragma unroll
            for (int b = 0; b < nblocks; b++) {
                #pragma unroll
                for (int gg = 0; gg < ncols_out_per_group; gg++) {
                    auto in_ptr = in_col_ptrs[gg];
                    auto x0 = load_si256i(in_ptr + 0 * in_col_stride); 
                    auto x1 = load_si256i(in_ptr + 1 * in_col_stride);//+in_col_stride to get row adjacent val 
                    in_col_ptrs[gg] += simd_vec_sz;

                    // pack bits and store result
                    auto x01 = _mm256_or_si256(x0, _mm256_slli_epi16(x1, 4));
                    _mm256_store_si256((__m256i*)(out_ptrs[gg]), x01);
                    out_ptrs[gg] += simd_vec_sz;
                }
            }
        }
        codes_in += chunk_sz;
        codes_out += chunk_sz; 
    }
}


// template<int ncodebooks>
void _compute_offsets_scale_from_mins_maxs(
    const __m256* mins, const __m256* maxs, int ncodebooks,
    float* out_offsets, float& out_offset_sum, float& out_scale)
{
    // NOTE: out_offset_sum was -145 v I calculated -140 based on dbg output (but of ints not floats) and python

    // we now have the mins and maxes for each codebook; compute offsets
    // for each codebook, then global offset, then largest value - offset
    // auto vmins = mins[0];
    // float offset = 0;
    out_offset_sum = 0;
    __m256 vmax = _mm256_set1_ps(std::numeric_limits<float>::min());
    for (int c = 0; c < ncodebooks; c++) {
        auto vmin = broadcast_min(mins[c]);
        auto offset = pfirst(vmin);  // minimum value
        out_offsets[c] = offset;
        out_offset_sum += offset;

        // update vector of max difference seen so far
        vmax = _mm256_max_ps(vmax, _mm256_sub_ps(maxs[c], vmin));
    }
    vmax = broadcast_max(vmax);
    out_scale = pfirst(vmax);
    if (out_scale <= 0.f) {
        out_scale = 0.;
        return; // data is constant; just return
    }

    // round scale up to nearest power of 2
    float exponent = std::ceil(std::log2f(out_scale));
    out_scale = std::exp2(-exponent);  // reciprocal so we can use fma
    out_scale *= (255.f - 1e-10f);  // so max val is at most just under 255

    // update offsets based on scale, so that one can incorporate offsets
    // in an fma (specifically, fmsub to create lut and fma to invert)
    for (int c = 0; c < ncodebooks; c++) {
        out_offsets[c] *= out_scale;
    }
}

// template<int CodebookTileSz=2, int RowTileSz=2>
// NOTE: ColTileSz has no effect on performance; already unrolled plenty
// template<int CodebookTileSz=2, int RowTileSz=2, int ColTileSz=1>
template<int CodebookTileSz=2, int RowTileSz=2>
void dense_lut_f32(const float* Q, int nrows, int ncols, int ncodebooks,
                 const float* centroids, float* out)
{
    static constexpr int ColTileSz = 1;
    static constexpr int ncentroids = 16;
    static constexpr int lut_sz = ncentroids;
    static constexpr int packet_width = 8; // objs per simd register
    static constexpr int nstripes = lut_sz / packet_width;
    assert(ncodebooks % CodebookTileSz == 0);
    assert(nrows % RowTileSz == 0);

    // __m256 accumulators[CodebookTileSz * RowTileSz * nstripes];
    __m256 accumulators[CodebookTileSz][RowTileSz][nstripes];
    __m256 vbroadcasted[RowTileSz];

    const float* queries_ptrs[RowTileSz];
    const float* centroids_ptrs[CodebookTileSz];
    float* out_ptrs[RowTileSz][CodebookTileSz];

    auto q_row_stride = ncols;
    auto centroids_codebook_stride = ncentroids * ncols;
    auto out_row_stride = ncodebooks * lut_sz;
    auto out_codebook_stride = lut_sz;

    auto ncodebook_blocks = ncodebooks / CodebookTileSz;
    auto nrow_blocks = nrows / RowTileSz;
    auto ncol_blocks_full = ncols / ColTileSz;

    for (int r = 0; r < nrow_blocks; r++) {
        for (int c = 0; c < ncodebook_blocks; c++) {
            for (int cc = 0; cc < CodebookTileSz; cc++) {
                // set centroid start ptrs for this codebook
                auto codebook = (c * CodebookTileSz) + cc;
                centroids_ptrs[cc] =
                    centroids + (centroids_codebook_stride * codebook);
                for (int rr = 0; rr < RowTileSz; rr++) {
                    // set output ptrs for this codebook
                    auto row = (r * RowTileSz) + rr;
                    out_ptrs[rr][cc] = out + (out_row_stride * row) +
                        (out_codebook_stride * codebook);

                    // zero accumulators
                    for (int s = 0; s < nstripes; s++) {
                        accumulators[cc][rr][s] = _mm256_setzero_ps();
                        // auto idx = cc * (RowTileSz + nstripes) + (rr * RowTileSz) + s;
                        // accumulators[idx] = _mm256_setzero_ps();
                    }
                }
            }
            for (int rr = 0; rr < RowTileSz; rr++) {
                auto row = (r * RowTileSz) + rr;
                queries_ptrs[rr] = Q + (q_row_stride * row);
            }

            // compute sums for each output row for this block of codebooks
            // for (int j = 0; j < ncols; j++) {
            for (int j = 0; j < ncol_blocks_full; j++) {
                for (int jj = 0; jj < ColTileSz; jj++) {
                    for (int rr = 0; rr < RowTileSz; rr++) {
                        auto qval = *queries_ptrs[rr];
                        vbroadcasted[rr] = _mm256_set1_ps(qval);
                        queries_ptrs[rr]++;
                    }

                    for (int cc = 0; cc < CodebookTileSz; cc++) {
                        for (int s = 0; s < nstripes; s++) {
                            auto centroids_col = _mm256_load_ps(centroids_ptrs[cc]);
                            centroids_ptrs[cc] += packet_width;
                            for (int rr = 0; rr < RowTileSz; rr++) {
                                accumulators[cc][rr][s] = fma(vbroadcasted[rr],
                                    centroids_col, accumulators[cc][rr][s]);
                                // auto idx = cc * (RowTileSz + nstripes) + (rr * RowTileSz) + s;
                                // accumulators[idx] = fma(vbroadcasted[rr],
                                //     centroids_col, accumulators[idx]);
                            }
                        }
                    }
                }
            }
            // handle trailing cols
            for (int jj = ncol_blocks_full * ColTileSz; jj < ncols; jj++) {
                for (int rr = 0; rr < RowTileSz; rr++) {
                    auto qval = *queries_ptrs[rr];
                    vbroadcasted[rr] = _mm256_set1_ps(qval);
                    queries_ptrs[rr]++;
                }
                for (int cc = 0; cc < CodebookTileSz; cc++) {
                    for (int s = 0; s < nstripes; s++) {
                        auto centroids_col = _mm256_load_ps(centroids_ptrs[cc]);
                        centroids_ptrs[cc] += packet_width;
                        for (int rr = 0; rr < RowTileSz; rr++) {
                            accumulators[cc][rr][s] = fma(vbroadcasted[rr],
                                centroids_col, accumulators[cc][rr][s]);
                        }
                    }
                }
            }
            // write out sums
            for (int rr = 0; rr < RowTileSz; rr++) {
                for (int cc = 0; cc < CodebookTileSz; cc++) {
                    for (int s = 0; s < nstripes; s++) {
                        _mm256_store_ps(out_ptrs[rr][cc], accumulators[cc][rr][s]);
                        // auto idx = cc * (RowTileSz + nstripes) + (rr * RowTileSz) + s;
                        // _mm256_store_ps(out_ptrs[rr][cc], accumulators[idx]);
                        out_ptrs[rr][cc] += packet_width;
                    }
                }
            }
        }
    }
}
// // force it to instantiate this template
// template void dense_lut_f32<2, 3>(const float* Q, int nrows, int ncols,
//     int ncodebooks, const float* centroids, float* out);


// this is basically just a dense matmul that also tracks the min/max
// product; Q = nrows, ncols; centroids = ncols, ncodebooks; but centroids.T
// is already in a block-colmajor layout, with block size of 16; also Q
// is rowmajor
template<int CodebookTileSz=2, int RowTileSz=2>
void _dense_lut_f32_fused(const float* Q, int nrows, int ncols, int ncodebooks,
    // const float* centroids, float* out)
    // SELF: above args are fast, while ones below make it like 2x slower
    __m256*__restrict__ mins, __m256*__restrict__ maxs,
    const float* centroids, float*__restrict__ out_offsets,
    float& out_offset_sum, float& out_scale, float*__restrict__ out)
    //Why aren't out_offset_sum and out_scale used?
{
    static constexpr int ncentroids = 16;
    static constexpr int lut_sz = ncentroids;
    static constexpr int packet_width = 8; // objs per simd register
    static constexpr int nstripes = lut_sz / packet_width;
    assert(ncodebooks % CodebookTileSz == 0);
    assert(nrows % RowTileSz == 0);

    __m256 accumulators[CodebookTileSz][RowTileSz][nstripes];
    __m256 vbroadcasted[RowTileSz];

    const float* queries_ptrs[RowTileSz];
    const float* centroids_ptrs[CodebookTileSz];
    float* out_ptrs[RowTileSz][CodebookTileSz];

    // __m256 mins[ncodebooks];
    // __m256 maxs[ncodebooks];
    // for (int c = 0; c < ncodebooks; c++) {
    //     mins[c] = _mm256_set1_ps(std::numeric_limits<float>::max());
    //     maxs[c] = _mm256_set1_ps(std::numeric_limits<float>::min());
    // }

    auto q_row_stride = ncols;
    auto centroids_codebook_stride = ncentroids * ncols;
    auto out_row_stride = ncodebooks * lut_sz;
    auto out_codebook_stride = lut_sz;

    auto ncodebook_blocks = ncodebooks / CodebookTileSz;
    auto nrow_blocks = nrows / RowTileSz;

    static constexpr int ColTileSz = 1;
    auto ncol_blocks_full = ncols / ColTileSz;

    for (int r = 0; r < nrow_blocks; r++) {
        for (int c = 0; c < ncodebook_blocks; c++) {
            for (int cc = 0; cc < CodebookTileSz; cc++) {
                // set centroid start ptrs for this codebook
                auto codebook = (c * CodebookTileSz) + cc;
                centroids_ptrs[cc] =
                    centroids + (centroids_codebook_stride * codebook);
                for (int rr = 0; rr < RowTileSz; rr++) {
                    // set output ptrs for this codebook
                    auto row = (r * RowTileSz) + rr;
                    out_ptrs[rr][cc] = out + (out_row_stride * row) +
                        (out_codebook_stride * codebook);
                    // zero accumulators
                    for (int s = 0; s < nstripes; s++) {
                        accumulators[cc][rr][s] = _mm256_setzero_ps();
                    }
                }
            }
            for (int rr = 0; rr < RowTileSz; rr++) {
                auto row = (r * RowTileSz) + rr;
                queries_ptrs[rr] = Q + (q_row_stride * row);
            }

            // compute sums for each output row for this block of codebooks
            // for (int j = 0; j < ncols; j++) {
            for (int j = 0; j < ncol_blocks_full; j++) {
                for (int jj = 0; jj < ColTileSz; jj++) {
                    for (int rr = 0; rr < RowTileSz; rr++) {
                        auto qval = *queries_ptrs[rr];
                        vbroadcasted[rr] = _mm256_set1_ps(qval);
                        queries_ptrs[rr]++;
                    }

                    for (int cc = 0; cc < CodebookTileSz; cc++) {
                        for (int s = 0; s < nstripes; s++) {
                            auto centroids_col = _mm256_load_ps(centroids_ptrs[cc]);
                            centroids_ptrs[cc] += packet_width;
                            for (int rr = 0; rr < RowTileSz; rr++) {
                                accumulators[cc][rr][s] = fma(vbroadcasted[rr],
                                    centroids_col, accumulators[cc][rr][s]);
                            }
                        }
                    }
                }
            }
            // write out sums
            for (int rr = 0; rr < RowTileSz; rr++) {
                for (int cc = 0; cc < CodebookTileSz; cc++) {
                    auto codebook = (c * CodebookTileSz) + cc;
                    for (int s = 0; s < nstripes; s++) {
                        auto half_lut = accumulators[cc][rr][s];
                        mins[codebook] = _mm256_min_ps(
                            mins[codebook], half_lut);
                        maxs[codebook] = _mm256_max_ps(
                            maxs[codebook], half_lut);

                        _mm256_store_ps(out_ptrs[rr][cc], half_lut);
                        out_ptrs[rr][cc] += packet_width;
                    }
                }
            }
        }
    }
}

template<int CodebookTileSz=2, int RowTileSz=2>
void dense_lut_f32_fused(const float* Q, int nrows, int ncols, int ncodebooks,
    // const float* centroids, float* out)
    // SELF: above args are fast, while ones below make it like 2x slower
    const float* centroids, float*__restrict__ out_offsets,
    float& out_offset_sum, float& out_scale, float*__restrict__ out)
{
    static constexpr int ncentroids = 16;
    static constexpr int lut_sz = ncentroids;
    static_assert(RowTileSz >= 1, "RowTileSz must be >= 1");
    static_assert(RowTileSz <= 4, "RowTileSz must be <= 4 for now");

    // initilize mins and maxes; note that it's okay for the two calls to
    // see different mins and maxes since these arrays aren't used except
    // to compute the offsets and scale at the very end
    __m256 mins[ncodebooks];
    __m256 maxs[ncodebooks];
    for (int c = 0; c < ncodebooks; c++) {
        mins[c] = _mm256_set1_ps(std::numeric_limits<float>::max());
        maxs[c] = _mm256_set1_ps(std::numeric_limits<float>::min());
    }
    // handle most rows
    auto nrows_trailing = nrows % RowTileSz;
    auto nrows_round = nrows - nrows_trailing;
    if (nrows_round > 0) {
        _dense_lut_f32_fused<CodebookTileSz, RowTileSz>(
            Q, nrows_round, ncols, ncodebooks, mins, maxs,
            centroids, out_offsets, out_offset_sum, out_scale, out);
    }
    // handle trailing rows
    auto q_row_stride = ncols;
    Q += q_row_stride * nrows_round;
    auto out_row_stride = ncodebooks * lut_sz;
    out += out_row_stride * nrows_round;

    // NOTE: if we hardcode this to 1 instead of having a switch, or just
    // rm handling of the trailing rows entirely, code is twice as fast
    _dense_lut_f32_fused<CodebookTileSz, 1>(
            Q, nrows_trailing, ncols, ncodebooks, mins, maxs,
            centroids, out_offsets, out_offset_sum, out_scale, out);

    // switch(nrows_trailing) {
    //     case 0: break;
    //     case 1: _dense_lut_f32_fused<CodebookTileSz, 1>(
    //         Q, nrows_trailing, ncols, ncodebooks, mins, maxs,
    //         centroids, out_offsets, out_offset_sum, out_scale, out); break;
    //     case 2: _dense_lut_f32_fused<CodebookTileSz, 2>(
    //         Q, nrows_trailing, ncols, ncodebooks, mins, maxs,
    //         centroids, out_offsets, out_offset_sum, out_scale, out); break;
    //     case 3: _dense_lut_f32_fused<CodebookTileSz, 3>(
    //         Q, nrows_trailing, ncols, ncodebooks, mins, maxs,
    //         centroids, out_offsets, out_offset_sum, out_scale, out); break;
    // }
    // write out stats using mins and maxs
    _compute_offsets_scale_from_mins_maxs(
        mins, maxs, ncodebooks, out_offsets, out_offset_sum, out_scale);
}

// Here nrows/ncols mean the number of elements in each row/col; flipped from 'total number of' rows/cols in constructor
// Why nrows/ncols are swapped when passed into this fn
template<int CodebookTileSz=2, int RowTileSz=2>
void sparse_lut_f32(const float* Q, int nrows, int ncols, int ncodebooks,
                     const float* centroids,
                     const int* idxs, int nnz_per_centroid,
                     float* out)
{
    static constexpr int ncentroids = 16;
    static constexpr int lut_sz = ncentroids;
    static constexpr int packet_width = 8; // objs per simd register
    static constexpr int nstripes = lut_sz / packet_width;
    assert(ncodebooks % CodebookTileSz == 0);
    assert(nrows % RowTileSz == 0);

    __m256 accumulators[CodebookTileSz][RowTileSz][nstripes];
    __m256 vbroadcasted[RowTileSz];

    const float* query_start_ptrs[RowTileSz];
    const int* idx_ptrs[CodebookTileSz];
    const float* centroids_ptrs[CodebookTileSz];
    float* out_ptrs[RowTileSz][CodebookTileSz];

    auto q_row_stride = ncols;
    auto idxs_codebook_stride = nnz_per_centroid;
    auto centroids_codebook_stride = ncentroids * ncols; //num elem in each col of Q since centroids not orthogonal, take full X row
    auto out_row_stride = ncodebooks * lut_sz;
    auto out_codebook_stride = lut_sz;

    auto ncodebook_blocks = ncodebooks / CodebookTileSz;
    auto nrow_blocks = nrows / RowTileSz;

    for (int r = 0; r < nrow_blocks; r++) {
        // prefetch contents of rows, so we don't end up with random access
        // EDIT: doesn't help; is actually slightly slower
        // static constexpr int cache_line_sz_bytes = 64;  // on almost everything
        // static constexpr int stride = cache_line_sz_bytes / sizeof(Q[0]);
        // for (int rr = 0; rr < RowTileSz; rr++) {
        //         auto row = (r * RowTileSz) + rr;
        //         query_start_ptrs[rr] = Q + (q_row_stride * row);
        //     }
        // for (int j = 0; j < ncols; j += stride) {
        //     for (int rr = 0; rr < RowTileSz; rr++) {
        //         __builtin_prefetch(query_start_ptrs[rr] + j);
        //     }
        // }
        // compute all luts for this block of rows
        for (int c = 0; c < ncodebook_blocks; c++) {
            for (int cc = 0; cc < CodebookTileSz; cc++) {
                auto codebook = (c * CodebookTileSz) + cc;
                // set centroid start ptrs for this codebook
                centroids_ptrs[cc] =
                    centroids + (centroids_codebook_stride * codebook);
                // set idxs start ptrs for this codebook
                idx_ptrs[cc] = idxs + (idxs_codebook_stride * codebook);

                for (int rr = 0; rr < RowTileSz; rr++) {
                    // set output ptrs for this codebook
                    auto row = (r * RowTileSz) + rr;
                    out_ptrs[rr][cc] = out + (out_row_stride * row) +
                        (out_codebook_stride * codebook);
                    // zero accumulators
                    for (int s = 0; s < nstripes; s++) {
                        accumulators[cc][rr][s] = _mm256_setzero_ps();
                    }
                }
            }
            for (int rr = 0; rr < RowTileSz; rr++) {
                auto row = (r * RowTileSz) + rr;
                query_start_ptrs[rr] = Q + (q_row_stride * row);
            }

            // compute sums for each output row for this block of codebooks
            for (int j = 0; j < nnz_per_centroid; j++) {
                for (int cc = 0; cc < CodebookTileSz; cc++) {
                    auto idx = *idx_ptrs[cc];
                    idx_ptrs[cc]++;
                    for (int rr = 0; rr < RowTileSz; rr++) {
                        auto row_start_ptr = query_start_ptrs[rr];
                        auto qval = row_start_ptr[idx];
                        vbroadcasted[rr] = _mm256_set1_ps(qval);
                    }
                    for (int s = 0; s < nstripes; s++) {
                        auto centroids_col = _mm256_load_ps(centroids_ptrs[cc]); 
                        const uintptr_t addr = reinterpret_cast<const uintmax_t>(centroids_ptrs[cc]);
                        centroids_ptrs[cc] += packet_width;
                        for (int rr = 0; rr < RowTileSz; rr++) {
                            accumulators[cc][rr][s] = fma(vbroadcasted[rr],
                                centroids_col, accumulators[cc][rr][s]);
                        }
                    }
                }
            }
            // write out sums
            for (int rr = 0; rr < RowTileSz; rr++) {
                for (int cc = 0; cc < CodebookTileSz; cc++) {
                    for (int s = 0; s < nstripes; s++) {
                        _mm256_store_ps(out_ptrs[rr][cc], accumulators[cc][rr][s]);
                        out_ptrs[rr][cc] += packet_width;
                    }
                }
            }
        }
    }
}

// this is just so that we can profile this separately
// template<int ncodebooks, int RowTileSz=1>
template<int RowTileSz=1>
void mithral_learn_lut_offsets_scales(
    const float* luts_f32, int nrows, int ncodebooks,
    float* out_offsets, float& out_offset_sum, float& out_scale)
{
    static constexpr int lut_sz = 16;
    // static constexpr int codebook_group_sz = 2; // 4 f32 luts -> 1 epu8 lut
    static constexpr int packet_width = 8; // objs per simd register
    // static constexpr int nstripes = lut_sz / packet_width;
    // static constexpr int ncodebook_groups = ncodebooks / codebook_group_sz;
    // static_assert(ncodebooks % codebook_group_sz == 0,
    //     "Number of codebooks must be a multiple of 2");
    assert(nrows % RowTileSz == 0);

    auto row_stride = ncodebooks * lut_sz;
    auto nrow_blocks = RowTileSz > 1 ? nrows / RowTileSz : 0;
    auto nrows_round = nrow_blocks * RowTileSz;

    const float* in_ptrs[RowTileSz];
    const float* offset_ptrs[RowTileSz];
    uint8_t* out_ptrs[RowTileSz];

    __m256 mins[ncodebooks];
    __m256 maxs[ncodebooks];
    for (int c = 0; c < ncodebooks; c++) {
        mins[c] = _mm256_set1_ps(std::numeric_limits<float>::max());
        maxs[c] = _mm256_set1_ps(std::numeric_limits<float>::min());
    }

    /* .LBB1_3:  // using 8 codebooks; this loop is like 40% of the total time
        mov     esi, ecx
        and     esi, -128
        vmovaps ymm5, ymmword ptr [rbx + 4*rsi + 32]
        vmovaps ymm6, ymmword ptr [rbx + 4*rsi + 96]
        vmovaps ymm7, ymmword ptr [rbx + 4*rsi + 160]
        vmovaps ymm9, ymmword ptr [rbx + 4*rsi + 224]
        vminps  ymm0, ymm15, ymmword ptr [rbx + 4*rsi]
        vminps  ymm15, ymm0, ymm5
        vminps  ymm0, ymm8, ymmword ptr [rbx + 4*rsi + 64]
        vminps  ymm8, ymm0, ymm6
        vminps  ymm0, ymm14, ymmword ptr [rbx + 4*rsi + 128]
        vminps  ymm10, ymm10, ymmword ptr [rbx + 4*rsi + 192]
        vminps  ymm14, ymm0, ymm7
        vminps  ymm10, ymm10, ymm9
        vminps  ymm0, ymm4, ymmword ptr [rbx + 4*rsi + 256]
        vmovaps ymm11, ymmword ptr [rbx + 4*rsi + 288]
        vminps  ymm4, ymm0, ymm11
        vminps  ymm0, ymm3, ymmword ptr [rbx + 4*rsi + 320]
        vmovaps ymm12, ymmword ptr [rbx + 4*rsi + 352]
        vminps  ymm3, ymm0, ymm12
        vminps  ymm0, ymm2, ymmword ptr [rbx + 4*rsi + 384]
        vmovaps ymm13, ymmword ptr [rbx + 4*rsi + 416]
        vminps  ymm2, ymm0, ymm13
        vminps  ymm1, ymm1, ymmword ptr [rbx + 4*rsi + 448]
        vmovaps ymm0, ymmword ptr [rbx + 4*rsi + 480]
        vminps  ymm1, ymm1, ymm0
        sub     rcx, -128
        dec     rax
        jne     .LBB1_3
     */
    // compute min and max vals for each codebook
    for (int r = 0; r < nrow_blocks; r++) {
        // new set of rows; reset read ptrs
        for (int rr = 0; rr < RowTileSz; rr++) {
            auto row = (r * RowTileSz) + rr;
            in_ptrs[rr] = luts_f32 + (row * row_stride);
        }
        // update all the mins and maxes
        for (int c = 0; c < ncodebooks; c++) {
            for (int rr = 0; rr < RowTileSz; rr++) {
                auto vlut_stripe0 = _mm256_load_ps(in_ptrs[rr]);
                in_ptrs[rr] += packet_width;
                auto vlut_stripe1 = _mm256_load_ps(in_ptrs[rr]);
                in_ptrs[rr] += packet_width;

                mins[c] = _mm256_min_ps(mins[c], vlut_stripe0);
                mins[c] = _mm256_min_ps(mins[c], vlut_stripe1);
                maxs[c] = _mm256_max_ps(maxs[c], vlut_stripe0);
                maxs[c] = _mm256_max_ps(maxs[c], vlut_stripe1);
            }
        }
    }
    for (int row = nrows_round; row < nrows; row++) { // handle trailing rows
        auto in_ptr = luts_f32 + (row * row_stride);
        for (int c = 0; c < ncodebooks; c++) {
            auto vlut_stripe0 = _mm256_load_ps(in_ptr);
            in_ptr += packet_width;
            auto vlut_stripe1 = _mm256_load_ps(in_ptr);
            in_ptr += packet_width;

            mins[c] = _mm256_min_ps(mins[c], vlut_stripe0);
            mins[c] = _mm256_min_ps(mins[c], vlut_stripe1);
            maxs[c] = _mm256_max_ps(maxs[c], vlut_stripe0);
            maxs[c] = _mm256_max_ps(maxs[c], vlut_stripe1);
        }
    }
    _compute_offsets_scale_from_mins_maxs(
        mins, maxs, ncodebooks, out_offsets, out_offset_sum, out_scale);
}

template<int ncodebooks, int RowTileSz=1>
void quantize_luts(const float* luts_f32, int nrows,
                   const float* offsets,
                   float scaleby, uint8_t* out_luts)
{
    static constexpr int lut_sz = 16;
    static constexpr int codebook_group_sz = 2; // 4 f32 luts -> 1 epu8 lut
    static constexpr int packet_width = 8; // objs per simd register
    static constexpr int nstripes = lut_sz / packet_width;
    static constexpr int ncodebook_groups = ncodebooks / codebook_group_sz;
    static_assert(ncodebooks % codebook_group_sz == 0,
        "Number of codebooks must be a multiple of 2");
    assert(nrows % RowTileSz == 0);

    auto row_stride = ncodebooks * lut_sz;
    auto nrow_blocks = RowTileSz > 1 ? nrows / RowTileSz : 0;
    auto nrows_round = nrow_blocks * RowTileSz;

    const float* in_ptrs[RowTileSz];
    uint8_t* out_ptrs[RowTileSz];

    // if luts constant, just zero the output and return
    if (scaleby <= 0.f) {
        size_t total_sz = nrows * ncodebooks * lut_sz;
        for (size_t i = 0; i < total_sz; i++) {
            *out_luts++ = 0;
        }
        return;
    }

    /* inner loop gets unrolled to this:
     vmovaps        ymm10, ymmword ptr [rbx + 4*rdx + 384]
     vfmadd132ps    ymm10, ymm4, ymm1 # ymm10 = (ymm10 * ymm1) + ymm4
     vmovaps        ymm11, ymmword ptr [rbx + 4*rdx + 416]
     vfmadd132ps    ymm11, ymm4, ymm1 # ymm11 = (ymm11 * ymm1) + ymm4
     vcvtps2dq      ymm10, ymm10
     vmovaps        ymm12, ymmword ptr [rbx + 4*rdx + 448]
     vfmadd132ps    ymm12, ymm5, ymm1 # ymm12 = (ymm12 * ymm1) + ymm5
     vcvtps2dq      ymm11, ymm11
     vmovaps        ymm13, ymmword ptr [rbx + 4*rdx + 480]
     vfmadd132ps    ymm13, ymm5, ymm1 # ymm13 = (ymm13 * ymm1) + ymm5
     vcvtps2dq      ymm12, ymm12
     vpackssdw      ymm10, ymm10, ymm11
     vcvtps2dq      ymm11, ymm13
     vpackssdw      ymm11, ymm12, ymm11
     vpackuswb      ymm10, ymm10, ymm11
     vpermd         ymm10, ymm2, ymm10
     vmovdqa        ymmword ptr [r14 + rdx + 96], ymm10
     */
    // given offsets and overall scale, actually quantize luts; this is
    // basically the same as the first 2 loops that pull out the offsets
    __m256i luts_epi16[RowTileSz][codebook_group_sz];
    auto vmulby = _mm256_set1_ps(scaleby);
    for (int r = 0; r < nrow_blocks; r++) {
        // new set of rows; reset read and write ptrs
        for (int rr = 0; rr < RowTileSz; rr++) {
            auto row = (r * RowTileSz) + rr;
            in_ptrs[rr] = luts_f32 + (row * row_stride);
            out_ptrs[rr] = out_luts + (row * row_stride);
        }
        // for each column group, col in group, row in rowgroup
        for (int g = 0; g < ncodebook_groups; g++) {
            for (int gg = 0; gg < codebook_group_sz; gg++) {
                auto c = (g * codebook_group_sz) + gg;
                // auto fma_offset = offsets[c] * scaleby;
                auto fma_offset = offsets[c];
                auto voffset = _mm256_set1_ps(fma_offset);  // p5

                for (int rr = 0; rr < RowTileSz; rr++) {
                    auto vlut_f32_0 = _mm256_load_ps(in_ptrs[rr]);
                    in_ptrs[rr] += packet_width;
                    auto vlut_f32_1 = _mm256_load_ps(in_ptrs[rr]);
                    in_ptrs[rr] += packet_width;

                    // fmas on p01; cvtps on p1
                    vlut_f32_0 = _mm256_fmsub_ps(vlut_f32_0, vmulby, voffset);
                    vlut_f32_1 = _mm256_fmsub_ps(vlut_f32_1, vmulby, voffset);
                    auto vlut_epi32_0 = _mm256_cvtps_epi32(vlut_f32_0);
                    auto vlut_epi32_1 = _mm256_cvtps_epi32(vlut_f32_1);

                    // the tricky part here is that we have to buffer luts from
                    // two consecutive columns to get a full epi32 vector
                    luts_epi16[rr][gg] = _mm256_packs_epi32( // p5
                        vlut_epi32_0, vlut_epi32_1);
                }
            }
            // combine epi16 luts from the 2 cols into 1 epu8 lut and store it
            for (int rr = 0; rr < RowTileSz; rr++) {
                auto lut0 = luts_epi16[rr][0];
                auto lut1 = luts_epi16[rr][1];
                auto lut = _mm256_packus_epi16(lut0, lut1); // p5
                lut = _mm256_permutevar8x32_epi32(  // p5
                    lut, _mm256_setr_epi32(0,4, 1,5, 2,6, 3,7)); // p5
                _mm256_store_si256((__m256i*)out_ptrs[rr], lut);
                out_ptrs[rr] += 32;
            }
        }
    }
    for (int row = nrows_round; row < nrows; row++) { // handle trailing rows
        auto in_ptr = luts_f32 + (row * row_stride);
        auto out_ptr = out_luts + (row * row_stride);
        for (int g = 0; g < ncodebook_groups; g++) {
            for (int gg = 0; gg < codebook_group_sz; gg++) {
                auto c = (g * codebook_group_sz) + gg;
                // auto fma_offset = offsets[c] * scaleby;
                auto fma_offset = offsets[c];
                auto voffset = _mm256_set1_ps(fma_offset);

                auto vlut_f32_0 = _mm256_load_ps(in_ptr);
                in_ptr += packet_width;
                auto vlut_f32_1 = _mm256_load_ps(in_ptr);
                in_ptr += packet_width;
                
                //multiply then subtract, but to get final out we divide then add. a*b-c = x; x/b+c!=a
                vlut_f32_0 = _mm256_fmsub_ps(vlut_f32_0, vmulby, voffset); 
                vlut_f32_1 = _mm256_fmsub_ps(vlut_f32_1, vmulby, voffset);
                // vlut_f32_0 = fma(vlut_f32_0, vmulby, voffset);
                // vlut_f32_1 = fma(vlut_f32_1, vmulby, voffset);
                auto vlut_epi32_0 = _mm256_cvtps_epi32(vlut_f32_0);
                auto vlut_epi32_1 = _mm256_cvtps_epi32(vlut_f32_1);

                luts_epi16[0][gg] = _mm256_packs_epi32(
                    vlut_epi32_0, vlut_epi32_1);
            }
            auto lut0 = luts_epi16[0][0];
            auto lut1 = luts_epi16[0][1];
            //we're saturating to 255 for everything that was above the cutoff.
            //Why do we need to multiply the luts by scaleby? That's what causes everything to hit max
            //(In case scaleby was 0.01 and everything would just got to 0 without it)
            auto lut = _mm256_packus_epi16(lut0, lut1); 
            lut = _mm256_permutevar8x32_epi32(
                lut, _mm256_setr_epi32(0,4, 1,5, 2,6, 3,7));
            _mm256_store_si256((__m256i*)out_ptr, lut);
            out_ptr += 32;
        }
    }
}
template<int RowTileSz=1>
void quantize_luts(const float* luts_f32, int nrows, int ncodebooks,
                   const float* offsets, float scaleby, uint8_t* out_luts)
{
    switch (ncodebooks) {
        case 2: quantize_luts<2, RowTileSz>(
            luts_f32, nrows, offsets, scaleby, out_luts); break;
        case 4: quantize_luts<4, RowTileSz>(
            luts_f32, nrows, offsets, scaleby, out_luts); break;
        case 8: quantize_luts<8, RowTileSz>(
            luts_f32, nrows, offsets, scaleby, out_luts); break;
        case 16: quantize_luts<16, RowTileSz>(
            luts_f32, nrows, offsets, scaleby, out_luts); break;
        case 32: quantize_luts<32, RowTileSz>(
            luts_f32, nrows, offsets, scaleby, out_luts); break;
        case 64: quantize_luts<64, RowTileSz>(
            luts_f32, nrows, offsets, scaleby, out_luts); break;
        case 128: quantize_luts<128, RowTileSz>(
            luts_f32, nrows, offsets, scaleby, out_luts); break;
        case 256: quantize_luts<256, RowTileSz>(
            luts_f32, nrows, offsets, scaleby, out_luts); break;
        default: assert(false);  // unsupported ncodebooks
    }
}

static constexpr bool is_power_of_2(int64_t x) {
    return (x & (x - 1)) == 0 && x > 0;
}

// https://godbolt.org/z/cP80FF
// template<int NBytes, int UpcastEvery=16, bool Force16BitOutput=true>
template<int NBytes, int UpcastEvery=16, bool Force16BitOutput=false>
void mithral_scan_notile(const uint8_t* codes, int64_t nblocks,
                         const uint8_t* luts, uint8_t* dists_out)
{
    static_assert(NBytes > 0, "Code length <= 0 is not valid");
    static_assert(UpcastEvery % 2 == 0, "UpcastEvery must be even");
    static_assert(UpcastEvery >= 2, "UpcastEvery must be >= 2");
    static_assert(UpcastEvery <= 256, "UpcastEvery must be <= 128");
    static_assert(is_power_of_2(UpcastEvery),
        "UpcastEvery must be a power of 2!");
    static constexpr int ncodebooks = 2 * NBytes;
    static constexpr int ncols = NBytes;
    static constexpr int actually_upcast_every = MIN(UpcastEvery, ncodebooks);
    // static_assert(actually_upcast_every == 2 || UpcastEvery == 4 || UpcastEvery == 8, "UpcastEvery must be <= 16");
    static constexpr int colgroup_sz = actually_upcast_every / 2;
    static_assert(is_power_of_2(colgroup_sz),
        "Invalid number of columns to unroll at once");
    static constexpr int ncolgroups = ncols / colgroup_sz;
    static_assert(colgroup_sz <= ncodebooks, "WTF, did some math wrong");
    static_assert(ncols % colgroup_sz == 0,
        "Size of column group must evenly number of columns");

    // unpack 16B luts into 32B registers
    __m256i luts_ar[ncodebooks];
    auto lut_ptr = luts;
    for (uint8_t j = 0; j < NBytes; j++) {
        auto both_luts = load_si256i(lut_ptr);
        lut_ptr += 32;
        auto lut0 = _mm256_permute2x128_si256(both_luts, both_luts, 0 + (0 << 4));
        auto lut1 = _mm256_permute2x128_si256(both_luts, both_luts, 1 + (1 << 4));
        luts_ar[2 * j] = lut0;
        luts_ar[2 * j + 1] = lut1;
    }

    for (int64_t i = 0; i < nblocks; i++) {
        // used if ncolgroups > 1, in which case we have to upcast
        auto totals_0_15 = _mm256_setzero_si256();
        auto totals_16_31 = _mm256_setzero_si256();

        auto low_4bits_mask = _mm256_set1_epi8(0x0F); // not static so sits in reg

        for (int g = 0; g < ncolgroups; g++) {

            __m256i avg_prev1 = _mm256_undefined_si256();
            __m256i avg_prev2 = _mm256_undefined_si256();
            __m256i avg_prev4 = _mm256_undefined_si256();
            __m256i avg_prev8 = _mm256_undefined_si256();
            __m256i avg_prev16 = _mm256_undefined_si256();
            __m256i avg_prev32 = _mm256_undefined_si256();
            __m256i avg_prev64 = _mm256_undefined_si256();
            __m256i avg_prev128 = _mm256_undefined_si256();

            // TODO tile this to compute OutColTileSz outputs at once

            #pragma unroll
            for (int gg = 0; gg < colgroup_sz; gg++) {
                auto j = (g * colgroup_sz) + gg;

                auto x_col = load_si256i(codes);
                codes += 32;

                auto lut_low = luts_ar[2 * j];
                auto lut_high = luts_ar[2 * j + 1];

                auto x_low = _mm256_and_si256(x_col, low_4bits_mask);
                auto x_shft = _mm256_srli_epi16(x_col, 4);
                auto x_high = _mm256_and_si256(x_shft, low_4bits_mask);

                auto dists_low = _mm256_shuffle_epi8(lut_low, x_low);
                auto dists_high = _mm256_shuffle_epi8(lut_high, x_high);

                auto avgs = _mm256_avg_epu8(dists_low, dists_high);

                // update running averages; this is messy because if you
                // need to current and previous average to be over the same
                // number of values, or else it's a weird weighted average
                // instead of a true average
                // note that we need to use inline asm to get the right
                // instruction here on my machine for unclear reasons
                if (gg % 128 == 127) {
                    auto new_avg_prev2 = avg_epu8(avg_prev1, avgs);
                    auto new_avg_prev4 = avg_epu8(avg_prev2, new_avg_prev2);
                    auto new_avg_prev8 = avg_epu8(avg_prev4, new_avg_prev4);
                    auto new_avg_prev16 = avg_epu8(avg_prev8, new_avg_prev8);
                    auto new_avg_prev32 = avg_epu8(avg_prev16, new_avg_prev16);
                    auto new_avg_prev64 = avg_epu8(avg_prev32, new_avg_prev32);
                    avg_prev128 = avg_epu8(avg_prev64, new_avg_prev64);
                }
                if (gg % 64 == 63) {
                    auto new_avg_prev2 = avg_epu8(avg_prev1, avgs);
                    auto new_avg_prev4 = avg_epu8(avg_prev2, new_avg_prev2);
                    auto new_avg_prev8 = avg_epu8(avg_prev4, new_avg_prev4);
                    auto new_avg_prev16 = avg_epu8(avg_prev8, new_avg_prev8);
                    auto new_avg_prev32 = avg_epu8(avg_prev16, new_avg_prev16);
                    avg_prev64 = avg_epu8(avg_prev32, new_avg_prev32);
                }
                if (gg % 32 == 31) {
                    auto new_avg_prev2 = avg_epu8(avg_prev1, avgs);
                    auto new_avg_prev4 = avg_epu8(avg_prev2, new_avg_prev2);
                    auto new_avg_prev8 = avg_epu8(avg_prev4, new_avg_prev4);
                    auto new_avg_prev16 = avg_epu8(avg_prev8, new_avg_prev8);
                    avg_prev32 = avg_epu8(avg_prev16, new_avg_prev16);
                }
                if (gg % 16 == 15) {
                    auto new_avg_prev2 = avg_epu8(avg_prev1, avgs);
                    auto new_avg_prev4 = avg_epu8(avg_prev2, new_avg_prev2);
                    auto new_avg_prev8 = avg_epu8(avg_prev4, new_avg_prev4);
                    avg_prev16 = avg_epu8(avg_prev8, new_avg_prev8);
                }
                // if ((gg + 1) % 8 == 0) {
                if (gg % 8 == 7) {
                    auto new_avg_prev2 = avg_epu8(avg_prev1, avgs);
                    auto new_avg_prev4 = avg_epu8(avg_prev2, new_avg_prev2);
                    avg_prev8 = avg_epu8(avg_prev4, new_avg_prev4);
                }
                // if ((gg + 1) % 4 == 0) {
                if (gg % 4 == 3) {
                    auto new_avg_prev2 = avg_epu8(avg_prev1, avgs);
                    avg_prev4 = avg_epu8(avg_prev2, new_avg_prev2);
                }
                // if ((gg + 1) % 2 == 0) {
                if (gg % 2 == 1) {
                    avg_prev2 = avg_epu8(avg_prev1, avgs);
                } else {
                    avg_prev1 = avgs;
                }
            }
            auto group_avg = colgroup_sz == 1  ? avg_prev1 :
                             colgroup_sz == 2  ? avg_prev2 :
                             colgroup_sz == 4  ? avg_prev4 :
                             colgroup_sz == 8  ? avg_prev8 :
                             colgroup_sz == 16 ? avg_prev16 :
                             colgroup_sz == 32 ? avg_prev32 :
                             colgroup_sz == 64 ? avg_prev64 :
                             avg_prev128;

            if (ncolgroups == 1 && !Force16BitOutput) { // write out 8b values
                // _mm256_store_si256((__m256i*)dists_out, group_avg);
                _mm256_stream_si256((__m256i*)dists_out, group_avg);
                dists_out += 32;
            } else {
                auto avgs_0_15 = _mm256_cvtepi8_epi16(
                    _mm256_extracti128_si256(group_avg, 0));
                auto avgs_16_31 = _mm256_cvtepi8_epi16(
                    _mm256_extracti128_si256(group_avg, 1));
                totals_0_15 = _mm256_add_epi16(totals_0_15, avgs_0_15);
                totals_16_31 = _mm256_add_epi16(totals_16_31, avgs_16_31);
            }
        }
        // if (true) {
        if (ncolgroups > 1 || Force16BitOutput) {
            // _mm256_store_si256((__m256i*)(dists_out + 0), totals_0_15);
            // _mm256_store_si256((__m256i*)(dists_out + 32), totals_16_31);
            _mm256_stream_si256((__m256i*)(dists_out + 0), totals_0_15);
            _mm256_stream_si256((__m256i*)(dists_out + 32), totals_16_31);
            dists_out += 64;
        }
    }
}

// I don't think this handles UpcastEvery correctly
template<int NBytes, int UpcastEvery=16, int _OutTileSz=1,
         bool Force16BitOutput=false, typename OutType=uint8_t>
void mithral_scan(const uint8_t* codes, int64_t nblocks,
                  const uint8_t* luts, OutType* dists_out, int64_t N=-1)
{
    static_assert(NBytes > 0, "Code length <= 0 is not valid");
    static_assert(UpcastEvery % 2 == 0, "UpcastEvery must be even");
    static_assert(UpcastEvery >= 2, "UpcastEvery must be >= 2");
    static_assert(UpcastEvery <= 256, "UpcastEvery must be <= 256, only average 128 times max");
    static_assert(is_power_of_2(UpcastEvery),
        "UpcastEvery must be a power of 2!");
    // Upcast means that after average together N numbers, they're summed
    static constexpr int ncodebooks = 2 * NBytes;
    static constexpr int ncols = NBytes;
    assert((!std::is_same<OutType, uint8_t>::value) || (UpcastEvery >= ncodebooks));
        //"U8s always written straight to output; they're not summed across averaging groups (would saturate)"
    static constexpr int actually_upcast_every = MIN(MIN(UpcastEvery, ncodebooks), 128*2); //128 is largest can handle in averaging for colgroup_sz
    static constexpr int colgroup_sz = actually_upcast_every / 2;
    static_assert(is_power_of_2(colgroup_sz),
        "Invalid number of columns to unroll at once");
    static constexpr int ncolgroups = ncols / colgroup_sz; //=ncols * 2 /ncodebooks; why depends on codebook size?
    static_assert(colgroup_sz <= ncodebooks, "WTF, did some math wrong");
    static_assert(ncols % colgroup_sz == 0,
        "Size of column group must evenly number of columns");
    static constexpr bool use_uint8_output = std::is_same<OutType, uint8_t>::value && ncolgroups == 1 && !Force16BitOutput;
    static constexpr int OutTileSz = _OutTileSz > 0 ? _OutTileSz : 1;

    static constexpr bool avg_as_uint8 = true; //use_uint8_output; 
    static_assert(avg_as_uint8 >= use_uint8_output, "use_uint8_output implies avg_as_uint8, but could ouput int16 and avg_as_uint8");

    //out_stride should always equal N since colmajor matrix? Won't always for last chunk of blocks from mithral_scan_in_chunks
    //int64_t out_stride = use_uint8_output ? nblocks * 32 : 2 * nblocks * 32; //int16 output should keep stride outputs the same
    int64_t out_stride = N > 0 ? N : nblocks * 32; //int16 vs int8 dists_out still inc ix by same amount
    int lut_stride = ncodebooks * 16;

    OutType* out_ptrs[OutTileSz];
    for (int mm = 0; mm < OutTileSz; mm++) {
        out_ptrs[mm] = dists_out + (mm * out_stride);
    }
    
    //start of each code col at the top
    const uint8_t* code_cols[NBytes];
    for (uint8_t j = 0; j < NBytes; j++) {
        code_cols[j] = codes + j*out_stride;    
    }

    // unpack 16B luts into 32B registers
    __m256i lut_arrays[ncodebooks][OutTileSz];
    for (int mm = 0; mm < OutTileSz; mm++) {
        auto lut_ptr = luts + (mm * lut_stride);
        for (uint8_t j = 0; j < NBytes; j++) {
            auto both_luts = load_si256i(lut_ptr);
            lut_ptr += 32;
            auto lut0 = _mm256_permute2x128_si256(both_luts, both_luts, 0 + (0 << 4));
            auto lut1 = _mm256_permute2x128_si256(both_luts, both_luts, 1 + (1 << 4));
            lut_arrays[2 * j][mm] = lut0;
            lut_arrays[2 * j + 1][mm] = lut1;
        }
    }

    for (int64_t i = 0; i < nblocks; i++) {
        __m256i totals_0_15[OutTileSz];
        __m256i totals_16_31[OutTileSz];
        for (int mm = 0; mm < OutTileSz; mm++) {
            totals_0_15[mm] = _mm256_setzero_si256();
            totals_16_31[mm] = _mm256_setzero_si256();
        }

        auto low_4bits_mask = _mm256_set1_epi8(0x0F); // not static so sits in reg

        for (int g = 0; g < ncolgroups; g++) {

            __m256i avg_prev1[OutTileSz];
            __m256i avg_prev2[OutTileSz];
            __m256i avg_prev4[OutTileSz];
            __m256i avg_prev8[OutTileSz];
            __m256i avg_prev16[OutTileSz];
            __m256i avg_prev32[OutTileSz];
            __m256i avg_prev64[OutTileSz];
            __m256i avg_prev128[OutTileSz];
            for (int mm = 0; mm < OutTileSz; mm++) {
                avg_prev1[mm] = _mm256_undefined_si256();
                avg_prev2[mm] = _mm256_undefined_si256();
                avg_prev4[mm] = _mm256_undefined_si256();
                avg_prev8[mm] = _mm256_undefined_si256();
                avg_prev16[mm] = _mm256_undefined_si256();
                avg_prev32[mm] = _mm256_undefined_si256();
                avg_prev64[mm] = _mm256_undefined_si256();
                avg_prev128[mm] = _mm256_undefined_si256();
            }

            #pragma unroll
            for (int gg = 0; gg < colgroup_sz; gg++) {
                auto j = (g * colgroup_sz) + gg; //2*j and 2*j+1 are the codebook_ix's used for this loop
                
                auto x_col = load_si256i(code_cols[j]); //have 256/4=64 entries in x_col, each int represents 2 centroid_ix or'd and shifted
                code_cols[j] += 32;
                //auto x_col = load_si256i(codes); 
                //codes += 32;
                 auto x_low = _mm256_and_si256(x_col, low_4bits_mask);
                 auto x_shft = _mm256_srli_epi16(x_col, 4); //x_shft is good: print([i&15 for i in [] ])  
                 auto x_high = _mm256_and_si256(x_shft, low_4bits_mask);  //shifting each i16 by 4 moves '2 codes' to low_ix, and out just the 2 i8 codes

                for (int mm = 0; mm < OutTileSz; mm++) {
                    auto lut_low = lut_arrays[2 * j][mm]; 
                    auto lut_high = lut_arrays[2 * j + 1][mm];
                    
                    //Matches: np.ravel(task.amm.luts, order='c')[np.ravel(task.amm.codes, order='f')[:32]]
                    auto dists_low = _mm256_shuffle_epi8(lut_low, x_low);
                    auto dists_high = _mm256_shuffle_epi8(lut_high, x_high);

                    if (avg_as_uint8) {
                        auto avgs = _mm256_avg_epu8(dists_low, dists_high);

                        // update running averages; this is messy because you
                        // need the current and previous average to be over the same
                        // number of values, or else it's a weird weighted average
                        // instead of a true average;
                        // note that we need to use inline asm to get the right
                        // instruction here on my machine for unclear reasons
                        if (gg % 128 == 127) {
                            assert(gg < 128);//else avg_prev128 incorrect
                            auto new_avg_prev2 = avg_epu8(avg_prev1[mm], avgs);
                            auto new_avg_prev4 = avg_epu8(avg_prev2[mm], new_avg_prev2);
                            auto new_avg_prev8 = avg_epu8(avg_prev4[mm], new_avg_prev4);
                            auto new_avg_prev16 = avg_epu8(avg_prev8[mm], new_avg_prev8);
                            auto new_avg_prev32 = avg_epu8(avg_prev16[mm], new_avg_prev16);
                            auto new_avg_prev64 = avg_epu8(avg_prev32[mm], new_avg_prev32);
                            avg_prev128[mm] = avg_epu8(avg_prev64[mm], new_avg_prev64);
                        }
                        if (gg % 64 == 63) {
                            auto new_avg_prev2 = avg_epu8(avg_prev1[mm], avgs);
                            auto new_avg_prev4 = avg_epu8(avg_prev2[mm], new_avg_prev2);
                            auto new_avg_prev8 = avg_epu8(avg_prev4[mm], new_avg_prev4);
                            auto new_avg_prev16 = avg_epu8(avg_prev8[mm], new_avg_prev8);
                            auto new_avg_prev32 = avg_epu8(avg_prev16[mm], new_avg_prev16);
                            avg_prev64[mm] = avg_epu8(avg_prev32[mm], new_avg_prev32);
                        }
                        if (gg % 32 == 31) {
                            auto new_avg_prev2 = avg_epu8(avg_prev1[mm], avgs);
                            auto new_avg_prev4 = avg_epu8(avg_prev2[mm], new_avg_prev2);
                            auto new_avg_prev8 = avg_epu8(avg_prev4[mm], new_avg_prev4);
                            auto new_avg_prev16 = avg_epu8(avg_prev8[mm], new_avg_prev8);
                            avg_prev32[mm] = avg_epu8(avg_prev16[mm], new_avg_prev16);
                        }
                        if (gg % 16 == 15) {
                            auto new_avg_prev2 = avg_epu8(avg_prev1[mm], avgs);
                            auto new_avg_prev4 = avg_epu8(avg_prev2[mm], new_avg_prev2);
                            auto new_avg_prev8 = avg_epu8(avg_prev4[mm], new_avg_prev4);
                            avg_prev16[mm] = avg_epu8(avg_prev8[mm], new_avg_prev8);
                        }
                        if (gg % 8 == 7) {
                            auto new_avg_prev2 = avg_epu8(avg_prev1[mm], avgs);
                            auto new_avg_prev4 = avg_epu8(avg_prev2[mm], new_avg_prev2);
                            avg_prev8[mm] = avg_epu8(avg_prev4[mm], new_avg_prev4);
                        }
                        if (gg % 4 == 3) {
                            auto new_avg_prev2 = avg_epu8(avg_prev1[mm], avgs);
                            avg_prev4[mm] = avg_epu8(avg_prev2[mm], new_avg_prev2);
                        }
                        if (gg % 2 == 1) {
                            avg_prev2[mm] = avg_epu8(avg_prev1[mm], avgs);
                        } else {
                            avg_prev1[mm] = avgs;
                        }
                        
                    } else {
                        //Average together then sum as int16
                        auto avgs = _mm256_avg_epu8(dists_low, dists_high);
                        auto avgs_0_15 = _mm256_cvtepu8_epi16( //used to cast u8s to i16s=u16s here
                            _mm256_extracti128_si256(avgs, 0));
                        auto avgs_16_31 = _mm256_cvtepu8_epi16(
                            _mm256_extracti128_si256(avgs, 1));

                        //so output is scaled the same, had ~no time impact
                        avgs_0_15 =  _mm256_adds_epu16(avgs_0_15,  avgs_0_15); 
                        avgs_16_31 =  _mm256_adds_epu16(avgs_16_31,  avgs_16_31); 
                        totals_0_15[mm] = _mm256_adds_epu16(totals_0_15[mm], avgs_0_15);
                        totals_16_31[mm] = _mm256_adds_epu16(totals_16_31[mm], avgs_16_31);


                        ////sum as int16 is a little slower
                        //auto avgs_0_15_low = _mm256_cvtepu8_epi16( //used to cast i8s to i16s. No u8 to u16 instruction
                        //    _mm256_extracti128_si256(dists_low, 0));
                        //auto avgs_16_31_low = _mm256_cvtepu8_epi16(
                        //    _mm256_extracti128_si256(dists_low, 1));
                        //auto avgs_0_15_high = _mm256_cvtepu8_epi16( 
                        //    _mm256_extracti128_si256(dists_high, 0));
                        //auto avgs_16_31_high = _mm256_cvtepu8_epi16(
                        //    _mm256_extracti128_si256(dists_high, 1));
                        ////with _mm256_adds_epu16 vs add_epi16 won't make a difference, as long as <128 ncolgroups 
                        //totals_0_15[mm] = _mm256_adds_epu16(totals_0_15[mm], avgs_0_15_low);
                        //totals_16_31[mm] = _mm256_adds_epu16(totals_16_31[mm], avgs_16_31_low);
                        //totals_0_15[mm] = _mm256_adds_epu16(totals_0_15[mm], avgs_0_15_high);
                        //totals_16_31[mm] = _mm256_adds_epu16(totals_16_31[mm], avgs_16_31_high);
                    }
                }
            }

            
            for (int mm = 0; avg_as_uint8 && mm < OutTileSz; mm++) {
                auto group_avg = colgroup_sz == 1  ? avg_prev1[mm] :
                                 colgroup_sz == 2  ? avg_prev2[mm] :
                                 colgroup_sz == 4  ? avg_prev4[mm] :
                                 colgroup_sz == 8  ? avg_prev8[mm] :
                                 colgroup_sz == 16 ? avg_prev16[mm] :
                                 colgroup_sz == 32 ? avg_prev32[mm] :
                                 colgroup_sz == 64 ? avg_prev64[mm] :
                                 avg_prev128[mm];
                if (use_uint8_output) { // write out 8b values
                    _mm256_stream_si256((__m256i*)out_ptrs[mm], group_avg);
                    out_ptrs[mm] += 32;
                } else {
                    auto avgs_0_15 = _mm256_cvtepu8_epi16( //used to cast u8s to i16s=u16s here
                        _mm256_extracti128_si256(group_avg, 0));
                    auto avgs_16_31 = _mm256_cvtepu8_epi16(
                        _mm256_extracti128_si256(group_avg, 1));
                    //with _mm256_adds_epu16 vs add_epi16 won't make a difference, as long as <128 ncolgroups 
                    totals_0_15[mm] = _mm256_adds_epu16(totals_0_15[mm], avgs_0_15);
                    totals_16_31[mm] = _mm256_adds_epu16(totals_16_31[mm], avgs_16_31);
                } 
            }
            
        }
        if (!use_uint8_output) {
            for (int mm = 0; mm < OutTileSz; mm++) {
                _mm256_stream_si256(
                    (__m256i*)(out_ptrs[mm] + 0), totals_0_15[mm]);
                _mm256_stream_si256(
                    (__m256i*)(out_ptrs[mm] + 16), totals_16_31[mm]);
                out_ptrs[mm] += 32; //changed from 32/64 to 16/32 for int16 outputs
            }
        }
    }
}

template<int UpcastEvery=64>
void mithral_scan_notile(const uint8_t* codes, int64_t nblocks, int ncodebooks,
                         const uint8_t* luts, uint8_t* out)
{
    switch(ncodebooks) {
        case 2: mithral_scan_notile<1, UpcastEvery>(codes, nblocks, luts, out); break;
        case 4: mithral_scan_notile<2, UpcastEvery>(codes, nblocks, luts, out); break;
        case 8: mithral_scan_notile<4, UpcastEvery>(codes, nblocks, luts, out); break;
        case 16: mithral_scan_notile<8, UpcastEvery>(codes, nblocks, luts, out); break;
        case 32: mithral_scan_notile<16, UpcastEvery>(codes, nblocks, luts, out); break;
        case 64: mithral_scan_notile<32, UpcastEvery>(codes, nblocks, luts, out); break;
        case 128: mithral_scan_notile<64, UpcastEvery>(codes, nblocks, luts, out); break;
        default: assert(false);  // unsupported ncodebooks
    }
}

template<int UpcastEvery=64, int OutTileSz=1, typename OutType=uint8_t>
void mithral_scan_T(const uint8_t* codes, int64_t nblocks, int ncodebooks,
             const uint8_t* luts, OutType* out, int64_t N=-1)
{
    switch(ncodebooks) {
        case 2: mithral_scan<1,   UpcastEvery, OutTileSz>(
            codes, nblocks, luts, out, N); break;
        case 4: mithral_scan<2,   UpcastEvery, OutTileSz>(
            codes, nblocks, luts, out, N); break;
        case 8: mithral_scan<4,   UpcastEvery, OutTileSz>(
            codes, nblocks, luts, out, N); break;
        case 16: mithral_scan<8,  UpcastEvery, OutTileSz>(
            codes, nblocks, luts, out, N); break;
        case 32: mithral_scan<16, UpcastEvery, OutTileSz>(
            codes, nblocks, luts, out, N); break;
        case 64: mithral_scan<32, UpcastEvery, OutTileSz>(
            codes, nblocks, luts, out, N); break;
        case 128: mithral_scan<64, UpcastEvery, OutTileSz>(
            codes, nblocks, luts, out, N); break;
        case 256: mithral_scan<128, UpcastEvery, OutTileSz>(
            codes, nblocks, luts, out, N); break;
        default: assert(false);  // unsupported ncodebooks
    }
}

template<int UpcastEvery=64>
// void mithral_scan(const uint8_t* codes, int64_t nblocks, int ncodebooks,
void mithral_scan_nochunk(const uint8_t* codes, int64_t nblocks, int ncodebooks,
                  int noutputs, const uint8_t* luts, uint8_t* dists_out)
{
    static constexpr int block_nrows = 32;
    static constexpr int lut_sz = 16;
    auto out_ptr = dists_out;
    auto out_stride = nblocks * block_nrows;
    auto lut_ptr = luts;
    auto lut_stride = ncodebooks * lut_sz;

    for (int i = 0; i < noutputs; i++) {
        mithral_scan_notile<UpcastEvery>(
            codes, nblocks, ncodebooks, lut_ptr, out_ptr);
        out_ptr += out_stride;
        lut_ptr += lut_stride;
    }
}

// We chunk the code and output rows, we iterate over all luts in loop here.
// Since the full codebooks are in colorder, each encoded X row is stored in row order.
// Take row i of codes, row j of luts and can write out[i,j] 
template<int UpcastEvery=128, int _OutTileSz=2, typename OutType=uint8_t> //OutType should be uint8 or uint16
void mithral_scan_in_chunks(const uint8_t* codes, int64_t nblocks, int ncodebooks,
                  int noutputs, const uint8_t* luts, OutType* dists_out)
{
    static constexpr int OutTileSz = _OutTileSz > 0 ? _OutTileSz : 1;
    static constexpr int block_nrows = 32;
    static constexpr int lut_sz = 16;
    // static constexpr int chunk_nrows = 999999;  // no chunking
    // static constexpr int chunk_nblocks = chunk_nrows / block_nrows;

    static constexpr int target_chunk_nbytes = 24 * 1024;  // most of L1 cache
    int codes_row_nbytes = ncodebooks / 2;
    int codes_block_nbytes = codes_row_nbytes * block_nrows;
    int chunk_nblocks = target_chunk_nbytes / codes_block_nbytes;
    int chunk_nrows = chunk_nblocks * block_nrows;

    auto codes_row_stride = ncodebooks / 2;
    //auto codes_chunk_stride = codes_row_stride * chunk_nrows;
    auto out_chunk_stride = chunk_nrows;
    auto out_col_stride = nblocks * block_nrows;
    auto lut_chunk_stride = 0; //since we already loop over all luts
    auto lut_col_stride = ncodebooks * lut_sz;

    auto nchunks = (nblocks + chunk_nblocks - 1) / chunk_nblocks;
    for (int chunk = 0; chunk < nchunks; chunk++) { // for each chunk of codes/output rows to write 
        int64_t use_nblocks = chunk_nblocks;
        if (chunk == (nchunks - 1)) { // handle last chunk
            auto nblocks_done = chunk * chunk_nblocks;
            use_nblocks = nblocks - nblocks_done;
        }
        const uint8_t* codes_ptr = codes + (chunk * chunk_nrows); //break X into smaller 'vertical' pieces
        OutType* out_ptr = dists_out + (chunk * out_chunk_stride);
        const uint8_t* lut_ptr = luts + (chunk * lut_chunk_stride);

        int nfullgroups_out = noutputs / OutTileSz;
        for (int g = 0; g < nfullgroups_out; g++) {
            mithral_scan_T<UpcastEvery, OutTileSz, OutType>( //OutTileSz is how many columns to write
               codes_ptr, use_nblocks, ncodebooks, lut_ptr, out_ptr, out_col_stride);
            
            //auto n = out_col_stride;
            //mithral_scan_test_zipped<OutType>(codes_ptr, n, ncodebooks, OutTileSz,
            //       0, 1,
            //       lut_ptr, out_ptr, use_nblocks*block_nrows);
    
            //We're iterating over columns of output, and rows of Luts
            out_ptr += out_col_stride * OutTileSz;
            lut_ptr += lut_col_stride * OutTileSz;
        }
        int ntrailing_outputs = noutputs % OutTileSz;
        for (int m = 0; m < ntrailing_outputs; m++) {
            mithral_scan_T<UpcastEvery, 1, OutType>(
                codes_ptr, use_nblocks, ncodebooks, lut_ptr, out_ptr, nblocks*block_nrows);
            out_ptr += out_col_stride * OutTileSz;
            lut_ptr += lut_col_stride * OutTileSz;
        }
    }
}

} // anon namespace

#ifdef MITHRAL_USE_BOLT_SAFE_SCAN
    #undef MITHRAL_USE_BOLT_SAFE_SCAN
#endif

#endif // __MITHRAL_HPP
