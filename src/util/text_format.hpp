/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Declare text formatting-related functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <fmt/color.h>
#include <fmt/core.h>

#include <filesystem>
#include <functional>
#include <string>

#include "util/terminal_attributes.hpp"

namespace dvlab {

namespace fmt_ext {

template <typename T>
auto styled_if_ansi_supported(FILE* f, T const& value, fmt::text_style ts) -> fmt::detail::styled_arg<std::remove_cvref_t<T>> {
    return fmt::styled(value, utils::ansi_supported(f) ? ts : fmt::text_style{});
}

template <typename T>
auto styled_if_ansi_supported(T const& value, fmt::text_style ts) -> fmt::detail::styled_arg<std::remove_cvref_t<T>> {
    return styled_if_ansi_supported(stdout, value, ts);
}

fmt::text_style ls_color(std::filesystem::path const& path);

}  // namespace fmt_ext

}  // namespace dvlab