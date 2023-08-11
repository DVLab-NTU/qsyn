/****************************************************************************
  FileName     [ tmpFiles.h ]
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

std::filesystem::path createTmpDir(std::string_view prefix);
std::filesystem::path createTmpFile(std::string_view prefix);

}  // namespace detail

class TmpDir {
public:
    TmpDir() : _dir{detail::createTmpDir(std::filesystem::temp_directory_path().string() + "/dvlab-")} {}
    TmpDir(std::string_view prefix) : _dir{detail::createTmpDir(prefix)} {}
    ~TmpDir() { std::filesystem::remove_all(_dir); }

    std::filesystem::path path() const { return _dir; }

private:
    std::filesystem::path const _dir;
};

class TmpFile {
public:
    TmpFile() : _stream{detail::createTmpFile(std::filesystem::temp_directory_path().string() + "/dvlab-")} {
        assert(_stream.is_open());
    }
    TmpFile(std::string_view prefix) : _stream{detail::createTmpFile(prefix)} {
        assert(_stream.is_open());
    }

    std::fstream& stream() { return _stream; }

private:
    std::fstream _stream;
};

}  // namespace dvlab_utils

#endif  // DVLAB_TEMP_FILES_H