import sys

def main():
    filename = sys.argv[1]
    output = "./benchmark/benchmark_SABRE/small/"+filename.rsplit('/', 1)[1]
    with open("./tests/zxrules/result/dof/FR.dof", 'w') as f:
        f.write("ver 1\nqccread %s\nqc2zx\nzxgp -Q\nzxgs -FR\nzxgp -Q\nq -f\n" %output)

if __name__ == "__main__":
    main()