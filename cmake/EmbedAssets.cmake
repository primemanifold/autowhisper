function(autowhisper_embed_asset)
    set(oneValueArgs SOURCE OUTPUT VAR_NAME)
    cmake_parse_arguments(A "" "${oneValueArgs}" "" ${ARGN})

    file(READ "${A_SOURCE}" content)

    set(header "#pragma once\n#include <string_view>\nnamespace autowhisper::settings::assets {\ninline constexpr std::string_view ${A_VAR_NAME} = R\"AW_ASSET(${content})AW_ASSET\";\n}  // namespace\n")

    file(WRITE "${A_OUTPUT}" "${header}")
endfunction()

function(autowhisper_embed_web_assets OUT_DIR)
    file(MAKE_DIRECTORY "${OUT_DIR}")
    autowhisper_embed_asset(
        SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/settings/web/index.html
        OUTPUT ${OUT_DIR}/index_html.h
        VAR_NAME kIndexHtml)
    autowhisper_embed_asset(
        SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/settings/web/style.css
        OUTPUT ${OUT_DIR}/style_css.h
        VAR_NAME kStyleCss)
    autowhisper_embed_asset(
        SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/settings/web/app.js
        OUTPUT ${OUT_DIR}/app_js.h
        VAR_NAME kAppJs)

    file(WRITE "${OUT_DIR}/assets.h"
         "#pragma once\n#include \"index_html.h\"\n#include \"style_css.h\"\n#include \"app_js.h\"\n")

    # The embed above runs at configure time only. Re-run cmake when any web
    # asset changes so a plain `cmake --build` never ships a stale UI.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/settings/web/index.html
        ${CMAKE_CURRENT_SOURCE_DIR}/src/settings/web/style.css
        ${CMAKE_CURRENT_SOURCE_DIR}/src/settings/web/app.js)
endfunction()
