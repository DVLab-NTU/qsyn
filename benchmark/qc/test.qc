.v a b result c 1
.i c a BEGIN

BEGIN
H 1
S c
tof a b
Z a 1
X result
S* a
cnot result a
cx a b
tof a b c
tof 1

END