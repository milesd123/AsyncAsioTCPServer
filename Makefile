# cc = g++ -O3 -std=c++11 -Isrc/external/asio -fsanitize=address -fno-omit-frame-pointer -g
cc = g++ -O3 -std=c++20 -Isrc/external/asio -Isrc/external/json

ProxyServer: src/build/main.o src/build/session.o src/build/varint.o src/build/mcpacketreader.o
	$(cc) $^ -o $@

src/build/main.o: src/code/main.cpp
	$(cc) -c $^ -o $@

src/build/session.o: src/code/Session.cpp
	$(cc) -c $^ -o $@

src/build/varint.o: src/code/VarInt.cpp
	$(cc) -c $^ -o $@

src/build/mcpacketreader.o: src/code/MCPacketReader.cpp
	$(cc) -c $^ -o $@

clean:
	rm -rf src/build/*