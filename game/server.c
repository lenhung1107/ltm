#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_PLAYERS 100
#define LOG_FILE "server_log.txt"
#define USER "accounts.txt"
#define ROOM "room.txt"
#define PORT 8082
#define BUFFER_SIZE 1024
#define MAX_ROOMS 100
#define MAX_WORDS 1000      // Số từ tối đa có thể lưu
#define MAX_WORD_LENGTH 100 // Độ dài từ tối đa

typedef struct
{
    char name[50];
    char password[50];
} User;

typedef struct
{
    int id;                          // ID phòng chơi
    char owner[50];                  // Tên chủ phòng
    char players[MAX_PLAYERS][50];   // Danh sách người chơi
    int player_count;                // Số lượng người chơi trong phòng
    int player_sockets[MAX_PLAYERS]; // Thêm mảng lưu socket của người chơi
    int game_started;                // Trạng thái phòng (0: chưa bắt đầu, 1: đã bắt đầu)
    int scores[MAX_PLAYERS];         // Thêm mảng lưu điểm của từng người chơi
    int turn;                        // Lượt chơi hiện tại (0: A, 1: B)
    char randomWord[BUFFER_SIZE];    // Từ cần đoán của phòng (sửa thành mảng ký tự)

} Room;

Room room_list[100];        // Danh sách phòng chơi
int room_count = 0;         // Số lượng phòng hiện tại
pthread_mutex_t room_mutex; // Mutex để tránh xung đột truy cập dữ liệu phòng

int saveUser(const char *filename, const User *user)
{
    FILE *file = fopen(filename, "a");
    if (file == NULL)
    {
        return -1;
    }
    fprintf(file, "%s %s\n", user->name, user->password);
    fclose(file);
    return 0;
}

int findUser(const char *filename, const char *name, User *foundUser)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        return -1;
    }

    User temp;
    while (fscanf(file, "%s %s",
                  temp.name, temp.password) == 2)
    {
        if (strcmp(temp.name, name) == 0)
        {
            *foundUser = temp;
            fclose(file);
            return 0;
        }
    }
    fclose(file);
    return -1;
}

int registerUser(const char *filename, const User *user)
{
    User foundUser;
    if (findUser(filename, user->name, &foundUser) == 0)
    {
        // Username đã tồn tại
        return -1;
    }
    return saveUser(filename, user);
}

int loginUser(const char *filename, const char *name, const char *password)
{
    User foundUser;
    if (findUser(filename, name, &foundUser) == 0)
    {
        if (strcmp(foundUser.password, password) == 0)
        {
            return 0; // Đăng nhập thành công
        }
    }
    return -1; // Đăng nhập thất bại
}

// Hàm ghi log
void log_message(const char *message)
{
    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file == NULL)
    {
        perror("Could not open log file");
        return;
    }
    time_t now = time(NULL);
    char time_str[26];
    struct tm *tm_info = localtime(&now);

    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(log_file, "[%s] %s\n", time_str, message);
    fclose(log_file);
}

// Hàm nạp dữ liệu các phòng từ file khi khởi động server
void load_rooms()
{
    FILE *file = fopen(ROOM, "r");
    if (file == NULL)
    {
        printf("Could not open room file. Starting with an empty room list.\n");
        return;
    }

    while (fscanf(file, "Room ID: %d, Owner: %s, Player Count: %d, Game Started: %d\n",
                  &room_list[room_count].id,
                  room_list[room_count].owner,
                  &room_list[room_count].player_count,
                  &room_list[room_count].game_started) == 4)
    {
        room_count++;
    }

    fclose(file);
    printf("Loaded %d rooms from file.\n", room_count);
}

// Lấy từ ngẫu nhiên
char *getRandomWord(const char *filename)
{
    static char words[MAX_WORDS][MAX_WORD_LENGTH];
    int wordCount = 0;

    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        perror("Error opening file");
        return NULL;
    }

    while (fscanf(file, "%s", words[wordCount]) != EOF && wordCount < MAX_WORDS)
    {
        wordCount++;
    }

    fclose(file);

    if (wordCount == 0)
    {
        printf("The file is empty or no words found.\n");
        return NULL;
    }

    srand(time(NULL));
    int randomIndex = rand() % wordCount;

    return words[randomIndex];
}

