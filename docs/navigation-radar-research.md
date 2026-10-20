# Investigación e implementación de un radar fiable usando el mapa de colisiones de Diablo II: Resurrected

## Contexto

Estamos desarrollando un mod de accesibilidad para **Diablo II: Resurrected (D2R)**, cargado dentro del juego mediante nuestro sistema actual basado en **D2R Loader**.

Ya tenemos varias partes del mod funcionando, pero el radar de navegación todavía falla demasiado al intentar determinar:

- si delante del jugador hay espacio abierto;
- si una dirección está realmente cerrada;
- dónde termina una habitación;
- dónde comienza un pasillo;
- si un espacio aparentemente libre está conectado con el jugador;
- si hay una pared parcial, obstáculo o geometría que hace que un simple muestreo/raycast produzca resultados incorrectos.

El enfoque actual basado en probes/raycasts/consultas puntuales de colisión no parece suficientemente fiable.

El objetivo de esta tarea es **investigar una fuente de geometría más autoritativa y construir una representación navegable del mapa**, preferiblemente aprovechando datos internos de D2R o reconstruyendo la misma información que utiliza el automapa.

---

# Objetivo principal

Encontrar la forma más fiable de obtener en runtime una representación del terreno que permita responder:

> Desde la posición actual del jugador, ¿qué celdas son realmente transitables y cuáles están conectadas entre sí?

No queremos limitar el sistema a:

> ¿Existe colisión exactamente en este punto?

Necesitamos poder razonar sobre **regiones conectadas**, pasillos, habitaciones, paredes y salidas.

La solución final debería permitir construir algo equivalente a:

```text
###########
#.........#
#.........#
#####..####
#.........#
###########
```

donde:

- `#` = no transitable / bloqueado;
- `.` = transitable;

o, mejor todavía, una cuadrícula donde cada celda indique hacia qué celdas vecinas se puede mover realmente.

---

# Repositorios de referencia prioritarios

## 1. d2mapapi_mod

Repositorio:

https://github.com/soarqin/d2mapapi_mod

Archivos especialmente importantes:

```text
collisionmap.cpp
collisionmap.h
mapdata.cpp
mapdata.h
pathfinder.cpp
pathfinder.h
d2map.cpp
d2map.h
d2structs.h
d2ptrs.h
offset.cpp
offset.h
```

No asumir que este proyecto puede integrarse directamente en D2R.

`d2mapapi_mod` genera mapas utilizando código/datos de **Diablo II Legacy** y su README indica soporte para versiones clásicas como 1.11/1.12/1.13.

Lo queremos principalmente como referencia para entender:

- representación de collision maps;
- transformación de los datos del mapa a una cuadrícula navegable;
- conectividad entre tiles;
- detección de bordes;
- pathfinding;
- coordenadas;
- representación de salidas y objetos.

---

## 2. D2RMH

Repositorio:

https://github.com/soarqin/D2RMH

Estudiar especialmente:

- cómo obtiene desde D2R:
  - game seed;
  - difficulty;
  - current area / level;
  - player position;
  - información necesaria para generar el mapa;
- cómo conecta D2R con `d2mapapi_mod`;
- cómo representa `walkable area`;
- cómo detecta bordes;
- cómo genera caminos hacia objetivos;
- cómo transforma coordenadas del mapa a coordenadas de D2R.

Revisar también:

```text
doc/ChangeLog.md
```

porque el proyecto añadió explícitamente:

- walkable area;
- edge detection;
- BFS pathfinding;
- walkable path to target.

Importante:

**No copiar la arquitectura completa de D2RMH ni convertir nuestro mod en un maphack.**

El objetivo es aprender cómo obtiene y representa el terreno.

---

## 3. MapAssist

Repositorio:

https://github.com/ResurrectedTrader/MapAssist

Investigar principalmente:

- estructuras de memoria de D2R;
- cómo obtiene:
  - player;
  - act;
  - room;
  - level;
  - seed;
  - position;
  - area ID;
- transformaciones de coordenadas;
- offsets/pattern scanning;
- generación de mapa;
- identificación de exits / adjacent areas.

MapAssist fue una referencia utilizada por D2RMH para offsets e ideas de lectura de memoria.

No asumir que offsets antiguos continúan válidos.

Buscar siempre la lógica conceptual y comprobarla contra nuestra versión actual de D2R.

---

# Parte especialmente importante: `pathfinder.cpp`

Analizar cuidadosamente:

