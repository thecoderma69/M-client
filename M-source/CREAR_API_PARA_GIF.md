# Crear API para GIF en GIPHY

Este tutorial explica como crear una API key de GIPHY para usar el buscador de GIF del cliente.

La API key sirve para que el apartado GIF pueda buscar GIFs online desde GIPHY. Sin esa key, el cliente puede mostrar la base local, pero no podra buscar todos los GIFs desde internet.

## 1. Entrar a GIPHY Developers

1. Abre esta pagina:

   https://developers.giphy.com/

2. Inicia sesion o crea una cuenta.
3. Entra al panel de desarrollador.
4. Busca el boton o apartado llamado `Create an API Key`.

## 2. Elegir el tipo correcto

Cuando aparezca la pantalla `Create A New API Key 1/2`, GIPHY mostrara dos opciones:

- `SDK`
- `API`

Para el cliente debes elegir:

```text
API
```

No elijas `SDK`. El cliente solo necesita una API key normal para usar los endpoints de busqueda de GIPHY.

## 3. Completar el formulario

En la pantalla `Create A New API Key 2/2`, completa asi:

### Your App Name

Puedes colocar:

```text
M Client
```

O si quieres usar el nombre completo:

```text
MΛ ツ Client
```

### Platform

Selecciona:

```text
Other
```

### App Description

Puedes copiar este texto:

```text
GIF search for my custom DDNet client. The API key will be used to search and preview GIFs in the in-game GIF browser, chat media, and GIF wheel.
```

### Terms

Marca la casilla donde aceptas los terminos de GIPHY API.

Despues presiona:

```text
Create API Key
```

## 4. Copiar la API key

Cuando GIPHY cree la key, copia el texto completo de la API key.

La key normalmente es una cadena larga de letras y numeros.

Ejemplo de formato:

```text
AbCdEf123456xxxxxxxxxxxx
```

No uses ese ejemplo. Debes copiar la key real que te entregue GIPHY.

## 5. Pegar la key en el cliente

Abre el juego y entra a:

```text
Configuracion > MΛ ツ > GIF
```

En el apartado `Buscador GIF`, selecciona:

```text
GIPHY
```

Luego pega la key en:

```text
GIPHY API key
```

Despues de pegarla, usa el boton:

```text
Recargar
```

Ahora el buscador deberia cargar GIFs desde GIPHY.

## 6. Como probar si funciona

1. Entra a `MΛ ツ > GIF`.
2. Selecciona `GIPHY`.
3. Escribe algo en el buscador, por ejemplo:

```text
cat
```

4. Si aparecen GIFs, la API key esta funcionando.
5. Puedes arrastrar un GIF hacia la rueda GIF para guardarlo en la posicion que quieras.

## 7. Notas importantes

- GIPHY entrega keys beta al principio.
- Las keys beta tienen limite de uso. Segun la documentacion oficial de GIPHY, las beta keys tienen limite de 100 busquedas o llamadas API por hora.
- Si muchas personas usan la misma key, ese limite se puede llenar rapido.
- Para compartir el cliente con muchas personas, lo mejor es que cada usuario cree su propia key o que mas adelante pidas a GIPHY subir la key a produccion.
- No publiques una API key privada en GitHub si no quieres que otras personas la usen.

## 8. Si no aparecen GIFs

Revisa esto:

1. Que este seleccionado `GIPHY`, no `Local`.
2. Que la key este bien copiada, sin espacios al inicio o al final.
3. Que tengas internet.
4. Que no se haya superado el limite de la key beta.
5. Que hayas presionado `Recargar` despues de pegar la key.

## Links oficiales

- GIPHY Developers: https://developers.giphy.com/
- Documentacion de GIPHY API: https://developers.giphy.com/docs/api/
