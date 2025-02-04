#include "layoutcir_mgr.hpp"
#include <fmt/core.h>
#include <cstddef>
#include "placer.hpp"

namespace qsyn::duostra {

bool LayoutCirMgr::map() {
    fmt::println("start mapping layout circuit");

    fmt::println("Initial Placement...");

    std::vector<QubitIdType> assign;
    spdlog::info("Calculating Initial Placement...");
    auto placer = get_placer(_config.placer_type);
    assign      = placer->place_and_assign(_device);

    size_t gate_idx = 0;

    while (true){
        
        
    }
    return true;
};

}  // namespace qsyn::duostra