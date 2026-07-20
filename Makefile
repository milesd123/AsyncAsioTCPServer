# cc = g++ -O3 -std=c++11 -Isrc/external/asio-1.36.0/include/ -fsanitize=address -fno-omit-frame-pointer -fsanitize=thread -g
cc = g++ -O3 -std=c++11 -Isrc/external/asio-1.36.0/include/ 

src/build/program: src/build/main.o src/build/session.o
	$(cc) $^ -o $@

src/build/main.o: src/code/main.cpp
	$(cc) -c $^ -o $@

src/build/session.o: src/code/Session.cpp
	$(cc) -c $^ -o $@

clean:
	rm -rf src/build/*