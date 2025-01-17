#include "test/scrap/reproduce_valgrind.hpp"

#include "src/quantize/mithral.hpp"
#include "test/quantize/profile_amm.hpp"

# include <eigen3/Eigen/Core>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/eigen.h>
#include <pybind11/numpy.h>

#if __has_include(<immintrin.h>)
#include <immintrin.h>
#endif
#include <string> 
#include <stdint.h>
#include <stdlib.h>

using namespace std;

namespace py = pybind11;
    
    struct Test { int N, D, M; const char* name;};

    void printNameMatmul(const MatmulTaskShape& shape)
    {
        cout << shape.name << endl;
        printf("shapename: %s\n", shape.name);
    }
    
    void printNameTest(const Test& shape)
    {
        cout << "out:    " << shape.name << endl;
        printf("shapename: %s\n", shape.name);
    }

//struct wrapped_mithral_amm_float : public mithral_amm<float> {
//    // //need to wrap 
//    //const float* centroids;
//    //const uint32_t* splitdims;
//    //const int8_t* splitvals;
//    //const scale_t* encode_scales;
//    //const offset_t* encode_offsets;
//    //const int* idxs; 
//
//};


PYBIND11_MODULE(mithral_wrapped, m) {
    m.doc() = "pybind11 plugin that wrapps mithral"; // Optional module docstring

    // -- reproduce_valgrind_error.hpp--
    m.def("test_valgrind", &test_valgrind, "mre of valgrind error");

   //---Mithral.hpp---
    m.def("add", &add, "A function that adds two numbers");
    m.def("sub", &sub, "A function that subs two numbers");
    
    //---profile_amm.hpp ---
    m.def("_profile_mithral", py::overload_cast<const MatmulTaskShape&, std::vector<int> , std::vector<float> >(&_profile_mithral), "generic C++ tests run");
    m.def("_profile_mithral_int8", py::overload_cast<const MatmulTaskShape&, std::vector<int> , std::vector<float> >(&_profile_mithral<int8_t>), "generic C++ tests run in int8");

    //Holds basic info 
    py::class_<MatmulTaskShape>(m, "MatmulTaskShape")
       .def(py::init([](int N, int D, int M, char *c){
           const size_t len = strlen(c);
           char * tmp_filename = new char[len + 1];
           strncpy(tmp_filename, c, len);
           tmp_filename[len] = '\0';  //nessisary line(why ?!?)
           const char* c2 = tmp_filename;
           return MatmulTaskShape{N,D,M,c2};
       }))
       .def_readwrite("name", &MatmulTaskShape::name) 
       .def_readonly("N", &MatmulTaskShape::N) 
       .def_readonly("D", &MatmulTaskShape::D) 
       .def_readonly("M", &MatmulTaskShape::M) 
        .def("__repr__",
        [](const MatmulTaskShape &a) {
            //std::cout << "PPPprinting:    " << a.name << std::endl; 
            std::string name(a.name);
            //std::cout << std::string("from C++: <example.MatmulTaskShape: name: " + name + " sizes: " + std::to_string(a.N) + " ," + std::to_string(a.D) + ", " + std::to_string(a.M) + ">") << std::endl;
            return std::string("<example.MatmulTaskShape: name: " + name + " sizes: " + std::to_string(a.N) + " ," + std::to_string(a.D) + ", " + std::to_string(a.M) + ">");
        }
        );

    //task wrapper with all info
    py::class_<mithral_amm_task<float>>(m, "mithral_amm_task_float")
       .def(py::init([](int N, int D, int M, int ncodebooks,
                        float lut_work_const){
           return mithral_amm_task<float>{N,D,M,ncodebooks, lut_work_const};
       }))
       .def("encode"                                                           , &mithral_amm_task<float>::encode)
       .def("mithral_encode_only"                                              , &mithral_amm_task<float>::mithral_encode_only)
       .def("resize"                                                           , &mithral_amm_task<float>::resize)
       .def("lut"                                                              , &mithral_amm_task<float>::lut)
       .def("scan"                                                             , &mithral_amm_task<float>::scan)
       .def("run_matmul"                                                       , &mithral_amm_task<float>::run_matmul)
       .def_readonly("amm"                                                     , &mithral_amm_task<float>::amm) // whole amm object
       .def("output"                                                           , &mithral_amm_task<float>::output) // by copy
        // .def("get_output"                                                      , &mithral_amm_task<float>::output, py::return_value_policy::reference_internal) // no copy, flags.writeable = True,  flags.owndata = False
       .def_readwrite("X"                                                      , &mithral_amm_task<float>::X) //can return out matricies?
       .def_readwrite("Q"                                                      , &mithral_amm_task<float>::Q)
       // stuff we pass into the amm object (would be learned during training). 
       // TODO: None of these params are actually used so should be removed(?)
       .def_readwrite("N_padded"                                               , &mithral_amm_task<float>::N_padded)
       .def_readwrite("centroids"                                              , &mithral_amm_task<float>::centroids)
       .def_readwrite("nsplits"                                                , &mithral_amm_task<float>::nsplits)
       .def_readwrite("splitdims"                                              , &mithral_amm_task<float>::splitdims)
       .def_readwrite("splitvals"                                              , &mithral_amm_task<float>::splitvals)
       .def_readwrite("encode_scales"                                          , &mithral_amm_task<float>::encode_scales)
       .def_readwrite("encode_offsets"                                         , &mithral_amm_task<float>::encode_offsets)
       .def_readwrite("nnz_per_centroid"                                       , &mithral_amm_task<float>::nnz_per_centroid)
       .def_readwrite("idxs"                                                   , &mithral_amm_task<float>::idxs)
        
       .def("__repr__",
       [](const mithral_amm_task<float> &a) {
           std::stringstream ss;
           ss << &a;  
           std::string address = ss.str(); 
           return std::string("mithral_amm_task<float> at " + address);
       }
       );
        
    //amm so can call attributes from mithtral_amm_task.output()
    using traits = mithral_input_type_traits<float>; //TODO  try int16 or 8? faster
    using scale_t = typename traits::encoding_scales_type;
    using offset_t = typename traits::encoding_offsets_type;
    using output_t = typename traits::output_type;
    py::class_<mithral_amm<float>>(m, "mithral_amm_float")
       .def(py::init([](int N, int D, int M, int ncodebooks, const float* centroids,
                        // for encoding
                        const uint32_t* splitdims, const int8_t* splitvals,
                        const scale_t* encode_scales, const offset_t* encode_offsets,
                        // for lut creation
                        const int* idxs, int nnz_per_centroid
                        ){
           return mithral_amm<float>{ N, D, M, ncodebooks,centroids,
                                     splitdims, splitvals,
                                     encode_scales, encode_offsets,
                                      idxs,  nnz_per_centroid};
        }))
        .def("scan_test"                    , &mithral_amm<float>::scan_test)
        .def("zip_bolt_colmajor_only"       , &mithral_amm<float>::zip_bolt_colmajor_only)
        .def("scan_test_zipped"             , &mithral_amm<float>::scan_test_zipped)
        // ctor params
        .def_readwrite("N"                  , &mithral_amm<float>::N)
        .def_readwrite("D"                  , &mithral_amm<float>::D)
        .def_readwrite("M"                  , &mithral_amm<float>::M)
        .def_readwrite("ncodebooks"         , &mithral_amm<float>::ncodebooks)
        //  ?dont have to make const attributes readonly?
        //  Need to copy these pointers to Python as arrays. Eigen matricies are auto-converted to numpy
        // nsplits_per_codebook=4; scan_block_nrows=32; lut_sz=16; CodebookTileSz=2; RowTileSz = 2 or 1
        // nblocks = N/scan_block_nrows; total_nsplits = ncodebooks * nsplits_per_codebook;centroids_codebook_stride=ncentroids*ncols; ncentroids=16
        
        .def_readwrite("centroids"        , &mithral_amm<float>::centroids)  //shape: centroids_codebook_stride * ncodebooks
        .def_readwrite("splitdims"        , &mithral_amm<float>::splitdims) //shape: total_nsplits
        .def_readwrite("splitvals"        , &mithral_amm<float>::splitvals) //shape:  total_nsplits
        .def_readwrite("encode_scales"    , &mithral_amm<float>::encode_scales) //shape: total_nsplits
        .def_readwrite("encode_offsets"   , &mithral_amm<float>::encode_offsets) //shape: total_nsplits
        .def_readwrite("idxs"             , &mithral_amm<float>::idxs) //shape:  nnz_per_centroid * ncodebooks // used if lut sparse (nnz_per_centroid>0)
        .def_readwrite("nnz_per_centroid" , &mithral_amm<float>::nnz_per_centroid) //value: lut_work_const > 0 ? lut_work_const * D / ncodebooks : D //lut_work_const an element from lutconsts {-1 , 1 , 2 , 4}
        //return COPY from pointers, but you have to know shape going in. Read won't trigger page faults(!?!)

        // Fns to call C++ with references from Python, only using amm obj. Will error if numpy not col order or f32
        .def("encode_col_order", [](mithral_amm<float> &self, Eigen::Ref<Eigen::Matrix<float, -1,-1, Eigen::ColMajor>> X) { 
            self.encode(X.data());
        })
        .def("lut_col_order", [](mithral_amm<float> &self, Eigen::Ref<Eigen::Matrix<float, -1,-1, Eigen::ColMajor>> Q) { 
            self.lut(Q.data());
        })
        // Return reference to C++ data w/o copy, in Col order, upcast to floats
        .def("scan_ret_col_order", [](mithral_amm<float> &self) {
            // below works but is slow, copy data 3x 
            self.scan();
            Eigen::MatrixXf out = self.out_mat.cast<float>();
            out *= self.ncodebooks / self.out_scale;
            out.array() += self.out_offset_sum;
            return out;
        })
        .def("scan_ret_col_order_upcast", [](mithral_amm<float> &self) { // convert out_mat into correct floats
            self.scan();
            const output_t * in = self.out_mat.data(); // column order
            auto NM = self.N * self.M; 
            float * out  =static_cast<float*>(aligned_alloc(64, NM* sizeof(float))); // python takes ownership at end
            #ifdef __AVX512F__
                const __m512 scales = _mm512_set1_ps(self.ncodebooks/self.out_scale);
                const __m512 offsets = _mm512_set1_ps(self.out_offset_sum);
                __m512* fma_ptr = (__m512*)out;
                if (std::is_same<output_t, uint8_t>::value) {
                    __m128i* u8_ptr = (__m128i*)in;
                    // N has to be multiple of 32, scan_block_nrows
                    for (int index = 0; index < NM ; index += 32) {
                        //__m128i u8 = *u8_ptr++; // Eigen  dynamic matricies aren't garenteed to be aligned, unless define EIGEN_MAX_ALIGN_BYTES
                        __m128i u8 = _mm_loadu_si128(u8_ptr++);
                        __m128i u8_  = _mm_loadu_si128(u8_ptr++);
                        __m512i i32 = _mm512_cvtepu8_epi32(u8);
                        __m512i i32_ = _mm512_cvtepu8_epi32(u8_);
                        __m512 f32 = _mm512_cvtepi32_ps(i32);
                        __m512 f32_  = _mm512_cvtepi32_ps(i32_);
                        __m512 fma = _mm512_fmadd_ps(f32, scales, offsets); 
                        __m512 fma_  = _mm512_fmadd_ps(f32_, scales, offsets);
                        //_mm512_stream_ps((float*)(fma_ptr++), fma); 
                        //_mm512_stream_ps((float*)(fma_ptr++), fma_); 
                        // //makes no sense. out+16 - out = 64 bytes or 512 bits, same as (fma_ptr++)-fma_ptr.
                        // //fma_ptr++ twice is same bits as out+32-out. But above code is correct and below code is incorrect
                        //_mm512_stream_ps(out, fma);
                        //_mm512_stream_ps(out+16, fma_);
                        //out += 32;
                        _mm512_store_ps(fma_ptr++, fma);
                        _mm512_store_ps(fma_ptr++, fma_);
                        //*fma_ptr++   = fma;
                        //*fma_ptr++   = fma_;
                    }
                } else if (std::is_same<output_t, uint16_t>::value)  {
                    __m256i* u16_ptr = (__m256i*)in;
                    for (int index = 0; index < NM ; index += 32) {
                        __m256i u16  = _mm256_loadu_si256(u16_ptr++);
                        __m256i u16_ = _mm256_loadu_si256(u16_ptr++);
                        __m512i i32  = _mm512_cvtepu16_epi32(u16);
                        __m512i i32_ = _mm512_cvtepu16_epi32(u16_);
                        __m512 f32   = _mm512_cvtepi32_ps(i32);
                        __m512 f32_  = _mm512_cvtepi32_ps(i32_);
                        __m512 fma   = _mm512_fmadd_ps(f32, scales, offsets);
                        __m512 fma_  = _mm512_fmadd_ps(f32_, scales, offsets);
                        _mm512_store_ps(fma_ptr++, fma);
                        _mm512_store_ps(fma_ptr++, fma_); 
                        //*fma_ptr++   = fma; // This is a little faster?
                        //*fma_ptr++   = fma_;
                    }
                } else {
                    throw;
                } 
            #else 
                const __m256 scales = _mm256_set1_ps(self.ncodebooks/self.out_scale);
                const __m256 offsets = _mm256_set1_ps(self.out_offset_sum);
                if (std::is_same<output_t, uint8_t>::value) {
                    for (int index = 0; index < NM ; index += 16) {
                        __m128i lo8 = _mm_loadl_epi64( (const __m128i*)(in + index));
                        __m128i hi8 = _mm_loadl_epi64( (const __m128i*)(in + index + 8));
                        __m256i lo32 = _mm256_cvtepu8_epi32(lo8);
                        __m256i hi32 = _mm256_cvtepu8_epi32(hi8);
                        __m256 lo = _mm256_cvtepi32_ps(lo32);
                        __m256 hi = _mm256_cvtepi32_ps(hi32);
                        __m256 fma_lo = _mm256_fmadd_ps(lo, scales, offsets); 
                        __m256 fma_hi = _mm256_fmadd_ps(hi, scales, offsets); 
                        _mm256_storeu_ps(out + index, fma_lo);
                        _mm256_storeu_ps(out + 8 + index, fma_hi);
                    }
                } else if (std::is_same<output_t, uint16_t>::value)  {
                    __m128i* u16_ptr = (__m128i*)in;
                    __m256* fma_ptr = (__m256*)out;
                    for (int index = 0; index < NM ; index += 16) {
                        __m128i u16  = _mm_loadu_si128(u16_ptr++);
                        __m128i u16_ = _mm_loadu_si128(u16_ptr++);
                        __m256i i32  = _mm256_cvtepu16_epi32(u16);
                        __m256i i32_ = _mm256_cvtepu16_epi32(u16_);
                        __m256 f32   = _mm256_cvtepi32_ps(i32);
                        __m256 f32_  = _mm256_cvtepi32_ps(i32_);
                        __m256 fma   = _mm256_fmadd_ps(f32, scales, offsets);
                        __m256 fma_  = _mm256_fmadd_ps(f32_, scales, offsets);
                        *fma_ptr++   = fma;
                        *fma_ptr++   = fma_;
                    }
                } else {
                    throw;
                }
            #endif

            pybind11::capsule cleanup(out, [](void *f) {
                free(f);
            });
            return pybind11::array_t<float>(
               {self.N, self.M},          // shape
               {sizeof(float), self.N * sizeof(float)}, // stride, col major
               out,   // pointer to data
               cleanup        // garbage collection callback
            );
        }, py::return_value_policy::take_ownership)
        
        // Python Thinks it's getting in row major but Eigen returns in column major by default
        .def("getCentroids", [](mithral_amm<float> &self) { 
            //TODO: return in 3d
            // Why don't these rows/cols need to be flipped to match c, centroids is also ColMatrix.
            // Is the Python not returned in corect format?
            const int rows=self.ncodebooks*16;//k=16
            const int cols=self.D;
            Eigen::Map<Eigen::Matrix<float, -1, -1, Eigen::RowMajor>> mf(const_cast<float*>(self.centroids),rows,cols);
            return mf; 
        })
        .def("getSplitdims", [](mithral_amm<float> &self) {
            const int rows=self.ncodebooks;
            const int cols=4;
            Eigen::Map<Eigen::Matrix<uint32_t, -1,-1, Eigen::RowMajor>> mf(const_cast<uint32_t*>(self.splitdims),rows,cols);
            return mf; 
        })
        .def("getSplitvals", [](mithral_amm<float> &self) {
            // rows/cols flipped since original is ColMatrix
            const int cols=16; //15 pad 1
            //const int rows=self.ncodebooks;  //python
            const int rows=self.ncodebooks*4; // what's in C++, nsplits; why?
            Eigen::Map<Eigen::Matrix<int8_t, -1, -1, Eigen::RowMajor>> mf(const_cast<int8_t*>(self.splitvals),rows,cols);
            return mf; 
        })
        .def("getEncode_scales", [](mithral_amm<float> &self) {
            const int rows=self.ncodebooks;
            const int cols=4;
            Eigen::Map<Eigen::Matrix<scale_t, -1, -1, Eigen::RowMajor>> mf(const_cast<scale_t*>(self.encode_scales),rows,cols);
            return mf; 
        })
        .def("getEncode_offsets", [](mithral_amm<float> &self) {
            const int rows=self.ncodebooks;
            const int cols=4;
            Eigen::Map<Eigen::Matrix<offset_t, -1, -1, Eigen::RowMajor>> mf(const_cast<offset_t*>(self.encode_offsets),rows,cols);
            return mf; 
        })
        .def("getIdxs", [](mithral_amm<float> &self) {
            const int rows=self.nnz_per_centroid;
            const int cols=self.ncodebooks;
            Eigen::Map<Eigen::Matrix<int, -1, -1, Eigen::RowMajor>> mf(const_cast<int*>(self.idxs),rows,cols);
            return mf; 
        })
        //This would convert c++ from being column to row, and be different from task.amm.out_mat
        //.def("getOutput", [](mithral_amm<float> &self) {
        //    const int rows=self.N;
        //    const int cols=self.M;
        //    Eigen::Map<Eigen::Matrix<output_t, -1, -1, Eigen::RowMajor>> mf(const_cast<output_t*>(self.out_mat.data()),rows,cols);
        //    return mf; 
        //})
    
        //setters, Can change pointer to new value; can't overwrite existing. Fine to Copy by value here, only used initally
        // passing references causes segfault when change data on python side. Passing raw errors initally
        // Make a copy from reference?
        .def("setCentroids", [](mithral_amm<float> &self , py::array_t<float, py::array::c_style> mf) {
            //The first 8 nums were wrong once(?!)
            self.centroids =const_cast<const float*>(mf.data()); 
        })
        //WARN: Modification is done on C++; raw copying centroids doesn't work yet. How to project from 3d to 2d/1d need for C++?
        .def("setCentroidsCopyData", [](mithral_amm<float> &self , py::array_t<float, py::array::c_style> mf) {
            //py::array_t<float> *t=new py::array_t<float>(mf);
         
            py::buffer_info buf1 = mf.request();

            // void* ptr = 0; //Wrong, need to be within the function space of rest of pointers
            // std::size_t size_remain = buf1.size ; //should be reference?
            // align(32, buf1.size, ptr, size_remain);
            // const float * centroid_ptr = add_const_t<float*>(static_cast<float*>(ptr));
            // // py::array_t<float, 16> result2 = py::array_t<float>(buf1.size);
            //py::buffer_info buf3 = py::array_t<float>(buf1.size, const_cast<float*>(centroid_ptr)).request();

            /* If no pointer is passed, NumPy will allocate the buffer */
            auto sz2 = buf1.size*sizeof(float);
            float * centroid_ptr = static_cast<float*>(aligned_alloc(32, sz2)); 
            assert(reinterpret_cast<uintptr_t>(centroid_ptr)%32 ==0);
            //float *  = (ptr);
            //Creates an object, but instead of using the new object we make request to get buffer info of the new object.
            //py::buffer_info buf3 = py::array_t<float>(buf1.size, const_cast<float*>(centroid_ptr)).request(); //which constructor is a 
            //assert(buf3.ptr == centroid_ptr); //fails since py::array_t creates a new object

            float *ptr1 = static_cast<float *>(buf1.ptr);
            for (size_t idx = 0; idx < buf1.size; idx++) {
                centroid_ptr[idx] = ptr1[idx]; //can I just assign the data like this?
            }
            //delete[] self.centroids; 
            //free(const_cast<float*>(self.centroids));  //free'ing here errors python the first time; but self.centroids isn't python memory yet?
            //is centroids supposed to be in column order? Doubtfull since c++ pointer
            self.centroids=const_cast<const float*>(centroid_ptr);
        })
        .def("setSplitdims", [](mithral_amm<float> &self , py::array_t<uint32_t, py::array::c_style>& mf) {
            //py::array_t<uint32_t> t=py::array_t<uint32_t>(mf);
            //delete self.splitdims; //segfaults when run with delete or delete[], but maybe not. Unsure when does or doesn't
            self.splitdims =const_cast<const uint32_t*>(mf.data());
        })
        .def("setSplitvals", [](mithral_amm<float> &self , py::array_t<int8_t, py::array::c_style>& mf) {
            //py::array_t<int8_t> t=py::array_t<int8_t>(mf);
            //delete self.splitvals;
            self.splitvals =const_cast<const int8_t*>(mf.data());
        })
        .def("setEncode_scales", [](mithral_amm<float> &self , py::array_t<scale_t, py::array::c_style>& mf) {
            //py::array_t<scale_t> t=py::array_t<scale_t>(mf);
            //delete self.encode_scales;
            self.encode_scales =const_cast<const scale_t*>(mf.data());
        })
        .def("setEncode_offsets", [](mithral_amm<float> &self , py::array_t<offset_t, py::array::c_style>& mf) {
            //py::array_t<offset_t> t=py::array_t<offset_t>(mf);
            //delete self.encode_offsets;
            self.encode_offsets =const_cast<const offset_t*>(mf.data());
        })
        .def("setIdxs", [](mithral_amm<float> &self , py::array_t<int, py::array::c_style>& mf) {
            //py::array_t<int> t=py::array_t<int>(mf);
            //delete self.idxs;
            self.idxs = const_cast<const int*>(mf.data()); 
        })
        
        .def("setSplitdimsCopyData", [](mithral_amm<float> &self , py::array_t<uint32_t, py::array::c_style>& mf) {
            py::buffer_info buf1 = mf.request();
            auto sz2 = buf1.size*sizeof(uint32_t);
            auto * _ptr = static_cast<uint32_t*>(aligned_alloc(32, sz2)); 
            auto *ptr1 = static_cast<uint32_t *>(buf1.ptr);
            for (size_t idx = 0; idx < buf1.size; idx++) {
                _ptr[idx] = ptr1[idx]; 
            }
            self.splitdims=const_cast<const uint32_t*>(_ptr);
        })
        .def("setSplitvalsCopyData", [](mithral_amm<float> &self , py::array_t<int8_t, py::array::c_style>& mf) {
            py::buffer_info buf1 = mf.request();
            auto sz2 = buf1.size*sizeof(int8_t);
            auto * _ptr = static_cast<int8_t*>(aligned_alloc(32, sz2)); 
            auto *ptr1 = static_cast<int8_t *>(buf1.ptr);
            for (size_t idx = 0; idx < buf1.size; idx++) {
                _ptr[idx] = ptr1[idx]; 
            }
            self.splitvals=const_cast<const int8_t*>(_ptr);
        })
        .def("setEncode_scalesCopyData", [](mithral_amm<float> &self , py::array_t<scale_t, py::array::c_style>& mf) {
            py::buffer_info buf1 = mf.request();
            auto sz2 = buf1.size*sizeof(scale_t);
            auto * _ptr = static_cast<scale_t*>(aligned_alloc(32, sz2)); 
            auto *ptr1 = static_cast<scale_t *>(buf1.ptr);
            for (size_t idx = 0; idx < buf1.size; idx++) {
                _ptr[idx] = ptr1[idx]; 
            }
            self.encode_scales=const_cast<const scale_t*>(_ptr);
        })
        .def("setEncode_offsetsCopyData", [](mithral_amm<float> &self , py::array_t<offset_t, py::array::c_style>& mf) {
            py::buffer_info buf1 = mf.request();
            auto sz2 = buf1.size*sizeof(offset_t);
            auto * _ptr = static_cast<offset_t*>(aligned_alloc(32, sz2)); 
            auto *ptr1 = static_cast<offset_t *>(buf1.ptr);
            for (size_t idx = 0; idx < buf1.size; idx++) {
                _ptr[idx] = ptr1[idx]; 
            }
            self.encode_offsets=const_cast<const offset_t*>(_ptr);
        })

        //// // Doesn't work to prevent segfaults. Is it cause I'm not copying over the right data?
        //// Since these are pointers to const data can't overwrite existing, have to point to entierly new object causing memleak
        // .def("setSplitdimsCopyData", [](mithral_amm<float> &self , py::array_t<uint32_t, py::array::c_style>& mf) {
        //    // delete [] self.splitdims; //but freeing here causes segfault?
        //     py::buffer_info buf1 = mf.request();
        //     auto result = py::array_t<uint32_t>(buf1.size);
        //     py::buffer_info buf3 = result.request();
        //     uint32_t *ptr1 = static_cast<uint32_t *>(buf1.ptr);
        //     uint32_t *ptr3 = static_cast<uint32_t *>(buf3.ptr);
        //     for (size_t idx = 0; idx < buf1.shape[0]; idx++)
        //         ptr3[idx] = ptr1[idx];
        //     self.splitdims=ptr3;
        // })
        // .def("setSplitvalsCopyData", [](mithral_amm<float> &self , py::array_t<int8_t, py::array::c_style>& mf) {
        //     py::buffer_info buf1 = mf.request();
        //     auto result = py::array_t<int8_t>(buf1.size);
        //     py::buffer_info buf3 = result.request();
        //     int8_t *ptr1 = static_cast<int8_t *>(buf1.ptr);
        //     int8_t *ptr3 = static_cast<int8_t *>(buf3.ptr);
        //     for (size_t idx = 0; idx < buf1.shape[0]; idx++)
        //         ptr3[idx] = ptr1[idx];
        //     self.splitvals=ptr3;
        // })
        // .def("setEncode_scalesCopyData", [](mithral_amm<float> &self , py::array_t<scale_t, py::array::c_style>& mf) {
        //     py::buffer_info buf1 = mf.request();
        //     auto result = py::array_t<scale_t>(buf1.size);
        //     py::buffer_info buf3 = result.request();
        //     scale_t *ptr1 = static_cast<scale_t *>(buf1.ptr);
        //     scale_t *ptr3 = static_cast<scale_t *>(buf3.ptr);
        //     for (size_t idx = 0; idx < buf1.shape[0]; idx++)
        //         ptr3[idx] = ptr1[idx];
        //     self.encode_scales=ptr3;
        // })
        // .def("setEncode_offsetsCopyData", [](mithral_amm<float> &self , py::array_t<offset_t, py::array::c_style>& mf) {
        //     py::buffer_info buf1 = mf.request();
        //     auto result = py::array_t<offset_t>(buf1.size);
        //     py::buffer_info buf3 = result.request();
        //     offset_t *ptr1 = static_cast<offset_t *>(buf1.ptr);
        //     offset_t *ptr3 = static_cast<offset_t *>(buf3.ptr);
        //     for (size_t idx = 0; idx < buf1.shape[0]; idx++)
        //         ptr3[idx] = ptr1[idx];
        //     self.encode_offsets=ptr3;
        // })
        // .def("setIdxsCopyData", [](mithral_amm<float> &self , py::array_t<int, py::array::c_style>& mf) {
        //     py::buffer_info buf1 = mf.request();
        //     auto result = py::array_t<int>(buf1.size);
        //     py::buffer_info buf3 = result.request();
        //     int *ptr1 = static_cast<int *>(buf1.ptr);
        //     int *ptr3 = static_cast<int *>(buf3.ptr);
        //     for (size_t idx = 0; idx < buf1.shape[0]; idx++)
        //         ptr3[idx] = ptr1[idx];
        //     self.idxs=ptr3;
        // })
        
        // storage for intermediate values
        .def_readwrite("tmp_codes"          , &mithral_amm<float>::tmp_codes) 
        .def_readwrite("codes"              , &mithral_amm<float>::codes) //shape: (N/B, C) where B blocks are zipped into each col. zip_bolt_colmajor from tmp_codes: "we go from storing 4-bit codes as u8 values in column-major order to storing pairs of 4-bit codes in a blocked column-major layout" per https://github.com/dblalock/bolt/issues/20
        .def_readwrite("tmp_luts_f32"       , &mithral_amm<float>::tmp_luts_f32)
        .def_readwrite("luts"               , &mithral_amm<float>::luts)
        // outputs
        .def_readwrite("out_offset_sum"      , &mithral_amm<float>::out_offset_sum)
        .def_readwrite("out_scale"           , &mithral_amm<float>::out_scale)
        .def_readwrite("out_mat"             , &mithral_amm<float>::out_mat) // read only
        .def("getOutUint8", [](mithral_amm<float> &self) { 
            //Mithral by default writes uint8; not 16int we expect
            //Eigen::Map<Eigen::Matrix<uint16_t,-1,-1, Eigen::RowMajor>> mf(const_cast<uint8_t*>(self.out_mat.data()), self.out_mat.rows()/2, self.out_mat.cols());
            
            Eigen::Map<Eigen::Matrix<uint8_t,-1,-1,Eigen::ColMajor>> modified_rows(reinterpret_cast<uint8_t*>(self.out_mat.data()), self.out_mat.rows(), self.out_mat.cols());
            //casts to int16? Or just smushes up again?
            //Eigen::Map<Eigen::Matrix<uint16_t,-1,-1,Eigen::ColMajor>> mf(reinterpret_cast<uint16_t*>(modified_rows.data()), self.out_mat.rows(), self.out_mat.cols());
            
            Eigen::Matrix<uint16_t,-1,-1,Eigen::ColMajor> mf = modified_rows.cast<uint16_t>();
            return mf; 
        })
        ;

    ////Eigen type so can return real matrix
    //   .def_readonly("data"     , &ColMatrix<float>::output) // whole amm object?
    //   .def_readonly("size"     , &ColMatrix<float>::output) // whole amm object?





    // ---TEMP---- 
    m.def("printNameMatmul", &printNameMatmul, "print matmul");
    m.def("printNameTest", &printNameTest, "printTest");

    // Hacking. Want to pass a python string to a struct which stores it in a const char * and returns a string python can read 
    struct clark { 
        clark(const char* c) : c{c}, s{c} {}; // w/ default constructor the std::string methods don't work
        const char* getName() const { return c; }
        const std::string getNameString() const { 
            // std::string cast_c(c);
            cout << "const char* " << c << endl;
            cout << "std::string " << s << endl;
            return s;
            //return std::string (c); //Python can't decode this return
        }
        const char *c;
        std::string s;
    } ;

    py::class_<clark>(m, "clark")
       .def(py::init([](char *c){
           //WORKS: makes a copy of pystring inside pybind. 
           //Causes a memory leak of tmp_filename
           const size_t len = strlen(c);
           char * tmp_filename = new char[len + 1];
           strncpy(tmp_filename, c, len);
           tmp_filename[len] = '\0';  //nessisary line(why ?!?)
           const char* c2 = tmp_filename;
           return clark{c2};
       }))
        .def("getName", &clark::getName)
        .def("getNameString", &clark::getNameString)
       .def_readwrite("c", &clark::c) ;
    
    py::class_<Test>(m, "Test")
       .def(py::init([](int N, int D, int M, char *c){
           const size_t len = strlen(c);
           char * tmp_filename = new char[len + 1];
           strncpy(tmp_filename, c, len);
           tmp_filename[len] = '\0';  //nessisary line(why ?!?)
           const char* c2 = tmp_filename;
           return Test{N,D,M,c2};
       }))
       .def_readwrite("name", &Test::name) 
       .def_readonly("N", &Test::N) 
       .def_readonly("D", &Test::D) 
       .def_readonly("M", &Test::M) 
        .def("__repr__",
        [](const Test &a) {
            std::cout << "PPPprinting:    " << a.name << std::endl; 
            std::string name(a.name);
            std::cout << std::string("from C++: <Test: name: " + name + " sizes: " + std::to_string(a.N) + " ," + std::to_string(a.D) + ", " + std::to_string(a.M) + ">") << std::endl;
            return std::string("<Test: name: " + name + " sizes: " + std::to_string(a.N) + " ," + std::to_string(a.D) + ", " + std::to_string(a.M) + ">");
        }
        );

    // can print out fine
    m.def("utf8_test",
    [](const std::string s) {
        const char * c = s.c_str();
        cout << "utf-8 is icing on the cake.\n";
        cout << c << "\n";
        return c;
    }
    );
    m.def("utf8_charptr",
        [](const char *s) {
            cout << "My favorite food is\n";
            cout << s << "\n";
            return s; //Python can print returned char* 
        }
    );
    
}