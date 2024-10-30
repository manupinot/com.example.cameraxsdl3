/*
 * Program Name: CameraXSDL3
 * Description:
 * This program integrates Android's CameraX API with SDL (Simple DirectMedia Layer)
 * to capture, process, and render YUV images in a cross-platform multimedia environment.
 * It includes Java and native C components, leveraging JNI to process YUV image data
 * and update textures in SDL for rendering. Core functionalities include handling
 * permissions, initializing resources, managing events, and rendering frames with
 * orientation adjustments.
 *
 * Usage:
 * - The program is free to use, modify, and distribute under the following terms.
 * - Ensure that the necessary Android permissions (e.g., Camera) are set up for
 *   full functionality on Android devices.
 *
 * License:
 * This software is provided 'as-is,' without any express or implied warranty. In no event
 * will the authors be held liable for any damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose, including commercial
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not claim that you wrote
 *    the original software. If you use this software in a product, an acknowledgment in the
 *    product documentation would be appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be misrepresented
 *    as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Author: Emmanuel Pinot
 * Email: manu.pinot@gmail.com
 * Year: 2024
 */


#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <jni.h>
#include <stdlib.h>
#include <errno.h>

#define LOG_MESSAGE(message) SDL_Log("Thread ID: %lu, %s", SDL_GetCurrentThreadID(), message)
#define VIDEO_WIDTH 320
#define VIDEO_HEIGHT 280

// Define a struct for handling image data and related properties
typedef struct image_s
{
    SDL_Texture* texture; // SDL texture representation of the image for rendering in SDL
    SDL_Mutex *mutex;     // Mutex for thread-safe access to the image data
    uint8_t* data;        // Pointer to the raw image data (pixel information)
    size_t length;        // Size of the image data in bytes
    int width;            // Width of the image in pixels
    int height;           // Height of the image in pixels
    float videoRatio;     // Aspect ratio of the image, used for scaling
    bool new;             // Flag to indicate if this is a new image (e.g., if it has been updated)
} cImage;


static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static int mWidth = 0;
static int mHeight = 0;
static cImage* image = NULL;
static int mOrientation = 270;
static SDL_FRect screenRect;

/**
 * @brief Frees dynamically allocated memory using a specified free function.
 *
 * This function takes a pointer to memory and a custom free function,
 * calls the free function to deallocate the memory, and then sets the
 * pointer to NULL to avoid dangling pointers.
 *
 * @param mem      Pointer to the memory that needs to be freed.
 * @param freeFunc Function pointer to the specific free function to
 *                 deallocate the memory.
 */
void free_memory(void* mem, void (*freeFunc)(void*))
{
    freeFunc(mem);  // Call the specified free function to release the memory
    mem = NULL;     // Set the pointer to NULL to prevent dangling references
}

/**
 * @brief Destroys a `cImage` object by freeing all dynamically allocated
 *        resources within the structure and then freeing the structure itself.
 *
 * This function checks each member of the `cImage` struct to ensure it is
 * not NULL before attempting to free it, to avoid double-free errors.
 * After freeing each resource, it frees the `cImage` structure itself.
 *
 * @param me Pointer to the `cImage` structure to be destroyed and freed.
 */
void cImage_Destroy(cImage* me)
{
    // Check if the cImage pointer itself is NULL; if so, exit function early
    if (me == NULL) return;

    // Free the raw image data if it exists
    if (me->data != NULL)
        free_memory(me->data, free);

    // Free the mutex if it exists, using SDL_DestroyMutex as the free function
    if (me->mutex != NULL)
        free_memory(me->mutex, (void (*)(void *)) SDL_DestroyMutex);

    // Free the texture if it exists, using SDL_DestroyTexture as the free function
    if (me->texture != NULL)
        free_memory(me->texture, (void (*)(void *)) SDL_DestroyTexture);

    // Finally, free the cImage structure itself
    free_memory(me, free);
}

