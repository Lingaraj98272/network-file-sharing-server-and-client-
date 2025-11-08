// client.cpp - C++ Client Implementation using POSIX Sockets
// NOW WITH "SIMPLE" AUTH & ENCRYPTION (and password prompt)

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm> // For std::min

// --- CONFIGURATION ---
#define SERVER_IP "127.0.0.1" // Server IP address
#define PORT 65432
#define BUFFER_SIZE 1024
const std::string CLIENT_DIR = "client_downloads/"; 

// --- "SIMPLE" SECURITY CONSTANTS ---
const std::string ENCRYPTION_KEY = "a_very_simple_shared_key";
// --- SERVER_PASSWORD constant is removed ---

// Function Prototypes
void send_file(int sock, const std::string& filename);
void receive_file(int sock, const std::string& filename);
void xor_encrypt_decrypt(char* data, size_t len, const std::string& key);

// ===========================================
// MAIN FUNCTION (MODIFIED FOR PROMPT)
// ===========================================
int main() {
    // 1. Create socket file descriptor
    int sock = 0;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error." << std::endl;
        return -1;
    }

    // 2. Setup server address structure
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/Address not supported." << std::endl;
        return -1;
    }

    // 3. Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed. Is the server running?" << std::endl;
        return -1;
    }
    
    std::cout << "Connected to server." << std::endl;

    // --- 4. AUTHENTICATION PHASE (MODIFIED) ---
    char auth_buffer[BUFFER_SIZE] = {0};
    
    // --- ADDED: Prompt user for password ---
    std::string user_entered_password;
    std::cout << "Please enter the server password: ";
    std::getline(std::cin, user_entered_password);
    // --- END ADDED ---

    // Copy entered password to buffer, encrypt it, and send it
    strncpy(auth_buffer, user_entered_password.c_str(), BUFFER_SIZE);
    // Ensure we don't copy more than the buffer size
    auth_buffer[BUFFER_SIZE - 1] = '\0'; 
    
    xor_encrypt_decrypt(auth_buffer, user_entered_password.length(), ENCRYPTION_KEY);
    send(sock, auth_buffer, user_entered_password.length(), 0);

    // Wait for auth response
    memset(auth_buffer, 0, BUFFER_SIZE);
    int valread = recv(sock, auth_buffer, BUFFER_SIZE, 0);
    if (valread <= 0) {
        std::cerr << "Server disconnected during auth." << std::endl;
        close(sock);
        return -1;
    }

    // Decrypt response
    xor_encrypt_decrypt(auth_buffer, valread, ENCRYPTION_KEY);
    std::string auth_response(auth_buffer, valread);

    if (auth_response != "AUTH_OK") {
        std::cerr << "Authentication failed. Server response: " << auth_response << std::endl;
        close(sock);
        return -1;
    }

    std::cout << "Authentication successful." << std::endl;
    std::cout << "Successfully connected to server " << SERVER_IP << ":" << PORT << std::endl;

    // --- 5. MAIN COMMAND LOOP ---
    std::string command_line;
    char buffer[BUFFER_SIZE] = {0};

    while (true) {
        std::cout << "\nEnter command (LIST, GET <file>, PUT <file>, QUIT): ";
        std::getline(std::cin, command_line);
        
        if (command_line.empty()) continue;

        // Encrypt the command and send it
        char command_buffer[BUFFER_SIZE] = {0};
        strncpy(command_buffer, command_line.c_str(), BUFFER_SIZE);
        // Ensure null termination just in case
        command_buffer[BUFFER_SIZE - 1] = '\0';

        xor_encrypt_decrypt(command_buffer, command_line.length(), ENCRYPTION_KEY);
        send(sock, command_buffer, command_line.length(), 0);

        if (command_line == "QUIT") {
            break;
        }

        // Parse command locally to know what to expect
        std::string command, filename;
        size_t space_pos = command_line.find(' ');
        if (space_pos != std::string::npos) {
            command = command_line.substr(0, space_pos);
            filename = command_line.substr(space_pos + 1);
        } else {
            command = command_line;
        }

        if (command == "LIST") {
            // Receive, decrypt, and print file list
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_recvd = recv(sock, buffer, BUFFER_SIZE, 0);
            if (bytes_recvd > 0) {
                xor_encrypt_decrypt(buffer, bytes_recvd, ENCRYPTION_KEY);
                std::cout << "\n--- Available Files ---\n" << std::string(buffer, bytes_recvd) << "-----------------------\n";
            }
        
        } else if (command == "GET" && !filename.empty()) {
            receive_file(sock, filename); // Decryption handled inside

        } else if (command == "PUT" && !filename.empty()) {
            send_file(sock, filename); // Encryption handled inside
        }
    }

    close(sock);
    std::cout << "Client connection closed." << std::endl;
    return 0;
}

