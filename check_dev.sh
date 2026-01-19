#!/bin/sh
# Usage: ./check_dev.sh <package_name> <header_file>
# Example: ./check_dev.sh libglm-dev glm/glm.hpp

PKG=$1
HDR=$2

if [ -z "$PKG" ] || [ -z "$HDR" ]; then
    echo "Usage: $0 <package_name> <header_name>"
    exit 1
fi

echo "--- Checking Dependency: $PKG ($HDR) ---"

# 1. System Package Check
check_pkg() {
    if command -v dpkg >/dev/null 2>&1; then
        dpkg -l "$PKG" 2>/dev/null | grep -q "^ii"
    elif command -v brew >/dev/null 2>&1; then
        brew list "$PKG" >/dev/null 2>&1
    elif command -v rpm >/dev/null 2>&1; then
        rpm -q "$PKG" >/dev/null 2>&1
    else
        return 1
    fi
}

#Direct Code Injection Method 

cat << 'EOF' > glm_test.cpp
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
EOF

g++ glm_test.cpp -o glm_test_bin && ./glm_test_bin && rm glm_test_bin

if check_pkg; then
    echo "[✔] Package '$PKG' is registered in the system."

else
    echo "[✘] Package '$PKG' NOT found in package manager."
fi

# 2. Compiler Header Check (The ultimate test)
# This asks the compiler directly if it can find the header in its search path.
echo "#include <$HDR>" | g++ -x c++ -E - > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "[✔] Header <$HDR> is accessible to the compiler."
else
    echo "[✘] Header <$HDR> is NOT found in compiler include paths."
    
    # Optional: Locate where it might be
    LOC=$(locate -b "\\$HDR" 2>/dev/null | head -n 1)
    if [ -n "$LOC" ]; then
        echo "    Note: Found a match at $LOC. You may need to add -I to your g++ command."
    fi
fi
