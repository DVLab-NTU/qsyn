from qiskit.chemistry.components.variational_forms import UCCSD
from .mypauli import pauliString

def gene_uccsd_oplist(num_orbitals, num_particles, qubit_mapping='bravyi_kitaev'):
    var_form = UCCSD(num_orbitals=num_orbitals, num_particles=num_particles, qubit_mapping=qubit_mapping)
    oplist = []
    for i in var_form._hopping_ops:
        t = []
        ps = i.to_dict()['paulis']
        for j in ps:
            t.append(pauliString(j['label'], real=j['coeff']['real'], imag=j['coeff']['imag']))
        oplist.append(t)
    return oplist

from qiskit.chemistry.drivers import PySCFDriver, UnitsType
from qiskit.chemistry import FermionicOperator
def get_qubit_info(atom_config):
    driver = PySCFDriver(atom=atom_config, unit=UnitsType.ANGSTROM, charge=0, spin=0, basis='sto3g')
    molecule = driver.run()
    num_particles = molecule.num_alpha + molecule.num_beta
    num_spin_orbitals = molecule.num_orbitals * 2
    return num_particles, num_spin_orbitals

def get_qubit_op(atom_config, freeze_core=False, two_qubit_reduction=False, remove_list=None, qubit_mapping='bravyi_kitaev'):
    driver = PySCFDriver(atom=atom_config, unit=UnitsType.ANGSTROM, charge=0, spin=0, basis='sto3g')
    molecule = driver.run()
    repulsion_energy = molecule.nuclear_repulsion_energy
    num_particles = molecule.num_alpha + molecule.num_beta
    num_spin_orbitals = molecule.num_orbitals * 2
    ferOp = FermionicOperator(h1=molecule.one_body_integrals, h2=molecule.two_body_integrals)
    energy_shift = 0
    if freeze_core == True:
        freeze_list = molecule.core_orbitals
        len_freeze_list = len(freeze_list)
        freeze_list = [x % molecule.num_orbitals for x in freeze_list]
        freeze_list += [x + molecule.num_orbitals for x in freeze_list]
        ferOp, energy_shift = ferOp.fermion_mode_freezing(freeze_list)
        num_spin_orbitals -= len(freeze_list)
        num_particles -= len(freeze_list)
    else:
        len_freeze_list = 0
    if remove_list != None:
        remove_list = [x % molecule.num_orbitals for x in remove_list]
        remove_list = [x - len_freeze_list for x in remove_list]
        remove_list += [x + molecule.num_orbitals - len_freeze_list for x in remove_list]
        ferOp = ferOp.fermion_mode_elimination(remove_list)
        num_spin_orbitals -= len(remove_list)
    qubitOp = ferOp.mapping(map_type=qubit_mapping, threshold=0.00000001)
    shift = energy_shift + repulsion_energy
    return qubitOp, num_particles, num_spin_orbitals, shift
    
def lih_oplist():
    # dist = 1.3
    # atom = "Li .0 .0 .0; H .0 .0 " + str(dist)
    # qubitOp, num_particles, num_spin_orbitals, shift = get_qubit_op(atom, freeze_core=True)
    # freezed core. 2, 10
    qubit_mapping = 'bravyi_kitaev' # 'jordan_wigner' BRAVYI_KITAEV
    return gene_uccsd_oplist(10, 2, qubit_mapping=qubit_mapping)

def beh2_oplist():
    # dist = 0.76
    # atom = "Be .0 .0 .0; H .0 .0 -" + str(dist)+"; H .0 .0 " + str(dist)
    # num_particles, num_spin_orbitals = get_qubit_info(atom)
    qubit_mapping = 'bravyi_kitaev'
    return gene_uccsd_oplist(14, 6, qubit_mapping=qubit_mapping)
    
def ch4_oplist():
    qubit_mapping = 'bravyi_kitaev'
    return gene_uccsd_oplist(18, 10, qubit_mapping=qubit_mapping)
    
def mgh_oplist():
    # dist = 1.3
    # atom = "Mg .0 .0 .0; H .0 .0 -" + str(dist)+"; H .0 .0 " + str(dist)
    # num_particles, num_spin_orbitals = get_qubit_info(atom)
    qubit_mapping = 'bravyi_kitaev'
    return gene_uccsd_oplist(22, 14, qubit_mapping=qubit_mapping)
    
def licl_oplist():
    # atom = "Li .0 .0 .0; Cl .0 .0 -1.5"
    # num_particles, num_spin_orbitals = get_qubit_info(atom)
    qubit_mapping = 'bravyi_kitaev'
    return gene_uccsd_oplist(26, 20, qubit_mapping=qubit_mapping)
    
def co2_oplist():
    qubit_mapping = 'bravyi_kitaev'
    return gene_uccsd_oplist(30, 11, qubit_mapping=qubit_mapping)
    
