##########################################
# File: CMakeLists.txt                   #
# Copyright Richard Stebbing 2014.       #
# Distributed under the MIT License.     #
# (See accompany file LICENSE or copy at #
#  http://opensource.org/licenses/MIT)   #
##########################################
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.0)

PROJECT(DOO-SABIN)

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

MACRO(EXPECT_FILES VARIABLE)
  FOREACH(EXPECTED_FILE ${ARGN})
    IF (NOT EXISTS ${${VARIABLE}}/${EXPECTED_FILE})
      MESSAGE(FATAL_ERROR
        "Caller defined ${VARIABLE}: ${${VARIABLE}} does not contain "
        "${EXPECTED_FILE}.")
    ENDIF (NOT EXISTS ${${VARIABLE}}/${EXPECTED_FILE})
  ENDFOREACH()
ENDMACRO(EXPECT_FILES)

SET(EIGEN_INCLUDE_DIR "" CACHE PATH "")
EXPECT_FILES(EIGEN_INCLUDE_DIR Eigen/Dense)
INCLUDE_DIRECTORIES(${EIGEN_INCLUDE_DIR})

SET(COMMON_CPP_INCLUDE_DIR "" CACHE PATH "")
EXPECT_FILES(COMMON_CPP_INCLUDE_DIR Mesh/general_mesh.h
                                    Math/linalg.h
                                    Math/modulo.h)
INCLUDE_DIRECTORIES(${COMMON_CPP_INCLUDE_DIR})

ENABLE_TESTING()
OPTION(BUILD_TESTING "Enable tests" ON)

SET(DOOSABIN_INCLUDE_DIR include)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/${DOOSABIN_INCLUDE_DIR})
SET(DOOSABIN_HDRS ${DOOSABIN_INCLUDE_DIR}/doosabin.h
                  ${DOOSABIN_INCLUDE_DIR}/uniform_quadratic_bspline.h)

IF (BUILD_TESTING)
  ADD_EXECUTABLE(doosabin_test doosabin_test.cpp ${DOOSABIN_HDRS})
  ADD_TEST(NAME doosabin_test
           COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/doosabin_test)
ENDIF (BUILD_TESTING)
