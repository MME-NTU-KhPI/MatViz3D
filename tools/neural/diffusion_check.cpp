// Stand-alone check of CellDiffusion (no Qt): the numbers tools/neural/check_cell_diffusion.py compares with PyTorch.
//
//   clang++ -O2 -std=c++17 -I. tools/neural/diffusion_check.cpp celldiffusion.cpp cellnet.cpp celldecoder.cpp -o diffusion_check \
//       -DMATVIZ_USE_ACCELERATE -framework Accelerate -Xpreprocessor -fopenmp -I/opt/homebrew/opt/libomp/include \
//       -L/opt/homebrew/opt/libomp/lib -lomp
//   ./diffusion_check models/cell_diffusion.bin models/cell_decoder.bin <out folder> [seed] [steps]
//
// Prints the request and the output of the network for the reference input of export_cell_diffusion.py --reference (the same
// numbers must come out), then samples one code and writes request.bin, noise.bin, predicted.bin, code.bin (float32)
// and cell.bin (uint8, 64^3) into the folder.
#include "celldecoder.h"
#include "celldiffusion.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>

static double seconds()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

template <class T> static void dump(const std::string& path, const std::vector<T>& v)
{
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(v.data()), std::streamsize(v.size() * sizeof(T)));
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        std::fprintf(stderr, "usage: diffusion_check <cell_diffusion.bin> <cell_decoder.bin> <out folder> [seed] [steps]\n");
        return 2;
    }
    const std::string out = argv[3];
    const uint32_t seed = argc > 4 ? uint32_t(std::strtoul(argv[4], nullptr, 10)) : 7;
    const int steps = argc > 5 ? std::atoi(argv[5]) : 0;

    CellDiffusion net;
    std::string error;
    double t0 = seconds();
    if (!net.open(argv[1], &error)) { std::fprintf(stderr, "open: %s\n", error.c_str()); return 1; }
    const double openS = seconds() - t0;
    t0 = seconds();
    if (!net.load(&error)) { std::fprintf(stderr, "load: %s\n", error.c_str()); return 1; }
    std::printf("%s: %zu weights, header %.1f ms, weights %.2f s | returns %s, request %s | %d steps from level %d, offset %g, "
                "clamp %g, guidance %g\n",
                net.name().c_str(), net.weightCount(), openS * 1e3, seconds() - t0, net.predictsV() ? "v" : "the noise",
                net.usesTokens() ? "as tokens too" : "as an added vector", net.steps(), net.startLevel(), double(net.offset()),
                double(net.clamp()), double(net.guidance()));
    std::printf("note: %s\n", net.note().c_str());

    // The reference input of export_cell_diffusion.py --reference.
    const int n = net.codeSize(), zc = net.codeChannels();
    std::vector<float> x(net.codeLength());
    const double w = 2.0 * M_PI / n;
    for (int c = 0; c < zc; ++c)
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                for (int k = 0; k < n; ++k)
                    x[((size_t(c) * n + i) * n + j) * n + k] =
                        float(1.3 * std::sin(w * ((c + 1) * i + 2 * j + 3 * k) + 0.5 * c) * std::cos(w * (i - (c + 2) * j + k)) + 0.1 * c);
    CellDiffusion::RequestNotes notes;
    const std::vector<float> req = net.request({ { "Ex", 3.0e8 }, { "Ey", 3.0e8 }, { "Ez", 3.0e8 }, { "density", 0.30 },
                                                 { "EmaxEmin", 1.5 }, { "nuMin", 0.1 } }, 2.0, 0.38, 1.0, &notes);
    std::printf("request:");
    for (size_t i = 0; i < req.size(); ++i)
        if (req[i] != 0.0f) std::printf(" [%zu] %.6f", i, double(req[i]));
    std::printf(" | used %zu, clipped %zu, unknown %zu\n", notes.used.size(), notes.clipped.size(), notes.unknown.size());
    t0 = seconds();
    const std::vector<float> e = net.predict(x, 500, req);
    const double first = seconds() - t0;
    t0 = seconds();
    net.predict(x, 500, req);
    double sum = 0, sq = 0;
    for (float v : e) { sum += v; sq += double(v) * v; }
    std::printf("output: mean %.6f, rms %.6f | one pass %.3f s (the first %.3f s)\n", sum / double(e.size()),
                std::sqrt(sq / double(e.size())), seconds() - t0, first);
    const int probes[8][4] = { { 0, 0, 0, 0 }, { 1, 1, 2, 3 }, { 2, 15, 0, 7 }, { 3, 8, 8, 8 },
                               { 0, 5, 11, 2 }, { 3, 15, 15, 15 }, { 1, 0, 9, 14 }, { 2, 7, 3, 12 } };
    std::printf("probes:");
    for (const auto& p : probes) std::printf(" %.6f", double(e[((size_t(p[0]) * n + p[1]) * n + p[2]) * n + p[3]]));
    std::printf("\n");
    dump(out + "/input.bin", x);
    dump(out + "/predicted.bin", e);
    dump(out + "/request.bin", req);

    // One code from noise, then the cell.
    const std::vector<float> noise = net.startNoise(seed);
    dump(out + "/noise.bin", noise);
    t0 = seconds();
    std::vector<float> code = net.sample(req, noise, {}, steps);
    const double sampleS = seconds() - t0;
    dump(out + "/code.bin", code);
    std::printf("sampling: seed %u, %d steps of %d passes, %.1f s (%.3f s per step)\n", seed, steps > 0 ? steps : net.steps(),
                net.passesPerStep(), sampleS, sampleS / (steps > 0 ? steps : net.steps()));

    std::ifstream file(argv[2], std::ios::binary);
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    CellDecoder decoder;
    if (!decoder.load(bytes.data(), bytes.size(), &error)) { std::fprintf(stderr, "decoder: %s\n", error.c_str()); return 1; }
    const size_t n3 = size_t(n) * n * n;
    for (int c = 0; c < zc; ++c)
        for (size_t i = 0; i < n3; ++i) code[size_t(c) * n3 + i] = code[size_t(c) * n3 + i] * net.codeSpread()[size_t(c)] + net.codeMean()[size_t(c)];
    t0 = seconds();
    const std::vector<uint8_t> cell = decoder.decode(code);
    size_t solid = 0;
    for (uint8_t v : cell) solid += v;
    std::printf("cell: %d^3, density %.4f (asked 0.30), decoded in %.2f s\n", decoder.cellSize(), double(solid) / double(cell.size()),
                seconds() - t0);
    dump(out + "/cell.bin", cell);
    return 0;
}
