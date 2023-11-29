/**
 * X11 Desktop Cube with OpenGL
 *
 * Description:
 *     This program demonstrates how to create an X11
 *     `_NET_WM_WINDOW_TYPE_DESKTOP` window with an OpenGL context, producing an
 *     effect of drawing graphics directly onto the desktop.
 *
 * Author: Michael Knap
 * Date: 4/11/2023
 *
 * License:
 *     MIT License.
 *
 * Dependencies:
 *     - X11 development libraries
 *     - OpenGL libraries
 *     - Xinerama
 *     - GLEW
 */

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <GL/glew.h>
#include <GL/glx.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xinerama.h>

// Color Palette
#include "nord.h"

#define APP_TITLE "OPENGL DESKTOP"

// Flag to control program termination
volatile sig_atomic_t terminate = 0;

// FPS and frame duration constants
const int TARGET_FPS = 60;
const int TARGET_FRAME_DURATION = 1000000 / TARGET_FPS;

// GLX attributes for OpenGL context creation
int glx_attributes[] = {
    GLX_RGBA,               // Use RGBA color mode
    GLX_DOUBLEBUFFER,       // Enable double buffering
    GLX_RED_SIZE, 8,        // 8 bits for the red channel
    GLX_GREEN_SIZE, 8,      // 8 bits for the green channel
    GLX_BLUE_SIZE, 8,       // 8 bits for the blue channel
    GLX_DEPTH_SIZE, 24,     // 24 bits for the depth buffer
    GLX_SAMPLE_BUFFERS, 1,  // Enable multisampling
    GLX_SAMPLES, 4,         // 4x antialiasing
    None                    // Terminate the attribute list
};


// 3D cube vertices and indices
GLfloat vertices[] = {
    -1.0, -1.0, 1.0,   // 0 Bottom Left Front
    1.0,  -1.0, 1.0,   // 1 Bottom Right Front
    1.0,  -1.0, -1.0,  // 2 Bottom Right Back
    -1.0, -1.0, -1.0,  // 3 Bottom Left Back
    -1.0, 1.0,  1.0,   // 4 Top Left Front
    1.0,  1.0,  1.0,   // 5 Top Right Front
    1.0,  1.0,  -1.0,  // 6 Top Right Back
    -1.0, 1.0,  -1.0   // 7 Top Left Back
};

GLubyte indices[] = {
    0, 1, 2, 3,  // Bottom
    4, 5, 6, 7,  // Top
    0, 4, 7, 3,  // Left
    1, 5, 6, 2,  // Right
    0, 1, 5, 4,  // Back
    3, 7, 6, 2   // Front
};

// Colors for cube vertices
GLfloat colors[] = {
    NORD9,   // Light blue
    NORD10,  // Darker blue
    NORD11,  // Red
    NORD12,  // Orange
    NORD9,   // Light blue
    NORD10,  // Darker blue
    NORD11,  // Red
    NORD12,  // Orange
};

// Struct to hold app context and data
typedef struct {
    Display *display;
    Window window;
    XineramaScreenInfo *screen_info;
    XVisualInfo *visual_info;
    GLXContext glx_context;
    Colormap color_map;
    GLuint vertex_buffer;
    GLuint index_buffer;
    GLuint color_buffer;
    int num_screens;
} AppData;

// Function to handle signal termination
void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        terminate = 1;
    }
}

// Function to handle cleanup
void cleanup(AppData *app_data) {
    if (app_data->vertex_buffer) glDeleteBuffers(1, &app_data->vertex_buffer);
    if (app_data->index_buffer) glDeleteBuffers(1, &app_data->index_buffer);
    if (app_data->color_buffer) glDeleteBuffers(1, &app_data->color_buffer);
    if (app_data->color_map) XFreeColormap(app_data->display, app_data->color_map);
    if (app_data->window) XDestroyWindow(app_data->display, app_data->window);
    if (app_data->glx_context) glXDestroyContext(app_data->display, app_data->glx_context);
    if (app_data->visual_info) XFree(app_data->visual_info);
    if (app_data->screen_info) XFree(app_data->screen_info);
    if (app_data->display) XCloseDisplay(app_data->display);
}

