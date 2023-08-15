/****************************************************************************
  FileName     [ textFormat.hpp ]
  PackageName  [ util ]
  Synopsis     [ Declare text formatting functions such as colors, bold, etc. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <fmt/color.h>
#include <fmt/core.h>

#include <filesystem>
#include <functional>
#include <string>

#include "util/terminalAttributes.hpp"

namespace dvlab_utils {

namespace fmt_ext {

template <typename T>
auto styled_if_ANSI_supported(FILE* f, T const& value, fmt::text_style ts) -> fmt::detail::styled_arg<std::remove_cvref_t<T>> {
    return fmt::styled(value, ANSI_supported(f) ? ts : fmt::text_style{});
}

template <typename T>
auto styled_if_ANSI_supported(T const& value, fmt::text_style ts) -> fmt::detail::styled_arg<std::remove_cvref_t<T>> {
    return styled_if_ANSI_supported(stdout, value, ts);
}

fmt::text_style ls_color(std::filesystem::path const& path);

}  // namespace fmt_ext

template <typename F>
size_t ansi_token_size(F const& F_) {
    return F_("").size();
}

}  // namespace dvlab_utils
