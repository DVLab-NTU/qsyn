"""JW / BK / Parity fermion-to-qubit mappings for molecular Hamiltonians.

Primary path: load shipped coefficient caches under
``baselines/ham_cache/`` (no PySCF required).

Optional path: regenerate with ``qiskit-nature`` + ``pyscf`` when caches are
missing or ``--regen`` is requested.

Track B convention (same fermionic H, three mappings)::

    LiH  → ham_model=hf   (active space → 12 qubits)
    H2O / N2 / H2S / CO2 → ham_model=full
"""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Dict, List, Literal, Optional, Sequence, Tuple

from pauli_list import PauliCircuit, Rotation, write_pauli_file
from util import BENCHMARK_DIR

HERE = Path(__file__).resolve().parent
HAM_CACHE_DIR = HERE / "baselines" / "ham_cache"

MappingName = Literal["jordan_wigner", "bravyi_kitaev", "parity"]
HamModel = Literal["hf", "full"]

TRACK_B_MOLECULES: Tuple[str, ...] = ("LiH", "H2O", "N2", "H2S", "CO2")
MAPPING_NAMES: Tuple[MappingName, ...] = (
    "jordan_wigner",
    "bravyi_kitaev",
    "parity",
)
MAPPING_ALIASES = {
    "jw": "jordan_wigner",
    "jordan-wigner": "jordan_wigner",
    "jordan_wigner": "jordan_wigner",
    "bk": "bravyi_kitaev",
    "bravyi-kitaev": "bravyi_kitaev",
    "bravyi_kitaev": "bravyi_kitaev",
    "parity": "parity",
}

MOLECULE_HAM_MODEL: Dict[str, HamModel] = {
    "LiH": "hf",
    "H2O": "full",
    "N2": "full",
    "H2S": "full",
    "CO2": "full",
}

MOLECULE_GEOM: Dict[str, str] = {
    "LiH": "Li 0 0 0; H 0 0 0.76",
    "H2O": "O 0 0 0; H 0.95 -0.55 0; H -0.95 -0.55 0",
    "N2": "N 0 0 0; N 0 0 -1.5",
    "H2S": "S 0 0 0; H 0 0 -1.5; H 0 0 1.5",
    "CO2": "O 1.4 0 0; C 0 0 0; O -1.4 0 0",
}

MAPPER_CLASSES = {
    "jordan_wigner": ("qiskit_nature.second_q.mappers", "JordanWignerMapper"),
    "bravyi_kitaev": ("qiskit_nature.second_q.mappers", "BravyiKitaevMapper"),
    "parity": ("qiskit_nature.second_q.mappers", "ParityMapper"),
}

_PROBLEM_CACHE: Dict[str, object] = {}


def normalize_mapping(name: str) -> MappingName:
    key = name.strip().lower().replace(" ", "_")
    if key not in MAPPING_ALIASES:
        raise ValueError(
            f"unknown mapping {name!r}; expected one of "
            f"{list(MAPPING_NAMES)} (aliases: jw, bk, parity)"
        )
    return MAPPING_ALIASES[key]  # type: ignore[return-value]


def ham_model_for(benchmark: str) -> HamModel:
    if benchmark not in MOLECULE_HAM_MODEL:
        raise KeyError(f"unknown Track B molecule: {benchmark}")
    return MOLECULE_HAM_MODEL[benchmark]


def cache_path(
    benchmark: str,
    mapping: MappingName,
    *,
    ham_model: Optional[HamModel] = None,
    cache_dir: Path = HAM_CACHE_DIR,
) -> Path:
    model = ham_model or ham_model_for(benchmark)
    return Path(cache_dir) / f"{benchmark}_{model}_{mapping}.json"


def load_ham_cache(
    benchmark: str,
    mapping: MappingName,
    *,
    ham_model: Optional[HamModel] = None,
    cache_dir: Path = HAM_CACHE_DIR,
) -> Optional[Tuple[int, Dict[str, float], dict]]:
    path = cache_path(benchmark, mapping, ham_model=ham_model, cache_dir=cache_dir)
    if not path.is_file():
        return None
    d = json.loads(path.read_text(encoding="utf-8"))
    return (
        int(d["n_qubits"]),
        {k: float(v) for k, v in d["coeffs"].items()},
        dict(d.get("meta", {})),
    )


