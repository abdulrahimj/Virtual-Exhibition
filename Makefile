# =============================================================
# Makefile for Virtual Exhibition Space
# Computer Graphics Final Year Project
# =============================================================

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -O2
LDFLAGS  = -lglew32 -lglfw3 -lopengl32 -lgdi32

SRC_DIR  = src
SRC      = $(SRC_DIR)/main.cpp
TARGET   = exhibition

# ----- Default target: compile everything -----
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

# ----- Clean up directory -----
clean:
	rm -f $(TARGET) $(TARGET).exe *.o core

# ----- Run the program -----
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run