Diablo II: Resurrected AccessRoadmap técnico de investigación

D2RLoader + lector de pantalla + audio cues + navegación manual

Documento de exploración técnica — 14 de septiembre de 2026

Objetivo: convertir D2R en una experiencia jugable por una persona ciega sin automatizar el juego: lector de pantalla para interfaz y texto, audio cues para información espacial, soporte de mando, navegación manual y feedback de combate/loot.

Estado de partida: D2R comprado en Steam y D2RLoader ya instalado. El enfoque propuesto asume desarrollo y pruebas principalmente offline mientras se investiga el comportamiento de hooks, lectura de memoria y plugins.

# 1. Resumen ejecutivo

El camino más prometedor ya no es intentar terminar una recreación completa de Diablo II. En septiembre de 2026, D2RLoader dispone de un Plugin SDK C++ oficial con API v4. Además de los Native Hooks, el SDK ofrece servicios de alto nivel que son especialmente útiles para accesibilidad: eventos del ciclo de juego, input registrado dentro del menú de controles, eventos de UI, acceso seguro a widgets existentes, localización, inventario, interacciones con objetos, acceso a tablas de datos y ejecución de tareas en los hilos correctos del juego.

Al mismo tiempo, hay proyectos externos de lectura de memoria que documentan estructuras actuales de D2R. El patrón central es UnitHashTable, que contiene cinco familias de entidades: jugadores, NPC/monstruos, objetos, misiles e ítems. Proyectos como MapAssist, diablo2utils, @diablo2 y D2RMH demuestran que se pueden obtener posición del jugador, nivel actual, seed del mapa, stats, objetos del suelo, monstruos y puntos de interés sin reconstruir el motor.

La estrategia recomendada es híbrida: usar primero las APIs seguras de D2RLoader para interfaz, input, localización e inventario; usar lectura de estado o hooks sólo para la información del mundo que el SDK todavía no expone; y mantener TTS/audio en una capa separada para poder iterar sin recompilar toda la parte nativa.

El primer gran objetivo no debería ser 'hacer el juego accesible entero', sino construir dos sondas diagnósticas: una sonda de UI que registre todos los mensajes y widgets relevantes mientras se navega con mando, y una sonda de mundo que publique snapshots de jugador, monstruos, objetos, misiles y loot. Si ambas funcionan de forma fiable, el resto del proyecto se vuelve un problema de UX y clasificación más que de ingeniería inversa básica.

# 2. Principios de diseño

- Gameplay fiel: el mod debe informar y orientar, no jugar automáticamente.

- Navegación manual: evitar pathfinder que mueva al personaje. Se puede usar geometría o una ruta sólo para calcular dirección, distancia, cruces y desviaciones.

- Mando como primera clase: aprovechar el sistema de foco/targeting de D2R siempre que sea posible, en lugar de imponer muchas teclas nuevas.

- TTS sólo cuando aporta información semántica. Información espacial repetitiva debe ir preferentemente por audio cues.

- Audio cues diferenciables por función, con paneo/posición, distancia y cadencia; evitar saturar al jugador con una señal por cada entidad.

- Arquitectura modular: adquisición de estado, accesibilidad UI, TTS, audio espacial y configuración deben poder evolucionar por separado.

- Event-driven siempre que sea posible. Polling sólo para datos que realmente no tengan evento disponible.

- Offline primero. Herramientas que leen o modifican memoria pueden implicar riesgo de detección o sanción en Battle.net; no asumir que un método es seguro online.

- Todo dato dependiente de build debe detectarse mediante firma/pattern scan o validación fuerte; evitar offsets mágicos sin verificación.

- Accesibilidad reproducible: cada descubrimiento debe quedar documentado con build de D2R, evento observado, función/offset y pasos para volver a encontrarlo.

# 3. Arquitectura recomendada para investigar

## 3.1. Componente nativo: D2RAccessNative.dll

- Plugin de D2RLoader, idealmente con rol Client mientras sólo haga UI, input y comportamiento local.

- Usar primero servicios oficiales del Plugin SDK v4. Activar NativeHooks únicamente para vacíos concretos.

- Responsabilidades: registrar eventos, acceder a widgets/tablas/inventario, detectar foco o target cuando sea posible y publicar snapshots compactos.