/**
 * @brief Allocates and initializes a new `cImage` structure.
 *
 * This function allocates memory for a `cImage` structure and initializes
 * its mutex. If any allocation or initialization fails, it logs an error
 * message and cleans up by calling `cImage_Destroy`.
 *
 * @param addressImage Double pointer to a `cImage*` which will point to the
 *                     newly allocated `cImage` structure if successful.
 * @return `true` if the allocation and initialization succeed, `false` otherwise.
 */
bool cImage_New(cImage** addressImage)
{
    bool ret = false;  // Variable to store the return value (default to false)

    // Allocate memory for the cImage struct and initialize all fields to zero
    *addressImage = calloc(1, sizeof(cImage));

    // Check if memory allocation was successful
    if (*addressImage == NULL)
    {
        LOG_MESSAGE(strerror(errno));  // Log the error message if allocation failed
        goto EXIT;                     // Jump to cleanup on failure
    }

    // Create a mutex for the cImage instance
    (*addressImage)->mutex = SDL_CreateMutex();
    if ((*addressImage)->mutex == NULL)
    {
        LOG_MESSAGE(SDL_GetError());  // Log the error if mutex creation failed
        goto EXIT;                    // Jump to cleanup on failure
    }

    // If everything is successful, return true
    return true;

    EXIT:
    cImage_Destroy(*addressImage);  // Clean up allocated resources on failure
    return false;
}

/**
 * @brief Updates the texture of a `cImage` object if necessary.
 *
 * This function locks the `cImage`'s mutex to ensure thread-safe access, checks
 * if the texture dimensions match the current width and height, and updates or
 * recreates the texture as needed. If the image data is new, it updates the texture
 * with the latest data.
 *
 * @param me Pointer to the `cImage` structure whose texture is to be updated.
 * @return `true` if the texture is successfully updated, `false` if an error occurs.
 */
bool cImage_TextureUpdate(cImage* me)
{
    bool ret = false;  // Default return value, assuming failure

    SDL_LockMutex(me->mutex);  // Lock mutex to ensure thread-safe access to `cImage`

    // Check if the current texture dimensions differ from the desired width and height
    if (mWidth != me->width || mHeight != me->height)
    {
        // Delete the existing texture if it exists, then create a new one
        if (me->texture != NULL)
        {
            free_memory(me->texture, (void (*)(void *)) SDL_DestroyTexture);
        }

        // Create a new texture with updated width and height
        me->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_NV12, SDL_TEXTUREACCESS_STREAMING,
                                        me->width, me->height);
        if (me->texture == NULL)  // Check for texture creation failure
        {
            LOG_MESSAGE(SDL_GetError());  // Log error message if texture creation fails
            goto EXIT;                    // Exit on failure
        }

        // Update cached dimensions and calculate the aspect ratio
        mWidth = me->width;
        mHeight = me->height;
        me->videoRatio = (float)me->width / (float)me->height;
    }

    // If image data length is non-zero, update the texture with new data if available
    if (me->length != 0)
    {
        if (me->new)  // Check if the image data is marked as new
        {
            // Update the texture with the new data
            if (!SDL_UpdateTexture(me->texture, NULL, me->data, me->width))
            {
                LOG_MESSAGE(SDL_GetError());  // Log error if texture update fails
                goto EXIT;                    // Exit on failure
            }
            me->new = false;  // Reset the new flag after updating
        }
    }

    ret = true;  // Set return value to true to indicate success

    EXIT:
    SDL_UnlockMutex(me->mutex);  // Unlock the mutex before returning
    return ret;
}

/**
 * @brief Retrieves the dimensions of the render output and sets them in an SDL_FRect structure.
 *
 * This function fetches the current render output size of the window and assigns it to an
 * `SDL_FRect` structure, representing the screen rectangle. If successful, it initializes
 * the rectangle's x and y coordinates to 0, with width and height matching the window size.
 *
 * @param window_rect Pointer to an `SDL_FRect` structure where the window dimensions will be stored.
 * @return `true` if the render output size is successfully retrieved and assigned; `false` otherwise.
 */