// Function to initialize X11 and OpenGL
int initialize(AppData *app_data) {
    // Open a connection to the X server
    app_data->display = XOpenDisplay(NULL);
    if (!app_data->display) {
        fprintf(stderr, "Failed to open X display\n");
        return -1;
    }

    // Query Xinerama for multi-monitor info
    int number_of_screens;
    app_data->screen_info = XineramaQueryScreens(app_data->display, &number_of_screens);
    if (!app_data->screen_info) {
        fprintf(stderr, "Failed to query multi-monitor information\n");
        return -1;
    }
    app_data->num_screens = number_of_screens;

    // Calculate combined width and height of all monitors
    int combined_width = 0;
    int combined_height = 0;
    for (int i = 0; i < number_of_screens; i++) {
        combined_width += app_data->screen_info[i].width;
        combined_height +=
            (app_data->screen_info[i].height > combined_height) ? app_data->screen_info[i].height : 0;
    }

    // Get a suitable visual for OpenGL rendering
    Window root = DefaultRootWindow(app_data->display);
    app_data->visual_info = glXChooseVisual(app_data->display, 0, glx_attributes);
    if (!app_data->visual_info) {
        fprintf(stderr, "No appropriate visual found\n");
        return -1;
    }

    // Create a colormap and set window attributes
    app_data->color_map = XCreateColormap(app_data->display, root, app_data->visual_info->visual, AllocNone);
    if (!app_data->color_map) {
        fprintf(stderr, "Failed to create colormap\n");
        return -1;
    }
    XSetWindowAttributes window_attributes = {
        .colormap = app_data->color_map, .event_mask = ExposureMask | KeyPressMask
    };

    // Create an X window and set its name
    app_data->window = XCreateWindow(app_data->display, root, 0, 0, combined_width, combined_height, 0,
                                     app_data->visual_info->depth, InputOutput, app_data->visual_info->visual,
                                     CWColormap | CWEventMask, &window_attributes);
    if (!app_data->window) {
        fprintf(stderr, "Failed to create window\n");
        return -1;
    }
    XStoreName(app_data->display, app_data->window, APP_TITLE);

    // Create an OpenGL rendering context
    app_data->glx_context = glXCreateContext(app_data->display, app_data->visual_info, NULL, GL_TRUE);
    if (!app_data->glx_context) {
        fprintf(stderr, "Failed to create GLX context\n");
        return -1;
    }

    // Set the window type to desktop
    Atom net_wm_window_type = XInternAtom(app_data->display, "_NET_WM_WINDOW_TYPE", False);
    Atom net_wm_window_type_desktop =
        XInternAtom(app_data->display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    XChangeProperty(app_data->display, app_data->window, net_wm_window_type, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&net_wm_window_type_desktop,
                    1);
    XMapWindow(app_data->display, app_data->window);

    // Initialize GLEW for OpenGL extensions
    glXMakeCurrent(app_data->display, app_data->window, app_data->glx_context);
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        return -1;
    }

    // Generate and set up the vertex buffer.
    glGenBuffers(1, &app_data->vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, app_data->vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexPointer(3, GL_FLOAT, 0, NULL);

    // Generate and set up the index buffer.
    glGenBuffers(1, &app_data->index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app_data->index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                 GL_STATIC_DRAW);

    // Generate and set up the color buffer.
    glGenBuffers(1, &app_data->color_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, app_data->color_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
    glColorPointer(4, GL_FLOAT, 0, NULL);

    // Enable depth testing and multi-sampling for improved rendering quality.
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    // Enable client-side capabilities for vertex and color arrays.
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    // Set dark background
    glClearColor(NORD0);

    return 0;
}

// Rotation angles for animation
float rotation_angle_x = 0.0f;
float rotation_angle_y = 0.0f;

// Update rotation angles for animation
void update_rotation_angles(float *angle_x, float *angle_y) {
    *angle_x = fmod(*angle_x + 0.5f, 360.0f);
    *angle_y = fmod(*angle_y + 0.5f, 360.0f);
}

// Function to handle main rendering loop
void main_loop(AppData *app_data) {
    while (!terminate) {
        struct timespec start_time, end_time;

        // Record start time
        clock_gettime(CLOCK_MONOTONIC, &start_time);

        // Clear the screen and update rotation angles
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        update_rotation_angles(&rotation_angle_x, &rotation_angle_y);

        // Render cubes: loop through all screens,
        // set their viewports, and draw cubes.
        for (int i = 0; i <  app_data->num_screens; i++) {

            // Define the viewport for the current screen
            glViewport(
                app_data->screen_info[i].x_org,
                app_data->screen_info[i].y_org,
                app_data->screen_info[i].width,
                app_data->screen_info[i].height
            );

            // Set projection matrix for perspective rendering
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            gluPerspective(
                50,
                (GLfloat)app_data->screen_info[i].width / (GLfloat)app_data->screen_info[i].height,
                0.1,
                10.0
            );

            // Set the model view matrix and define the camera's
            // position and orientation
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            gluLookAt(0, 0, 5, 0, 0, 0, 0, 1, 0);
            glRotatef(rotation_angle_x, 1.0f, 0.0f, 0.0f);
            glRotatef(rotation_angle_y, 0.0f, 1.0f, 0.0f);

            // Draw the cube
            glDrawElements(GL_QUADS, 24, GL_UNSIGNED_BYTE, NULL);
        }

        // Swap buffers for double buffering
        glXSwapBuffers(app_data->display, app_data->window);

        // Record end time
        clock_gettime(CLOCK_MONOTONIC, &end_time);

        // Calculate time taken for the frame
        int frame_time_elapsed = (end_time.tv_sec - start_time.tv_sec) * 1000000 +
                                 (end_time.tv_nsec - start_time.tv_nsec) / 1000;

        // Calculate remaining time to delay to achieve the target frame duration
        int time_to_sleep = TARGET_FRAME_DURATION - frame_time_elapsed;
        if (time_to_sleep > 0) {
            usleep(time_to_sleep);
        }
        XFlush(app_data->display);
    }
}

int main(void) {
    AppData app_data = {0};

    // Register signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (initialize(&app_data) != 0) {
        fprintf(stderr, "Initialization failed\n");
        cleanup(&app_data);
        exit(EXIT_FAILURE);
    }
    main_loop(&app_data);
    cleanup(&app_data);
    exit(EXIT_SUCCESS);
}
