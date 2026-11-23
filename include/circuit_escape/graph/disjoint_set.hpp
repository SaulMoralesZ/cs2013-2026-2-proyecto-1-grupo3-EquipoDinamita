#pragma once

#include <cstddef>
#include <vector>

namespace circuit_escape {

// DisjointSet (Union-Find) con compresion de camino y union por rango.
// Se usa en Kruskal para registrar que casillas ya estan conectadas.
// Biblioteca propia: no se usan bibliotecas externas.
template<typename T = int>
class DisjointSet {
public:
    // Crea n conjuntos disjuntos (cada elemento es su propio padre).
    void makeSet(std::size_t n) {
        parent_.resize(n);
        rank_.assign(n, 0);
        for (std::size_t i = 0; i < n; ++i) {
            parent_[i] = static_cast<T>(i);
        }
    }

    // Encuentra el representante del conjunto de x (con compresion de camino).
    T find(T x) {
        if (parent_[static_cast<std::size_t>(x)] != x) {
            parent_[static_cast<std::size_t>(x)] =
                find(parent_[static_cast<std::size_t>(x)]);
        }
        return parent_[static_cast<std::size_t>(x)];
    }

    // Une los conjuntos de a y b. Devuelve true si se unieron (eran distintos).
    bool unionSets(T a, T b) {
        T ra = find(a);
        T rb = find(b);
        if (ra == rb) return false;
        // Union por rango
        std::size_t ia = static_cast<std::size_t>(ra);
        std::size_t ib = static_cast<std::size_t>(rb);
        if (rank_[ia] < rank_[ib]) {
            parent_[ia] = rb;
        } else if (rank_[ia] > rank_[ib]) {
            parent_[ib] = ra;
        } else {
            parent_[ib] = ra;
            ++rank_[ia];
        }
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept { return parent_.size(); }

private:
    std::vector<T> parent_;
    std::vector<std::size_t> rank_;
};

}  // namespace circuit_escape