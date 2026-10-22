#include "framework.h"
#include "MiddleFingerSanjou.h"


using namespace glm;
using namespace std;

struct Mesh {
	GLuint vao = 0, vbo = 0, ebo = 0;
	GLsizei count = 0;
};

inline string filetobuf(const char* path) {
	fstream f(path);
	if (!f.is_open()) return "";
	return string((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
}

inline GLuint compileShader(GLenum type, const char* path) {
	string src = filetobuf(path);
	if (src.empty())
		cerr << "Failed to load shader: " << path << endl;
	const char* p = src.c_str();
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &p, nullptr);
	glCompileShader(s);

	GLint result;
	GLchar log[512];
	glGetShaderiv(s, GL_COMPILE_STATUS, &result);
	if (!result) {
		glGetShaderInfoLog(s, 512, nullptr, log);
		cerr << "Failed to compile shader: " << path << endl;
	}
	return s;
}

inline GLuint makeShaderProgram(const char* vsPath = "vertex.glsl", const char* fsPath = "fragment.glsl") {
	GLuint vs = compileShader(GL_VERTEX_SHADER, vsPath);
	GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsPath);

	GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);

	GLint result;
	GLchar log[512];
	glGetProgramiv(program, GL_LINK_STATUS, &result);
	if (!result) {
		glGetProgramInfoLog(program, 512, nullptr, log);
		cerr << "Failed to link shader program" << endl;
	}

	glDeleteShader(vs);
	glDeleteShader(fs);

	return program;
}

inline Mesh makeMesh(const vector<GLfloat>& verts, const vector <GLuint>& idx = {}) {
	Mesh m;
	glGenVertexArrays(1, &m.vao);
	glGenBuffers(1, &m.vbo);

	glBindVertexArray(m.vao);
	glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(GLfloat), verts.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);

	if (!idx.empty()) {
		glGenBuffers(1, &m.ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(GLuint), idx.data(), GL_STATIC_DRAW);
		m.count = (GLsizei)idx.size();
	}
	else {
		m.count = (GLsizei)(verts.size() / 6);
	}
	glBindVertexArray(0);
	return m;
}

inline void pushVertex(vector<GLfloat>& v, const glm::vec3& p, const glm::vec3& c) {
	v.insert(v.end(), { p.x, p.y, p.z, c.x, c.y, c.z });
}

inline void pushTri(vector<GLfloat>& v, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& color) {
	pushVertex(v, a, color);
	pushVertex(v, b, color);
	pushVertex(v, c, color);
}

inline void pushQuad(vector<GLfloat>& v, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& color) {
	pushTri(v, a, b, c, color);
	pushTri(v, a, c, d, color);
}

inline Mesh makeAxes() {
	vector <GLfloat> v;
	pushVertex(v, glm::vec3(-1, 0, 0), glm::vec3(1, 0, 0));
	pushVertex(v, glm::vec3(1, 0, 0), glm::vec3(1, 0, 0));
	pushVertex(v, glm::vec3(0, -1, 0), glm::vec3(0, 1, 0));
	pushVertex(v, glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));
	pushVertex(v, glm::vec3(0, 0, -1), glm::vec3(0, 0, 1));
	pushVertex(v, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1));
	return makeMesh(v);
}


inline GLFWwindow* initwindow(int w, int h, const char* title) {
	if (!glfwInit()) return nullptr;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	GLFWwindow* win = glfwCreateWindow(w, h, title, nullptr, nullptr);
	if (!win) {
		glfwTerminate();
		return nullptr;
	}
	glfwMakeContextCurrent(win);
	glViewport(0, 0, w, h);
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK) {
		glfwDestroyWindow(win);
		glfwTerminate();
		return nullptr;
	}
	glfwSetFramebufferSizeCallback(win, [](GLFWwindow*, int fw, int fh) {glViewport(0, 0, fw, fh); });
	return win;
}