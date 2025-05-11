#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <filesystem>
#include <vector>
#include <threadpool.h>
#include <map>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

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

long __attribute__ ((noinline)) mapPixelHilbert(long byteIndex, long sidePow) {
    long pixelIndex = byteIndex;
    long channel = byteIndex % 3;
    long mappedPixel = mapHilbert(sidePow, pixelIndex);
    return mappedPixel + channel;
}

struct pixel {
    char r;
    char g;
    char b;
};

void encodeToFile(FILE* inputFile, long outputFile) {
    
    //set up on demand buffered reader
    fseek(inputFile, 0, SEEK_END);
    long inputLen = ftell(inputFile);
    rewind(inputFile);
    printf("parsing %ld bytes of data\n", inputLen);

    //compute image size
    long pixelCount = (inputLen / sizeof(pixel));
    long sqrtPixels = std::sqrt(pixelCount);
    long sidePow = (long)(std::ceil(std::log2(sqrtPixels)));
    long side = 1 << sidePow;
    long area = side * side * sizeof(pixel);
    std::cout << "data stored: " << area << "\npixels: " << side * side << "\ndimensions: " << side << "x" << side << "\n";

    //innit output vector
    auto grid = (pixel*)mmap(NULL, area, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    auto data = (pixel*)mmap(NULL, area, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    
    fread(data, 1, inputLen, inputFile);

    //fun part :)
    ThreadPool pool(std::thread::hardware_concurrency());
    std::vector<std::future<void>> tasks;
    for (long i = 0; i < pixelCount/side; ++i) {
        tasks.emplace_back(pool.addTask([i, side, sidePow, &grid, &data](){
            for (long j = i * side; j < (i+1) * side; j++) {
                long mappedIndex = mapPixelHilbert(j, sidePow);
                grid[mappedIndex] = data[j];
            }
        }));
    }
    for (long i = 0; i < tasks.size(); i++) {tasks[i].get();};

    fclose(inputFile);

    //write to PPM
    std::cout << "writing to file...\n";
    dprintf(outputFile, "P6\n%ld %ld\n255\n", side, side);
    size_t written = 0;
    while (written < area) {
        written += write(outputFile, grid + written/sizeof(pixel), area - written);
    }
    close(outputFile);
}

int main(int argc, char* argv[]) {
    std::cout << std::filesystem::current_path() << std::endl;

    if (argc > 1) {
        printf("input file: %s\n", argv[1]);
    } else {
        puts("please provide input file.\n");
        return 1;
    }
    FILE* inputFile = fopen(argv[1], "r");
    if (argc > 2) {
        printf("output file: %s\n", argv[2]);
    } else {
        puts("please provide output file. (extension must be .ppm)\n");
        return 1;
    }
    long outputFile = open(argv[2], O_WRONLY | O_CREAT, 0644);

    encodeToFile(inputFile, outputFile);

    std::cout << "done!\n";
}
