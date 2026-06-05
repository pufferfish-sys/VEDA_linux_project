#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#define BUF_SIZE 1024

// 서버에 연결하여 명령을 보내고 결과를 받아오는 함수
void send_command_to_server(const char *host, int port, const char *cmd) {
    int sockfd;
    int readn;
    char buf[BUF_SIZE];
    struct hostent *he;
    struct sockaddr_in server_addr;

    if ((he = gethostbyname(host)) == NULL) {
        perror("[ERROR] gethostbyname() 실패");
        exit(1);
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[ERROR] socket() 생성 실패");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr = *(struct in_addr *)he->h_addr;

    // 💡 여기서 연결이 실패하면 메뉴 화면으로 가지 않고 즉시 프로그램을 종료합니다.
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("\n❌ [FATAL ERROR] 라즈베리파이 서버 연결 실패 (Connection refused)");
        fprintf(stderr, "서버가 켜져 있는지, IP/포트(%s:%d)가 맞는지 확인하세요.\n\n", host, port);
        close(sockfd);
        exit(1); // 프로그램 즉시 폭파
    }

    snprintf(buf, sizeof(buf), "%s\n", cmd);
    if (write(sockfd, buf, strlen(buf)) == -1) {
        perror("[ERROR] 데이터 전송 실패");
        close(sockfd);
        return;
    }

    memset(buf, 0, sizeof(buf));
    readn = read(sockfd, buf, sizeof(buf) - 1);
    if (readn > 0) {
        buf[readn] = '\0';
        printf("\n📢 [라즈베리파이 응답]: %s", buf);
    }
    close(sockfd);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <Server IP> <Server Port>\n", argv[0]);
        exit(1);
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]); 
    char main_input[64];
    char sub_input[64];
    int is_led_on = 0; 

    // 1. 화면 띄우기 전에 연결부터 무조건 시도
    printf("🌐 [NETWORK] 라즈베리파이 서버 (%s:%d) 연결 시도 중...\n", server_ip, server_port);
    
    // 이 함수 안에서 connect가 실패하면 'exit(1)'로 프로세스가 무조건 즉시 파괴됩니다.
    send_command_to_server(server_ip, server_port, "ledOff");
    
    // connect가 성공해야만 비로소 이 아래 줄로 내려옵니다.
    printf("✅ [SUCCESS] 서버가 확인되었습니다. 제어판을 시작합니다.\n");

    printf("==================================================\n");
    printf("🖥️  우분투 전용 라즈베리파이 원격 제어 클라이언트\n");
    printf("==================================================\n");

    // 💡 초기 연결 관문을 통과한 경우에만 아래 메뉴판 반복문이 작동합니다.
    while (1) {
        printf("\n🏠 [ 메인 모드 선택 메뉴 ]\n");
        printf("1: LED 제어 모드\n");
        printf("2: 조도 센서 값 측정\n");
        printf("3: 부저 음악 재생 모드\n");
        printf("4: 7세그먼트 카운트다운 모드\n");
        printf("q: 프로그램 완전히 종료\n");
        printf("👉 모드를 선택하세요: ");

        if (fgets(main_input, sizeof(main_input), stdin) == NULL) break;
        main_input[strcspn(main_input, "\r\n")] = '\0';

        if (strcmp(main_input, "q") == 0 || strcmp(main_input, "Q") == 0) {
            printf("\n[CLIENT] 제어 프로그램을 완전히 종료합니다.\n");
            break;
        }

        // ================= [ 1번: LED 제어 모드 ] =================
        if (strcmp(main_input, "1") == 0) {
            while (1) {
                printf("\n💡 [ LED 제어 서브 메뉴 ] (현재상태: %s)\n", is_led_on ? "ON" : "OFF");
                printf("1: 켜기 (ledOn)\n");
                printf("2: 밝기 - 낮음 (min)\n");
                printf("3: 밝기 - 보통 (mid)\n");
                printf("4: 밝기 - 높음 (max)\n");
                printf("5: 끄기 (ledOff)\n");
                printf("q: 메인 화면으로 돌아가기\n");
                printf("👉 명령 선택: ");

                if (fgets(sub_input, sizeof(sub_input), stdin) == NULL) break;
                sub_input[strcspn(sub_input, "\r\n")] = '\0';

                if (strcmp(sub_input, "q") == 0 || strcmp(sub_input, "Q") == 0) {
                    printf("🏠 메인 화면으로 이동합니다.\n");
                    break;
                }

                if (strcmp(sub_input, "1") == 0) {
                    send_command_to_server(server_ip, server_port, "ledOn");
                    is_led_on = 1;
                } 
                else if (strcmp(sub_input, "5") == 0) {
                    send_command_to_server(server_ip, server_port, "ledOff");
                    is_led_on = 0;
                } 
                else if (strcmp(sub_input, "2") == 0 || strcmp(sub_input, "3") == 0 || strcmp(sub_input, "4") == 0) {
                    if (!is_led_on) {
                        printf("\n❌ [경고] 밝기를 조절하려면 먼저 LED를 켜야 합니다! (1번 선택)\n");
                        continue;
                    }
                    
                    if (strcmp(sub_input, "2") == 0) send_command_to_server(server_ip, server_port, "min");
                    else if (strcmp(sub_input, "3") == 0) send_command_to_server(server_ip, server_port, "mid");
                    else if (strcmp(sub_input, "4") == 0) send_command_to_server(server_ip, server_port, "max");
                }
                else {
                    printf("\n❌ 올바른 번호를 입력해 주세요.\n");
                }
                usleep(100000);
            }
        } 
        // ================= [ 2번: 조도 센서 측정 ] =================
        else if (strcmp(main_input, "2") == 0) {
            printf("\n☀️ 조도 센서 값을 계측합니다...");
            send_command_to_server(server_ip, server_port, "cdsCheck");
        } 
        // ================= [ 3번: 부저 음악 재생 모드 ] =================
        else if (strcmp(main_input, "3") == 0) {
            while (1) {
                printf("\n🎵 [ 부저 음악 재생 서브 메뉴 ]\n");
                printf("1: 마리오 음악 시작 (buzzerOn)\n");
                printf("q: 음악 끄고 메인 화면으로 나가기\n");
                printf("👉 선택: ");

                if (fgets(sub_input, sizeof(sub_input), stdin) == NULL) break;
                sub_input[strcspn(sub_input, "\r\n")] = '\0';

                if (strcmp(sub_input, "1") == 0) {
                    send_command_to_server(server_ip, server_port, "buzzerOn");
                } 
                else if (strcmp(sub_input, "q") == 0 || strcmp(sub_input, "Q") == 0) {
                    printf("\n🛑 음악 정지 명령 전송 중...\n");
                    send_command_to_server(server_ip, server_port, "buzzerOff");
                    printf("🏠 메인 화면으로 이동합니다.\n");
                    break;
                } 
                else {
                    printf("\n❌ 올바른 옵션을 선택하세요.\n");
                }
                usleep(100000);
            }
        } 
        // ================= [ 4번: 세그먼트 모드 ] =================
        else if (strcmp(main_input, "4") == 0) {
            while (1) {
                printf("\n⏱️  [ 7세그먼트 카운트다운 서브 메뉴 ]\n");
                printf("1: 5초 카운트다운 시작 (countdown)\n");
                printf("q: 메인 화면으로 돌아가기\n");
                printf("👉 선택: ");

                if (fgets(sub_input, sizeof(sub_input), stdin) == NULL) break;
                sub_input[strcspn(sub_input, "\r\n")] = '\0';

                if (strcmp(sub_input, "1") == 0) {
                    send_command_to_server(server_ip, server_port, "countdown");
                }
                else if (strcmp(sub_input, "q") == 0 || strcmp(sub_input, "Q") == 0) {
                    printf("🏠 메인 화면으로 이동합니다.\n");
                    break;
                }
                else {
                    printf("\n❌ 올바른 옵션을 선택하세요.\n");
                }
                usleep(100000);
            }
        } 
        else {
            printf("\n❌ 잘못된 입력입니다. 메뉴판의 번호를 선택해 주세요.\n");
        }
        usleep(200000); 
    }

    return 0;
}