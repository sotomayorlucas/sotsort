// sotsort_ooo.hpp — SotSort-OOO (EXPERIMENTAL, aislado de sotsort.hpp)
//
// Planificación por dependencias del quicksort estable: cada rango pendiente
// de QuickEstable es una tarea; una tarea queda lista apenas termina la
// partición que la creó, y se ejecuta en cualquier orden y en cualquier hilo.
// No hay barreras globales ni rondas de mezcla.
//
// Dependencias falsas por el buffer ("renombrado"):
//   modo Completo: la tarea sobre [lo, hi) usa aux[lo..hi) (zonas disjuntas);
//   modos Bloques y O(1): un buffer por trabajador (O(P) buffers de tamaño fijo).
// La cola está acotada (64 P tareas): con la cola llena, la mitad grande se
// queda en la pila local del trabajador, como en el algoritmo secuencial.
// Los pivotes de cada tarea salen de una semilla derivada de (semilla, lo, hi):
// el árbol de rangos no depende del orden de ejecución.
//
// Camino de runs naturales, entradas chicas y un solo hilo: se delega en
// sot::stable_sort_fast (el modo paralelo v8 por rondas de merge path).
#pragma once
#include "../sotsort.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace sot {

struct OooStats {
    const char* path = "";       // "ooo", "delegado" (runs naturales / chico / 1 hilo), "ordenado", "conteo"
    unsigned workers = 0;
    std::size_t tasks = 0;       // tareas ejecutadas
    std::size_t refused = 0;     // cesiones rechazadas por cola llena
    std::size_t waits = 0;       // veces que un trabajador esperó sin tareas
    std::size_t max_queue = 0;   // largo máximo de la cola
    double idle_s = 0;           // tiempo total de espera, sumado sobre trabajadores
    double wall_s = 0;           // tiempo de la fase de tareas
    std::size_t max_task = 0;    // tarea más grande ejecutada después de la raíz (elementos)
    double root_s = 0;           // tiempo de la partición paralela de la raíz (0 si no se usó)
};

namespace detail {

inline std::uint64_t mix64(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ull; x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull; return x ^ (x >> 31);
}

template <class T, class Cmp>
class OooPool {
public:
    struct Task { std::size_t lo, hi; int bad; bool has; T lb; };
    using S = Sorter<T, Cmp>;

    OooPool(T* a, Cmp c, unsigned P, std::uint64_t seed, std::size_t grain, Plan::Mode mode, T* aux, std::size_t E)
        : a_(a), c_(c), P_(P), seed_(seed), grain_(grain), mode_(mode), aux_(aux), E_(E), cap_(64 * static_cast<std::size_t>(P)) {}

    void add(std::size_t lo, std::size_t hi, int bad, bool has, const T& lb) { if (hi - lo > 1) q_.push_back(Task{lo, hi, bad, has, has ? lb : T{}}); }
    void run(OooStats& st) {
        auto t0 = std::chrono::steady_clock::now();
        std::vector<std::thread> th;
        th.reserve(P_);
        unsigned started = 1;
        for (unsigned i = 1; i < P_; ++i) {
            try { th.emplace_back([this, i] { worker(i); }); ++started; }
            catch (...) { break; }                 // sin hilos: siguen los que haya
        }
        worker(0);
        for (auto& t : th) t.join();
        st.wall_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        st.workers = started; st.tasks = tasks_; st.refused = refused_; st.waits = waits_;
        st.max_queue = max_queue_; st.idle_s = idle_; st.max_task = max_task_;
        if (err_) std::rethrow_exception(err_);
    }

private:
    struct Sp final : S::Spawner {
        OooPool* pool;
        bool spawn(std::size_t lo, std::size_t hi, int bad, bool has, const T& lb) override {
            std::lock_guard<std::mutex> g(pool->mu_);
            if (pool->q_.size() >= pool->cap_ || pool->err_) { ++pool->refused_; return false; }
            pool->q_.push_back(Task{lo, hi, bad, has, has ? lb : T{}});
            if (pool->q_.size() > pool->max_queue_) pool->max_queue_ = pool->q_.size();
            pool->cv_.notify_one();
            return true;
        }
    };

