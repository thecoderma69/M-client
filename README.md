# MΛ ツ Client 2.2.2

Cliente personalizado basado en DDNet/TClient para Windows x64, preparado para descargar, extraer y jugar.

Repositorio oficial: [thecoderma69/M-client](https://github.com/thecoderma69/M-client)

## Descripcion

MΛ ツ Client es una version modificada del cliente DDNet con mejoras visuales, opciones de personalizacion, herramientas de comodidad, recursos propios y apartados especiales para stream, HUD, GIFs, musica y efectos visuales.

La version 2.2.2 mejora el apartado GIF: agrega favoritos, apartados/perfiles para la rueda GIF y permite elegir cuantos GIFs quieres colocar en cada apartado.

## Descargar

El juego listo para usar esta en:

[Releases](https://github.com/thecoderma69/M-client/releases)

Descarga:

```txt
M-Client-v2.2.2-win64.zip
```

Luego extrae el `.zip`. Dentro encontraras una carpeta:

```txt
M-Client-v2.2.2-win64/
```

Abre el juego desde:

```txt
M-Client-v2.2.2-win64/DDNet.exe
```

## Codigo Fuente

El codigo fuente esta en:

```txt
M-source/
```

## Novedades De La Version 2.2.2

- Nuevo sistema de `Favoritos` en el buscador GIF.
- La rueda GIF ahora tiene `Apartado 1`, `Apartado 2`, `Apartado 3` y `Apartado 4` como perfiles separados.
- Cada apartado de la rueda puede tener una cantidad editable de GIFs con `GIFs en este apartado`.
- Cada apartado permite de 1 a 32 espacios para GIFs.
- Los primeros 8 espacios antiguos se mantienen compatibles para no perder los GIFs que ya estaban guardados.
- El apartado `En rueda` muestra los GIFs del apartado seleccionado.
- Se ajusto el tamano visual de los GIFs de la rueda para que quepan mejor cuando hay muchos.
- Texto del menu principal actualizado a la version `2.2.2`.

## Novedades De La Version 2.2.1

- Nuevo ajuste en `MΛ ツ > Configuracion > Estadisticas de team`: `Mostrar solo al final del mapa`.
- Si esa opcion esta desactivada, las estadisticas de team se muestran siempre como antes.
- Al terminar el mapa se escribe un resumen local en el chat del cliente, visible solo para quien usa el cliente.
- Se corrigio que el panel de estadisticas desapareciera cuando no correspondia.
- Texto del menu principal actualizado a la version `2.2.1`.

## Novedades De La Version 2.2

- Buscador GIF simplificado: se quito Tenor del apartado y se dejo `Local` + `GIPHY`.
- La rueda GIF permite colocar cada GIF en una posicion exacta arrastrandolo al hueco que quieras.
- Puedes mover GIFs ya puestos dentro de la rueda arrastrandolos a otra posicion.
- Se agregaron slots fijos visibles en la rueda para que no se asignen de forma aleatoria.
- Se corrigio un crash al iniciar cuando habia GIFs guardados en la configuracion.
- Se agrego el tutorial `CREAR_API_PARA_GIF.md` para explicar como obtener la API key de GIPHY.
- El `.zip` del release incluye una carpeta principal para que al descomprimir no queden todos los archivos sueltos.
- Texto del menu principal actualizado a `MΛ ツ 2.2`.

## Novedades De La Version 2.1.7

- Nuevo `Chat de stream` en `MΛ ツ > Visual` para ver el chat de Twitch dentro del juego.
- Nuevo apartado `Fuente de actividad` para ver estadisticas y eventos del directo.
- `Fuente de actividad` tiene su propio HUD independiente: se puede mover y redimensionar sin mover el chat de stream.
- El HUD de `Fuente de actividad` guarda posicion y escala automaticamente.
- Los campos sensibles de Twitch, YouTube y Kick se muestran ocultos como contrasena para que no se vean en directo.
- Correccion del guardado de posicion del HUD del chat de stream.
- Texto del menu principal actualizado a `MΛ ツ 2.1.7`.

## Novedades De La Version 2.1.6

- Correccion de carga para GIFs e imagenes del chat.
- Tenor ya no usa la API antigua que devolvia error y ahora resuelve el medio desde la pagina real.
- Imgur prioriza formatos mas compatibles y livianos para evitar quedarse en `Downloading media...`.
- Se agrego progreso visible al descargar medios del chat.
- Se agrego proteccion para cancelar descargas colgadas y probar otro formato automaticamente.
- Texto del menu principal actualizado a `MΛ ツ 2.1.6`.

## Tutoriales Incluidos

Dentro del cliente se incluyen estos archivos:

```txt
CREAR_API_PARA_GIF.md
TUTORIAL_FUENTE_DE_ACTIVIDAD.txt
```

`CREAR_API_PARA_GIF.md` explica paso a paso como crear una API key de GIPHY y donde pegarla dentro del cliente.

## Notas De Configuracion

El cliente lee las configuraciones desde la carpeta normal de DDNet en `%appdata%`, por lo que cada jugador conserva sus propios ajustes, skins, configs y preferencias locales.

La carpeta `user/` no se sube al repositorio para evitar publicar configuraciones personales, cuentas, dumps o archivos privados.