/* clienteFTP.c - clienteFTP */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#define LINELEN 1024
#define BUFSIZE 4096

extern int connectTCP(const char *host, const char *service);
extern int passiveTCP(const char *service, int qlen);
extern int errexit(const char *format, ...);

/* Envia cmds FTP al servidor, recibe respuestas y las despliega */
void sendCmd(int s, char *cmd, char *res) {
    int n;
    char buffer[LINELEN];

    // Copiamos el comando al buffer para no modificar el original
    strncpy(buffer, cmd, LINELEN);

    n = strlen(buffer);
    buffer[n] = '\r';   /* formatear cmd FTP: \r\n al final */
    buffer[n+1] = '\n';
    buffer[n+2] = '\0'; // Aseguramos fin de string

    if (write(s, buffer, n+2) < 0) /* envia cmd por canal de control */
        errexit("Fallo al escribir en el socket: %s\n", strerror(errno));

    n = read (s, res, LINELEN); /* lee respuesta del svr */
    if (n < 0)
        errexit("Fallo al leer del socket: %s\n", strerror(errno));
        
    res[n] = '\0'; /* despliega respuesta */
    printf ("%s", res); // Usamos %s (sin \n) porque el servidor ya lo incluye
}

void readRes(int s, char *res) {
    int n = read (s, res, LINELEN); /* lee respuesta del svr */
    if (n < 0)
        errexit("Fallo al leer del socket: %s\n", strerror(errno));
        
    res[n] = '\0';
    printf ("%s", res); // Muestra la respuesta
}

/* envia cmd PASV; recibe IP,pto del SVR; se conecta al SVR y retorna sock conectado */
int pasivo (int s){
    int sdata;			/* socket para conexion de datos */
    int nport;			/* puerto (en numeros) en SVR */
    char cmd[128], res[LINELEN], *p;  /* comando y respuesta FTP */
    char host[64], port[8];	/* host y port del SVR (como strings) */
    int h1,h2,h3,h4,p1,p2;	/* octetos de IP y puerto del SVR */

    sprintf (cmd, "PASV");
    sendCmd(s, cmd, res); //

    /* Parsear la respuesta: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2). */
    p = strchr(res, '(');
    if (p == NULL) {
        printf("Error: El servidor no devolvio un modo pasivo valido.\n");
        return -1;
    }
    
    sscanf(p+1, "%d,%d,%d,%d,%d,%d", &h1,&h2,&h3,&h4,&p1,&p2);
    snprintf(host, 64, "%d.%d.%d.%d", h1,h2,h3,h4);
    nport = p1*256 + p2;
    snprintf(port, 8, "%d", nport);

    /* Conectarse al puerto de datos que indico el servidor */
    sdata = connectTCP(host, port);
    if (sdata < 0) {
        errexit("Error al conectar (pasivo) a %s:%s\n", host, port);
    }
    
    return sdata;
}

/* Implementacion de MODO ACTIVO (PORT) */
int activo(int s_ctrl) {
    int slisten;
    struct sockaddr_in sin;
    socklen_t sin_len = sizeof(sin);
    char cmd[128], res[LINELEN];
    char my_ip_str[INET_ADDRSTRLEN];
    unsigned int my_port;
    
    // Crear un socket pasivo en un puerto temporal (0)
    // Usamos "0" para que el SO elija un puerto
    slisten = passiveTCP("0", 1); 

    // Obtener la IP y el puerto que nos asigno el SO
    if (getsockname(slisten, (struct sockaddr *)&sin, &sin_len) < 0)
        errexit("getsockname: %s\n", strerror(errno));
    
    // Obtenemos el puerto asignado en orden de host
    my_port = ntohs(sin.sin_port);

    // Para la IP, obtenemos la IP local de la conexion de control
    if (getsockname(s_ctrl, (struct sockaddr *)&sin, &sin_len) < 0)
        errexit("getsockname (control): %s\n", strerror(errno));
        
    inet_ntop(AF_INET, &sin.sin_addr, my_ip_str, sizeof(my_ip_str));
    
    // Formatear el comando PORT h1,h2,h3,h4,p1,p2
    // Reemplazamos los '.' de la IP por ','
    for (char *p = my_ip_str; *p; ++p) {
        if (*p == '.') *p = ',';
    }
    
    // Formateamos el comando PORT
    sprintf(cmd, "PORT %s,%d,%d", my_ip_str, (my_port >> 8) & 0xFF, my_port & 0xFF);
    
    // Enviar el comando PORT
    sendCmd(s_ctrl, cmd, res);
    if (res[0] != '2') { // Esperamos un "200 PORT command successful."
        printf("Error: Comando PORT fallo.\n");
        close(slisten);
        return -1;
    }

    return slisten;
}


