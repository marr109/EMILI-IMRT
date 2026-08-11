#include "imrt_bao.h"

#include "../emilibase.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace emili {
namespace imrt {

//=== Representación de solución ===============================================

emili::Solution* BaoSolution::clone()
{
    // Crea un nuevo objeto en heap con los mismos ángulos activos e intensidades
    BaoSolution* s = new BaoSolution(active_angles_, (int)intensities_.size());

    // Copia los vectores por valor — el clon es independiente del original
    s->angle_degrees_ = angle_degrees_;
    s->intensities_   = intensities_;

    // Copia el valor objetivo para no pagar otra evaluación FMO
    s->solution_value = solution_value;

    return s;
}

std::string BaoSolution::getSolutionRepresentation()
{
    std::ostringstream oss;

    // Si no fue evaluada aún, muestra los índices activos (angle_degrees_ vacío)
    const std::vector<int>& deg = angle_degrees_.empty() ? active_angles_ : angle_degrees_;

    oss << "angles=[";
    for (int i = 0; i < (int)deg.size(); ++i) {
        if (i) oss << ",";   // separador de coma entre elementos (omite antes del primero)
        oss << deg[i];
    }
    oss << "]" << (angle_degrees_.empty() ? "(idx)" : "deg");

    // Agrega el valor objetivo con precisión fija de 2 decimales
    oss << " f=" << std::fixed << std::setprecision(2) << solution_value;

    return oss.str();
}


//=== Evaluación y registro ====================================================

void BaoProblem::openCsvLog(const std::string& path)
{
    csv_file_.open(path);
    if (csv_file_.is_open()) {
        // `cached` distingue una visita de una resolución FMO nueva.
        csv_file_ << "eval,angles_deg,objective,cached\n";
        std::cout << "  CSV log: " << path << "\n";
    } else {
        // Falla silenciosa: el algoritmo continúa sin log si el archivo no se puede abrir
        std::cerr << "[BAO] Could not open CSV log: " << path << "\n";
    }
}

double BaoProblem::calcObjectiveFunctionValue(emili::Solution& s)
{
    // Downcast seguro: s siempre es BaoSolution en este contexto
    BaoSolution& bs = static_cast<BaoSolution&>(s);

    // El FMO depende del conjunto de ángulos, no del orden de los slots.
    // Canonicalizar la clave permite reutilizar también permutaciones equivalentes.
    std::vector<int> cache_key = bs.active_angles_;
    std::sort(cache_key.begin(), cache_key.end());

    auto cached = fmo_cache_.find(cache_key);
    if (cached != fmo_cache_.end()) {
        bs.intensities_ = cached->second.intensities;
        last_eval_cached_ = true;
        return cached->second.objective;
    }

    // Llamada más costosa del sistema: resuelve el FMO con los ángulos activos.
    // Devuelve par<intensidades, valor_objetivo>
    auto res = fmo_.solve(bs.active_angles_);

    // Guarda objetivo e intensidades para evitar futuras llamadas idénticas al solver FMO.
    CachedFmoResult entry;
    entry.intensities = std::move(res.first);
    entry.objective = res.second;
    auto inserted = fmo_cache_.insert(std::make_pair(cache_key, std::move(entry)));

    bs.intensities_ = inserted.first->second.intensities;
    last_eval_cached_ = false;

    // Devuelve solo el valor numérico del objetivo; las intensidades quedan en bs
    return inserted.first->second.objective;
}

double BaoProblem::evaluateSolution(emili::Solution& s)
{
    // Resuelve el FMO y llena bs.intensities_ como efecto secundario
    double f = calcObjectiveFunctionValue(s);

    // Persiste el valor en la clase base para que el algoritmo pueda comparar soluciones
    s.setSolutionValue(f);

    // Downcast seguro: necesario para acceder a angle_degrees_ y active_angles_
    BaoSolution& bs = static_cast<BaoSolution&>(s);

    // Convierte índices activos a grados reales del catálogo — llena angle_degrees_
    bs.angle_degrees_.resize(bs.active_angles_.size());
    for (int i = 0; i < (int)bs.active_angles_.size(); ++i)
        bs.angle_degrees_[i] = inst_.angles[bs.active_angles_[i]];

    // Logging en consola solo si verbose_ está activo — no afecta el resultado
    if (verbose_) {
        std::cout << "  BAO eval: angles=[";
        for (int i = 0; i < (int)bs.angle_degrees_.size(); ++i) {
            if (i) std::cout << ",";
            std::cout << bs.angle_degrees_[i];
        }
        std::cout << "deg] -> f=" << std::fixed << std::setprecision(2) << f;
        if (last_eval_cached_) std::cout << " [cached]";
        std::cout << "\n";
    }

    if (csv_file_.is_open()) {
        // Preincremento: el contador arranca en 1
        csv_file_ << ++eval_count_ << ",\"";
        for (int i = 0; i < (int)bs.angle_degrees_.size(); ++i) {
            if (i) csv_file_ << ";";   // separador interno ; para no romper el formato CSV
            csv_file_ << bs.angle_degrees_[i];
        }
        // 6 decimales en CSV — más precisión que el display en consola
        csv_file_ << "\"," << std::fixed << std::setprecision(6) << f
                  << "," << (last_eval_cached_ ? "true" : "false") << "\n";
        // flush fuerza escritura al disco — útil si el proceso se interrumpe
        csv_file_.flush();
    }

    return f;
}

//=== Soluciones iniciales =====================================================

emili::Solution* FirstKAnglesInit::generateEmptySolution()
{
    const ImrtInstance& inst = bao_.getInstance();

    // Ordena índices, no el catálogo: inst.angles mantiene su correspondencia
    // con las matrices de dosis cargadas para cada ángulo.
    std::vector<int> degree_order(inst.n_angles);
    for (int i = 0; i < inst.n_angles; ++i) degree_order[i] = i;
    std::sort(degree_order.begin(), degree_order.end(),
        [&inst](int a, int b) { return inst.angles[a] < inst.angles[b]; });

    // Selecciona los K ángulos de menor grado real. Por ejemplo, en el
    // catálogo de 36 ángulos produce 0°, 10°, 20°, 30° aunque el archivo
    // de configuración esté ordenado lexicográficamente.
    std::vector<int> angles(degree_order.begin(),
                            degree_order.begin() + bao_.K());
    std::sort(angles.begin(), angles.end());

    // Crea la solución en heap — sin evaluar: angle_degrees_ vacío, solution_value = 0
    return new BaoSolution(angles, bao_.getInstance().n_dimlets);
}

emili::Solution* FirstKAnglesInit::generateSolution()
{
    // Construye la estructura vacía con los primeros K ángulos
    emili::Solution* s = generateEmptySolution();

    // Evalúa: resuelve el FMO y llena angle_degrees_ y solution_value
    bao_.evaluateSolution(*s);
    return s;
}

emili::Solution* RandomKAnglesInit::generateEmptySolution()
{
    int n = bao_.getInstance().n_angles;  // total de ángulos candidatos en el catálogo
    int K = bao_.K();                     // cuántos ángulos debe tener la solución

    // Crea una permutación completa [0, 1, 2, ..., n-1]
    std::vector<int> perm(n);
    for (int i = 0; i < n; ++i) perm[i] = i;

    // Fisher-Yates parcial: mezcla solo los primeros K elementos — O(K) en vez de O(n)
    // No se usa std::shuffle porque emili::generateRandomNumber() no es compatible con std
    for (int i = 0; i < K; ++i) {
        int j = i + emili::generateRandomNumber() % (n - i);  // índice aleatorio en [i, n-1]
        std::swap(perm[i], perm[j]);
    }

    // Toma los primeros K elementos mezclados como ángulos activos
    std::vector<int> angles(perm.begin(), perm.begin() + K);

    // Ordena los índices para mantener consistencia con el resto del algoritmo
    std::sort(angles.begin(), angles.end());

    // Crea la solución en heap — sin evaluar: angle_degrees_ vacío, solution_value = 0
    return new BaoSolution(angles, bao_.getInstance().n_dimlets);
}

emili::Solution* RandomKAnglesInit::generateSolution()
{
    // Construye la estructura vacía con K ángulos aleatorios
    emili::Solution* s = generateEmptySolution();

    // Evalúa: resuelve el FMO y llena angle_degrees_ y solution_value
    bao_.evaluateSolution(*s);
    return s;
}

//=== Vecindario: swap de ángulos ==============================================

int AngleSwapNeighborhood::size()
{
    int K  = bao_.K();                      // ángulos activos
    int na = bao_.getInstance().n_angles;   // total de ángulos en el catálogo

    // Vecindario = K opciones para sacar × (na - K) opciones para meter
    // Ej: K=5, na=36 → 5 × 31 = 155 vecinos
    return K * (na - K);
}

emili::Neighborhood::NeighborhoodIterator
AngleSwapNeighborhood::begin(emili::Solution* base)
{
    // Downcast seguro: necesitamos active_angles_ que no existe en Solution base
    BaoSolution* bs = static_cast<BaoSolution*>(base);

    // Guarda copia del estado base — computeStep siempre parte de acá para cada swap
    base_angles_ = bs->active_angles_;

    // Marca qué ángulos están activos en un vector de flags
    std::vector<bool> active_flag(n_angles_, false);
    for (int ai : base_angles_) active_flag[ai] = true;

    // Recolecta todos los ángulos que NO están activos — candidatos para entrar en un swap
    inactive_list_.clear();
    for (int a = 0; a < n_angles_; ++a)
        if (!active_flag[a]) inactive_list_.push_back(a);

    // Inicializa cursores al inicio del recorrido
    cur_active_idx_   = 0;    // primer ángulo activo a sacar
    cur_inactive_idx_ = 0;    // primer ángulo inactivo a meter
    first_            = true; // evita avanzar cursores en la primera llamada a computeStep

    // Devuelve el iterador listo para que el framework llame a computeStep K×(n-K) veces
    return emili::Neighborhood::NeighborhoodIterator(this, base);
}

void AngleSwapNeighborhood::reset()
{
    // Reinicia el recorrido al primer vecino — usado cuando el framework relanza la búsqueda
    cur_active_idx_   = 0;
    cur_inactive_idx_ = 0;
    first_            = true;
}

emili::Solution* AngleSwapNeighborhood::computeStep(emili::Solution* step)
{
    if (!first_) {
        // Avanza al siguiente inactivo; si se agotaron, pasa al siguiente activo
        ++cur_inactive_idx_;
        if (cur_inactive_idx_ >= (int)inactive_list_.size()) {
            ++cur_active_idx_;
            cur_inactive_idx_ = 0;
        }
    }
    first_ = false;

    // Fin del vecindario: se recorrieron todos los swaps posibles
    if (cur_active_idx_ >= (int)base_angles_.size()) return nullptr;
    if (inactive_list_.empty())                       return nullptr;

    // Aplica el swap: reemplaza el ángulo activo actual por el inactivo actual
    BaoSolution* bs = static_cast<BaoSolution*>(step);
    bs->active_angles_ = base_angles_;
    bs->active_angles_[cur_active_idx_] = inactive_list_[cur_inactive_idx_];
    std::sort(bs->active_angles_.begin(), bs->active_angles_.end());

    // Evalúa el vecino generado
    bao_.evaluateSolution(*bs);
    return bs;
}

void AngleSwapNeighborhood::reverseLastMove(emili::Solution* step)
{
    // Restaura los ángulos activos al estado base — el framework ya resetea solution_value
    // Las intensidades quedan desactualizadas pero se sobreescriben en el próximo computeStep
    BaoSolution* bs = static_cast<BaoSolution*>(step);
    bs->active_angles_ = base_angles_;
}

emili::Solution* AngleSwapNeighborhood::random(emili::Solution* s)
{
    BaoSolution* bs = static_cast<BaoSolution*>(s);

    // Si no hay inactivos no se puede hacer swap — devuelve clon sin cambios
    if (inactive_list_.empty()) return s->clone();

    // Elige un ángulo activo e inactivo al azar
    int ai = emili::generateRandomNumber() % bs->active_angles_.size();
    int ii = emili::generateRandomNumber() % inactive_list_.size();

    // Aplica el swap sobre una copia para no mutar la solución original
    BaoSolution* nb = static_cast<BaoSolution*>(s->clone());
    nb->active_angles_[ai] = inactive_list_[ii];
    std::sort(nb->active_angles_.begin(), nb->active_angles_.end());
    bao_.evaluateSolution(*nb);
    return nb;
}

//=== Vecindario: shift de ángulos ==============================================

void AngleShiftNeighborhood::buildDegreeOrder()
{
    const ImrtInstance& inst = bao_.getInstance();

    // Construye la permutación de índices de catálogo ordenados por grado real ascendente
    // (el array crudo inst.angles está en orden lexicográfico de string, no numérico)
    degree_order_.resize(n_angles_);
    for (int i = 0; i < n_angles_; ++i) degree_order_[i] = i;
    std::sort(degree_order_.begin(), degree_order_.end(),
        [&inst](int a, int b) { return inst.angles[a] < inst.angles[b]; });

    // Lookup inverso: índice de catálogo -> su posición en degree_order_
    degree_rank_.resize(n_angles_);
    for (int pos = 0; pos < n_angles_; ++pos) degree_rank_[degree_order_[pos]] = pos;
}

int AngleShiftNeighborhood::size()
{
    // Cota superior: 2 movimientos (±step) por cada ángulo activo.
    // El conteo real puede ser menor por colisiones con otros ángulos activos.
    return 2 * bao_.K();
}

emili::Neighborhood::NeighborhoodIterator
AngleShiftNeighborhood::begin(emili::Solution* base)
{
    // First/Best Improvement llama a begin() al iniciar cada ronda de búsqueda.
    // Guardamos la solución actual porque TODOS los vecinos de esta ronda deben
    // construirse a partir de la misma base.
    BaoSolution* bs = static_cast<BaoSolution*>(base);
    base_angles_ = bs->active_angles_;

    // El catálogo puede estar almacenado como 0,100,10,110,...; este lookup
    // permite que una posición represente el siguiente grado real: 0,10,20,...
    buildDegreeOrder();

    // Orden de generación:
    //   slot 0: -step, +step
    //   slot 1: -step, +step
    //   ...
    cur_active_idx_ = 0;
    cur_dir_        = 0;    // 0: resta; 1: suma
    first_          = true;

    return emili::Neighborhood::NeighborhoodIterator(this, base);
}

void AngleShiftNeighborhood::reset()
{
    // Reinicia el recorrido al primer vecino — usado cuando el framework relanza la búsqueda
    cur_active_idx_ = 0;
    cur_dir_        = 0;
    first_          = true;
}

emili::Solution* AngleShiftNeighborhood::computeStep(emili::Solution* step)
{
    // Esta es la función usada por First Improvement y Best Improvement.
    // Genera UN vecino determinista por llamada. No se usa aleatoriedad aquí.
    BaoSolution* bs = static_cast<BaoSolution*>(step);

    // step_ cuenta posiciones del catálogo ordenado por grados. En una instancia
    // con ángulos cada 10°, step_=1 equivale a desplazar exactamente 10°.
    // La normalización también permite desplazamientos circulares.
    int step_mod = ((step_ % n_angles_) + n_angles_) % n_angles_;

    while (true) {
        if (!first_) {
            // Después de probar -step, prueba +step sobre EL MISMO slot.
            // Cuando ambas direcciones terminan, avanza al siguiente slot.
            ++cur_dir_;
            if (cur_dir_ >= 2) {
                ++cur_active_idx_;
                cur_dir_ = 0;
            }
        }
        first_ = false;

        // No quedan combinaciones (slot, dirección): termina el iterador.
        if (cur_active_idx_ >= (int)base_angles_.size()) return nullptr;

        // Obtiene el ángulo que ocupa el slot actual y busca su posición dentro
        // del catálogo ordenado numéricamente.
        int active_catalog_idx = base_angles_[cur_active_idx_];
        int pos     = degree_rank_[active_catalog_idx];

        // Aplica -step o +step con wrap-around circular. Ejemplos para step_=1:
        //   20° - 10° = 10°
        //    0° - 10° = 350°
        //  350° + 10° = 0°
        int new_pos = (cur_dir_ == 0)
            ? (pos - step_mod + n_angles_) % n_angles_
            : (pos + step_mod) % n_angles_;
        int candidate = degree_order_[new_pos];

        // Una solución BAO no puede contener dos veces el mismo ángulo. Si el
        // candidato ya ocupa otro slot, esta combinación se omite y el while
        // continúa automáticamente con la dirección o el slot siguiente.
        bool collision = false;
        for (int a : base_angles_) {
            if (a == candidate) { collision = true; break; }
        }
        if (collision) continue;

        // Siempre reconstruye el vecino desde base_angles_. Solo reemplaza el
        // slot actual y NO ordena el vector, preservando la identidad del slot:
        //   base   [20,120,230,340]
        //   vecino [10,120,230,340]
        bs->active_angles_ = base_angles_;
        bs->active_angles_[cur_active_idx_] = candidate;

        // evaluateSolution consulta primero la caché FMO. El solver FMO solo se
        // ejecuta si este conjunto de ángulos todavía no fue evaluado.
        bao_.evaluateSolution(*bs);
        return bs;
    }
}

void AngleShiftNeighborhood::reverseLastMove(emili::Solution* step)
{
    // Restaura los ángulos activos al estado base — el framework ya resetea solution_value
    BaoSolution* bs = static_cast<BaoSolution*>(step);
    bs->active_angles_ = base_angles_;
}

emili::Solution* AngleShiftNeighborhood::random(emili::Solution* s)
{
    // Esta función NO participa en `first ... nangshift` ni en
    // `best ... nangshift`. Solo se usa si otro componente pide explícitamente
    // un vecino aleatorio al Neighborhood (por ejemplo, una perturbación).
    BaoSolution* bs = static_cast<BaoSolution*>(s);

    // Si random() se llama sin haber pasado antes por begin(), construye el orden por grado
    if ((int)degree_order_.size() != n_angles_) buildDegreeOrder();

    // Marca qué ángulos están activos, para detectar colisiones
    std::vector<bool> active_flag(n_angles_, false);
    for (int a : bs->active_angles_) active_flag[a] = true;

    int step_mod = ((step_ % n_angles_) + n_angles_) % n_angles_;

    // Reintento acotado: prueba unas pocas combinaciones (ángulo, dirección) al azar
    const int max_attempts = 8;
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        int ai  = emili::generateRandomNumber() % bs->active_angles_.size();
        int dir = emili::generateRandomNumber() % 2;

        int active_catalog_idx = bs->active_angles_[ai];
        int pos     = degree_rank_[active_catalog_idx];
        int new_pos = (dir == 0)
            ? (pos - step_mod + n_angles_) % n_angles_
            : (pos + step_mod) % n_angles_;
        int candidate = degree_order_[new_pos];

        if (active_flag[candidate]) continue;   // colisión: reintenta con otra combinación

        // Aplica el shift sobre una copia para no mutar la solución original
        // y conserva el slot elegido para que la trayectoria identifique
        // inequívocamente qué ángulo fue desplazado.
        BaoSolution* nb = static_cast<BaoSolution*>(s->clone());
        nb->active_angles_[ai] = candidate;
        bao_.evaluateSolution(*nb);
        return nb;
    }

