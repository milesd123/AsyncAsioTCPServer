# cc = g++ -O3 -std=c++11 -Isrc/external/asio -fsanitize=address -fno-omit-frame-pointer -g
cc = g++ -O3 -std=c++20 -Isrc/external/asio -Isrc/external/json/include

ProxyServer: build_folder/main.o build_folder/session.o build_folder/varint.o build_folder/mcpacketreader.o
	$(cc) $^ -o $@

build_folder/main.o: src/code/main.cpp
	$(cc) -c $^ -o $@

build_folder/session.o: src/code/Session.cpp
	$(cc) -c $^ -o $@

build_folder/varint.o: src/code/VarInt.cpp
	$(cc) -c $^ -o $@

build_folder/mcpacketreader.o: src/code/MCPacketReader.cpp
	$(cc) -c $^ -o $@

clean:
	rm -rf build_folder/*