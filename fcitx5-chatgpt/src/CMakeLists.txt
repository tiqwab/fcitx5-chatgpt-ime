# for nlohmann_json
set(JSON_Install OFF CACHE INTERNAL "")
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/nlohmann_json ${CMAKE_CURRENT_BINARY_DIR}/nlohmann_json)

add_library(fcitx5-chatgpt SHARED engine.cpp hiraganatable.cpp imeclient.cpp)
target_compile_features(fcitx5-chatgpt PUBLIC cxx_std_20)
target_link_libraries(fcitx5-chatgpt PRIVATE Fcitx5::Core)
set_target_properties(fcitx5-chatgpt PROPERTIES PREFIX "")
install(TARGETS fcitx5-chatgpt DESTINATION "${FCITX_INSTALL_LIBDIR}/fcitx5")

# Show the C++ standard version for the target
get_property(CPP_STANDARD TARGET fcitx5-chatgpt PROPERTY CXX_STANDARD)
message(STATUS "C++ standard version for fcitx5-chatgpt: ${CPP_STANDARD}")

# Addon config file
# We need additional layer of conversion because we want PROJECT_VERSION in it.
configure_file(chatgpt-addon.conf.in.in chatgpt-addon.conf.in)
fcitx5_translate_desktop_file("${CMAKE_CURRENT_BINARY_DIR}/chatgpt-addon.conf.in" chatgpt-addon.conf)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/chatgpt-addon.conf" RENAME chatgpt.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")

# Input Method registration file
fcitx5_translate_desktop_file(chatgpt.conf.in chatgpt.conf)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/chatgpt.conf" DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod")