def save_ham_cache(
    benchmark: str,
    mapping: MappingName,
    n_qubits: int,
    coeffs: Dict[str, float],
    meta: dict,
    *,
    ham_model: Optional[HamModel] = None,
    cache_dir: Path = HAM_CACHE_DIR,
) -> Path:
    path = cache_path(benchmark, mapping, ham_model=ham_model, cache_dir=cache_dir)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps({"n_qubits": n_qubits, "coeffs": coeffs, "meta": meta}, indent=2),
        encoding="utf-8",
    )
    return path


def w_to_theta(w_coeffs: Dict[str, float], *, dt: float = 0.05) -> Dict[str, float]:
    """Hamiltonian weight ``w`` → Trotter angle ``θ = 2 w dt``."""
    return {p: 2.0 * float(w) * dt for p, w in w_coeffs.items()}


def coeffs_to_pauli_circuit(
    name: str,
    n_qubits: int,
    w_coeffs: Dict[str, float],
    *,
    dt: float = 0.05,
) -> PauliCircuit:
    """Build Pauli list from Hamiltonian weights (``θ = 2 w dt``)."""
    rots: List[Rotation] = []
    for p, w in sorted(w_coeffs.items()):
        if set(p) == {"I"}:
            continue
        theta = 2.0 * float(w) * dt
        if abs(theta) < 1e-15:
            continue
        rots.append(Rotation(p, theta))
    return PauliCircuit(name, n_qubits, rots)


def _load_mapper(name: MappingName):
    mod_name, cls_name = MAPPER_CLASSES[name]
    import importlib

    return getattr(importlib.import_module(mod_name), cls_name)()


def _qiskit_to_coeffs(qubit_op) -> Tuple[int, Dict[str, float]]:
    n = qubit_op.num_qubits
    out: Dict[str, float] = {}
    for lbl, c in qubit_op.to_list():
        if abs(c.imag) > 1e-10:
            raise ValueError(f"non-real coefficient on {lbl}: {c}")
        w = float(c.real)
        if abs(w) < 1e-14:
            continue
        out[lbl] = out.get(lbl, 0.0) + w
    return n, out


def _fermionic_problem(benchmark: str, ham_model: HamModel, *, basis: str = "sto3g"):
    key = f"{benchmark}:{ham_model}:{basis}"
    if key not in _PROBLEM_CACHE:
        from qiskit_nature.second_q.drivers import PySCFDriver

        driver = PySCFDriver(
            atom=MOLECULE_GEOM[benchmark],
            basis=basis,
            charge=0,
            spin=0,
        )
        problem = driver.run()
        if ham_model == "hf":
            if benchmark != "LiH":
                raise ValueError("ham_model=hf is only supported for LiH")
            from qiskit_nature.second_q.transformers import ActiveSpaceTransformer

            problem = ActiveSpaceTransformer(
                num_electrons=(2, 2),
                num_spatial_orbitals=6,
            ).transform(problem)
        _PROBLEM_CACHE[key] = problem
    return _PROBLEM_CACHE[key]


def generate_molecule_hamiltonian(
    benchmark: str,
    mapping: MappingName,
    *,
    ham_model: Optional[HamModel] = None,
    basis: str = "sto3g",
    use_cache: bool = True,
    cache_dir: Path = HAM_CACHE_DIR,
    allow_regen: bool = True,
) -> Tuple[int, Dict[str, float], dict]:
    """Return ``(n_qubits, w_coeffs, meta)`` for one mapping."""
    mapping = normalize_mapping(mapping)
    model = ham_model or ham_model_for(benchmark)
    if use_cache:
        cached = load_ham_cache(
            benchmark, mapping, ham_model=model, cache_dir=cache_dir
        )
        if cached is not None:
            return cached
    if not allow_regen:
        raise FileNotFoundError(
            f"ham cache missing for {benchmark}/{model}/{mapping}: "
            f"{cache_path(benchmark, mapping, ham_model=model, cache_dir=cache_dir)}\n"
            "Install optional deps (qiskit-nature, pyscf) and pass allow_regen=True, "
            "or restore shipped baselines/ham_cache/*.json"
        )
    problem = _fermionic_problem(benchmark, model, basis=basis)
    qubit_op = _load_mapper(mapping).map(problem.second_q_ops()[0])
    n, coeffs = _qiskit_to_coeffs(qubit_op)
    meta = {
        "source": "pyscf+qiskit-nature",
        "ham_model": model,
        "basis": basis,
        "mapping": mapping,
        "n_spatial_orbitals": problem.num_spatial_orbitals,
        "n_particles": problem.num_particles,
    }
    if use_cache:
        save_ham_cache(
            benchmark, mapping, n, coeffs, meta, ham_model=model, cache_dir=cache_dir
        )
    return n, coeffs, meta


