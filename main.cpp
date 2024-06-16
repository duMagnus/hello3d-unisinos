#include <iostream>
#include <string>
#include <assert.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>

using namespace std;

// STB_IMAGE
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// GLAD
#include <glad/glad.h>

// GLFW
#include <GLFW/glfw3.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

std::vector<glm::vec3> pontos;
size_t ponto_atual = 0;
float tempo_percorrido = 0.0f;
const float duracao_ponto = 2.0f; // duração de cada translação entre pontos em segundos

void carregarPontos(const std::string& caminho) {
	std::ifstream arquivo(caminho);
	if (!arquivo.is_open()) {
		std::cerr << "Erro ao abrir o arquivo de pontos: " << caminho << std::endl;
		return;
	}

	std::string linha;
	while (std::getline(arquivo, linha)) {
		std::istringstream iss(linha);
		float x, y, z;
		if (iss >> x >> y >> z) {
			pontos.emplace_back(x, y, z);
		}
	}
	arquivo.close();
}

glm::vec3 interpolar(const glm::vec3& ponto_inicial, const glm::vec3& ponto_final, float t) {
	return ponto_inicial + t * (ponto_final - ponto_inicial);
}

struct Material {
	glm::vec3 Ka; // Ambient reflectivity
	glm::vec3 Kd; // Diffuse reflectivity
	glm::vec3 Ks; // Specular reflectivity
	glm::vec3 Ke; // Emissive coefficient
	float Ns;     // Specular exponent
	float Ni;     // Optical density
	float d;      // Transparency
	int illum;    // Illumination model
	std::string map_Kd; // Diffuse texture map
};

// Protótipo da função de callback de teclado
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void process_input(GLFWwindow *window);

// Protótipos das funções
int setupShader();
int setupGeometry();
int loadTexture(string path);
int loadSimpleOBJ(string filepath, int& nVerts, glm::vec3 color);
std::unordered_map<std::string, Material> loadMTL(const std::string& filename);

// Dimensões da janela (pode ser alterado em tempo de execução)
const GLuint WIDTH = 1000, HEIGHT = 1000;

const GLchar* vertexShaderSource = "#version 450 core\n"
"layout (location = 0) in vec3 position;\n"
"layout (location = 1) in vec3 normal;\n"
"layout (location = 2) in vec2 texCoord;\n"
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"out vec3 FragPos;\n"
"out vec3 Normal;\n"
"out vec2 TexCoord;\n"
"void main()\n"
"{\n"
"    FragPos = vec3(model * vec4(position, 1.0));\n"
"    Normal = mat3(transpose(inverse(model))) * normal;\n"
"    TexCoord = texCoord;\n"
"    gl_Position = projection * view * vec4(FragPos, 1.0);\n"
"}\0";

const GLchar* fragmentShaderSource = "#version 450 core\n"
"in vec3 FragPos;\n"
"in vec3 Normal;\n"
"in vec2 TexCoord;\n"
"out vec4 color;\n"
"uniform vec3 lightPos;\n"
"uniform vec3 viewPos;\n"
"uniform vec3 lightColor;\n"
"uniform sampler2D ourTexture;\n"
"uniform vec3 Ka;\n"
"uniform vec3 Kd;\n"
"uniform vec3 Ks;\n"
"uniform float Ns;\n"
"void main()\n"
"{\n"
"    // Ambient\n"
"    vec3 ambient = Ka * lightColor;\n"
"\n"
"    // Diffuse\n"
"    vec3 norm = normalize(Normal);\n"
"    vec3 lightDir = normalize(lightPos - FragPos);\n"
"    float diff = max(dot(norm, lightDir), 0.0);\n"
"    vec3 diffuse = Kd * diff * lightColor;\n"
"\n"
"    // Specular\n"
"    vec3 viewDir = normalize(viewPos - FragPos);\n"
"    vec3 reflectDir = reflect(-lightDir, norm);\n"
"    float spec = pow(max(dot(viewDir, reflectDir), 0.0), Ns);\n"
"    vec3 specular = Ks * spec * lightColor;\n"
"\n"
"    vec3 result = ambient + diffuse + specular;\n"
"    color = texture(ourTexture, TexCoord) * vec4(result, 1.0);\n"
"}\0";

