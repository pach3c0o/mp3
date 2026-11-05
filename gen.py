import random
import sys

if len(sys.argv) != 6:
    print("usage: python3 gen.py ROWS COLUMNS GENERATIONS DENSITY SEED > input.txt")
    sys.exit(1)

rows = int(sys.argv[1])
columns = int(sys.argv[2])
generations = int(sys.argv[3])
density = float(sys.argv[4])
seed = int(sys.argv[5])

random.seed(seed)

objects = []
for row in range(rows):
    for column in range(columns):
        if random.random() < density:
            object_type = random.choice(["ROCK", "RABBIT", "RABBIT", "FOX"])
            objects.append((object_type, row, column))

gen_proc_rabbits = 3
gen_proc_foxes = 8
gen_food_foxes = 5
print(gen_proc_rabbits, gen_proc_foxes, gen_food_foxes, generations, rows, columns, len(objects))
for object_type, row, column in objects:
    print(object_type, row, column)
