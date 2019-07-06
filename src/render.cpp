#ifndef XLIB
#ifndef GLFW3
// prefer GLFW3 above XLIB when nothing is chosen
#define GLFW3 1
#endif
#endif

#include <GL/glew.h>
// glew.h should be loaded before GL/gl.h

#include <GL/gl.h>
#include <GL/glu.h>
#if XLIB > 0
#include <GL/glx.h>
#elif GLFW3 > 0
#include <GLFW/glfw3.h>
#endif
#include <assert.h>
#include <fstream>
#include <iostream>
#include <ostream>
#include <sstream>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#if XLIB > 0
Display *display;     // Connection to the X server
Window window;        // Main window
Atom wmDeleteMessage; // The expected client event on a close
GLXContext glc;       // OpenGL rendering context
#elif GLFW3 > 0
GLFWwindow *window; // Main window
#endif

const unsigned int kTextureSize = 256;       // number of pixels in each side of a texture
const unsigned int kBufferSize = 64 * 1024;  // use a 16k buffer
char *kBuffer = (char *)malloc(kBufferSize); // a buffer to use for file and network reading

unsigned int x_textures; // number of horizontal textures
unsigned int y_textures; // number of vertical textures
GLuint *textures;        // uploaded texture identifiers
GLsizei no_vertexes;     // number of vertexes in the vertexBuffer
GLuint vertexBuffer;     // buffer for the needed vertextes = 3d grid points
GLsizei no_indexes;      // number of indexes in the indexBuffer
GLuint indexBuffer;      // buffer with the indexes into the vertex to plot

// If a test is not true return the next error and exit the program.
void test(int test, const char *format, ...) {
  if (!test) {
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);
    fprintf(stderr, "\n");
    exit(1);
  }
}

