set(EDN_GIT_REF "v0.0.1") # update when tagging releases

vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL https://github.com/cgrinker/edn.git
    REF ${EDN_GIT_REF}
)

vcpkg_cmake_configure(
    SOURCE_PATH ${SOURCE_PATH}
    OPTIONS -DEDN_BUILD_TESTS=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/edn)

file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/edn RENAME copyright)

# Usage file
file(WRITE ${CURRENT_PACKAGES_DIR}/share/edn/usage
"Header-only edn library\n\nCMake example:\n  find_package(edn CONFIG REQUIRED)\n  target_link_libraries(<tgt> PRIVATE edn::edn)\n")
