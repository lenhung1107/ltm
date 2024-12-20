#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LOG_FILE "server_log.txt"
#define USER "accounts.txt"
#define ROOM "room.txt"
#define PORT 8082
#define BUFFER_SIZE 1024
#define MAX_PLAYERS 100
#define MAX_ROOMS 100
#define MAX_WORDS 1000      // Số từ tối đa có thể lưu
#define MAX_WORD_LENGTH 100 // Độ dài từ tối đa

typedef struct
{
    char name[50];
    char password[50];
} User;

// Hàm nhập thông tin đăng ký từ người dùng
void getRegisterInfo(User *user)
{
    printf("Enter Name: ");
    fgets(user->name, 50, stdin);
    user->name[strcspn(user->name, "\n")] = '\0'; // Loại bỏ ký tự newline nếu có
    printf("Enter Password: ");
    fgets(user->password, 50, stdin);
    user->password[strcspn(user->password, "\n")] = '\0'; // Loại bỏ ký tự newline nếu có
}

// Hàm nhập thông tin đăng nhập từ người dùng
void getLoginInfo(char *name, char *password)
{
    printf("Enter Username: ");
    fgets(name, 50, stdin);
    name[strcspn(name, "\n")] = '\0'; // Loại bỏ ký tự newline nếu có
    printf("Enter Password: ");
    fgets(password, 50, stdin);
    password[strcspn(password, "\n")] = '\0'; // Loại bỏ ký tự newline nếu có
}