https://github.com/soarqin/d2mapapi_mod/blob/master/pathfinder.cpp

La función:

La búsqueda usa `pathFindBFS` en `pathfinder.cpp`.

recibe:

El mapa de `pathfinder.cpp` se consulta como una secuencia de bytes; cada celda conserva sus permisos direccionales.

y cada celda utiliza bits de dirección.

El código itera:

En `pathfinder.cpp`, la búsqueda examina los permisos 1, 2, 4 y 8 por separado.

y comprueba:

La búsqueda de `pathfinder.cpp` descarta una dirección cuando el byte de la celda no contiene su permiso correspondiente.

La interpretación aparente es:

```text
1 = movimiento hacia izquierda
2 = movimiento hacia derecha
4 = movimiento hacia arriba
8 = movimiento hacia abajo
```

Verificar esto en el código completo y documentar exactamente cómo se genera ese `mapData`.

Esto es importante porque para nuestro radar es mucho mejor tener:

```text
celda (x,y)
 ├── izquierda accesible
 ├── derecha accesible
 ├── arriba accesible
 └── abajo accesible
```

que simplemente:

```text
celda bloqueada / celda libre
```

---

# Primera misión: entender nuestro radar actual

Antes de modificar nada:

1. Localiza todo el código actual del radar.
2. Documenta cómo determina actualmente:
   - posición del jugador;
   - orientación;
   - tiles/celdas alrededor;
   - colisión;
   - abierto/cerrado;
   - distancia;
   - actualización del radar.
3. Identifica exactamente por qué puede generar:
   - falsos abiertos;
   - falsos cerrados;
   - fluctuaciones;
   - resultados contradictorios entre frames.
4. No reemplaces todavía el sistema.
5. Escribe primero un resumen técnico de la arquitectura actual.

Busca especialmente situaciones donde el algoritmo haga algo parecido a:

```text
sample N tiles forward
sample NE
sample E
...
```

o utilice raycasts independientes.

---

# Problema conceptual del sistema de probes

Un probe puntual puede producir:

```text
###########
#.........#
#.........#
#####..####
#.........#
###########
```

Si comprobamos únicamente un punto exactamente al norte del jugador, podemos tocar la pared central y concluir:

```text
Norte: cerrado
```

aunque exista un paso conectado desplazado ligeramente hacia un lado.

También puede ocurrir lo contrario:

```text
###########
#....#....#
#....#....#
#....#....#
###########
```

Un probe puede encontrar espacio vacío al otro lado de una pared y concluir:

```text
Este: abierto
```

aunque el jugador no pueda acceder a esa región.

Por eso:

**espacio vacío != espacio alcanzable**

Nuestro radar debería trabajar principalmente con:

**espacio conectado y alcanzable desde el jugador**

---

# Arquitectura deseada

Intentar llegar a una representación interna similar a:

Propuesta de diseño: `NavigationCell` conservaría transitabilidad, permiso de movimiento hacia izquierda/derecha/arriba/abajo y, opcionalmente, las banderas originales de colisión. Es un contrato propuesto en esta investigación, no una declaración de que exista ya ese tipo en producción.

o una representación compacta:

La representación propuesta asigna los bits 1, 2, 4 y 8 a izquierda, derecha, arriba y abajo, respectivamente. Véase `DirectionBits` como nombre del contrato de diseño en esta sección.

con:

La cuadrícula propuesta `navigationGrid` almacenaría un byte de permisos por celda; su implementación debe localizarse en el módulo de navegación que adopte esta propuesta.

No copiar ciegamente esta API; adaptar a la arquitectura existente del proyecto.

---

# Investigar tres posibles fuentes de geometría

Comparar técnicamente estas tres estrategias.

---

## Estrategia A — Collision map interno de D2R

Esta sería potencialmente la mejor opción.

Investigar si D2R mantiene estructuras runtime equivalentes conceptualmente a las estructuras clásicas:

```text
Player
  -> Path
  -> Room
  -> CollisionMap
```

o:

```text
Unit
 -> Room1
 -> Room2
 -> Level
```

No asumir nombres ni offsets exactos.

Buscar estructuras actuales mediante:

- símbolos/patterns existentes en el proyecto;
- referencias conocidas de D2R;
- MapAssist;
- proyectos de reverse engineering;
- funciones utilizadas por D2R para movimiento;
- funciones utilizadas por el automapa;
- funciones que consulten collision flags;
- llamadas relacionadas con rooms / levels / pathing.

