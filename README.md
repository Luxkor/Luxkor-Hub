# Luxkor Hub

Centro para instalar, actualizar y desinstalar mis aplicaciones sin ir al repositorio.
App nativa de Windows (C + Win32 + GDI+), ~80 KB.

## Descargar
Ultima version en **Releases** (`LuxkorHub.exe`).

## Como aparece una app en el hub
1. El repo debe ser publico y estar en la cuenta `Luxkor`.
2. Anadir el topic `luxkor-app` al repo.
3. Publicar un Release (tag `v1.0.0`) con un unico `.exe` adjunto.
4. En el hub, pulsar **Actualizar**.

## Compilar
`build.sh` (mingw-w64) o el workflow de GitHub Actions (Actions > build > Run workflow, o al subir un tag `v*`).
