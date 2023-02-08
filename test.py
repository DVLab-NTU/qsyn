from argparse import ArgumentParser

parser = ArgumentParser()

parser.add_argument("apple", nargs='*', type=int)
parser.add_argument("-banana", type=str)

result = parser.parse_args()

print(result)