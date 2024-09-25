#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>

// Rotation angles
float rotationAngleAroundXAxis = 0;
float rotationAngleAroundYAxis = 0;
float rotationAngleAroundZAxis = 0;

float halfCubeWidth = 10;
int consoleScreenBufferWidth = 160, consoleScreenBufferHeight = 44;

// Default to cube
char currentShape = '1';  

// Z-buffer for depth handling
float zBuffer[160 * 44];

// ASCII buffer for rendering
char buffer[160 * 44];

int backgroundASCIICode = ' ';
int distanceFromCam = 60;

// Scaling constant for 3D to 2D projection
float K1 = 40; 

// Reduced speed for finer shapes
float incrementSpeed = 0.1; 

// Default speed of rotation for 3D shape
float rotationSpeed = 0.25;

float x, y, z, ooz;
int xp, yp, idx;

// ASCII characters for shading based on depth (wider range for more detailed rendering)
const char shadingCharacters[] = " .:-=+*#%@";

// ASCII characters for each face of the cube
const char cubeFaceChar[6] = {'#', '@', '%', '*', '+', '='};

// Function to set the terminal in raw mode to capture input without waiting for Enter
void setTerminalRawMode(int enable) {
    static struct termios oldt, newt;
    if (enable) {
        // Save the terminal settings
        tcgetattr(STDIN_FILENO, &oldt); 

        newt = oldt;

        // Disable canonical mode (line buffering) and echo
        newt.c_lflag &= ~(ICANON | ECHO); 

        // Apply the new settings
        tcsetattr(STDIN_FILENO, TCSANOW, &newt); 
    } else {
        // Restore the old settings
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt); 
    }
}

// Function to check if a key is pressed without blocking
int kbhit() {
    // No wait time
    struct timeval tv = {0L, 0L};

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
}

// Proper rotation matrix calculation for 3D points
void rotate3D(float *x, float *y, float *z) {
    float tempY, tempZ, tempX;

    // Rotation around X-axis
    tempY = *y;
    tempZ = *z;
    *y = tempY * cos(rotationAngleAroundXAxis) - tempZ * sin(rotationAngleAroundXAxis);
    *z = tempY * sin(rotationAngleAroundXAxis) + tempZ * cos(rotationAngleAroundXAxis);

    // Rotation around Y-axis
    tempX = *x;
    tempZ = *z;
    *x = tempX * cos(rotationAngleAroundYAxis) + tempZ * sin(rotationAngleAroundYAxis);
    *z = -tempX * sin(rotationAngleAroundYAxis) + tempZ * cos(rotationAngleAroundYAxis);

    // Rotation around Z-axis
    tempX = *x;
    tempY = *y;
    *x = tempX * cos(rotationAngleAroundZAxis) - tempY * sin(rotationAngleAroundZAxis);
    *y = tempX * sin(rotationAngleAroundZAxis) + tempY * cos(rotationAngleAroundZAxis);
}


char calculateShading(float zValue) {
    // Adjust depth value based on distance to camera
    float depthRange = distanceFromCam + halfCubeWidth * 2;
    // Use full shading character range
    int shadeIndex = (int)((zValue / depthRange) * (strlen(shadingCharacters) - 1)); 

    if (shadeIndex < 0) shadeIndex = 0;
    if (shadeIndex >= (int)strlen(shadingCharacters)) shadeIndex = strlen(shadingCharacters) - 1;
    return shadingCharacters[shadeIndex];
}

// Function to calculate surface projection and apply shading with custom character for cube faces
void calculateForSurfaceWithCustomChar(float shapeX, float shapeY, float shapeZ, char faceChar) {
    x = shapeX;
    y = shapeY;
    z = shapeZ;

    // Apply rotation
    rotate3D(&x, &y, &z);

    // Move the shape into the screen
    z += distanceFromCam;

    // 3D to 2D projection
    ooz = 1 / z;

    xp = (int)(consoleScreenBufferWidth / 2 + K1 * ooz * x * 2);
    yp = (int)(consoleScreenBufferHeight / 2 + K1 * ooz * y);

    // Check bounds to avoid glitches
    if (xp >= 0 && xp < consoleScreenBufferWidth && yp >= 0 && yp < consoleScreenBufferHeight) {
        idx = xp + yp * consoleScreenBufferWidth;
        if (ooz > zBuffer[idx]) {
            zBuffer[idx] = ooz;
            buffer[idx] = faceChar;  // Use custom ASCII character for each face
        }
    }
}