GLuint CompileShader(const char *file_path, GLenum shader_type) {
  GLuint ShaderID = glCreateShader(shader_type);
  std::ifstream ShaderStream(file_path, std::ios::in);
  test(ShaderStream.is_open(), "Cannot open shader: '%s'", file_path);
  std::stringstream sstr;
  sstr << ShaderStream.rdbuf();
  std::string ShaderCode = sstr.str();
  ShaderStream.close();

  char const *SourcePointer = ShaderCode.c_str();
  glShaderSource(ShaderID, 1, &SourcePointer, NULL);
  glCompileShader(ShaderID);

  GLint Result = GL_FALSE;
  int InfoLogLength;
  glGetShaderiv(ShaderID, GL_COMPILE_STATUS, &Result);
  glGetShaderiv(ShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if (InfoLogLength > 0) {
    std::vector<char> ShaderErrorMessage(InfoLogLength + 1);
    glGetShaderInfoLog(ShaderID, InfoLogLength, NULL, &ShaderErrorMessage[0]);
    printf("%s\n", &ShaderErrorMessage[0]);
    exit(1);
  }
  return ShaderID;
}

void CleanupShader(GLuint ProgramID, GLuint ShaderID) {
  glDetachShader(ProgramID, ShaderID);
  glDeleteShader(ShaderID);
}

GLuint LinkShaders(const char *vertex_file_path, const char *fragment_file_path) {
  GLuint VertexShaderID = CompileShader(vertex_file_path, GL_VERTEX_SHADER);
  GLuint FragmentShaderID = CompileShader(fragment_file_path, GL_FRAGMENT_SHADER);

  GLuint ProgramID = glCreateProgram();
  GLint Result = GL_FALSE;
  int InfoLogLength;
  glAttachShader(ProgramID, VertexShaderID);
  glAttachShader(ProgramID, FragmentShaderID);
  glLinkProgram(ProgramID);
  glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
  glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if (InfoLogLength > 0) {
    std::vector<char> ProgramErrorMessage(InfoLogLength + 1);
    glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
    printf("%s\n", &ProgramErrorMessage[0]);
    exit(1);
  }

  CleanupShader(ProgramID, VertexShaderID);
  CleanupShader(ProgramID, FragmentShaderID);
  return ProgramID;
}

// Start a X window, give it a name and bind opengl to it
void init_window(unsigned int width, unsigned int height) {
  const char *title = "Robotic Eye";
#if XLIB > 0
  display = XOpenDisplay(NULL); // Connection to the screen from $(DISPLAY)
  test(display != NULL, "Cannot open X display");
  Window root = DefaultRootWindow(display); // the parent of all windows
  GLint attributeList[] = {
      GLX_RGBA,           // full colour with opaque RED/GREEN/BLUE/ALPHA
      GLX_DEPTH_SIZE, 24, // size of the depth buffer
      GLX_DOUBLEBUFFER,   // use double buffers to prevent flickers
      None                // end of the attribute list
  };
  XVisualInfo *vi = glXChooseVisual(display, 0, attributeList);
  test(vi != NULL, "No appropriate visual found, check glxinfo");

  XSetWindowAttributes swa;
  swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask;

  swa.colormap = XCreateColormap(display, root, vi->visual, AllocNone);
  window = XCreateWindow(display, root, 0, 0, width, height, 0, vi->depth, InputOutput, vi->visual,
                         CWEventMask | CWColormap, &swa);
  XMapWindow(display, window);
  XStoreName(display, window, title);
  wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display, window, &wmDeleteMessage, 1);

  glc = glXCreateContext(display, vi, NULL, GL_TRUE);
  test(glc != NULL, "Cannot create gl context");
  glXMakeCurrent(display, window, glc);
#elif GLFW3 > 0
  test(glfwInit(), "Failed to init glfw");
  glfwWindowHint(GLFW_SAMPLES, 4);               // 4x antialiasing
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // We want OpenGL 3.3
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // To make MacOS happy; should not be needed
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // We don't want the old OpenGL

  window = glfwCreateWindow(width, height, title, NULL, NULL);
  test(window != NULL, "Failed to open window.");
  glfwMakeContextCurrent(window); // Initialize GLEW
#endif

  glewExperimental = true; // Needed for core profile
  test(glewInit() == GLEW_OK, "Failed to initialize GLEW");
  GLuint VertexArrayID;
  glGenVertexArrays(1, &VertexArrayID);
  glBindVertexArray(VertexArrayID);

  // glEnable(GL_DEPTH_TEST);
  glClearColor(0.3, 0.3, 0.3, 1.0);
}

// Stop opengl and the main window
void destroy_window() {
#if XLIB > 0
  glXMakeCurrent(display, None, NULL);
  glXDestroyContext(display, glc);
  XDestroyWindow(display, window);
  XCloseDisplay(display);
#elif GLFW3 > 0
  glfwTerminate();
#endif
}

