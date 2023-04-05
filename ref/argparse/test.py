from argparse import ArgumentParser

parser = ArgumentParser()


parser.add_argument("cat")
parser.add_argument("goose", nargs='?', type=int)
parser.add_argument("dog")

result = parser.parse_args()

print(result)