static bool getScreenRect(SDL_FRect* window_rect)
{
    // Variables to store the window width and height
    int width, height;

    // Retrieve the render output size; log an error if retrieval fails
    if (!SDL_GetRenderOutputSize(renderer, &width, &height))
    {
        LOG_MESSAGE(SDL_GetError());  // Log the SDL error message on failure
        return false;                 // Return false to indicate failure
    }

    // Initialize the SDL_FRect structure to represent the entire window's size
    window_rect->x = 0.0f;          // Set x to 0.0 (left edge of the window)
    window_rect->y = 0.0f;          // Set y to 0.0 (top edge of the window)
    window_rect->w = (float)width;  // Assign the window's width as a float
    window_rect->h = (float)height; // Assign the window's height as a float

    return true;  // Return true to indicate success
}


/**
 * @brief Calculates and sets the dimensions and position of a rectangle to fit
 *        within a display rectangle while maintaining aspect ratio.
 *
 * This function centers a target rectangle within a given display rectangle
 * and adjusts its dimensions based on the aspect ratio and rotation.
 * For portrait orientations, it swaps width and height adjustments.
 *
 * @param rect       Pointer to the `SDL_FRect` where the calculated rectangle will be stored.
 * @param displayRect Pointer to the `SDL_FRect` defining the display area.
 * @param rotation   Rotation angle (90, 180, 270) to adjust width and height orientation.
 * @param videoRatio Aspect ratio of the video, used to maintain consistent scaling.
 */
static void calculateRect(SDL_FRect* rect, const SDL_FRect* displayRect,
                          int rotation, float videoRatio)
{
    // Find the center point of the display rectangle
    SDL_FPoint mid;
    mid.x = displayRect->x + (displayRect->w / 2);
    mid.y = displayRect->y + (displayRect->h / 2);

    // Initialize width and height based on the display rectangle's dimensions
    float adjustedWidth = displayRect->w;
    float adjustedHeight = displayRect->h;

    // Adjust dimensions to maintain the aspect ratio, with orientation consideration
    if (rotation == 90 || rotation == 270)
    {
        // For portrait orientation, adjust width based on video aspect ratio
        if (adjustedHeight > adjustedWidth * videoRatio)
        {
            adjustedWidth = adjustedHeight / videoRatio;
        }
        else
        {
            adjustedHeight = adjustedWidth * videoRatio;
        }
    }
    else
    {
        // For landscape orientation, adjust height based on video aspect ratio
        if (adjustedWidth > adjustedHeight * videoRatio)
        {
            adjustedHeight = adjustedWidth / videoRatio;
        }
        else
        {
            adjustedWidth = adjustedHeight * videoRatio;
        }
    }

    // Set the final width and height of `rect` based on rotation requirements
    if (rotation == 90 || rotation == 270)
    {
        rect->w = adjustedHeight;  // Swap width and height for portrait orientation
        rect->h = adjustedWidth;
    }
    else
    {
        rect->w = adjustedWidth;
        rect->h = adjustedHeight;
    }

    // Center the rectangle within the display rectangle
    rect->x = mid.x - (rect->w / 2);
    rect->y = mid.y - (rect->h / 2);
}

/**
 * @brief Retrieves the current display orientation and sets the appropriate
 *        rotation angle for the application.
 *
 * This function obtains the display ID for the active window and fetches the
 * current display orientation. It sets the rotation angle based on the display
 * orientation, which allows the application to adjust to the display's layout.
 *
 * @param orientation Pointer to an integer where the rotation angle will be stored:
 *                    0, 90, 180, or 270 degrees based on orientation.
 * @return `true` if the orientation was successfully retrieved and set; `false` otherwise.
 */
