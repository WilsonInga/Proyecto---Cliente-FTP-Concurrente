# Proyecto - Cliente FTP Concurrente

Este proyecto es una implementacion de un cliente FTP en C, disenado para ejecutarse en sistemas UNIX (Linux).

El cliente implementa los comandos basicos del RFC 959 y utiliza **concurrencia** para manejar las transferencias de archivos, permitiendo que el canal de control permanezca activo mientras las transferencias de datos estan en progreso.

## Caracteristicas

- **Concurrencia Real:** Utiliza `fork()` para crear un nuevo proceso hijo por cada transferencia de datos. Esto permite al proceso padre seguir aceptando comandos del usuario.
- **Manejo de Modos de Datos:**
  - **Modo Pasivo (PASV):** Implementado para los comandos `get`, `put`, y `dir`.
  - **Modo Activo (PORT):** Implementado con el comando `pput`.
- **Gestion de Procesos:** Incluye un manejador de senales `SIGCHLD` para limpiar procesos "difuntos" y evitar la saturacion de recursos del sistema.
- **Comandos Implementados:**
  - `USER <usuario>` y `PASS <clave>`
  - `PASV` y `PORT`
  - `RETR <archivo>` (get)
  - `STOR <archivo>` (put/pput)
  - `LIST` (dir)
  - `CWD <dir>` (cd)
  - `QUIT`
  - `PWD`
  - `MKD <dir>`
  - `DELE <archivo>`
  - `REST <bytes>`

## Arquitectura del Cliente

El cliente opera bajo un modelo concurrente.

1. **Proceso Padre (Canal de Control):**

   - Establece la conexion con el servidor vsftpd.
   - Maneja la interaccion con el usuario.
   - Envia comandos y recibe respuestas.

2. **Procesos Hijos (Canal de Datos):**

   - Se crean mediante `fork()` cuando se ejecuta `get`, `put`, o `dir`.
   - El proceso hijo realiza toda la transferencia de datos.
   - El proceso padre continua atendiendo comandos del usuario.

3. **Modo Activo (PORT):**
   - Para `pput`, el cliente actua como servidor de datos.
   - Usa `passiveTCP` para abrir un socket de escucha.
   - **Requiere un puerto temporal (puerto 0)** para que el OS asigne un puerto libre.
   - Esto **necessito modificar el archivo `passivesock.c` original**, el cual no aceptaba `service = "0"`.

## Archivos del Proyecto

- `clienteFTP.c`: Logica principal, concurrencia y manejo del protocolo FTP.
- `Makefile`: Automatiza la compilacion.
- `connectsock.c`: Crea sockets activos (cliente).
- `connectTCP.c`: Wrapper para connectsock usando TCP.
- `passivesock.c`: Biblioteca para crear sockets pasivos.
  - **Modificado:** El codigo original trataba el servicio "0" como un error. Se agrego una condicion para permitir `service = "0"`, habilitando puertos temporales necesarios para el Modo Activo.
- `passiveTCP.c`: Envoltorio para passivesock usando TCP.
- `errexit.c`: Manejo de errores.

---

# 🛠 Nota sobre la Modificacion en `passivesock.c`

El archivo `passivesock.c` original esta disenado solo para servidores que usan puertos fijos (ftp, telnet, etc.). Su comportamiento original es:

```

else if ((sin.sin_port = htons((unsigned short)atoi(service))) == 0)
errexit("can't get "%s" service entry\n", service);

```

Esto significa que si se intenta usar el servicio `"0"`, la funcion **termina con error**, porque asume que `0` nunca es valido.

## Por que es un problema?

El comando FTP **PORT** requiere que el cliente abra un socket de escucha en un **puerto temporal** (puerto 0), para que el sistema operativo asigne un puerto disponible dinamicamente. Sin esto, el Modo Activo **es imposible** de implementar.

## Solucion implementada:

Se modifico el archivo y se agrego esta condicion:

```c
if (strcmp(service, "0") == 0) {
    sin.sin_port = htons(0);  // Solicitar puerto temporal asignado por el OS
}
```

Esto permite usar:

```
passiveTCP("0", 1);
```

y obtener un puerto valido mediante `getsockname()`, que luego se utiliza para construir el comando:

```
PORT h1,h2,h3,h4,p1,p2
```

Sin esta modificacion, el Modo Activo (`pput`) no funcionaria.

---

## Compilacion

Para compilar:

```
make
```

Para limpiar:

```
make clean
```

## Uso

```
./clienteFTP <host> [puerto]
```

---

## Ejemplo de Sesion (Prueba de Concurrencia)

Crear un archivo grande:

```
dd if=/dev/zero of=archivo_grande.dat bs=1M count=50
```

Conectarse y autenticarse:

```
./clienteFTP localhost
```

Probar concurrencia:

```
ftp> put archivo_grande.dat
ftp> pwd
257 "/home/usuario"
```

El comando `pwd` responde aun mientras el archivo se transfiere en segundo plano.

---

## Autor

- Wilson Inga+