// Read a bmp file with an image into several textures
void bmp(const char *path) {
  unsigned int data = 0; // temporary space for header data
  unsigned int offset;   // offset of the real image data from the start of the file
  unsigned int width;    // width of the image in pixels
  unsigned int height;   // height of the image in pixels
  FILE *file = fopen(path, "rb");
  test(file != NULL, "Image %s could not be opened", path);
  test(fread(&data, 1, 2, file) == 2, "Not a correct bmp file");
  test(data == 0x4d42, "Not a correct bmp file");
  test(fread(&data, 1, 4, file) == 4, "Not a correct bmp file"); // file size
  // printf("size %d\n", data);
  test(fread(&data, 1, 4, file) == 4, "Not a correct bmp file");   // reserved
  test(fread(&offset, 1, 4, file) == 4, "Not a correct bmp file"); // data offset
  // printf("offset %d\n", offset);
  test(fread(&data, 1, 4, file) == 4, "Not a correct bmp file");  // header size
  test(fread(&width, 1, 4, file) == 4, "Not a correct bmp file"); // width
  // printf("width %d\n", width);
  test(fread(&height, 1, 4, file) == 4, "Not a correct bmp file"); // height
  // printf("heigth %d\n", height);
  test(fread(&data, 1, 4, file) == 4, "Not a correct bmp file");
  test(data == 0x200001, "Expect a 32 bits per pexil bmp file");
  fseek(file, offset, SEEK_SET);
  unsigned int imageSize = width * height * 3; // expect 3 bytes per pixel
  // printf("size %d\n", imageSize);
  x_textures = (width / kTextureSize) + (width % kTextureSize > 0 ? 1 : 0);
  y_textures = (height / kTextureSize) + (height % kTextureSize > 0 ? 1 : 0);
  unsigned int no_textures = x_textures * y_textures; // number of textures needed
  textures = new GLuint[no_textures];

  char **tex_data = new char *[no_textures]; // temporary data for the textures till we upload them to the cpu
  for (unsigned int t = 0; t < no_textures; t++) {
    tex_data[t] = (char *)malloc(kTextureSize * kTextureSize * 3);
    memset(tex_data[t], 0, kTextureSize * kTextureSize * 3); // initialize with zeros
  }
  no_vertexes = (x_textures + 1) * (y_textures + 1);
  GLfloat *vertexes = new GLfloat[no_vertexes * 3]; // vertextes to render the textures
  const GLfloat sw = 2.0f / x_textures;
  const GLfloat sh = 2.0f / y_textures;
  for (unsigned int x = 0; x <= x_textures; x++) {
    for (unsigned int y = 0; y <= y_textures; y++) {
      int v = x + y * (x_textures + 1);
      vertexes[v * 3] = (GLfloat)x * sw - 1.0f;
      vertexes[v * 3 + 1] = (GLfloat)y * sh - 1.0f;
      vertexes[v * 3 + 2] = 0.0f;
    }
  }

  /* dump all vertexes
  printf("x:%i * y:%i = %i vertexes:%i size:%i\n", x_textures, y_textures, no_textures, no_vertexes, no_vertexes * 3);
  printf("sw:%f sh:%f\n", sw, sh);
  for (int v = 0; v < no_vertexes; v++)
    printf("v%i %f, %f, %f\n", v, vertexes[v * 3], vertexes[v * 3 + 1], vertexes[v * 3 + 2]);
  printf("size %li\n", no_vertexes * 3 * sizeof(*vertexes));
   */
  glGenBuffers(1, &vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, no_vertexes * 3 * sizeof(*vertexes), vertexes, GL_STATIC_DRAW);

  no_indexes = 2 * (x_textures + 2) * y_textures - 2;
  GLubyte *indexes = new GLubyte[no_indexes];
  int i = 0;
  for (unsigned int y = 0; y < y_textures; y++) {
    for (unsigned int x = 0; x <= x_textures; x++) {
      int to = x + y * (x_textures + 1);
      indexes[i++] = to;
      if (y > 0 && x == 0)
        indexes[i++] = to;
      indexes[i++] = x + (y + 1) * (x_textures + 1);
    }
    if (y + 1 < y_textures) {
      int to = indexes[i - 1];
      indexes[i++] = to;
    }
  }
  /* dump all indexes
  printf("indexes:%i\n", no_indexes);
  for (i = 0; i < no_indexes; i++) {
    int index = indexes[i];
    printf("i:%i index:%i (%f, %f)\n", i, index, vertexes[index * 3], vertexes[index * 3 + 1]);
  }
   */
  glGenBuffers(1, &indexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, no_indexes, indexes, GL_STATIC_DRAW);

  unsigned int remainder = imageSize; // data left to read in the data file
  unsigned int d = 0;                 // data position within the original image
  unsigned int pixel = 0;             // current pixel within the original image
  unsigned int line = 0;              // current line within the original image
  unsigned int tex = 0;               // texture number to write to
  unsigned int tex_x = 0;             // pixel position on the texture
  unsigned int tex_y = 0;             // line position on the texture
  unsigned int tex_pos = 0;           // address on the texture;
  while (remainder > 0) {
    unsigned int read = kBufferSize > remainder ? remainder : kBufferSize;
    test(fread(kBuffer, 1, read, file) == read, "File read error");
    for (unsigned int b = 0; b < read; b++) {
      tex_data[tex][tex_pos] = kBuffer[pixel * 3];
      d++;
      if (d % 3 == 0) {
        pixel++;
        if (pixel >= width) {
          pixel -= width;
          line++;
        }
        tex = (pixel / kTextureSize) + x_textures * (line / kTextureSize);
        tex_x = pixel % kTextureSize;
        tex_y = line % kTextureSize;
        tex_pos = (tex_x + tex_y * kTextureSize) * 3;
      }
    }
    remainder -= read;
  }
  // We cannot compress the textures because we need the maximum upload speed
  for (unsigned int t = 0; t < no_textures; t++) {
    glGenTextures(no_textures, textures);
    glBindTexture(GL_TEXTURE_2D, textures[t]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, kTextureSize, kTextureSize, 0, GL_BGR, GL_UNSIGNED_BYTE, tex_data[t]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  }
  for (unsigned int t = 0; t < no_textures; t++)
    free(tex_data[t]);
  delete[] tex_data;
}

// Render a simple rectangle with a given texture
void Redraw(GLuint ProgramID) {
#if XLIB > 0
  XWindowAttributes gwa;

  XGetWindowAttributes(display, window, &gwa);
  glViewport(0, 0, gwa.width, gwa.height);
#endif

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_TEXTURE_2D);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  glUseProgram(ProgramID);

  const GLuint index = 0;
  glEnableVertexAttribArray(index);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  glVertexAttribPointer(index,    // index
                        3,        // size: components per generic vertex attribute
                        GL_FLOAT, // type
                        GL_FALSE, // normalized?
                        0,        // stride
                        0         // array buffer offset
  );

  // Draw the triangles
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
  glDrawElements(GL_TRIANGLE_STRIP, // efficient triangle strip (also GL_LINE_STRIP)
                 no_indexes,        // number of indexes
                 GL_UNSIGNED_BYTE,  // type of index
                 0                  // index buffer offset
  );
  glDisableVertexAttribArray(index);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

#if XLIB > 0
  glXSwapBuffers(display, window);
#elif GLFW3 > 0
  glfwSwapBuffers(window);
#endif
}

