#include <cassert>
#include <cstring>
#define _USE_MATH_DEFINES
#include <cmath>//    return 0;
#include <climits>
#include <cfloat>
#include <algorithm>
#include <string>
#include <iostream>
#include <vector>
#include <memory>
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "Camera.h"
#include "GLSL.h"
#include "MatrixStack.h"
#include "Program.h"
#include "Shape.h"
#include "Material.h"

using namespace std;

class Light {
public:
    glm::vec3 position;
    glm::vec3 color;
    Light(glm::vec3 p, glm::vec3 c) : position(p), color(c) {}
};

GLFWwindow* window;
string RESOURCE_DIR = "./";
int TASK = 1;
bool OFFLINE = false;
shared_ptr<Camera> camera;
shared_ptr<Program> prog;
shared_ptr<Program> progBP;
shared_ptr < Program> progSil;
shared_ptr<Program> progCel;
shared_ptr<Shape> shape;  // Bunny
shared_ptr<Shape> teapot; // Teapot

glm::vec3 bunnyTranslate;
float bunnyScale;
vector<shared_ptr<Material>> materials;
vector<shared_ptr<Light>> lights;
int shaderIndex = 0;
int materialIndex = 0;
int lightIndex = 0;
bool keyToggles[256] = { false };

static void error_callback(int error, const char* description) { cerr << description << endl; }
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(window, GL_TRUE);
}
static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    double xmouse, ymouse;
    glfwGetCursorPos(window, &xmouse, &ymouse);
    if (action == GLFW_PRESS) {
        camera->mouseClicked((float)xmouse, (float)ymouse, (mods & GLFW_MOD_SHIFT), (mods & GLFW_MOD_CONTROL), (mods & GLFW_MOD_ALT));
    }
}
static void cursor_position_callback(GLFWwindow* window, double xmouse, double ymouse) {
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) camera->mouseMoved((float)xmouse, (float)ymouse);
}

static void char_callback(GLFWwindow* window, unsigned int key) {
    keyToggles[key] = !keyToggles[key];
    if (TASK >= 2) {
        //if (key == 's' || key == 'S') {
        //    // Cycle through 3 shaders if in Task 5, otherwise 2
        //    int numShaders = (TASK >= 5) ? 3 : 2;
        //    shaderIndex = (shaderIndex + 1) % numShaders;
        //}
        if (key == 's' || key == 'S') {
            int numShaders = (TASK >= 6) ? 4 : (TASK >= 5) ? 3 : 2;
            shaderIndex = (shaderIndex + 1) % numShaders;
        }
        if (key == 'm' || key == 'M') materialIndex = (materialIndex + 1) % (int)materials.size();
    }
    if (TASK >= 3) {
        if (key == 'l' || key == 'L') lightIndex = (lightIndex + 1) % (int)lights.size();
        if (key == 'x') lights[lightIndex]->position.x -= 0.1f;
        if (key == 'X') lights[lightIndex]->position.x += 0.1f;
        if (key == 'y') lights[lightIndex]->position.y -= 0.1f;
        if (key == 'Y') lights[lightIndex]->position.y += 0.1f;
    }
}

static void resize_callback(GLFWwindow* window, int width, int height) { glViewport(0, 0, width, height); }

static void saveImage(const char* filepath, GLFWwindow* w)
{
    int width, height;
    glfwGetFramebufferSize(w, &width, &height);
    GLsizei nrChannels = 3;
    GLsizei stride = nrChannels * width;
    stride += (stride % 4) ? (4 - stride % 4) : 0;
    GLsizei bufferSize = stride * height;
    std::vector<char> buffer(bufferSize);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer.data());
    stbi_flip_vertically_on_write(true);
    int rc = stbi_write_png(filepath, width, height, nrChannels, buffer.data(), stride);
    if (rc) {
        cout << "Wrote to " << filepath << endl;
    }
    else {
        cout << "Couldn't write to " << filepath << endl;
    }
}

// Updated to take the specific shape to draw

