# Cuarzito Pre-Show Game

Minijuego arcade de pre-show para la experiencia teatral **Cuarzito**. Está hecho en **C++ / Qt** para Windows y está pensado para ejecutarse en monitor o TV grande.

## Qué es

El jugador controla a **Cuarzito**, una pequeña figura encapuchada, mientras persigue cuatro gemas por una cueva/túnel cósmica. El diseño actual ya no es un juego de esquiva: es un juego de persecución con pista, energía, puntuación y finales según resultado.

## Diseño del juego

- Cuarzito recorre una pista de túnel definida por segmentos.
- Las cuatro gemas avanzan por delante del jugador.
- Chocar con paredes, suelo o techo reduce energía y velocidad.
- La partida termina al llegar al final de la pista, capturar todas las gemas o agotar la energía.
- El objetivo visual es una cueva oscura, legible a distancia y adecuada para uso escénico.

## Arquitectura y tecnología

Estado actual del prototipo:

- `MainWindow` crea e incrusta `GameWidget`.
- `GameWidget` es un `QOpenGLWidget` y posee bucle, render y entrada.
- `GameScene` concentra estado del juego, actualización y dibujo auxiliar.
- `CaveRenderer` genera el túnel/cueva y el fondo visual.
- `InputManager` abstrae teclado, XInput y SDL3 opcional.

Stack técnico:

| Capa | Tecnología |
|---|---|
| Lenguaje | C++23 |
| Framework | Qt 6.7.3 |
| Plataforma principal | Windows |
| Ventana/render principal | `QOpenGLWidget` |
| Dibujo 2D/HUD | `QPainter` |
| Audio | `Qt::Multimedia` |
| Persistencia | JSON para puntuaciones |
| Entrada | teclado, XInput y SDL3 opcional |

## Estado actual

- Juego Qt jugable con flujo de attract, intro, countdown, partida y pantallas finales.
- Túnel basado en pista segmentada cargada desde JSON de recursos.
- Dos pistas integradas: `demo_tunnel` y `live_tunnel`.
- Compatibilidad con teclado y mando.
- HUD, puntuación, energía, efectos de impacto y ranking local.
- Render actual basado en `QPainter` sobre `QOpenGLWidget`, aislado para poder evolucionar después.

## Estructura relevante

```text
cuarzito_race/
├── .claude/CLAUDE.md
├── CODEX.md
├── README.md
├── RUNBOOK.md
├── NEXT_STEPS.md
├── CMakeLists.txt
├── docs/
├── resources/
└── src/
```

## Referencias visuales

- `resources/images/cuarzito.png`: referencia del personaje.
- `resources/images/cave.png`: referencia del entorno de cueva/túnel.