void ayuda () {
    printf ("Cliente FTP Concurrente. Comandos disponibles:\n"
            " help \t\t- Despliega este texto\n"
            " dir \t\t- Lista el directorio actual del servidor (LIST)\n"
            " get <archivo>\t- Copia el archivo desde el servidor (RETR, PASV)\n"
            " put <archivo>\t- Copia el archivo hacia el servidor (STOR, PASV)\n"
            " pput <archivo>\t- Copia el archivo hacia el servidor (STOR, PORT)\n"
            " cd <dir>\t- Cambia al directorio 'dir' en el servidor (CWD)\n"
            " pwd\t\t- Muestra el directorio de trabajo actual (PWD)\n"
            " mkdir <dir>\t- Crea el directorio 'dir' en el servidor (MKD)\n"
            " dele <archivo>\t- Borra el 'archivo' en el servidor (DELE)\n"
            " rest <bytes>\t- Fija el punto de reinicio (REST)\n"
            " quit\t\t- Finaliza la sesion FTP (QUIT)\n\n");
}

void salir (char *msg) {
    printf ("%s\n", msg);
    exit (1);
}

/* Manejador para limpiar procesos hijos difuntos */
void reaper(int sig) {
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0); 
}

/* Funciones de Transferencia Concurrente */

// Descarga un archivo (RETR)
void do_get(int s_ctrl, char *filename) {
    int sdata, pid, n;
    char cmd[128], res[LINELEN];
    char buffer[BUFSIZE];
    FILE *local_file;

    sdata = pasivo(s_ctrl); // Establecer canal de datos PASV
    if (sdata < 0) return;

    sprintf(cmd, "RETR %s", filename); // Enviar comando RETR
    sendCmd(s_ctrl, cmd, res);

    if (res[0] != '1') { // Esperamos "150 Opening data connection..."
        printf("Error: No se pudo iniciar la transferencia.\n");
        close(sdata);
        return;
    }

    pid = fork(); 
    if (pid < 0) errexit("fork: %s\n", strerror(errno));
    
    if (pid == 0) { 
        close(s_ctrl); // El hijo no usa el canal de control
        
        local_file = fopen(filename, "wb");
        if (local_file == NULL)
            errexit("Error al crear archivo local: %s\n", strerror(errno));
        
        printf("...Iniciando descarga de %s...\n", filename);
        while ((n = read(sdata, buffer, BUFSIZE)) > 0) {
            fwrite(buffer, 1, n, local_file);
        }
        
        fclose(local_file);
        close(sdata);
        printf("...Descarga de %s completada...\n", filename);
        exit(0); // El hijo termina
    
    } else { // Proceso Padre
        close(sdata); // El padre no usa el canal de datos
        readRes(s_ctrl, res); // Espera "226 Transfer complete."
    }
}

