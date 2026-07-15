# MΛ ツ Client

Cliente personalizado basado en DDNet/TClient para Windows x64, preparado para compartir y usar sin incluir configuraciones personales del creador.

## Descripcion

MΛ ツ Client es una version modificada del cliente de DDNet con mejoras visuales, opciones de personalizacion y herramientas enfocadas en comodidad de juego. Esta carpeta contiene una compilacion lista para ejecutar en Windows de 64 bits.

El cliente mantiene compatibilidad con la carpeta de configuracion normal de DDNet en `%APPDATA%\DDNet`, por lo que puede leer ajustes, skins, demos y otros datos que el usuario ya tenga instalados.

## Caracteristicas principales

- Apartado propio `MΛ ツ` dentro de la configuracion del cliente.
- Opciones visuales personalizadas para particulas 3D.
- Estela de tee con estilos personalizados.
- Reaccion visual con musica para efectos compatibles.
- Fondo multimedia para menu y juego.
- Editor de HUD integrado.
- Editor de skins.
- Opciones de entrada/input, incluyendo modos personalizados.
- Auto-Reactions dentro de `MΛ ツ > Configuracion`.
- Sonidos personalizados y ajustes visuales adicionales.
- Preparado para compartir sin llevar configuraciones privadas.

## Requisitos

- Windows 10 o superior.
- Sistema de 64 bits.
- Drivers de video actualizados.
- Una GPU compatible con OpenGL/Vulkan.

## Como ejecutar

1. Descarga o clona este repositorio.
2. Extrae la carpeta completa si la descargaste en `.zip`.
3. Abre `DDNet.exe`.

No borres los archivos `.dll`, la carpeta `data` ni `storage.cfg`, porque son necesarios para que el cliente funcione correctamente.

## Configuraciones y datos del usuario

Este cliente usa el archivo `storage.cfg` con estas rutas:

```txt
add_path $USERDIR
add_path $DATADIR
add_path $CURRENTDIR
```

En Windows, `$USERDIR` apunta normalmente a:

```txt
%APPDATA%\DDNet
```

Eso significa que el cliente puede leer configuraciones existentes del usuario, como:

- `settings_ddnet.cfg`
- skins
- demos
- capturas
- mapas descargados
- otros datos guardados por DDNet

La carpeta `user/` no se incluye en este repositorio para evitar subir configuraciones personales, cuentas, ajustes privados o archivos locales del creador.

## Estructura de la carpeta

```txt
MΛ ツ client/
|-- data/                 # Archivos de datos del cliente
|-- DDNet.exe             # Ejecutable principal
|-- DDNet-Server.exe      # Ejecutable del servidor
|-- storage.cfg           # Rutas de lectura/guardado de datos
|-- config_directory.bat  # Acceso rapido a la carpeta de configuracion
|-- LEEME_CONFIGS.txt     # Nota sobre configuraciones compartidas
|-- license.txt           # Licencia del proyecto base
`-- *.dll                 # Librerias necesarias
```

## Recomendaciones para GitHub

Este repositorio debe incluir la compilacion lista para ejecutar, pero no deberia incluir datos personales o temporales. El archivo `.gitignore` ya excluye:

- `user/`
- `dumps/`
- `screenshots/`
- `videos/`
- archivos `.log`
- archivos `.tmp`
- archivos `.bak`

Antes de subir una nueva version, revisa que no hayas agregado carpetas con configuraciones privadas.

## Solucion de problemas

Si el cliente no abre:

- Verifica que todos los `.dll` esten junto a `DDNet.exe`.
- Asegurate de no haber borrado la carpeta `data`.
- Actualiza los drivers de la tarjeta grafica.
- Revisa la carpeta `dumps/` si aparece un error de crash.

Si tus configuraciones no aparecen:

- Abre `config_directory.bat` para revisar la carpeta usada por el cliente.
- Comprueba que tus archivos existan en `%APPDATA%\DDNet`.
- Verifica que `storage.cfg` conserve `add_path $USERDIR`.

## Creditos

Cliente personalizado por MΛ ツ.

Basado en DDNet/TClient y sus respectivos proyectos originales.

## Licencia

Este proyecto incluye componentes basados en DDNet/TClient. Revisa `license.txt` y las licencias de los proyectos originales para mas informacion.

Este cliente es una modificacion no oficial y no esta afiliado oficialmente con DDNet.
