#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ifaddrs.h>

#include "comunicacion.h"

int mensaje_no_copiado = 1;                                 // Condicion para el mutex de paso de mensajes
pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;   // Mutex para ejecucion de operaciones en direcotrio y ficheros
pthread_mutex_t mutex_msg = PTHREAD_MUTEX_INITIALIZER;      // Mutex para control de paso de mensajes
pthread_cond_t condvar_msg = PTHREAD_COND_INITIALIZER;      // Var. condicion asociada al mutex de paso de mensajes



// Prototipos de la API del lado servidor
char* register_serv(char *user, char *alias, char *date);
char* unregister_serv(char *alias);
char* connect_serv(char *alias, char *IP, char *port_escucha);
char* disconnect_serv(char *alias);
Response send_serv(char *alias, char *destino, char *mensaje);
Response connected_users_serv(char *alias);

// Función para tratar las peticiones de los clientes
void tratar_pet (void* pet){

    // Estructuras de datos de entrada y salida
    Request peticion;
    Service respuesta;

    // Hilo obtiene la peticion del cliente
    pthread_mutex_lock(&mutex_msg);
    peticion = (*(Request *) pet);
    mensaje_no_copiado = 0;
    pthread_cond_signal(&condvar_msg);
    pthread_mutex_unlock(&mutex_msg);

    if (strcmp(peticion.op, "REGISTER") == 0){
        
        // Obtiene acceso exclusivo al directorio "users" y sus ficheros
        pthread_mutex_lock(&users_mutex);
        respuesta.status = register_serv(peticion.content.usuario, peticion.content.alias, peticion.content.fecha);
        pthread_mutex_unlock(&users_mutex);

        // Se envia la respuesta al cliente con el status
        if (sendMessage(peticion.sock_client, respuesta.status, strlen(respuesta.status)+1) == -1)
            perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente\n");
            
    } else if(strcmp(peticion.op, "UNREGISTER") == 0){

        // Obtiene acceso exclusivo al directorio "users" y sus ficheros
        pthread_mutex_lock(&users_mutex);
        respuesta.status = unregister_serv(peticion.content.alias);
        pthread_mutex_unlock(&users_mutex);

        // Se envia la respuesta al cliente con el status
        if (sendMessage(peticion.sock_client, respuesta.status, strlen(respuesta.status)+1) == -1)
            perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente\n");

    } else if(strcmp(peticion.op, "CONNECT") == 0){

        // Obtiene acceso exclusivo al directorio "users" y sus ficheros
        pthread_mutex_lock(&users_mutex);
        respuesta.status = connect_serv(peticion.content.alias, peticion.content.IP, peticion.content.port_escucha);

        // Se envia la respuesta al cliente con el status
        if (sendMessage(peticion.sock_client, respuesta.status, strlen(respuesta.status)+1) == -1)
            perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente\n");

        // Si todo ha salido bien se procede a enviar los posibles mensajes que tenga pendientes
        if (strcmp(respuesta.status, "0") == 0){

            // Se crea la ruta al fichero de mensajes del usuario
            char ruta_fichero[2048];
            sprintf(ruta_fichero, "users/register_%s/msg_%s.dat",peticion.content.alias, peticion.content.alias);
            
            // Creamos el fichero de mensajes
            FILE* user_fp;

            if ((user_fp = fopen(ruta_fichero, "r+")) == NULL){
                perror("[SERVIDOR][ERROR] El fichero de mensajes del usuario no pudo ser abierto\n");
            }

            Message mensaje;
            // Se lee cada mensaje del fichero y se manda uno por uno
            while (fread(&mensaje, sizeof(Message), 1, user_fp)) {

                User emisor;

                // Se crea la ruta al fichero de registro del emisor
                char ruta_emisor[2048];
                sprintf(ruta_emisor, "users/register_%s/data_%s.dat",mensaje.alias_emisor, mensaje.alias_emisor);
                
                // Creamos el fichero de registro del emisor
                FILE* emisor_fp;

                if ((emisor_fp = fopen(ruta_emisor, "r+")) == NULL){
                    perror("[SERVIDOR][ERROR] El fichero de mensajes del emisor no pudo ser abierto\n");
                    strcpy(respuesta.content.status, "2");
                }

                if (fread(&emisor, sizeof(User), 1, emisor_fp) == 0){
                    perror("[SERVIDOR][ERROR] No se pudo leer el fichero del emisor\n");
                    fclose(emisor_fp);
                    strcpy(respuesta.content.status, "2");
                }

                // Hace falta comprobar que el emisor este conectado para poder enviarle el ACK
                if (emisor.estado == 1) {

                    // Información del servidor y cliente
                    int sock_client_fd;
                    struct sockaddr_in serv_addr;
                    unsigned short puerto = (unsigned short) atoi(peticion.content.port_escucha);

                    // Inicializamos a 0
                    bzero((char *)&serv_addr, sizeof(serv_addr));

                    // Dirección y puerto del socket de escucha del cliente
                    serv_addr.sin_family = AF_INET;
                    serv_addr.sin_addr.s_addr = inet_addr(peticion.content.IP);
                    serv_addr.sin_port = htons(puerto);
                    // Creación socket para comunicaciones con el cliente
                    if ((sock_client_fd = socket(AF_INET, SOCK_STREAM, 0))==-1) {
                        perror("[SERVIDOR][ERROR] No se pudo crear socket de comunicaciones con el cliente\n");
                        strcpy(respuesta.content.status, "2");
                    }

                    // Conectamos con el socket de escucha del cliente, se continúa solo si es exitoso
                    if (connect(sock_client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
                        perror("[SERVIDOR][ERROR] No se pudo conectar con el socket de escucha del cliente\n");
                        if(close(sock_client_fd) == -1) {
                            perror("[SERVIDOR][ERROR] No se pudo cerrar el socket de comunicaciones con el cliente\n");
                            strcpy(respuesta.content.status, "2");
                        }
                    } else {
                        
                        char operacion[14];
                        strcpy(operacion, "RECEIVE_MESS");

                        // Enviamos la operación y depués los 3 campos del mensaje al receptor
                        if (sendMessage(sock_client_fd, operacion, strlen(operacion)+1) == -1){
                            perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (operacion)\n");
                            strcpy(respuesta.content.status, "2");
                        }
                        if (sendMessage(sock_client_fd, mensaje.id_emisor, strlen(mensaje.id_emisor)+1) == -1){
                            perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (id del mensaje)\n");
                            strcpy(respuesta.content.status, "2");
                        }

                        if (sendMessage(sock_client_fd, mensaje.alias_emisor, strlen(mensaje.alias_emisor)+1) == -1) {
                            perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (emisor)\n");
                            strcpy(respuesta.content.status, "2");
                        }

                        if (sendMessage(sock_client_fd, mensaje.mensaje, strlen(mensaje.mensaje)+1) == -1){
                            perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (mensaje)\n");
                            strcpy(respuesta.content.status, "2");
                        }

                        printf("s> MESSAGE %s FROM %s TO %s\n", mensaje.id_emisor, mensaje.alias_emisor, peticion.content.alias);

                        // Si el status sigue siendo 0, todo ha salido bien y se envía el ACK
                        if (strcmp(respuesta.content.status, "0") == 0){

                            // Información del servidor y emisor
                            int sock_emisor_fd;
                            struct sockaddr_in serv_addr;
                            unsigned short puerto = (unsigned short) atoi(emisor.port_escucha);

                            // Inicializamos a 0
                            bzero((char *)&serv_addr, sizeof(serv_addr));

                            // Dirección y puerto del socket de escucha del emisor
                            serv_addr.sin_family = AF_INET;
                            serv_addr.sin_addr.s_addr = inet_addr(emisor.IP);
                            serv_addr.sin_port = htons(puerto);
                            // Creación socket para comunicaciones con el emisor
                            if ((sock_emisor_fd = socket(AF_INET, SOCK_STREAM, 0))==-1) {
                                perror("[SERVIDOR][ERROR] No se pudo crear socket de comunicaciones con el emisor\n");
                            }

                            // Conectamos con el socket de escucha del emisor se continúa solo si ha salido bien
                            if (connect(sock_emisor_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
                                perror("[SERVIDOR][ERROR] No se pudo conectar con el socket de escucha del emisor\n");
                                if(close(sock_emisor_fd) == -1) {
                                    perror("[SERVIDOR][ERROR] No se pudo cerrar el socket de comunicaciones con el emisor\n");
                                    strcpy(respuesta.content.status, "2");
                                }
                            } else {

                                strcpy(operacion, "SEND_MESS_ACK");
                                // Enviamos la operacion
                                if (sendMessage(sock_emisor_fd, operacion, strlen(operacion)+1) == -1){
                                    perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (operacion)\n");
                                    strcpy(respuesta.content.status, "2");
                                }
                                // Enviamos el id del mensaje
                                if (sendMessage(sock_emisor_fd, mensaje.id_emisor, strlen(mensaje.id_emisor)+1) == -1){
                                    perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (id del mensaje)\n");
                                    strcpy(respuesta.content.status, "2");
                                }
                                // Cerramos el socket emisor
                                if(close(sock_emisor_fd) == -1) {
                                    perror("[SERVIDOR][ERROR] No se pudo cerrar el socket de comunicaciones\n");
                                    strcpy(respuesta.content.status, "2");
                                }
                                 
                            }
                        }
                        // Cerramos el socket receptor
                        if(close(sock_client_fd) == -1) {
                            perror("[SERVIDOR][ERROR] No se pudo cerrar el socket de comunicaciones\n");
                            strcpy(respuesta.content.status, "2");
                        }
                    }
                }
                // Cerramos el archivo de registro del emisor
                fclose(emisor_fp);
            }

            // Cerramos el archivo de mensajes del receptor
            fclose(user_fp);

            // Abrimos el archivo de mensajes de nuevo y cerramos para borrar su contenido
            FILE* user_fp2;
            if ((user_fp2 = fopen(ruta_fichero, "wb")) == NULL){
                perror("[SERVIDOR][ERROR] El fichero de mensajes del usuario no pudo ser abierto tras la lectura de mensajes\n");
                strcpy(respuesta.content.status, "2");
            }
            fclose(user_fp2);
        }

        pthread_mutex_unlock(&users_mutex);        
        
    } else if(strcmp(peticion.op, "DISCONNECT") == 0){

            // Obtiene acceso exclusivo al directorio "users" y sus ficheros
            pthread_mutex_lock(&users_mutex);
            respuesta.status = disconnect_serv(peticion.content.alias);
            pthread_mutex_unlock(&users_mutex);

            // Se envia la respuesta al cliente con el status
            if (sendMessage(peticion.sock_client, respuesta.status, strlen(respuesta.status)+1) == -1)
                perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente\n");
                
    } else if (strcmp(peticion.op, "SEND") == 0){
        
        // Obtiene acceso exclusivo al directorio "users" y sus ficheros
        pthread_mutex_lock(&users_mutex);
        respuesta.content = send_serv(peticion.content.alias, peticion.content.destinatario, peticion.content.mensaje); 

        // Si se recibe 1 o 2, ha surgido algún error
        if ((strcmp(respuesta.content.status, "1") == 0) || (strcmp(respuesta.content.status, "2") == 0)){
            // Se envia la respuesta de status al cliente
            if (sendMessage(peticion.sock_client, respuesta.content.status, strlen(respuesta.content.status)+1) == -1)
                perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (status)\n");

        // Si se recibe 0, todo ha salido bien y ambos usuarios están conectados por lo que se envía directamente
        } else if (strcmp(respuesta.content.status, "0") == 0){
        
            char id_emisor2[32];
            sprintf(id_emisor2, "%u", respuesta.content.id);

            // Primero se envían el status y el id
            if (sendMessage(peticion.sock_client, respuesta.content.status, strlen(respuesta.content.status)+1) == -1)
                perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (status)\n");

            if (sendMessage(peticion.sock_client, id_emisor2, strlen(id_emisor2)+1) == -1)
                perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (status)\n");

            // Creamos el el socket para conectarnos al hilo de escucha del receptor
            // Información del servidor y cliente
            int sock_client_fd;
            struct sockaddr_in serv_addr;
            unsigned short puerto = (unsigned short) atoi(respuesta.content.port_escucha);

            // Inicializamos a 0
            bzero((char *)&serv_addr, sizeof(serv_addr));

            // Dirección y puerto del socket de escucha del cliente
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_addr.s_addr = inet_addr(respuesta.content.IP);
            serv_addr.sin_port = htons(puerto);

            // Creación socket para comunicaciones con el cliente
            if ((sock_client_fd = socket(AF_INET, SOCK_STREAM, 0))==-1) {
                perror("[SERVIDOR][ERROR] No se pudo crear socket de comunicaciones con el cliente\n");
            }

            // Conectamos con el socket de escucha del cliente
            if (connect(sock_client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
                perror("[SERVIDOR][ERROR] No se pudo conectar con el socket de escucha del cliente\n");
                if(close(sock_client_fd) == -1) {
                    perror("[SERVIDOR][ERROR] No se pudo cerrar el socket de comunicaciones con el cliente\n");
                }
            } else {

                Message mensaje;
                sprintf(mensaje.id_emisor, "%u", respuesta.content.id);
                sprintf(mensaje.alias_emisor, "%s", peticion.content.alias);
                sprintf(mensaje.mensaje, "%s", peticion.content.mensaje);

                // Enviamos el mensaje al destinatario tras la operación
                char operacion[14];
                strcpy(operacion, "RECEIVE_MESS");

                if (sendMessage(sock_client_fd, operacion, strlen(operacion)+1) == -1){
                    perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (operacion)\n");
                    strcpy(respuesta.content.status, "2");
                }
                if (sendMessage(sock_client_fd, mensaje.id_emisor, strlen(mensaje.id_emisor)+1) == -1){
                    perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (id del mensaje)\n");
                    strcpy(respuesta.content.status, "2");
                }
                if (sendMessage(sock_client_fd, mensaje.alias_emisor, strlen(mensaje.alias_emisor)+1) == -1) {
                    perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (emisor)\n");
                    strcpy(respuesta.content.status, "2");
                }
                if (sendMessage(sock_client_fd, mensaje.mensaje, strlen(mensaje.mensaje)+1) == -1){
                    perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (mensaje)\n");
                    strcpy(respuesta.content.status, "2");
                }
                
                // Cerramos el socket
                if(close(sock_client_fd) == -1) {
                    perror("[SERVIDOR][ERROR] No se pudo cerrar el socket de comunicaciones\n");
                    strcpy(respuesta.content.status, "2");
                }

                // Si todo ha salido bien se envía el ACK al emisor
                if (strcmp(respuesta.content.status, "0") == 0){
                        User emisor;

                        // Se crea la ruta al fichero de registro del emisor
                        char ruta_emisor[2048];
                        sprintf(ruta_emisor, "users/register_%s/data_%s.dat",mensaje.alias_emisor, mensaje.alias_emisor);
                        
                        // Creamos el fichero de registro del emisor
                        FILE* emisor_fp;

                        if ((emisor_fp = fopen(ruta_emisor, "r+")) == NULL){
                            perror("[SERVIDOR][ERROR] El fichero de mensajes del emisor no pudo ser abierto\n");
                            strcpy(respuesta.content.status, "2");
                        }
                        
                        // Lo leemos
                        if (fread(&emisor, sizeof(User), 1, emisor_fp) == 0){
                            perror("[SERVIDOR][ERROR] No se pudo leer el fichero del emisor\n");
                            fclose(emisor_fp);
                            strcpy(respuesta.content.status, "2");
                        }

                        // Información del servidor y emisor
                        int sock_emisor_fd;
                        struct sockaddr_in serv_addr;
                        unsigned short puerto = (unsigned short) atoi(emisor.port_escucha);

                        // Inicializamos a 0
                        bzero((char *)&serv_addr, sizeof(serv_addr));

                        // Dirección y puerto del socket de escucha del emisor
                        serv_addr.sin_family = AF_INET;
                        serv_addr.sin_addr.s_addr = inet_addr(emisor.IP);
                        serv_addr.sin_port = htons(puerto);
                        // Creación socket para comunicaciones con el emisor
                        if ((sock_emisor_fd = socket(AF_INET, SOCK_STREAM, 0))==-1) {
                            perror("[SERVIDOR][ERROR] No se pudo crear socket de comunicaciones con el emisor\n");
                        }

                        // Conectamos con el socket de escucha del emisor, se continúa solo si ha salido correctamente
                        if (connect(sock_emisor_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
                            perror("[SERVIDOR][ERROR] No se pudo conectar con el socket de escucha del emisor\n");
                            if(close(sock_emisor_fd) == -1) {
                                perror("[SERVIDOR][ERROR] No se pudo cerrar el socket de comunicaciones con el emisor\n");
                            }
                        } else {
                            strcpy(operacion, "SEND_MESS_ACK");
                            // Enviamos la operacion
                            if (sendMessage(sock_emisor_fd, operacion, strlen(operacion)+1) == -1){
                                perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (operacion)\n");
                                strcpy(respuesta.content.status, "2");
                            }
                            // Enviamos el id del mensaje
                            if (sendMessage(sock_emisor_fd, mensaje.id_emisor, strlen(mensaje.id_emisor)+1) == -1){
                                perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (id del mensaje)\n");
                                strcpy(respuesta.content.status, "2");
                            }
                            // Cerramos el socket
                            if(close(sock_emisor_fd) == -1) {
                                perror("[SERVIDOR][ERROR] No se pudo cerrar el socket de comunicaciones\n");
                                strcpy(respuesta.content.status, "2");
                            }
                        }  
                }
                printf("s> MESSAGE %s FROM %s TO %s\n", mensaje.id_emisor, mensaje.alias_emisor, peticion.content.destinatario);
            }

        // Si el status es 3, todo ha salido correctamente pero el receptor no esta conectado por lo que se guarda en su fichero
        } else if ((strcmp(respuesta.content.status, "3")) == 0){
            
            Message mensaje;
            sprintf(mensaje.id_emisor, "%u", respuesta.content.id);
            sprintf(mensaje.alias_emisor, "%s", peticion.content.alias);
            sprintf(mensaje.mensaje, "%s", peticion.content.mensaje);

            // Escribimos el mensaje en el fichero de mensajes del destinatario
            FILE* fichero_mensajes;
            if ((fichero_mensajes = fopen(respuesta.content.pend_mensajes, "ab")) == NULL) {
                perror("[SERVIDOR][ERROR] No se pudo abrir el fichero de mensajes del destinatario\n");
                respuesta.content.status = "2";
            }
            // Escribimos el mensaje en el fichero
            if (fwrite(&mensaje, sizeof(Message), 1, fichero_mensajes) == 0) {
                perror("[SERVIDOR][ERROR] No se pudo escribir el mensaje en el fichero de mensajes del destinatario\n");
                respuesta.content.status = "2";
            }

            fclose(fichero_mensajes);

            // Para seguir el protocolo se debe cambiar el 3 por un 0 (ya que es una operación exitosa)
            respuesta.content.status = "0";

            // Enviamos el status al emisor
            if (sendMessage(peticion.sock_client, respuesta.content.status, strlen(respuesta.content.status)+1) == -1)
                perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (status)\n");

            if (sendMessage(peticion.sock_client, mensaje.id_emisor, strlen(mensaje.id_emisor)+1) == -1)
                perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (status)\n");

            printf("s> MESSAGE %s FROM %s TO %s STORED\n", mensaje.id_emisor, mensaje.alias_emisor, peticion.content.destinatario);

        }
        pthread_mutex_unlock(&users_mutex);

    } else if (strcmp(peticion.op, "CONNECTEDUSERS") == 0){

        // Obtiene acceso exclusivo al directorio "users" y sus ficheros
        pthread_mutex_lock(&users_mutex);
        respuesta.content = connected_users_serv(peticion.content.alias);

        // Se envia la respuesta de status al cliente
        if (sendMessage(peticion.sock_client, respuesta.content.status, strlen(respuesta.content.status)+1) == -1)
            perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (status)\n");
        
        // Si el status es 0, todo ha salido correctamente y se envía la lista de conectados uno a uno
        if (strcmp(respuesta.content.status, "0") == 0) {
            // Pasamos a string el número de usuarios conectados
            char num_users[32];
            sprintf(num_users, "%d", respuesta.content.num_users);

            // Se envia el número de usuarios conectados
            if (sendMessage(peticion.sock_client, num_users, strlen(num_users)+1) == -1)
                perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (número de usuarios)\n");

            // Se separan los usuarios conectados por comas
            char *persona = strtok(respuesta.content.users, ",");
            while (persona != NULL) {
                // Se envían los usuarios conectados
                if (sendMessage(peticion.sock_client, persona, strlen(persona)+1) == -1)
                    perror("[SERVIDOR][ERROR] No se pudo enviar la respuesta al cliente (usuarios)\n");
                persona = strtok(NULL, ",");
            }
        }
        pthread_mutex_unlock(&users_mutex);

    } else {
        perror("[SERVIDOR][ERROR] Operación no válida\n");
    }
    // Se cierra el socket del cliente
    if (close(peticion.sock_client) == -1){
        perror("[SERVIDOR][ERROR] Socket del cliente no pudo cerrarse\n");
    }
    pthread_exit(NULL);
}


int main(int argc, char* argv[]){

    // Estructura para agrupar la información
    Request peticion;

    // Manejo de hilos
    pthread_t thid;
    pthread_attr_t th_attr; 

    // Informacion para el socket del servidor y del cliente
    int sock_serv_fd, sock_client_fd;
    struct sockaddr_in serv_addr, client_addr;

    // Variables para guardar lo recibido
    char op[15];
    char usuario[64];
    char destinatario[64];
    char alias[32];
    char fecha[11];
    char client_ip[INET_ADDRSTRLEN];
    char port_escucha[6];
    char mensaje[256];

    // Se comprueba que se ha introducido el puerto como argumento
    if (argc != 3){
        printf("[SERVIDOR][ERROR] Debe introducir el puerto como argumento\n");
        return -1;
    }

    if (strcmp(argv[1], "-p") != 0){
        printf("[SERVIDOR][ERROR] La forma de ejecución es <./servidor_pf -p PUERTO>\n");
        return -1;
    }

    // Se almacena el puerto introducido como argumento y se comprueba que es valido
    short puerto = (short) atoi(argv[2]);

    if (puerto < 1024 || puerto > 65535){
        printf("[SERVIDOR][ERROR] El puerto introducido no es valido\n");
        return -1;
    }

    // Se obtiene la IP de la interfaz de red eth0
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char *desiredInterface = "eth0";
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        perror("Error al obtener la información de las interfaces de red");
        return 1;
    }

    // Recorre la lista de interfaces de red
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }

        family = ifa->ifa_addr->sa_family;

        // Filtra las interfaces de red IPv4 y coincide con el nombre deseado
        if (family == AF_INET && strcmp(ifa->ifa_name, desiredInterface) == 0) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                            host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("[SERVIDOR][ERROR] Error al obtener la dirección IP del servidor: %s\n", gai_strerror(s));
                return 1;
            }

        }
    }

    // Se muestra mensaje de inicio con la IP y el puerto del servidor
    printf("init server %s:%d\n", host, puerto);
    printf("s>\n");

    freeifaddrs(ifaddr);

    // Se crea el socket del servidor
    if ((sock_serv_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[SERVIDOR][ERROR] No se pudo crear socket de recepción de peticiones\n");
        return -1;
    }
    
    // Se establece la opcion de reutilizacion de direcciones
    int val = 1;
    setsockopt(sock_serv_fd, SOL_SOCKET, SO_REUSEADDR, (char *) &val, sizeof(int));

    // Se inicializa la estructura de datos para el socket del servidor
    socklen_t client_addr_len = sizeof(client_addr);
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(puerto);

    // Se enlaza el socket del servidor con la direccion y puerto y se procede a ponerlo en modo escucha
    if (bind(sock_serv_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1){
        perror("[SERVIDOR][ERROR] No se pudo enlazar el socket de recepción de peticiones\n");
        return -1;
    }

    if (listen(sock_serv_fd, SOMAXCONN) == -1){
        perror("[SERVIDOR][ERROR] No se pudo poner el socket en modo escucha\n");
        return -1;
    }

    // Se inicializa el mutex para el directorio "users" y sus ficheros
    pthread_attr_init(&th_attr);
    pthread_attr_setdetachstate(&th_attr,PTHREAD_CREATE_DETACHED);

    // Bucle para aceptar conexiones de clientes
    while(1){

        // Se acepta la conexion del cliente
        if ((sock_client_fd = accept(sock_serv_fd, (struct sockaddr*) &client_addr, &client_addr_len)) == -1){
            perror("[SERVIDOR][ERROR] No se pudo aceptar la conexión del cliente\n");
            break;
        }

        // Convertir dirección IP del cliente a una cadena legible
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);  

        // Se recibe la operación a realizar
        if (readLine(sock_client_fd, (char*) &op, sizeof(op)) == -1){
            perror("[SERVIDOR][ERROR] No se pudo recibir la petición del cliente (operacion)\n");
            break;
        }

        if (strcmp(op, "REGISTER") != 0){
            // Se recibe el alias
            if (readLine(sock_client_fd, (char*) &alias, sizeof(alias)) == -1){
                perror("[SERVIDOR][ERROR] No se pudo recibir la petición del cliente (alias)\n");
                break;
            }
        }

        if (strcmp(op, "REGISTER") == 0){

            // Se recibe el usuario
            if (readLine(sock_client_fd, (char*) &usuario, sizeof(usuario)) == -1){
                perror("[SERVIDOR][ERROR] No se pudo recibir la petición del cliente (usuario)\n");
                break;
            }

            // Se recibe el alias
            if (readLine(sock_client_fd, (char*) &alias, sizeof(alias)) == -1){
                perror("[SERVIDOR][ERROR] No se pudo recibir la petición del cliente (alias)\n");
                break;
            }
        
            // Se recibe la fecha de nacimiento
            if (readLine(sock_client_fd, (char*) &fecha, sizeof(fecha)) == -1){
                perror("[SERVIDOR][ERROR] No se pudo recibir la petición del cliente (fecha de nacimiento)\n");
                break;
            }
        }

        if (strcmp(op, "CONNECT") == 0) {

            // Se recibe el puerto de escucha del cliente
            if (readLine(sock_client_fd, (char*) &port_escucha, sizeof(port_escucha)) == -1){
                perror("[SERVIDOR][ERROR] No se pudo recibir la petición del cliente (puerto de escucha)\n");
                break;
            }

        }

        if (strcmp(op, "SEND") == 0) {

            // Se recibe el destinatario
            if (readLine(sock_client_fd, (char*) &destinatario, sizeof(destinatario)) == -1){
                perror("[SERVIDOR][ERROR] No se pudo recibir la petición del cliente (destinatario)\n");
                break;
            }

            // Se recibe el mensaje
            if (readLine(sock_client_fd, (char*) &mensaje, sizeof(mensaje)) == -1){
                perror("[SERVIDOR][ERROR] No se pudo recibir la petición del cliente (mensaje)\n");
                break;
            }

        }

        // Almacenamos lo leído en la estructura
        strcpy(peticion.op, op);
        peticion.sock_client = sock_client_fd;
        strcpy(peticion.content.IP, client_ip);
        strcpy(peticion.content.alias, alias);

        if (strcmp(op, "REGISTER") == 0){
            strcpy(peticion.content.usuario, usuario);
            strcpy(peticion.content.fecha, fecha);
        }
        if (strcmp(op, "CONNECT") == 0) {
            strcpy(peticion.content.port_escucha, port_escucha);
        }
        if (strcmp(op, "SEND") == 0) {
            strcpy(peticion.content.destinatario, destinatario);
            strcpy(peticion.content.mensaje, mensaje);
        }
        

        // Crea un hilo por peticion
        if(pthread_create(&thid, &th_attr, (void*) &tratar_pet, (void *) &peticion) == -1){
            perror("[SERVIDOR][ERROR] Hilo no pudo ser creado\n");
            break;
        }

        // Asegura que la peticion se copia correctamente en el hilo que atiende al cliente
        pthread_mutex_lock(&mutex_msg);
        while (mensaje_no_copiado) pthread_cond_wait(&condvar_msg, &mutex_msg);
            mensaje_no_copiado = 1;
            pthread_mutex_unlock(&mutex_msg);
        }

        // Se cierra el socket del servidor
        if (close(sock_serv_fd) == -1){
            perror("[SERVIDOR][ERROR] No se pudo cerrar el socket de recepción de peticiones\n");
            return -1;
        }
        return 0;
}
