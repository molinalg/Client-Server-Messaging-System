# Client-Server Messaging System 

## Description
Developed in 2023, "Client-Server Messaging System" is a university project made during the third course of Computer Engineering at UC3M in collaboration with @EnriqueMorenoG88.

It was made for the subject "Distributed Systems" and corresponds to the final practice of this course. The main goal of the project is to learn how to use **threads**, **sockets** and the **client-server architecture** using **C** and **Python** programming languages. 

**NOTE:** The code and comments are in spanish.

## Table of Contents
- [Installation](#installation-linux)
- [Usage](#usage-linux)
- [Problem Proposed](#problem-proposed)
- [License](#license)
- [Contact](#contact)

## Installation (Linux)
To install the necessary libraries run the following command:

```sh
pip install spyne lxml PySimpleGUI
sudo apt-get install python3-tk
```

## Usage (Linux)
To execute this program, first compile the code using:
```sh
make
```
Then open 3 different terminals. In the first one, execute the server using:
```sh
./server -p 8080
```
**NOTE:** The port can be changed. 8080 is just an example.

In the second one, execute the web server using:
```sh
python3 servidor_web.py
```

Finally, run the client interface in the third terminal using:
```sh
python3 client.py
```

It is necessary to use a new terminal for every client that will have it's own interface to interact with.

**IMPORTANT:** After the development of this project, PySimpleGUI, one of the libraries used in it, stopped being free. This means that using the client interface won't be possible for most people.

## Problem Proposed
This repository includes a program in C and Python that uses a client-server architecture to work as a messaging system where **2 or more clients can exchange text messages**.

To do so, the code was divided into 3 main parts:

- **Client:** Developed in Python, the client uses sockets and threads to process the different requests made by the client to the server. Clients have to register in the system or log in to be able to send or receive messages. They can also eliminate their accounts or log off.

- **Server:** The server will process every request comming from the client making sure they stay in touch with every client logged in at any moment. This code is written in C.

- **Web server:** SOAP service in charge of processing messages users send to each other. It is an intermediate step between the client and the server and is in charge of eliminating any extra spaces before or after the text and between words.

Every interaction done by the client is done using the interface that simulates a regular instant messaging app.

## License
This project is licensed under the **MIT License**. This means you are free to use, modify, and distribute the software, but you must include the original license and copyright notice in any copies or substantial portions of the software.

## Contact
If necessary, contact the owner of this repository.
