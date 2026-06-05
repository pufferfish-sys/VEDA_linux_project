# [컴파일러] 라즈베리파이 64비트(ARMv8) 아키텍처용 크로스 컴파일러 지정
CC = aarch64-linux-gnu-gcc
# 우분투 PC 로컬 빌드용 기본 컴파일러 지정
HOST_CC = gcc

# 구조 경로 매핑
SRC_DIR    = src
MOD_DIR    = modules
INC_DIR    = include
BUILD_DIR  = build
TARGET_DIR = target
LIB_DIR    = $(TARGET_DIR)/project_library

# [파일 정의] 
# 하드웨어 상위 로직과 개별 드라이버 소스들을 새 폴더 경로로 지정
MAIN_SRC    = $(SRC_DIR)/main_server.c $(SRC_DIR)/hardware_control.c
MAIN_TARGET = $(TARGET_DIR)/main_server

# 클라이언트 소스와 최종 타겟을 '현재 폴더(./client)'로 설정
CLIENT_SRC    = $(SRC_DIR)/client.c
CLIENT_TARGET = client

# 1) LED 라이브러리 경로 설정
LED_SRC    = $(MOD_DIR)/led_control.c
LED_TARGET = $(LIB_DIR)/libled.so

# 2) 조도센서 라이브러리 경로 설정
CDS_SRC    = $(MOD_DIR)/cds_control.c
CDS_TARGET = $(LIB_DIR)/libcds.so

# 3) 부저(스피커) 라이브러리 경로 설정
BUZZER_SRC    = $(MOD_DIR)/buzzer_control.c
BUZZER_TARGET = $(LIB_DIR)/libbuzzer.so

# 4) 7세그먼트(FND) 라이브러리 경로 설정
FND_SRC    = $(MOD_DIR)/fnd_control.c
FND_TARGET = $(LIB_DIR)/libfnd.so

RP_USER = puffer
RP_IP   = 172.20.27.216
RP_DIR  = /home/puffer/kjw_project

CFLAGS_MAIN = -rdynamic -Wl,-rpath,./project_library -I$(INC_DIR)

# [라이브러리 옵션]
CFLAGS_LIB  = -fPIC -shared -I$(INC_DIR)

# [링커 라이브러리] 
LIBS_MAIN = -lpthread -ldl -lwiringPi -lcrypt
LIBS_LIB  = -lwiringPi -lcrypt


# -------------------------------------------------------
# 빌드 규칙 (Rules)
# -------------------------------------------------------

# 'make' 입력 시 기본 실행: 우분투용 클라이언트(CLIENT_TARGET)도 함께 빌드 대상에 포함
all: prepare $(LED_TARGET) $(CDS_TARGET) $(BUZZER_TARGET) $(FND_TARGET) $(MAIN_TARGET) $(CLIENT_TARGET)

# 컴파일 전 빌드 중간체(build) 및 결과물(target) 폴더를 준비하는 규칙
prepare:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(LIB_DIR)

# 1) LED 라이브러리 컴파일 규칙
$(LED_TARGET): $(LED_SRC)
	$(CC) $(CFLAGS_LIB) $(LED_SRC) -o $(LED_TARGET) $(LIBS_LIB)

# 2) 조도센서 라이브러리 컴파일 규칙
$(CDS_TARGET): $(CDS_SRC)
	$(CC) $(CFLAGS_LIB) $(CDS_SRC) -o $(CDS_TARGET) $(LIBS_LIB)

# 3) 부저 라이브러리 컴파일 규칙
$(BUZZER_TARGET): $(BUZZER_SRC)
	$(CC) $(CFLAGS_LIB) $(BUZZER_SRC) -o $(BUZZER_TARGET) $(LIBS_LIB)

# 4) 7세그먼트 라이브러리 컴파일 규칙
$(FND_TARGET): $(FND_SRC)
	$(CC) $(CFLAGS_LIB) $(FND_SRC) -o $(FND_TARGET) $(LIBS_LIB)

# 메인 프로그램(main_server) 컴파일 규칙
$(MAIN_TARGET): $(MAIN_SRC) $(INC_DIR)/hardware_control.h
	$(CC) $(CFLAGS_MAIN) $(MAIN_SRC) -o $(MAIN_TARGET) $(LIBS_MAIN)
  
# 5) 클라이언트 빌드 규칙: HOST_CC(gcc)를 사용하여 우분투 현재 폴더에 생성
$(CLIENT_TARGET): $(CLIENT_SRC)
	$(HOST_CC) -Wall -O2 $(CLIENT_SRC) -o $(CLIENT_TARGET)

# 'make deploy' 입력 시: 라즈베리파이 홈의 kjw_project 내부로 깔끔하게 원격 전송
# 클라이언트(client)는 전송 목록에서 제외되어 우분투에 그대로 남아있습니다.
deploy: all
	LC_ALL=C ssh $(RP_USER)@$(RP_IP) "mkdir -p $(RP_DIR)/project_library"
	LC_ALL=C scp $(MAIN_TARGET) $(RP_USER)@$(RP_IP):$(RP_DIR)
	LC_ALL=C scp $(LIB_DIR)/*.so $(RP_USER)@$(RP_IP):$(RP_DIR)/project_library

  
# 'make clean' 입력 시 생성된 빌드 흔적, target 폴더, 그리고 현재 폴더의 client까지 일괄 청소
clean:
	rm -rf $(BUILD_DIR) $(TARGET_DIR) $(CLIENT_TARGET)