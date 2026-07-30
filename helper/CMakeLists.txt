cmake_minimum_required(VERSION 3.16)
project(sc_cef_helper LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SC_FP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../patches")
if(NOT DEFINED CEF_ROOT)
  set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/cef" CACHE PATH "CEF binary distribution")
endif()

set(SC_COMMON_SOURCES
  src/main.cc
  src/control_server.cc
  src/fingerprint_config.cc
  src/json_util.cc
  src/browser_manager.cc
  src/fingerprint_runtime.cc
  ${SC_FP_ROOT}/M1_navigator_network/src/sc_fp_engine.cc
  ${SC_FP_ROOT}/M2_canvas_webgl_audio/src/sc_fp_render.cc
  ${SC_FP_ROOT}/M3_fonts_webrtc/src/sc_fp_media.cc
)

set(SC_INCLUDE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${SC_FP_ROOT}/M1_navigator_network/src
  ${SC_FP_ROOT}/M2_canvas_webgl_audio/src
  ${SC_FP_ROOT}/M3_fonts_webrtc/src
)

option(SC_FORCE_STUB "Force stub build even if CEF is present" OFF)

if(NOT SC_FORCE_STUB AND EXISTS "${CEF_ROOT}/include/cef_app.h" AND EXISTS "${CEF_ROOT}/cmake/FindCEF.cmake")
  message(STATUS "CEF detected at ${CEF_ROOT}")
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
  find_package(CEF REQUIRED)
  add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

  set(CEF_TARGET "sc-cef-helper")
  set(CEF_HELPER_TARGET "sc-cef-helper_Helper")
  set(CEF_HELPER_OUTPUT_NAME "sc-cef-helper Helper")

  if(OS_MAC)
    SET_CEF_TARGET_OUT_DIR()

    # --- Main app bundle ---
    add_executable(${CEF_TARGET} MACOSX_BUNDLE ${SC_COMMON_SOURCES}
      src/browser_manager_host_cef.cc
      src/cdp_emulation.cc
      src/mac_window_host.mm)
    SET_EXECUTABLE_TARGET_PROPERTIES(${CEF_TARGET})
    add_dependencies(${CEF_TARGET} libcef_dll_wrapper)
    target_include_directories(${CEF_TARGET} PRIVATE ${SC_INCLUDE} ${CEF_ROOT})
    target_link_libraries(${CEF_TARGET} PRIVATE libcef_dll_wrapper ${CEF_STANDARD_LIBS}
      "-framework AppKit" "-framework Cocoa")

    set(EXECUTABLE_NAME "${CEF_TARGET}")
    set(PRODUCT_NAME "${CEF_TARGET}")
    set(CEF_APP "${CEF_TARGET_OUT_DIR}/${CEF_TARGET}.app")

    set_target_properties(${CEF_TARGET} PROPERTIES
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/mac/Info.plist.in"
      OUTPUT_NAME "${CEF_TARGET}"
    )

    COPY_MAC_FRAMEWORK("${CEF_TARGET}" "${CEF_BINARY_DIR}" "${CEF_APP}")

    # --- Helper app bundles (Renderer/GPU/…) ---
    foreach(_suffix_list ${CEF_HELPER_APP_SUFFIXES})
      string(REPLACE ":" ";" _suffix_list "${_suffix_list}")
      list(GET _suffix_list 0 _name_suffix)
      list(GET _suffix_list 1 _target_suffix)
      list(GET _suffix_list 2 _plist_suffix)

      set(_helper_target "${CEF_HELPER_TARGET}${_target_suffix}")
      set(_helper_output_name "${CEF_HELPER_OUTPUT_NAME}${_name_suffix}")

      set(_helper_info_plist "${CMAKE_CURRENT_BINARY_DIR}/helper-Info${_target_suffix}.plist")
      file(READ "${CMAKE_CURRENT_SOURCE_DIR}/mac/helper-Info.plist.in" _plist_contents)
      string(REPLACE "\${EXECUTABLE_NAME}" "${_helper_output_name}" _plist_contents "${_plist_contents}")
      string(REPLACE "\${PRODUCT_NAME}" "${_helper_output_name}" _plist_contents "${_plist_contents}")
      string(REPLACE "\${BUNDLE_ID_SUFFIX}" "${_plist_suffix}" _plist_contents "${_plist_contents}")
      file(WRITE "${_helper_info_plist}" "${_plist_contents}")

      add_executable(${_helper_target} MACOSX_BUNDLE src/process_helper_mac.cc)
      SET_EXECUTABLE_TARGET_PROPERTIES(${_helper_target})
      add_dependencies(${_helper_target} libcef_dll_wrapper)
      target_link_libraries(${_helper_target} PRIVATE libcef_dll_wrapper ${CEF_STANDARD_LIBS})
      set_target_properties(${_helper_target} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${_helper_info_plist}"
        OUTPUT_NAME "${_helper_output_name}"
      )
      add_dependencies(${CEF_TARGET} "${_helper_target}")

      add_custom_command(
        TARGET ${CEF_TARGET}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CEF_TARGET_OUT_DIR}/${_helper_output_name}.app"
                "${CEF_APP}/Contents/Frameworks/${_helper_output_name}.app"
        VERBATIM
      )
    endforeach()

    # Convenience symlink for scripts that look for helper/build/sc-cef-helper
    add_custom_command(
      TARGET ${CEF_TARGET}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E create_symlink
              "${CEF_APP}/Contents/MacOS/${CEF_TARGET}"
              "${CMAKE_BINARY_DIR}/${CEF_TARGET}-bin"
      COMMENT "Symlink MacOS binary for scripts"
    )
  else()
    add_executable(${CEF_TARGET} ${SC_COMMON_SOURCES} src/browser_manager_host_cef.cc src/cdp_emulation.cc)
    SET_EXECUTABLE_TARGET_PROPERTIES(${CEF_TARGET})
    add_dependencies(${CEF_TARGET} libcef_dll_wrapper)
    target_include_directories(${CEF_TARGET} PRIVATE ${SC_INCLUDE} ${CEF_ROOT})
    ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
    target_link_libraries(${CEF_TARGET} PRIVATE libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBS})
    set_target_properties(${CEF_TARGET} PROPERTIES OUTPUT_NAME "sc-cef-helper")
  endif()
else()
  message(WARNING "Building STUB helper")
  add_executable(sc-cef-helper ${SC_COMMON_SOURCES} src/browser_manager_host_stub.cc)
  target_compile_definitions(sc-cef-helper PRIVATE SC_CEF_STUB_BUILD=1)
  target_include_directories(sc-cef-helper PRIVATE ${SC_INCLUDE})
  set_target_properties(sc-cef-helper PROPERTIES OUTPUT_NAME "sc-cef-helper")
endif()