### Preguntas que debes responder

1. ¿Existe en memoria una collision grid accesible directamente?
2. ¿Está almacenada por Room?
3. ¿Qué resolución utiliza?
4. ¿Cómo se relacionan sus coordenadas con la posición del jugador?
5. ¿Hay que cargar/unload rooms?
6. ¿Podemos obtener las rooms vecinas?
7. ¿El automapa utiliza estos mismos datos?
8. ¿La collision grid contiene únicamente paredes o también:
   - objetos;
   - puertas;
   - props;
   - unidades;
   - obstáculos temporales?
9. ¿Qué flags representan realmente terreno no transitable?

Si encontramos esta estructura y resulta estable, preferirla sobre reconstruir todo el mapa externamente.

---

## Estrategia B — Reconstrucción determinista usando seed + area

D2RMH obtiene información runtime de D2R y utiliza `d2mapapi_mod` para generar la geometría.

Investigar:

```text
D2R
 ↓
seed
difficulty
area ID
 ↓
map generator
 ↓
CollisionMap
```

Determinar si podemos reproducir esto dentro del mod.

Pero tener en cuenta:

- `d2mapapi_mod` utiliza componentes/estructuras de Diablo II Legacy;
- parte del código es 32-bit;
- nuestro plugin D2R probablemente es 64-bit;
- enlazarlo directamente puede no ser trivial;
- no queremos requerir que el usuario instale manualmente otro programa.

Analizar si sería viable:

### B1. Portar solamente el algoritmo necesario

Extraer/reimplementar limpiamente la lógica imprescindible dentro de nuestro proyecto.

### B2. Biblioteca embebida

Convertir las partes relevantes a una librería compatible con nuestro proceso.

### B3. Helper bundled

Como último recurso, incluir un helper automáticamente con el mod.

Pero evitar esto si podemos trabajar completamente dentro del proceso de D2R.

La experiencia deseada es:

```text
instalar mod
abrir D2R
jugar
```

sin configuración manual complicada.

---

## Estrategia C — Híbrida

Ejemplo:

```text
geometría estática:
    seed + generated map

obstáculos dinámicos:
    collision/runtime queries

posición y orientación:
    datos directos de D2R
```

Esto podría ser útil si la collision map generada representa perfectamente paredes y terreno pero no puertas/objetos dinámicos.

Investigar esta opción explícitamente.

---

# Flood fill / BFS para el radar

Una vez tengamos una grid fiable:

**No clasificar direcciones mirando solamente una línea.**

Desde la celda actual del jugador:

```text
playerCell
   ↓
BFS / flood fill
   ↓
reachableCells
```

Sólo las celdas alcanzables deberían participar en la clasificación de espacio abierto.

Pseudocódigo:

Propuesta: un flood fill desde la celda del jugador obtiene las celdas alcanzables de `navigationGrid`; después se analizan las de cada sector direccional. Esta sección describe el algoritmo, sin reproducir una implementación.

---

# Radar basado en sectores

Podemos dividir el espacio alrededor del jugador en:

```text
N
NE
E
SE
S
SW
W
NW
```

pero no determinar el estado con un único raycast.

Para cada sector calcular, por ejemplo:

```text
reachable cell count
maximum reachable distance
average corridor width
distance to first major obstruction
connected component size
```

Después clasificar.

Ejemplo conceptual:

```text
N:
    reachableTiles = 87
    maxDistance = 24
    corridorWidth = 5
    => OPEN

NE:
    reachableTiles = 8
    maxDistance = 6
    corridorWidth = 1
    => NARROW / LIMITED

E:
    reachableTiles = 0
    => BLOCKED
```

No implementar necesariamente estas métricas exactas hasta comprobar qué representación obtenemos.

---

# Importante: orientación del jugador

Separar claramente:

```text
World directions:
N / NE / E / ...

Relative directions:
front
front-right
right
back-right
...
```

La grid debería mantenerse en coordenadas del mundo.

La conversión a radar relativo debe hacerse al final usando la orientación del jugador.

No rotar/reconstruir toda la grid cada frame.

---

# Resolución

Determinar exactamente qué significa un "tile" en cada sistema:

- posición de Unit/Player;
- coordenadas de Room;
- coordenadas de Level;
- collision map;
- automap;
- d2mapapi.

Documentar transformaciones como:

```text
worldX/worldY
    ↓
room-local coordinates
    ↓
collision cell
```

