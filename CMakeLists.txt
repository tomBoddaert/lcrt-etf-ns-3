include(FetchContent)

FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    # v0.6 will be updated to point to the latest patch version.
    # Use v0.6.<patch_version> or the commit hash to prevent such auto updates.
    GIT_TAG v0.6.1
)
# Set any global configuration variables such as `Rust_TOOLCHAIN` before this line!
FetchContent_MakeAvailable(Corrosion)

corrosion_import_crate(
    MANIFEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lcrt-etf/Cargo.toml
    CRATES lcrt_c
)

check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/lcrt-examples-test-suite.cc
        )
endif()

build_lib(
    LIBNAME lcrt
    SOURCE_FILES model/lcrt.cc
                 model/lcrt-header.cc
                 helper/lcrt-helper.cc
                 model/etf.cc
                 helper/etf-helper.cc
    HEADER_FILES lcrt-etf/lcrt_c/include/liblcrt.hpp
                 model/lcrt.h
                 model/lcrt-header.h
                 helper/lcrt-helper.h
                 model/etf.h
                 helper/etf-helper.h
    LIBRARIES_TO_LINK ${libcore}
                      ${libinternet}
                      ${libmobility}
                      lcrt_c-static
    TEST_SOURCES test/lcrt-test-suite.cc
                 ${examples_as_tests_sources}
)