bool rotateX = false, rotateY = false, rotateZ = false;

glm::vec3 translation = glm::vec3(0.0f, 0.0f, 0.0f);
float scale = 1.0f;

glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

bool firstMouse = true;
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = WIDTH / 2.0;
float lastY = HEIGHT / 2.0;
float fov = 45.0f;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Função MAIN
int main()
{

	// Inicialização da GLFW
	glfwInit();

	//Muita atenção aqui: alguns ambientes não aceitam essas configurações
	//Você deve adaptar para a versão do OpenGL suportada por sua placa
	//Sugestão: comente essas linhas de código para desobrir a versão e
	//depois atualize (por exemplo: 4.5 com 4 e 5)
	//glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	//glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	//Essencial para computadores da Apple
//#ifdef __APPLE__
//	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
//#endif

	// Criação da janela GLFW
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Ola 3D -- Eduardo!", nullptr, nullptr);
	glfwMakeContextCurrent(window);

	// Fazendo o registro da função de callback para a janela GLFW
	glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// GLAD: carrega todos os ponteiros d funções da OpenGL
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;

	}

	// Obtendo as informações de versão
	const GLubyte* renderer = glGetString(GL_RENDERER); /* get renderer string */
	const GLubyte* version = glGetString(GL_VERSION); /* version as a string */
	cout << "Renderer: " << renderer << endl;
	cout << "OpenGL version supported " << version << endl;

	// Definindo as dimensões da viewport com as mesmas dimensões da janela da aplicação
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	glViewport(0, 0, width, height);

	carregarPontos("../pontos.txt");
	if (pontos.empty()) {
		std::cerr << "Nenhum ponto foi carregado. Encerrando a aplicação." << std::endl;
		glfwTerminate();
		return -1;
	}

	// Compilando e buildando o programa de shader
	GLuint shaderID = setupShader();
    glUseProgram(shaderID);

    GLuint texID = loadTexture("../Cube.png");
    int nVerts;
    GLuint VAO = loadSimpleOBJ("../cube.obj", nVerts, glm::vec3(0,0,0));

    glm::vec3 lightPos(1.2f, 1.0f, 2.0f);
    glm::vec3 lightColor(1.0f, 1.0f, 1.0f);
    glm::vec3 objectColor(1.0f, 1.0f, 1.0f);

    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    GLint viewLoc = glGetUniformLocation(shaderID, "view");
    GLint projLoc = glGetUniformLocation(shaderID, "projection");
    GLint lightPosLoc = glGetUniformLocation(shaderID, "lightPos");
    GLint viewPosLoc = glGetUniformLocation(shaderID, "viewPos");
    GLint lightColorLoc = glGetUniformLocation(shaderID, "lightColor");
    GLint objectColorLoc = glGetUniformLocation(shaderID, "objectColor");

    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    glm::mat4 projection = glm::perspective(glm::radians(fov), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);

    glUniform3fv(lightPosLoc, 1, glm::value_ptr(lightPos));
    glUniform3fv(viewPosLoc, 1, glm::value_ptr(cameraPos));
    glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));
    glUniform3fv(objectColorLoc, 1, glm::value_ptr(objectColor));

	std::unordered_map<std::string, Material> materials = loadMTL("../cube.mtl");
	Material material = materials["Material"];

	glUniform3f(glGetUniformLocation(shaderID, "Ka"), material.Ka.r, material.Ka.g, material.Ka.b);
	glUniform3f(glGetUniformLocation(shaderID, "Kd"), material.Kd.r, material.Kd.g, material.Kd.b);
	glUniform3f(glGetUniformLocation(shaderID, "Ks"), material.Ks.r, material.Ks.g, material.Ks.b);
	glUniform1f(glGetUniformLocation(shaderID, "Ns"), material.Ns);

    // Definindo a matriz de projeção para a janela
    projection = glm::perspective(glm::radians(fov), (GLfloat)WIDTH / (GLfloat)HEIGHT, 0.1f, 100.0f);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    glEnable(GL_DEPTH_TEST);

    while (!glfwWindowShouldClose(window))
    {
        // Calcula o delta time do frame atual
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Checa se houveram eventos de input (ex: teclado, mouse)
        glfwPollEvents();
        process_input(window);

        // Limpa o buffer de cor
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindVertexArray(VAO);
        glBindTexture(GL_TEXTURE_2D, texID);

        // Atualiza a matriz de visualização (view) com base nas entradas do teclado e do mouse
        view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    	tempo_percorrido += deltaTime;
    	if (tempo_percorrido >= duracao_ponto) {
    		tempo_percorrido = 0.0f;
    		ponto_atual = (ponto_atual + 1) % pontos.size();
    	}

    	size_t proximo_ponto = (ponto_atual + 1) % pontos.size();
    	float t = tempo_percorrido / duracao_ponto;
    	translation = interpolar(pontos[ponto_atual], pontos[proximo_ponto], t);

        // Atualiza a matriz de modelo (model) com base nas entradas do teclado
        model = glm::translate(glm::mat4(1.0f), translation);
        model = glm::scale(model, glm::vec3(scale, scale, scale));
        if (rotateX) model = glm::rotate(model, (GLfloat)glfwGetTime(), glm::vec3(1.0f, 0.0f, 0.0f));
        if (rotateY) model = glm::rotate(model, (GLfloat)glfwGetTime(), glm::vec3(0.0f, 1.0f, 0.0f));
        if (rotateZ) model = glm::rotate(model, (GLfloat)glfwGetTime(), glm::vec3(0.0f, 0.0f, 1.0f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        // Draw the object
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texID);
        glUniform1i(glGetUniformLocation(shaderID, "ourTexture"), 0);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, nVerts);

        // Swap buffers
        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
	    float velocity = 20.0f;
        switch (key)
        {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GL_TRUE);
            break;
        case GLFW_KEY_W:
            cameraPos += cameraFront * deltaTime * velocity;
            break;
        case GLFW_KEY_S:
            cameraPos -= cameraFront * deltaTime * velocity;
            break;
        case GLFW_KEY_A:
            cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * deltaTime * velocity;
            break;
        case GLFW_KEY_D:
            cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * deltaTime * velocity;
            break;
        case GLFW_KEY_Q:
            translation.x += 0.1f;
            break;
        case GLFW_KEY_E:
            translation.x -= 0.1f;
            break;
        case GLFW_KEY_R:
            translation.y += 0.1f;
            break;
        case GLFW_KEY_F:
            translation.y -= 0.1f;
            break;
        case GLFW_KEY_T:
            translation.z += 0.1f;
            break;
        case GLFW_KEY_G:
            translation.z -= 0.1f;
            break;
        case GLFW_KEY_Z:
            scale += 0.1f;
            break;
        case GLFW_KEY_X:
            scale -= 0.1f;
            break;
        case GLFW_KEY_1:
            rotateX = !rotateX;
            break;
        case GLFW_KEY_2:
            rotateY = !rotateY;
            break;
        case GLFW_KEY_3:
            rotateZ = !rotateZ;
            break;
        default:
            break;
        }
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);

	glm::vec3 right = glm::normalize(glm::cross(cameraFront,
	glm::vec3(0.0,1.0,0.0)));
	cameraUp = glm::normalize(glm::cross(right, cameraFront));
}

