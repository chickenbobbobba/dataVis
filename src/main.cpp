#include <cstddef>
#include <cstdio>
#include <exception>
#include <iostream>
#include <fstream>
#include <ostream>
#include <string>
#include <cmath>
#include <filesystem>
#include <vector>
#include <threadpool.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

struct pixel {
    char r;
    char g;
    char b;
};

void loadData(std::string input, std::vector<pixel>& data, long& bytesWritten);

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

long mapPixelHilbert(long byteIndex, long sidePow) {
    long pixelIndex = byteIndex;
    long channel = byteIndex % 3;
    long mappedPixel = mapHilbert(sidePow, pixelIndex);
    return mappedPixel + channel;
}

void encodeToFile(std::string inputDir, long outputFile) {
    std::vector<pixel> data;
    long bytesWritten = 0;
    loadData(inputDir, data, bytesWritten);
    
    long inputLen = data.size();
    std::cout << "parsing " << inputLen * 3 << " bytes of data" << std::endl;

    //compute image size
    long pixelCount = (inputLen);
    long sqrtPixels = std::sqrt(pixelCount);
    long sidePow = (long)(std::ceil(std::log2(sqrtPixels)));
    long side = 1 << sidePow;
    long area = side * side * sizeof(pixel);
    //data.resize(area);
    std::cout << "data stored: " << area/1024/1024 << " MiB\npixels: " << side * side << "\ndimensions: " << side << "x" << side << std::endl;

    //innit output vector
    std::cout << "allocating output vector of size " << area/1024/1024 << " MiB" << std::endl;
    auto grid = (pixel*)mmap(NULL, area, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    
    //fun part :)
    std::cout << "mapping..." << std::endl;
    ThreadPool pool(std::thread::hardware_concurrency());
    std::vector<std::future<void>> tasks;
    for (long i = 0; i < pixelCount/side; ++i) {
        tasks.emplace_back(pool.addTask([i, side, sidePow, area, &grid, &data](){
            for (long j = i * side; j < (i+1) * side; j++) {
                long mappedIndex = mapPixelHilbert(j, sidePow);
                if (mappedIndex < area/3) {
                    grid[mappedIndex] = data[j];
                } else {
                    std::cout << "index " << mappedIndex << " out of bounds for area " << area/3 << "\n";
                }
            }
        }));
    }
    for (long i = 0; i < tasks.size(); i++) {tasks[i].get();};

    //write to PPM
    std::cout << "writing to file...\n";
    dprintf(outputFile, "P6\n%ld %ld\n255\n", side, side);
    size_t written = 0;
    while (written < area) {
        written += write(outputFile, grid + written/sizeof(pixel), area - written);
    }
    close(outputFile);
}

void loadData(std::string input, std::vector<pixel>& data, long& bytesWritten) {
    if (std::filesystem::is_directory(input)) {
        std::vector<std::filesystem::directory_entry> dirs;
        for(const auto& entry: std::filesystem::directory_iterator(input)) {
            dirs.push_back(entry);
        }
        std::sort(dirs.begin(), dirs.end(),
    [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) {

            if (a.is_directory() && !b.is_directory()) return true;
            if (!a.is_directory() && b.is_directory()) return false;
            if (a.is_directory() && b.is_directory()) return false;

            size_t c = 0;
            size_t d = 0;

            try {c = std::filesystem::file_size(a.path());} catch (std::exception){};
            try {d = std::filesystem::file_size(b.path());} catch (std::exception){};

            return c < d;
        });

        for (auto i : dirs) {
            try {
                loadData(i.path(), data, bytesWritten);
            } catch (std::exception){
                std::cout << "could not read file\n";
            }
        }
    } else {
        std::ifstream inputFile(input, std::ios::binary);
        size_t fileSize = std::filesystem::file_size(input);

        if (!inputFile) {
            // file failed to open
        } else if (fileSize != 0) {
            data.resize((fileSize + bytesWritten + sizeof(pixel) - 1)/sizeof(pixel) + 1); // ceil division
            std::cout << "\33[2K\r# bytes written - dir - size | " << bytesWritten << " - " << input << " - " << fileSize << std::flush;
            inputFile.read((char*)data.data() + bytesWritten, fileSize);
            bytesWritten += fileSize;
        }
            //file empty, do nothing
    }
}

int main(int argc, char* argv[]) {
    std::cout << std::filesystem::current_path() << std::endl;
    //temp way to pass files in
    if (argc > 1) {
        printf("input file: %s\n", argv[1]);
    } else {
        puts("please provide input file.\n");
        return 1;
    }
    FILE* inputFile = fopen(argv[1], "r");
    std::string inputDir = argv[1];
    if (argc > 2) {
        printf("output file: %s\n", argv[2]);
    } else {
        puts("please provide output file. (extension must be .ppm)\n");
        return 1;
    }
    long outputFile = open(argv[2], O_WRONLY | O_CREAT, 0644);

    encodeToFile(inputDir, outputFile);

    std::cout << "done!\n";
}