bool getOrientation(int* orientation)
{
    bool ret = false;  // Default return value, assuming failure

    // Get the display ID for the current window
    SDL_DisplayID windowDisplayID = SDL_GetDisplayForWindow(window);
    if (windowDisplayID == 0)  // Check if retrieving display ID failed
    {
        LOG_MESSAGE(SDL_GetError());  // Log SDL error message on failure
        goto EXIT;                    // Exit on failure
    }

    // Get the current orientation of the specified display
    SDL_DisplayOrientation currOrientation = SDL_GetCurrentDisplayOrientation(windowDisplayID);

    // Default orientation set to 270 degrees (portrait)
    *orientation = 270;

    // Adjust orientation based on the current display orientation
    switch (currOrientation)
    {
        case SDL_ORIENTATION_UNKNOWN:
        case SDL_ORIENTATION_LANDSCAPE:
            *orientation = 180;  // Set to 180 degrees for landscape
            break;
        case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
            *orientation = 0;    // Set to 0 degrees for landscape flipped
            break;
        case SDL_ORIENTATION_PORTRAIT:
            *orientation = 270;  // Portrait orientation, keep at 270 degrees
            break;
        case SDL_ORIENTATION_PORTRAIT_FLIPPED:
            *orientation = 90;   // Set to 90 degrees for portrait flipped
            break;
    }

    ret = true;  // Indicate successful orientation retrieval and assignment

    EXIT:
    return ret;  // Return the result (true if successful, false otherwise)
}

/**
 * @brief Renders the texture of a `cImage` object within a specified parent rectangle,
 *        applying the specified orientation and scaling.
 *
 * This function updates the texture of the `cImage` object if needed, calculates
 * the rendering rectangle, and renders the texture to the screen with rotation and
 * vertical flipping. It logs any errors encountered during rendering.
 *
 * @param me Pointer to the `cImage` object containing the texture to render.
 * @param parentRect Pointer to an `SDL_FRect` defining the display area for rendering.
 * @param orientation Integer specifying the rotation angle (0, 90, 180, or 270).
 * @return `true` if the texture is successfully rendered, `false` if an error occurs.
 */
bool cImage_Render(cImage* me, SDL_FRect* parentRect, int orientation)
{
    bool ret = false;  // Default return value, assuming failure

    // Update the texture of the cImage object; exit if update fails
    if (!cImage_TextureUpdate(me)) goto EXIT;

    // Calculate the rendering rectangle based on the parent rectangle and orientation
    SDL_FRect rect;
    calculateRect(&rect, parentRect, orientation, me->videoRatio);

    if (me->texture != NULL)
    {
        // Render the texture with rotation and vertical flipping
        if (!SDL_RenderTextureRotated(renderer,
                                      me->texture,
                                      NULL,
                                      &rect,
                                      orientation,
                                      0,
                                      SDL_FLIP_VERTICAL))
        {
            LOG_MESSAGE(SDL_GetError());  // Log error message if rendering fails
            goto EXIT;                    // Exit on failure
        }
    }

    ret = true;  // Set return value to true to indicate success

    EXIT:
    return ret;  // Return the result (true if successful, false otherwise)
}

/**
 * @brief Starts the camera on an Android device by calling a Java method
 *        if permission is granted.
 *
 * This function checks if the required permission has been granted. If so, it
 * retrieves the Android activity and calls the `startCameraX` Java method with
 * specified width and height parameters.
 *
 * @param userdata Pointer to user data passed to the function (unused here).
 * @param permission String representing the permission required (unused here).
 * @param granted Boolean indicating if the required permission was granted.
 */