void process_input(GLFWwindow *window)
{
    // Checa se houveram eventos de input (ex: teclado, mouse)
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

//Esta função está basntante hardcoded - objetivo é compilar e "buildar" um programa de
// shader simples e único neste exemplo de código
// O código fonte do vertex e fragment shader está nos arrays vertexShaderSource e
// fragmentShader source no iniçio deste arquivo
// A função retorna o identificador do programa de shader
int setupShader()
{
	// Vertex shader
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);
	// Checando erros de compilação (exibição via log no terminal)
	GLint success;
	GLchar infoLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	}
	// Fragment shader
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);
	// Checando erros de compilação (exibição via log no terminal)
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
	}
	// Linkando os shaders e criando o identificador do programa de shader
	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);
	// Checando por erros de linkagem
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
	}
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return shaderProgram;
}

// Esta função está bastante harcoded - objetivo é criar os buffers que armazenam a
// geometria de um triângulo
// Apenas atributo coordenada nos vértices
// 1 VBO com as coordenadas, VAO com apenas 1 ponteiro para atributo
// A função retorna o identificador do VAO
int setupGeometry()
{
	// Aqui setamos as coordenadas x, y e z do triângulo e as armazenamos de forma
	// sequencial, já visando mandar para o VBO (Vertex Buffer Objects)
	// Cada atributo do vértice (coordenada, cores, coordenadas de textura, normal, etc)
	// Pode ser arazenado em um VBO único ou em VBOs separados
	GLfloat vertices[] = {
		// Face frontal (vermelho)
		// Triângulo 1
		-0.5, -0.5,  0.5, 1.0, 0.0, 0.0,
		 0.5, -0.5,  0.5, 1.0, 0.0, 0.0,
		 0.5,  0.5,  0.5, 1.0, 0.0, 0.0,
		// Triângulo 2
		-0.5, -0.5,  0.5, 1.0, 0.0, 0.0,
		 0.5,  0.5,  0.5, 1.0, 0.0, 0.0,
		-0.5,  0.5,  0.5, 1.0, 0.0, 0.0,

		// Face traseira (verde)
		// Triângulo 3
		-0.5, -0.5, -0.5, 0.0, 1.0, 0.0,
		 0.5, -0.5, -0.5, 0.0, 1.0, 0.0,
		 0.5,  0.5, -0.5, 0.0, 1.0, 0.0,
		// Triângulo 4
		-0.5, -0.5, -0.5, 0.0, 1.0, 0.0,
		 0.5,  0.5, -0.5, 0.0, 1.0, 0.0,
		-0.5,  0.5, -0.5, 0.0, 1.0, 0.0,

		// Face superior (azul)
		// Triângulo 5
		-0.5,  0.5,  0.5, 0.0, 0.0, 1.0,
		 0.5,  0.5,  0.5, 0.0, 0.0, 1.0,
		 0.5,  0.5, -0.5, 0.0, 0.0, 1.0,
		// Triângulo 6
		-0.5,  0.5,  0.5, 0.0, 0.0, 1.0,
		 0.5,  0.5, -0.5, 0.0, 0.0, 1.0,
		-0.5,  0.5, -0.5, 0.0, 0.0, 1.0,

		// Face inferior (amarelo)
		// Triângulo 7
		-0.5, -0.5,  0.5, 1.0, 1.0, 0.0,
		 0.5, -0.5,  0.5, 1.0, 1.0, 0.0,
		 0.5, -0.5, -0.5, 1.0, 1.0, 0.0,
		// Triângulo 8
		-0.5, -0.5,  0.5, 1.0, 1.0, 0.0,
		 0.5, -0.5, -0.5, 1.0, 1.0, 0.0,
		-0.5, -0.5, -0.5, 1.0, 1.0, 0.0,

		// Face esquerda (magenta)
		// Triângulo 9
		-0.5, -0.5,  0.5, 1.0, 0.0, 1.0,
		-0.5,  0.5,  0.5, 1.0, 0.0, 1.0,
		-0.5,  0.5, -0.5, 1.0, 0.0, 1.0,
		// Triângulo 10
		-0.5, -0.5,  0.5, 1.0, 0.0, 1.0,
		-0.5,  0.5, -0.5, 1.0, 0.0, 1.0,
		-0.5, -0.5, -0.5, 1.0, 0.0, 1.0,

		// Face direita (ciano)
		// Triângulo 11
		 0.5, -0.5,  0.5, 0.0, 1.0, 1.0,
		 0.5,  0.5,  0.5, 0.0, 1.0, 1.0,
		 0.5,  0.5, -0.5, 0.0, 1.0, 1.0,
		// Triângulo 12
		 0.5, -0.5,  0.5, 0.0, 1.0, 1.0,
		 0.5,  0.5, -0.5, 0.0, 1.0, 1.0,
		 0.5, -0.5, -0.5, 0.0, 1.0, 1.0,

	};


	GLuint VBO, VAO;

	//Geração do identificador do VBO
	glGenBuffers(1, &VBO);

	//Faz a conexão (vincula) do buffer como um buffer de array
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	//Envia os dados do array de floats para o buffer da OpenGl
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	//Geração do identificador do VAO (Vertex Array Object)
	glGenVertexArrays(1, &VAO);

	// Vincula (bind) o VAO primeiro, e em seguida  conecta e seta o(s) buffer(s) de vértices
	// e os ponteiros para os atributos
	glBindVertexArray(VAO);

	//Para cada atributo do vertice, criamos um "AttribPointer" (ponteiro para o atributo), indicando:
	// Localização no shader * (a localização dos atributos devem ser correspondentes no layout especificado no vertex shader)
	// Numero de valores que o atributo tem (por ex, 3 coordenadas xyz)
	// Tipo do dado
	// Se está normalizado (entre zero e um)
	// Tamanho em bytes
	// Deslocamento a partir do byte zero

	//Atributo posição (x, y, z)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);

	//Atributo cor (r, g, b)
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3*sizeof(GLfloat)));
	glEnableVertexAttribArray(1);



	// Observe que isso é permitido, a chamada para glVertexAttribPointer registrou o VBO como o objeto de buffer de vértice
	// atualmente vinculado - para que depois possamos desvincular com segurança
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Desvincula o VAO (é uma boa prática desvincular qualquer buffer ou array para evitar bugs medonhos)
	glBindVertexArray(0);

	return VAO;
}