    // No se encontró un movimiento válido tras los reintentos: devuelve clon sin cambios
    return s->clone();
}

//=== Perturbación =============================================================

emili::Solution* RandomAnglesPerturbation::perturb(emili::Solution* current)
{
    // Trabaja sobre una copia — no muta la solución actual
    BaoSolution* bs = static_cast<BaoSolution*>(current->clone());
    const int n = bao_.getInstance().n_angles;

    // Construye la lista de ángulos inactivos
    std::vector<bool> active_flag(n, false);
    for (int ai : bs->active_angles_) active_flag[ai] = true;
    std::vector<int> inactive;
    inactive.reserve(n - (int)bs->active_angles_.size());
    for (int a = 0; a < n; ++a)
        if (!active_flag[a]) inactive.push_back(a);

    // Limita los swaps al mínimo entre p_, activos disponibles e inactivos disponibles
    int swaps = std::min(p_, std::min((int)bs->active_angles_.size(),
                                      (int)inactive.size()));

    // Fisher-Yates parcial sobre ambas listas para seleccionar 'swaps' pares aleatorios
    for (int i = 0; i < swaps; ++i) {
        int ai = i + (int)(emili::generateRandomNumber() % (bs->active_angles_.size() - i));
        std::swap(bs->active_angles_[i], bs->active_angles_[ai]);
        int ii = i + (int)(emili::generateRandomNumber() % (inactive.size() - i));
        std::swap(inactive[i], inactive[ii]);
    }

    // Reemplaza los primeros 'swaps' activos por los primeros 'swaps' inactivos seleccionados
    for (int i = 0; i < swaps; ++i)
        bs->active_angles_[i] = inactive[i];

    std::sort(bs->active_angles_.begin(), bs->active_angles_.end());
    bao_.evaluateSolution(*bs);
    return bs;
}

