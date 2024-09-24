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

// Flags to track keypresses for simultaneous movements
int upPressed = 0, downPressed = 0, leftPressed = 0, rightPressed = 0;

// Function to set the terminal in raw mode to capture input without waiting for Enter
void setTerminalRawMode(int enable) {
    static struct termios oldt, newt;
    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt); // Save the terminal settings
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode (line buffering) and echo
        newt.c_cc[VMIN] = 0; // Non-blocking mode, no minimum characters for read()
        newt.c_cc[VTIME] = 0; // No timeout for read()
        tcsetattr(STDIN_FILENO, TCSANOW, &newt); // Apply the new settings
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // Restore the old settings
    }
}

// Function to check for key presses (arrow keys and 'q' for quit)
void handleInput() {
    char ch[3];  // To capture escape sequences (like arrow keys)
    int nread = read(STDIN_FILENO, ch, sizeof(ch));

    if (nread == 0) return;  // No input, just return

    if (ch[0] == '\x1b' && ch[1] == '[') {  // Escape sequence detected
        switch(ch[2]) {
            case 'A':  // Up arrow
                upPressed = 1;
                break;
            case 'B':  // Down arrow
                downPressed = 1;
                break;
            case 'C':  // Right arrow
                rightPressed = 1;
                break;
            case 'D':  // Left arrow
                leftPressed = 1;
                break;
        }
    } else if (ch[0] == 'q') {  // Quit the program if 'q' is pressed
        setTerminalRawMode(0);  // Restore terminal settings
        printf("\nProgram exited.\n");
        exit(0);
    }
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

// Function to display instructions
void displayInstructions() {
    printf("\n\nControls:\n");
    printf("  Use arrow keys to rotate the cube:\n");
    printf("    - Up Arrow:    Rotate upwards (X-axis)\n");
    printf("    - Down Arrow:  Rotate downwards (X-axis)\n");
    printf("    - Left Arrow:  Rotate left (Y-axis)\n");
    printf("    - Right Arrow: Rotate right (Y-axis)\n");
    printf("  Press 'q' to quit the program.\n");
}

// Function to apply movement based on keypress flags
void applyMovement() {
    if (upPressed) rotationAngleAroundXAxis -= 0.05;
    if (downPressed) rotationAngleAroundXAxis += 0.05;
    if (leftPressed) rotationAngleAroundYAxis -= 0.05;
    if (rightPressed) rotationAngleAroundYAxis += 0.05;
}

int main() {
    // Set terminal to raw mode to capture keypresses without waiting for Enter
    setTerminalRawMode(1);

    // Clear the screen
    printf("\x1b[2J");

    // Initial stationary cube
    while(1) {
        // Clear buffers
        memset(buffer, backgroundASCIICode, consoleScreenBufferWidth * consoleScreenBufferHeight);
        memset(zBuffer, 0, consoleScreenBufferWidth * consoleScreenBufferHeight * sizeof(float));

        // Handle input and update flags
        handleInput();

        // Apply rotations based on keypress flags
        applyMovement();

        // Draw the cube
        drawCube(halfCubeWidth);

        // Print the buffer to the console
        printf("\x1b[H");
        for (int k = 0; k < consoleScreenBufferWidth * consoleScreenBufferHeight; k++) {
            putchar(k % consoleScreenBufferWidth ? buffer[k] : 10);  // Newline after each row
        }

        // Display the controls below the cube
        displayInstructions();

        // Add a short delay for smoother animation
        usleep(5000);
    }

    // Restore terminal settings
    setTerminalRawMode(0);

    return 0;
}