int loadTexture(string path)
{
	GLuint texID;

	// Gera o identificador da textura na memória
	glGenTextures(1, &texID);
	glBindTexture(GL_TEXTURE_2D, texID);

	//Ajusta os parâmetros de wrapping e filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	//Carregamento da imagem
	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);

	if (data)
	{

		if (nrChannels == 3) //jpg, bmp
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		}
		else if (nrChannels == 4)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cout << "Failed to load texture" << std::endl;
	}

	stbi_image_free(data);

	return texID;
}

int loadSimpleOBJ(string filepath, int& nVerts, glm::vec3 color)
{
	vector<glm::vec3> vertices;
	vector <GLuint> indices;
	vector <glm::vec2> texCoords;
	vector <glm::vec3> normals;
	vector <GLfloat> vbuffer;

	ifstream inputFile;
	inputFile.open(filepath.c_str());
	if (inputFile.is_open())
	{
		char line[100];
		string sline;

		while (!inputFile.eof())
		{
			inputFile.getline(line, 100);
			sline = line;

			string word;

			istringstream ssline(line);
			ssline >> word;
			if (word == "v")
			{
				glm::vec3 v;
				ssline >> v.x >> v.y >> v.z;

				vertices.push_back(v);
			}
			if (word == "vt")
			{
				glm::vec2 vt;
				ssline >> vt.s >> vt.t;

				texCoords.push_back(vt);
			}
			if (word == "vn")
			{
				glm::vec3 vn;
				ssline >> vn.x >> vn.y >> vn.z;

				normals.push_back(vn);
			}
			if (word == "f")
			{
				string tokens[3];

				ssline >> tokens[0] >> tokens[1] >> tokens[2];

				for (int i = 0; i < 3; i++)
				{
					//Recuperando os indices de v
					int pos = tokens[i].find("/");
					string token = tokens[i].substr(0, pos);
					int index = atoi(token.c_str()) - 1;
					indices.push_back(index);

					vbuffer.push_back(vertices[index].x);
					vbuffer.push_back(vertices[index].y);
					vbuffer.push_back(vertices[index].z);

					vbuffer.push_back(color.r);
					vbuffer.push_back(color.g);
					vbuffer.push_back(color.b);

					//Recuperando os indices de vts
					tokens[i] = tokens[i].substr(pos + 1);
					pos = tokens[i].find("/");
					token = tokens[i].substr(0, pos);
					index = atoi(token.c_str()) - 1;
					vbuffer.push_back(texCoords[index].s);
					vbuffer.push_back(texCoords[index].t);

					//Recuperando os indices de vns
					tokens[i] = tokens[i].substr(pos + 1);
					index = atoi(tokens[i].c_str()) - 1;
					vbuffer.push_back(normals[index].x);
					vbuffer.push_back(normals[index].y);
					vbuffer.push_back(normals[index].z);
				}
			}
		}
	}
	else
	{
		cout << "Problema ao encontrar o arquivo " << filepath << endl;
	}
	inputFile.close();
	GLuint VBO, VAO;
	nVerts = vbuffer.size() / 11;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vbuffer.size() * sizeof(GLfloat), vbuffer.data(), GL_STATIC_DRAW);
	glGenVertexArrays(1, &VAO);

	glBindVertexArray(VAO);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	//Atributo cor (r, g, b)
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);
	//Atributo coordenada de textura (s, t)
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2);
	//Atributo normal do vértice (x, y, z)
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)(8 * sizeof(GLfloat)));
	glEnableVertexAttribArray(3);

	// Observe que isso é permitido, a chamada para glVertexAttribPointer registrou o VBO como o objeto de buffer de vértice
	// atualmente vinculado - para que depois possamos desvincular com segurança
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	// Desvincula o VAO (é uma boa prática desvincular qualquer buffer ou array para evitar bugs medonhos)
	glBindVertexArray(0);
	return VAO;
}

