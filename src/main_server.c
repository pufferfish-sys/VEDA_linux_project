#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dlfcn.h>
#include "hardware_control.h"
#include <wiringPi.h>

#define BUF_SIZE 1024
#define LED_PIN 0

static int is_run = 1;
static int ssock;
static int led_status = 0; // LED 상태 저장 변수
// 충돌 방지 뮤텍스 선언
pthread_mutex_t led_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cds_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t buzzer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fnd_mutex = PTHREAD_MUTEX_INITIALIZER;

// 동적 라이브러리 함수 포인터 타입 정의
typedef int (*led_func_t)(const char*);
typedef int (*cds_func_t)(void);
typedef int (*buzzer_func_t)(int);
typedef int (*fnd_func_t)(int);

// 전역 함수 프로토타입 선언
void make_daemon(char *cmd_name);
void handle_signal(int sig);
void* webserverFunction(void *arg);

static void *clnt_connection(void *arg);
void sendData(int csock, int cds_val);
void sendOk(int csock);
void sendError(int csock);

int main(int argc, char **argv) {

  pthread_t ptWebserver;
  
  if(argc != 2) {
    printf("usage: %s <port>\n", argv[0]);
    return -1;
  }
  
  // 데몬 프로세스 생성 로직 가동
  make_daemon(argv[0]);
  
  wiringPiSetup();
  init_led_library();
  
  
  // 데몬 백그라운드 구동 중 종료 시그널 감시 핸들러 등록
  struct sigaction sa_term;
  sa_term.sa_handler = handle_signal;
  sigemptyset(&sa_term.sa_mask);
  sa_term.sa_flags = 0;
  sigaction(SIGTERM, &sa_term, NULL);
  sigaction(SIGINT, &sa_term, NULL);
  
  // 웹서버를 독립적으로 구동할 대기실 쓰레드 생성
  pthread_create(&ptWebserver, NULL, webserverFunction, (void*)(intptr_t)atoi(argv[1]));
  
  //백그라운드는 데몬이므로 메인 쓰레드는 무한 대기하며 시그널 기다림   
  while(is_run) {
    pause(); // 시그널이 올 때까지 대기
  }
  
  pthread_join(ptWebserver, NULL);
  syslog(LOG_INFO, "Daemon Server Safely Terminated.");
  
  closelog();
  return 0;
}


// daemon 예제 활용 함수
void make_daemon(char *cmd_name) {

  struct sigaction sa;
  struct rlimit rl;
  int fd0, fd1, fd2, i;
  pid_t pid;
  
  // 
  umask(0);
  
  if(getrlimit(RLIMIT_NOFILE, &rl) <0) {
    perror("getlimit()");
  }
  
  if((pid = fork()) < 0){
    perror("error()");
    exit(1);  
  } else if(pid != 0) { // 부모 프로세스 종료
    exit(0);
  }

  // 터미널을 제어할 수 없도록 세션의 리더가 됨
  setsid();
  
  sa.sa_handler = SIG_IGN;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if(sigaction(SIGHUP, &sa, NULL) < 0) {
      perror("sigaction() : Can't ignore SIGHUP");
  }
  
  /* 프로세스의 워킹 디렉터리를 ‘/’로 설정한다. */
    if(chdir("/") < 0) {
        perror("cd()");
  }
  // 프로세스의 모든 파일 디스크립터 닫기
  if(rl.rlim_max == RLIM_INFINITY) {
    rl.rlim_max = 1024;
  }
  
  for(i = 0; i < rl.rlim_max; i++) {
    close(i);
  }
  
  /* 파일 디스크립터 0, 1과 2를 /dev/null로 연결한다. */
  fd0 = open("/dev/null", O_RDWR);
  fd1 = dup(0);
  fd2 = dup(0);
  
  /* 로그 출력을 위한 파일 로그를 연다. */
  openlog(cmd_name, LOG_CONS, LOG_DAEMON);
    
  if(fd0 != 0 || fd1 != 1 || fd2 != 2) {
    syslog(LOG_ERR, "unexpected file descriptors %d %d %d", fd0, fd1, fd2);
    exit(1);
  }
  
/* 로그 파일에 정보 수준의 로그를 출력한다. */
  syslog(LOG_INFO, "Daemon Process Initialized Successfully.");
    
}