// Sube un archivo (STOR) usando PASV
void do_put(int s_ctrl, char *filename, int modo_activo) {
    int sdata, slisten = -1, pid, n;
    char cmd[128], res[LINELEN];
    char buffer[BUFSIZE];
    FILE *local_file;

    /* Preparar la conexion */
    if (modo_activo) {
        // En modo activo, obtenemos el socket que ESCUCHA (todavia sin conexion)
        slisten = activo(s_ctrl);
        if (slisten < 0) return;
    } else {
        // En modo pasivo, obtenemos el socket CONECTADO
        sdata = pasivo(s_ctrl); 
        if (sdata < 0) return;
    }

    /* Enviar comando STOR */
    sprintf(cmd, "STOR %s", filename); 
    sendCmd(s_ctrl, cmd, res);

    if (res[0] != '1') { // Esperamos "150 Opening data connection..."
        printf("Error: No se pudo iniciar la transferencia.\n");
        if (modo_activo) close(slisten);
        else close(sdata);
        return;
    }

    /* Completar conexion (Solo Modo Activo) */
    if (modo_activo) {
        // AHORA que enviamos STOR, el servidor intentara conectarse. Aceptamos.
        sdata = accept(slisten, (struct sockaddr *)NULL, NULL);
        close(slisten); // Ya no necesitamos escuchar
        if (sdata < 0) {
            printf("Error al aceptar conexion de datos (Modo Activo).\n");
            return;
        }
    }

    /*  Transferencia (Fork)*/
    pid = fork(); 
    if (pid < 0) errexit("fork: %s\n", strerror(errno));
    
    if (pid == 0) { // Hijo
        close(s_ctrl);
        
        local_file = fopen(filename, "rb");
        if (local_file == NULL) {
            printf("Error: No se pudo abrir '%s'.\n", filename);
            close(sdata);
            exit(1);
        }
        
        printf("...Iniciando subida de %s...\n", filename);
        while ((n = fread(buffer, 1, BUFSIZE, local_file)) > 0) {
            if (write(sdata, buffer, n) < 0) break;
        }
        
        fclose(local_file);
        close(sdata);
        printf("...Subida de %s completada...\n", filename);
        exit(0);
    
    } else { // Padre
        close(sdata);
        readRes(s_ctrl, res); // Espera "226 Transfer complete."
    }
}
// Lista el directorio (LIST)
void do_dir(int s_ctrl) {
    int sdata, pid, n;
    char cmd[128], res[LINELEN];
    char buffer[BUFSIZE];

    sdata = pasivo(s_ctrl); // Establecer canal de datos PASV
    if (sdata < 0) return;

    sprintf(cmd, "LIST");
    sendCmd(s_ctrl, cmd, res);

    if (res[0] != '1') { // Esperamos "150..."
        printf("Error: No se pudo listar el directorio.\n");
        close(sdata);
        return;
    }

    pid = fork(); // Crear proceso hijo concurrente
    if (pid < 0) errexit("fork: %s\n", strerror(errno));
    
    if (pid == 0) { // Proceso Hijo 
        close(s_ctrl); // El hijo no usa el canal de control
        
        printf("...Listando directorio...\n");
        while ((n = read(sdata, buffer, BUFSIZE)) > 0) {
            write(STDOUT_FILENO, buffer, n); // Escribir a la salida estandar
        }
        
        close(sdata);
        printf("...Fin del listado...\n");
        exit(0); // El hijo termina
    
    } else { // Proceso Padre
        close(sdata); // El padre no usa el canal de datos
        readRes(s_ctrl, res); // Espera "226 Transfer complete."
    }
}


/* Programa Principal */

