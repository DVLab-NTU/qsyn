#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "qcir/qcir_io.hpp"

namespace {

class TemporaryQcFile {
public:
    explicit TemporaryQcFile(std::string const& contents)
        : _path{std::filesystem::temp_directory_path() /
                ("qsyn-qc-reader-" +
                 std::to_string(std::chrono::steady_clock::now()
                                    .time_since_epoch()
                                    .count()) +
                 ".qc")} {
        std::ofstream output{_path};
        output << contents;
    }

    ~TemporaryQcFile() { std::filesystem::remove(_path); }

    std::filesystem::path const& path() const { return _path; }

private:
    std::filesystem::path _path;
};

}  // namespace

TEST_CASE("QC reader supports legacy adjoint phase gates", "[qcir][reader]") {
    TemporaryQcFile const input{
        ".v q\n"
        "BEGIN\n"
        "T* q\n"
        "P q\n"
        "P* q\n"
        "END\n"};

    auto circuit = qsyn::qcir::from_qc(input.path());

    REQUIRE(circuit.has_value());
    REQUIRE(circuit->get_num_gates() == 3);
    CHECK(qsyn::qcir::to_qasm(*circuit) ==
          "OPENQASM 2.0;\n"
          "include \"qelib1.inc\";\n"
          "qreg q[1];\n"
          "tdg q[0];\n"
          "s q[0];\n"
          "sdg q[0];\n");
}
