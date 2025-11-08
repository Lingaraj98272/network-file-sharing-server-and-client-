// server.cpp - C++ Implementation using POSIX Sockets
// NOW WITH "SIMPLE" AUTH & ENCRYPTION

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm> // for std::min

// --- CONFIGURATION ---
#define PORT 65432
#define BUFFER_SIZE 1024
const std::string SHARED_DIR = "server_files/";

// --- "SIMPLE" SECURITY CONSTANTS ---
const std::string ENCRYPTION_KEY = "a_very_simple_shared_key";
const std::string SERVER_PASSWORD = "supersecret123";

// Function Prototypes
void handle_client(int client_socket);
void send_file(int client_socket, const std::string& filename);
void receive_file(int client_socket, const std::string& filename);
std::string list_files();
void xor_encrypt_decrypt(char* data, size_t len, const std::string& key);

// ===========================================
// MAIN FUNCTION (Unchanged)
// ===========================================
int main() {
    // 1. Create socket file descriptor
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        return 1;
    }

    // 2. Bind the socket
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return 1;
    }

    // 3. Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        return 1;
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    // 4. Accept loop
    while (true) {
        int new_socket;
        socklen_t addrlen = sizeof(address);
        
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            perror("accept failed");
            continue;
        }

        std::cout << "New client connected. Waiting for authentication..." << std::endl;
        // Handle this client in a single thread
        handle_client(new_socket); 
    }
    
    close(server_fd); 
    return 0;
}

// ===========================================
// ENCRYPTION HELPER (NEW)
// ===========================================
void xor_encrypt_decrypt(char* data, size_t len, const std::string& key) {
    if (key.empty()) return;
    for (size_t i = 0; i < len; ++i) {
        data[i] = data[i] ^ key[i % key.length()];
    }
}

// ===========================================
// CLIENT HANDLER (HEAVILY MODIFIED)
// ===========================================
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    int valread;

    // --- 1. AUTHENTICATION PHASE ---
    valread = read(client_socket, buffer, BUFFER_SIZE);
    if (valread <= 0) {
        std::cerr << "Client disconnected before auth." << std::endl;
        close(client_socket);
        return;
    }

    // Decrypt the password
    xor_encrypt_decrypt(buffer, valread, ENCRYPTION_KEY);
    std::string received_password(buffer, valread);

    char response_buffer[32];
    if (received_password == SERVER_PASSWORD) {
        std::cout << "Client authenticated successfully." << std::endl;
        strncpy(response_buffer, "AUTH_OK", sizeof(response_buffer));
    } else {
        std::cerr << "Client failed authentication." << std::endl;
        strncpy(response_buffer, "AUTH_FAIL", sizeof(response_buffer));
    }

    // Encrypt and send the response
    xor_encrypt_decrypt(response_buffer, strlen(response_buffer), ENCRYPTION_KEY);
    send(client_socket, response_buffer, strlen(response_buffer), 0);
    
    // If auth failed, close connection
    if (received_password != SERVER_PASSWORD) {
        close(client_socket);
        return;
    }

    // --- 2. COMMAND LOOP PHASE ---
    // Keep handling commands from this client until they "QUIT"
    // or disconnect.
    while ( (valread = read(client_socket, buffer, BUFFER_SIZE)) > 0) {
        
        // Decrypt the command
        xor_encrypt_decrypt(buffer, valread, ENCRYPTION_KEY);
        std::string command_line(buffer, valread);

        // Trim newline characters
        command_line.erase(command_line.find_last_not_of(" \n\r\t")+1);

        std::cout << "Received command: [" << command_line << "]" << std::endl;

        std::string command, filename;
        size_t space_pos = command_line.find(' ');
        if (space_pos != std::string::npos) {
            command = command_line.substr(0, space_pos);
            filename = command_line.substr(space_pos + 1);
        } else {
            command = command_line;
        }

        // --- COMMAND LOGIC ---
        if (command == "LIST") {
            std::string file_list = list_files();
            // Encrypt and send file list
            xor_encrypt_decrypt(const_cast<char*>(file_list.c_str()), file_list.length(), ENCRYPTION_KEY);
            send(client_socket, file_list.c_str(), file_list.length(), 0);
        
        } else if (command == "GET" && !filename.empty()) {
            send_file(client_socket, filename); // Encryption handled inside

        } else if (command == "PUT" && !filename.empty()) {
            receive_file(client_socket, filename); // Decryption handled inside
        
        } else if (command == "QUIT") {
            std::cout << "Client sent QUIT. Closing connection." << std::endl;
            break; // Exit the loop
        }

        // Clear the buffer for the next read
        memset(buffer, 0, BUFFER_SIZE);
    }
    
    std::cout << "Client disconnected. Closing socket." << std::endl;
    close(client_socket); // Close the connection socket
}

