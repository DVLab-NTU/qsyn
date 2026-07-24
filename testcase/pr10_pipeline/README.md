# PR-10 — CPF product pipeline CLI

Exercises the target pipeline:

1. `qcir to-u3cx` — logical U3+CX (QSD/KAK, BQSKit-equivalent role)
2. `qcir to-zyz` — ZYZ+CX (RZ/RY/RZ + CX)
3. `qcir to-tableau --fold` — interleaved Tableau + `global_fold`
4. `qcir from-tableau` — Gray-synth (naive fallback for non-diagonal blocks)
5. `qcpfq` — same as `qcir cpf-pipeline -r`

```bash
./build/qsyn -f testcase/pr10_pipeline/dof/pipeline_steps.dof
```
