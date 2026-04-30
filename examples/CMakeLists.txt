build_lib_example(
  NAME lcrt-example
  SOURCE_FILES lcrt-example.cc
  LIBRARIES_TO_LINK
      ${liblcrt}
      ${libwifi}
      ${libinternet}
      ${libapplications}
      ${libnetanim}
)

# build_lib_example(
#   NAME lcrt-basic
#   SOURCE_FILES basic.cc
#   LIBRARIES_TO_LINK
#     ${liblcrt}
#     ${libwifi}
#     ${libinternet}
#     ${libapplications}
# )

build_lib_example(
  NAME lcrt-benchmark
  SOURCE_FILES benchmark.cc
  LIBRARIES_TO_LINK
    ${liblcrt}
    ${libwifi}
    ${libinternet}
    ${libapplications}
    ${libflow-monitor}
    ${libnetanim}
    ${libolsr}
)