std::unordered_map<std::string, Material> loadMTL(const std::string& filename) {
	std::unordered_map<std::string, Material> materials;
	std::ifstream file(filename);
	std::string line, key;
	Material currentMaterial;
	std::string materialName;

	while (std::getline(file, line)) {
		std::istringstream iss(line);
		iss >> key;

		if (key == "newmtl") {
			iss >> materialName;
			currentMaterial = Material();
			materials[materialName] = currentMaterial;
		} else if (key == "Ns") {
			float ns;
			iss >> ns;
			currentMaterial.Ns = ns;
		} else if (key == "Ka") {
			iss >> currentMaterial.Ka.r >> currentMaterial.Ka.g >> currentMaterial.Ka.b;
		} else if (key == "Kd") {
			iss >> currentMaterial.Kd.r >> currentMaterial.Kd.g >> currentMaterial.Kd.b;
		} else if (key == "Ks") {
			iss >> currentMaterial.Ks.r >> currentMaterial.Ks.g >> currentMaterial.Ks.b;
		} else if (key == "Ke") {
			iss >> currentMaterial.Ke.r >> currentMaterial.Ke.g >> currentMaterial.Ke.b;
		} else if (key == "Ni") {
			iss >> currentMaterial.Ni;
		} else if (key == "d") {
			iss >> currentMaterial.d;
		} else if (key == "illum") {
			iss >> currentMaterial.illum;
		} else if (key == "map_Kd") {
			iss >> currentMaterial.map_Kd;
		}

		if (!materialName.empty()) {
			materials[materialName] = currentMaterial;
		}
	}
	return materials;
}
