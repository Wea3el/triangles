#define GL_SILENCE_DEPRECATION
#define STB_IMAGE_IMPLEMENTATION

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#define GL_GLEXT_PROTOTYPES 1
#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>

#define LOG(argument) std::cout << argument << '\n'

const int WINDOW_WIDTH  = 640,
          WINDOW_HEIGHT = 480;

const float BG_RED     = 0.0f,
            BG_BLUE    = 0.0f,
            BG_GREEN   = 0.0f,
            BG_OPACITY = 1.0f;

const int VIEWPORT_X = 0,
          VIEWPORT_Y = 0,
          VIEWPORT_WIDTH  = WINDOW_WIDTH,
          VIEWPORT_HEIGHT = WINDOW_HEIGHT;

const char V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
           F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

const float MILLISECONDS_IN_SECOND = 1000.0;

const char PLAYER_SPRITE_FILEPATH[] = "assets/download.jpeg";
const char OTHER[] = "assets/licensed-image.jpeg";

const float MINIMUM_COLLISION_DISTANCE = 1.0f;

const float RADIUS = 2.0f;      // radius of circle
const float ROT_SPEED = 0.01f;  // rotational speed
const float ROT_ANGLE = glm::radians(5.0f);

float g_angle    = 0.0f;        // current angle accumulated
float g_x_coords = RADIUS;      // current x-coord
float g_y_coords = 0.0f;


SDL_Window* display_window;
bool game_is_running = true;
bool is_growing = true;

ShaderProgram g_program;
glm::mat4 g_view_matrix, g_model_matrix, g_projection_matrix, other_model_matrix;

float previous_ticks = 0.0f;

GLuint player_texture_id;
GLuint other_texture_id;

glm::vec3 player_position = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 player_movement = glm::vec3(0.2f, 0.2f, 0.0f);

glm::vec3 other_position = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 other_movement = glm::vec3(0.2f, 0.2f, 0.0f);

glm::vec3 player_orientation = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 player_rotation    = glm::vec3(0.0f, 0.0f, 0.0f);

float player_speed = 1.0f;  // move 1 unit per second
float other_speed = 1.5f;
#define LOG(argument) std::cout << argument << '\n'

const int NUMBER_OF_TEXTURES = 1; // to be generated, that is
const GLint LEVEL_OF_DETAIL  = 0;  // base image level; Level n is the nth mipmap reduction image
const GLint TEXTURE_BORDER   = 0;   // this value MUST be zero

GLuint load_texture(const char* filepath)
{
    // STEP 1: Loading the image file
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);
    
    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }
    
    // STEP 2: Generating and binding a texture ID to our image
    GLuint textureID;
    glGenTextures(NUMBER_OF_TEXTURES, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER, GL_RGBA, GL_UNSIGNED_BYTE, image);
    
    // STEP 3: Setting our texture filter parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    // STEP 4: Releasing our file from memory and returning our texture id
    stbi_image_free(image);
    
    return textureID;
}

void initialise()
{
    SDL_Init(SDL_INIT_VIDEO);
    display_window = SDL_CreateWindow("Hello, Textures!",
                                      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      WINDOW_WIDTH, WINDOW_HEIGHT,
                                      SDL_WINDOW_OPENGL);
    
    SDL_GLContext context = SDL_GL_CreateContext(display_window);
    SDL_GL_MakeCurrent(display_window, context);
    
#ifdef _WINDOWS
    glewInit();
#endif
    
    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    
    g_program.load(V_SHADER_PATH, F_SHADER_PATH);
    
    g_model_matrix = glm::mat4(1.0f);
    other_model_matrix = glm::mat4(1.0f);
    other_model_matrix = glm::translate(other_model_matrix, glm::vec3(1.0f, 1.0f, 0.0f));
    
    
    g_view_matrix = glm::mat4(1.0f);  // Defines the position (location and orientation) of the camera
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);  // Defines the characteristics of your camera, such as clip planes, field of view, projection method etc.
    
    g_program.set_projection_matrix(g_projection_matrix);
    g_program.set_view_matrix(g_view_matrix);
    
    glUseProgram(g_program.get_program_id());
    
    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);
    
    player_texture_id = load_texture(PLAYER_SPRITE_FILEPATH);
    other_texture_id = load_texture(OTHER);
    
    // enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    // VERY IMPORTANT: If nothing is pressed, we don't want to go anywhere
    SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE)
            {
                game_is_running = false;
            }
        }
    
}

/**
 Uses distance formula.
 */

void update()
{
    float ticks = (float) SDL_GetTicks() / MILLISECONDS_IN_SECOND; // get the current number of ticks
    float delta_time = ticks - previous_ticks; // the delta time is the difference from the last frame
    previous_ticks = ticks;
    
    
    // Add direction * units per second * elapsed time
    player_position += player_movement * player_speed * delta_time;
    
    other_position += other_movement * other_speed * delta_time;
    
    g_model_matrix = glm::mat4(1.0f);
    other_model_matrix= glm::mat4(1.0f);
    g_model_matrix = glm::translate(g_model_matrix, player_position);
    
    g_angle += ROT_SPEED;
        
    // 2. Calculate x,y-coordinates based on angle using trig
    g_x_coords = RADIUS * glm::cos(g_angle);
    g_y_coords = RADIUS * glm::sin(g_angle);
        
    // 3. Reset model matrix back to the origin, and apply translation based on step 2
    other_model_matrix = g_model_matrix;
    other_model_matrix = glm::translate(other_model_matrix, glm::vec3(g_x_coords, g_y_coords, 0.0f));
    other_model_matrix = glm::rotate(other_model_matrix, ROT_ANGLE, glm::vec3(0.0f, 1.0f, 0.0f));
    
   
}

void draw_object(glm::mat4 &object_model_matrix, GLuint &object_texture_id)
{
    g_program.set_model_matrix(object_model_matrix);
    glBindTexture(GL_TEXTURE_2D, object_texture_id);
    glDrawArrays(GL_SQUARE_NV, 0, 8); // we are now drawing 2 triangles, so we use 6 instead of 3
}

void render() {
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Vertices
    float vertices[] = {
        -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f,  // triangle 1
        -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f   // triangle 2
    };

    // Textures
    float texture_coordinates[] = {
        0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,     // triangle 1
        0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,     // triangle 2
    };
    
    glVertexAttribPointer(g_program.get_position_attribute(), 2, GL_FLOAT, false, 0, vertices);
    glEnableVertexAttribArray(g_program.get_position_attribute());
    
    glVertexAttribPointer(g_program.get_tex_coordinate_attribute(), 2, GL_FLOAT, false, 0, texture_coordinates);
    glEnableVertexAttribArray(g_program.get_tex_coordinate_attribute());
    
    // Bind texture
    draw_object(g_model_matrix, player_texture_id);
    draw_object(other_model_matrix, other_texture_id);
    
    // We disable two attribute arrays now
    glDisableVertexAttribArray(g_program.get_position_attribute());
    glDisableVertexAttribArray(g_program.get_tex_coordinate_attribute());
    
    SDL_GL_SwapWindow(display_window);
}

void shutdown() { SDL_Quit(); }


int main(int argc, char* argv[])
{
    initialise();
    
    while (game_is_running)
    {
        process_input();
        update();
        render();
    }
    
    shutdown();
    return 0;
}
