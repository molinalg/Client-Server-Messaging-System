#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <limits.h>

const char* filename_register_prefix= "data_";     // Prefijo en ruta para fichero de datos de registo de usuario N
const char* filename_msg_prefix = "msg_";          // Prefijo en ruta para fichero de mensajes pendientes para usuario N
const char* ext = ".dat";                           // Extension del fichero de usuario (data y mensajes pendientes)
const char* dirname =  "users";                   // Nombre del directorio principal de registros
const char* register_prefix = "register_";          // Prefijo para directorio de registro N


// Estructura para información
typedef struct info{
    char usuario[64];
    char destinatario[64];
    char alias[32];
    char fecha[11];
    char port_escucha[6];
    char mensaje[256];
} Info;

// Estructura para almacenar datos del usuario
typedef struct user{
    char usuario[64];
    char alias[32];
    char fecha[11];
    int estado;
    char IP[INET_ADDRSTRLEN];
    char port_escucha[6];
    char pend_mensajes[256];
    unsigned int id_msg;
} User;

// Estructura para información recibida
typedef struct response{
    char* status;
    unsigned int id;
    char port_escucha[6];
    char IP[INET_ADDRSTRLEN];
    char pend_mensajes[256];
    int num_users;
    char users[2048];
} Response;

// Registra a un usuario
char* register_serv(char *username, char *alias, char *date){
    
    DIR* user_dir;

    // Creamos el nombre del directorio
    char user_dirname [1024];
    sprintf(user_dirname, "./%s/%s%s", dirname, register_prefix, alias);

    // Comprobamos si existe
    if ((user_dir = opendir(user_dirname)) != NULL){
        perror("[SERVIDOR][ERROR] El directorio del usuario ya existe\n");
        closedir(user_dir);
        return "1";
    }
    
    // Creamos el directorio
    if (mkdir(user_dirname, 0755) == -1) {
        perror("[SERVIDOR][ERROR] El directorio no pudo ser creado\n");
        return "2";
    }

    // Creamos el nombre del fichero de datos personales de usuario
    char user_file [2048];
    sprintf(user_file, "%s/%s%s%s", user_dirname, filename_register_prefix, alias, ext);

    // Obtiene nombre completo del fichero de mensajes
    char messages_file [2048];
    sprintf(messages_file, "%s/%s%s%s", user_dirname, filename_msg_prefix, alias, ext);

    // Creamos el fichero de registro
    FILE* user_fp;

    if ((user_fp = fopen(user_file, "w")) == NULL){
        perror("[SERVIDOR][ERROR] El fichero para el registro del usuario no pudo ser creado\n");
        return "2";
    }

    // Creamos el fichero de  mensajes
    FILE* messages_fp;
    
    if ((messages_fp = fopen(messages_file, "w")) == NULL){
        perror("[SERVIDOR][ERROR] El fichero del usuario para mensajes no pudo ser abierto\n");
        return "2";
    }

    fclose(messages_fp);

    // Escribimos lo necesario en el fichero
    User user;
    strcpy(user.usuario, username);
    strcpy(user.alias, alias);
    strcpy(user.fecha, date);
    user.estado = 0;
    strcpy(user.pend_mensajes, messages_file);
    user.id_msg = 0;

    if (fwrite(&user, sizeof(User), 1, user_fp) == 0){
        perror("[SERVIDOR][ERROR] No se pudo escribir en el fichero\n");
        fclose(user_fp);
        return "2";
    }

    fclose(user_fp);
    return "0";
}


// Da de baja a un usuario
char* unregister_serv(char *alias){

    DIR* user_dir;
    struct dirent *user_entry;
    char entry_path[512];

    // Creamos el nombre del directorio
    char user_dirname [1024];
    sprintf(user_dirname, "./%s/%s%s", dirname, register_prefix, alias);

    // Comprobamos si existe
    if ((user_dir = opendir(user_dirname)) == NULL){
        perror("[SERVIDOR][ERROR] El directorio del usuario no existe\n");
        return "1";
    }

    // Elimina los ficheros con extesion .dat
    while ((user_entry = readdir(user_dir)) != NULL) {
        if (strstr(user_entry->d_name, ".dat") != NULL){
            snprintf(entry_path, 512, "%s/%s%s/%s", dirname, register_prefix, alias, user_entry->d_name);
            remove(entry_path);
        }
    }

    closedir(user_dir);

    // Elimina el directorio de la persona
    if (rmdir(user_dirname) == -1) {
        perror("[SERVIDOR][ERROR] El directorio no pudo ser eliminado\n");
        return "2";
    }

    return "0";
}