int main(int argc, char *argv[]) {
    int s_ctrl;         /* socket para el canal de control */
    char *host;
    char *service = "ftp"; /* servicio puerto 21 */
    char res[LINELEN];  /* buffer para respuestas del servidor */
    char cmd[LINELEN], arg[LINELEN]; /* comando y argumento del usuario */
    char user_cmd[LINELEN];
    
    switch (argc) {
        case 2:
            host = argv[1];
            break;
        case 3:
            host = argv[1];
            service = argv[2];
            break;
        default:
            fprintf(stderr, "Uso: %s host [port]\n", argv[0]);
            exit(1);
    }

    /* Instalar el manejador de senales para hijos difuntos */
    signal(SIGCHLD, reaper);

    /* Conectar al canal de control del servidor FTP */
    s_ctrl = connectTCP(host, service);
    
    /* El servidor FTP envia un mensaje 220 de bienvenida */
    readRes(s_ctrl, res);
    if (res[0] != '2')
        salir("Error: El servidor no esta listo.");

    /* Autenticacion*/
    printf("Usuario (%s): ", host);
    fgets(user_cmd, sizeof(user_cmd), stdin);
    user_cmd[strcspn(user_cmd, "\n")] = 0; // Quitar newline
    
    sprintf(cmd, "USER %s", user_cmd);
    sendCmd(s_ctrl, cmd, res); //
    if (res[0] == '5') // 530 Not logged in
        salir("Error de autenticacion (USER).");
    
    /* Pedir contrasena */
    printf("Contrasena: ");
    fgets(user_cmd, sizeof(user_cmd), stdin);
    user_cmd[strcspn(user_cmd, "\n")] = 0; // Quitar newline
    
    sprintf(cmd, "PASS %s", user_cmd);
    sendCmd(s_ctrl, cmd, res); //
    if (res[0] != '2') // Esperamos "230 User logged in"
        salir("Error de autenticacion (PASS).");

    /* Bucle de Comandos */
    printf("\nCliente FTP concurrente. Escriba 'help' para ayuda.\n");
    
    while (1) {
        printf("ftp> ");
        
        if (fgets(user_cmd, sizeof(user_cmd), stdin) == NULL)
            break; 
        
        /* Parsear el comando y el argumento */
        cmd[0] = arg[0] = '\0';
        sscanf(user_cmd, "%s %s", cmd, arg);

        if (cmd[0] == '\0') // No se ingreso nada
            continue;

        /* Manejo de Comandos */

        if (strncmp(cmd, "help", 4) == 0) {
            ayuda();
        } 
        else if (strncmp(cmd, "quit", 4) == 0) {
            sprintf(cmd, "QUIT");
            sendCmd(s_ctrl, cmd, res);
            break; // Salir del bucle
        }
        else if (strncmp(cmd, "get", 3) == 0) {
            if (arg[0] == '\0') printf("Uso: get <archivo>\n");
            else do_get(s_ctrl, arg);
        }
        else if (strncmp(cmd, "pput", 4) == 0) {
            if (arg[0] == '\0') printf("Uso: pput <archivo>\n");
            else do_put(s_ctrl, arg, 1); // 1 = modo activo
        }
        else if (strncmp(cmd, "put", 3) == 0) {
            if (arg[0] == '\0') printf("Uso: put <archivo>\n");
            else do_put(s_ctrl, arg, 0); // 0 = modo pasivo
        }
        else if (strncmp(cmd, "dir", 3) == 0) {
            do_dir(s_ctrl);
        }
        else if (strncmp(cmd, "cd", 2) == 0) {
            if (arg[0] == '\0') printf("Uso: cd <directorio>\n");
            else {
                sprintf(cmd, "CWD %s", arg);
                sendCmd(s_ctrl, cmd, res);
            }
        }
        else if (strncmp(cmd, "pwd", 3) == 0) {
            sprintf(cmd, "PWD");
            sendCmd(s_ctrl, cmd, res); //
        }
        else if (strncmp(cmd, "mkdir", 5) == 0) {
            if (arg[0] == '\0') printf("Uso: mkdir <directorio>\n");
            else {
                sprintf(cmd, "MKD %s", arg);
                sendCmd(s_ctrl, cmd, res); //
            }
        }
        else if (strncmp(cmd, "dele", 4) == 0) {
            if (arg[0] == '\0') printf("Uso: dele <archivo>\n");
            else {
                sprintf(cmd, "DELE %s", arg);
                sendCmd(s_ctrl, cmd, res); //
            }
        }
        else if (strncmp(cmd, "rest", 4) == 0) {
            if (arg[0] == '\0') printf("Uso: rest <bytes>\n");
            else {
                sprintf(cmd, "REST %s", arg);
                sendCmd(s_ctrl, cmd, res); //
            }
        }
        else {
            printf("Comando desconocido. Escriba 'help' para ayuda.\n");
        }
    } 
    close(s_ctrl);
    printf("Sesion FTP finalizada.\n");
    return 0;
}
