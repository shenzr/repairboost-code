CC = g++ -std=c++11
JCC = gcc 
CLIBS = -lgf_complete
CFLAGS = -O3 -mmmx -msse -mpclmul -msse4.2 -DINTEL_SSE4 -mavx

SRC_DIR = src
UTIL_SRC_DIR = src/Util

JERASURE_C_FILES = $(wildcard $(UTIL_SRC_DIR)/*.c)

OBJ_DIR = obj
JERASURE_OBJ_FILES = $(addprefix $(OBJ_DIR)/J_, $(notdir $(JERASURE_C_FILES:.c=.o)))

O_FILES := $(JERASURE_OBJ_FILES)

all : directories createdata_rs createdata_lrc createdata_butterfly

directories : $(OBJ_DIR)

$(OBJ_DIR) : 
	mkdir -p $(OBJ_DIR)

createdata_rs : $(SRC_DIR)/createdata_rs.cc $(O_FILES)
	$(CC) $(CFLAGS) -o $@ $^ $(CLIBS)

createdata_lrc : $(SRC_DIR)/createdata_lrc.cc $(O_FILES)
	$(CC) $(CFLAGS) -o $@ $^ $(CLIBS)

createdata_butterfly : $(SRC_DIR)/createdata_butterfly.cc $(O_FILES)
	$(CC) $(CFLAGS) -o $@ $^ $(CLIBS)

$(OBJ_DIR)/J_%.o : $(UTIL_SRC_DIR)/%.c $(UTIL_SRC_DIR)/%.h
	$(JCC) $(CFLAGS) -o $@ -c $<

clean :
	rm -rf createdata_rs createdata_lrc createdata_butterfly obj/*