// Conecta a un usuario
char* connect_serv(char *alias, char *IP, char *port_escucha){

    DIR* user_dir;

    // Creamos el nombre del directorio
    char user_dirname [1024];
    sprintf(user_dirname, "./%s/%s%s", dirname, register_prefix, alias);

    // Comprobamos si existe
    if ((user_dir = opendir(user_dirname)) == NULL){
        perror("[SERVIDOR][ERROR] El directorio del usuario no existe\n");
        return "1";
    }

    closedir(user_dir);

    // Creamos el nombre del fichero de datos personales de usuario
    char user_file [2048];
    sprintf(user_file, "%s/%s%s%s", user_dirname, filename_register_prefix, alias, ext);

    // Comprobamos si existe
    if (access(user_file, F_OK)){
        perror("[SERVIDOR][ERROR] Registro de usuario no existe\n");
        return "1";
    }

    // Abrimos el fichero registro
    FILE* user_fp;

    if ((user_fp = fopen(user_file, "r+")) == NULL){
        perror("[SERVIDOR][ERROR] El fichero del usuario para registro no pudo ser abierto\n");
        return "3";
    }

    // Leemos el fichero
    User user;

    if (fread(&user, sizeof(User), 1, user_fp) == 0){
        perror("[SERVIDOR][ERROR] No se pudo leer el fichero\n");
        fclose(user_fp);
        return "3";
    }

    // Comprobamos si ya esta conectado
    if (user.estado == 1) {
        perror("[SERVIDOR][ERROR] Usuario ya conectado\n");
        fclose(user_fp);
        return "2";
    }

    // Actualizamos el fichero
    strcpy(user.IP, IP);
    strcpy(user.port_escucha, port_escucha);

    user.estado = 1;

    // Hace falta volver al inicio del fichero para sobreescribir
    rewind(user_fp);

    if (fwrite(&user, sizeof(User), 1, user_fp) == 0){
        perror("[SERVIDOR][ERROR] No se pudo escribir en el fichero\n");
        fclose(user_fp);
        return "3";
    }

    fclose(user_fp);

    return "0";
}

// Desconecta a un usuario
char* disconnect_serv(char *alias){

    DIR* user_dir;

    // Creamos el nombre del directorio
    char user_dirname [1024];
    sprintf(user_dirname, "./%s/%s%s", dirname, register_prefix, alias);

    // Comprobamos si existe
    if ((user_dir = opendir(user_dirname)) == NULL){
        perror("[SERVIDOR][ERROR] El directorio del usuario no existe\n");
        return "1";
    }

    closedir(user_dir);

    // Creamos el nombre del fichero de datos personales de usuario
    char user_file [2048];
    sprintf(user_file, "%s/%s%s%s", user_dirname, filename_register_prefix, alias, ext);

    // Comprobamos si existe
    if (access(user_file, F_OK)){
        perror("[SERVIDOR][ERROR] Registro de usuario no existe\n");
        return "1";
    }

    // Abrimos el fichero registro
    FILE* user_fp;

    if ((user_fp = fopen(user_file, "r+")) == NULL){
        perror("[SERVIDOR][ERROR] El fichero del usuario para registro no pudo ser abierto\n");
        return "3";
    }

    // Leemos el fichero
    User user;

    if (fread(&user, sizeof(User), 1, user_fp) == 0){
        perror("[SERVIDOR][ERROR] No se pudo leer el fichero\n");
        fclose(user_fp);
        return "3";
    }

    // Comprobamos si ya esta desconectado
    if (user.estado == 0) {
        perror("[SERVIDOR][ERROR] Usuario no está conectado\n");
        fclose(user_fp);
        return "2";
    }

    // Actualizamos el fichero
    user.estado = 0;

    // Hace falta volver al inicio del fichero para sobreescribir
    rewind(user_fp);

    if (fwrite(&user, sizeof(User), 1, user_fp) == 0){
        perror("[SERVIDOR][ERROR] No se pudo escribir en el fichero\n");
        fclose(user_fp);
        return "3";
    }

    fclose(user_fp);

    return "0";

}

