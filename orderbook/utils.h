#include <vector>
#include <set>
#include <utility>
#include <queue>
#include <map>

/// @brief Remove provided elements from a vector
/// @param vec
/// @param idxToRemove trust these are in accending order!
template <typename T>
void removeIdxs(std::vector<T>& vec, const std::vector<size_t>& idxToRemove){
    if(idxToRemove.empty()) return;

    size_t write = 0;
    size_t prev = 0;
    size_t n = vec.size();

    // Assumption: idxToRemove is sorted ascending
    for(size_t i = 0; i < idxToRemove.size(); ++i){
        size_t remPos = idxToRemove[i];
        if(remPos >= n) break;

        // move the block [prev, remPos) to write
        for(size_t k = prev; k < remPos; ++k){
            vec[write++] = std::move(vec[k]);
        }
        prev = remPos + 1; // skip the removed index
    }

    // move the tail [prev, n)
    for(size_t k = prev; k < n; ++k){
        vec[write++] = std::move(vec[k]);
    }

    vec.resize(write);
};
