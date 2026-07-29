# MΛ ツ Client 2.1.7

Cliente personalizado basado en DDNet/TClient para Windows x64, preparado para descargar, extraer y jugar.

Repositorio oficial: [thecoderma69/M-client](https://github.com/thecoderma69/M-client)

## Descripcion

MΛ ツ Client es una version modificada del cliente DDNet con mejoras visuales, opciones de personalizacion, herramientas de comodidad y recursos propios.

La version 2.1.7 agrega chat de stream para Twitch, fuente de actividad para directos y mejoras de seguridad para ocultar credenciales en pantalla.

## Descargar

El juego listo para usar esta en:

[Releases](https://github.com/thecoderma69/M-client/releases)

Descarga:

```txt
M-Client-v2.1.7-win64.zip
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

## Novedades De La Version 2.1.7

- Nuevo `Chat de stream` en `MΛ ツ > Visual` para ver el chat de Twitch dentro del juego.
- Nuevo apartado `Fuente de actividad` para ver estadisticas y eventos del directo.
- `Fuente de actividad` ahora tiene su propio HUD independiente: se puede mover y redimensionar sin mover el chat de stream.
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

## Novedades De La Version 2.1.5

- Nueva `Proteccion automatica de FPS` en `MΛ ツ > Configuracion > Optimizer`.
- Ajustes de rendimiento para particulas 3D, estela tee, clima y efecto musica video.
- Las particulas 3D reducen detalle, glow y cantidad automaticamente cuando el FPS baja.
- La estela tee baja carga en mapas pesados y limita estelas de otros jugadores si el rendimiento cae.
- El clima respeta la opcion de desactivar particulas y reduce spawn automaticamente.
- Texto del menu principal actualizado a `MΛ ツ 2.1.5`.

## Novedades De La Version 2.1.4

- Nuevos modelos reales para el HUD de teclas: `Redondo`, `Diamante` y `Hexagonal`.
- Selector separado para `Modelo teclado` y `Modelo mouse`.
- Vista previa del editor de HUD actualizada para mostrar los mismos modelos que se ven en partida.
- Texto del menu principal actualizado a `MΛ ツ 2.1.4`.

## Novedades De La Version 2.1.3

- MA input mejorado con offset adaptativo mas fluido.
- Nuevos ajustes internos para reducir sensacion de lag visual.
- Valores recomendados para MA: `Auto`, `Intensidad MA 55%`, `Estabilidad MA 75%`.
- Texto del menu principal actualizado a `MΛ ツ 2.1.3`.
- Enlace de desarrollador actualizado al GitHub nuevo.

## Novedades De La Version 2.1.2

- Nueva opcion `Musica de inicio` en `MΛ ツ > Visual`.
- Voz de bienvenida predeterminada en español con estilo futurista.
- Selector para usar audios propios desde `data/ma/startup_music`.
- Correccion de volumen para que la voz de inicio se escuche con mas fuerza.
- Compatibilidad con rutas anteriores de musica de inicio.

## Novedades De La Version 2.1.1

- El reproductor de musica puede ocultar el timer original del juego y usar su propio tiempo como HUD principal.
- El contador de tees vivos/congelados ahora se puede mover y escalar desde el editor de HUD.
- El efecto musica video engancha el ritmo mas rapido al iniciar una cancion.
- Correccion para evitar reconfigurar el visualizador de audio en cada frame.
- Ajustes del editor de HUD para mover y redimensionar elementos con mas precision.

## Novedades De La Version 2.1

- Fondo multimedia con opcion `Usar otro fondo para juego`.
- Selector separado para `Fondo del menu` y `Fondo del juego`.
- Control de `Opacidad del fondo del juego`.
- Boton `General` en `Recursos > Audio` para abrir la carpeta principal de packs.
- Build actualizado para compartir como release nueva.

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

Tambien puedes usar `General` para abrir directamente:

```txt
assets/audio
```

Desde ahi puedes copiar una carpeta completa de pack de audio.

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

### Chat De Stream Y Fuente De Actividad

Estas opciones estan pensadas para streamers que quieren ver informacion del directo mientras juegan.

Ruta dentro del cliente:

```txt
Configuracion > MΛ ツ > Visual
```

Dentro de `Visual` encontraras dos bloques separados:

```txt
Chat de stream
Fuente de actividad
```

`Chat de stream` muestra mensajes del chat en el juego. `Fuente de actividad` muestra eventos y estadisticas del directo, como espectadores reales, mensajes y pico de espectadores.

#### Activar Chat De Stream

1. Abre el cliente.
2. Entra a `Configuracion`.
3. Entra a `MΛ ツ`.
4. Abre la pestana `Visual`.
5. Busca `Chat de stream`.
6. Activa `Activar chat de stream`.
7. En `Plataforma`, selecciona `Twitch`.
8. En `Canal / URL`, escribe tu canal. Puedes usar cualquiera de estos formatos:

```txt
tu_canal
https://twitch.tv/tu_canal
twitch.tv/tu_canal
```

9. Ajusta `Lineas visibles`, `Opacidad texto`, `Opacidad fondo` y `Color texto` a tu gusto.
10. Usa `Reconectar` si cambiaste el canal o si el chat no conecta al primer intento.

#### Activar Fuente De Actividad

1. En la misma ruta `Configuracion > MΛ ツ > Visual`, baja hasta `Fuente de actividad`.
2. Activa `Mostrar fuente de actividad` si quieres ver eventos del directo.
3. Activa `Mostrar estadisticas` si quieres ver resumen de mensajes, chatters y tiempo activo.
4. Activa `Mostrar espectadores reales` si quieres consultar espectadores desde la API de la plataforma.
5. En `Fuente`, puedes elegir:

```txt
Usar plataforma del chat
Twitch
YouTube
Kick
```

Si usas `Usar plataforma del chat`, la fuente toma la misma plataforma elegida en `Chat de stream`.

#### Configurar Twitch Para Espectadores Reales

Para Twitch necesitas un `Client ID` y un `Access Token`. No necesitas pegar tu `Client Secret` dentro del juego. El secret se usa solo para generar el token y debe quedarse privado.

1. Entra a Twitch Developer Console.
2. Crea o abre tu aplicacion.
3. Copia el `Client ID`.
4. Genera un `Client Secret` nuevo si no tienes uno.
5. Abre PowerShell y usa este ejemplo con tus datos reales:

```powershell
$clientId = "PEGA_AQUI_TU_CLIENT_ID"
$clientSecret = Read-Host "Pega tu Client Secret de Twitch"

$r = Invoke-RestMethod -Method Post -Uri "https://id.twitch.tv/oauth2/token" -Body @{
  client_id = $clientId
  client_secret = $clientSecret
  grant_type = "client_credentials"
}

$r.access_token
```

6. PowerShell te mostrara un token. Copia solo ese token.
7. En el cliente, vuelve a `MΛ ツ > Visual > Fuente de actividad`.
8. Activa `Mostrar espectadores reales`.
9. En `Fuente`, selecciona `Twitch` o `Usar plataforma del chat` si tu chat ya esta en Twitch.
10. Coloca tu `Client ID` en `Twitch Client ID`.
11. Coloca el token generado en `Twitch token`.
12. Ajusta `Actualizar espectadores cada`. Un valor entre 30 y 60 segundos es suficiente para evitar muchas consultas.

Los campos de credenciales se ven ocultos en pantalla. Aun asi, evita mostrar la pagina de Twitch Developer Console en directo, porque ahi el secret puede verse completo.

#### Configurar YouTube

Para YouTube la fuente de actividad usa la API de YouTube y el video en vivo.

1. En `Chat de stream`, coloca la URL o ID del video en vivo en `Canal / URL`.
2. En `Fuente de actividad`, selecciona `YouTube`.
3. Activa `Mostrar espectadores reales`.
4. Coloca tu `YouTube API key` en el campo correspondiente.

Formatos aceptados para el video:

```txt
https://www.youtube.com/watch?v=ID_DEL_VIDEO
https://youtu.be/ID_DEL_VIDEO
https://www.youtube.com/live/ID_DEL_VIDEO
ID_DEL_VIDEO
```

#### Configurar Kick

Para Kick se usa un token de API y el broadcaster ID numerico.

1. En `Fuente de actividad`, selecciona `Kick`.
2. Activa `Mostrar espectadores reales`.
3. Coloca tu token en `Kick token`.
4. Coloca el ID numerico del streamer en `Kick broadcaster ID`.

Si no colocas el broadcaster ID numerico, el cliente mostrara que Kick necesita ese dato.

#### Mover Chat Y Fuente De Actividad En El HUD

El chat y la fuente de actividad ahora son elementos separados.

1. Entra a una partida o a una demo.
2. Abre `Configuracion > MΛ ツ > Visual`.
3. En `Chat de stream`, pulsa `Editar HUD`.
4. En el editor veras dos cajas distintas:

```txt
Chat de stream
Fuente de actividad
```

5. Arrastra `Chat de stream` para mover solo el chat.
6. Arrastra `Fuente de actividad` para mover solo las estadisticas/eventos.
7. Usa las esquinas o bordes del editor para cambiar tamano.
8. Cierra el editor y el juego guardara la posicion automaticamente.

La posicion de `Fuente de actividad` se guarda independiente del chat, asi que puedes poner el chat abajo a la izquierda y las estadisticas en otra zona de la pantalla.

#### Recomendaciones Para Directos

- No muestres tus claves reales ni el panel de desarrollador de Twitch/YouTube/Kick en stream.
- Usa un token nuevo si alguna vez se vio por accidente.
- Manten `Actualizar espectadores cada` entre 30 y 60 segundos.
- Si algo no actualiza, pulsa `Reconectar` en `Chat de stream` o desactiva/activa `Mostrar espectadores reales`.
- Para compartir el cliente con amigos, no incluyas tu carpeta `%APPDATA%\DDNet` ni archivos de configuracion personales.

## Estructura De La Descarga

```txt
M-Client-v2.1.7-win64/
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
