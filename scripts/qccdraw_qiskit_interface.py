'''
  FileName     [ qccdraw_qiskit_interface.py ]
  PackageName  [ qcir ]
  Synopsis     [ Define class Lattice and LTContainer structures ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
'''
import sys
import argparse
from qiskit import QuantumCircuit

def draw_qc(args) -> bool:
    qc = QuantumCircuit.from_qasm_file(args.input)
    ret = qc.draw(args.drawer, scale=args.scale, filename=args.output)
    if ((args.drawer == 'text' or args.drawer == 'latex_source') and args.output == None):
        print(ret)
    return True # if successfully drawn the circuit; else return False

def main() -> int:
    parser = argparse.ArgumentParser(
        description='Draw a .qasm file using Qiskit. '
                    'This file also serves as the interface for qsyn '
                    'to call Qiskit.',
        formatter_class=argparse.RawTextHelpFormatter
    )
    
    parser.add_argument(
        "-drawer", 
        type=str,
        choices=['text', 'mpl', 'latex', 'latex_source'],
        default='text',
        help='the style of quantum circuit output. \n'
             '\'text\'        : ASCII art TextDrawing that can be printed in the console. \n'
             '\'mpl\'         : images with color rendered purely in Python using matplotlib. \n'
             '\'latex\'       : high-quality images compiled via latex. \n'
             '\'latex_source\': raw uncompiled latex output. '
    )
    
    parser.add_argument(
        "-input", 
        type=str,
        required=True,
        help='the input .qasm file'
    )
    parser.add_argument(
        "-output",
        type=str,
        help='if specified, output the resulting drawing into this file.\n'
             'This argument is mandatory if the style is \'mpl\' or \'latex\''                    
    )
    parser.add_argument(
        "-scale",
        type=float,
        default=1,
        help='if specified, scale the resulting drawing by this factor.\n'
             'This argument should not appear with \'text\' drawer'
    )

    args = parser.parse_args()

    if (args.drawer == 'mpl' and args.output == None):
        parser.error('Using drawer \'mpl\' requires an output destination')
    if (args.drawer == 'latex' and args.output == None):
        parser.error('Using drawer \'latex\' requires an output destination')
    if (args.drawer == 'text' and args.scale != 1):
        parser.error('Cannot set scale for \'text\' drawer')
    
    ## uncomment the following four lines to check parsing result
    # print(f'print mode  = {args.drawer}')
    # print(f'file input  = {args.input}')
    # print(f'file output = {args.output}')
    # print(f'scale       = {args.scale}')

    
    return draw_qc(args) == True

if __name__ == '__main__':
    main()