No asumir que todas las APIs trabajan con la misma escala.

Muchos bugs de radar pueden venir de:

- división incorrecta;
- offsets de room;
- redondeo;
- coordenadas absolutas vs relativas;
- escala 1:5 / 5:1;
- origen diferente.

Crear funciones explícitas como:

El contrato propuesto necesita conversiones explícitas entre mundo y colisión y entre mundo y habitación: `WorldToCollision`, `CollisionToWorld` y `WorldToRoom`. Estos son nombres de diseño, no funciones verificadas aquí.

si corresponde.

---

# Rooms

Investigar a fondo el sistema de rooms.

Posible problema:

La collision map de la room actual puede no ser suficiente para un radar de largo alcance.

Necesitamos saber:

- cómo encontrar rooms adyacentes;
- cuáles están cargadas;
- cuándo se generan;
- si es seguro consultar rooms todavía no activas;
- cómo unir grids de varias rooms;
- cómo manejar cambios de área.

Idealmente construir:

```text
LevelNavigationMap
```

en lugar de depender solamente de:

```text
CurrentRoomCollision
```

---

# Cambio de nivel / área

Detectar explícitamente:

```text
area ID changed
seed changed
difficulty changed
game changed
```

Cuando ocurra:

```text
invalidate navigation map
rebuild
```

No reconstruir todo innecesariamente en cada frame.

---

# Cache

Diseñar cache por:

```text
game/seed
difficulty
areaId
```

o equivalente.

El radar debería consultar una estructura ya procesada:

La consulta propuesta `NavigationMap::GetCell` centraliza el acceso a una celda por coordenadas; las claves y la invalidación se describen alrededor de esta sección.

y no regenerar geometría constantemente.

---

# Antispam / estabilidad

El nuevo sistema debe ayudarnos también con el problema de antispam.

El estado del radar no debería fluctuar:

```text
OPEN
BLOCKED
OPEN
BLOCKED
OPEN
```

porque un probe cambia ligeramente de tile.

Guardar un estado semántico por dirección:

El estado propuesto `RadarSpaceState` distingue desconocido, bloqueado, estrecho y abierto. La estabilidad temporal descrita aquí decide cuándo se anuncia un cambio.

y emitir audio/TTS únicamente cuando haya un cambio real suficientemente estable.

Considerar hysteresis:

```text
nuevo estado debe mantenerse X frames/ms
```

antes de anunciarlo.

Pero:

**primero arreglar la geometría.**

No intentar esconder errores geométricos mediante debounce.

---

# Objetivos de accesibilidad

Este radar no pretende jugar automáticamente.

NO queremos:

```text
"Ve 13 metros al norte, luego 8 al este..."
```

como navegación obligatoria.

Queremos dar al jugador información espacial suficiente para explorar manualmente.

Prioridades:

1. espacio delante;
2. paredes;
3. pasillos;
4. aperturas;
5. puertas/salidas;
6. entradas a otras áreas;
7. elementos relevantes;
8. exploración de mayor alcance.

El jugador seguirá tomando las decisiones de movimiento.

---

# Posible radar de corto y largo alcance

Si la collision map funciona bien, diseñar dos conceptos separados.

## Radar inmediato

Radio pequeño:

```text
3-10 tiles
```

Detecta:

- pared;
- esquina;
- apertura;
- pasillo;
- puerta;
- cambio fuerte de anchura.

## Radar de exploración

Radio mayor:

```text
20-40+ tiles
```

Analiza regiones conectadas.

Podría responder:

```text
Norte: área abierta
Este: pasillo
Sureste: salida de área
Oeste: cerrado
```

No implementar mensajes definitivos todavía; primero construir una base geométrica fiable.

---

# Exits y transiciones de área

Investigar cómo:

- d2mapapi_mod;
- D2RMH;
- MapAssist

representan:

- exits;
- adjacent levels;
- stairs;
- warps;
- doors;
- waypoints.

Queremos distinguir:

```text
apertura normal
```

de:

```text
salida hacia otra área
```

Esto será extremadamente útil para accesibilidad.

---

# Automapa de D2R

Investigar también el automapa real del juego.

Preguntas:

1. ¿Qué función determina qué geometría dibujar?
2. ¿El automapa consume directamente Room/Level data?
3. ¿Existe una lista de automap cells?
4. ¿Esas cells contienen geometría suficiente para navegación?
5. ¿Podemos acceder a esos datos antes/después de ser revelados?
6. ¿La información del automapa distingue paredes de suelo?
7. ¿Podemos localizar exits y doors a partir de sus cells?

