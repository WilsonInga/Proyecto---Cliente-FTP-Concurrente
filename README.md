# Cliente FTP Concurrente

Este proyecto implementa un cliente FTP compatible con el estándar RFC 959.  
Soporta operaciones **concurrentes** mediante `fork()`, permitiendo mantener el canal de control activo mientras las transferencias de datos se ejecutan en segundo plano.  
El cliente funciona tanto en **Modo Pasivo (PASV)** como **Modo Activo (PORT)**.

---

## 1. Configuración del Servidor vsftpd

Para usar todas las funciones (incluyendo Modo Activo), el servidor debe permitir escritura y conexiones activas.

Archivo: `/etc/vsftpd.conf`

```ini
local_enable=YES
write_enable=YES
pasv_enable=YES
port_enable=YES
connect_from_port_20=YES
chroot_local_user=YES
allow_writeable_chroot=YES
````

Reiniciar servicio:

```
sudo systemctl restart vsftpd
```

---

## 2. Compilación

El proyecto incluye un Makefile:

```
make clean
make
```

Genera el ejecutable:

```
clienteFTP
```

---

## 3. Archivos de Prueba

Archivo pequeño:

```
echo "Hola Mundo FTP" > archivo_pequeno.txt
```

Archivo grande (50MB):

```
dd if=/dev/zero of=archivo_grande.dat bs=1M count=50
```

---

## 4. Ejecución

Ejecutar:

```
./clienteFTP localhost
```

Autenticarse con usuario y contraseña del sistema.

### Comandos disponibles

| Comando        | Descripción        | Modo     |
| -------------- | ------------------ | -------- |
| help           | Ayuda              | -        |
| pwd            | Directorio actual  | -        |
| dir            | Listar directorio  | PASV     |
| cd <dir>       | Cambiar directorio | -        |
| mkdir <dir>    | Crear carpeta      | -        |
| dele <archivo> | Eliminar archivo   | -        |
| get <archivo>  | Descargar archivo  | PASV     |
| put <archivo>  | Subir archivo      | PASV     |
| pput <archivo> | Subir archivo      | **PORT** |
| quit           | Salir              | -        |

---

## 5. Pruebas Importantes

### A. Concurrencia

1. Ejecutar transferencia grande:

```
ftp> put archivo_grande.dat
```

2. Inmediatamente ejecutar otro comando:

```
ftp> pwd
```

**Resultado esperado:**
`pwd` responde aunque la transferencia siga ejecutándose.

### B. Modo Activo (pput)

```
ftp> pput archivo_pequeno.txt
```

Debe aparecer:

* `200 PORT command successful`
* Transferencia iniciada por el servidor

---

## 6. Notas de Implementación

### Estructura del proyecto

* `clienteFTP.c`: Lógica principal y concurrencia (`fork`).
* `connectsock.c` / `connectTCP.c`: Conexiones salientes.
* `passivesock.c` / `passiveTCP.c`: Sockets de escucha (Modo Activo).
* `errexit.c`: Manejo de errores.

### Modificación necesaria en `passivesock.c`

El archivo original NO permitía usar:

```
service = "0"
```

y ejecutaba:

```c
else if ((sin.sin_port = htons((unsigned short)atoi(service))) == 0)
    errexit("can't get \"%s\" service entry\n", service);
```

Esto es un problema porque el comando FTP **PORT** requiere un **puerto temporal** asignado por el sistema operativo.

#### Solución implementada

Se agregó soporte explícito para `"0"`:

```c
if (strcmp(service, "0") == 0) {
    sin.sin_port = htons(0);   // Solicitar puerto temporal
}
```

Esto permite:

```
passiveTCP("0", 1);
```

y obtener el puerto real con `getsockname()`, necesario para construir:

```
PORT h1,h2,h3,h4,p1,p2
```

Sin esta modificación, el Modo Activo (`pput`) no funciona.
