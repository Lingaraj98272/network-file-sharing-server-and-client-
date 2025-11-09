Here is the raw Markdown code for the `README.md` file. You can copy this directly:


# Simple C++ Socket File Transfer

This project is a simple, command-line based file sharing application built in C++. It uses POSIX TCP sockets to create a client-server architecture where clients can connect to a server to list, upload (`PUT`), and download (`GET`) files from a shared directory.

## 📜 Features

* **List Files:** View all files available in the server's shared directory (`LIST`).

* **Download Files:** Retrieve a file from the server (`GET <filename>`).

* **Upload Files:** Send a file from the client's directory to the server (`PUT <filename>`).

* **Persistent Connection:** The server handles multiple commands from a single client until they send the `QUIT` command.

## ⚙️ Prerequisites

* A C++11 compliant compiler (e.g., `g++` or `clang++`).

* A POSIX-compliant OS (Linux, macOS, BSD).

## 🚀 How to Build

You can compile both the `server.cpp` and `client.cpp` files using `g++`:

```bash
# Compile the server
g++ server.cpp -o server

# Compile the client
g++ client.cpp -o client
````

## 🏃 How to Run

### 1\. Create Required Directories

Before running, you must create the directories that are hardcoded in the source files.

```bash
# Create the server's shared folder
mkdir server_files

# Create the client's download folder
mkdir client_downloads
```

You can place test files (e.g., `test.txt`) into the `server_files/` directory for the `LIST` and `GET` commands to work.

### 2\. Start the Server

Open a terminal and run the compiled server executable:

```bash
./server
```

You should see the message:
`Server listening on port 65432`

### 3\. Run the Client

Open a separate terminal and run the client executable:

```bash
./client
```

You should see the message:

```
Successfully connected to server 127.0.0.1:65432
Enter command (LIST, GET <file>, PUT <file>, QUIT):
```

## ⌨️ How to Use (Client Commands)

Once the client is connected, you can use the following commands:

  * **`LIST`**
    Displays all files available in the server's `server_files/` directory.

  * **`GET <filename>`**
    Downloads the specified file from the server's `server_files/` directory to your local `client_downloads/` directory.

      * *Example:* `GET test.txt`

  * **`PUT <filename>`**
    Uploads a file from your local `client_downloads/` directory to the server's `server_files/` directory.

      * *Example:* `PUT my_upload.log`

  * **`QUIT`**
    Disconnects from the server and exits the client application.

<!-- end list -->

```
```