int createRoom(Room *room_list, int *room_count, const char *owner, int client_socket)
{
    if (*room_count >= MAX_ROOMS)
        return -1; // Danh sách phòng đã đầy

    Room *new_room = &room_list[*room_count];
    new_room->id = *room_count + 1; // ID phòng tự tăng
    strcpy(new_room->owner, owner);
    new_room->player_count = 1;
    strcpy(new_room->players[0], owner);
    new_room->player_sockets[0] = client_socket; // Lưu socket của người tạo phòng
    new_room->scores[0] = 10; // Gán điểm khởi tạo cho người tạo phòng (index 0)
    new_room->game_started = 0;
    (*room_count)++;
    // Mở file room.txt để ghi thông tin
    FILE *file = fopen("room.txt", "a"); // Mở file ở chế độ append
    if (file == NULL)
    {
        printf("Could not open room file.\n");
        return -2; // Lỗi khi mở file
    }
    fprintf(file, "Room ID: %d, Owner: %s, Player Count: %d, Game Started: %d\n",
            new_room->id, new_room->owner, new_room->player_count, new_room->game_started);
    fclose(file); // Đóng file sau khi ghi
    printf("Room created successfully and logged to room.txt\n");
    return new_room->id;
}

// Tham gia phòng
int joinRoom(Room *room_list, int room_count, const char *player, int player_socket, int room_id)
{

    for (int i = 0; i < room_count; i++)
    {
        if (room_list[i].id == room_id)
        {

            if (room_list[i].player_count >= MAX_PLAYERS)
                return -1; // Phòng đầy
            if (room_list[i].game_started)
                return -2; // Game đã bắt đầu

            strcpy(room_list[i].players[room_list[i].player_count], player);
            room_list[i].player_sockets[room_list[i].player_count] = player_socket; // Lưu socket của người chơi
            room_list[i].scores[room_list[i].player_count]=10;
            room_list[i].player_count++;
            FILE *file = fopen("room.txt", "w");
            if (file == NULL)
            {
                perror("Could not open room file for writing");

                break;
            }
            for (int j = 0; j < room_count; j++)
            {
                fprintf(file, "Room ID: %d, Owner: %s, Player Count: %d, Game Started: %d\n",
                        room_list[j].id,
                        room_list[j].owner,
                        room_list[j].player_count,
                        room_list[j].game_started);
            }
            fclose(file);
            return 0; // Tham gia thành công
        }
    }
    return -3; // Phòng không tồn tại
}
int startGame(Room *room_list, int room_count, const char *owner, int playerSocket)
{
    for (int i = 0; i < room_count; i++)
    {
        if (strcmp(room_list[i].owner, owner) == 0)
        {
            if (room_list[i].player_count < 2)
            {
                return -1; // Không đủ người chơi
            }
            room_list[i].game_started = 0;
            room_list[i].turn = 0; // Bắt đầu với người chơi A
            // Gửi thông báo đến tất cả người chơi rằng game đã bắt đầu
            char start_message[BUFFER_SIZE] = "Game has started! Wait for your turn.\n";
            for (int j = 0; j < room_list[i].player_count; j++)
            {
                if (room_list[i].player_sockets[j] != playerSocket)
                {
                    send(room_list[i].player_sockets[j], start_message, strlen(start_message), 0);
                }
            }
            return 0; // Game bắt đầu thành công
        }
    }
    return -2; // Không tìm thấy phòng
}
int broadcast_hint(Room *room_list, int room_count, int playerSocket, const char *owner, const char *hint)
{
    char formatted_hint[BUFFER_SIZE];
    snprintf(formatted_hint, sizeof(formatted_hint), "Hint: %s", hint); // Định dạng gợi ý
    for (int i = 0; i < room_count; i++)
    {
        if (strcmp(room_list[i].owner, owner) == 0)
        {
            for (int j = 0; j < room_list[i].player_count; j++)
            {
                if (room_list[i].player_sockets[j] != playerSocket)
                {
                    send(room_list[i].player_sockets[j], formatted_hint, strlen(formatted_hint), 0);
                }
            }
            return 0;
        }
    }
    return -2; // Không tìm thấy phòng
}