// Procesa pedición de envío de mensaje a un usuario
Response send_serv(char *alias, char *destino, char *mensaje){

    Response service;

    DIR* user_dir;

    // Creamos el nombre del directorio del emisor
    char user_dirname [1024];
    sprintf(user_dirname, "./%s/%s%s", dirname, register_prefix, alias);

    // Comprobamos si existe el emisor
    if ((user_dir = opendir(user_dirname)) == NULL){
        perror("[SERVIDOR][ERROR] El directorio del emisor no existe\n");
        service.status = "1";
        return service;
    }

    closedir(user_dir);

    // Creamos el nombre del fichero de datos personales del emisor
    char user_file [2048];
    sprintf(user_file, "%s/%s%s%s", user_dirname, filename_register_prefix, alias, ext);

    // Comprobamos si existe
    if (access(user_file, F_OK)){
        perror("[SERVIDOR][ERROR] Registro de usuario no existe\n");
        service.status = "1";
        return service;
    }

    // Abrimos el fichero registro del emisor
    FILE* user_fp;

    if ((user_fp = fopen(user_file, "r+")) == NULL){
        perror("[SERVIDOR][ERROR] El fichero del emisor para registro no pudo ser abierto\n");
        service.status = "2";
        return service;
    }

    // Leemos el fichero emisor
    User user;

    if (fread(&user, sizeof(User), 1, user_fp) == 0){
        perror("[SERVIDOR][ERROR] No se pudo leer el fichero del emisor\n");
        fclose(user_fp);
        service.status = "2";
        return service;
    }

    // Comprobamos si está conectado
    if (user.estado == 0) {
        perror("[SERVIDOR][ERROR] Emisor no está conectado\n");
        fclose(user_fp);
        service.status = "2";
        return service;
    }

    DIR* dest_dir;

    // Creamos el nombre del directorio
    char dest_dirname [1024];
    sprintf(dest_dirname, "./%s/%s%s", dirname, register_prefix, destino);

    // Comprobamos si existe
    if ((dest_dir = opendir(dest_dirname)) == NULL){
        perror("[SERVIDOR][ERROR] El directorio del destinatario no existe\n");
        service.status = "1";
        return service;
    }

    closedir(dest_dir);

    // Creamos el nombre del fichero de datos personales del destinatario
    char dest_file [2048];
    sprintf(dest_file, "%s/%s%s%s", dest_dirname, filename_register_prefix, destino, ext);

    // Comprobamos si existe
    if (access(dest_file, F_OK)){
        perror("[SERVIDOR][ERROR] Registro de destinatario no existe\n");
        service.status = "1";
        return service;
    }

    // Abrimos el fichero registro del destinatario
    FILE* dest_fp;

    if ((dest_fp = fopen(dest_file, "r+")) == NULL){
        perror("[SERVIDOR][ERROR] El fichero del destinatario para registro no pudo ser abierto\n");
        service.status = "2";
        return service;
    }

    // Leemos el fichero emisor
    User dest;

    if (fread(&dest, sizeof(User), 1, dest_fp) == 0){
        perror("[SERVIDOR][ERROR] No se pudo leer el fichero del destinatario\n");
        fclose(dest_fp);
        service.status = "2";
        return service;
    }

    // Actualizamos el id del emisor
    user.id_msg %= UINT_MAX;
    user.id_msg++;

    // Hace falta volver al inicio del fichero para sobreescribir
    rewind(user_fp);

    // Actualizamos el fichero emisor
    if (fwrite(&user, sizeof(User), 1, user_fp) == 0){
        perror("[SERVIDOR][ERROR] No se pudo escribir en el fichero\n");
        fclose(user_fp);
        fclose(dest_fp);
        service.status = "2";
        return service;
    }

    fclose(user_fp);
    fclose(dest_fp);

    service.id = user.id_msg;
    strcpy(service.port_escucha, dest.port_escucha);
    strcpy(service.IP, dest.IP);

    // Se devuelve 0 si están los 2 conectados y 3 si el receptor no
    if (dest.estado == 1) {
        service.status = "0";
    } else {
        service.status = "3";
        strcpy(service.pend_mensajes, dest.pend_mensajes);
    }

    return service;

}

