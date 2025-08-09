CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall
LDLIBS = -lreadline

# If system-installed json:
# LDLIBS += -lnlohmann-json

all: ai_shell

ai_shell: ai_shell.cpp
	$(CXX) $(CXXFLAGS) ai_shell.cpp -o ai_shell $(LDLIBS)