// Hàm xử lý client (chạy trên mỗi thread)
void *handle_client(void *arg)
{
    int client_socket = *(int *)arg;
    free(arg); // Giải phóng bộ nhớ đã cấp phát cho client socket
    char buffer[BUFFER_SIZE] = {0};

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received > 0)
        {
            buffer[bytes_received] = '\0';
            printf("Received: %s\n", buffer);
            log_message("Received from client:");
            log_message(buffer);
        }
        else if (bytes_received == 0)
        {
            printf("Client disconnected.\n");
            break;
        }
        else
        {
            perror("recv error");
            break;
        }

        // Xử lý đăng ký
        if (strncmp(buffer, "register", 8) == 0)
        {
            User user;
            sscanf(buffer + 9, "%s %s", user.name, user.password);

            char response[BUFFER_SIZE];
            memset(response, 0, BUFFER_SIZE);

            if (registerUser(USER, &user) == 0)
            {
                snprintf(response, BUFFER_SIZE, "Register success");
            }
            else
            {
                snprintf(response, BUFFER_SIZE, "Register failed");
            }
            send(client_socket, response, strlen(response), 0); // Gửi phản hồi
            log_message("Received sent to client: ");
            log_message(response);
        }

        // Xử lý đăng nhập
        else if (strncmp(buffer, "login", 5) == 0)
        {
            char name[50], password[50];
            sscanf(buffer + 6, "%s %s", name, password);

            char response[BUFFER_SIZE];
            memset(response, 0, BUFFER_SIZE);

            if (loginUser(USER, name, password) == 0)
            {
                snprintf(response, BUFFER_SIZE, "Login successful");
            }
            else
            {
                snprintf(response, BUFFER_SIZE, "Login failed");
            }
            send(client_socket, response, strlen(response), 0); // Gửi phản hồi
            log_message("Received sent to client: ");
            log_message(response);
        }

        // Xử lý các yêu cầu khác như trong code của bạn...
        else if (strncmp(buffer, "create_room", 11) == 0)
        {
            char owner[50];
            sscanf(buffer + 12, "%s", owner);

            pthread_mutex_lock(&room_mutex); // Dùng mutex để bảo vệ truy cập dữ liệu phòng
            int room_id = createRoom(room_list, &room_count, owner, client_socket);

            pthread_mutex_unlock(&room_mutex);

            if (room_id > 0)
            {
                sprintf(buffer, "Room created successfully! Room ID: %d\n", room_id);
            }
            else if (room_id == -1)
            {
                sprintf(buffer, "Failed to create room: Room list is full.\n");
            }
            else
            {
                sprintf(buffer, "Failed to create room: Unknown error.\n");
            }

            send(client_socket, buffer, strlen(buffer), 0);
            log_message("Sent to client:");
            log_message(buffer);
        }

        else if (strncmp(buffer, "join_room", 9) == 0)
        {
            int room_id;
            char player[50];
            sscanf(buffer + 10, "%d %s", &room_id, player);
            pthread_mutex_lock(&room_mutex); // Bảo vệ dữ liệu room_listF

            int result = joinRoom(room_list, room_count, player, client_socket, room_id);
            pthread_mutex_unlock(&room_mutex);
            if (result == 0)
            {
                snprintf(buffer, BUFFER_SIZE, "Joined room %d successfully.", room_id);
                if (room_list->game_started == 0)
                {
                    snprintf(buffer, BUFFER_SIZE, "Waiting... Game has not started yet.\n");
                }
                else
                {
                    snprintf(buffer, BUFFER_SIZE, "Game start. Wait to receive hint.\n");
                }
            }
            else if (result == -1)
            {
                snprintf(buffer, BUFFER_SIZE, "Room is full.");
            }
            else if (result == -2)
            {
                snprintf(buffer, BUFFER_SIZE, "Game has already started.");
            }
            else
            {
                snprintf(buffer, BUFFER_SIZE, "Room ID not found.");
            }
            send(client_socket, buffer, strlen(buffer), 0);
            log_message("Sent to client: ");
            log_message(buffer);
        }
        // Các lệnh khác tương tự...
        else if (strncmp(buffer, "start_game", 10) == 0)
        {
            char owner[50];
            sscanf(buffer + 11, "%s", owner);
            pthread_mutex_lock(&room_mutex); // Bảo vệ dữ liệu phòng
            int result = startGame(room_list, room_count, owner, client_socket);
            pthread_mutex_unlock(&room_mutex);
            if (result == 0)
            {
                snprintf(buffer, BUFFER_SIZE, "Game started successfully");
            }
            else if (result == -1)
            {
                snprintf(buffer, BUFFER_SIZE, "Not enough players to start.");
            }
            else
            {
                snprintf(buffer, BUFFER_SIZE, "Room not found.");
            }
            send(client_socket, buffer, strlen(buffer), 0);
            log_message("Sent to client: ");
            log_message(buffer);
        }
        else if (strncmp(buffer, "guess_word", 10) == 0)
        {
            printf("Ready to guess the word!\n");

            char *word = getRandomWord("word.txt");
            if (word != NULL)
            {
                // Gửi từ gợi ý cho client
                char message[BUFFER_SIZE];
                snprintf(message, sizeof(message), "Word received: %s", word);
                send(client_socket, message, strlen(message), 0);
                for (int i = 0; i < room_count; i++)
                {
                    for (int j = 0; j < room_list[i].player_count; j++)
                    {
                        if (room_list[i].player_sockets[j] == client_socket) // Kiểm tra client_socket
                        {
                            strncpy(room_list[i].randomWord, word, BUFFER_SIZE - 1); // Gán từ
                            room_list[i].randomWord[BUFFER_SIZE - 1] = '\0';         // Đảm bảo kết thúc chuỗi
                            printf("Room %d randomWord set to: %s\n", room_list[i].id, room_list[i].randomWord);
                            break;
                        }
                    }
                }
                // Nhận gợi ý từ người chơi
                char hint[BUFFER_SIZE]; // Thêm biến hint để lưu gợi ý
                memset(hint, 0, BUFFER_SIZE);
                int bytes_received = recv(client_socket, hint, BUFFER_SIZE - 1, 0);

                if (bytes_received > 0)
                {
                    hint[bytes_received] = '\0'; // Kết thúc chuỗi gợi ý
                    printf("Hint received from client : %s\n", hint);

                    // Gửi gợi ý tới tất cả người chơi khác trong cùng phòng
                    // broadcast_hint(room_list, room_count, client_socket, hint);

                    char owner[50];
                    sscanf(buffer + 11, "%s", owner);
                    pthread_mutex_lock(&room_mutex); // Bảo vệ dữ liệu phòng
                    int result = broadcast_hint(room_list, room_count, client_socket, owner, hint);

                    if (result == 0)
                    {
                        printf("Hint broadcasted successfully.\n");
                    }
                    else
                    {
                        printf("Error broadcasting hint.\n");
                    }

                    pthread_mutex_unlock(&room_mutex);
                    log_message("Sent to client: ");
                    log_message(buffer);
                }
                else
                {
                    printf("Error receiving hint from sender.\n");
                }
            }
            else
            {
                printf("Word file is empty or error reading.\n");
                send(client_socket, "No words available!", strlen("No words available!"), 0);
            }
        }

        else if (strncmp(buffer, "guess:", 6) == 0) // Kiểm tra nếu nhận từ đoán
        {
            char guessedWord[BUFFER_SIZE];
            strncpy(guessedWord, buffer + 6, BUFFER_SIZE - 1); // Lấy từ đoán từ buffer
            guessedWord[BUFFER_SIZE - 1] = '\0';               // Đảm bảo chuỗi kết thúc

            printf("Received guessed word: %s\n", guessedWord);

            pthread_mutex_lock(&room_mutex); // Bảo vệ dữ liệu phòng

            int found_room = 0;
            for (int i = 0; i < room_count; i++)
            {
                for (int j = 0; j < room_list[i].player_count; j++)
                {

                    if (room_list[i].player_sockets[j] == client_socket)
                    {
                        found_room = 1;
                        printf("Room %d randomWord: %s\n", room_list[i].id, room_list[i].randomWord);

                        if (strcmp(room_list[i].randomWord, guessedWord) == 0)
                        {
                            // Từ đoán chính xác
                            room_list[i].scores[j] += 1; // Cộng điểm cho người chơi
                            send(client_socket, "Correct", strlen("Correct"), 0);
                            printf("Player guessed correctly!\n");
                            int word_length = strlen(room_list[i].randomWord);
                            for (int k = 0; k < room_list[i].player_count; k++)
                            {
                                if (strcmp(room_list[i].owner, room_list[i].players[k]) == 0)
                                {
                                    room_list[i].scores[k] += word_length;
                                    break;
                                }
                            }
                            printf("Player guessed correctly! Score updated.\n");
                        }
                        else
                        {
                            // Từ đoán sai
                            room_list[i].scores[j] -= 1; // Trừ điểm cho người chơi
                            send(client_socket, "Incorrect", strlen("Incorrect"), 0);
                            printf("Player guessed incorrectly.\n");
                        }
                        break;
                    }
                }
                if (found_room)
                    break;
            }

            if (!found_room)
            {
                printf("Error: Room not found for client_socket %d\n", client_socket);
                send(client_socket, "Error: Room not found!", strlen("Error: Room not found!"), 0);
            }

            pthread_mutex_unlock(&room_mutex);
        }

        else if (strcmp(buffer, "view_scores") == 0)
        {
            pthread_mutex_lock(&room_mutex);

            int found_room = 0;
            for (int i = 0; i < room_count; i++)
            {
                for (int j = 0; j < room_list[i].player_count; j++)
                {
                    if (room_list[i].player_sockets[j] == client_socket)
                    {
                        found_room = 1;

                        char score_message[BUFFER_SIZE];
                          memset(score_message, 0, BUFFER_SIZE); // Reset buffer trước khi dùng
                        snprintf(score_message, sizeof(score_message), "Scores:\n");

                        for (int k = 0; k < room_list[i].player_count; k++)
                        {
                            char player_score[100];
                            snprintf(player_score, sizeof(player_score), "%s: %d\n", room_list[i].players[k], room_list[i].scores[k]);
                            strncat(score_message, player_score, sizeof(score_message) - strlen(score_message) - 1);
                        }

                        send(client_socket, score_message, strlen(score_message), 0);
                        break;
                    }
                }
                if (found_room)
                    break;
            }

            if (!found_room)
            {
                send(client_socket, "Error: Room not found!", strlen("Error: Room not found!"), 0);
            }

            pthread_mutex_unlock(&room_mutex);
        }

        else if (strncmp(buffer, "exit", 4) == 0)
        {
            printf("Client requested disconnection.\n");
            break;
        }
        else
        {
            char response[BUFFER_SIZE] = "Invalid command";
            send(client_socket, response, strlen(response), 0);
        }
    }

    close(client_socket);
    return NULL;
}

int main()
{
    int server_fd, *new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    // Tạo socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Gắn socket với địa chỉ và cổng
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    pthread_mutex_init(&room_mutex, NULL); // Khởi tạo mutex
    load_rooms();                          // Tải lại dữ liệu các phòng từ file

    while (1)
    {
        new_socket = malloc(sizeof(int));
        if ((*new_socket = accept(server_fd, (struct sockaddr *)&address,
                                  (socklen_t *)&addrlen)) < 0)
        {
            perror("Accept failed");
            free(new_socket);
            continue;
        }

        printf("New client connected.\n");

        pthread_t client_thread;
        pthread_create(&client_thread, NULL, handle_client, (void *)new_socket);
        pthread_detach(client_thread); // Tự động thu hồi tài nguyên sau khi thread kết thúc
    }

    pthread_mutex_destroy(&room_mutex); // Hủy mutex
    return 0;
}