// Devuelve usuarios conectados
Response connected_users_serv(char *alias){

    char conectados[1024] = "";
    int num_conectados = 0;

    Response service;

    DIR* user_dir;
    struct dirent* user_dirent;

    // Creamos el nombre del directorio del emisor
    char user_dirname [1024];
    sprintf(user_dirname, "./%s/", dirname);

    // Comprobamos si existe el emisor
    if ((user_dir = opendir(user_dirname)) == NULL){
        perror("[SERVIDOR][ERROR] El directorio del emisor no existe\n");
        service.status = "2";
        return service;
    }

    // Creamos el nombre del fichero de datos personales del emisor
    char user_file [2048];
    sprintf(user_file, "%s/%s%s/%s%s%s", user_dirname, register_prefix, alias, filename_register_prefix, alias, ext);

    // Comprobamos si existe
    if (access(user_file, F_OK)){
        perror("[SERVIDOR][ERROR] Registro de usuario no existe\n");
        service.status = "2";
        return service;
    }

    // Abrimos el fichero registro del emisor
    FILE* user_fp;

    if ((user_fp = fopen(user_file, "r+")) == NULL){
        perror("[SERVIDOR][ERROR] El fichero del usuario para registro no pudo ser abierto\n");
        service.status = "2";
        return service;
    }

    // Leemos el fichero emisor
    User user;

    if (fread(&user, sizeof(User), 1, user_fp) == 0){
        perror("[SERVIDOR][ERROR] No se pudo leer el fichero del usuario\n");
        fclose(user_fp);
        service.status = "2";
        return service;
    }

    // Comprobamos si está conectado
    if (user.estado == 0) {
        perror("[SERVIDOR][ERROR] Usuario no está conectado\n");
        fclose(user_fp);
        service.status = "1";
        return service;
    }

    fclose(user_fp);

    // Leemos uno por uno los directorios de registro exitentes
    // En cada uno se abre el fichero de registro y se lee
    int primero = 0;
    while ((user_dirent = readdir(user_dir)) != NULL) { // Leer cada directorio
        if (user_dirent->d_type == DT_DIR) {
            if (strcmp(user_dirent->d_name, ".") != 0 && strcmp(user_dirent->d_name, "..") != 0) {
                
                char subdir_path[2048];
                sprintf(subdir_path, "%s%s/", user_dirname, user_dirent->d_name); // Ruta al directorio de registro de cada uno
                DIR *subdir = opendir(subdir_path);

                if (subdir != NULL) {
                    struct dirent *subent;
                    while ((subent = readdir(subdir)) != NULL) { // Leer cada directorio de registro
                        
                        if (strncmp(subent->d_name, "data_", 5) == 0) { // Se comprueba si el fichero empieza por data_
                            char filepath[4096];
                            sprintf(filepath, "%s%s", subdir_path, subent->d_name);
                            
                            // Se abre y se lee el archivo
                            FILE* actual_fp;
                            if ((actual_fp = fopen(filepath, "r+")) == NULL){
                                perror("[SERVIDOR][ERROR] El fichero actual no pudo ser abierto\n");
                                service.status = "2";
                                return service;
                            }

                            User actual;
                            if (fread(&actual, sizeof(User), 1, actual_fp) == 0){
                                perror("[SERVIDOR][ERROR] No se pudo leer el fichero actual\n");
                                fclose(actual_fp);
                                service.status = "2";
                                return service;
                            }
                            
                            // Si está conectado se añade a la lista (cada usuario se separa con una coma)
                            if (actual.estado == 1){
                                if (primero == 0) {
                                    strcat(conectados, actual.alias);
                                    primero = 1;
                                } else {
                                    strcat(conectados, ",");
                                    strcat(conectados, actual.alias);
                                }
                                num_conectados++;
                            }
                            fclose(actual_fp);
                        }
                    }
                    closedir(subdir);
                }
            }
        }
    }

    closedir(user_dir);

    // Se devuelve 0 si todo ha salido bien
    service.status = "0";
    strcpy(service.users, conectados);
    service.num_users = num_conectados;

    return service;
}