cmake_minimum_required(VERSION 3.18)
project(imsrg-paired-benchmark LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 14)
set(BLA_VENDOR OpenBLAS)
find_package(MPI REQUIRED COMPONENTS CXX)
find_package(OpenMP REQUIRED)
find_package(GSL REQUIRED)
find_package(BLAS REQUIRED)
find_package(Boost REQUIRED)
get_filename_component(IMSRG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
add_executable(paired-kernels paired.cc)
target_compile_definitions(paired-kernels PRIVATE IMSRG_USE_MPI)
target_compile_options(paired-kernels PRIVATE -frounding-math)
target_include_directories(paired-kernels PRIVATE
 "${IMSRG_ROOT}/src" "${IMSRG_ROOT}/src/extern/armadillo"
 "${IMSRG_ROOT}/src/extern/half/include" "${IMSRG_ROOT}/src/Commutator"
 "${IMSRG_ROOT}/src/ThreeBodyME" "${IMSRG_ROOT}/src/ReferenceImplementations"
 "${IMSRG_ROOT}/src/imsrg_util" ${Boost_INCLUDE_DIRS})
target_link_libraries(paired-kernels PRIVATE "${IMSRG_LIBRARY_DIR}/libIMSRG.so" MPI::MPI_CXX OpenMP::OpenMP_CXX GSL::gsl BLAS::BLAS)
