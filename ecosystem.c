#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NO_NEIGHBOR -1
#define FOX_STARVES -2

enum CellType { EMPTY, ROCK, RABBIT, FOX };

typedef struct {
    int type;
    int procreationAge;
    int hunger;
} Cell;

typedef struct {
    int genProcRabbits;
    int genProcFoxes;
    int genFoodFoxes;
    int numberOfGenerations;
    int rows;
    int columns;
} Config;

static Config config;

static const int rowOffset[4] = {-1, 0, 1, 0};
static const int columnOffset[4] = {0, 1, 0, -1};

static int isInsideWorld(int row, int column) {
    return row >= 0 && row < config.rows && column >= 0 && column < config.columns;
}

static int cellIndex(int row, int column) {
    return row * config.columns + column;
}

static Cell makeCell(int type, int procreationAge, int hunger) {
    Cell cell;
    cell.type = type;
    cell.procreationAge = procreationAge;
    cell.hunger = hunger;
    return cell;
}

static int chooseNeighbor(const Cell *world, int row, int column, int generation, int wantedType) {
    int candidates[4];
    int candidateCount = 0;

    for (int direction = 0; direction < 4; direction++) {
        int neighborRow = row + rowOffset[direction];
        int neighborColumn = column + columnOffset[direction];

        if (!isInsideWorld(neighborRow, neighborColumn)) {
            continue;
        }
        if (world[cellIndex(neighborRow, neighborColumn)].type != wantedType) {
            continue;
        }
        candidates[candidateCount] = cellIndex(neighborRow, neighborColumn);
        candidateCount++;
    }

    if (candidateCount == 0) {
        return NO_NEIGHBOR;
    }
    return candidates[(generation + row + column) % candidateCount];
}

static int rabbitDestination(const Cell *world, int row, int column, int generation) {
    int emptyNeighbor = chooseNeighbor(world, row, column, generation, EMPTY);

    if (emptyNeighbor == NO_NEIGHBOR) {
        return cellIndex(row, column);
    }
    return emptyNeighbor;
}

static int foxDestination(const Cell *world, int row, int column, int generation, int *eatsRabbit) {
    int rabbitNeighbor = chooseNeighbor(world, row, column, generation, RABBIT);

    if (rabbitNeighbor != NO_NEIGHBOR) {
        *eatsRabbit = 1;
        return rabbitNeighbor;
    }

    *eatsRabbit = 0;
    int hungerAfterThisGeneration = world[cellIndex(row, column)].hunger + 1;
    if (hungerAfterThisGeneration >= config.genFoodFoxes) {
        return FOX_STARVES;
    }

    int emptyNeighbor = chooseNeighbor(world, row, column, generation, EMPTY);
    if (emptyNeighbor == NO_NEIGHBOR) {
        return cellIndex(row, column);
    }
    return emptyNeighbor;
}

static Cell nextCellAfterRabbitsMove(const Cell *before, int row, int column, int generation) {
    int index = cellIndex(row, column);
    Cell current = before[index];

    if (current.type == ROCK || current.type == FOX) {
        return current;
    }

    if (current.type == RABBIT) {
        int newAge = current.procreationAge + 1;
        int destination = rabbitDestination(before, row, column, generation);

        if (destination == index) {
            return makeCell(RABBIT, newAge, 0);
        }
        if (newAge > config.genProcRabbits) {
            return makeCell(RABBIT, 0, 0);
        }
        return makeCell(EMPTY, 0, 0);
    }

    Cell result = makeCell(EMPTY, 0, 0);

    for (int direction = 0; direction < 4; direction++) {
        int neighborRow = row + rowOffset[direction];
        int neighborColumn = column + columnOffset[direction];

        if (!isInsideWorld(neighborRow, neighborColumn)) {
            continue;
        }
        Cell neighbor = before[cellIndex(neighborRow, neighborColumn)];
        if (neighbor.type != RABBIT) {
            continue;
        }
        if (rabbitDestination(before, neighborRow, neighborColumn, generation) != index) {
            continue;
        }

        int arrivingAge = neighbor.procreationAge + 1;
        if (arrivingAge > config.genProcRabbits) {
            arrivingAge = 0;
        }

        int noRabbitYet = result.type == EMPTY;
        int arrivingIsOlder = arrivingAge > result.procreationAge;
        if (noRabbitYet || arrivingIsOlder) {
            result = makeCell(RABBIT, arrivingAge, 0);
        }
    }
    return result;
}

static Cell nextCellAfterFoxesMove(const Cell *before, int row, int column, int generation) {
    int index = cellIndex(row, column);
    Cell current = before[index];
    int eatsRabbit;

    if (current.type == ROCK) {
        return current;
    }

    if (current.type == FOX) {
        int newAge = current.procreationAge + 1;
        int destination = foxDestination(before, row, column, generation, &eatsRabbit);

        if (destination == FOX_STARVES) {
            return makeCell(EMPTY, 0, 0);
        }
        if (destination == index) {
            return makeCell(FOX, newAge, current.hunger + 1);
        }
        if (newAge > config.genProcFoxes) {
            return makeCell(FOX, 0, 0);
        }
        return makeCell(EMPTY, 0, 0);
    }

    Cell result = current;
    int foxArrived = 0;

    for (int direction = 0; direction < 4; direction++) {
        int neighborRow = row + rowOffset[direction];
        int neighborColumn = column + columnOffset[direction];

        if (!isInsideWorld(neighborRow, neighborColumn)) {
            continue;
        }
        Cell neighbor = before[cellIndex(neighborRow, neighborColumn)];
        if (neighbor.type != FOX) {
            continue;
        }
        if (foxDestination(before, neighborRow, neighborColumn, generation, &eatsRabbit) != index) {
            continue;
        }

        int arrivingAge = neighbor.procreationAge + 1;
        if (arrivingAge > config.genProcFoxes) {
            arrivingAge = 0;
        }
        int arrivingHunger = eatsRabbit ? 0 : neighbor.hunger + 1;

        int arrivingIsOlder = arrivingAge > result.procreationAge;
        int sameAgeButLessHungry = arrivingAge == result.procreationAge && arrivingHunger < result.hunger;
        if (!foxArrived || arrivingIsOlder || sameAgeButLessHungry) {
            result = makeCell(FOX, arrivingAge, arrivingHunger);
            foxArrived = 1;
        }
    }
    return result;
}

