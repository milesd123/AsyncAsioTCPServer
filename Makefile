# cc = g++ -O3 -std=c++11 -Isrc/external/asio -fsanitize=address -fno-omit-frame-pointer -g
cc = $(CXX) -O3 -std=c++20 -pthread -Wall -Wextra -Wpedantic -isystem src/external/asio -isystem src/external/json/include
.DEFAULT_GOAL := ProxyServer

objects = build_folder/main.o build_folder/session.o build_folder/varint.o build_folder/mcpacketreader.o build_folder/protocol.o build_folder/server.o build_folder/serverconfig.o

-include $(objects:.o=.d)

ProxyServer: $(objects)
	$(cc) $^ -o $@

build_folder/main.o: src/code/main.cpp
	$(cc) -MMD -MP -c $< -o $@

build_folder/session.o: src/code/Session.cpp
	$(cc) -MMD -MP -c $< -o $@

build_folder/varint.o: src/code/VarInt.cpp
	$(cc) -MMD -MP -c $< -o $@

build_folder/mcpacketreader.o: src/code/MCPacketReader.cpp
	$(cc) -MMD -MP -c $< -o $@

build_folder/protocol.o: src/code/Protocol.cpp
	$(cc) -MMD -MP -c $< -o $@

build_folder/server.o: src/code/Server.cpp
	$(cc) -MMD -MP -c $< -o $@

build_folder/serverconfig.o: src/code/ServerConfig.cpp
	$(cc) -MMD -MP -c $< -o $@

$(objects): | build_folder

build_folder:
	mkdir -p $@

.PHONY: clean test
clean:
	rm -f $(objects) $(objects:.o=.d) ProxyServer build_folder/protocol_tests

test: ProxyServer build_folder/protocol_tests
	./build_folder/protocol_tests
	python3 tests/integration_tests.py ./ProxyServer

build_folder/protocol_tests: tests/protocol_tests.cpp build_folder/varint.o build_folder/protocol.o build_folder/serverconfig.o
	$(cc) $^ -o $@