// Create data for a texture, beware that the size should be a
unsigned char *initial_texture() {
  unsigned int width = 256;
  unsigned int height = 256;
  unsigned int imageSize = width * height * 3;
  unsigned char *data = new unsigned char[imageSize];

  for (unsigned int x = 0; x < width; x++) {
    for (unsigned int y = 0; y < height; y++) {
      unsigned int p = (x + y * width) * 3;
      data[p] = x;
      data[p + 1] = y;
      data[p + 2] = 255 - (x + y) / 2;
    }
  }
  return data;
}

// The main loop of the program. Read events and handle them.
int main() {
  // unsigned char *data = initial_texture();
  init_window(640, 480);
  const GLuint ProgramID = LinkShaders("../shaders/simple.vertex", "../shaders/simple.fragment");

  bmp("../test.bmp");

#if XLIB > 0
  XEvent xev;
  while (true) {
    XNextEvent(display, &xev);
    if (xev.type == Expose) {
      Redraw(ProgramID);
    } else if (xev.type == ClientMessage) {
      Atom event = (Atom)xev.xclient.data.l[0];
      if (event == wmDeleteMessage)
        break;
      printf("client event %li\n", event);
    } else if (xev.type == KeyPress) {
      XKeyEvent *e = (XKeyEvent *)&xev;
      if (e->keycode == 24) // Q key
        break;
      printf("Key %i\n", e->keycode);
    } else if (xev.type == ButtonPress) {
      XButtonEvent *e = (XButtonEvent *)&xev;
      printf("Button %i\n", e->button);
    }
  }
#elif GLFW3 > 0
  glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
  while (true) {
    Redraw(ProgramID);
    glfwPollEvents();
    if (glfwWindowShouldClose(window) != 0 || glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
      break;
  }
#endif
  destroy_window();
  return 0;
}
