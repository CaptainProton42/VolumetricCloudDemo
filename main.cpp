#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#define STB_IMAGE_IMPLEMENTATION    
#include <stb_image.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <iostream>

#include "camera.h"
#include "shader.h"

#define PI 3.1415926535f

constexpr GLuint SCR_WIDTH = 1920;
constexpr GLuint SCR_HEIGHT = 1080;

// Delta time
GLfloat deltaTime = 0.0f;
GLfloat lastFrame = 0.0f;

// Camera
Camera camera;
bool firstMouse = true;
GLfloat lastX = SCR_WIDTH / 2.0f;
GLfloat lastY = SCR_HEIGHT / 2.0f;
glm::mat4 view, projection, ivpm; // view, projection and inverse view-projection matrices
bool mouseControlsCamera = true;
bool cameraDirty = false;

////////////////////////////////////////////////////
//                SHADER PARAMS                   //
////////////////////////////////////////////////////

// Sun Params
float sunEnergy = 1.0;
glm::vec3 sunColor = glm::vec3(1.0, 1.0, 1.0);
float sunAltitude = 0.2f * PI;
float sunAzimuth = PI;

// Cloud Layer Params
float cloudBottom = 1500.0f;
float cloudTop = 5000.0f;

// Cloud Shape Params
float shapeScale = 0.00002f;
glm::vec3 shapeWeights = glm::vec3(0.625f, 0.250f, 0.125f);
float cloudCover = 0.35f;

// Cloud Detail Params.
float detailScale = 0.0004f;
glm::vec3 detailWeights = glm::vec3(0.625f, 0.250f, 0.125f);
float detailMultiplier = 0.05f;

// Cloud Animation Params.
float windSpeed = 50.0f;

// Lighting Params
float lightAbsorptionTowardsSun = 0.2f;
float forwardScattering = 0.1f;
float powderStrength = 0.25f;

// Cloud Absorption Params.
float cloudAbsorption = 1.0f;
glm::vec3 ambientColor = glm::vec3(0.4f, 0.4f, 0.4f);

// Raymarching Params.
int raymarchingSteps = 256;
float renderDistance = 50000.0f;

// Use a single large triangle instead of a quad for better cache coherency:
// https://michaldrobot.com/2014/04/01/gcn-execution-patterns-in-full-screen-passes/
GLfloat vertices[] = {
	-1.0, -1.0,
	-1.0,  3.0,
	 3.0, -1.0
};

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void processInput(GLFWwindow *window);

void drawGUI();

