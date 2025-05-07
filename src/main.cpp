#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <filesystem>
#include <vector>

std::vector<char> readBytes(std::string inputFile) {
    std::ifstream input(inputFile, std::ios::binary|std::ios::ate);
    std::ifstream::pos_type pos = input.tellg();
    if (pos == 0) {
        return std::vector<char>{};
    }
    std::vector<char> result(pos);
    input.seekg(0, std::ios::beg);
    input.read(&result[0], pos);

    return result;
}

struct pixel {
    char r;
    char g;
    char b;
};

// Function to map linear pixel data to Hilbert curve order
struct Vec2 {
    long x;
    long y;
};

Vec2 hilbert(long i, long order) {
    const std::vector<Vec2> points = {
        {0, 0},
        {0, 1},
        {1, 1},
        {1, 0}
    };

    Vec2 v = points[i & 3];

    for (long j = 1; j < order; j++) {
        i = i >> 2; // Equivalent to i / 4
        long index = i & 3;
        long len = std::pow(2, j);
        if (index == 0) {
            std::swap(v.x, v.y);
        } else if (index == 1) {
            v.y += len;
        } else if (index == 2) {
            v.x += len;
            v.y += len;
        } else if (index == 3) {
            long temp = len - 1 - v.x;
            v.x = len - 1 - v.y;
            v.y = temp;
            v.x += len;
        }
    }
    return v;
}

std::vector<pixel> mapLinearToHilbert(const std::vector<pixel>& pixMap, long width, long height) {
    long order = std::ceil(std::log2(width)); // Determine the order based on the larger dimension
    long size = pixMap.size();
    std::vector<pixel> hilbMap(size);

    for (long i = 0; i < size; i++) {
        Vec2 v = hilbert(i, order);
        long hilbertIndex = v.y * width + v.x; // Calculate the linear index from the 2D coordinates
        hilbMap[hilbertIndex] = pixMap[i];
        if (i % 100000 == 0)
        std::cout << "\r" << (float)i / (size)* 100 << "%                                  ";
    }   std::cout << std::endl;
    return hilbMap;
}

int main(int, char**) {
    std::cout << std::filesystem::current_path() << std::endl;
    std::string inputFile = "../resources/input";
    std::string outputFile = "../resources/output.ppm";
    std::ofstream output(outputFile, std::ios::binary);
    
    std::cout << "reading file...\n";
    
    std::vector<char> data = readBytes(inputFile);
    
    std::cout << "file successfully read. parsing file...\n";
    std::cout << data.size() << std::endl;

    long  pixCount = data.size()/3;
    //round side lengths up to the nearest power of 2 
    long width = std::pow(2, std::ceil(std::log2(std::sqrt(pixCount))));
    long height = width;
    std::cout << pixCount << " pixels, " << width << "x" << height << std::endl;
    
    //append data to make the PPM file valid
    std::cout << "padding data...\n";
    std::cout << "allocating " << width * height * 3 / 1024/1024 << "MiB" << std::endl;
    data.reserve(width * height * 3);
    while (data.size() < width * height * 3) {
        data.push_back(0);
        if (data.size() % 10000000 == 0)
            std::cout << "\r" << (float)data.size() / (width * height * 3)* 100 << "%                                  ";
    }
    std::cout << std::endl;
    
    std::cout << "mapping to curve...\n";
    std::vector<pixel> pixMap;
    for (long i = 0; i < data.size(); i += 3) {
        pixMap.push_back({data[i], data[i+1], data[i+2]});
    }

    std::vector<pixel> hilbMap(width * height);
    hilbMap = mapLinearToHilbert(pixMap, width, height);
    //hilbMap = pixMap;
    std::cout << hilbMap.size() << std::endl;

    while (hilbMap.size() < width * height) {
        hilbMap.push_back({'0', '0','0'});
    }
    
    //write to PPM
    std::cout << "writing to file...\n";
    output << "P6\n" << width << " " << height << "\n255\n";
    for (long i = 0; i < hilbMap.size(); i++) {
        output << hilbMap[i].r << hilbMap[i].g << hilbMap[i].b;
    }
    std::cout << "done!\n";
}
