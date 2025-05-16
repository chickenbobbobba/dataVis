#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <iostream>
#include <fstream>
#include <ostream>
#include <string>
#include <cmath>
#include <filesystem>
#include <unordered_set>
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

namespace fs = std::filesystem;
void loadData(std::string input, std::vector<pixel>& data, long& bytesWritten, long depth = 0);

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

long accumulateSize(const fs::path& p,
                                 std::unordered_set<fs::path>& seen,
                                 std::error_code& ec) {
    // Resolve symlinks; skip on error
    fs::path real = fs::canonical(p, ec);
    if (ec) return 0;

    // Dedupe to avoid loops
    if (!seen.insert(real).second)
        return 0;

    // Get status; skip on error
    fs::file_status st = fs::status(real, ec);
    if (ec) return 0;

    if (fs::is_regular_file(st)) {
        // Regular file → add size (skip on error)
        return fs::file_size(real, ec) ?: 0;
    }
    else if (fs::is_directory(st)) {
        uintmax_t total = 0;
        for (auto& de : fs::directory_iterator(real, ec)) {
            if (ec) break;
            total += accumulateSize(de.path(), seen, ec);
            if (ec) ec.clear();
        }
        return total;
    }
    // Other types → skip
    return 0;
}

/// Returns total size in bytes of `dir` (follows symlinks, skips unreadable items).
long getDirSize(const fs::path& dir) {
    std::error_code ec;
    std::unordered_set<fs::path> seen;
    return accumulateSize(dir, seen, ec);
}

void encodeToFile(std::string inputDir, long outputFile) {
    std::vector<pixel> data;
    
    long bytesWritten = 0;
    loadData(inputDir, data, bytesWritten);

    long inputLen = data.size();
    std::cout << "parsing " << inputLen * 3 /1024/1024<< " MiB of data" << std::endl;

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

void loadData(std::string input, std::vector<pixel>& data, long& bytesWritten, long depth) {
    if (depth == 0) {
        std::cout << "preallocating data..." << std::endl;
        long dirSize = getDirSize(input);
        std::cout << "allocating " << dirSize/1024/1024 << " MiB" << std::endl;
        data.resize((dirSize+sizeof(pixel) - 1)/sizeof(pixel));
    }

    depth++;
    if (fs::is_directory(input)) {
        std::vector<fs::directory_entry> dirs;
        for(const auto& entry: fs::directory_iterator(input)) {
            dirs.push_back(entry);
        }
        std::sort(dirs.begin(), dirs.end(),
    [](const fs::directory_entry& a, const fs::directory_entry& b) {

            if (a.is_directory() && !b.is_directory()) return true;
            if (!a.is_directory() && b.is_directory()) return false;
            if (a.is_directory() && b.is_directory()) return false;

            size_t c = 0;
            size_t d = 0;

            try {c = fs::file_size(a.path());} catch (std::exception){};
            try {d = fs::file_size(b.path());} catch (std::exception){};

            return c < d;
        });

        for (auto i : dirs) {
            try {
                loadData(i.path(), data, bytesWritten, depth);
            } catch (std::exception){
                // std::cout << "could not read file\n";
            }
        }
    } else {
        std::ifstream inputFile(input, std::ios::binary);
        size_t fileSize = fs::file_size(input);

        if (!inputFile) {
            //std::cout << "file failed to open!" << std::endl;
        } else if (fileSize != 0) {
            long minPixVecSize = (fileSize + bytesWritten + sizeof(pixel) - 1)/sizeof(pixel);
            if (data.size() < minPixVecSize) {
                data.resize((unsigned long)minPixVecSize);
                //std::cout << "resize" << std::endl;
            }
            //data.resize((fileSize + bytesWritten + sizeof(pixel) - 1)/sizeof(pixel) + 1); // ceil division
            std::cout << "\33[2K\r# bytes written - dir - size | " << bytesWritten << " - " << input << " - " << fileSize << std::flush;
            inputFile.read((char*)data.data() + bytesWritten, fileSize);
            bytesWritten += fileSize;
        }
            //file empty, do nothing
    }
    depth--;
    if (depth > 0) return;
    //std::cout << "resizeFinal" << std::endl;
    std::cout << std::endl;
    data.resize((bytesWritten + sizeof(pixel) - 1)/sizeof(pixel));
    //data.push_back({(char)255,(char)255,(char)255});
}

int main(int argc, char* argv[]) {
    std::cout << fs::current_path() << std::endl;
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