static void JavaStartCamera(void *userdata, const char *permission, bool granted)
{
    if (granted)  // Proceed only if the permission was granted
    {
        JNIEnv *env = SDL_GetAndroidJNIEnv();  // Get the JNI environment
        jobject activity = (jobject) SDL_GetAndroidActivity();  // Get the current Android activity

        // Get the Java class for the activity
        jclass activityClass = (*env)->GetObjectClass(env, activity);

        // Find the method ID for the startCameraX method, which takes two integers as parameters
        jmethodID startCameraMethod = (*env)->GetMethodID(env, activityClass, "startCameraX", "(II)V");

        if (startCameraMethod == NULL)  // Check if the method ID was successfully retrieved
        {
            SDL_Log("Could not find startCameraX method");  // Log an error if the method is not found
            return;  // Exit the function if method ID is not found
        }

        // Call the Java startCameraX method with video width and height parameters
        (*env)->CallVoidMethod(env, activity, startCameraMethod, VIDEO_WIDTH, VIDEO_HEIGHT);
    }
}


/**
 * @brief Initializes the SDL application by requesting camera permission,
 *        setting up SDL video, and creating essential resources.
 *
 * This function is called once at startup to initialize the application.
 * It requests the necessary camera permission, initializes SDL video,
 * creates the window and renderer, and sets up image and orientation resources.
 *
 * @param appstate Pointer to an application-specific state (unused here).
 * @param argc Argument count (unused here).
 * @param argv Argument vector (unused here).
 * @return `SDL_APP_CONTINUE` if initialization is successful; `SDL_APP_FAILURE` otherwise.
 */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    // Request Android camera permission, attaching JavaStartCamera as the callback
    if (!SDL_RequestAndroidPermission("android.permission.CAMERA", JavaStartCamera, NULL))
    {
        LOG_MESSAGE(SDL_GetError());  // Log error if permission request fails
        goto EXIT;                    // Exit if permission request fails
    }

    // Initialize SDL with video subsystem
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        LOG_MESSAGE(SDL_GetError());  // Log error if SDL initialization fails
        goto EXIT;                    // Exit if initialization fails
    }

    // Create an SDL window and renderer for displaying the camera feed
    if (!SDL_CreateWindowAndRenderer("CameraXSDL3", 0, 0, SDL_WINDOW_RESIZABLE, &window, &renderer))
    {
        LOG_MESSAGE(SDL_GetError());  // Log error if window or renderer creation fails
        goto EXIT;                    // Exit if creation fails
    }

    // Initialize a new cImage structure for handling camera textures
    if (!cImage_New(&image)) goto EXIT;

    // Get the initial screen orientation and set it in mOrientation
    if (!getOrientation(&mOrientation)) goto EXIT;

    // Retrieve the screen rectangle dimensions for positioning content
    if (!getScreenRect(&screenRect)) goto EXIT;

    return SDL_APP_CONTINUE;  // Return success if all initializations complete

    EXIT:
    return SDL_APP_FAILURE;   // Return failure if any initialization step fails
}

/**
 * @brief Handles events such as quit requests and window resizing.
 *
 * This function is called whenever a new SDL event occurs. It checks for quit
 * events to end the program and responds to window resize events by updating
 * the screen orientation and dimensions.
 *
 * @param appstate Pointer to an application-specific state (unused here).
 * @param event Pointer to the `SDL_Event` structure representing the current event.
 * @return `SDL_APP_SUCCESS` if the program should exit, `SDL_APP_CONTINUE` to continue,
 *         or `SDL_APP_FAILURE` if an error occurs.
 */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    // Check if the event is a quit request
    if (event->type == SDL_EVENT_QUIT)
    {
        return SDL_APP_SUCCESS;  // End the program, reporting success to the OS
    }

    // Check if the event is a window resize
    if (event->type == SDL_EVENT_WINDOW_RESIZED)
    {
        // Update orientation if the window is resized
        if (!getOrientation(&mOrientation)) goto EXIT;

        // Update the screen rectangle dimensions based on the new window size
        if (!getScreenRect(&screenRect)) goto EXIT;
    }

    return SDL_APP_CONTINUE;  // Continue running the program

    EXIT:
    return SDL_APP_FAILURE;   // Return failure if orientation or screen rectangle update fails
}