// Function to draw the cube with different characters on each face
void drawCube(float halfCubeWidth) {
    for(float cubeX = -halfCubeWidth; cubeX < halfCubeWidth; cubeX += incrementSpeed) {
        for (float cubeY = -halfCubeWidth; cubeY < halfCubeWidth; cubeY += incrementSpeed) {
            // Front face (@)
            calculateForSurfaceWithCustomChar(cubeX, cubeY, -halfCubeWidth, cubeFaceChar
        [0]);
            // Back face (%)
            calculateForSurfaceWithCustomChar(-cubeX, cubeY, halfCubeWidth, cubeFaceChar
        [1]);
            
            // Left face (*)
            calculateForSurfaceWithCustomChar(-halfCubeWidth, cubeY, cubeX, cubeFaceChar
        [2]);
            // Right face (+)
            calculateForSurfaceWithCustomChar(halfCubeWidth, cubeY, -cubeX, cubeFaceChar
        [3]);
            
            // Top face (#)
            calculateForSurfaceWithCustomChar(cubeX, halfCubeWidth, cubeY, cubeFaceChar
        [4]);
            // Bottom face (=)
            calculateForSurfaceWithCustomChar(cubeX, -halfCubeWidth, -cubeY, cubeFaceChar
        [5]);
        }
    }
}

void drawSphere(float radius) {
    for (float theta = 0; theta < 2 * M_PI; theta += incrementSpeed) {
        for (float phi = 0; phi < M_PI; phi += incrementSpeed) {
            // Calculate 3D coordinates of the sphere surface
            float sphereX = radius * sin(phi) * cos(theta);
            float sphereY = radius * sin(phi) * sin(theta);
            float sphereZ = radius * cos(phi);
            
            // Project the 3D points into the 2D space
            calculateForSurfaceWithCustomChar(sphereX, sphereY, sphereZ, shadingCharacters[4]);
        }
    }
}

// Function to draw the triangular faces of the pyramid
void drawPyramid(float height, float baseHalfWidth) {
    // Define vertices
    float vertices[5][3] = {
        {0, height, 0},  // Apex of the pyramid

        // Base vertices
        {-baseHalfWidth, 0, -baseHalfWidth},
        {baseHalfWidth, 0, -baseHalfWidth},
        {baseHalfWidth, 0, baseHalfWidth},
        {-baseHalfWidth, 0, baseHalfWidth}
    };

    // Draw the four triangular faces of the pyramid
    for (int i = 1; i <= 4; ++i) {
        int nextIndex = (i % 4) + 1;
        
        // Fill triangular faces by interpolating between apex and base edges
        for (float t1 = 0; t1 <= 1; t1 += incrementSpeed) {
            for (float t2 = 0; t2 <= 1 - t1; t2 += incrementSpeed) {
                float faceX = vertices[0][0] * (1 - t1 - t2) + vertices[i][0] * t1 + vertices[nextIndex][0] * t2;
                float faceY = vertices[0][1] * (1 - t1 - t2) + vertices[i][1] * t1 + vertices[nextIndex][1] * t2;
                float faceZ = vertices[0][2] * (1 - t1 - t2) + vertices[i][2] * t1 + vertices[nextIndex][2] * t2;
                calculateForSurfaceWithCustomChar(faceX, faceY, faceZ, shadingCharacters[4]);
            }
        }
    }

    // Draw the base of the pyramid
    for (int i = 1; i <= 4; ++i) {
        int nextIndex = (i % 4) + 1;
        for (float t = 0; t <= 1; t += incrementSpeed) {
            float baseX = vertices[i][0] * (1 - t) + vertices[nextIndex][0] * t;
            float baseY = vertices[i][1] * (1 - t) + vertices[nextIndex][1] * t;
            float baseZ = vertices[i][2] * (1 - t) + vertices[nextIndex][2] * t;
            calculateForSurfaceWithCustomChar(baseX, baseY, baseZ, shadingCharacters[3]);
        }
    }
}