static void moveRabbits(const Cell *before, Cell *after, int generation) {
    #pragma omp parallel for schedule(dynamic, 4)
    for (int row = 0; row < config.rows; row++) {
        for (int column = 0; column < config.columns; column++) {
            after[cellIndex(row, column)] = nextCellAfterRabbitsMove(before, row, column, generation);
        }
    }
}

static void moveFoxes(const Cell *before, Cell *after, int generation) {
    #pragma omp parallel for schedule(dynamic, 4)
    for (int row = 0; row < config.rows; row++) {
        for (int column = 0; column < config.columns; column++) {
            after[cellIndex(row, column)] = nextCellAfterFoxesMove(before, row, column, generation);
        }
    }
}

static void printGeneration(const Cell *world, int generation) {
    const char symbols[] = {' ', '*', 'R', 'F'};
    int rabbits = 0;
    int foxes = 0;

    fprintf(stderr, "Gen %d\n", generation);
    for (int row = 0; row < config.rows; row++) {
        fputc('|', stderr);
        for (int column = 0; column < config.columns; column++) {
            Cell cell = world[cellIndex(row, column)];
            fputc(symbols[cell.type], stderr);
            if (cell.type == RABBIT) rabbits++;
            if (cell.type == FOX) foxes++;
        }
        fputs("|\n", stderr);
    }
    fprintf(stderr, "rabbits=%d foxes=%d\n", rabbits, foxes);
}

static void printFinalState(const Cell *world) {
    const char *names[] = {"EMPTY", "ROCK", "RABBIT", "FOX"};
    int totalCells = config.rows * config.columns;
    int objectCount = 0;

    for (int index = 0; index < totalCells; index++) {
        if (world[index].type != EMPTY) {
            objectCount++;
        }
    }

    printf("%d %d %d 0 %d %d %d\n", config.genProcRabbits, config.genProcFoxes, config.genFoodFoxes,
           config.rows, config.columns, objectCount);

    for (int index = 0; index < totalCells; index++) {
        if (world[index].type != EMPTY) {
            printf("%s %d %d\n", names[world[index].type], index / config.columns, index % config.columns);
        }
    }
}

static int objectTypeFromName(const char *name) {
    if (strcmp(name, "ROCK") == 0) return ROCK;
    if (strcmp(name, "RABBIT") == 0) return RABBIT;
    if (strcmp(name, "FOX") == 0) return FOX;
    return EMPTY;
}

int main(int argc, char **argv) {
    int verbose = argc > 1 && strcmp(argv[1], "-v") == 0;
    int objectCount;

    int fieldsRead = scanf("%d %d %d %d %d %d %d", &config.genProcRabbits, &config.genProcFoxes,
                           &config.genFoodFoxes, &config.numberOfGenerations, &config.rows,
                           &config.columns, &objectCount);
    if (fieldsRead != 7 || config.rows <= 0 || config.columns <= 0) {
        fprintf(stderr, "Error: the first line must have 7 integers and R, C must be positive.\n");
        return 1;
    }

    size_t totalCells = (size_t)config.rows * config.columns;
    Cell *world = calloc(totalCells, sizeof(Cell));
    Cell *buffer = calloc(totalCells, sizeof(Cell));
    if (world == NULL || buffer == NULL) {
        fprintf(stderr, "Error: not enough memory for a %d x %d world.\n", config.rows, config.columns);
        return 1;
    }

    for (int line = 0; line < objectCount; line++) {
        char name[16];
        int row, column;

        if (scanf("%15s %d %d", name, &row, &column) != 3) {
            fprintf(stderr, "Error: object line %d must look like 'FOX 3 4'.\n", line + 2);
            return 1;
        }
        int type = objectTypeFromName(name);
        if (type == EMPTY) {
            fprintf(stderr, "Error: unknown object '%s' on line %d.\n", name, line + 2);
            return 1;
        }
        if (!isInsideWorld(row, column)) {
            fprintf(stderr, "Error: position (%d, %d) on line %d is outside the world.\n", row, column, line + 2);
            return 1;
        }
        world[cellIndex(row, column)] = makeCell(type, 0, 0);
    }

    if (verbose) {
        printGeneration(world, 0);
    }

    for (int generation = 0; generation < config.numberOfGenerations; generation++) {
        moveRabbits(world, buffer, generation);
        moveFoxes(buffer, world, generation);
        if (verbose) {
            printGeneration(world, generation + 1);
        }
    }

    printFinalState(world);

    free(world);
    free(buffer);
    return 0;
}
