option(SEED_ENABLE_SANITIZERS "Enable ASan/UBSan" OFF)

function(seed_apply_sanitizers target)
  if(NOT SEED_ENABLE_SANITIZERS)
    return()
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(${target} PRIVATE
      -fsanitize=address,undefined
      -fno-omit-frame-pointer
    )
    target_link_options(${target} PRIVATE
      -fsanitize=address,undefined
    )
    message(STATUS "Sanitizers enabled (GCC/Clang): ${target}")

  elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # MSVC AddressSanitizer (requires VS 2019 16.9+ or VS 2022+)
    target_compile_options(${target} PRIVATE /fsanitize=address)
    # /fsanitize=address implies /INCREMENTAL:NO for the linker
    set_target_properties(${target} PROPERTIES
      LINK_FLAGS "/INCREMENTAL:NO"
    )
    message(STATUS "AddressSanitizer enabled (MSVC): ${target}")

    # Note: MSVC ASan requires the ASan runtime DLLs to be in PATH at runtime.
    # They are typically found under:
    #   C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\<version>\bin\HostX64\x64
    # CMake/VS usually copies them automatically for Debug builds.
  else()
    message(WARNING "Sanitizers not supported for compiler: ${CMAKE_CXX_COMPILER_ID}")
  endif()
endfunction()