- No meter aquí la lógica completa de speech, filtrado de spam ni audio espacial salvo que resulte claramente más simple.

## 3.2. Compañero C#: D2RAccess.exe

- Recibir eventos por Named Pipe, shared memory o socket local.

- Integrar PRISM/Tolk/NVDA y fallback SAPI.

- Gestionar colas de voz, prioridad, interrupciones, repetición del último mensaje y filtros anti-spam.

- Gestionar NAudio/OpenAL/FMOD u otra capa de audio espacial para beacons y cues.

- Guardar configuración accesible, perfiles y logs de diagnóstico.

- Permitir desarrollar gran parte de la UX sin tocar hooks nativos.

## 3.3. Sonda externa opcional

Durante investigación puede ser más rápido mantener un pequeño lector externo de memoria. No tiene por qué formar parte de la versión final. Sirve para comparar lo que ve D2RLoader con UnitHashTable y para descubrir entidades sin arriesgar la estabilidad del plugin.

D2R.exe

  ├─ D2RLoader / D2RAccessNative.dll

  │    ├─ Lifecycle / UI / Widget / Localization

  │    ├─ Inventory / ItemInteraction / DataTables

  │    └─ Native hooks sólo donde falten APIs

  │

  └─ estado de mundo

       ├─ UnitHashTable / rooms / map data

       └─ snapshots de player, NPC, object, missile, item

                 ↓ IPC

