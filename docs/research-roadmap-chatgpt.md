# Diablo II: Resurrected Access — Roadmap técnico de investigación

**D2RLoader + lector de pantalla + audio cues + navegación manual**  
**Fecha:** 14 de septiembre de 2026

Este archivo acompaña al documento Word y está pensado para poder pasarlo directamente a un agente de código.

## Resumen

El enfoque recomendado es híbrido:

- Usar primero el **Plugin SDK v4 de D2RLoader** para lifecycle, input, eventos de UI, widgets, localización, inventario, interacciones de ítems y tablas de datos.
- Usar **UnitHashTable / lectura de memoria / pattern scanning** para datos del mundo que el SDK todavía no expone.
- Mantener **TTS y audio cues** en un companion C# cuando sea conveniente.
- Reservar **NativeHooks** para vacíos concretos: foco real de UI, target del mando, colisión o funciones nativas no expuestas.
- Mantener navegación **manual**, sin mover automáticamente al personaje.

## Experimentos prioritarios

1. Logger de LifecycleService.
2. InputService con acciones accesibles y prueba de bindings de mando.
3. UI message spy con SharedEventService (target/command/text).
4. Indexador de JSON de layouts y controller/pc.
5. Widget probe usando WidgetService.
6. Descubrimiento de FocusChanged; NativeHook sólo si los eventos públicos no bastan.
7. LocalizationService para resolver ids/keys.
8. DataTableService para Levels, Skills, Items e ItemTypes.
9. InventoryService + ItemInteractionService para inventario/cubo/stash.
10. UnitHashTable scanner para Player/NPC/Object/Missile/Item.
11. Hover/controller target probe.
12. UI flags mapper.
13. Room graph.
14. Collision probe frente/izquierda/derecha.
15. Seed + map generator para exits/waypoints/POIs.
16. Enemy spatial cues.
17. Ground loot alerts + beacon.
18. Missile/hazard whitelist.
19. Speech bridge C++ -> C#.
20. Replay offline de telemetría.

## Arquitectura sugerida

```text
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
  └─ Logs y replay
```

## Regla de trabajo para el agente

Antes de buscar firmas u offsets, revisar los headers actuales de PluginSDK. No asumir que documentación de API v2/v3 sigue describiendo la API actual. Para cada offset, guardar build, patrón y validaciones. Hacer sondas pequeñas y observacionales; separar adquisición de datos de presentación accesible.

## Fuentes

- D2RLoader Plugin SDK: https://github.com/D2RLoader/PluginSDK
- D2RLoader docs: https://d2rloader.net/docs.html
- D2RLoader changelog: https://d2rloader.net/changelog.html
- D2RL-Plugins: https://github.com/eezstreet/D2RL-Plugins
- diablo2utils: https://github.com/ChrisTitusTech/diablo2utils
- MapAssist: https://github.com/ResurrectedTrader/MapAssist
- @diablo2: https://github.com/blacha/diablo2
- D2RMH: https://github.com/soarqin/D2RMH
- Diablo Access: https://github.com/mojsior/diablo-access
- D2R Reimagined: https://github.com/D2R-Reimagined/d2r-reimagined-mod
- D2R-Excel: https://github.com/pinkufairy/D2R-Excel

> El DOCX contiene la versión ampliada, con hipótesis, criterios de éxito, riesgos y orden sugerido.