emili::Solution* MultiScaleAngleShake::shake(emili::Solution* s, int k)
{
    // k controla la intensidad del shake: k=0 → 1 swap, k=1 → 2 swaps, etc.
    // Delega en RandomAnglesPerturbation con p = k+1
    RandomAnglesPerturbation pert(bao_, k + 1);
    return pert.perturb(s);
}

emili::Solution* GreedyAnglesPerturbation::perturb(emili::Solution* current)
{
    // Trabaja sobre una copia — no muta la solución actual
    BaoSolution* bs = static_cast<BaoSolution*>(current->clone());
    const int n = bao_.getInstance().n_angles;
    const int K = bao_.K();

    // Construye la lista de ángulos inactivos
    std::vector<bool> active_flag(n, false);
    for (int ai : bs->active_angles_) active_flag[ai] = true;
    std::vector<int> inactive;
    inactive.reserve(n - K);
    for (int a = 0; a < n; ++a)
        if (!active_flag[a]) inactive.push_back(a);

    // D: cuántos ángulos destruir y reconstruir — acotado por activos e inactivos disponibles
    int D = std::min(D_, std::min(K, (int)inactive.size()));

    // Destrucción: elimina D ángulos activos al azar (Fisher-Yates sobre el prefijo)
    for (int i = 0; i < D; ++i) {
        int ai = i + emili::generateRandomNumber() % (K - i);
        std::swap(bs->active_angles_[i], bs->active_angles_[ai]);
        inactive.push_back(bs->active_angles_[i]);   // el ángulo removido pasa a inactivos
    }
    bs->active_angles_.erase(bs->active_angles_.begin(),
                              bs->active_angles_.begin() + D);

    // Construcción greedy: agrega D ángulos uno a uno eligiendo el mejor FMO en cada paso
    for (int step = 0; step < D; ++step) {
        double best_f   = 1e30;
        int    best_pos = 0;

        for (int j = 0; j < (int)inactive.size(); ++j) {
            // Prueba agregar inactive[j] temporalmente y evalúa
            bs->active_angles_.push_back(inactive[j]);
            std::sort(bs->active_angles_.begin(), bs->active_angles_.end());

            double f = bao_.calcObjectiveFunctionValue(*bs);
            bs->setSolutionValue(f);

            if (f < best_f) { best_f = f; best_pos = j; }

            // Deshace: elimina el ángulo de prueba
            bs->active_angles_.erase(
                std::find(bs->active_angles_.begin(),
                          bs->active_angles_.end(), inactive[j]));
        }

        // Confirma el mejor ángulo encontrado en este paso
        bs->active_angles_.push_back(inactive[best_pos]);
        std::sort(bs->active_angles_.begin(), bs->active_angles_.end());
        bao_.evaluateSolution(*bs);
        inactive.erase(inactive.begin() + best_pos);
    }

    return bs;
}