    void worker(unsigned id) {
        std::unique_ptr<S> sorter;
        try { sorter.reset(new S(a_, c_)); } catch (...) { fail(std::current_exception()); return; }
        if (mode_ == Plan::Full) sorter->follow_buffer(aux_);
        else if (mode_ == Plan::Block) sorter->use_buffer(aux_ + id * (E_ / P_), E_ / P_);
        Sp sp; sp.pool = this;
        std::unique_lock<std::mutex> lk(mu_);
        for (;;) {
            while (q_.empty() && active_ > 0 && !err_) {
                ++waits_;
                auto w0 = std::chrono::steady_clock::now();
                cv_.wait(lk);
                idle_ += std::chrono::duration<double>(std::chrono::steady_clock::now() - w0).count();
            }
            if (q_.empty() || err_) { cv_.notify_all(); return; }   // sin tareas y nadie trabajando: fin
            Task t = std::move(q_.front());                          // la más vieja: suele ser la más grande
            q_.pop_front();
            ++active_; ++tasks_;
            if (tasks_ > 1 && t.hi - t.lo > max_task_) max_task_ = t.hi - t.lo;
            lk.unlock();
            try {
                sorter->seed(seed_ ^ mix64(t.lo * 0x9E3779B97F4A7C15ull + t.hi));
                sorter->quick_task(t.lo, t.hi, t.bad, t.has, t.lb, &sp, grain_);
            } catch (...) { fail(std::current_exception()); }
            lk.lock();
            --active_;
            if (q_.empty() && active_ == 0) cv_.notify_all();
        }
    }
    void fail(std::exception_ptr e) {
        std::lock_guard<std::mutex> g(mu_);
        if (!err_) err_ = e;
        q_.clear();
        cv_.notify_all();
    }

    T* a_; Cmp c_; unsigned P_; std::uint64_t seed_; std::size_t grain_;
    Plan::Mode mode_; T* aux_; std::size_t E_; std::size_t cap_;
    std::mutex mu_; std::condition_variable cv_;
    std::deque<Task> q_;
    std::size_t active_ = 0, tasks_ = 0, refused_ = 0, waits_ = 0, max_queue_ = 1, max_task_ = 0;
    double idle_ = 0;
    std::exception_ptr err_;
};

}  // namespace detail