// Function to display the main menu
void displayMenu() {
    // Clear screen
    printf("\x1b[2J");

    // Move cursor to home position
    printf("\x1b[H");  

    printf("===== Shape Rotation Program =====\n");
    printf("Choose a shape to rotate:\n");
    printf("1. Cube\n");
    printf("2. Sphere\n");
    printf("3. Pyramid\n");
    printf("q. Quit\n");
    printf("Enter your choice: ");
}

void handleShapeRotation() {
    char ch;
    while (1) {
        // Clear buffers
        memset(buffer, backgroundASCIICode, consoleScreenBufferWidth * consoleScreenBufferHeight);
        memset(zBuffer, 0, consoleScreenBufferWidth * consoleScreenBufferHeight * sizeof(float));

        // Draw the selected shape
        if (currentShape == '1') {
            drawCube(halfCubeWidth);
        } else if (currentShape == '2') {
             // Use halfCubeWidth as radius
            drawSphere(halfCubeWidth);
        } else if (currentShape == '3') {
            // Use halfCubeWidth as pyramid size
            drawPyramid(halfCubeWidth * 1.5, halfCubeWidth);  
        }

        // Move the cursor to the top-left corner
        printf("\x1b[H");

        // Print the buffer to the console without clearing the screen
        for (int k = 0; k < consoleScreenBufferWidth * consoleScreenBufferHeight; k++) {
            // Newline after each row
            putchar(k % consoleScreenBufferWidth ? buffer[k] : 10);  
        }

        // Flush the buffer to force immediate screen update
        fflush(stdout);

        // Display controls
        printf("\nControls:\n");
        printf("  Up Arrow: Rotate along X axis\n");
        printf("  Down Arrow: Rotate along X axis (opposite)\n");
        printf("  Left Arrow: Rotate along Y axis\n");
        printf("  Right Arrow: Rotate along Y axis (opposite)\n");
        printf("  '.' Decrease rotation speed\n");
        printf("  '/' Increase rotation speed\n");
        printf("  'm': Back to Menu\n");
        printf("  'q' to Quit.\n");

        // Check for key inputs
        while (kbhit()) {
            ch = getchar();
            // Escape sequence for arrow keys
            if (ch == '\x1b') {  
                // Skip the '['
                getchar();  

                // Capture the actual arrow key
                switch (getchar()) {  
                    // Up arrow
                    case 'A':  
                        rotationAngleAroundXAxis -= rotationSpeed;
                        break;
                    // Down arrow
                    case 'B':  
                        rotationAngleAroundXAxis += rotationSpeed;
                        break;
                    // Right arrow
                    case 'C':  
                        rotationAngleAroundYAxis += rotationSpeed;
                        break;
                    // Left arrow
                    case 'D':  
                        rotationAngleAroundYAxis -= rotationSpeed;
                        break;
                }
             // Decrease speed
            } else if (ch == '.') {  
                if (rotationSpeed > 0.1) {
                    rotationSpeed -= 0.1;
                }

            // Increase speed
            } else if (ch == '/') {  
                rotationSpeed += 0.1;

            // Go back to the menu
            } else if (ch == 'm') {  
                return;

            // Quit the program
            } else if (ch == 'q') {  
                // Restore terminal settings
                setTerminalRawMode(0);  

                printf("\nProgram exited.\n");
                exit(0);
            }
        }

        // Adding a short delay for smoother animation
        usleep(16000);  
    }
}

int main() {
    char choice;

    // Set terminal to raw mode to capture keypresses without waiting for Enter
    setTerminalRawMode(1);

    while(1) {
        displayMenu();
        choice = getchar();

        if (choice == '1' || choice == '2' || choice == '3') {
            // Set the shape based on user input
            currentShape = choice;  

            // Start rotating the selected shape
            handleShapeRotation();  
        } else if (choice == 'q') {
            // Restore terminal settings
            setTerminalRawMode(0);  
            
            printf("\nProgram exited.\n");
            return 0;
        }
    }

    // Restore terminal settings
    setTerminalRawMode(0);
    return 0;
}