// 종료 신호를 받았을 때 안전하게 루프를 탈출하기 위한 핸들러
void handle_signal(int sig) {
  syslog(LOG_INFO, "Termination Signal %d Received.", sig);
  is_run = 0;
  close(ssock); // 리스닝 소켓을 닫아 accept 루프 탈출 유도
}

// 웹 서버 함수
void* webserverFunction(void *arg) {

  struct sockaddr_in servaddr, cliaddr;
  socklen_t len = sizeof(cliaddr);
  int port = (int)(intptr_t)arg;
  
  // 소켓 유형 TCP 지정 
  ssock = socket(AF_INET, SOCK_STREAM, 0);
  
  if(ssock == -1) {
    syslog(LOG_ERR, "Socket Creation Failed.");
    exit(1);
  } 
  
  // 소켓 옵션 설정: 서버 비정상 종료 시 포트가 TIME_WAIT을 무시하고 즉시 재사용하도록 설정
  int opt = 1;
  setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));  
  
  // 서버 주소 구조체 초기화 및 설정
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;                // IPv4 체계 지정
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // 어떤 IP로 들어오는 접속이든 수용
  servaddr.sin_port = htons(port);             // 포트 번호 주입
  
  // Bind : 생성한 소켓에 설정한 IP 주소와 포트 번호를 귀속 및 할당
  if(bind(ssock, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
    syslog(LOG_ERR, "Bind Failed.");
    exit(1);
  }
  
  // Listen : 리스닝 모드, 동시 접속 대기 큐 10
    if(listen(ssock, 10) == -1) {
        syslog(LOG_ERR, "Listen Failed."); // 리슨 실패 시 로그 기록 후 프로그램 종료
        exit(1);
  }
  
  while(is_run) {
        int *csock = malloc(sizeof(int));
        *csock = accept(ssock, (struct sockaddr *)&cliaddr, &len);
        if(*csock == -1) {
            free(csock);
            continue;
        }
        
        pthread_t clnt_thread;
        pthread_create(&clnt_thread, NULL, clnt_connection, (void*)csock);
        pthread_detach(clnt_thread);
    }
    
    return NULL;
    
}

