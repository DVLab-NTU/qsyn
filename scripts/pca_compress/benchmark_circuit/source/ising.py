import numpy as np
from qiskit.optimization.applications.ising import max_cut
from .mypauli import pauliString

def gene_dot_1d(w, interaction='Z'):
    nq = w + 1
    oplist = []
    for i in range(nq - 1):
        ps = i*'I' + interaction + interaction + (nq-2-i)*'I'
        oplist.append([pauliString(ps, coeff=1.0)])
    return oplist

# w >= 2, h >= 2
def gene_dot_2d(w, h, offset=0, numq=0, interaction='Z'):
    if numq == 0:
        nq = (w+1)*(h+1)
    else:
        nq = numq
    oplist = []
    for i in range(w):
        for j in range(h):
            k = (w+1)*j + i + offset
            ps = ['I']*nq
            ps[k] = interaction
            ps[k+1] = interaction
            oplist.append([pauliString("".join(ps), coeff=1.0)])
            ps = ['I']*nq
            ps[k] = interaction
            ps[k+1+w] = interaction
            oplist.append([pauliString("".join(ps), coeff=1.0)])
    for j in range(h):
        k = (w+1)*j + w + offset
        ps = ['I']*nq
        ps[k] = interaction
        ps[k+1+w] = interaction
        oplist.append([pauliString("".join(ps), coeff=1.0)])
    for i in range(w):
        k = h*(w+1) + i + offset
        ps = ['I']*nq
        ps[k] = interaction
        ps[k+1] = interaction
        oplist.append([pauliString("".join(ps), coeff=1.0)])
    return oplist

# w, h, l >= 2
# w*(h+1)*(l+1) + (w+1)*h*(l+1) + (w+1)*(h+1)*l
def gene_dot_3d(w, h, l, interaction='Z'):
    oplist = []
    nq = (w+1)*(h+1)*(l+1)
    for i in range(l+1):
        oplist += gene_dot_2d(w, h, numq=nq, offset=i*(w+1)*(h+1), interaction=interaction)
    for i in range(l):
        for j in range((w+1)*(h+1)):
            k = i*(w+1)*(h+1) + j
            ps = ['I']*nq
            ps[k] = interaction
            ps[k+(w+1)*(h+1)] = interaction
            oplist.append([pauliString("".join(ps), coeff=1.0)])
    return oplist
