# Simulación de ecosistema: zorros, conejos y rocas

Proyecto 300CIP013. Simula un ecosistema en una matriz de R × C celdas donde viven conejos y zorros, y donde hay rocas. Incluye:

- `ecosystem.c`: el simulador. Se compila en versión secuencial y en versión paralela (OpenMP) a partir del mismo fichero.
- `ecosystem.html`: visualización interactiva que usa las mismas reglas.
- Herramientas de prueba: `ex1.in`, `ex1.out`, `check_engine.js` y `gen.py`.

## Ficheros

| Fichero | Para qué sirve |
|---|---|
| `ecosystem.c` | Simulador. Lee la entrada por `stdin` y escribe el estado final por `stdout`. |
| `Makefile` | `make` compila las dos versiones, `make test` las comprueba y `make clean` borra los binarios. |
| `ecosystem.html` | Visualización. Se abre con doble clic y no necesita servidor. |
| `ex1.in`, `ex1.out` | Ejemplo del enunciado y su salida esperada. |
| `check_engine.js` | Ejecuta el motor de `ecosystem.html` con Node para compararlo con el programa en C. |
| `gen.py` | Genera mundos aleatorios grandes para pruebas y medidas de rendimiento. |

## Compilar

```sh
make            # crea ./ecosystem (secuencial) y ./ecosystem-omp (paralelo)
make test       # comprueba las dos versiones y el motor del HTML con el ejemplo del enunciado
```

- **macOS:** el compilador de Apple no trae OpenMP. Instala la biblioteca con `brew install libomp`. El `Makefile` la busca en `/opt/homebrew/opt/libomp`; si está en otra ruta, usa `make OMP=/ruta/a/libomp`.
- **Linux:** con `gcc` funciona directamente (`-fopenmp`).

## Ejecutar

```sh
./ecosystem < ex1.in                       # versión secuencial
OMP_NUM_THREADS=8 ./ecosystem-omp < ex1.in # versión paralela con 8 hilos
./ecosystem -v < ex1.in                    # además muestra cada generación por stderr
```

Con `-v` se imprime la matriz de cada generación (`*` roca, `R` conejo, `F` zorro) y el número de conejos y zorros, con el mismo formato que la tabla del enunciado.

### Formato de entrada y salida

La primera línea contiene, por este orden:

```
GEN_PROC_RABBITS GEN_PROC_FOXES GEN_FOOD_FOXES N_GEN R C N
```

Después vienen `N` líneas con la forma `OBJETO X Y`. `OBJETO` es `ROCK`, `RABBIT` o `FOX`; `X` es la fila e `Y` la columna, contando desde 0.

La salida usa el mismo formato que la entrada, con `N_GEN = 0`. Los objetos se listan en orden de filas.

## Reglas implementadas

En cada generación `G` (la primera es la 0) se mueven primero todos los conejos y después todos los zorros. Dentro de cada grupo los movimientos son simultáneos: todos deciden mirando el estado anterior al movimiento de su grupo.

**Elección de celda.** Se recorren los vecinos en sentido horario: norte, este, sur y oeste. Solo se cuentan las `P` celdas que cumplen la condición (vacías, o con conejo cuando un zorro busca comida). Se elige la número `(G + X + Y) mod P`.

**Conejos.**
- Se mueven a una celda vecina vacía. Si no hay ninguna, se quedan donde están.
- Su edad de reproducción aumenta en 1 en cada generación.
- Si se mueven y esa edad ya es mayor que `GEN_PROC_RABBITS`, dejan una cría en la celda de origen y las edades de los dos pasan a 0.

**Zorros.**
- Si tienen un conejo al lado, se lo comen: ocupan su celda y su hambre vuelve a 0.
- Si no hay conejo, su hambre aumenta en 1. Si llega a `GEN_FOOD_FOXES`, mueren antes de intentar moverse.
- Si siguen vivos, se mueven a una celda vacía, o se quedan donde están si no hay ninguna.
- Se reproducen igual que los conejos, pero con `GEN_PROC_FOXES`.

**Conflictos.** Si varios animales llegan a la misma celda:
- Entre conejos sobrevive el de mayor edad de reproducción.
- Entre zorros sobrevive el de mayor edad de reproducción y, si empatan, el menos hambriento.

La edad que se compara es la que tiene cada animal después de moverse, es decir, 0 si acaba de reproducirse.

**Rocas.** No se mueven y nadie puede ocupar su celda.

### Decisiones de interpretación

El enunciado deja dos detalles abiertos. Los resolvimos así, y el ejemplo del enunciado sale idéntico generación a generación:

- "Han pasado `GEN_PROC` generaciones" se interpreta como que la edad, ya incrementada en esta generación, es **mayor** que `GEN_PROC`. Con "mayor o igual" el ejemplo no coincide.
- Un animal que no puede moverse no se reproduce, pero su edad sigue aumentando.

## Implementación paralela

En lugar de mover cada animal y escribir en su celda de destino (*push*), cada celda de la nueva matriz calcula su propio contenido (*pull*):

- **Una celda ocupada** decide si su animal se queda, se va, se va dejando una cría o muere de hambre.
- **Una celda libre** (o con un conejo, en la fase de zorros) mira a sus cuatro vecinos y comprueba cuáles se moverían hacia ella. Si llega más de uno, aplica la regla de conflictos.

Cada celda nueva depende solo de la matriz anterior y escribe solo en sí misma, así que:

- no hay condiciones de carrera ni hacen falta cerrojos o regiones críticas;
- no hay que intercambiar animales entre las zonas de distintos hilos al final de cada generación;
- el resultado es idéntico con cualquier número de hilos y en cualquier orden.

Cada fase es un `#pragma omp parallel for` sobre las filas, con `schedule(dynamic, 4)`. Así el reparto se equilibra solo cuando hay zonas con muchos animales y zonas casi vacías, que es la mejora adaptativa que sugiere el enunciado. La barrera implícita al final de cada bucle separa la fase de conejos de la de zorros.

El precio es que el destino de un animal puede calcularse hasta cuatro veces, una por cada celda vecina que lo consulta. A cambio no hay sincronización dentro de la fase. Sin OpenMP el `#pragma` se ignora y el mismo código es la versión secuencial.

### Rendimiento

Mundo de 1500 × 1500 durante 300 generaciones, generado con `python3 gen.py 1500 1500 300 0.3 2`, en un Mac de 8 núcleos:

| Hilos | Tiempo | Aceleración |
|---|---|---|
| 1 | 25,0 s | 1,0× |
| 2 | 12,7 s | 2,0× |
| 4 | 7,5 s | 3,3× |
| 8 | 5,7 s | 4,4× |

Para repetir la medida:

```sh
python3 gen.py 1500 1500 300 0.3 2 > med.in
for t in 1 2 4 8; do /usr/bin/time -p env OMP_NUM_THREADS=$t ./ecosystem-omp < med.in > /dev/null; done
```

## Verificación

- `make test` comprueba que las versiones secuencial y paralela, y el motor de `ecosystem.html`, reproducen exactamente `ex1.out`.
- En un mundo aleatorio de 2000 × 2000 durante 500 generaciones, la versión paralela con 1, 4 y 8 hilos dio una salida idéntica byte a byte a la secuencial.
- El motor del HTML coincide con el programa en C en mundos aleatorios. Para comprobarlo:

```sh
python3 gen.py 120 90 300 0.4 1 > t.in
node check_engine.js < t.in | diff - <(./ecosystem < t.in) && echo iguales
```

`gen.py` recibe `filas columnas generaciones densidad semilla`. Coloca un objeto en cada celda con la probabilidad indicada por la densidad: 50 % conejos, 25 % zorros y 25 % rocas. Usa `GEN_PROC_RABBITS = 3`, `GEN_PROC_FOXES = 8` y `GEN_FOOD_FOXES = 5`.

`check_engine.js` lee el bloque `<script id="engine">` de `ecosystem.html`, simula el fichero de entrada con Node y escribe el resultado con el mismo formato que el programa en C.

## Visualización (`ecosystem.html`)

Se abre con doble clic en cualquier navegador. Las fuentes se cargan de Google Fonts; sin internet se usan las del sistema y todo sigue funcionando. Cubre los cuatro puntos de *Implementation Requirements*:

- **Mapa del ecosistema:** cada especie tiene forma y color propios (conejo ●, zorro ▲, roca ■). Los bordes muestran las coordenadas de fila y columna, con el origen (0, 0) arriba a la izquierda como en el enunciado.
- **Estadísticas:** contadores de la generación actual, los conejos, los zorros y las rocas, y una gráfica con la población de conejos y zorros en cada generación.
- **Parámetros ajustables:**
  - `GEN_PROC_RABBITS`, `GEN_PROC_FOXES` y `GEN_FOOD_FOXES` se pueden cambiar durante la simulación y se aplican en la generación siguiente.
  - El tamaño del mundo, los porcentajes iniciales de cada objeto y la semilla se aplican al pulsar **Crear mundo**. La misma semilla genera siempre el mismo mundo.
- **Control del tiempo:** Reproducir/Pausar, Paso (avanza una generación), Reiniciar (vuelve a la generación 0 del mismo mundo) y velocidad de 1 a 60 generaciones por segundo.

El cuadro **Fichero de entrada** usa el formato del enunciado:

- **Cargar** lee el texto del cuadro y pausa la simulación en su `N_GEN`. Viene relleno con el ejemplo del PDF, así que con Cargar y Reproducir se ve el ejemplo generación a generación.
- **Exportar estado actual** escribe en el cuadro el estado actual con el formato de salida del programa en C.

Los valores por defecto (2 / 8 / 6, mundo de 80 × 100 con 20 % de conejos, 3 % de zorros y 8 % de rocas) producen los ciclos de población del apartado *Expected Outcome*: cuando hay muchos zorros los conejos caen, luego los zorros pasan hambre y los conejos se recuperan.

### Cómo está organizado el código del HTML

- `<script id="engine">` contiene la simulación. Es una traducción directa de `ecosystem.c`: las funciones tienen los mismos nombres (`chooseNeighbor`, `rabbitDestination`, `foxDestination`, `nextCellAfterRabbitsMove`, `nextCellAfterFoxesMove`...) y siguen el mismo método *pull*. También incluye `parseInputFile` y `formatOutput` para leer y escribir el formato del enunciado.
- El segundo `<script>` contiene la interfaz: crear el mundo aleatorio, el temporizador de reproducción, el dibujo del mapa y de la gráfica, y los botones.

# video: https://drive.google.com/drive/folders/1oOSPjMpXzV7KTHt8ligkIxZ4peRDLCxN?usp=drive_link