def load_mapping_pauli_circuit(
    benchmark: str,
    mapping: MappingName | str,
    *,
    dt: float = 0.05,
    ham_model: Optional[HamModel] = None,
    use_cache: bool = True,
    allow_regen: bool = True,
) -> Tuple[PauliCircuit, dict]:
    """Fermionic H → mapped PauliCircuit (Trotter θ = 2 w dt)."""
    mapping = normalize_mapping(str(mapping))
    n, w_coeffs, meta = generate_molecule_hamiltonian(
        benchmark,
        mapping,
        ham_model=ham_model,
        use_cache=use_cache,
        allow_regen=allow_regen,
    )
    name = f"{benchmark}-{mapping}"
    pc = coeffs_to_pauli_circuit(name, n, w_coeffs, dt=dt)
    meta = {**meta, "dt": dt, "n_terms": pc.n_rot, "benchmark": benchmark}
    return pc, meta


def _emit_qasm(pc: PauliCircuit, *, dt: float, title: str) -> str:
    """Emit one-Trotter-step OpenQASM via gen_benchmark_qasm."""
    if str(BENCHMARK_DIR) not in sys.path:
        sys.path.insert(0, str(BENCHMARK_DIR))
    import gen_benchmark_qasm as gen  # noqa: E402

    # gen expects Hamiltonian weights w with angle = 2 w dt
    plist = [(r.pauli, r.theta / (2.0 * dt)) for r in pc.rotations]
    return gen.emit_trotter_qasm(pc.n_qubits, plist, dt=dt, title=title)


def prepare_mapping_benchmark(
    out_root: Path,
    benchmark: str,
    mapping: MappingName | str,
    *,
    dt: float = 0.05,
    ham_model: Optional[HamModel] = None,
    allow_regen: bool = True,
) -> Path:
    """Write ``<out>/<Bench>-<mapping>/{...}.pauli,.qasm}``."""
    mapping = normalize_mapping(str(mapping))
    pc, meta = load_mapping_pauli_circuit(
        benchmark, mapping, dt=dt, ham_model=ham_model, allow_regen=allow_regen
    )
    label = f"{benchmark}-{mapping}"
    bench_dir = Path(out_root) / label
    bench_dir.mkdir(parents=True, exist_ok=True)
    write_pauli_file(pc, bench_dir / f"{label}.pauli")
    qasm = _emit_qasm(
        pc,
        dt=dt,
        title=f"{benchmark} mapping={mapping} ham_model={meta.get('ham_model')} dt={dt}",
    )
    (bench_dir / f"{label}.qasm").write_text(qasm, encoding="utf-8")
    meta_path = bench_dir / "meta.json"
    meta_path.write_text(json.dumps(meta, indent=2), encoding="utf-8")
    print(
        f"[prepare-mapping] {label}: {pc.n_qubits}q, {pc.n_rot} rotations → {bench_dir}"
    )
    return bench_dir


def prepare_all_mappings(
    out_root: Path,
    benches: Optional[Sequence[str]] = None,
    mappings: Optional[Sequence[str]] = None,
    *,
    dt: float = 0.05,
    allow_regen: bool = True,
) -> List[Path]:
    benches = list(benches) if benches else list(TRACK_B_MOLECULES)
    maps = (
        [normalize_mapping(m) for m in mappings]
        if mappings
        else list(MAPPING_NAMES)
    )
    written: List[Path] = []
    for b in benches:
        for m in maps:
            written.append(
                prepare_mapping_benchmark(
                    out_root, b, m, dt=dt, allow_regen=allow_regen
                )
            )
    return written
