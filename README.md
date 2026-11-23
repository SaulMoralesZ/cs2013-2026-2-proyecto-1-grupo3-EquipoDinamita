# EquipoDinamita - Proyecto CS2013_2026-2

Un agente 🤖 recorre un tablero por turnos para llegar a la salida 🏁 antes de quedarse sin
energía o agotar el límite de turnos. Cada acción consume energía; los recursos 💎 suman
puntos, las baterías ⚡ recargan energía y las trampas 💥 la quitan. La partida se puede jugar
con el teclado o con un controlador automático (aleatorio o heurístico).

El motor (`Grid`, `NavigationEnvironment`, reglas, eventos y controladores) no depende de la
interfaz: la consola usa [FTXUI](https://github.com/ArthurSonzogni/FTXUI) y solo traduce teclas
y dibuja el estado.

## 🚀 Configuración y Compilación

### Requisitos
- **CMake** 3.10 o superior
- **Compilador C++20**:
  - Linux: `gcc` / `g++`
  - Windows: Visual Studio 2019+ o MinGW
  - macOS: Clang

### Instalación rápida

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get install cmake g++ build-essential
```

#### Windows
- Descarga Visual Studio Community: https://visualstudio.microsoft.com/
- O instala MinGW: https://www.mingw-w64.org/

#### macOS
```bash
brew install cmake
```

---

## 📋 Compilar el proyecto

### 1. Clonar el repositorio
```bash
git clone https://github.com/SaulMoralesZ/cs2013-2026-2-proyecto-1-grupo3-EquipoDinamita.git
cd cs2013-2026-2-proyecto-1-grupo3-EquipoDinamita
```

### 2. Crear carpeta de compilación
```bash
mkdir build
cd build
```

### 3. Generar archivos de compilación

**Linux/macOS:**
```bash
cmake ..
```

**Windows (Visual Studio):**
```bash
cmake .. -G "Visual Studio 17 2022"
```

**Windows (MinGW):**
```bash
cmake .. -G "MinGW Makefiles"
```

### 4. Compilar
```bash
make
```

O en Windows con Visual Studio:
```bash
cmake --build .
```

### 5. Ejecutar
```bash
./proyecto
```

---
## Compilación

Todos los comandos se ejecutan desde la **raíz del repositorio**.

### Linux y macOS

```bash
git clone https://github.com/SaulMoralesZ/cs2013-2026-2-proyecto-1-grupo3-EquipoDinamita.git
cd cs2013-2026-2-proyecto-1-grupo3-EquipoDinamita
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Dependencias en Ubuntu/Debian: `sudo apt install build-essential cmake git`.
En macOS: `xcode-select --install` y `brew install cmake`.

### Windows (PowerShell o Command Prompt)

Con MinGW (por ejemplo, el que incluye CLion) o con Visual Studio 2022 instalado:

```powershell
git clone https://github.com/SaulMoralesZ/cs2013-2026-2-proyecto-1-grupo3-EquipoDinamita.git
cd cs2013-2026-2-proyecto-1-grupo3-EquipoDinamita
cmake -S . -B build
cmake --build build --config Release
```

- Con MinGW, las DLLs del compilador (`libstdc++-6.dll`, `libgcc_s_seh-1.dll`,
  `libwinpthread-1.dll`) se copian automáticamente junto a cada `.exe`, así que el juego
  funciona fuera del IDE.
- Con Visual Studio los ejecutables quedan en `build\Release\` en lugar de `build\`.

### CLion

1. Abrir la carpeta del repositorio (CLion detecta `CMakeLists.txt`).
2. *Run → Edit Configurations → navigation_game → Working directory* = `$ProjectFileDir$`
   (el juego busca los mapas en `assets/maps/` relativo a la carpeta de trabajo).
3. Compilar con *Build → Build Project*. La carpeta de compilación es `cmake-build-debug/`.
### Solo el motor, sin interfaz

Para compilar y probar el núcleo sin descargar FTXUI:

```bash
cmake -S . -B build-core -DCIRCUIT_ESCAPE_WITH_UI=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

Esto demuestra que `Grid`, `NavigationEnvironment`, eventos y controladores no dependen de FTXUI.

---
## Pruebas

```bash
ctest --test-dir build --output-on-failure
```

Con Visual Studio agregar `-C Release`. Con CMake anterior a 3.20: `cd build` y luego
`ctest --output-on-failure`. Si en Windows `ctest` no se reconoce como comando, usar la
terminal integrada de CLion o agregar la carpeta `bin` de CMake al `PATH`.

| Ejecutable | Qué verifica |
|---|---|
| `grid_test` | dimensiones válidas (3×4 a 40×60) e inválidas (0, sobre el límite), acceso válido e inválido, vecindad en los cuatro bordes, recorrido por filas, algoritmos genéricos con rangos vacíos y con `vector` y `list` |
| `scenario_test` | `Scenario` válido; rechazo de salidas, posiciones, celdas y reglas inválidas; `reset`; `entry_cost`; orden de `Position`; lectura de archivos con fin de línea CRLF |
| `turn_test` | turno en tres métodos y su orden obligatorio, vigilante solo en turnos pares, A y B en ambos órdenes, salida sin A y B, capturas, energía 0 en la salida, último turno, determinismo |
| `observation_test` | la observación trae mapa, interruptores y posición del vigilante; una política de vigilante adaptada con `PolicyController` persigue y captura |
| `environment_test` | movimiento, costos configurados, acciones disponibles, perfiles, término de la partida y su precedencia |
| `interactions_test` | recursos, baterías, trampas y eventos generados |
| `controllers_test` | controladores aleatorio y heurístico, reproducibilidad con la misma semilla |
| `ui_test` | traducción de teclas sin distinguir mayúsculas, rechazo de comandos desconocidos sin modificar el entorno, glifos emoji y ASCII (celdas, vigilante e interruptores), tableros de cualquier tamaño, ayuda H sin avanzar turno, tamaño mínimo de terminal |

`ui_test` se ejecuta sin abrir una terminal interactiva: renderiza en una pantalla de FTXUI en
memoria.

---

## Ejecución

Siempre desde la **raíz del repositorio**:

| Sistema | Comando |
|---|---|
| Linux / macOS | `./build/navigation_game` |
| PowerShell | `.\build\navigation_game.exe` |
| Command Prompt | `build\navigation_game.exe` |

### Opciones

| Opción | Descripción | Valor por defecto |
|---|---|---|
| `--scenario <ruta>` | mapa a cargar | `assets/maps/scenario_01.txt` |
| `--difficulty easy\|standard\|hard` | perfil de reglas | `standard` |
| `--ascii` | presentación sin emojis | modo emoji |
| `--controller random\|heuristic` | juega una política automática | jugador humano |
| `--seed <número>` | semilla de la política aleatoria | `42` |
| `--help` | muestra la ayuda | — |


### Ejemplos

```bash
# Partida humana estándar con emojis
./build/navigation_game
 
# Partida difícil en el segundo escenario
./build/navigation_game --difficulty hard --scenario assets/maps/scenario_02.txt
 
# Terminal sin soporte de emojis
./build/navigation_game --ascii
 
# Controlador heurístico
./build/navigation_game --controller heuristic
 
# Controlador aleatorio reproducible: la misma semilla produce la misma partida
./build/navigation_game --controller random --seed 7
```

En modo `--controller`, cada tecla de juego (WASD, flechas o E) avanza un turno del
controlador automático; Q sale.

Al salir, el programa imprime un resumen: resultado, turnos, energía restante, recursos y
puntaje.

### Emojis en Windows

La vista emoji necesita una terminal moderna con fuente de emojis y ~80 columnas de ancho.
En Windows se recomienda **Windows Terminal** (incluido en Windows 11). La consola clásica
puede mostrar cuadros vacíos o desalinear las coordenadas; en ese caso usar `--ascii`.
 
---
## Controles

| Tecla | Acción |
|---|---|
| `W` / ↑ | mover arriba |
| `S` / ↓ | mover abajo |
| `A` / ← | mover a la izquierda |
| `D` / → | mover a la derecha |
| `E` | esperar un turno |
| `H` | abrir o cerrar la ayuda (con la ayuda abierta no pasa ningún turno) |
| `Esc` | cerrar la ayuda; con la ayuda cerrada, abandonar la partida |
| `Q` | abandonar la partida |

Las letras funcionan en mayúscula o minúscula. Una tecla desconocida muestra un aviso y no
consume turno.

### Leyenda

| Emoji | ASCII | Significado |
|---|---|---|
| 🤖 | `@` | agente |
| ⬜ | `.` | espacio libre |
| ⬛ | `#` | muro |
| 🟫 | `~` | terreno de costo elevado |
| 💎 | `R` | recurso |
| ⚡ | `B` | batería |
| 💥 | `T` | trampa |
| 🏁 | `S` | salida |
| 👾 | `G` | vigilante |
| 🔴 | `!` | interruptor pendiente |
| 🟢 | `+` | interruptor activado |

Los tres últimos solo aparecen en partidas con vigilante (Proyecto 2). Una segunda línea bajo la
barra de estado muestra el estado de A y B y si el vigilante se mueve tras tu acción.
 
---
## Reglas y perfiles de dificultad

- Moverse a una celda normal cuesta energía; el terreno elevado cuesta más.
- Esperar o intentar moverse contra un muro o fuera del tablero también cuesta energía y
  consume el turno, sin mover al agente.
- Cada recurso suma puntos una sola vez; cada batería recarga energía una sola vez, sin superar
  el máximo.
- Una trampa resta energía y puntaje cada vez que se pisa.
- **Victoria:** llegar a la salida con energía mayor que 0.
- **Derrota:** quedarse sin energía o alcanzar el límite de turnos.
- Si en el mismo turno se cumplen varias condiciones, la precedencia es: salida, luego energía
  agotada, luego límite de turnos. (Con vigilante y interruptores la precedencia cambia; ver
  [Orden del turno](#orden-del-turno-con-vigilante).)
  | Parámetro | easy | standard | hard |
  |---|---|---|---|
  | Energía inicial y máxima | 80 | 60 | 40 |
  | Límite de turnos | 240 | 180 | 140 |
  | Costo de movimiento normal | 1 | 1 | 1 |
  | Costo de terreno elevado | 2 | 2 | 3 |
  | Costo de esperar | 1 | 1 | 1 |
  | Costo de intento inválido | 1 | 1 | 1 |
  | Puntos por recurso | 15 | 10 | 8 |
  | Recarga por batería | 5 | 3 | 2 |
  | Trampa: energía perdida | 1 | 2 | 3 |
  | Trampa: puntaje perdido | 0 | 1 | 2 |

Los valores están en `include/circuit_escape/game_rules.hpp` (`rulesFor`).
 
---
## Formato de los mapas

Los escenarios están en `assets/maps/` como texto plano, una fila por línea:

| Carácter | Celda |
|---|---|
| `#` | muro |
| `.` | libre |
| `~` | terreno elevado |
| `R` | recurso |
| `B` | batería |
| `T` | trampa |
| `S` | salida (exactamente una) |
| `@` | posición inicial del agente (exactamente una) |

---


## 📁 Estructura del proyecto

```
├── CMakeLists.txt
├── README.md
├── app/main.cpp                  # aplicación: opciones, ciclo de FTXUI
├── include/circuit_escape/       # cabeceras (los templates viven aquí)
├── src/                          # implementaciones no-template
├── tests/                        # pruebas ejecutadas con CTest
├── assets/maps/                  # escenarios
└── docs/
    ├── design.md                 # decisiones de diseño
    └── contributions.md          # aportes de cada integrante
```

---

## Reutilización del Proyecto 1 (Bloque 1)

Qué se conservó del Proyecto 1, cómo se adaptó, dónde se usa hoy y qué prueba lo verifica.

| Componente original | Adaptación | Uso actual | Prueba |
|---|---|---|---|
| `Grid<CellType, Rows, Columns>` | Filas y columnas pasan al constructor; `std::array` → `std::vector`; se conservan `Position`, `neighbor`, `at`/`operator()` e iteradores; se valida `0 < lado <= max_side` | Tablero de cualquier tamaño en el motor y la consola | `grid_test` |
| `Position` | Se conserva `{row, column}`; se añade `operator<=>` | Desempates y contenedores ordenados | `scenario_test` |
| `countCells`, `findFirstCell`, `findPosition` | Sin cambio de firma | El constructor del motor cuenta las salidas (`countCells`) y localiza la salida (`findPosition`) | `grid_test`, `scenario_test` |
| `NavigationEnvironment` (`reset`, `state`, `step`) | Ya no es template; se construye desde un `Scenario` validado. El turno se parte en `step_player` / `step_guard` / `finish_turn`; se añaden interruptores A y B, vigilante y captura. `step` se conserva para las pruebas del Proyecto 1 | Partida con A, B y vigilante | `scenario_test`, `turn_test`, `environment_test` |
| `entryCost` (privado) | Pasa a `entry_cost(cell, rules)`, público | El motor; Dijkstra y la ayuda H usarán la misma función | `scenario_test` |
| `EndReason` y `firstEnd` | Se añade `captured`; el orden de comprobaciones pasa a captura → energía → interruptor → escape → límite | Resultado del turno | `turn_test` |
| `Observation` | Añade mapa de solo lectura, estado de A y B y posición del vigilante | Entrada del controlador del vigilante | `observation_test` |
| `IController`, concept `NavigationPolicy`, `PolicyController<Policy>` | Sin cambios de contrato | El vigilante se adapta como política con `PolicyController` (la política BFS está en integración) | `controllers_test`, `observation_test` |
| `ConsoleUI` | Se quitan `Rows`/`Columns`; dibuja vigilante e interruptores; ayuda H que no avanza turno; aviso de terminal pequeña | `app/main.cpp` | `ui_test` |
| `parseScenario` | Devuelve `BoardLayout` (tablero e inicio) para los mapas del Proyecto 1 | `app/main.cpp` mientras se cargan mapas de ese formato | `scenario_test` |

### Nombres heredados y nuevos

Los tipos usan `PascalCase` y las funciones **nuevas** `snake_case` (`step_player`, `finish_turn`,
`guard_moves_this_turn`, `entry_cost`, `player_position`, `resolve_command`…). Los nombres del
Proyecto 1 no se renombran para no romper el código y las pruebas heredadas: `step`, `reset`,
`state`, `availableActions`, `selectAction`, `positionOf`, `countCells`, `findFirstCell`,
`findPosition`, `isFinished`, `endReason`, `rulesFor`.

### Orden del turno con vigilante

Un turno son tres llamadas al motor, en este orden:

1. `step_player(accion)`: el jugador mueve o espera; el motor aplica costos e interacciones y
   comprueba la captura inicial (el jugador pisó al vigilante).
2. `step_guard(accion)`: solo en turnos pares (los turnos se cuentan desde 1). El motor valida
   la acción contra `guard_legal_actions()`; el vigilante no gasta energía del jugador ni
   recoge baterías o recursos.
3. `finish_turn()`: comprobaciones finales, en este orden: **captura**, **energía agotada**,
   **activar el interruptor** de la casilla del jugador, **escape** (en la salida, con A y B
   activos y energía positiva) y **límite de turnos**.

Consecuencias: la captura tiene prioridad sobre el escape; el último turno permitido cuenta (el
escape se comprueba antes del límite); pisar la salida sin A y B no gana ni termina la partida.
El motor lanza `std::logic_error` si las llamadas se hacen fuera de orden.

**Decisión sobre los interruptores.** Se activan en el paso final del turno, solo si ese turno no
termina en derrota. Quién gana o pierde es idéntico a activarlos al entrar; la única diferencia
visible es que, si te capturan en el mismo turno en que pisas A, A queda sin activar. La
activación vive en una sola función (`activate_switch_at`), llamada desde un único lugar.

---

## Decisiones de diseño principales

El detalle está en [`docs/design.md`](docs/design.md). En resumen:

- **Motor separado de la interfaz.** `NavigationEnvironment` es el único que modifica el estado;
  la interfaz solo traduce teclas a `UiCommand` y dibuja un `const NavigationEnvironment&`.
  Por eso el motor y sus pruebas compilan sin FTXUI (`-DCIRCUIT_ESCAPE_WITH_UI=OFF`).
- **Reglas como datos.** Todos los costos, recompensas y límites viven en `GameRules`; los
  perfiles easy, standard y hard son tres valores de ese struct, no ramas `if` en el motor.
- **Celdas y eventos como `std::variant`.** Agregar un tipo obliga a tratarlo en cada
  `std::visit`; el compilador avisa si falta un caso.
- **Controladores intercambiables.** Cualquier tipo que cumpla el concept `NavigationPolicy` se
  adapta a `IController` con `PolicyController<Policy>`, sin `typeid` ni `dynamic_cast`.
- **Entorno determinista.** El azar vive solo en `RandomPolicy`, con semilla configurable, así
  que la misma semilla reproduce la misma partida.
---

## Elección de contenedores

| Contenedor / utilidad | Dónde | Por qué |
|---|---|---|
| `std::vector` | almacenamiento de `Grid` | filas y columnas se eligen al ejecutar (Proyecto 2); memoria contigua y acceso por índice |
| `std::vector` | eventos de `StepResult`, acciones disponibles, líneas del mapa | la cantidad varía en cada turno y solo se agrega al final |
| `std::deque` | eventos recientes en `app/main.cpp` | ventana de los últimos eventos: se agrega al final y se descarta al inicio en O(1) |
| `std::span` | parámetros `legalActions` y `recentEvents` | vista sin copia sobre un vector existente |
| `std::variant` + `std::visit` | `Cell`, `NavigationEvent`, `UiCommand` | un valor es exactamente uno de varios tipos conocidos, sin herencia ni punteros |
| `std::optional` | `neighbor`, `findPosition`, `translate`, lectura de opciones | operaciones que pueden no tener resultado, sin valores especiales |
| `std::unordered_map` | celdas visitadas en `HeuristicPolicy` | consulta en O(1) promedio por posición, con `PositionHash` propio |
| `std::unique_ptr` | `IController` creado por `makeController` | propiedad única de un objeto polimórfico, liberado automáticamente (RAII) |
| `<random>` (`std::mt19937`) | `RandomPolicy` | generador con semilla controlable para reproducir partidas |
| `<algorithm>` | `std::clamp` de la energía, `std::for_each`, `std::copy_if` | algoritmos estándar en lugar de bucles repetidos |
 
---
## Ubicación de los temas obligatorios

El detalle y la justificación de cada decisión están en [`docs/design.md`](docs/design.md).

| Tema | Dónde |
|---|---|
| Template de clase | `Grid<CellType>` en `grid.hpp` (en el Proyecto 1 tenía además `Rows` y `Columns` no-tipo; en el 2 son parámetros del constructor) |
| Templates de función con iteradores | `countCells`, `findFirstCell` en `grid.hpp`; `bestAction` en `controllers.hpp` |
| Template usado con dos contenedores | `countCells` con `std::vector` y `std::list` en `tests/grid_test.cpp` |
| Especialización total | `CellTraits<Wall>`, `CellTraits<Battery>` en `cells.hpp` |
| Especialización parcial | `CellTraits<ResourceCell<Reward>>` en `cells.hpp` |
| Templates variádicos y fold expressions | `Overloaded` en `cells.hpp`; `publish` y `firstEnd` en `environment.hpp` |
| `std::variant` y `std::visit` | `Cell` (`cells.hpp`), `NavigationEvent` (`events.hpp`), `UiCommand` (`console_ui.hpp`) |
| Concept | `NavigationPolicy` en `controllers.hpp` |
| Polimorfismo dinámico | `IController` y `PolicyController<Policy>` en `controllers.hpp` |
| Smart pointers | `std::unique_ptr<IController>` en `makeController` y `app/main.cpp` |
| Prueba negativa del concept | TODO (pendiente) |
 
---
## Limitaciones conocidas

- La vista emoji depende de la fuente y de la terminal; en consolas antiguas usar `--ascii`.
- Cada celda ocupa 2 columnas en ambos modos: un tablero de 40×60 necesita una terminal de al
  menos 122×43 (122×44 con vigilante). Si es menor, el juego lo avisa y no deja jugar.
- El programa debe ejecutarse desde la raíz del repositorio para encontrar los mapas por defecto.


---

## 📚 Librerías

- **FTXUI v7.0.3**: Interfaz de terminal interactiva (se descarga automáticamente)
  o con sudo apt install libftxui-dev (Ubuntu/debian base)
- **C++20**: Estándar de lenguaje usado en el proyecto

---

## 📊 Documentación con Excalidraw

### 🎨 ¿Qué es Excalidraw?

Excalidraw es una herramienta para crear diagramas y documentar visualmente nuestro código. Cada miembro del equipo puede:
- 🎯 Dibujar diagramas de arquitectura, flujos, componentes, etc.
- 🎨 Usar su propio estilo y creatividad
- 📝 Explicar sus diagramas con total libertad

### 📍 Acceder al Excalidraw compartido

**Link de la sesión compartida:**
```
https://excalidraw.com/#json=Eu9n-74ERvm1Kk6g4Lux8,flYbTyMF9DIFenXuZSylww
```

**Cómo usarlo:**
1. Abre el link arriba
2. Dibuja, documenta y explica tu parte del código
3. Cuando termines, **descarga el archivo** como `.excalidraw`
4. Renómbralo a `documentacion_grafico.excalidraw`
5. Sube el archivo al repositorio en la carpeta `docs/`

### 💾 Guardar los diagramas localmente

Es importante guardar los diagramas en el repo para tener un histórico y que todos puedan acceder:

```bash
# Después de descargar el archivo de Excalidraw
mv archivo_descargado.excalidraw documentacion_grafico.excalidraw
git add docs/documentacion_grafico.excalidraw
git commit -m "docs: Update graphic documentation diagrams"
git push origin main
```

---

## 🐛 Solución de problemas

### CMake no encontrado
```bash
cmake --version
```
Si no aparece, instala CMake según tu SO.

### Errores de compilación
- Asegúrate de tener C++20 o superior
- Linux: `g++ --version` debe ser >= 9.0
- Windows: Usa Visual Studio 2019 o superior

### FTXUI no descarga
```bash
# Limpia la carpeta build e intenta de nuevo
rm -rf build
mkdir build && cd build
cmake ..
```

---

## 👥 Contribuyentes
- Saul Morales
- Rafael Vargas
- Treicy Calsina

---
