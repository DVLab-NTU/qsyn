/****************************************************************************
  FileName     [ tempFiles.h ]
  PackageName  [ util ]
  Synopsis     [ RAII wrapper for temporary files and directories ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef DVLAB_TEMP_FILES_H
#define DVLAB_TEMP_FILES_H

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace dvlab_utils {

namespace detail {

std::filesystem::path createTempDir(std::string_view prefix);
std::filesystem::path createTempFile(std::string_view prefix);

}  // namespace detail

class TempDir {
public:
    TempDir() : _dir{detail::createTempDir(std::filesystem::temp_directory_path().string() + "/dvlab-")} {}
    TempDir(std::string_view prefix) : _dir{detail::createTempDir(prefix)} {}
    ~TempDir() { std::filesystem::remove_all(_dir); }

    std::filesystem::path path() const { return _dir; }

private:
    std::filesystem::path const _dir;
};

class TempFile {
public:
    TempFile() : _stream{detail::createTempFile(std::filesystem::temp_directory_path().string())} {
        assert(_stream.is_open());
    }
    TempFile(std::string_view prefix) : _stream{detail::createTempFile(prefix)} {
        assert(_stream.is_open());
    }

    std::fstream& stream() { return _stream; }

private:
    std::fstream _stream;
};

}  // namespace dvlab_utils

#endif  // DVLAB_TEMP_FILES_H