// ===========================================
// ENCRYPTION HELPER (Unchanged)
// ===========================================
void xor_encrypt_decrypt(char* data, size_t len, const std::string& key) {
    if (key.empty()) return;
    for (size_t i = 0; i < len; ++i) {
        data[i] = data[i] ^ key[i % key.length()];
    }
}

// ===========================================
// SEND FILE (Unchanged)
// ===========================================
void send_file(int sock, const std::string& filename) {
    std::string filepath = CLIENT_DIR + filename;
    
    std::ifstream file(filepath, std::ios::in | std::ios::binary); 
    if (!file.is_open()) {
        std::cerr << "Local file not found: " << filename << std::endl;
        // NOTE: This is a protocol flaw in our simple app.
        // We are not telling the server that the file doesn't exist.
        // The server will hang waiting for a file size.
        // A robust solution would send an error code *instead* of file size.
        // For this demo, we'll just return.
        return;
    }
    
    // 1. Send file size
    struct stat file_status;
    stat(filepath.c_str(), &file_status);
    long file_size = file_status.st_size;
    
    // Encrypt and send file size
    long encrypted_size = file_size;
    xor_encrypt_decrypt(reinterpret_cast<char*>(&encrypted_size), sizeof(long), ENCRYPTION_KEY);
    send(sock, &encrypted_size, sizeof(long), 0);
    
    // 2. Send file chunks
    char buffer[BUFFER_SIZE];
    while (file.read(buffer, BUFFER_SIZE)) {
        // Encrypt chunk
        xor_encrypt_decrypt(buffer, BUFFER_SIZE, ENCRYPTION_KEY);
        send(sock, buffer, BUFFER_SIZE, 0);
    }
    if (file.gcount() > 0) {
        // Encrypt last chunk
        xor_encrypt_decrypt(buffer, file.gcount(), ENCRYPTION_KEY);
        send(sock, buffer, file.gcount(), 0);
    }

    file.close();
    std::cout << "Successfully uploaded file: " << filename << std::endl;
}

// ===========================================
// RECEIVE FILE (Unchanged)
// ===========================================
void receive_file(int sock, const std::string& filename) {
    // 1. Receive file size (or error message)
    long file_size;
    if (recv(sock, &file_size, sizeof(long), 0) <= 0) {
        std::cerr << "Error receiving file size." << std::endl;
        return;
    }
    // Decrypt file size
    xor_encrypt_decrypt(reinterpret_cast<char*>(&file_size), sizeof(long), ENCRYPTION_KEY);

    // Check for server error
    if (file_size == -1) {
        char error_buffer[BUFFER_SIZE] = {0};
        int bytes_recvd = recv(sock, error_buffer, BUFFER_SIZE, 0);
        if (bytes_recvd > 0) {
            xor_encrypt_decrypt(error_buffer, bytes_recvd, ENCRYPTION_KEY);
            std::cerr << "Server Error: " << std::string(error_buffer, bytes_recvd) << std::endl;
        }
        return;
    }

    // 2. Open file for writing
    std::string filepath = CLIENT_DIR + filename;
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
        
        int bytes_received = recv(sock, buffer, bytes_to_read, 0);
        
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
    std::cout << "Successfully downloaded file: " << filename << " (" << total_received << " bytes)" << std::endl;
}