void showGameMenu(int sock, User user)
{
    int choice;
    char buffer[BUFFER_SIZE];

    while (1)
    {
        printf("\n--- Game Menu ---\n");
        printf("1. Create Room\n");
        printf("2. Join Room\n");
        printf("3. Exit Game\n");
        printf("Your choice: ");
        scanf("%d", &choice);
        getchar(); // Xóa ký tự '\n' trong buffer

        if (choice == 1)
        {
            printf("Enter your name (owner of the room): ");
            char owner[50];
            scanf("%s", owner);
            // Gửi yêu cầu tạo phòng
            snprintf(buffer, BUFFER_SIZE, "create_room %s", owner);
            send(sock, buffer, strlen(buffer), 0);
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received > 0)
            {
                buffer[bytes_received] = '\0';           // Đảm bảo chuỗi kết thúc
                printf("Server response: %s\n", buffer); // Hiển thị thông báo từ server
                if (strstr(buffer, "Room created successfully!"))
                {
                    printf("\n--- Room Options ---\n");
                    printf("1. Start Game\n");
                    printf("2. Back to Main Menu\n");
                    printf("Your choice: ");
                    int choice;
                    scanf("%d", &choice);
                    if (choice == 1)
                    {
                        // Gửi tín hiệu đến server để bắt đầu trò chơi
                        snprintf(buffer, BUFFER_SIZE, "start_game");
                        send(sock, buffer, strlen(buffer), 0);
                        printf("Waiting for server to start the game...\n");
                        memset(buffer, 0, BUFFER_SIZE);                              // Xóa buffer trước khi nhận phản hồi
                        int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0); // Nhận dữ liệu
                        if (bytes_received > 0)
                        {
                            buffer[bytes_received] = '\0'; // Đảm bảo kết thúc chuỗi
                            if (strncmp(buffer, "Game started successfully", 25) == 0)
                            {
                                printf("Game started successfully\n");
                                printf(" Ready to Guess Word\n");
                                printf("1. Yes \n");
                                printf("2. No \n");
                                printf("Your choice: ");
                                int choice;
                                scanf("%d", &choice);
                                if (choice == 1)
                                {
                                    snprintf(buffer, BUFFER_SIZE, "guess_word");
                                    send(sock, buffer, strlen(buffer), 0);
                                    printf("Waiting for server send guess word...\n");
                                    memset(buffer, 0, BUFFER_SIZE);
                                    int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
                                    if (bytes_received > 0)
                                    {
                                        buffer[bytes_received] = '\0';
                                        printf("Word to guess received: %s\n", buffer);

                                        // Gửi gợi ý
                                        // fflush(stdin);

                                        // Nhập gợi ý
                                        printf("Enter your hint for other players: ");
                                        int c;
                                        while ((c = getchar()) != '\n' && c != EOF)
                                            ;
                                        fgets(buffer, BUFFER_SIZE, stdin);
                                        buffer[strcspn(buffer, "\n")] = '\0'; // Loại bỏ newline
                                        send(sock, buffer, strlen(buffer), 0);
                                        printf("Hint sent to server.\n");

                                        printf("\n--- Options ---\n");
                                        printf("1. View scores\n");
                                        printf("2. Back to Main Menu\n");
                                        printf("Your choice: ");
                                        int choice;
                                        scanf("%d", &choice);
                                        if (choice == 1)
                                        {
                                            snprintf(buffer, BUFFER_SIZE, "view_scores");
                                            send(sock, buffer, strlen(buffer), 0);

                                            // Nhận phản hồi từ server
                                            memset(buffer, 0, BUFFER_SIZE); // Xóa buffer trước khi nhận dữ liệu
                                            int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
                                            if (bytes_received > 0)
                                            {
                                                buffer[bytes_received] = '\0'; // Đảm bảo chuỗi kết thúc
                                                printf("%s\n", buffer);        // Hiển thị nội dung điểm số
                                                
                                            }
                                            else
                                            {
                                                printf("Error: Failed to receive scores from server.\n");
                                            }
                                        }
                                    }
                                    else
                                    {
                                        printf("Error: Failed to receive response from server.\n");
                                    }
                                }
                            }
                            else
                            {
                                buffer[bytes_received] = '\0';           // Đảm bảo kết thúc chuỗi
                                printf("Server response: %s\n", buffer); // Hiển thị thông báo từ server
                                printf("Please wait for enough players. When the room is full, the system will start the game.\n");
                            }
                        }
                    }
                }
            }
            else
            {
                printf("Error: Failed to receive response from server.\n");
            }
        }
        else if (choice == 2)
        {
            // Nhập ID phòng và gửi yêu cầu tham gia phòng
            int room_id;
            printf("Enter Room ID: ");
            scanf("%d", &room_id);
            getchar();
            snprintf(buffer, BUFFER_SIZE, "join_room %d %s", room_id, user.name);
            send(sock, buffer, strlen(buffer), 0);
            memset(buffer, 0, BUFFER_SIZE);
            // Nhận dữ liệu từ server

            int bytes_received;
            while (1) // Vòng lặp nhận dữ liệu liên tục
            {
                bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
                if (bytes_received > 0)
                {
                    buffer[bytes_received] = '\0';        // Đảm bảo chuỗi kết thúc
                    if (strncmp(buffer, "Hint:", 5) == 0) // Kiểm tra gợi ý
                    {
                        printf("Hint received: %s\n", buffer + 5); // Hiển thị nội dung gợi ý                        // break; // Thoát khỏi vòng lặp nếu nhận đủ gợi ý
                        printf("\n--- Options ---\n");
                        printf("1. Send GuessWord\n");
                        printf("2. Back to Main Menu\n");
                        printf("Your choice: ");
                        int choice;
                        scanf("%d", &choice);
                        if (choice == 1)
                        {
                            printf("Enter your guess: ");
                            int c;
                            while ((c = getchar()) != '\n' && c != EOF)
                                ;
                            fgets(buffer, BUFFER_SIZE, stdin);
                            buffer[strcspn(buffer, "\n")] = '\0'; // Loại bỏ newline
                            char message[BUFFER_SIZE];
                            snprintf(message, sizeof(message), "guess:%s", buffer); // Định dạng từ đoán
                            send(sock, message, strlen(message), 0);                // Gửi tới server

                            char response[BUFFER_SIZE];
                            int bytes_received = recv(sock, response, BUFFER_SIZE - 1, 0);

                            if (bytes_received > 0)
                            {
                                response[bytes_received] = '\0'; // Đảm bảo chuỗi kết thúc
                                printf("Server response: %s\n", response);

                                if (strcmp(response, "Correct") == 0)
                                {
                                    printf("Congratulations! Your guess is correct.\n");
                                }
                                else if (strcmp(response, "Incorrect") == 0)
                                {
                                    printf("Sorry, your guess is incorrect. Try again!\n");
                                }
                                else
                                {
                                    printf("Unexpected server response: %s\n", response);
                                }
                                while (1)
                                {
                                    printf("\n--- Options ---\n");
                                    printf("1. View scores\n");
                                    printf("2. Back to Main Menu\n");
                                    printf("Your choice: ");

                                    int choice;
                                    scanf("%d", &choice);
                                    if (choice == 1)
                                    {
                                        snprintf(buffer, BUFFER_SIZE, "view_scores");
                                        send(sock, buffer, strlen(buffer), 0);

                                        // Nhận phản hồi từ server
                                        memset(buffer, 0, BUFFER_SIZE); // Xóa buffer trước khi nhận dữ liệu
                                        int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
                                        if (bytes_received > 0)
                                        {
                                            buffer[bytes_received] = '\0'; // Đảm bảo chuỗi kết thúc
                                            printf("%s\n", buffer);        // Hiển thị nội dung điểm số
                                        }
                                        else
                                        {
                                            printf("Error: Failed to receive scores from server.\n");
                                        }
                                    }
                                    else if (choice == 2)
                                    {
                                        printf("Back to Main Menu.\n");
                                        break; // Thoát vòng lặp và quay lại menu chính
                                    }
                                    else
                                    {
                                        printf("Invalid option. Please try again.\n");
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        printf("Server response: %s\n", buffer);
                    }
                }
                else if (bytes_received == 0)
                {
                    printf("Server closed the connection.\n");
                    break;
                }
                else
                {
                    perror("Error receiving data from server.");
                    break;
                }
                memset(buffer, 0, BUFFER_SIZE); // Xóa buffer trước khi nhận dữ liệu mới
            }
        }
        else if (choice == 3)
        {
            // Thoát game
            snprintf(buffer, BUFFER_SIZE, "exit_game");
            send(sock, buffer, strlen(buffer), 0);
            break;
        }
        else
        {
            printf("Invalid choice. Please try again.\n");
        }

        // Nhận phản hồi từ server
        memset(buffer, 0, BUFFER_SIZE);
        recv(sock, buffer, BUFFER_SIZE, 0);
        printf("Server response : %s\n", buffer);
    }
}

int main()
{
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    // Tạo socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Socket creation error\n");
        return -1;
    }

    // Thiết lập thông tin địa chỉ server
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Chuyển địa chỉ IP thành dạng nhị phân
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        printf("Invalid address\n");
        return -1;
    }

    // Kết nối đến server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("Connection Failed\n");
        return -1;
    }

    int choice;
    User user;
    char name[50], password[50];

    while (1)
    {
        // Menu chọn hành động
        // Menu chọn hành động
        printf("\n--- Menu ---\n1. Register\n2. Login\n0. Exit\nYour choice: ");
        if (scanf("%d", &choice) != 1)
        {
            printf("Invalid input, please enter a number.\n");
            while (getchar() != '\n')
                ; // Xóa toàn bộ buffer nhập liệu
            continue;
        }
        getchar();

        if (choice == 1)
        {
            // Nhập thông tin đăng ký
            getRegisterInfo(&user);
            snprintf(buffer, BUFFER_SIZE, "register %s %s",
                     user.name, user.password);
            send(sock, buffer, strlen(buffer), 0); // Gửi thông tin đăng ký đến server
        }
        else if (choice == 2)
        {
            // Nhập thông tin đăng nhập
            getLoginInfo(name, password);
            strncpy(user.name, name, sizeof(user.name) - 1);
            user.name[sizeof(user.name) - 1] = '\0'; // Đảm bảo chuỗi null-terminated

            strncpy(user.password, password, sizeof(user.password) - 1);
            user.password[sizeof(user.password) - 1] = '\0'; // Đảm bảo chuỗi null-terminated
            snprintf(buffer, BUFFER_SIZE, "login %s %s", name, password);
            send(sock, buffer, strlen(buffer), 0); // Gửi thông tin đăng nhập đến server
        }
        else if (choice == 0)
        {
            break; // Thoát khỏi vòng lặp
        }
        else
        {
            printf("Invalid choice, please select again.\n");
            continue;
        }

        // Đọc phản hồi từ server
        memset(buffer, 0, BUFFER_SIZE);                              // Xóa buffer trước khi nhận phản hồi
        int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0); // Nhận dữ liệu
        if (bytes_received > 0)
        {
            buffer[bytes_received] = '\0'; // Đảm bảo kết thúc chuỗi
            if (strncmp(buffer, "Login successful", 16) == 0)
            {

                printf("Login successful. Showing game menu...\n");
                showGameMenu(sock, user); // Chỉ hiển thị menu khi đăng nhập thành công
            }
        }
        else if (bytes_received == 0)
        {
            printf("Server closed the connection.\n");
            break;
        }
        else
        {
            printf("Error receiving data from server.\n");
        }
        printf("Response: %s\n", buffer);
    }
    // Đóng kết nối
    close(sock);
    return 0;
}