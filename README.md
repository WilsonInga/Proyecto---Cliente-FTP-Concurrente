# Cliente FTP Concurrente

Este proyecto implementa un cliente FTP funcional compatible con el estándar RFC 959. Soporta operaciones concurrentes mediante el uso de procesos, permitiendo mantener el canal de control activo mientras se realizan transferencias de datos en segundo plano. Además, soporta tanto el **Modo Pasivo** como el **Modo Activo**.

## 1. Configuración del Servidor.

Para que todas las funcionalidades del cliente funcionen correctamente, el servidor FTP `vsftpd` debe estar configurado para permitir escritura y conexiones activas.

**Archivo:** `/etc/vsftpd.conf`

```ini
# Permitir usuarios locales y escritura
local_enable=YES
write_enable=YES

# Permitir Modo Pasivo y Activo
pasv_enable=YES
port_enable=YES

# Configuración de seguridad para Modo Activo (Puerto 20)
connect_from_port_20=YES

# Solución al error 550 en chroot (Permitir escribir en la raíz)
chroot_local_user=YES
allow_writeable_chroot=YES
```

**Nota:** Después reiniciar el servicio:

```bash
sudo systemctl restart vsftpd
```

-----

## 2. Compilación

El proyecto incluye un `Makefile` para automatizar la construcción.

1.  **Limpiar compilaciones anteriores:**

    ```bash
    make clean
    ```

2.  **Compilar el proyecto:**

    ```bash
    make
    ```

    Esto generará el ejecutable binario `clienteFTP`.

-----

## 3. Preparación de Archivos de Prueba

Para verificar la transferencia de archivos, se recomienda crear dos archivos de prueba en la carpeta del proyecto **antes** de ejecutar el cliente.

1.  **Archivo pequeño (para pruebas rápidas):**

    ```bash
    echo "Hola Mundo FTP" > archivo_pequeno.txt
    ```

2.  **Archivo grande (para probar la concurrencia):**
    Este comando crea un archivo de 50MB.

    ```bash
    dd if=/dev/zero of=archivo_grande.dat bs=1M count=50
    ```

-----

## 4. Ejecución y Uso

Ejecute el cliente indicando la dirección del servidor (use `localhost` para pruebas locales).

```bash
./clienteFTP localhost
```

Ingrese su usuario y contraseña del sistema cuando se le solicite.

### Lista de Comandos

| Comando | Descripción | Modo FTP |
| :--- | :--- | :--- |
| `help` | Muestra la ayuda. | - |
| `pwd` | Muestra el directorio remoto actual. | - |
| `dir` | Lista archivos del directorio remoto. | Pasivo |
| `cd <dir>` | Cambia de directorio remoto. | - |
| `mkdir <dir>` | Crea un directorio en el servidor. | - |
| `dele <archivo>` | Borra un archivo en el servidor. | - |
| `get <archivo>` | Descarga un archivo del servidor. | Pasivo |
| `put <archivo>` | Sube un archivo al servidor. | Pasivo |
| `pput <archivo>` | Sube un archivo usando **Modo Activo**. | **Activo (PORT)** |
| `quit` | Cierra la sesión y sale. | - |

-----

## 5. Verificación de Funcionalidades Clave

### A. Prueba de Concurrencia

El objetivo es demostrar que el cliente no se bloquea durante una transferencia larga.

1.  Inicie la subida del archivo grande:
    `ftp> put archivo_grande.dat`
2.  **Inmediatamente**, mientras se realiza la subida, escriba otro comando:
    `ftp> pwd`
3.  **Resultado Esperado:** El cliente debe responder con la ruta del directorio (`257 ...`) **antes** de que aparezca el mensaje `226 Transfer complete` de la subida.

### B. Prueba de Modo Activo (`pput`)

Verifica la implementación del comando `PORT`.

1.  Ejecute: `ftp> pput archivo_pequeno.txt`
2.  **Resultado Esperado:**
      * El cliente abre un puerto local efímero.
      * Envía `PORT h1,h2...`.
      * Recibe `200 PORT command successful`.
      * El servidor inicia la conexión y transfiere el archivo.

-----

## Notas de Implementación

### Estructura de Archivos

  * `clienteFTP.c`: Lógica principal, manejo de comandos y `fork()` para procesos hijos.
  * `connectsock.c` / `connectTCP.c`: Funciones para crear conexiones salientes (Canal de Control y Datos en modo Pasivo).
  * `passivesock.c` / `passiveTCP.c`: Funciones para crear sockets de escucha (Para modo Activo).
  * `errexit.c`: Utilidad de manejo de errores.

### Modificación Técnica Importante

Se realizó una modificación en la Función **`passivesock.c`** original.
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
