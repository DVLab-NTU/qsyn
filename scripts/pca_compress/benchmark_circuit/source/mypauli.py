import numpy as np
class pauliString():
    def __init__(self, ps, coeff=0., real=0., imag=0.):
        self.ps = ps
        self.coeff = coeff
        if real != 0.:
            self.real = real
            self.coeff += real
        else:
            self.real = np.real(self.coeff)
        if imag != 0.:
            self.imag = imag
            self.coeff += 1j*imag
        else:
            self.imag = np.imag(self.coeff)
    def __len__(self):
        return len(self.ps)
    def count(self, c):
        return self.ps.count(c)
    # def __print__(self):
    #     return self.ps
    def __repr__(self):
         return self.ps
    # def __str__(self):
    #     return self.ps

# tools
def pauli_string_oplist(psl):
    op = []
    for i in psl:
        op.append([pauliString(i, real=1.0)])
    return op

def pauli_string_layer(psl):
    pl = []
    for i in psl:
        pl.append([[pauliString(i, real=1.0)]])
    return pl

def pauli_layer_pauli_list(pl):
    psl = []
    from qiskit.quantum_info import Pauli
    for i in pl:
        for j in i:
            for k in j:
                psl.append([k.coeff, Pauli.from_label(k.ps[::-1])])
    return psl