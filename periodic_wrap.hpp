// periodic_wrap.hpp
// Header-only periodic winding analysis of voxel clusters. No Qt dependency.
//
//   periodic::wrapRank6 - rank (0..3) of the winding vectors of a voxel set on
//                         an n^3 periodic grid, under 6-connectivity
//
// Rank 3 means the set carries a face-connected path around the cell along three
// independent directions, i.e. it percolates in 3D. Paths that close only through
// an edge or corner contact do not count, because those contacts are not faces.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace periodic {

namespace detail {

// Rank over the rationals of the vectors added so far. Image counts are bounded
// by the voxel count over n, so for any supported grid the determinant fits in
// 64 bits.
class RankAccumulator {
public:
    void add(const std::array<int64_t, 3>& v)
    {
        if (m_rank == 0) {
            if (v[0] != 0 || v[1] != 0 || v[2] != 0)
                push(v);
        } else if (m_rank == 1) {
            const std::array<int64_t, 3> c = cross(m_basis[0], v);
            if (c[0] != 0 || c[1] != 0 || c[2] != 0)
                push(v);
        } else if (m_rank == 2) {
            const std::array<int64_t, 3> c = cross(m_basis[0], m_basis[1]);
            if (c[0] * v[0] + c[1] * v[1] + c[2] * v[2] != 0)
                push(v);
        }
    }

    int rank() const { return m_rank; }

private:
    static std::array<int64_t, 3> cross(const std::array<int64_t, 3>& a,
                                        const std::array<int64_t, 3>& b)
    {
        return { a[1] * b[2] - a[2] * b[1],
                 a[2] * b[0] - a[0] * b[2],
                 a[0] * b[1] - a[1] * b[0] };
    }

    void push(const std::array<int64_t, 3>& v) { m_basis[m_rank++] = v; }

    std::array<std::array<int64_t, 3>, 3> m_basis{};
    int m_rank = 0;
};

} // namespace detail

// Breadth-first search that tracks the periodic image each voxel was reached in.
// Reaching a visited voxel in a different image closes a loop around the cell,
// and the image difference is its winding vector. The loops closed by non-tree
// edges generate all loops of the graph, so the rank does not depend on the
// visiting order.
//
// Coord needs integer members x, y, z in [0, n). If the set has several
// 6-connected components, the result is the largest rank of any one of them:
// windings of separate components do not combine into one load path.
template <class Coord>
int wrapRank6(const std::vector<Coord>& voxels, int n)
{
    if (n <= 0 || voxels.empty())
        return 0;

    const int64_t n2 = static_cast<int64_t>(n) * n;
    const auto flatIndex = [n, n2](int x, int y, int z) {
        return x * n2 + static_cast<int64_t>(y) * n + z;
    };

    // Flat grid index -> position in cells[], -1 where unoccupied
    struct Cell {
        int x, y, z;
        int imageX = 0, imageY = 0, imageZ = 0;
        bool visited = false;
    };
    std::vector<int32_t> slot(static_cast<size_t>(n2 * n), -1);
    std::vector<Cell> cells;
    cells.reserve(voxels.size());
    for (const Coord& c : voxels) {
        int32_t& s = slot[static_cast<size_t>(flatIndex(c.x, c.y, c.z))];
        if (s < 0) {
            s = static_cast<int32_t>(cells.size());
            Cell cell;
            cell.x = c.x;
            cell.y = c.y;
            cell.z = c.z;
            cells.push_back(cell);
        }
    }

    static const int kSteps[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 },
                                      { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

    // Returns the wrapped coordinate and adds the boundary crossing to image
    const auto wrap = [n](int coord, int& image) {
        if (coord >= n) { ++image; return coord - n; }
        if (coord < 0) { --image; return coord + n; }
        return coord;
    };

    int best = 0;
    std::vector<int32_t> queue;
    queue.reserve(cells.size());

    for (size_t start = 0; start < cells.size(); ++start) {
        if (cells[start].visited)
            continue;

        detail::RankAccumulator rank;
        queue.clear();
        queue.push_back(static_cast<int32_t>(start));
        cells[start].visited = true;

        for (size_t head = 0; head < queue.size(); ++head) {
            const Cell cur = cells[queue[head]];
            for (const auto& step : kSteps) {
                int imageX = cur.imageX, imageY = cur.imageY, imageZ = cur.imageZ;
                const int x = wrap(cur.x + step[0], imageX);
                const int y = wrap(cur.y + step[1], imageY);
                const int z = wrap(cur.z + step[2], imageZ);

                const int32_t next = slot[static_cast<size_t>(flatIndex(x, y, z))];
                if (next < 0)
                    continue;

                Cell& neighbor = cells[next];
                if (!neighbor.visited) {
                    neighbor.visited = true;
                    neighbor.imageX = imageX;
                    neighbor.imageY = imageY;
                    neighbor.imageZ = imageZ;
                    queue.push_back(next);
                } else {
                    rank.add({ static_cast<int64_t>(imageX) - neighbor.imageX,
                               static_cast<int64_t>(imageY) - neighbor.imageY,
                               static_cast<int64_t>(imageZ) - neighbor.imageZ });
                }
            }
        }

        if (rank.rank() > best)
            best = rank.rank();
        if (best == 3)
            break;
    }
    return best;
}

} // namespace periodic