void *clnt_connection(void *arg) {
    int csock = *((int*)arg);
    free(arg);
    
    char buf[BUF_SIZE];
    char req_line[BUF_SIZE];
    int current_cds = -1;  
    int is_browser = 0;    // 브라우저 진입(HTTP 표준 포맷)인지, 우분투 터미널 진입인지 구별하는 플래그
    int valid_request = 1; 
    
    int readn = read(csock, buf, sizeof(buf) - 1);
    if (readn <= 0) {
        close(csock);
        return NULL;
    }
    buf[readn] = '\0';
    
    // 문자열에 "GET "이 포함되어 있다면 브라우저 요청으로 판단합니다.
    if (strstr(buf, "GET ") != NULL) {
        is_browser = 1;
        char *token = strtok(buf, "\n");
        if (token == NULL) { close(csock); return NULL; }
        strcpy(req_line, token);
    } else {
        // 개행 문자(\r, \n)를 제거하여 순수 명령어만 추출합니다.
        buf[strcspn(buf, "\r\n")] = '\0';
        strcpy(req_line, buf);
    }
    
    // 2) LED 제어 파트 (개별 명령어 분기 방식 및 상태 체크 반영)
    if (strstr(req_line, "GET /ledOn") != NULL || strcmp(req_line, "ledOn") == 0) {
        handle_led_logic("on");
        led_status = 1; // LED 상태를 '켜짐'으로 기록
    }
    else if (strstr(req_line, "GET /ledOff") != NULL || strcmp(req_line, "ledOff") == 0) {
        handle_led_logic("off");
        led_status = 0; // LED 상태를 '꺼짐'으로 기록
    }
    
    // --- 밝기 조절 영역 (led_status가 1(켜짐)일 때만 작동) ---
    else if (strstr(req_line, "GET /max") != NULL || strcmp(req_line, "max") == 0) {
        if (led_status == 1) {
            handle_led_logic("max");
        } else {
            syslog(LOG_INFO, "LED is OFF. 'max' command ignored.");
        }
    }
    else if (strstr(req_line, "GET /mid") != NULL || strcmp(req_line, "mid") == 0) {
        if (led_status == 1) {
            handle_led_logic("mid");
        } else {
            syslog(LOG_INFO, "LED is OFF. 'mid' command ignored.");
        }
    }
    else if (strstr(req_line, "GET /min") != NULL || strcmp(req_line, "min") == 0) {
        if (led_status == 1) {
            handle_led_logic("min");
        } else {
            syslog(LOG_INFO, "LED is OFF. 'min' command ignored.");
        }
    }
    // 3) 조도 센서 파트
    else if (strstr(req_line, "GET /cdsCheck") != NULL || strcmp(req_line, "cdsCheck") == 0) {
        current_cds = handle_cds_logic(); 
    }
    
    // 4) 부저 음악 On / Off
    else if (strstr(req_line, "GET /buzzerOn") != NULL || strcmp(req_line, "buzzerOn") == 0) {
        handle_buzzer_logic(1);
    }
    else if (strstr(req_line, "GET /buzzerOff") != NULL || strcmp(req_line, "buzzerOff") == 0) {
        handle_buzzer_logic(0);
    }
    
    // 5) 7세그먼트 파트
    else if (strstr(req_line, "countdown") != NULL) {
        int start_num = 5;
        char *p = strstr(req_line, "num=");
        if (p) start_num = atoi(p + 4);

        pthread_t fnd_th;
        pthread_create(&fnd_th, NULL, fnd_countdown_thread, (void*)(intptr_t)start_num);
        pthread_detach(fnd_th); 
    }

    // 6) 브라우저 기본 홈 화면 진입용 예외 처리
    else if (is_browser && (strstr(req_line, "GET / ") != NULL || strstr(req_line, "GET /index.html") != NULL)) {
        // 기본 홈 화면 접근 시에도 에러 없이 패스
    }
    
    
    else {
        valid_request = 0;
        if (is_browser) sendError(csock);
        else write(csock, "ERROR\n", 6);
    }
    
    // 최종 응답 전송부
    if (valid_request) {
        if (is_browser) {
           sendOk(csock);
            sendData(csock, current_cds);
        } else {
            if (current_cds >= 0) {
                char res_str[64];
                sprintf(res_str, "SUCCESS [CDS VALUE: %d]\n", current_cds);
                write(csock, res_str, strlen(res_str));
            } else {
                write(csock, "SUCCESS\n", 8);
            }
        }
    }
    
    close(csock); 
    return NULL;
}

// 웹서버 데이터 전송 포맷 함수 구현들

