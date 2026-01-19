#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <iomanip>

int main() {
    // 1. Initialize a vector
    glm::vec4 position(1.0f, 0.0f, 0.0f, 1.0f);
    
    // 2. Create a translation matrix (move 5 units on X, 2 on Y)
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 2.0f, 0.0f));
    
    // 3. Apply transformation
    glm::vec4 transformed = translation * position;

    std::cout << "--- GLM Injector Test (2026) ---" << std::endl;
    std::cout << "Original: (1, 0, 0)" << std::endl;
    std::cout << "Translated: (" << transformed.x << ", " << transformed.y << ", " << transformed.z << ")" << std::endl;

    if (transformed.x == 6.0f && transformed.y == 2.0f) {
        std::cout << "[✔] SUCCESS: GLM math verified." << std::endl;
    } else {
        std::cout << "[✘] FAILURE: Math mismatch." << std::endl;
    }
    return 0;
}
