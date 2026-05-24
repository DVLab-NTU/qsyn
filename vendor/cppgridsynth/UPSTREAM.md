# cppgridsynth (vendored)

C++ port of [GridSynth](https://github.com/amyhli/GridSynth) used by qsyn's `qcir gridsynth` command.

This directory is a **vendored snapshot** of the library sources (`include/`, `src/`). qsyn-specific glue lives in `src/qcir/gridsynth/gridsynth_adapter.cpp` (outside this tree).

When updating, sync from the upstream GridSynth/cppgridsynth repository and re-run qsyn tests that cover `qcir gridsynth`.

**Build-time dependency (not vendored):** system [GMP](https://gmplib.org/) and [MPFR](https://www.mpfr.org/) — linked only into the `cppgridsynth` static library, not into the rest of qsyn.
