add_subdirectory(Interfaces)
add_subdirectory(Traits)
add_subdirectory(Dialect)
add_subdirectory(Support)
add_subdirectory(Conversion)
add_subdirectory(Backend)
add_subdirectory(Builder)
add_subdirectory(CAPI)

add_mlir_library(TPUMLIRInitAll
  InitAll.cpp

  LINK_COMPONENTS
  Core

  LINK_LIBS PUBLIC
  MLIRIR
  MLIRSupport
  MLIRFuncDialect
  MLIRQuantDialect
  MLIRTosaDialect
  MLIRInferenceInterface
  TPUMLIRTopToTpu
  TPUMLIRTopToTosa
)
