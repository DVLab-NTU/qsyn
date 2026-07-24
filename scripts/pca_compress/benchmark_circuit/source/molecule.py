from qiskit.chemistry.transformations import (FermionicTransformation, FermionicTransformationType, FermionicQubitMappingType)
from qiskit.chemistry.drivers import PySCFDriver, UnitsType, Molecule
from .mypauli import pauliString

H2 = [['H', [0., 0, 0]],['H', [0, 0, -1.5]]]
LiH = [['Li', [0,0,0]],['H', [0,0,0.76]]]
N2 = [['N', [0., 0, 0]],['N', [0, 0, -1.5]]]
H2O = [['O', [0.,0.,0.]],['H', [0.95,-0.55,0.]],['H', [-0.95,-0.55,0.]]]
H2S = [['S', [0., 0, 0]],['H', [0, 0, -1.5]],['H', [0, 0, 1.5]]]
CH4 = [['C', [0.,1.0,1.0]],['H', [0.051054399,0.051054399,0.051054399]],
    ['H', [1.948945601,1.948945601,0.051054399]],
    ['H', [0.051054399,1.948945601,1.948945601]],
    ['H', [1.948945601,0.051054399,1.948945601]]]
MgO = [['Mg', [0., 0, 0]],['O', [0, 0, -1.5]]]
CO2 = [['O', [1.4, 0., 0.]],['C', [0., 0., 0.]],['O', [-1.4, 0., 0.]]]
NaCl = [['Na', [0., -1.5, -1.5]],['Cl', [1.5, -1.5, -1.5]]]
KOH = [['K', [3.4030, 0.2500, 0.0]],['O', [2.5369, -0.2500, -1.5]],['H', [2.0,0.06,0.0]]]
FeO = [['Fe', [0., 0, 0]],['O', [0, 0, -1.5]]]

def get_qubit_op(geo):
    molecule = Molecule(geometry=geo, charge=0, multiplicity=1)
    driver = PySCFDriver(molecule = molecule, unit=UnitsType.ANGSTROM, basis='sto3g')
    fermionic_transformation = FermionicTransformation(transformation=FermionicTransformationType.FULL, qubit_mapping=FermionicQubitMappingType.BRAVYI_KITAEV, two_qubit_reduction=False, freeze_core=False)
    qubit_op, _ = fermionic_transformation.transform(driver)
    # print(qubit_op.num_qubits, ",", len(qubit_op))
    # print(qubit_op.primitive_strings())
    # for i in qubit_op:
    #     print(i.primitive)
    # print(fermionic_transformation.molecule_info)
    return qubit_op

def gene_molecule_oplist(atom_geo):
    qubit_op = get_qubit_op(atom_geo)
    oplist = []
    for i in qubit_op:
        oplist.append([pauliString(i.primitive.to_label(), coeff=i.coeff)])
    return oplist

def gene_h20_oplist():
    return gene_molecule_oplist(H2O)
