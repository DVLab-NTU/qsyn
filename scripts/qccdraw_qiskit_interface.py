# TODO - t5 - Draw quantum circuits by calling qiskit
'''
* This script are to be used by qsyn via system call
* We have provided a simple calling interface for you, 
  so you don't need to completely start from scratch.

  The script can be called with 
  $ python3 scripts/qccdraw_qiskit_interface.py
'''
import sys
import argparse
from qiskit import QuantumCircuit

def draw_qc(style, input, output) -> bool:
    # TODO - t5 - Draw quantum circuits by calling qiskit
    '''
    Refer to Qiskit's documentation for how to read .qasm file and
    to draw a circuit

    You may define additional functions in this .py file as needed
    '''
    return True # if successfully drawn the circuit; else return False

def main() -> int:
    parser = argparse.ArgumentParser(
        description='Draw a .qasm file using Qiskit. '
                    'This file also serves as the interface for qsyn '
                    'to call Qiskit.',
        formatter_class=argparse.RawTextHelpFormatter
    )
    
    parser.add_argument(
        "-style", 
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
        required=True,
        help='the input .qasm file'
    )
    parser.add_argument(
        "-output",
        help='if specified, output the resulting drawing into this file.\n'
             'This argument is mandatory if the style is \'mpl\' or \'latex\''                    
    )

    # TODO - t5 - Draw quantum circuits by calling qiskit
    '''
    Refer to Qiskit's documentation for how to read .qasm file and
    to draw a circuit

    You may define additional functions in this .py file as needed
    '''
    args = parser.parse_args()

    if (args.style == 'mpl' and args.output == None):
        parser.error('Using style \'mpl\' requires an output destination')
    if (args.style == 'latex' and args.output == None):
        parser.error('Using style \'latex\' requires an output destination')
    
    ## uncomment the following three lines to check parsing result
    # print(f'print mode  = {args.style}')
    # print(f'file input  = {args.input}')
    # print(f'file output = {args.output}')

    
    return 0 if draw_qc(args.style, args.input, args.output) else 1

if __name__ == '__main__':
    main()