void drawShape(shared_ptr<MatrixStack> P, shared_ptr<MatrixStack> MV, shared_ptr<Shape> s) {
    shared_ptr<Program> activeProg;
    if (shaderIndex == 0)      activeProg = prog;
    else if (shaderIndex == 1) activeProg = progBP;
    else if (shaderIndex == 2) activeProg = progSil;
    else                       activeProg = progCel; // Task 6

    activeProg->bind();

    glUniformMatrix4fv(activeProg->getUniform("P"), 1, GL_FALSE, glm::value_ptr(P->topMatrix()));
    glUniformMatrix4fv(activeProg->getUniform("MV"), 1, GL_FALSE, glm::value_ptr(MV->topMatrix()));

    // Shaders requiring Normal Matrix
    if (activeProg == progBP || activeProg == progSil || activeProg == progCel) {
        glm::mat4 MVit = glm::transpose(glm::inverse(MV->topMatrix()));
        glUniformMatrix4fv(activeProg->getUniform("MVit"), 1, GL_FALSE, glm::value_ptr(MVit));
    }

    // Shaders requiring Materials and Lights
    if (activeProg == progBP || activeProg == progCel) {
        auto m = materials[materialIndex];
        glUniform3fv(activeProg->getUniform("ka"), 1, glm::value_ptr(m->ka));
        glUniform3fv(activeProg->getUniform("kd"), 1, glm::value_ptr(m->kd));
        glUniform3fv(activeProg->getUniform("ks"), 1, glm::value_ptr(m->ks));
        glUniform1f(activeProg->getUniform("s"), m->s);

        glm::vec3 posArray[2], colArray[2];
        for (int i = 0; i < 2; i++) {
            posArray[i] = lights[i]->position;
            colArray[i] = lights[i]->color;
        }
        glUniform3fv(activeProg->getUniform("lightPos"), 2, glm::value_ptr(posArray[0]));
        glUniform3fv(activeProg->getUniform("lightCol"), 2, glm::value_ptr(colArray[0]));
    }

    s->draw(activeProg);
    activeProg->unbind();
}

static void init() {
    glfwSetTime(0.0);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    // 1. Initialize Materials and Lights FIRST to prevent out-of-range errors
    materials.push_back(make_shared<Material>(glm::vec3(0.2), glm::vec3(0.8, 0.7, 0.7), glm::vec3(1.0, 0.9, 0.8), 200.0f));
    materials.push_back(make_shared<Material>(glm::vec3(0.0, 0.0, 0.3), glm::vec3(0.0, 0.0, 0.9), glm::vec3(0.2, 1.0, 0.2), 150.0f));
    materials.push_back(make_shared<Material>(glm::vec3(0.1, 0.1, 0.15), glm::vec3(0.3, 0.3, 0.45), glm::vec3(0.2), 10.0f));

    // Always provide the 2 lights so BP and Cel shaders don't crash
    lights.push_back(make_shared<Light>(glm::vec3(1.0, 1.0, 1.0), glm::vec3(0.8, 0.8, 0.8)));
    lights.push_back(make_shared<Light>(glm::vec3(-1.0, 1.0, 1.0), glm::vec3(0.2, 0.2, 0.0)));

    // 2. Set Initial Shader Index (Checking high to low)
    if (TASK >= 6)      shaderIndex = 3; // Start with Cel
    else if (TASK >= 5) shaderIndex = 2; // Start with Silhouette
    else if (TASK >= 2) shaderIndex = 1; // Start with Blinn-Phong
    else                shaderIndex = 0; // Default Task 1 Normals

    // 3. Load Meshes
    shape = make_shared<Shape>();
    shape->loadMesh(RESOURCE_DIR + "bunny.obj");
    shape->init();

    teapot = make_shared<Shape>();
    teapot->loadMesh(RESOURCE_DIR + "teapot.obj");
    teapot->init();

    // 4. Initialize Programs
    prog = make_shared<Program>();
    prog->setShaderNames(RESOURCE_DIR + "normal_vert.glsl", RESOURCE_DIR + "normal_frag.glsl");
    prog->init();
    prog->addAttribute("aPos");
    prog->addAttribute("aNor");
    prog->addAttribute("aTex");
    prog->addUniform("MV");
    prog->addUniform("P");

    progBP = make_shared<Program>();
    progBP->setShaderNames(RESOURCE_DIR + "bp_vert.glsl", RESOURCE_DIR + "bp_frag.glsl");
    progBP->init();
    progBP->addAttribute("aPos");
    progBP->addAttribute("aNor");
    progBP->addAttribute("aTex");
    progBP->addUniform("P");
    progBP->addUniform("MV");
    progBP->addUniform("MVit");
    progBP->addUniform("ka");
    progBP->addUniform("kd");
    progBP->addUniform("ks");
    progBP->addUniform("s");
    progBP->addUniform("lightPos"); // Added here for Task 2+
    progBP->addUniform("lightCol"); // Added here for Task 2+

    progSil = make_shared<Program>();
    progSil->setShaderNames(RESOURCE_DIR + "sil_vert.glsl", RESOURCE_DIR + "sil_frag.glsl");
    progSil->init();
    progSil->addAttribute("aPos");
    progSil->addAttribute("aNor");
    progSil->addAttribute("aTex");
    progSil->addUniform("P");
    progSil->addUniform("MV");
    progSil->addUniform("MVit");

    progCel = make_shared<Program>();
    progCel->setShaderNames(RESOURCE_DIR + "cel_vert.glsl", RESOURCE_DIR + "cel_frag.glsl");
    progCel->init();
    progCel->addAttribute("aPos");
    progCel->addAttribute("aNor");
    progCel->addAttribute("aTex");
    progCel->addUniform("P");
    progCel->addUniform("MV");
    progCel->addUniform("MVit");
    progCel->addUniform("ka");
    progCel->addUniform("kd");
    progCel->addUniform("ks");
    progCel->addUniform("s");
    progCel->addUniform("lightPos");
    progCel->addUniform("lightCol");

    // 5. Setup Camera and Constants
    camera = make_shared<Camera>();
    camera->setInitDistance(2.0f);
    bunnyTranslate = glm::vec3(0.0f, -0.5f, 0.0f);
    bunnyScale = 0.5f;
}