Buscar referencias y funciones relacionadas con nombres similares a:

```text
Automap
AutomapCell
AddAutomapCell
RevealAutomap
Room
Level
Collision
Path
```

No asumir que nombres históricos permanecen iguales en D2R.

---

# No usar el automapa visual como única fuente

Evitar soluciones basadas en:

- capturar la imagen del automapa;
- reconocimiento de colores;
- OCR;
- análisis de pixels.

Queremos los datos antes de renderizarlos.

Sólo considerar el pipeline gráfico como pista para descubrir dónde están las estructuras en memoria.

---

# Seguridad / robustez

No realizar writes de memoria innecesarios.

Preferir:

```text
read-only introspection
```

para obtener geometría.

No modificar:

- collision flags;
- player path;
- rooms;
- automap state

si no es estrictamente necesario.

Nuestro mod necesita leer información para accesibilidad, no alterar la lógica del juego.

---

# Compatibilidad offline/online

Separar claramente:

```text
qué información estamos leyendo;
qué funciones internas estamos llamando;
qué datos estamos generando nosotros.
```

No asumir que por ser técnicamente posible sea seguro usarlo online.

D2RMH y MapAssist son herramientas de maphack.

Usarlas solamente como **referencia de reverse engineering y representación de mapa**.

No integrar comportamientos de cheat que no tengan relación con accesibilidad.

---

# Licencias

Antes de copiar código:

- comprobar licencia de cada repositorio;
- `d2mapapi_mod` declara licencia MIT-style;
- MapAssist utiliza GPLv3;
- comprobar la licencia concreta de D2RMH y dependencias.

No copiar código GPL directamente dentro de un proyecto incompatible sin analizar implicaciones.

Podemos:

- estudiar algoritmos;
- reimplementar ideas;
- documentar estructuras;
- escribir nuestra propia implementación.

---

# Plan de investigación

## Fase 1 — Auditoría

Entregar:

```text
CurrentRadarAnalysis.md
```

con:

- arquitectura actual;
- fuente actual de collision;
- transformaciones de coordenadas;
- puntos donde se producen errores.

---

## Fase 2 — Investigación externa

Entregar:

```text
D2RNavigationResearch.md
```

con una tabla similar a:

| Fuente | Qué aporta | D2R directo | Runtime | Riesgo/limitación |
|---|---|---:|---:|---|
| Current collision probes | colisión local | Sí | Sí | muchos falsos resultados |
| D2R internal collision | por investigar | Sí | Sí | offsets/estructuras |
| d2mapapi_mod | mapa completo | No directamente | generado | Legacy/32-bit |
| D2RMH | bridge D2R + mapa | Sí | Sí | arquitectura externa |
| MapAssist | memory structs | Sí | Sí | offsets/versiones |

---

## Fase 3 — Proof of Concept

Antes de conectar sonidos o TTS:

Crear una prueba que pueda imprimir/loggear alrededor del jugador:

```text
###############
###.........###
##...........##
##....P......##
##...........##
#######..######
```

o un dump equivalente.

Debe moverse correctamente con el jugador.

Luego probar en:

1. Rogue Encampment;
2. Blood Moor;
3. Den of Evil;
4. pasillos estrechos;
5. habitaciones;
6. esquinas;
7. puertas;
8. transiciones entre rooms.

---

# Tests imprescindibles

Crear tests/logs reproducibles para:

## Test 1 — pared frontal

Jugador mirando directamente a una pared.

Esperado:

```text
front = blocked
```

---

## Test 2 — pared frontal con salida lateral

```text
########
#......#
###..###
#......#
########
```

El sistema no debe asumir que toda la región está cerrada simplemente porque la línea central choca.

---

## Test 3 — espacio inaccesible detrás de pared

```text
###########
#....#....#
#..P.#....#
#....#....#
###########
```

El espacio al otro lado no debe contarse como accesible.

---

## Test 4 — pasillo estrecho

Debe poder distinguir:

```text
corridor
```

de:

```text
large open room
```

---

## Test 5 — esquina

Comprobar que no fluctúe entre varias direcciones por pequeñas variaciones de posición.

---

## Test 6 — cambio de Room

Cruzar entre dos rooms sin:

- perder geometría;
- desplazar coordenadas;
- anunciar paredes inexistentes.

---

