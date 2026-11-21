cmake_minimum_required(VERSION 3.16)
project(lxr_oscillator_host_spike C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(LXR_SOURCE_DIR "" CACHE PATH "Path to the pinned lxr-desktop-app checkout")
set(LXR_EXPECTED_REVISION "dca0a44413931a12de65941355f06c07598a9be5" CACHE STRING "Required upstream commit")
option(LXR_SPIKE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(LXR_SPIKE_INTERPOLATE_OSC "Enable oscillator interpolation" ON)
option(LXR_SPIKE_INTERPOLATE_FM_OSC "Enable FM interpolation" ON)

if(NOT LXR_SOURCE_DIR)
  message(FATAL_ERROR "Set -DLXR_SOURCE_DIR=/path/to/lxr-desktop-app")
endif()

if(NOT EXISTS "${LXR_SOURCE_DIR}/.git")
  message(FATAL_ERROR "LXR_SOURCE_DIR must be a Git checkout: ${LXR_SOURCE_DIR}")
endif()

execute_process(
  COMMAND git -C "${LXR_SOURCE_DIR}" rev-parse HEAD
  OUTPUT_VARIABLE LXR_ACTUAL_REVISION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE LXR_GIT_RESULT
)
if(NOT LXR_GIT_RESULT EQUAL 0)
  message(FATAL_ERROR "Could not read Git revision from LXR_SOURCE_DIR")
endif()
if(NOT LXR_ACTUAL_REVISION STREQUAL LXR_EXPECTED_REVISION)
  message(FATAL_ERROR
    "Wrong upstream revision. Expected ${LXR_EXPECTED_REVISION}, got ${LXR_ACTUAL_REVISION}")
endif()

set(LXR_CORE "${LXR_SOURCE_DIR}/Core")
set(LXR_OSC_SRC "${LXR_CORE}/Src/Soundengine/DSPAudio/Oscillator.c")
set(LXR_TABLE_SRC "${LXR_CORE}/Src/Soundengine/DSPAudio/wavetable.c")
foreach(required_file IN ITEMS "${LXR_OSC_SRC}" "${LXR_TABLE_SRC}")
  if(NOT EXISTS "${required_file}")
    message(FATAL_ERROR "Missing upstream source file: ${required_file}")
  endif()
endforeach()

add_executable(oscillator_smoke
  "${LXR_OSC_SRC}"
  "${LXR_TABLE_SRC}"
  host_support.c
  oscillator_smoke.c
)

# The shim directory intentionally precedes upstream Core/Inc so only the
# platform/application boundary headers are replaced. DSP implementation and
# wavetable data are compiled directly from the pinned upstream checkout.
target_include_directories(oscillator_smoke BEFORE PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/shims"
  "${LXR_CORE}/Inc"
)
target_compile_definitions(oscillator_smoke PRIVATE
  INTERPOLATE_OSC=$<BOOL:${LXR_SPIKE_INTERPOLATE_OSC}>
  INTERPOLATE_FM_OSC=$<BOOL:${LXR_SPIKE_INTERPOLATE_FM_OSC}>
)
if(MSVC)
  target_compile_options(oscillator_smoke PRIVATE /W4)
else()
  target_compile_options(oscillator_smoke PRIVATE -Wall -Wextra -Wno-unused-parameter)
  target_link_libraries(oscillator_smoke PRIVATE m)
  if(LXR_SPIKE_SANITIZERS)
    target_compile_options(oscillator_smoke PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(oscillator_smoke PRIVATE -fsanitize=address,undefined)
  endif()
endif()

enable_testing()
add_test(NAME oscillator_smoke COMMAND oscillator_smoke)