/**
 * @brief Runs the main rendering loop for each frame, clearing the screen,
 *        rendering the image, and presenting the result.
 *
 * This function is the core of the program, responsible for rendering content
 * each frame. It clears the renderer, calls the `cImage_Render` function to
 * display the image, and then presents the rendered content to the screen.
 *
 * @param appstate Pointer to an application-specific state (unused here).
 * @return `SDL_APP_CONTINUE` if the frame renders successfully; `SDL_APP_FAILURE` if an error occurs.
 */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    // Clear the renderer to prepare for a new frame
    if (!SDL_RenderClear(renderer))
    {
        LOG_MESSAGE(SDL_GetError());  // Log error if clearing the renderer fails
        return SDL_APP_FAILURE;       // Return failure on error
    }

    // Render the cImage object within the specified screen rectangle and orientation
    if (!cImage_Render(image, &screenRect, mOrientation))
    {
        return SDL_APP_FAILURE;  // Return failure if rendering the image fails
    }

    // Present the rendered frame to the screen
    if (!SDL_RenderPresent(renderer))
    {
        LOG_MESSAGE(SDL_GetError());  // Log error if presenting the renderer fails
        return SDL_APP_FAILURE;       // Return failure on error
    }

    return SDL_APP_CONTINUE;  // Continue running the program if rendering succeeds
}

/**
 * @brief Cleans up resources and performs any necessary shutdown tasks.
 *
 * This function is called once at program shutdown to clean up resources
 * associated with the application. It specifically destroys the `cImage`
 * object, while SDL handles window and renderer cleanup automatically.
 *
 * @param appstate Pointer to an application-specific state (unused here).
 * @param result SDL_AppResult indicating the program's exit status.
 */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    // Destroy the cImage object and free associated resources
    cImage_Destroy(image);

    // Note: SDL automatically cleans up the window and renderer on exit.
}

/**
 * @brief Processes YUV image data from Java and updates the `cImage` structure.
 *
 * This function is called from Java to process YUV image data for the `cImage`
 * object. It locks the image mutex, resizes the data buffer if necessary, copies
 * the new YUV data, and sets image properties. If memory allocation fails, it
 * logs an error and exits.
 *
 * @param env Pointer to the JNI environment.
 * @param thiz Reference to the Java object calling this function.
 * @param yuv_data Byte array containing the YUV image data.
 * @param width Integer representing the width of the YUV image.
 * @param height Integer representing the height of the YUV image.
 */
JNIEXPORT void JNICALL
Java_com_example_cameraxsdl3_CameraXsdl3Activity_processYUVImage(JNIEnv *env, jobject thiz, jbyteArray yuv_data,
                                                                 jint width,
                                                                 jint height)
{
    // Lock the image mutex to ensure thread-safe access
    SDL_LockMutex(image->mutex);

    // Get the length of the YUV data byte array from Java
    jsize data_len = (*env)->GetArrayLength(env, yuv_data);

    // Check if the data buffer needs resizing based on new data length
    if (data_len > image->length)
    {
        // Update the stored length to match the new data length
        image->length = data_len;

        // Free the existing data buffer if it exists
        if (image->data != NULL)
        {
            free(image->data);
            image->data = NULL;
        }

        // Allocate a new buffer for the YUV data
        image->data = calloc(data_len, sizeof(*image->data));
        if (image->data == NULL)  // Check for memory allocation failure
        {
            LOG_MESSAGE(strerror(errno));  // Log error if allocation fails
            goto EXIT;                     // Exit if memory allocation fails
        }
    }

    // Copy the YUV data from Java byte array to the allocated image data buffer
    (*env)->GetByteArrayRegion(env, yuv_data, 0, data_len, (jbyte*) image->data);

    // Set image properties and mark the image data as new
    image->new = true;
    image->width = width;
    image->height = height;

    EXIT:
    SDL_UnlockMutex(image->mutex);  // Unlock the mutex before returning
}