D2RAccess.exe (C#)

  ├─ Screen reader

  ├─ Audio cues

  ├─ Filtrado / prioridades

  ├─ Configuración

  └─ Logs y herramientas de investigación

# 4. Exploraciones prioritarias dentro de D2RLoader

## 4.1. Sonda de LifecycleService

El SDK v4 documenta eventos de entrada/salida de partida, jugador local listo, cambios de acto y área, cambio de nivel de personaje, finalización de quests y resurrección. Estos eventos pueden convertirse en la columna vertebral de la máquina de estados de accesibilidad.

Experimento: crear un plugin que sólo escriba JSONL con timestamp y cada evento de lifecycle.

Éxito: poder saber con precisión cuándo empezar/parar scans, cuándo limpiar caches y cuándo anunciar cambio de zona sin polling.

Uso accesible: «Cold Plains», «Acto II», «nivel 18», «misión completada», reconstrucción de contexto tras muerte/resurrección.

## 4.2. InputService: acciones accesibles dentro de los controles del juego

InputService registra acciones con nombre, no hooks crudos de teclado. D2RLoader las integra en el menú de Controls y conserva bindings. Debe comprobarse específicamente el comportamiento con mando.

- Repeat last speech.

- Where am I / área, coordenada lógica y objetivo activo.

- Scan nearby interactables.

- Read current target.

- Read status: HP, mana, buffs/debuffs relevantes.

- Toggle navigation beacon.

- Cycle cue verbosity.

- Open accessibility settings panel, si se decide crear uno.

Experimento: registrar 2 o 3 acciones de prueba y vincularlas desde la UI de controles usando teclado y mando.

Éxito: no necesitar un sistema paralelo de hotkeys para las funciones básicas.

## 4.3. SharedEventService: construir un 'UI message spy'

Éste puede ser el experimento de mayor valor inicial. El SDK expone listeners de mensajes de UI que reciben target, command y text copiados. Además, esos listeners pueden observar mensajes normales de paneles y acciones enviadas mediante WidgetService.

Experimento: registrar todos los mensajes de UI mientras se recorren menú principal, selección de personaje, inventario, skills, waypoints, vendedor, opciones y pausa.

timestamp | panel/context | target | command | text

-------------------------------------------------

...       | inventory     | ...    | ...     | ...

...       | skills        | ...    | ...     | ...

Después, repetir exactamente las mismas secuencias con ratón/teclado y con mando. Buscar mensajes que correlacionen con el cambio de selección/foco. Si aparece un evento estable al mover el foco, puede evitarse un hook de bajo nivel.

## 4.4. WidgetService: indexar los widgets de los layouts

WidgetService puede localizar paneles y widgets existentes por nombre, leer su rectángulo local, cambiar visibilidad/enabled y enviar acciones normales. No parece ofrecer enumeración arbitraria, por lo que conviene construir un índice offline de nombres a partir de los JSON de UI de D2R.

- Extraer los layouts del CASC/mod y parsear recursivamente name, type, fields, children, onClickMessage y referencias.

- Indexar por panel: inventario, stash, cubo, skill tree, character sheet, waypoint, vendor, quest log, pause, character select.

- Mantener índices separados para keyboard/mouse y controller/pc.

- Usar los nombres encontrados para probar WidgetService y correlacionar cada widget con los mensajes del UI spy.

Hipótesis: muchas pantallas pueden hacerse accesibles combinando layout JSON + mensajes + localización sin conocer punteros nativos de widgets.

## 4.5. Descubrir el foco real del controlador

Para accesibilidad completa, el dato decisivo es 'qué elemento está seleccionado ahora'. Si el UI message spy no lo revela, éste es el primer candidato a Native Hook.

- Buscar la función que cambia selección/foco cuando se pulsa D-pad/stick.

- Hookear la asignación de 'focused widget' o el método que actualiza highlight/selected state.

- Registrar puntero, nombre del widget si se puede resolver, panel activo y texto/localization key.

- Comparar con cambios visuales en layouts como ButtonWidget, InventoryGridWidget y elementos controller-only.

- Priorizar un hook de lectura/observación; no modificar la navegación nativa.

Éxito: un evento único FocusChanged(widgetName, panel, metadata) para cualquier pantalla.

## 4.6. LocalizationService

El SDK permite copiar el texto activo en UTF-8 por id numérico o string key. Esto debería usarse para no hardcodear nombres de items, skills, áreas o etiquetas y para respetar el idioma seleccionado por el jugador.

- Crear un pequeño resolver que acepte id/key y devuelva texto.

- Loggear qué ids/keys aparecen al navegar widgets.

- Probar idiomas distintos para verificar que speech sale del contenido activo y no de una tabla propia.

- Usar localización como capa final después de convertir ids internos a claves mediante tablas de datos.

## 4.7. DataTableService

El SDK ofrece acceso de sólo lectura a tablas compiladas activas. La documentación confirma lookups útiles para Skills, Levels, Items e ItemTypes. Esto permite transformar ids internos en información semántica sin parsear todos los TXT manualmente.

- Construir un dump diagnóstico de las tablas soportadas por la build actual.

- Resolver levelId a nombre/metadata de área.

- Resolver item txtFileNo/código y tipos.

- Resolver skill ids para anunciar skill seleccionada, coste y estado si los campos disponibles lo permiten.

- Comparar datos Base/LoD/RotW si la instalación tiene contenido adicional.

## 4.8. InventoryService + ItemInteractionService

Esta combinación puede resolver una parte enorme de la accesibilidad del inventario sin hooks. InventoryService puede buscar items existentes en equipo, belt, inventario, cubo, trade, stash y shared stash; ItemInteractionService expone activaciones lógicas en grids probados y reporta container, celda seleccionada, modificadores y origen de input.

- Al abrir inventario, construir un modelo lógico de grid con coordenadas y tamaño de cada item.

- Cuando el jugador cambia de celda con mando, intentar correlacionar selección visual con ItemInteraction/Widget/UI events.

- Anunciar nombre corto al moverse; tecla de detalle para propiedades completas.

- Anunciar huecos vacíos de forma compacta sólo si hace falta para orientación.

- Implementar 'row/column' opcional para quien quiera contexto espacial del grid.

- Probar cubo y personal stash primero; vendor, trade, corpse, ground, equipment, belt y shared-stash requieren atención porque la API de interacciones no cubre todos esos caminos.

# 5. Exploración del estado del mundo

## 5.1. UnitHashTable como fuente universal

Documentación comunitaria reciente describe UnitHashTable como 5 grupos de 128 buckets: Player, NPC/Monster, Object, Missile e Item. Cada bucket encadena UnitAny. Esto es ideal para crear snapshots periódicos del mundo.

type 0 -> Player

type 1 -> NPC / Monster

type 2 -> Object

type 3 -> Missile

type 4 -> Item

El offset absoluto cambia con parches. La investigación debe implementar pattern scanning y validación, no copiar un valor fijo como contrato. diablo2utils documenta patrones y un flujo para recalcular offsets tras actualizaciones.

## 5.2. Snapshot diferencial en vez de hablar cada scan

El lector debe construir una foto del mundo y compararla con la anterior. La accesibilidad reacciona a cambios, no a la lista completa en cada tick.

EntityKey = (type, unitId)

prevSnapshot

currentSnapshot

spawned = current - prev

despawned = prev - current

changed = same key, relevant fields changed

- Monstruo entra en rango -> cue o anuncio según categoría.

- Monstruo muere/desaparece -> detener su cue.

- Nuevo item en el suelo -> evaluar filtro y anunciar una sola vez.

- Misil peligroso aparece -> cue direccional mientras sea relevante.

- Objeto/puerta cambia de estado -> actualizar cue interactuable.

- Jugador cambia de sala/área -> reconstruir índices cercanos.

## 5.3. Posición y orientación del jugador

La posición tile X/Y está documentada a través de UnitAny -> Path. Falta decidir cómo obtener orientación fiable. No asumir que 'dirección visual' sea necesaria: para muchos cues basta usar el vector de movimiento reciente como heading cuando el personaje se desplaza y mantener el último heading al parar.

- Comparar posición actual con posición anterior para derivar vector de movimiento.

- Buscar campos de target/destination en Path o funciones internas si se necesita orientación instantánea en reposo.

- Con mando, investigar si el sistema de auto-target ofrece un heading/target más útil que la rotación del sprite.

## 5.4. Hover y target actual

diablo2utils documenta un offset comunitario 'hover' que contiene type y unitId de la unidad bajo cursor. También documenta un offset de UI y roster. Estos offsets deben pattern-scanearse por build.

- Leer hovered unit con ratón para confirmar el formato.

- Comprobar si el sistema de mando reutiliza esa estructura para el target seleccionado.

- Si no, buscar la estructura equivalente observando el target de la barra de vida mientras cambia con el stick.

- Resolver (type, unitId) de vuelta a UnitHashTable para obtener nombre, posición, HP y stats.

Ganancia: si el target del mando puede obtenerse, combate y lectura de NPC/objetos se simplifican muchísimo.

## 5.5. UI state flags

También existe documentación comunitaria de un array de flags de UI donde cada byte representa estados/paneles. Puede servir como fallback para saber qué interfaz está abierta si los eventos de D2RLoader no cubren algún caso.

Experimento: loggear el array antes/después de abrir inventario, skills, quest, waypoint, vendor, chat, pause y menú de opciones; construir un mapa de bits semántico.

## 5.6. Party roster

- Explorar roster para posición/estado de compañeros si se quiere co-op accesible.

- Audio cue distinto para aliados, mercenario y jugador hostil.

- Anunciar entrada/salida de party y muerte de compañero sólo cuando sea relevante.

# 6. Navegación sin autopilot

## 6.1. Colisión local: la prioridad número uno

Antes de hacer rutas globales, conviene conseguir una respuesta fiable a: '¿puedo moverme en esta dirección durante los próximos pocos tiles?'. Esto permite recrear un radar abierto/bloqueado.

- Investigar collision map de la sala actual y salas vecinas.

- Revisar D2RMH/d2mapapi: el repositorio incluye collisionmap y pathfinder y puede servir como referencia para generar geometría desde la seed.

- Probar 3 o 5 rayos: frente, frente-izquierda, frente-derecha y opcionalmente laterales.

- Convertir distancia hasta obstáculo en frecuencia/volumen/cadencia, no TTS.

- Distinguir pared sólida de puerta/entrada/objeto interactuable si la clasificación lo permite.

## 6.2. Rooms y transiciones

Room expone rooms cercanas y lista de unidades de sala en documentación reciente. Aunque no dé por sí sola toda la colisión, es útil para detectar cambios de subárea y limitar scans.

- Identificar current room y neighbor rooms.

- Cachear entidades sólo en current + neighbors.

- Detectar transición de room para refrescar radar y POIs.

- Estudiar si room bounds permiten aproximar pasillos/intersecciones.

## 6.3. Seed + generador de mapa

El seed del mapa y el levelId están documentados. @diablo2 ofrece generación de mapa para Classic y Resurrected; D2RMH usa infraestructura d2mapapi. Esto abre un camino para conocer exits y topología sin hacer pathfinding automático.

- Generar mapa lógico de la zona actual desde seed + difficulty + levelId.

- Extraer exits, waypoints y objetos de quest conocidos.

- Calcular sólo vector/dirección al objetivo elegido.

- Si se calcula una ruta, usarla para detectar la próxima decisión (giro/intersección), no para enviar movimiento.

## 6.4. Beacon de objetivo

- El jugador selecciona un POI: salida, waypoint, corpse, portal, NPC, quest object o loot marcado.

- Un sonido espacial apunta al POI.

- Cadencia o pitch representa distancia; un segundo sonido confirma que el jugador está dentro del radio de interacción.

- Si la ruta topológica indica que caminar directo está bloqueado, el beacon puede apuntar al siguiente nodo de ruta, pero nunca mover al jugador.

# 7. Combate

## 7.1. Radar de enemigos

- Escanear Unit type 1 y clasificar hostil/NPC amistoso.

- Cue posicional para enemigos normales; sonidos distintos para champion, unique/superunique y boss.

- Cadencia creciente con proximidad; límite de voces simultáneas.

- Anunciar nombre sólo al encontrar un tipo nuevo o cuando se convierte en target activo.

- No leer continuamente HP de todos; reservar speech/sonido especial para target y bosses.

## 7.2. HP, estados y prioridades

- Usar StatList para vida y otros stats disponibles.

- Low-HP warning graduado parecido a Diablo Access: empieza a un umbral y acelera según baja la vida.

- Mana bajo sólo si la skill activa lo hace relevante.

- Explorar states/auras/curses para avisar congelación, poison, amp damage u otros estados peligrosos.

- Evitar anunciar cada hit; hablar cambios de estado, no números que fluctúan cada frame.

## 7.3. Misiles y hazards

Unit type 3 corresponde a Missile. Puede convertirse en una capa de accesibilidad muy potente si se filtra bien.

- Identificar misiles por txtFileNo y DataTables/datos de juego.

- Medir posición, trayectoria o delta de posición para estimar si se acercan.

- Sólo cuear categorías realmente peligrosas o telegráficas; no cada flecha del juego.

- Distinguir AoE persistente, proyectil rápido y efecto de boss si los datos lo permiten.

- Probar primero un pequeño whitelist de hazards conocidos.

## 7.4. Target del mando

Explorar si la selección de enemigo que usa la interfaz controller puede convertirse en el target accesible principal. Si se puede resolver a unitId, permite anunciar nombre, HP, distancia, dirección, inmunidades y estado sin inventar un sistema de targeting paralelo.

# 8. Loot, objetos e interactuables

## 8.1. Ground items

La cadena de memoria de ground items está bien documentada: Unit type 4, modo en suelo/dropping, ItemData, posición estática y calidad. MapAssist ya implementa alertas de loot.

- Anunciar sólo al aparecer, no en cada scan.

- Aplicar filtros por calidad, tipo, runa, socket, base o lista del usuario.

- Sonido direccional persistente para loot marcado, speech opcional al aparecer.

- Tecla de 'loot cercano' que liste por prioridad y distancia.

- Tracker/beacon al item seleccionado.

## 8.2. Objetos del mundo

- Unit type 2: puertas, cofres, shrines, racks, waypoints, quest objects, portales y otros interactuables.

- Crear un clasificador txtFileNo -> categoría accesible.

- Sonidos distintos: puerta/salida, contenedor, shrine/well, waypoint, portal, quest object.

- Hablar el nombre sólo al apuntar/seleccionar o al usar una función de exploración.

- Investigar states/mode del objeto para distinguir abierto/cerrado, usado/no usado.

## 8.3. Sonidos nativos vs audio externo

D2R permite sustituir/añadir recursos de sonido mediante mod data, y D2RLoader puede servir recursos desde el plugin. Aun así, para un prototipo accesible suele ser más rápido reproducir cues desde el companion.

- Camino A: NAudio/OpenAL externo. Ventaja: iteración rápida y control 3D.

- Camino B: custom SFX dentro de D2R. Ventaja: mezcla nativa y posicionamiento si se encuentra una función segura de reproducción.

- Investigar más adelante si D2RLoader publica o puede hookear una función de sonido estable; no bloquear el prototipo por esto.

# 9. Interfaz: pantallas a cubrir

Menú principal / offline-online / creación y selección de personaje: Prioridad muy alta. Probar UI messages, widget names, focus controller y strings localizadas.

Inventario / equipo / belt: Modelo de grid, item bajo selección, equip/unequip, comparación, cantidad, sockets, durabilidad.

Stash / shared stash / cubo: Navegación por tabs y grids; cuidado con las áreas no cubiertas por ItemInteractionService.

Vendor: Lista de items, buy/sell/repair/gamble. Probable necesidad de estudiar widgets o hooks específicos.

Skill tree: Skill enfocada, nivel actual, requisitos, coste, puntos disponibles y navegación entre ramas.

Character sheet: Stats base y avanzados, resistencias, breakpoints si se desea.

Quest log: Quest, objetivo, estado y texto descriptivo.

Waypoints: Acto, waypoint seleccionado, desbloqueado/no desbloqueado y activación.

NPC dialogue: Texto actual, opciones y avance de diálogo. Investigar mensajes de UI y localización.

Options / Controls: Necesario para configurar el propio mod con mando y lector.

Pause / death / corpse recovery: Estado crítico; debe ser accesible incluso si otros subsistemas fallan.

Horadric recipes / tooltips: Lectura de tooltip y propiedades; SharedEventService puede añadir texto pero la lectura requiere capturar contenido relevante.

# 10. Experimentos específicos que merece la pena pedirle a Claude

1. Logger de servicios disponibles. Enumerar QueryService exitosos, versión/structSize y capacidades. Guardar build del juego y versión del SDK.

2. UI event recorder. Registrar target/command/text para todos los UI messages; exportar JSONL y un resumen por pantalla.

3. Layout indexer. Parsear los JSON de data/global/ui/layouts y controller/pc; generar un índice widgetName -> type -> panel -> onClickMessage.

4. Widget probe. Dado un nombre del índice, usar WidgetService para comprobar si existe, visible/enabled y rectángulo.

5. Focus experiment. Navegar con mando y correlacionar cada pulsación con UI messages y cambios de widgets. Si no basta, localizar el setter de focus con NativeHooks.

6. Localization explorer. Resolver automáticamente claves/ids vistos en layouts y eventos; guardar pares key -> texto.

7. Lifecycle recorder. Probar entrada/salida, cambio de zona, level-up, quest y resurrección.

8. Controller action test. Registrar una acción de InputService y confirmar que puede vincularse y ejecutarse con mando.

9. Inventory snapshot. Enumerar inventario/equipo/belt/cubo/stash; imprimir coordenada, tamaño, código, calidad e información disponible.

10. Inventory focus correlation. Mover selección por grid con mando y encontrar qué dato cambia: UI event, hover, widget action, item interaction o memoria.

11. UnitHash scanner. Snapshot de los 5 tipos de UnitAny con validación y conteo por tipo.

12. Hovered/target unit probe. Pattern-scan del offset hover; comprobar mouse y luego controller target.

13. UI flags mapper. Tomar diff del array de flags al abrir/cerrar cada panel.

14. Room graph probe. Leer current room, neighbors y lista de units; producir un log de transición al caminar.

15. Collision prototype. Obtener collision map de la sala/mapa y devolver distancia libre en 3 direcciones.

16. Map seed prototype. Leer seed + levelId + difficulty y generar la zona con d2mapapi/@diablo2; comparar exits con la partida real.

17. Enemy spatial cue PoC. Elegir hasta 3 monstruos cercanos y reproducir cues paneados según ángulo/distancia.

18. Ground loot PoC. Detectar un nuevo drop, resolver nombre y calidad, hablar una vez y mantener beacon si pasa el filtro.

19. Missile whitelist. Loggear missiles durante una zona sencilla, identificar 3 o 4 tipos y crear cues sólo para ellos.

20. Speech bridge. Named Pipe de C++ a C# con mensajes Speak/Stop/State; integrar NVDA/Tolk/PRISM.

21. Replay de telemetría. Guardar snapshots/eventos a archivo y permitir que el motor accesible los reproduzca sin D2R abierto para ajustar audio y spam.

# 11. Orden recomendado de trabajo

Fase A — Instrumentación segura

- Hello plugin + logging.

- LifecycleService.

- InputService.

- UI message spy.

- Layout indexer.

- LocalizationService.

Fase B — Primera interfaz accesible

- Conseguir FocusChanged fiable al menos en menú principal y character select.

- Bridge de speech.

- Repetir patrón en pause/options/inventory.

Fase C — Estado de mundo

- UnitHashTable con pattern scan.

- Player position/level/seed.

- NPC/Object/Item snapshots.

- Hovered/controller target.

Fase D — Primer gameplay jugable

- Enemy cues.

- Ground loot alerts.

- Interactable cues.

- HP warning.

- Where am I y nearby scan.

Fase E — Navegación

- Room graph.

- Collision probes.

- Map seed generator.

- Exits/waypoints beacons.

- Ruta usada sólo como guía de giro, nunca para mover.

Fase F — Profundidad

- Missiles/hazards.

- Skills/stats/quests.

- Vendor/trade/shared stash.

- NPC dialogue.

- Co-op/party cues.

# 12. Criterios para decidir cuándo usar NativeHooks

Usar un hook nativo sólo si al menos una de estas condiciones se cumple:

- El SDK no expone el dato necesario.

- Existe un evento interno claramente más fiable que polling/memory scan.

- Se necesita identificar el focus/selected widget y no aparece en mensajes públicos.

- Se necesita target de controlador y no existe una estructura pública/externa estable.

- Se requiere una función nativa concreta como colisión, sonido o texto que no pueda reconstruirse externamente.

Evitar NativeHooks cuando:

- El mismo resultado puede obtenerse con Lifecycle/Input/SharedEvent/Widget/Localization/Inventory.

- El hook sólo ahorra unas pocas líneas pero aumenta fragilidad entre builds.

- Obliga a escribir memoria o cambiar lógica de gameplay cuando sólo necesitamos observar.

- El dato puede derivarse de snapshots con coste bajo.

# 13. Robustez frente a parches

- Guardar build/version del juego en cada log.

- Pattern-scan para globals importantes: UnitHashTable, UI flags, hover, roster y cualquier offset futuro.

- Validar estructuras: tipos dentro de rango, punteros dentro de memoria válida, difficulty 0..2, ids plausibles.

- Separar offsets/patterns del código de accesibilidad.

- Crear smoke tests: entrar a Blood Moor, verificar player, área, un NPC, un objeto y un item drop.

- Si falla una fuente, degradar sólo esa feature; no hacer crash del mod completo.

- Comparar offsets con más de un proyecto comunitario antes de asumir que un cambio es correcto.

- Preferir servicios oficiales de D2RLoader, que intentan ocultar punteros nativos y gestionar lifetime.

# 14. Logging y herramientas de investigación

Un mod de accesibilidad para un juego cerrado se vuelve mucho más fácil si la instrumentación es una feature de primera clase.

- Log de UI events en JSONL.

- Log de lifecycle.

- Snapshot opcional de UnitHashTable cada N ms con diff, no dump completo permanente.

- Comando de debug para describir target actual.

- Comando para describir widget bajo investigación.

- Comando para listar paneles/flags activos.

- Hotkey para marcar un momento en el log: 'acabo de mover foco a Repair'.

- Modo silencioso: recopila eventos sin speech para comparar secuencias.

- Replay offline de telemetría para ajustar TTS/audio sin lanzar D2R.

# 15. Riesgos y límites

- Battle.net: lectura/inyección/hooking puede ser detectado o sancionado. Desarrollar y validar en offline; no presentar ninguna técnica como 'segura online'.

- Offsets: valores documentados por proyectos externos son específicos de una build y pueden cambiar.

- Focus UI: probablemente sea el mayor desconocido técnico para lograr screen reader completo.

- Controller UI y keyboard/mouse UI usan layouts distintos; hay que probar ambos.

- Audio overload: el problema no será sólo 'obtener datos', sino seleccionar qué información merece sonido.

- Vendor/trade/equipment/belt/shared stash no están completamente cubiertos por el evento semántico de ItemInteractionService v1; pueden requerir exploración propia.

- Localización: no construir nombres desde IDs internos si el juego ya puede devolver strings activos.

- Performance: scans de 640 buckets son razonables si se hacen bien, pero no se debe volver a parsear tablas/layouts en cada frame.

# 16. Definición de un primer prototipo útil

El primer prototipo que demostraría que el proyecto es viable no necesita cubrir campañas ni todos los menús. Debería cumplir lo siguiente:

- El mod arranca con D2RLoader sin crash y detecta entrada/salida de partida.

- NVDA puede hablar mensajes enviados por el companion.

- Existe al menos una acción accesible configurable desde Controls.

- Puede anunciar el elemento seleccionado en al menos una pantalla real de D2R.

- Lee posición y área del jugador.

- Detecta monstruos cercanos y reproduce un cue direccional.

- Detecta un item nuevo en el suelo y lo anuncia una sola vez.

- Detecta al menos una puerta/salida/waypoint/objeto y puede crear un beacon.

- Un radar básico informa espacio libre/bloqueado en frente/izquierda/derecha.

- Todo esto funciona sin mover automáticamente al personaje.

# 17. Checklist para un agente de código (Claude/Codex)

- Antes de buscar firmas o offsets, leer los headers actuales del PluginSDK y comprobar si ya existe un servicio oficial.

- No asumir que documentación antigua de API v2/v3 describe la API actual; la v4 añade capacidades importantes.

- Para cualquier offset externo: registrar build, patrón, método de resolución y validaciones.

- No escribir en memoria durante la fase de discovery si sólo queremos observar.

- Hacer una sonda pequeña por hipótesis; no mezclar UI, world scan y audio en el mismo experimento.

- Guardar outputs legibles por humanos y por scripts: JSONL/CSV además del log textual.

- Cuando un evento parezca representar foco, probarlo en al menos cinco pantallas y con mando.

- Cuando un campo parezca una posición/HP/id, validar cambiándolo de forma controlada dentro del juego y observando correlación.

- Tratar todo puntero nativo como efímero y respetar el hilo UI/game indicado por el SDK.

- No guardar punteros de objetos del juego entre sesiones; invalidar caches al salir/entrar.

- Usar ThreadService para operaciones que deban ejecutarse en UI thread o game thread.

- Preferir Client role para funcionalidades puramente locales de accesibilidad; no convertir el plugin en Shared/Server sin necesidad.

- Separar 'adquisición de dato' de 'cómo se presenta al jugador'.

# 18. Fuentes técnicas para seguir explorando

- D2RLoader Plugin SDK oficial — API v4, servicios, roles y ejemplos

- Documentación oficial de D2RLoader

- Changelog de D2RLoader

- D2RL-Plugins — ejemplos de plugins DLL con hooks

- diablo2utils — offsets, pointer chains y guía de actualización por pattern scan

- MapAssist — lectura de estado, monstruos, POIs y loot

- @diablo2 — memory reader y map generation para D2R

- D2RMH — map reveal, collisionmap/pathfinder y d2mapapi

- Diablo Access — referencia de UX accesible para Diablo 1/DevilutionX

- D2R Reimagined — ejemplos de modificaciones extensas de UI/controller layouts

- D2R-Excel — guía de archivos de datos de D2R

# 19. Conclusión

El proyecto parece técnicamente viable y, con la API v4 actual de D2RLoader, hay más superficie oficial utilizable de la que había en versiones anteriores. La clave es no empezar haciendo ingeniería inversa de todo. Primero hay que exprimir Lifecycle, Input, Shared UI Events, Widgets, Localization, Inventory y DataTables. Después, cubrir los vacíos del mundo con UnitHashTable/pattern scanning y reservar NativeHooks para focus/target/colisión u otras piezas realmente no expuestas. El punto de inflexión será conseguir dos señales fiables: FocusChanged para la interfaz y EntitySnapshot para el mundo. A partir de ahí, el resto encaja naturalmente con un diseño de TTS + audio cues + navegación manual.