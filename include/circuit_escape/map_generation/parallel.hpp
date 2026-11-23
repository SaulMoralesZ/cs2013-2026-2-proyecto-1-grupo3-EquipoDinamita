#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

namespace circuit_escape {

// Ejecuta task(i, worker) para cada i en [0, tasks) repartiendo los indices
// entre `workers` hilos std::jthread (al menos uno, nunca mas que tareas).
//  - El reparto se protege con un contador atomico: cada indice se procesa
//    exactamente una vez, por un solo hilo.
//  - Devuelve cuando TODAS las tareas terminaron (los jthread se unen al
//    destruirse): sirve de barrera entre fases.
//  - task no debe lanzar excepciones: una excepcion que escapa de un hilo
//    termina el programa. Quien llama captura los errores por tarea.
// Devuelve la cantidad de trabajadores realmente usados.
template<typename Task>
std::size_t run_parallel(std::size_t tasks, std::size_t workers, Task&& task) {
    if (tasks == 0) return 0;
    workers = std::clamp<std::size_t>(workers, 1, tasks);
    std::atomic<std::size_t> next{0};
    {
        std::vector<std::jthread> pool;
        pool.reserve(workers);
        for (std::size_t w = 0; w < workers; ++w) {
            pool.emplace_back([&, w] {
                for (std::size_t i = next.fetch_add(1); i < tasks; i = next.fetch_add(1)) {
                    task(i, w);
                }
            });
        }
    }  // join de todos los trabajadores
    return workers;
}

// Trabajadores por defecto: los nucleos disponibles (minimo 1).
[[nodiscard]] inline std::size_t default_workers() noexcept {
    const unsigned hw = std::thread::hardware_concurrency();
    return hw == 0 ? 1 : hw;
}

}  // namespace circuit_escape
