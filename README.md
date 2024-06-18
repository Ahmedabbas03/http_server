# Multithreaded HTTP Server

A high-performance multithreaded HTTP server in C, featuring networking, security, and multithreading functionalities.

## Features
- POSIX-compliant implementation
- Multithreaded server using pthreads
- Basic HTTP response handling (200 OK, 404 Not Found, 501 Not Implemented)
- Error handling with C's errno and err functions
- File handling operations for reading from and writing to sockets

## Technologies Used
- C programming language
- POSIX threads (pthreads)
- Socket programming

## Usage
1. Compile the code using a C compiler.
2. Run the executable, specifying the desired port number as a command-line argument.
