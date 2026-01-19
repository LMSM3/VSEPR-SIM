#include <memory>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream> // Need iostream for output

int main() {
    // Example using GLM to create a simple translation matrix
    glm::mat4 trans = glm::mat4(1.0f);
    trans = glm::translate(trans, glm::vec3(1.0f, 0.0f, 0.0f));

    std::cout << "Translation Matrix (first column): " << trans[0].x << ", " << trans[0].y << ", " << trans[0].z << ", " << trans[0].w << std::endl;

    // Example using a standard library container
    std::vector<int> myVec = {1, 2, 3};
    std::cout << "Vector size: " << myVec.size() << std::endl;

    return 0;
}