// ===========================================
// SEND FILE (MODIFIED)
// ===========================================
void send_file(int client_socket, const std::string& filename) {
    std::string filepath = SHARED_DIR + filename;
    
    std::ifstream file(filepath, std::ios::in | std::ios::binary); 
    if (!file.is_open()) {
        std::string msg = "FILE_NOT_FOUND";
        long file_size = -1;
        
        // Encrypt and send error size
        xor_encrypt_decrypt(reinterpret_cast<char*>(&file_size), sizeof(long), ENCRYPTION_KEY);
        send(client_socket, &file_size, sizeof(long), 0);
        
        // Encrypt and send error message
        xor_encrypt_decrypt(const_cast<char*>(msg.c_str()), msg.length(), ENCRYPTION_KEY);
        send(client_socket, msg.c_str(), msg.length(), 0);
        
        std::cerr << "File not found: " << filename << std::endl;
        return;
    }
    
    // 1. Send file size (Crucial for receiver)
    struct stat file_status;
    stat(filepath.c_str(), &file_status);
    long file_size = file_status.st_size;
    
    // Encrypt and send file size
    long encrypted_size = file_size;
    xor_encrypt_decrypt(reinterpret_cast<char*>(&encrypted_size), sizeof(long), ENCRYPTION_KEY);
    send(client_socket, &encrypted_size, sizeof(long), 0);
    
    // 2. Send file chunks
    char buffer[BUFFER_SIZE];
    while (file.read(buffer, BUFFER_SIZE)) {
        // Encrypt chunk
        xor_encrypt_decrypt(buffer, BUFFER_SIZE, ENCRYPTION_KEY);
        send(client_socket, buffer, BUFFER_SIZE, 0);
    }
    // Send the last partial chunk
    if (file.gcount() > 0) {
        // Encrypt last chunk
        xor_encrypt_decrypt(buffer, file.gcount(), ENCRYPTION_KEY);
        send(client_socket, buffer, file.gcount(), 0);
    }

    file.close();
    std::cout << "Successfully sent file: " << filename << std::endl;
}

// ===========================================
// RECEIVE FILE (MODIFIED)
// ===========================================
void receive_file(int client_socket, const std::string& filename) {
    std::string filepath = SHARED_DIR + filename;
    
    // 1. Receive file size
    long file_size;
    if (read(client_socket, &file_size, sizeof(long)) <= 0) {
        std::cerr << "Error receiving file size." << std::endl;
        return;
    }
    // Decrypt file size
    xor_encrypt_decrypt(reinterpret_cast<char*>(&file_size), sizeof(long), ENCRYPTION_KEY);

    // 2. Open file for writing
    std::ofstream file(filepath, std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return;
    }
    
    // 3. Receive file chunks
    char buffer[BUFFER_SIZE];
    long total_received = 0;
    
    while (total_received < file_size) {
        long bytes_to_read = std::min((long)BUFFER_SIZE, file_size - total_received);
        
        int bytes_received = recv(client_socket, buffer, bytes_to_read, 0);
        
        if (bytes_received <= 0) {
            std::cerr << "Connection closed prematurely or error." << std::endl;
            break;
        }

        // Decrypt chunk
        xor_encrypt_decrypt(buffer, bytes_received, ENCRYPTION_KEY);

        file.write(buffer, bytes_received);
        total_received += bytes_received;
    }

    file.close();
    std::cout << "Successfully received file: " << filename << " (" << total_received << " bytes)" << std::endl;
}

// ===========================================
// FILE LISTING (Unchanged)
// ===========================================
std::string list_files() {
    DIR *dir;
    struct dirent *ent;
    std::string list = "";
    
    if ((dir = opendir(SHARED_DIR.c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            std::string name(ent->d_name);
            if (name != "." && name != "..") {
                list += name + "\n";
            }
        }
        closedir(dir);
    } else {
        list = "ERROR: Could not open shared directory.";
    }
    return list;
}