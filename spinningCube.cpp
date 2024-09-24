// Spinning Shapes by Ricardo Zapata
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>

// Cube rotation angles
float rotationAngleAroundXAxis = 0;
float rotationAngleAroundYAxis = 0;
float rotationAngleAroundZAxis = 0;
float halfCubeWidth = 10;
int consoleScreenBufferWidth = 160, consoleScreenBufferHeight = 44;

// Z-buffer for depth handling
float zBuffer[160 * 44];

// ASCII buffer for cube rendering
char buffer[160 * 44];

int backgroundASCIICode = ' ';
int distanceFromCam = 60;
float K1 = 40; // Scaling constant for 3D to 2D projection
float incrementSpeed = 0.6;

float x, y, z, ooz;
int xp, yp, idx;

// Function to set the terminal in raw mode to capture input without waiting for Enter
void setTerminalRawMode(int enable) {
    static struct termios oldt, newt;
    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt); // Save the terminal settings
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode (line buffering) and echo
        tcsetattr(STDIN_FILENO, TCSANOW, &newt); // Apply the new settings
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // Restore the old settings
    }
}

// Function to check if a key is pressed without blocking
int kbhit() {
    struct timeval tv = {0L, 0L};  // No wait time
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

void calculateForSurface(float cubeX, float cubeY, float cubeZ, char ch) {
    x = cubeX;
    y = cubeY;
    z = cubeZ;

    // Apply rotation
    rotate3D(&x, &y, &z);

    // Move the cube into the screen
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
            buffer[idx] = ch;
        }
    }
}

// Function to draw a cube with six surfaces
void drawCube(float halfCubeWidth) {
    for(float cubeX = -halfCubeWidth; cubeX < halfCubeWidth; cubeX += incrementSpeed) {
        for (float cubeY = -halfCubeWidth; cubeY < halfCubeWidth; cubeY += incrementSpeed) {
            calculateForSurface(cubeX, cubeY, -halfCubeWidth, '.');  // Front face
            calculateForSurface(halfCubeWidth, cubeY, cubeX, '#');  // Right face
            calculateForSurface(-halfCubeWidth, cubeY, -cubeX, '*');  // Left face
            calculateForSurface(-cubeX, cubeY, halfCubeWidth, '$');  // Back face
            calculateForSurface(cubeX, -halfCubeWidth, -cubeY, '&');  // Bottom face
            calculateForSurface(cubeX, halfCubeWidth, cubeY, 'X');  // Top face
        }
    }
}

// Function to display the main menu
void displayMenu() {
    printf("\x1b[2J");  // Clear screen
    printf("\x1b[H");   // Move cursor to home position
    printf("===== Shape Rotation Program =====\n");
    printf("Choose a shape to rotate:\n");
    printf("1. Cube\n");
    printf("q. Quit\n");
    printf("Enter your choice: ");
}

// Function to handle the shape rotation
void handleShapeRotation() {
    char ch;
    while(1) {
        // Clear buffers
        memset(buffer, backgroundASCIICode, consoleScreenBufferWidth * consoleScreenBufferHeight);
        memset(zBuffer, 0, consoleScreenBufferWidth * consoleScreenBufferHeight * sizeof(float));

        // Draw the cube (more shapes can be added later)
        drawCube(halfCubeWidth);

        // Print the buffer to the console
        printf("\x1b[H");
        for (int k = 0; k < consoleScreenBufferWidth * consoleScreenBufferHeight; k++) {
            putchar(k % consoleScreenBufferWidth ? buffer[k] : 10);  // Newline after each row
        }

        printf("\nPress 'm' for Menu, 'q' to Quit.\n");

        // Check for key inputs
        while (kbhit()) {
            ch = getchar();
            if (ch == '\x1b') {  // Escape sequence for arrow keys
                getchar();  // Skip the '['
                switch(getchar()) {  // Capture the actual arrow key
                    case 'A':  // Up arrow
                        rotationAngleAroundXAxis -= 0.1;
                        break;
                    case 'B':  // Down arrow
                        rotationAngleAroundXAxis += 0.1;
                        break;
                    case 'C':  // Right arrow
                        rotationAngleAroundYAxis += 0.1;
                        break;
                    case 'D':  // Left arrow
                        rotationAngleAroundYAxis -= 0.1;
                        break;
                }
            } else if (ch == 'm') {  // Go back to the menu
                return;
            } else if (ch == 'q') {  // Quit the program
                setTerminalRawMode(0);  // Restore terminal settings
                printf("\nProgram exited.\n");
                exit(0);
            }
        }

        // Add a short delay for smoother animation
        usleep(2000);
    }
}

int main() {
    char choice;

    // Set terminal to raw mode to capture keypresses without waiting for Enter
    setTerminalRawMode(1);

    while(1) {
        displayMenu();
        choice = getchar();

        if (choice == '1') {
            handleShapeRotation();  // Start rotating the cube
        } else if (choice == 'q') {
            setTerminalRawMode(0);  // Restore terminal settings
            printf("\nProgram exited.\n");
            return 0;
        }
    }

    // Restore terminal settings
    setTerminalRawMode(0);
    return 0;
}
