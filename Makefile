
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

OBJECTS_SRV = $(SRCD_SRV:.cpp=.o)
OBJ_SRV = $(addprefix obj_srv/, $(OBJECTS_SRV))

OBJECTS_CLN = $(SRCD_CLN:.cpp=.o)
OBJ_CLN = $(addprefix obj_cln/, $(OBJECTS_CLN))


TARGET_SRV = bin/srv_udp

TARGET_CLN = bin/cln_udp


#all: $(SRC_SRV) $(HDR_SRV) $(TARGET_SRV) $(SRC_CLN) $(HDR_CLN) $(TARGET_CLN)
all: $(TARGET_SRV) $(TARGET_CLN)

mk_dir:
	mkdir bin

$(TARGET_SRV): mk_dir $(OBJ_SRV)
	@echo
	@echo Сервер: сборка ...
	$(CC) $(LDFLAGS) $(OBJ_SRV) -o $@

$(TARGET_CLN): mk_dir $(OBJ_CLN)
	@echo
	@echo Клиент: сборка ...
	$(CC) $(LDFLAGS) $(OBJ_CLN) -o $@



obj_srv/%.o: src_srv/%.cpp $(HDR_SRV) Makefile
	@echo
	@echo Сервер: компиляция $< ...
	$(CC) $(CFLAGS) $(DEFS) $< -o $@


obj_cln/%.o: src_cln/%.cpp $(HDR_CLN) Makefile
	@echo
	@echo Клиент: компиляция $< ...
	$(CC) $(CFLAGS) $(DEFS) $< -o $@

run: run_srv run_cln

run_srv:
	./$(TARGET_SRV)

run_cln:
	./$(TARGET_CLN)


clean:
	-rm -f obj_srv/*.o
