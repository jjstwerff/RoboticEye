WARN = -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic
XLIB = -lX11 -D XLIB
GLFW3 = -lglfw -D GLFW3
LIBS = -lGL -lGLU -lGLEW $(GLFW3)

all : test render

render : render.cpp
	c++ render.cpp $(WARN) $(LIBS) -o render

clean :
	rm render -f

test :
	clang-format -i render.cpp
	clang-format -i mathematics.h
	clang-tidy render.cpp -p ../build