//=== Memoria tabú =============================================================

bool BaoTabuMemory::tabu_check(emili::Solution* s)
{
    const BaoSolution* bs = static_cast<const BaoSolution*>(s);

    // Recorre el buffer circular desde la entrada más reciente hacia atrás
    for (int i = 0; i < count_; ++i) {
        int idx = (head_ - 1 - i + tenure_) % tenure_;   // índice circular
        if (memory_[idx] == bs->active_angles_)
            return true;   // la solución ya fue visitada — está en la lista tabú
    }
    return false;
}

void BaoTabuMemory::forbid(emili::Solution* s)
{
    const BaoSolution* bs = static_cast<const BaoSolution*>(s);

    // Escribe en la posición actual del buffer y avanza el puntero circular
    memory_[head_] = bs->active_angles_;
    head_ = (head_ + 1) % tenure_;

    // count_ crece hasta tenure_ y se mantiene ahí — el buffer es de tamaño fijo
    if (count_ < tenure_) ++count_;
}

void AdaptiveBaoTabuMemory::resizeTenure(int new_tenure)
{
    // Crea un nuevo buffer con el nuevo tamaño
    std::vector<std::vector<int>> new_mem(new_tenure);

    // Copia las entradas más recientes que quepan en el nuevo buffer
    int entries = std::min(count_, new_tenure);
    for (int i = 0; i < entries; ++i) {
        int old_idx = (head_ - 1 - i + tenure_) % tenure_;
        int new_idx = (new_tenure - 1 - i + new_tenure) % new_tenure;
        new_mem[new_idx] = memory_[old_idx];
    }

    // Reemplaza el buffer y actualiza el estado interno
    memory_  = std::move(new_mem);
    head_    = entries % new_tenure;
    count_   = entries;
    tenure_  = new_tenure;
    setTabuTenure(new_tenure);
}

void AdaptiveBaoTabuMemory::forbid(emili::Solution* s)
{
    if (tabu_check(s)) {
        // Oscilación detectada: la solución ya fue visitada → aumenta el tenure para escapar
        if (tenure_ < tenure_max_) resizeTenure(tenure_ + 1);
        since_last_revisit_ = 0;
    } else {
        ++since_last_revisit_;
        // Período tranquilo: sin revisitas por 2×tenure pasos → reduce tenure para explorar más
        if (since_last_revisit_ >= 2 * tenure_ && tenure_ > tenure_min_) {
            resizeTenure(tenure_ - 1);
            since_last_revisit_ = 0;
        }
    }

    // Registra la solución en el buffer tabú base
    BaoTabuMemory::forbid(s);
}

} // namespace imrt
} // namespace emili
