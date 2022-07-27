
#-Wshadow

CC = g++
STD = -std=gnu++17
CFLAGS = -O3 $(STD) -c -Wall -Wextra -pedantic
LDFLAGS = -O3 $(STD) -lm -lrt -pthread

HDR_SRV = $(wildcard src_srv/*.h)
HDR_SRV += $(wildcard lib_msg/*.h)
SRC_SRV = $(wildcard src_srv/*.cpp)
SRCD_SRV = $(notdir $(SRC_SRV))

HDR_CLN = $(wildcard src_cln/*.h)
HDR_CLN += $(wildcard lib_msg/*.h)
SRC_CLN = $(wildcard src_cln/*.cpp)
SRCD_CLN = $(notdir $(SRC_CLN))

HDR_SHR = $(wildcard src_shr/*.h)
HDR_SHR += $(wildcard lib_msg/*.h)
SRC_SHR = $(wildcard src_shr/*.cpp)
SRCD_SHR = $(notdir $(SRC_SHR))

OBJECTS_SRV = $(SRCD_SRV:.cpp=.o)
OBJ_SRV = $(addprefix obj_srv/, $(OBJECTS_SRV))

OBJECTS_CLN = $(SRCD_CLN:.cpp=.o)
OBJ_CLN = $(addprefix obj_cln/, $(OBJECTS_CLN))

OBJECTS_SHR = $(SRCD_SHR:.cpp=.o)
OBJ_SHR = $(addprefix obj_shr/, $(OBJECTS_SHR))


TARGET_SRV = bin/srv_udp

TARGET_CLN = bin/cln_udp


all: $(TARGET_SRV) $(TARGET_CLN)


$(TARGET_SRV): $(OBJ_SHR) $(OBJ_SRV)
	@echo
	@echo Сервер: сборка ...
	@if [ ! -d bin ]; then mkdir -p bin; fi
	$(CC) $(LDFLAGS) $(OBJ_SRV) $(OBJ_SHR) -o $@

$(TARGET_CLN): $(OBJ_SHR) $(OBJ_CLN)
	@echo
	@echo Клиент: сборка ...
	@if [ ! -d bin ]; then mkdir -p bin; fi
	$(CC) $(LDFLAGS) $(OBJ_CLN) $(OBJ_SHR) -o $@



obj_srv/%.o: src_srv/%.cpp $(HDR_SRV) Makefile
	@echo
	@echo Сервер: компиляция $< ...
	@if [ ! -d obj_srv ]; then mkdir -p obj_srv; fi
	$(CC) $(CFLAGS) $(DEFS) $< -o $@


obj_cln/%.o: src_cln/%.cpp $(HDR_CLN) Makefile
	@echo
	@echo Клиент: компиляция $< ...
	@if [ ! -d obj_cln ]; then mkdir -p obj_cln; fi
	$(CC) $(CFLAGS) $(DEFS) $< -o $@

obj_shr/%.o: src_shr/%.cpp $(HDR_SHR) Makefile
	@echo
	@echo Display: компиляция $< ...
	@if [ ! -d obj_shr ]; then mkdir -p obj_shr; fi
	$(CC) $(CFLAGS) $(DEFS) $< -o $@

run: run_srv run_cln

run_srv:
	./$(TARGET_SRV)

run_cln:
	./$(TARGET_CLN)


clean:
	-rm -f obj_srv/*.o