void sendData(int csock, int cds_val) {
    char cds_str[128] = "미측정 (확인 버튼을 누르세요)";
    if (cds_val >= 0) {
        sprintf(cds_str, "%d (%s)", cds_val, (cds_val < 180) ? "맑음 / LED OFF" : "어두움 / LED ON");
    }
    
    char html[8192];

    snprintf(html, sizeof(html),
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "    <meta charset=\"UTF-8\">"
        "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
        "    <title>IoT 통합 관제 시스템</title>"
        "    <style>"
        "        *{margin:0;padding:0;box-sizing:border-box;}"
        "        body{"
        "            min-height:100vh;"
        "            background:linear-gradient(135deg,#0F172A,#1E3A8A);"
        "            font-family:'Segoe UI',sans-serif;"
        "            color:white;"
        "            text-align:center;"
        "            padding:30px;"
        "        }"
        "        h1{"
        "            font-size:2.7rem;"
        "            margin-bottom:10px;"
        "            text-shadow:0 0 20px rgba(255,255,255,0.2);"
        "        }"
        "        .status{"
        "            color:#22C55E;"
        "            font-weight:bold;"
        "            letter-spacing:3px;"
        "            margin-bottom:35px;"
        "        }"
        "        .container{"
        "            display:flex;"
        "            flex-wrap:wrap;"
        "            justify-content:center;"
        "            gap:25px;"
        "            max-width:1200px;"
        "            margin:0 auto;"
        "        }"
        "        .card{"
        "            background:rgba(255,255,255,0.08);"
        "            backdrop-filter:blur(15px);"
        "            border:1px solid rgba(255,255,255,0.15);"
        "            border-radius:20px;"
        "            padding:25px;"
        "            flex:1 1 300px;"
        "            box-shadow:0 8px 25px rgba(0,0,0,0.35);"
        "            transition:all 0.3s ease;"
        "        }"
        "        .card:hover{"
        "            transform:translateY(-8px);"
        "            box-shadow:0 15px 35px rgba(0,0,0,0.45);"
        "        }"
        "        h3{"
        "            margin-bottom:20px;"
        "            color:#E2E8F0;"
        "            font-size:1.3rem;"
        "        }"
        "        p{"
        "            font-size:1.1rem;"
        "            margin-bottom:15px;"
        "        }"
        "        button{"
        "            border:none;"
        "            border-radius:10px;"
        "            padding:12px 20px;"
        "            margin:5px;"
        "            font-size:14px;"
        "            font-weight:bold;"
        "            color:white;"
        "            cursor:pointer;"
        "            transition:0.3s;"
        "        }"
        "        button:hover{"
        "            transform:scale(1.05);"
        "        }"
        "        .btn-on{"
        "            background:linear-gradient(45deg,#22C55E,#16A34A);"
        "        }"
        "        .btn-off{"
        "            background:linear-gradient(45deg,#EF4444,#B91C1C);"
        "        }"
        "        .btn-scan{"
        "            background:linear-gradient(45deg,#3B82F6,#2563EB);"
        "        }"
        "        .btn-start{"
        "            background:linear-gradient(45deg,#8B5CF6,#6D28D9);"
        "        }"
        "        input[type='number']{"
        "            width:80px;"
        "            padding:10px;"
        "            border:none;"
        "            border-radius:10px;"
        "            text-align:center;"
        "            font-size:16px;"
        "            background:rgba(255,255,255,0.15);"
        "            color:white;"
        "        }"
        "        a{"
        "            text-decoration:none;"
        "        }"
        "        @media(max-width:768px){"
        "            .card{flex:1 1 100%%;}"
        "            h1{font-size:2rem;}"
        "        }"
        "    </style>"
        "</head>"
        "<body>"
        "    <h1>IoT Integrated Control System</h1>"
        "    <div class=\"status\">● SYSTEM ONLINE</div>"
        "    <div class=\"container\">"
        "        <div class=\"card\">"
        "            <h3>💡 LED CONTROL</h3>"
        "            <a href=\"/ledOn\"><button class=\"btn-on\">ON</button></a>"
        "            <a href=\"/ledOff\"><button class=\"btn-off\">OFF</button></a><br>"
        "            <a href=\"/max\"><button class=\"btn-on\">MAX</button></a>"
        "            <a href=\"/mid\"><button class=\"btn-start\">MID</button></a>"
        "            <a href=\"/min\"><button class=\"btn-off\">MIN</button></a>"
        "        </div>"
        "        <div class=\"card\">"
        "            <h3>☀️ LIGHT SENSOR</h3>"
        "            <p>%s</p>"
        "            <a href=\"/cdsCheck\"><button class=\"btn-scan\">SCAN</button></a>"
        "        </div>"
        "        <div class=\"card\">"
        "            <h3>🎵 BUZZER CONTROL</h3>"
        "            <a href=\"/buzzerOn\"><button class=\"btn-on\">START</button></a>"
        "            <a href=\"/buzzerOff\"><button class=\"btn-off\">STOP</button></a>"
        "        </div>"
        "        <div class=\"card\">"
        "            <h3>⏱️ COUNTDOWN TIMER</h3>"
        "            <form action=\"/countdown\" method=\"get\">"
        "                <input type=\"number\" name=\"num\" min=\"0\" max=\"9\" value=\"5\">"
        "                <button type=\"submit\" class=\"btn-start\">START</button>"
        "            </form>"
        "        </div>"
        "    </div>"
        "</body>"
        "</html>\r\n", cds_str);

    write(csock, html, strlen(html));
}

void sendOk(int csock) {
    // 각 항목 끝에 명확하게 \r\n을 넣고, 헤더의 끝을 알리는 빈 줄(\r\n\r\n)을 확실하게 구분해 줍니다.
    char header[] = "HTTP/1.1 200 OK\r\n"
                    "Server: Linux IoT Server\r\n"
                    "Content-Type: text/html; charset=UTF-8\r\n"
                    "\r\n"; // 👈 이 빈 줄이 있어야 브라우저 화면에 헤더 글자가 안 나타납니다!
                    
    write(csock, header, strlen(header));
}

void sendError(int csock) {
    char header[] = "HTTP/1.1 400 Bad Request\r\n\r\n";
    write(csock, header, strlen(header));
}