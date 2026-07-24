# PR-2 — `tableau/cpf/angle_utils`

`angle_utils` is a header-only-style namespace of phase predicates used by all
later CPF passes. It is exercised indirectly by the PR-3 / PR-4 testcases;
this directory documents expected values for hand verification.

## Reference table

| `dvlab::Phase` literal      | `is_zero` | `is_pi` | `is_clifford` |
|--|--|--|--|
| `Phase(0)`                  | true  | false | true  |
| `Phase(1, 4)` (= pi/4)      | false | false | false |
| `Phase(1, 2)` (= pi/2)      | false | false | true  |
| `Phase(-1, 2)` (= -pi/2)    | false | false | true  |
| `Phase(1, 1)` (= pi)        | false | true  | true  |
| `Phase(-1, 1)` (= -pi)      | false | true  | true  |   (-pi normalises to +pi)
| `Phase(3, 8)` (= 3pi/8)     | false | false | false |
| `Phase(1, 8) + Phase(7, 8)` | true  | false | true  |   (= 0 after norm)
| `Phase(1, 4) + Phase(3, 4)` | false | true  | true  |   (= pi after norm)

The properties above are encoded in the source header comments of
`src/tableau/cpf/angle_utils.{hpp,cpp}`.

## When to use which predicate

- `is_clifford_phase` — gating for `absorb_clifford_rotations`-style passes
  (a rotation with a Clifford phase can be absorbed into the Clifford layer).
- `is_zero_phase` — used by `propagation_merge` and `global_fold` to drop
  identity rotations created by an angle cancellation.
- `is_pi_phase` — used by ZX-style pre-processing (pi-rotation pushing).
- `approx_equal` — only for tolerance-aware tests; pure CPF logic stays
  rational and exact.
