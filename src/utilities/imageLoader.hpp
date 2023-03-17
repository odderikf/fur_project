#pragma once

#include <lodepng.h>
#include <vector>
#include <string>
#include <glad/glad.h>

typedef struct PNGImage {
	GLsizei width;
	GLsizei height;
	std::vector<unsigned char> pixels;
} PNGImage;

PNGImage loadPNGFile(std::string fileName);
