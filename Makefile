manager-test: manager.cpp manager.h
	g++ -o manager-test manager.cpp -lpthread -lrt
