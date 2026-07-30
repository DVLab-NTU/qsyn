#!/usr/bin/env bash
# Run paper 2-qubit NCF (w=128) on the 10 NCF Table-IV-style cases.
# Saves Clifford+RZ before/after and a singleton summary CSV.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
QSYN="${ROOT}/build/qsyn"
OUT="${ROOT}/out/ncf2q_10cases"
SUMMARY="${OUT}/summary.csv"
CASES=(
  h2_jw_four
  LiH_frz_JW
  Be2-JW-6
  BH-JW-10
  uccsd_10
  OH-JW10
  OH-JW12
  H2O_frz_JW_sto3g
  Li2-JW14
  NH-JW-14
)

echo "case,n_qubits,n_terms,n_anti_groups,n_singletons,Num_unitaries,merge_ratio,elapsed_s,before_qasm,after_qasm,status" > "$SUMMARY"

for case in "${CASES[@]}"; do
  dir="${OUT}/${case}"
  json="${dir}/${case}.json"
  dofile="${dir}/run_ncf2q.do"
  log="${dir}/run_ncf2q.log"
  before="${dir}/${case}_before_2qncf_clifford_rz.qasm"
  after="${dir}/${case}_after_2qncf_clifford_rz.qasm"
  blocks="${dir}/blocks.txt"

  cat > "$dofile" <<EOF
tableau from-terms ${json} --use-coeff
tableau optimize collapse
tableau copy 1
tableau checkout 1
convert tableau qcir -r naive
qcir write ${before}
tableau checkout 0
tableau optimize ncf --two-qubit --window 128
ncf print --blocks
convert tableau qcir -r ncf
qcir write ${after}
EOF

  echo "==== ${case} ===="
  t0=$(date +%s)
  set +e
  "${QSYN}" -f "$dofile" >"$log" 2>&1
  rc=$?
  set -e
  t1=$(date +%s)
  elapsed=$((t1 - t0))

  # Parse partition stats from log
  line=$(rg -N "NCF-2q partition:" "$log" | tail -1 || true)
  # e.g. NCF-2q partition: n_terms=14 anti_groups=2 singletons=5 Num_unitaries=7 window=0
  n_terms=$(echo "$line" | sed -n 's/.*n_terms=\([0-9]*\).*/\1/p')
  n_anti=$(echo "$line" | sed -n 's/.*anti_groups=\([0-9]*\).*/\1/p')
  n_sing=$(echo "$line" | sed -n 's/.*singletons=\([0-9]*\).*/\1/p')
  num_u=$(echo "$line" | sed -n 's/.*Num_unitaries=\([0-9]*\).*/\1/p')
  nq=$(python3 -c "import json; print(json.load(open('$json'))['n_qubits'])")
  if [[ -z "${n_terms}" ]]; then n_terms=$(python3 -c "import json; print(len(json.load(open('$json'))['terms']))"); fi
  if [[ -z "${n_anti}" ]]; then n_anti=""; fi
  if [[ -z "${n_sing}" ]]; then n_sing=""; fi
  if [[ -z "${num_u}" ]]; then num_u=""; fi

  merge=""
  if [[ -n "$num_u" && "$n_terms" -gt 0 ]]; then
    merge=$(python3 -c "print(f'{1.0 - float('$num_u')/float('$n_terms'):.6f}')")
  fi

  rg -N "block .*:" "$log" | rg "NCF-2q:|NCF: block|SINGLETON|multi size" > "$blocks" || true

  status="ok"
  if [[ $rc -ne 0 ]]; then status="fail_rc${rc}"; fi
  if [[ ! -f "$after" ]]; then status="${status}+no_after"; fi

  echo "${case},${nq},${n_terms},${n_anti},${n_sing},${num_u},${merge},${elapsed},${before},${after},${status}" | tee -a "$SUMMARY"
  echo "  anti=${n_anti} singletons=${n_sing} Num_u=${num_u} elapsed=${elapsed}s status=${status}"
done

echo
echo "Wrote ${SUMMARY}"
column -t -s, "$SUMMARY" 2>/dev/null || cat "$SUMMARY"