static void render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (keyToggles[(unsigned)'c']) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (keyToggles[(unsigned)'z']) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    camera->setAspect((float)width / (float)height);
    float t = (float)glfwGetTime();
    if (keyToggles[(unsigned)' ']) {
        t = 0.0f; // Reset or pause if spacebar is toggled
    }

    auto P = make_shared<MatrixStack>();
    auto MV = make_shared<MatrixStack>();

    P->pushMatrix();
    camera->applyProjectionMatrix(P);

    MV->pushMatrix();
    camera->applyViewMatrix(MV);

    // --- DRAW BUNNY (Task 4: Translated Left) ---
    MV->pushMatrix();

    float bunnyX = (TASK >= 4) ? -0.5f : 0.0f;
    MV->translate(glm::vec3(bunnyX, 0.0f, 0.0f));
    MV->translate(bunnyTranslate);
    // Apply rotation around Y-axis using time t
    float rotationAmount = (TASK >= 4) ? t : 0.0f;
    MV->rotate(rotationAmount, glm::vec3(0.0f, 1.0f, 0.0f));
    MV->scale(bunnyScale);
    drawShape(P, MV, shape);
    MV->popMatrix();

    // --- DRAW TEAPOT (Task 4 Transformations) ---
    if (TASK >= 4) {
        MV->pushMatrix();
        // 1. Final position in the world
        MV->translate(glm::vec3(0.5f, 0.0f, 0.0f));

        // 2. SHEAR (Applied second to last)
        glm::mat4 S(1.0f);
        S[0][1] = 0.5f *cos(t) ;
        MV->multMatrix(S);

        // 3. ROTATE (Applied second)
        // Face the spout toward the bunny before the shear hits it.
        MV->rotate(glm::radians(180.0f), glm::vec3(0, 1, 0));

        // 4. SCALE (Applied first to the local model)
        MV->scale(0.5f);

        drawShape(P, MV, teapot);
        MV->popMatrix();
    }
    if (OFFLINE) {
        string filename = "output" + std::to_string(TASK) + ".png";
        saveImage(filename.c_str(), window);
        GLSL::checkError(GET_FILE_LINE);
        glfwSetWindowShouldClose(window, true);
    }
}

int main(int argc, char** argv) {
    if (argc < 3) { cout << "Usage: A3 RESOURCE_DIR TASK" << endl; return 0; }
    RESOURCE_DIR = argv[1] + string("/");
    TASK = atoi(argv[2]);

    if (!glfwInit()) return -1;
    window = glfwCreateWindow(640, 480, "Assignment 3", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glewExperimental = true;
    if (glewInit() != GLEW_OK) return -1;
    if (argc >= 4) {
        OFFLINE = atoi(argv[3]) != 0;
    }

    glfwSetKeyCallback(window, key_callback);
    glfwSetCharCallback(window, char_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetFramebufferSizeCallback(window, resize_callback);

    init();
    while (!glfwWindowShouldClose(window)) {
        render();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}