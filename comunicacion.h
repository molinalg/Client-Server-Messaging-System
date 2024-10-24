#ifndef COMUNICACION_H
#define COMUNICACION_H

#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

// Estructura para información mandada
typedef struct info{
    char usuario[64];
    char destinatario[32];
    char alias[32];
    char fecha[11];
	int estado;
	char IP[INET_ADDRSTRLEN];
    char port_escucha[6];
    char mensaje[256];
	unsigned int id_msg;
} Info;

// Estructura para información de usuario
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

// Estructura para peticiones de cliente
typedef struct request{
    char op[15];
    int sock_client;
    Info content;
} Request;

// Estructura para respuesta del servidor a cliente
typedef struct service{
    char* status;
    Response content;
} Service;

// Estructura para el envío de mensajes
typedef struct message{
	char id_emisor[32];
	char alias_emisor[32];
	char mensaje[256];
} Message;

// Recepcion de mensajes por socket [TOMADO DE EJEMPLO DE CLASE]
int recvMessage(int socket, char *buffer, int len){
	int r, l = len;	
	do {	
		r = read(socket, buffer, l);
		l -= r ;
		buffer += r;
	} while ((l>0) && (r>=0));
	if (r < 0)
		return (-1);   /* fallo */
	return(0);	/* full length has been receive */
}

// Recepcion de mensajes por socket [TOMADO DE EJEMPLO DE CLASE]
int sendMessage(int socket, char * buffer, int len){
	int r, l = len;
	do {
		r = write(socket, buffer, l);
		l -= r;
		buffer += r;
	} while ((l>0) && (r>=0));
	if (r < 0){
		return (-1);
	}   /* fail */
	return(0);	/* full length has been sent */
}

// Recepcion de mensajes por socket [TOMADO DE EJEMPLO DE CLASE]
ssize_t readLine(int fd, void *buffer, size_t n){
	ssize_t numRead; /* num of bytes fetched by last read() */
	size_t totRead; /* total bytes read so far */
	char *buf;
	char ch;
	if (n <= 0 || buffer == NULL) {
		errno = EINVAL;
		return -1;
	}
	buf = buffer;
	totRead = 0;
	for (;;) {
		numRead = read(fd, &ch, 1); /* read a byte */
		if (numRead == -1) {
			if (errno == EINTR) /* interrupted -> restart read() */
				continue;
			else
				return -1; /* some other error */
		} else if (numRead == 0) { /* EOF */
			if (totRead == 0) /* no byres read; return 0 */
				return 0;
			else
				break;
		} else { /* numRead must be 1 if we get here*/
			if (ch == '\n')
				break;
			if (ch == '\0')
				break;
			if (totRead < n - 1) { /* discard > (n-1) bytes */
				totRead++;
				*buf++ = ch;
			}
		}
	}
	*buf = '\0';
	return totRead;
}

#endif