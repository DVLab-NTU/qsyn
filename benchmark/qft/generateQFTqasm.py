from argparse import ArgumentParser, Namespace
from pathlib import Path


def decomposeH(w, ID):
    w.write("rz(pi/2) q[{}];\n".format(ID))
    w.write("sx q[{}];\n".format(ID))
    w.write("rz(pi/2) q[{}];\n".format(ID))


def decomposeCRZ(w, ang, ctrl, targ):
    w.write("p(pi/{ang}) q[{ctrl}];\n".format(ang=format(ang * 2), ctrl=ctrl))
    w.write("p(pi/{ang}) q[{targ}];\n".format(ang=format(ang * 2), targ=targ))
    w.write("cx q[{ctrl}],q[{targ}];\n".format(ctrl=ctrl, targ=targ))
    w.write("p(-pi/{ang}) q[{targ}];\n".format(ang=ang * 2, targ=targ))
    w.write("cx q[{ctrl}],q[{targ}];\n".format(ctrl=ctrl, targ=targ))


def main(args):
    with open(
        "{root}/qft_{num}{dec}{pyzx}.qasm".format(
            root=args.output_root, num=args.qnum, dec="dec" if args.decompose else "", pyzx="crz" if args.forpyzx else ""
        ),
        "w",
    ) as qasmf:
        qasmf.write('OPENQASM 2.0;\ninclude "qelib1.inc";\n')
        qasmf.write("qreg q[{}];\n".format(args.qnum))
        for i in range(args.qnum):
            qasmf.write("h q[{}];\n".format(i))
            # if args.decompose:
            #     decomposeH(qasmf, i)
            # else:
            #     qasmf.write("h q[{}];\n".format(i))

            for j in range(args.qnum - i - 1):
                if args.decompose:
                    decomposeCRZ(
                        qasmf,
                        pow(2, j + 1)
                        if pow(2, j + 1) < args.max_denominator_value
                        else args.max_denominator_value,
                        i + j + 1,
                        i,
                    )
                elif args.forpyzx:
                    qasmf.write(
                        "rz(pi/{ang}) q[{targ}];\n".format(
                            ang=pow(2, j + 2)
                            if pow(2, j + 2) < args.max_denominator_value
                            else args.max_denominator_value,
                            targ=i + j + 1
                        )
                    )
                    qasmf.write(
                        "crz(pi/{ang}) q[{ctrl}],q[{targ}];\n".format(
                            ang=pow(2, j + 1)
                            if pow(2, j + 1) < args.max_denominator_value
                            else args.max_denominator_value,
                            ctrl=i + j + 1,
                            targ=i,
                        )
                    )
                else:
                    qasmf.write(
                        "cp(pi/{ang}) q[{ctrl}],q[{targ}];\n".format(
                            ang=pow(2, j + 1)
                            if pow(2, j + 1) < args.max_denominator_value
                            else args.max_denominator_value,
                            ctrl=i + j + 1,
                            targ=i,
                        )
                    )


def parse_args() -> Namespace:
    parser = ArgumentParser()
    parser.add_argument("--qnum", type=int, help="Size of qft", required=True)
    parser.add_argument(
        "--decompose",
        action="store_true",
        help="Decompose H and CR gate into CX and Rz",
    )
    parser.add_argument(
        "--forpyzx",
        action="store_true",
        help="Decompose CP into CRz and Rz for PyZX",
    )
    parser.add_argument(
        "--max_denominator_value",
        type=int,
        help="Maximum value of denominator",
        default=pow(2, 500),
    )
    parser.add_argument(
        "--output_root", type=Path, help="Output file directory", default="./"
    )
    args = parser.parse_args()
    return args


if __name__ == "__main__":
    args = parse_args()
    main(args)