template <class T, class Cmp = std::less<T>>
void stable_sort_ooo(T* a, std::size_t n, Cmp c = Cmp(), const Options& o = Options(),
                     OooStats* stats = nullptr, std::size_t grain = std::size_t(1) << 15, bool par_root = true) {
    OooStats local; OooStats& st = stats ? *stats : local;
    st = OooStats();
    Plan pl = make_plan<T>(n, o);
    if (n < 2) { st.path = "ordenado"; return; }
    unsigned P = o.threads ? o.threads : std::thread::hardware_concurrency();
    if (P < 1) P = 1;
    const std::uint64_t seed = (o.seed ? o.seed : detail::SIGMA) ^ n;
    {
        auto s0 = std::make_unique<Sorter<T, Cmp>>(a, c);
        if (s0->already_sorted(0, n)) { st.path = "ordenado"; return; }
        if constexpr (std::is_integral<T>::value && !std::is_same<T, bool>::value && detail::natural_less<T, Cmp>::value) {
            if (o.shortcuts && detail::int_shortcut(a, n, pl.budget)) { st.path = "conteo"; return; }
        }
        bool few = true;
        if (P >= 2 && n >= 4 * grain) {            // giros contados por tramos, en paralelo: mismo total
            std::vector<std::size_t> g(P, 0);
            detail::run_parallel(P, [&](unsigned i) {
                std::size_t f = static_cast<std::size_t>(static_cast<unsigned __int128>(n) * i / P);
                std::size_t t = static_cast<std::size_t>(static_cast<unsigned __int128>(n) * (i + 1) / P);
                Cmp cc = c;
                std::size_t k = 0;
                if (f < 2) f = 2;
                if (f < t) {
                    bool prev = cc(a[f - 1], a[f - 2]);
                    while (f < t) {                 // corte temprano: si un tramo solo ya llega a n/8, no hay pocos giros
                        std::size_t e = std::min(t, f + 4096);
                        for (; f < e; ++f) { bool dd = cc(a[f], a[f - 1]); k += dd != prev; prev = dd; }
                        if (k * 8 >= n) break;
                    }
                }
                g[i] = k;
            });
            std::size_t tot = 0;
            for (auto x : g) tot += x;
            few = tot * 8 < n && (!std::is_trivially_copyable<T>::value || s0->sample_is_ordered(n));
            s0->set_few_turns(few);
        }
        if (P < 2 || n < 4 * grain || few) {
            st.path = "delegado";
            s0->seed(seed);
            detail::fast_tail(a, n, c, pl, *s0, seed);     // sin repetir los recorridos
            return;
        }
    }
    detail::Aux<T> aux;
    while (pl.mode != Plan::O1 && !aux.alloc(pl.E)) {
        std::size_t e = pl.mode == Plan::Full ? n / 2 : pl.E / 2;
        std::size_t E = detail::bit_floor(e);
        if (E > Sorter<T, Cmp>::B0) { pl.mode = Plan::Block; pl.E = E; }
        else { pl.mode = Plan::O1; pl.E = 0; }
    }
    st.path = "ooo";
    detail::OooPool<T, Cmp> pool(a, c, P, seed, grain, pl.mode, aux.p, pl.E);
    int budget = 0;
    for (std::size_t k = n; k; k >>= 1) ++budget;
    auto t0 = std::chrono::steady_clock::now();
    bool rooted = false;
    if (par_root && pl.mode == Plan::Full && n >= (std::size_t(1) << 19)) {
        // Partición paralela de la raíz: (1) cada hilo parte su tramo; los que cumplen quedan
        // compactados al principio del tramo y el resto va a aux[tramo]; (2) los que cumplen se
        // juntan de izquierda a derecha (destino a la izquierda del origen: secuencial);
        // (3) el resto vuelve de aux a su lugar final, en paralelo (zonas disjuntas).
        T piv;
        { auto sp = std::make_unique<Sorter<T, Cmp>>(a, c); sp->seed(seed); piv = sp->pick_pivot(0, n); }
        std::vector<std::size_t> d(P + 1), m(P), F(P + 1, 0);
        for (unsigned i = 0; i <= P; ++i) d[i] = static_cast<std::size_t>(static_cast<unsigned __int128>(n) * i / P);
        detail::run_parallel(P, [&](unsigned i) {
            auto sp = std::make_unique<Sorter<T, Cmp>>(a, c);
            m[i] = sp->part_chunk(d[i], d[i + 1], piv, aux.p + d[i]);
        });
        std::size_t M = m[0];
        for (unsigned i = 1; i < P; ++i) { std::move(a + d[i], a + d[i] + m[i], a + M); M += m[i]; }
        for (unsigned i = 0; i < P; ++i) F[i + 1] = F[i] + (d[i + 1] - d[i] - m[i]);
        detail::run_parallel(P, [&](unsigned i) {
            std::move(aux.p + d[i], aux.p + d[i] + (d[i + 1] - d[i] - m[i]), a + M + F[i]);
        });
        st.root_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        if (M > 0) {           // M = 0: el pivote es el mínimo; la tarea raíz normal separa los iguales
            int bad = budget - (std::min(M, n - M) < n / 16 ? 1 : 0);
            pool.add(0, M, bad, false, piv);
            pool.add(M, n, bad, true, piv);
            rooted = true;
        }
    }
    if (!rooted) pool.add(0, n, budget, false, T{});
    pool.run(st);
}

}  // namespace sot