int main()
{
	
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Volumetric Clouds Demo", NULL, NULL);
	
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl; glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetKeyCallback(window, key_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl; return -1;
	}

	glClearColor(0.5, 0.5, 0.5, 1.0);

	// Load the cloud shader.
	Shader cloudShader("clouds.vert", "clouds.frag");
	cloudShader.use();
	cloudShader.setVec2("resolution", glm::vec2(SCR_WIDTH, SCR_HEIGHT));

	// Set up VAO and VBO for full-screen triangle
	GLuint VAO, VBO;
	glGenBuffers(1, &VBO);
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

	// Texture loading.
	glEnable(GL_TEXTURE_3D);
	int texWidth, texHeight, texChannels;
	unsigned char* texData; 
	stbi_set_flip_vertically_on_load(true);

	// Base Worley noise (128x128x128).
	texData = stbi_load("base_noise.png", &texWidth, &texHeight, &texChannels, 0);
	unsigned int baseNoiseTex;
	glGenTextures(1, &baseNoiseTex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, baseNoiseTex);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, texHeight, texHeight, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
	cloudShader.setInt("baseNoise", 0);

	// Detail Worley noise (32x32x32)
	texData = stbi_load("detail_noise.png", &texWidth, &texHeight, &texChannels, 0);
	unsigned int detailNoiseTex;
	glGenTextures(1, &detailNoiseTex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D, detailNoiseTex);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, texHeight, texHeight, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
	cloudShader.setInt("detailNoise", 1);

	// Imgui
	IMGUI_CHECKVERSION();
    ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	while (!glfwWindowShouldClose(window))
    {
		GLfloat currentFrame = (GLfloat)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

		processInput(window);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

        drawGUI();

		if (cameraDirty) {
			view = camera.GetViewMatrix();
			projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
			ivpm = glm::inverse(projection * view);
		}

		// Calculate sun direction vector.
		glm::vec3 sunDir = glm::vec3(0.0, 0.0, 1.0);
		sunDir = glm::rotate(sunDir, sunAltitude, glm::vec3(-1.0, 0.0, 0.0));
		sunDir = glm::rotate(sunDir, sunAzimuth, glm::vec3(0.0, 1.0, 0.0));

		cloudShader.use();
		cloudShader.setMat4("ivpm", ivpm);
		cloudShader.setMat4("view", view);
		cloudShader.setMat4("projection", projection);
		cloudShader.setVec3("cameraPos", camera.Position);
		cloudShader.setFloat("time", currentFrame);
		cloudShader.setFloat("sunEnergy", sunEnergy);
		cloudShader.setVec3("sunColor", sunColor);
		cloudShader.setVec3("sunDirection", sunDir);
		cloudShader.setFloat("cloudBottom", cloudBottom);
		cloudShader.setFloat("cloudTop", cloudTop);
		cloudShader.setFloat("shapeScale", shapeScale);
		cloudShader.setVec3("shapeWeights", shapeWeights);
		cloudShader.setFloat("cloudCover", cloudCover);
		cloudShader.setFloat("detailScale", detailScale);
		cloudShader.setVec3("detailWeights", detailWeights);
		cloudShader.setFloat("detailMultiplier", detailMultiplier);
		cloudShader.setFloat("windSpeed", windSpeed);
		cloudShader.setFloat("lightAbsorptionTowardsSun", lightAbsorptionTowardsSun);
		cloudShader.setFloat("forwardScattering", forwardScattering);
		cloudShader.setFloat("powderStrength", powderStrength);
		cloudShader.setFloat("cloudAbsorption", cloudAbsorption);
		cloudShader.setVec3("ambient", ambientColor);
		cloudShader.setInt("raymarchingSteps", raymarchingSteps);
		cloudShader.setFloat("renderDistance", renderDistance);

		glBindVertexArray(VAO);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
		glfwPollEvents();

		cameraDirty = false;
	}
	
	glfwTerminate();
	return 0;
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
	if (mouseControlsCamera) {
		if (firstMouse) {
			lastX = (GLfloat)xpos;
			lastY = (GLfloat)ypos;
			firstMouse = false;
		}

		GLfloat xoffset = (GLfloat)xpos - lastX;
		GLfloat yoffset = lastY - (GLfloat)ypos; // reversed since y-coordinates go from bottom to top

		lastX = (GLfloat)xpos;
		lastY = (GLfloat)ypos;

		camera.ProcessMouseMovement(xoffset, yoffset);

		cameraDirty = true;
	}
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
	if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
		mouseControlsCamera = !mouseControlsCamera;
		if (mouseControlsCamera) {
			double xpos, ypos;
			glfwGetCursorPos(window, &xpos, &ypos);
			lastX = (GLfloat)xpos;
			lastY = (GLfloat)ypos;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
		else glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
		cameraDirty = true;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
		cameraDirty = true;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
		cameraDirty = true;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
		cameraDirty = true;
	if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
        camera.ProcessKeyboard(UP, deltaTime);
		cameraDirty = true;
	if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
        camera.ProcessKeyboard(DOWN, deltaTime);
		cameraDirty = true;
}

void drawGUI() {
	// Shader Settings Window
	ImGui::Begin("Press [SPACE] to edit settings.");
	ImGui::Text("Sun Settings");
	ImGui::SliderFloat("Sun Energy", &sunEnergy, 0.0f, 1.0f);
	ImGui::ColorEdit3("Sun Color", glm::value_ptr(sunColor));
	ImGui::SliderAngle("Sun Altitude", &sunAltitude, -90.0f, 90.0f);
	ImGui::SliderAngle("Sun Azimuth", &sunAzimuth, 0.0f, 360.0f);
	ImGui::Text("Cloud Layer Settings");
	ImGui::SliderFloat("Cloud Min Height", &cloudBottom, 0.0f, 10000.0f);
	ImGui::SliderFloat("Cloud Max Height", &cloudTop, 0.0f, 10000.0f);
	ImGui::Text("Cloud Shape Settings");
	ImGui::SliderFloat("Cloud Scale", &shapeScale, 0.00001f, 0.00005f, "%.10f");
	ImGui::DragFloat3("Coud Shape Weights", glm::value_ptr(shapeWeights), 0.0f, 1.0f);
	ImGui::SliderFloat("Cloud Cover", &cloudCover, 0.0f, 1.0f);
	ImGui::Text("Cloud Detail Settings");
	ImGui::SliderFloat("Cloud Detail Scale", &detailScale, 0.0001f, 0.0005f, "%.10f");
	ImGui::DragFloat3("Cloud Detail Weights", glm::value_ptr(detailWeights), 0.0f, 1.0f);
	ImGui::SliderFloat("Cloud Detail Multiplier", &detailMultiplier, 0.0f, 1.0f);
	ImGui::Text("Cloud Animation Settings");
	ImGui::SliderFloat("Wind Speed", &windSpeed, 0.0f, 100.0f);
	ImGui::Text("Lighting Settings");
	ImGui::SliderFloat("Light Absorption Towards Sun", &lightAbsorptionTowardsSun, 0.0f, 1.0f);
	ImGui::SliderFloat("Forward Scattering", &forwardScattering, -1.0f, 1.0f);
	ImGui::SliderFloat("Powder Strength", &powderStrength, 0.0f, 1.0f);
	ImGui::Text("Cloud Absorption Settings");
	ImGui::SliderFloat("Cloud Absorption/Density", &cloudAbsorption, 0.0f, 1.0f);
	ImGui::ColorEdit3("Cloud Ambient Color", glm::value_ptr(ambientColor));
	ImGui::Text("Raymarch Settings");
	ImGui::SliderInt("Raymarch Steps", &raymarchingSteps, 8, 512);
	ImGui::SliderFloat("Render Distance", &renderDistance, 1000.0f, 250000.0f);
	ImGui::End();

	// FPS Counter
	ImGui::Begin("FPS");
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
}