## Test 7 — cambio de Level

Cruzar:

```text
Blood Moor -> Cold Plains
```

o equivalente.

La cache debe invalidarse correctamente.

---

# Performance

El radar debe poder funcionar continuamente sin generar coste excesivo.

Objetivo conceptual:

```text
expensive map build:
    solamente al cargar/cambiar área

cheap radar query:
    durante gameplay
```

No ejecutar BFS del mapa entero cada frame si no es necesario.

Posibles optimizaciones:

- BFS local;
- connected-component cache;
- precomputed directional connectivity;
- distance fields;
- sector summaries.

Primero priorizar corrección.

---

# Lo que NO debes hacer

No:

1. reescribir todo el radar antes de entender el actual;
2. copiar offsets antiguos sin validarlos;
3. asumir que `d2mapapi_mod` funciona directamente con D2R;
4. usar screenshots del automapa;
5. solucionar errores geométricos únicamente con debounce;
6. meter D2RMH entero como dependencia;
7. requerir al usuario ejecutar manualmente un segundo programa si existe alternativa;
8. implementar navegación automática como requisito para el radar;
9. añadir pathfinding de misión antes de tener una grid fiable;
10. romper las partes actualmente funcionales del mod.

---

# Prioridad técnica recomendada

Investigar en este orden:

```text
1. Nuestro radar actual
2. d2mapapi_mod/collisionmap.*
3. d2mapapi_mod/mapdata.*
4. d2mapapi_mod/pathfinder.cpp
5. D2RMH: adquisición de seed/area/player
6. MapAssist: estructuras actuales de D2R
7. collision/room structures internas de D2R
8. prueba de NavigationGrid
9. BFS/flood fill
10. conectar radar accesible
```

---

# Resultado ideal

La arquitectura debería terminar conceptualmente así:

```text
                  D2R
                   │
        ┌──────────┴──────────┐
        │                     │
 Player state           Map/room data
 position/yaw            collision/seed
        │                     │
        └──────────┬──────────┘
                   │
            NavigationMap
                   │
          connected topology
                   │
        ┌──────────┴──────────┐
        │                     │
 Immediate radar       Exploration radar
        │                     │
 walls/openings         regions/exits
        │                     │
        └──────────┬──────────┘
                   │
            Accessibility UI
              audio / TTS
```

La pieza crítica es:

```text
NavigationMap
```

Debe convertirse en la única fuente de verdad para la geometría del radar.

---

# Preguntas que debes responder antes de implementar la solución definitiva

Al terminar la investigación, responde explícitamente:

1. ¿De dónde viene actualmente nuestra colisión?
2. ¿Por qué produce resultados incorrectos?
3. ¿Podemos leer la collision map real de D2R?
4. Si sí:
   - ¿dónde?
   - ¿qué estructura?
   - ¿qué flags?
   - ¿qué resolución?
5. Si no:
   - ¿podemos reconstruirla usando seed?
6. ¿Qué hace exactamente D2RMH?
7. ¿Qué podemos reutilizar conceptualmente de `d2mapapi_mod`?
8. ¿Podemos crear un mapa completo de transitabilidad sin proceso externo?
9. ¿Cómo transformamos world coordinates a collision coordinates?
10. ¿Cómo manejamos rooms?
11. ¿Cómo manejamos areas/levels?
12. ¿Cómo determinamos reachability?
13. ¿Qué coste de CPU tendría?
14. ¿Cuál es la solución más limpia para un mod "install and play"?

---

# Instrucción final para el agente

No quiero una respuesta superficial ni únicamente sugerencias.

**Investiga el código de verdad.**

Sigue llamadas, estructuras y transformaciones.

Cuando encuentres algo importante:

- cita archivo;
- función;
- estructura;
- campo;
- flujo de llamadas;
- explica por qué es relevante.

Si el repositorio local ya contiene código relacionado, compáralo directamente con estas referencias.

No asumas que el enfoque planteado aquí es necesariamente correcto.

Si descubres una fuente de navegación más limpia dentro de D2R, úsala y documenta por qué es superior.

Primero entrega la investigación y una propuesta concreta.

Después, si la evidencia es suficiente, implementa un **proof of concept pequeño y aislado** sin destruir el radar actual.

La meta inmediata no es tener pathfinding de quests.

La meta es conseguir:

> **una representación fiable, estable y conectada del espacio transitable alrededor del jugador.**

Una vez eso funcione, construiremos el radar accesible encima.
