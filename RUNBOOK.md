# Cuarzito Pre-Show Game Runbook

## Deploy

- Qt asumido en: `C:/Qt/6.7.3/msvc2022_64`
- Ejecutable objetivo: `under_attack_cuarzito_race.exe`
- Soporte XInput disponible en Windows.
- Para mandos PlayStation/DualSense:
  - `SDL3.dll` debe estar junto a `under_attack_cuarzito_race.exe` o disponible en `PATH`
  - se carga dinámicamente mediante `SdlControllerBackend`
  - el despliegue previsto parte de `libs/SDL3.dll`

## Inicio de la aplicación

- La aplicación arranca en pantalla completa por defecto.
- Modo de prueba por CLI:
  - `under_attack_cuarzito_race.exe --test downhill`
  - `under_attack_cuarzito_race.exe --test uphill`
  - `under_attack_cuarzito_race.exe --test vertical`
- En modo `--test`:
  - se omiten intro y countdown
  - arranca directamente en `Playing`
  - fuerza invulnerabilidad
  - termina al acabar la pista de prueba
  - escribe trazas en `stderr`

## Manejo de la aplicación

### Teclado

| Acción | Tecla |
|---|---|
| Mover | Flechas / `WASD` |
| Acelerar / confirmar / empezar | `Space` / `Enter` |
| Frenar | `Shift` / `Ctrl` |
| Cancelar | `Escape` |
| Reiniciar partida actual | `R` |
| Alternar pantalla completa | `F11` |
| Alternar vista tercera/primera persona | `V` |
| Alternar invulnerabilidad | `I` |
| Alternar guide mode | `G` |
| Seleccionar pista en attract mode | `Left` / `Right` |

### Mando

| Acción | Mando |
|---|---|
| Mover | Left stick / D-pad |
| Acelerar / confirmar / empezar | `A` / `R2` / `Start` |
| Frenar | `B` / `L2` |
| Cancelar | `B` / `Back` |

## Consideraciones de operación

- Mantener teclado como fallback fiable para eventos en vivo.
- `demo_tunnel` es la pista corta de pruebas; `live_tunnel` es la ruta larga.
- La intro pasa directamente a `Playing`; no hay countdown después de la intro.
- La invulnerabilidad está activada por defecto al inicio de partida para demo/testing.
- `guide mode` está activado por defecto.
- El rectángulo de safe-zone visible durante gameplay es una ayuda de depuración.
