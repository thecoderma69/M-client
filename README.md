# MΛ ツ Client 2.0

Cliente personalizado basado en DDNet/TClient para Windows x64, preparado para descargar, extraer y jugar.

Repositorio oficial: [thecoderma69/M-client](https://github.com/thecoderma69/M-client)

## Descripcion

MΛ ツ Client es una version modificada del cliente DDNet con mejoras visuales, opciones de personalizacion, herramientas de comodidad y recursos propios.

La version 2.0 reorganiza el cliente para que las opciones principales esten mejor distribuidas entre `MΛ ツ`, `Visual` y `Recursos`, evitando duplicados y dejando la configuracion mas limpia para jugar.

## Descargar

El juego listo para usar esta en:

[Releases](https://github.com/thecoderma69/M-client/releases)

Descarga:

```txt
M-Client-v2.0-win64.zip
```

Luego extrae el `.zip` y abre:

```txt
DDNet.exe
```

## Codigo Fuente

El codigo fuente esta en:

```txt
M-source/
```

## Novedades De La Version 2.0

- Nuevo apartado `Visual` dentro de `MΛ ツ`.
- Particulas 3D con estilos: normales, corazones, estrellas, diamantes, lunas, rayos, mariposas, flores, notas musicales, calaveras, coronas, llamas y copos de nieve.
- Selector de color, cantidad, velocidad, velocidad de movimiento, opacidad y reaccion a la musica para particulas 3D.
- Estela de tee con estilos equivalentes, movimiento opcional, velocidad de movimiento, colores y reaccion a la musica.
- Efecto musica video editable, con imagen central personalizada, nombre de cancion, modo detras de todo, tamano, intensidad, opacidad, puntos y lineas de estela.
- Fondo multimedia para menu principal y fondo del juego.
- Relacion de aspecto personalizada con presets y modo manual.
- Editor de HUD integrado para mover elementos visuales del cliente.
- Traductor de chat integrado con boton compacto dentro del chat.
- Recursos nuevos para cursor y flecha, con estilos personalizados.
- Apartado `Recursos > Audio` para cambiar sonidos del juego mediante packs.
- Packs de audio incluidos: `MΛ ツ Space Pulse`, `MΛ ツ Retro Arcade`, `MΛ ツ Demon Core`, `MΛ ツ Magic Stars` y `MΛ ツ Dark Void`.
- Auto-Reactions dentro de `MΛ ツ > Configuracion`.
- Correcciones para selector de cursor/flecha, cambio de pantalla, fondo multimedia, editor de skins y estabilidad general.
- Ajustes de optimizacion para reducir carga visual innecesaria.

## Caracteristicas Principales

- Menu propio `MΛ ツ` dentro del cliente.
- Apartados organizados: Configuracion, Visual, Lluvia, Anime Love, HUD de teclas y Editor skins.
- Personalizacion visual avanzada sin tocar archivos del juego manualmente.
- Editor de skins integrado.
- Opciones de entrada/input, incluyendo modos personalizados como Saiko+.
- Selector de recursos para cursores, flechas y sonidos.
- Sistema preparado para compartir sin incluir configuraciones privadas.

## Requisitos

- Windows 10 o superior.
- Sistema de 64 bits.
- Drivers de video actualizados.
- GPU compatible con OpenGL o Vulkan.

## Como Ejecutar

1. Descarga el `.zip` desde Releases.
2. Extrae la carpeta completa.
3. Abre `DDNet.exe`.

No borres los archivos `.dll`, la carpeta `data` ni `storage.cfg`, porque son necesarios para que el cliente funcione correctamente.

## Configuraciones Del Usuario

El cliente mantiene compatibilidad con la carpeta normal de DDNet:

```txt
%APPDATA%\DDNet
```

El archivo `storage.cfg` usa estas rutas:

```txt
add_path $USERDIR
add_path $DATADIR
add_path $CURRENTDIR
```

Esto permite que el cliente pueda leer configuraciones existentes del usuario, como:

- `settings_ddnet.cfg`
- skins
- demos
- capturas
- mapas descargados
- otros datos guardados por DDNet

Tus configuraciones personales no vienen incluidas en la descarga. Cada jugador mantiene sus propios ajustes, skins, mapas y datos guardados desde su carpeta local de DDNet.

## Recursos

### Audio

Los packs de sonido se cambian desde:

```txt
Recursos > Audio
```

Para crear un pack nuevo, usa la opcion `Nueva` dentro del apartado Audio. El cliente crea una carpeta en:

```txt
assets/audio/mi_pack
```

Coloca ahi los sonidos con los nombres que muestra el cliente. Se aceptan archivos `.wv` y `.wav`.

### Cursor Y Flecha

Los cursores y flechas se cambian desde:

```txt
Recursos > Cursor
Recursos > Flecha
```

Los recursos personalizados incluidos estan dentro de:

```txt
data/assets/cursor
data/assets/arrow
```

### Efecto Musica Video

Las imagenes centrales del efecto se colocan en:

```txt
tclient/music_video_effect
```

Desde `MΛ ツ > Visual` puedes recargar la lista, elegir la imagen y ajustar tamano, color, opacidad e intensidad.

## Estructura De La Descarga

```txt
M-Client-v2.0-win64/
|-- data/                 # Archivos de datos y recursos del cliente
|-- DDNet.exe             # Ejecutable principal
|-- DDNet-Server.exe      # Ejecutable del servidor
|-- storage.cfg           # Rutas de lectura y guardado
|-- config_directory.bat  # Acceso rapido a la carpeta de configuracion
|-- LEEME_CONFIGS.txt     # Nota sobre configuraciones compartidas
|-- license.txt           # Licencia del proyecto base
`-- *.dll                 # Librerias necesarias
```

## Solucion De Problemas

Si el cliente no abre:

- Verifica que todos los `.dll` esten junto a `DDNet.exe`.
- Asegurate de no haber borrado la carpeta `data`.
- Actualiza los drivers de la tarjeta grafica.
- Cambia entre OpenGL/Vulkan si un backend grafico falla.
- Revisa la carpeta `dumps/` si aparece un error de crash.

Si tus configuraciones no aparecen:

- Abre `config_directory.bat`.
- Comprueba que tus archivos existan en `%APPDATA%\DDNet`.
- Verifica que `storage.cfg` conserve `add_path $USERDIR`.

## Creditos

Cliente personalizado por **MΛ ツ**.

Basado en DDNet/TClient y en componentes inspirados por otros clientes de la comunidad.

## Licencia

Este proyecto incluye componentes basados en DDNet/TClient. Revisa `M-source/license.txt`, `license.txt` y las licencias de los proyectos originales para mas informacion.

MΛ ツ Client es una modificacion no oficial y no esta afiliada oficialmente con DDNet.
