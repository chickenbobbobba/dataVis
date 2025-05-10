#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <filesystem>
#include <vector>
#include <threadpool.h>
#include <map>

void readBytes(std::string inputFile, std::vector<char>& data) {
    std::ifstream input(inputFile, std::ios::binary);
    input.seekg(0, std::ios::end);
    std::ifstream::pos_type size = input.tellg();
    if (size == 0) {
        return;
    }
    std::vector<char> result(size);
    input.seekg(0, std::ios::beg);
    input.read(&result[0], size);
    
    data.insert(data.end(), result.begin(), result.end());
}

class fileGroup {
    public:
    std::vector<char> readNext(size_t quantity);
    size_t getFileSize(std::string filePath);
    std::vector<char> readFile(std::string filePath, size_t begin, size_t end);
    void addFile();

    private:
    long fileRelativeIndex = 0;

    std::map<std::string, std::vector<char>> dataMap;
};

void loadData(std::string input, std::vector<char>& data, long depth = 0) {
    depth++;
    if (std::filesystem::is_directory(input)) {
        for(const auto& entry: std::filesystem::recursive_directory_iterator(input)) {
            loadData(entry.path(), data, depth);
        }
    } else {
        if (std::filesystem::file_size(input) != 0) {
            readBytes(input, data);
            std::cout << "# depth - dir | " << depth << " - " << input << "\n";
        }
    }
    depth--;
}

long mapHilbert(long sidePow, long index) {
    long x = 0;
    long y = 0;
    long t = index;
    long s = 1;

    for (long i = 0; i < sidePow; i++) {
        long rx = 1 & (t / 2);
        long ry = 1 & (t ^ rx);

        if (ry == 0) {
            if (rx == 1) {
                x = s - 1 - x;
                y =  s - 1 - y;
            }

            long temp = x;
            x = y;
            y = temp;
        }

        x += s * rx;
        y += s * ry;
        t /= 4;
        s *= 2;
    }

    return x + (y << sidePow);
}

long /*__attribute__ ((noinline)) */mapPixelHilbert(long byteIndex, long sidePow) {
    long pixelIndex = byteIndex / 3;
    long channel = byteIndex % 3;
    long mappedPixel = mapHilbert(sidePow, pixelIndex);
    return mappedPixel * 3 + channel;
}

int main(int argc, char* argv[]) {
    std::cout << std::filesystem::current_path() << std::endl;

    std::string inputFile;
    std::string outputFile;
    if (argc > 1) {
        inputFile = argv[1];
        std::cout << "input file: " << inputFile << "\n";
    } else {
        std::cout << "please provide input file.\n";
        return 1;
    }
    if (argc > 2) {
        outputFile = argv[2];
        std::cout << "output file: " << outputFile << "\n";
    } else {
        std::cout << "please provide output file. (extension must be .ppm)\n";
        return 1;
    }
    std::ofstream output(outputFile.c_str(), std::ios::binary);
    std::vector<char> data;

    //set up on demand buffered reader
    std::FILE* file = std::fopen(inputFile.c_str(), "r");
    fseek(file, 0, SEEK_END);
    size_t inputLen = ftell(file);
    fseek(file, 0, SEEK_SET);

    //compute image size, including end byte mark
    long pixel_count = (inputLen / 3) + 1;
    long sqrtPixels = std::sqrt(pixel_count);
    long sidePow = (long)(std::ceil(std::log2(sqrtPixels)));
    long side = 1 << sidePow;
    long area = side * side;

    //innit output vector
    std::vector<char> grid(3 * area);
    
    //fun part :)
    for (size_t i = 0; i < inputLen; ++i) {
        long mappedIndex = mapPixelHilbert(i, sidePow);
        char a;
        fread(&a, 1, 1, file);
        grid[mappedIndex] = a; 
    }

    fclose(file);

    //write to PPM
    std::cout << "writing to file...\n";
    output << "P6\n" << side << " " << side << "\n255\n";

    for (auto i : grid) output << i;

    std::cout << "done!\n";
}
