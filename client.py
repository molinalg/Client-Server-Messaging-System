import subprocess
import sys
import PySimpleGUI as sg
from enum import Enum
import argparse
import time

import socket
import threading
import binascii
import ipaddress
import zeep

class client :

    # ******************** TYPES *********************
    # *
    # * @brief Return codes for the protocol methods
    class RC(Enum) :
        OK = 0
        ERROR = 1
        USER_ERROR = 2

    # ****************** ATTRIBUTES ******************
    _server = None
    _port = -1
    _quit = 0
    _username = None
    _alias = None
    _date = None
    _hilo = None
    _socket = None
    # Se comprueba si el servidor web está conectado o no
    try:
        _soap = zeep.Client(wsdl="http://localhost:8000/?wsdl")
    except:
        print("Error al conectar con el servidor web, los mensajes no se formatearán")
    # ******************** METHODS *******************
    @staticmethod
    # Función para recibir mensajes del servidor (sacada de aula global)
    def leerValores(socket):
        mensaje = ""
        while True:
            rec = socket.recv(1)
            if rec == b'\0':
                break
            mensaje = mensaje + rec.decode("utf-8")
        return mensaje

    @staticmethod
    # Función para recibir los mensajes y acks del servidor 
    def hiloEscucha(window):
        # Creamos el socket para escuchar al servidor usando la ip del ordenador
        client._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, 0)
        client._socket.bind(server_address)
        client._socket.listen(1)

        # Aceptamos conexiones en un bucle while
        while True:
            # Esperamos a que se conecte un cliente
            socket_cliente, direccion = client._socket.accept()
            try:
                #Recibimos un mensaje con la operacion
                operacion = client.leerValores(socket_cliente)

                if operacion == "RECEIVE_MESS":
                    id = client.leerValores(socket_cliente)
                    remitente = client.leerValores(socket_cliente)
                    mensaje = client.leerValores(socket_cliente)
                    id = int(id,10)
                    window['_SERVER_'].print("s> MESSAGE {id} FROM {remitente}\n{mensaje}\nEND".format(id=id,remitente=remitente,mensaje=mensaje))
                elif operacion == "SEND_MESS_ACK":
                    id = client.leerValores(socket_cliente)
                    window['_SERVER_'].print("s> SEND MESSAGE {} OK".format(id))
                else:
                    print("La operación es: " + operacion)
                    window['_SERVER_'].print("s> ERROR RECEIVING MESSAGE")
            
            finally:
                socket_cliente.close()
                
    
    # *
    # * @param user - User name to register in the system
    # *
    # * @return OK if successful
    # * @return USER_ERROR if the user is already registered
    # * @return ERROR if another error occurred
    @staticmethod
    def register(user, window):

        #  1. Conectarse al servidor
        sock_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, client._port)
        try:
            sock_client.connect(server_address)
        except:
            window['_SERVER_'].print("s> COULDN'T CONNECT TO SERVER (MIGHT BE OFFLINE)")
            return client.RC.ERROR

        #  2. Enviar la cadena REGISTER
        try:
            mensaje = b'REGISTER\0'
            sock_client.sendall(mensaje)

        #  3. Enviar cadena con nombre de usuario
            usuario = client._username + "\0"
            mensaje = usuario.encode("utf-8")
            sock_client.sendall(mensaje)

        #  4. Enviar cadena con alias
            alias = user + "\0"
            mensaje = alias.encode("utf-8")
            sock_client.sendall(mensaje)

        #  5. Enviar cadena con fecha de nacimiento formato dd-mm-yyyy
            fecha = client._date + "\0"
            mensaje = fecha.encode("utf-8")
            sock_client.sendall(mensaje)

        #  6. Recibir respuesta del servidor (0 si bien, 1 si ya registrado, 2 si error)
            resultado = client.leerValores(sock_client)

        #  7. Cerrar conexion
        finally:
            sock_client.close()

        #  8. Evaluar resultado
        if resultado == "0":
            window['_SERVER_'].print("s> REGISTER OK")
            return client.RC.OK
        elif resultado == "1":
            window['_SERVER_'].print("s> USERNAME IN USE")
            return client.RC.USER_ERROR
        window['_SERVER_'].print("s> REGISTER FAIL")
        return client.RC.ERROR

    # *
    # 	 * @param user - User name to unregister from the system
    # 	 *
    # 	 * @return OK if successful
    # 	 * @return USER_ERROR if the user does not exist
    # 	 * @return ERROR if another error occurred
    @staticmethod
    def  unregister(user, window):
        
        #  1. Conectarse al servidor
        sock_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, client._port)
        try:
            sock_client.connect(server_address)
        except:
            window['_SERVER_'].print("s> COULDN'T CONNECT TO SERVER (MIGHT BE OFFLINE)")
            return client.RC.ERROR

        #  2. Enviar la cadena UNREGISTER
        try:
            mensaje = b'UNREGISTER\0'
            sock_client.sendall(mensaje)

        #  3. Enviar cadena con nombre de usuario (alias)
            usuario = user + "\0"
            mensaje = usuario.encode("utf-8")
            sock_client.sendall(mensaje)

        #  4. Recibir respuesta del servidor (0 si bien, 1 si no existe, 2 si error)
            resultado = client.leerValores(sock_client)

        #  5. Cerrar conexion
        finally:
            sock_client.close()

        #  6. Evaluar resultado
        if resultado == "0":
            window['_SERVER_'].print("s> UNREGISTER OK")
            client._username = None
            client._alias = None
            client._date = None
            return client.RC.OK
        elif resultado == "1":
            window['_SERVER_'].print("s> USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        
        window['_SERVER_'].print("s> UNREGISTER FAIL")
        return client.RC.ERROR


    # *
    # * @param user - User name to connect to the system
    # *
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist or if it is already connected
    # * @return ERROR if another error occurred
    @staticmethod
    def  connect(user, window):
        
        #  1. Conectarse al servidor
        sock_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, client._port)
        try:
            sock_client.connect(server_address)
        except:
            window['_SERVER_'].print("s> COULDN'T CONNECT TO SERVER (MIGHT BE OFFLINE)")
            return client.RC.ERROR

        #  2. Creamos hilo de escucha
        client._hilo = threading.Thread(target=client.hiloEscucha, args=(window,))
        client._hilo.start()

        #  3. Enviar la cadena CONNECT
        try:
            mensaje = b'CONNECT\0'
            sock_client.sendall(mensaje)
        
        #  4. Enviar cadena con nombre de usuario (alias)
            usuario = user + "\0"
            mensaje = usuario.encode("utf-8")
            sock_client.sendall(mensaje)
        
        #  5. Se envía una cadena con el número de puerto de escucha
            # Nos aseguramos de que se ejecute lo siguiente solo si el hilo ya ha podido crear su socket
            while client._socket == None:
                time.sleep(0.1)

            _, port = client._socket.getsockname()
            puerto = str(port) + "\0"
            mensaje = puerto.encode("utf-8")
            sock_client.sendall(mensaje)

        #  6. Recibir respuesta del servidor (0 si bien, 1 si no existe, 2 si ya está conectado, 3 si error)
            resultado = client.leerValores(sock_client)

        #  7. Cerrar conexion
        finally:
            sock_client.close()

        #  8. Evaluar resultado
        if resultado == "0":
            window['_SERVER_'].print("s> CONNECT OK")
            return client.RC.OK
        elif resultado == "1":
            try:
                if (client._socket != None):
                    client._socket.close()
                    client._socket = None
                client._hilo.terminate()
            except:
                pass
            window['_SERVER_'].print("s> CONNECT FAIL, USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        elif resultado == "2":
            try:
                if (client._socket != None):
                    client._socket.close()
                    client._socket = None
                client._hilo.terminate()
            except:
                pass
            window['_SERVER_'].print("s> USER ALREADY CONNECTED")
            return client.RC.USER_ERROR

        window['_SERVER_'].print("s> CONNECT FAIL")
        return client.RC.ERROR

    # *
    # * @param user - User name to disconnect from the system
    # *
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist
    # * @return ERROR if another error occurred
    @staticmethod
    def  disconnect(user, window):
        
        #  1. Conectarse al servidor
        sock_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, client._port)
        try:
            sock_client.connect(server_address)
        except:
            window['_SERVER_'].print("s> COULDN'T CONNECT TO SERVER (MIGHT BE OFFLINE)")
            return client.RC.ERROR

        #  2. Enviar la cadena DISCONNECT
        try:
            mensaje = b'DISCONNECT\0'
            sock_client.sendall(mensaje)

        #  3. Enviar cadena con nombre de usuario (alias)
            usuario = user + "\0"
            mensaje = usuario.encode("utf-8")
            sock_client.sendall(mensaje)

        #  4. Recibir respuesta del servidor (0 si bien, 1 si no existe, 2 si no conectado, 3 si error)
            resultado = client.leerValores(sock_client)

        #  5. Cerrar conexion
        finally:
            sock_client.close()

        #  6. Evaluar resultado
        if resultado == "0":
            try:
                if (client._socket != None):
                    client._socket.close()
                    client._socket = None
                client._hilo.terminate()
            except:
                pass
            window['_SERVER_'].print("s> DISCONNECT OK")
            return client.RC.OK
        elif resultado == "1":
            window['_SERVER_'].print("s> DISCONNECT FAIL / USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        elif resultado == "2":
            window['_SERVER_'].print("s> DISCONNECT FAIL / USER NOT CONNECTED")
            return client.RC.USER_ERROR
        
        window['_SERVER_'].print("s> DISCONNECT FAIL")
        return client.RC.ERROR

    # *
    # * @param user    - Receiver user name
    # * @param message - Message to be sent
    # *
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def  send(user, message, window):
    
        #  0. Se comprueba que el servidor web sigue activo y si es así se formatea el mensaje
        try:
            client._soap = zeep.Client(wsdl="http://localhost:8000/?wsdl")
            msg = client._soap.service.eliminar_espacios(message)
        except:
            msg = message
        
        if len(msg) > 255:
            window['_SERVER_'].print("s> SEND FAIL")
            return client.RC.ERROR

        print("SEND " + user + " " + msg)

        #  1. Conectarse al servidor
        sock_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, client._port)
        try:
            sock_client.connect(server_address)
        except:
            window['_SERVER_'].print("s> COULDN'T CONNECT TO SERVER (MIGHT BE OFFLINE)")
            return client.RC.ERROR

        #  2. Enviar la cadena SEND
        try:
            mensaje = b'SEND\0'
            sock_client.sendall(mensaje)

        #  3. Enviar cadena con nombre de usuario (alias)
            usuario = client._alias + "\0"
            mensaje = usuario.encode("utf-8")
            sock_client.sendall(mensaje)

        #  4. Enviar cadena con nombre de usuario (alias) del destinatario
            usuario = user + "\0"
            mensaje = usuario.encode("utf-8")
            sock_client.sendall(mensaje)

        #  5. Enviar cadena con el mensaje
            mensaje = msg + "\0"
            mensaje = mensaje.encode("utf-8")
            sock_client.sendall(mensaje)

        #  6. Recibir respuesta del servidor (0 si bien, 1 si no existe, 2 si error) y el id
            resultado = client.leerValores(sock_client)
            if resultado == "0":
                id_emisor = client.leerValores(sock_client)

        #  7. Cerrar conexion
        finally:
            sock_client.close()
        
        #  8. Evaluar resultado
        if resultado == "0":
            window['_SERVER_'].print("s> SEND OK - MESSAGE {id}".format(id=id_emisor))
            return client.RC.OK
        elif resultado == "1":
            window['_SERVER_'].print("s> SEND FAIL / USER DOES NOT EXIST")
            return client.RC.USER_ERROR

        window['_SERVER_'].print("s> SEND FAIL")
        return client.RC.ERROR

    # *
    # * @param user    - Receiver user name
    # * @param message - Message to be sent
    # * @param file    - file  to be sent

    # *
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def  sendAttach(user, message, file, window):
        window['_SERVER_'].print("s> SENDATTACH MESSAGE OK")
        print("SEND ATTACH " + user + " " + message + " " + file)
        #  Write your code here
        return client.RC.ERROR

    @staticmethod
    def connectedUsers(window):
        
        #  1. Conectarse al servidor
        sock_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, client._port)
        try:
            sock_client.connect(server_address)
        except:
            window['_SERVER_'].print("s> COULDN'T CONNECT TO SERVER (MIGHT BE OFFLINE)")
            return client.RC.ERROR

        #  2. Enviar la cadena CONNECTEDUSERS
        try:
            mensaje = b'CONNECTEDUSERS\0'
            sock_client.sendall(mensaje)
        
        #  3. Enviar cadena con nombre de usuario (alias)
            usuario = client._alias + "\0"
            mensaje = usuario.encode("utf-8")
            sock_client.sendall(mensaje)

        #  4. Recibir respuesta del servidor (0 si bien, 1 si usuario no conectado, 2 si error)
            resultado = client.leerValores(sock_client)
        
        #  5. Recibir cadena con el número de usuarios conectados si resultado es 0
            if resultado == "0":
                num_usuarios = client.leerValores(sock_client)
                print("Numero de usuarios: {}".format(num_usuarios))
                num_usuarios = int(num_usuarios, 10)
        
        #  6. Recibir cadenas con usuarios conectados
                conectados = ""
                for i in range(num_usuarios):
                    usuario = client.leerValores(sock_client)
                    if i == 0:
                        conectados = usuario
                    else:
                        conectados = conectados + ", " + usuario
        
        #  6. Cerrar conexion
        finally:
            sock_client.close()
        
        #  7. Evaluar resultado
        if resultado == "0":
            window['_SERVER_'].print("s> CONNECTED USERS ({num_users} users connected) OK - {conectados}".format(num_users=num_usuarios, conectados=conectados))
            return client.RC.OK
        elif resultado == "1":
            window['_SERVER_'].print("s> CONNECTED USERS FAIL / USER IS NOT CONNECTED")
            return client.RC.USER_ERROR
        
        window['_SERVER_'].print("s> CONNECTED USERS FAIL")
        return client.RC.ERROR

    @staticmethod
    def window_register():
        layout_register = [[sg.Text('Ful Name:'),sg.Input('Text',key='_REGISTERNAME_', do_not_clear=True, expand_x=True)],
                            [sg.Text('Alias:'),sg.Input('Text',key='_REGISTERALIAS_', do_not_clear=True, expand_x=True)],
                            [sg.Text('Date of birth:'),sg.Input('',key='_REGISTERDATE_', do_not_clear=True, expand_x=True, disabled=True, use_readonly_for_disable=False),
                            sg.CalendarButton("Select Date",close_when_date_chosen=True, target="_REGISTERDATE_", format='%d-%m-%Y',size=(10,1))],
                            [sg.Button('SUBMIT', button_color=('white', 'blue'))]
                            ]

        layout = [[sg.Column(layout_register, element_justification='center', expand_x=True, expand_y=True)]]

        window = sg.Window("REGISTER USER", layout, modal=True)
        choice = None

        while True:
            event, values = window.read()

            if (event in (sg.WINDOW_CLOSED, "-ESCAPE-")):
                break

            if event == "SUBMIT":
                if(values['_REGISTERNAME_'] == 'Text' or values['_REGISTERNAME_'] == '' or values['_REGISTERALIAS_'] == 'Text' or values['_REGISTERALIAS_'] == '' or values['_REGISTERDATE_'] == ''):
                    sg.Popup('Registration error', title='Please fill in the fields to register.', button_type=5, auto_close=True, auto_close_duration=1)
                    continue

                client._username = values['_REGISTERNAME_']
                client._alias = values['_REGISTERALIAS_']
                client._date = values['_REGISTERDATE_']
                break
        window.Close()


    # *
    # * @brief Prints program usage
    @staticmethod
    def usage() :
        print("Usage: python3 py -s <server> -p <port>")


    # *
    # * @brief Parses program execution arguments
    @staticmethod
    def  parseArguments(argv) :
        parser = argparse.ArgumentParser()
        parser.add_argument('-s', type=str, required=True, help='Server IP')
        parser.add_argument('-p', type=int, required=True, help='Server Port')
        args = parser.parse_args()

        if (args.s is None):
            parser.error("Usage: python3 py -s <server> -p <port>")
            return False

        if ((args.p < 1024) or (args.p > 65535)):
            parser.error("Error: Port must be in the range 1024 <= port <= 65535");
            return False;

        client._server = args.s
        client._port = args.p

        return True


    def main(argv):

        if (not client.parseArguments(argv)):
            client.usage()
            exit()

        lay_col = [[sg.Button('REGISTER',expand_x=True, expand_y=True),
                sg.Button('UNREGISTER',expand_x=True, expand_y=True),
                sg.Button('CONNECT',expand_x=True, expand_y=True),
                sg.Button('DISCONNECT',expand_x=True, expand_y=True),
                sg.Button('CONNECTED USERS',expand_x=True, expand_y=True)],
                [sg.Text('Dest:'),sg.Input('User',key='_INDEST_', do_not_clear=True, expand_x=True),
                sg.Text('Message:'),sg.Input('Text',key='_IN_', do_not_clear=True, expand_x=True),
                sg.Button('SEND',expand_x=True, expand_y=False)],
                [sg.Text('Attached File:'), sg.In(key='_FILE_', do_not_clear=True, expand_x=True), sg.FileBrowse(),
                sg.Button('SENDATTACH',expand_x=True, expand_y=False)],
                [sg.Multiline(key='_CLIENT_', disabled=True, autoscroll=True, size=(60,15), expand_x=True, expand_y=True),
                sg.Multiline(key='_SERVER_', disabled=True, autoscroll=True, size=(60,15), expand_x=True, expand_y=True)],
                [sg.Button('QUIT', button_color=('white', 'red'))]
            ]


        layout = [[sg.Column(lay_col, element_justification='center', expand_x=True, expand_y=True)]]

        window = sg.Window('Messenger', layout, resizable=True, finalize=True, size=(1000,400))
        window.bind("<Escape>", "-ESCAPE-")


        while True:
            event, values = window.Read()

            if (event in (None, 'QUIT')) or (event in (sg.WINDOW_CLOSED, "-ESCAPE-")):
                sg.Popup('Closing Client APP', title='Closing', button_type=5, auto_close=True, auto_close_duration=1)
                break

            #if (values['_IN_'] == '') and (event != 'REGISTER' and event != 'CONNECTED USERS'):
             #   window['_CLIENT_'].print("c> No text inserted")
             #   continue

            if (client._alias == None or client._username == None or client._alias == 'Text' or client._username == 'Text' or client._date == None) and (event != 'REGISTER'):
                sg.Popup('NOT REGISTERED', title='ERROR', button_type=5, auto_close=True, auto_close_duration=1)
                continue

            if (event == 'REGISTER'):
                client.window_register()

                if (client._alias == None or client._username == None or client._alias == 'Text' or client._username == 'Text' or client._date == None):
                    sg.Popup('NOT REGISTERED', title='ERROR', button_type=5, auto_close=True, auto_close_duration=1)
                    continue

                window['_CLIENT_'].print('c> REGISTER ' + client._alias)
                client.register(client._alias, window)

            elif (event == 'UNREGISTER'):
                window['_CLIENT_'].print('c> UNREGISTER ' + client._alias)
                client.unregister(client._alias, window)


            elif (event == 'CONNECT'):
                window['_CLIENT_'].print('c> CONNECT ' + client._alias)
                client.connect(client._alias, window)


            elif (event == 'DISCONNECT'):
                window['_CLIENT_'].print('c> DISCONNECT ' + client._alias)
                client.disconnect(client._alias, window)


            elif (event == 'SEND'):
                window['_CLIENT_'].print('c> SEND ' + values['_INDEST_'] + " " + values['_IN_'])

                if (values['_INDEST_'] != '' and values['_IN_'] != '' and values['_INDEST_'] != 'User' and values['_IN_'] != 'Text') :
                    client.send(values['_INDEST_'], values['_IN_'], window)
                else :
                    window['_CLIENT_'].print("Syntax error. Insert <destUser> <message>")


            elif (event == 'SENDATTACH'):

                window['_CLIENT_'].print('c> SENDATTACH ' + values['_INDEST_'] + " " + values['_IN_'] + " " + values['_FILE_'])

                if (values['_INDEST_'] != '' and values['_IN_'] != '' and values['_FILE_'] != '') :
                    client.sendAttach(values['_INDEST_'], values['_IN_'], values['_FILE_'], window)
                else :
                    window['_CLIENT_'].print("Syntax error. Insert <destUser> <message> <attachedFile>")


            elif (event == 'CONNECTED USERS'):
                window['_CLIENT_'].print("c> CONNECTEDUSERS")
                client.connectedUsers(window)



            window.Refresh()

        window.Close()


if __name__ == '__main__':
    client.main([])
    print("+++ FINISHED +++")
