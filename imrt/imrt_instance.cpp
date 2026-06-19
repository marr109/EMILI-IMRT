#include "imrt_instance.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <cstring>

namespace emili {
namespace imrt {

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static std::string trim(const std::string& s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static bool isComment(const std::string& line)
{
    std::string t = trim(line);
    return t.empty() || t[0] == '#';
}

// ─────────────────────────────────────────────────────────────────────────────
// ImrtInstance::loadFromDirectory
// ─────────────────────────────────────────────────────────────────────────────

bool ImrtInstance::loadFromDirectory(const char* dir_path)
{
    std::string dir(dir_path);
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
        dir += '/';

    std::string config_path = dir + "instance_config.txt";
    if (!loadConfig(config_path)) {
        std::cerr << "[IMRT] Failed to load config: " << config_path << std::endl;
        return false;
    }

    n_dimlets = n_angles * n_dimlets_per_angle;

    if (cort_format) {
        // ── CORT format: VOILists + per-angle full-grid Dij ────────────────

        // 1. Load VOILists to establish organ membership
        for (int o = 0; o < static_cast<int>(organs.size()); ++o) {
            const std::string& oname = organs[o].name;
            std::string voi_path;

            auto it = voilist_files_.find(oname);
            if (it != voilist_files_.end()) {
                // Path from config: absolute or relative to instance dir
                voi_path = (it->second[0] == '/' || it->second[0] == '\\')
                           ? it->second : dir + it->second;
            } else {
                voi_path = dir + oname + "_VOILIST.txt";
            }

            if (!loadVoiListFile(o, voi_path)) {
                std::cerr << "[IMRT] Warning: could not load VOIList " << voi_path
                          << " — organ " << oname << " will have no voxels." << std::endl;
            }
        }

        // 2. Count total boxets from VOIList sizes
        n_boxets_total = 0;
        for (auto& org : organs)
            n_boxets_total += org.n_boxets;

        // 3. Load per-angle Dij (filtered to organ voxels)
        int n_loaded = 0;
        for (int a = 0; a < n_angles; ++a) {
            int couch = (a < static_cast<int>(couch_angles.size()))
                        ? couch_angles[a] : 0;
            std::string fname = dir
                + "Gantry"  + std::to_string(angles[a])
                + "_Couch"  + std::to_string(couch)
                + "_D.txt";
            if (!loadAngleDijFileCort(fname, a)) {
                std::cerr << "[IMRT] Warning: could not load " << fname
                          << " — skipping angle " << angles[a] << "." << std::endl;
            } else {
                ++n_loaded;
            }
        }

        std::cout << "[IMRT] CORT instance loaded: "
                  << n_loaded << "/" << n_angles << " angles, "
                  << n_dimlets << " dimlets, "
                  << n_boxets_total << " boxets, "
                  << organs.size() << " organs." << std::endl;

    } else {
        // ── Old format: one text file per (organ, angle) ───────────────────

        for (int a = 0; a < n_angles; ++a) {
            for (int o = 0; o < static_cast<int>(organs.size()); ++o) {
                std::string fname = dir + organs[o].name
                                  + "_" + std::to_string(angles[a]) + ".txt";
                if (!loadOrganAngleFile(fname, o, a)) {
                    std::cerr << "[IMRT] Warning: could not load " << fname
                              << " — skipping." << std::endl;
                }
            }
        }

        n_boxets_total = 0;
        for (auto& org : organs)
            n_boxets_total += org.n_boxets;

        std::cout << "[IMRT] Instance loaded: "
                  << n_angles << " angles, "
                  << n_dimlets << " dimlets, "
                  << n_boxets_total << " boxets, "
                  << organs.size() << " organs." << std::endl;
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ImrtInstance::loadConfig
// ─────────────────────────────────────────────────────────────────────────────

bool ImrtInstance::loadConfig(const std::string& config_path)
{
    std::ifstream f(config_path);
    if (!f.is_open()) {
        std::cerr << "[IMRT] Cannot open config: " << config_path << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(f, line)) {
        if (isComment(line)) continue;

        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if (key == "angles") {
            iss >> n_angles;
            angles.resize(n_angles);
            couch_angles.assign(n_angles, 0);
            for (int i = 0; i < n_angles; ++i)
                iss >> angles[i];
        }
        else if (key == "dimlets") {
            iss >> n_dimlets_per_angle;
        }
        else if (key == "ptv") {
            OrganData org;
            org.a_eud = -10.0;
            iss >> org.name >> org.Dmin;
            iss >> org.a_eud;
            org.is_ptv    = true;
            org.Dmax      = 0.0;
            org.vol_frac  = 0.0;
            org.dose_limit= 0.0;
            org.n_boxets  = 0;
            organs.push_back(org);
        }
        else if (key == "oar") {
            OrganData org;
            org.a_eud = 10.0;
            iss >> org.name >> org.Dmax >> org.vol_frac >> org.dose_limit;
            iss >> org.a_eud;
            org.is_ptv = false;
            org.Dmin   = 0.0;
            org.n_boxets = 0;
            organs.push_back(org);
        }
        else if (key == "voilist") {
            // voilist <organ_name> <file_path>
            // Presence of any voilist line activates CORT format.
            std::string oname, fpath;
            iss >> oname >> fpath;
            voilist_files_[oname] = fpath;
            cort_format = true;
        }
        else if (key == "w_under")     { iss >> w_under; }
        else if (key == "w_over")      { iss >> w_over; }
        else if (key == "w_ptv_over")  { iss >> w_ptv_over; }
        else if (key == "max_intensity"){ iss >> max_intensity; }
    }

    return (n_angles > 0 && n_dimlets_per_angle > 0 && !organs.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// ImrtInstance::loadOrganAngleFile
//
// File format (space-separated, comments with #):
//   boxet_id  local_dimlet_id  dose_rate
//
// boxet_id and local_dimlet_id are 1-based in the files
// (converted to 0-based internally).
// ─────────────────────────────────────────────────────────────────────────────

bool ImrtInstance::loadOrganAngleFile(const std::string& file_path,
                                      int organ_idx,
                                      int angle_idx)
{
    std::ifstream f(file_path);
    if (!f.is_open()) return false;

    OrganData& org = organs[organ_idx];
    int max_boxet = 0;

    std::string line;
    while (std::getline(f, line)) {
        if (isComment(line)) continue;

        std::istringstream iss(line);
        int   boxet_local, dimlet_local;
        double dose;

        if (!(iss >> boxet_local >> dimlet_local >> dose)) continue;
        if (dose == 0.0) continue; // skip explicit zeros (already sparse)

        DoseEntry e;
        e.boxet_id  = boxet_local - 1; // convert to 0-based
        e.dimlet_id = globalDimletIndex(angle_idx, dimlet_local - 1);
        e.dose_rate = dose;

        org.entries.push_back(e);

        if (e.boxet_id > max_boxet)
            max_boxet = e.boxet_id;
    }

    // Update n_boxets (track the highest boxet_id seen for this organ)
    if (max_boxet + 1 > org.n_boxets)
        org.n_boxets = max_boxet + 1;

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ImrtInstance::findOrgan
// ─────────────────────────────────────────────────────────────────────────────

int ImrtInstance::findOrgan(const std::string& name) const
{
    for (int i = 0; i < static_cast<int>(organs.size()); ++i)
        if (organs[i].name == name) return i;
    return -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// ImrtInstance::computeOrganDoses
//
// Returns doses[organ_idx][local_boxet_idx] for all organs.
// Complexity: O(total non-zero entries in all sparse matrices).
// ─────────────────────────────────────────────────────────────────────────────

std::vector<std::vector<double>>
ImrtInstance::computeOrganDoses(const std::vector<double>& x) const
{
    int n_org = static_cast<int>(organs.size());
    std::vector<std::vector<double>> doses(n_org);
    for (int o = 0; o < n_org; ++o)
        doses[o].assign(organs[o].n_boxets, 0.0);

    for (int o = 0; o < n_org; ++o) {
        for (const DoseEntry& e : organs[o].entries) {
            if (e.dimlet_id < static_cast<int>(x.size()))
                doses[o][e.boxet_id] += e.dose_rate * x[e.dimlet_id];
        }
    }

    return doses;
}

// ─────────────────────────────────────────────────────────────────────────────
// ImrtInstance::loadVoiListFile  (CORT format)
//
// File format — 0-based global voxel indices, one per line, comments with #:
//   # VOIList for PTV
//   45823
//   45824
//   ...
// ─────────────────────────────────────────────────────────────────────────────

bool ImrtInstance::loadVoiListFile(int organ_idx, const std::string& file_path)
{
    std::ifstream f(file_path);
    if (!f.is_open()) return false;

    OrganData& org = organs[organ_idx];
    org.voxel_indices.clear();
    org.voxel_to_local.clear();

    std::string line;
    while (std::getline(f, line)) {
        if (isComment(line)) continue;

        std::istringstream iss(line);
        int voxel_id;
        if (!(iss >> voxel_id)) continue;

        int local_idx = static_cast<int>(org.voxel_indices.size());
        org.voxel_indices.push_back(voxel_id);
        org.voxel_to_local[voxel_id] = local_idx;
    }

    org.n_boxets = static_cast<int>(org.voxel_indices.size());
    return (org.n_boxets > 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// ImrtInstance::loadAngleDijFileCort  (CORT format)
//
// File format — 0-based indices, space-separated, comments with #:
//   global_voxel_id  local_beamlet_id  dose_rate
//   45823 0 0.002341
//   ...
//
// Only rows whose global_voxel_id appears in some organ's VOIList are stored.
// The entry is assigned to every organ that claims that voxel (organs are
// typically non-overlapping in clinical datasets).
// ─────────────────────────────────────────────────────────────────────────────

bool ImrtInstance::loadAngleDijFileCort(const std::string& file_path, int angle_idx)
{
    std::ifstream f(file_path);
    if (!f.is_open()) return false;

    const int n_org = static_cast<int>(organs.size());

    std::string line;
    while (std::getline(f, line)) {
        if (isComment(line)) continue;

        std::istringstream iss(line);
        int    global_voxel, local_beamlet;
        double dose;

        if (!(iss >> global_voxel >> local_beamlet >> dose)) continue;
        if (dose == 0.0) continue;

        for (int o = 0; o < n_org; ++o) {
            auto it = organs[o].voxel_to_local.find(global_voxel);
            if (it == organs[o].voxel_to_local.end()) continue;

            DoseEntry e;
            e.boxet_id  = it->second;
            e.dimlet_id = globalDimletIndex(angle_idx, local_beamlet);
            e.dose_rate = dose;
            organs[o].entries.push_back(e);
        }
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// reportPlan — literature-style clinical summary of a fluence map.
//
// Output layout (per ICRU-83 conventions):
//   Header block  : instance summary, K, selected angles, objective, OSQP info
//   Per-organ     : Dmin, Dmean, Dmax, D95, D5, D2  (Gy)
//   Constraints   : DVH goals vs achieved (Status OK / VIOL)
//   Indices       : CI (V_Rx / V_PTV), HI = (D2 - D98) / D_Rx, V95% coverage
//
// dvh_csv_path (optional): writes "organ,dose_gy,volume_pct" rows for plotting.
// ─────────────────────────────────────────────────────────────────────────────

static double percentileSorted(const std::vector<double>& sorted, double p)
{
    if (sorted.empty()) return 0.0;
    // p in [0, 1].  Pre-condition: input sorted ascending.
    size_t idx = (size_t)std::floor(p * (sorted.size() - 1));
    return sorted[idx];
}

static double volumeAtOrAbove(const std::vector<double>& sorted, double dose_gy)
{
    // Returns fraction (0..1) of voxels with dose >= dose_gy.
    auto it = std::lower_bound(sorted.begin(), sorted.end(), dose_gy);
    return (double)(sorted.end() - it) / (double)sorted.size();
}

void reportPlan(const ImrtInstance& inst,
                const std::vector<double>& x,
                double                     fmo_objective,
                const std::vector<int>&    angles_deg,
                std::ostream&              os,
                const std::string&         dvh_csv_path)
{
    auto doses = inst.computeOrganDoses(x);

    // ── Sorted dose vector per organ (ascending) ─────────────────────────────
    std::vector<std::vector<double>> sorted = doses;
    for (auto& v : sorted) std::sort(v.begin(), v.end());

    // ── PTV reference (first PTV organ) for CI/HI ────────────────────────────
    int ptv_idx = -1;
    double D_Rx = 0.0;
    for (int o = 0; o < (int)inst.organs.size(); ++o) {
        if (inst.organs[o].is_ptv) { ptv_idx = o; D_Rx = inst.organs[o].Dmin; break; }
    }

    // ── Header ───────────────────────────────────────────────────────────────
    os << "================================================================\n";
    os << "  IMRT FMO RESULT   K=" << angles_deg.size() << " beams\n";
    os << "================================================================\n";
    os << "Selected angles (deg) : [ ";
    for (size_t i = 0; i < angles_deg.size(); ++i) {
        if (i) os << ", ";
        os << angles_deg[i];
    }
    os << " ]\n";
    os << "FMO objective f*      : " << std::fixed << std::setprecision(2)
       << fmo_objective << "\n";
    if (D_Rx > 0.0)
        os << "Prescription dose Rx  : " << std::setprecision(2) << D_Rx << " Gy\n";

    // ── Per-organ dose statistics ────────────────────────────────────────────
    os << "\n----------------------------------------------------------------\n";
    os << "  Per-organ dose statistics  (Gy)\n";
    os << "----------------------------------------------------------------\n";
    os << std::left << std::setw(11) << "Structure"
       << std::right << std::setw(8) << "Voxels"
       << std::setw(8) << "Dmin"
       << std::setw(8) << "Dmean"
       << std::setw(8) << "Dmax"
       << std::setw(8) << "D95"
       << std::setw(8) << "D5"
       << std::setw(8) << "D2" << "\n";
    os << "----------------------------------------------------------------\n";

    for (int o = 0; o < (int)inst.organs.size(); ++o) {
        const auto& s = sorted[o];
        if (s.empty()) continue;
        double dmin  = s.front();
        double dmax  = s.back();
        double dmean = std::accumulate(s.begin(), s.end(), 0.0) / s.size();
        double d95   = percentileSorted(s, 0.05);  // dose at 95% volume
        double d5    = percentileSorted(s, 0.95);
        double d2    = percentileSorted(s, 0.98);

        os << std::left << std::setw(11) << inst.organs[o].name
           << std::right << std::setw(8) << inst.organs[o].n_boxets
           << std::fixed << std::setprecision(2)
           << std::setw(8) << dmin
           << std::setw(8) << dmean
           << std::setw(8) << dmax;
        if (inst.organs[o].is_ptv) {
            os << std::setw(8) << d95;
        } else {
            os << std::setw(8) << "-";
        }
        os << std::setw(8) << d5
           << std::setw(8) << d2 << "\n";
    }
    os << "----------------------------------------------------------------\n";

    // ── DVH constraints table ────────────────────────────────────────────────
    os << "\n----------------------------------------------------------------\n";
    os << "  DVH constraints  (clinical goals)\n";
    os << "----------------------------------------------------------------\n";
    os << std::left << std::setw(11) << "Structure"
       << std::setw(16) << "Metric"
       << std::setw(12) << "Goal"
       << std::setw(14) << "Achieved"
       << "Status\n";
    os << "----------------------------------------------------------------\n";

    for (int o = 0; o < (int)inst.organs.size(); ++o) {
        const OrganData& org = inst.organs[o];
        const auto& s = sorted[o];
        if (s.empty()) continue;

        if (org.is_ptv) {
            double d95 = percentileSorted(s, 0.05);
            double d2  = percentileSorted(s, 0.98);
            double goal_d95 = 0.95 * org.Dmin;
            double goal_d2  = 1.07 * org.Dmin;   // ICRU-83 hot-spot tolerance

            os << std::left << std::setw(11) << org.name
               << std::setw(16) << "D95 >= 0.95*Rx"
               << std::fixed << std::setprecision(2)
               << std::setw(12) << goal_d95
               << std::setw(14) << d95
               << (d95 >= goal_d95 ? "OK" : "VIOL") << "\n";

            os << std::left << std::setw(11) << org.name
               << std::setw(16) << "D2  <= 1.07*Rx"
               << std::fixed << std::setprecision(2)
               << std::setw(12) << goal_d2
               << std::setw(14) << d2
               << (d2 <= goal_d2 ? "OK" : "VIOL") << "\n";
        } else {
            double dmean = std::accumulate(s.begin(), s.end(), 0.0) / s.size();
            double v70 = 100.0 * volumeAtOrAbove(s, 70.0);
            double goal_v70 = 25.0;

            os << std::left << std::setw(11) << org.name
               << std::setw(16) << "Dmax <= limit"
               << std::fixed << std::setprecision(2)
               << std::setw(12) << org.Dmax
               << std::setw(14) << s.back()
               << (s.back() <= org.Dmax ? "OK" : "VIOL") << "\n";

            os << std::left << std::setw(11) << org.name
               << std::setw(16) << "V70 <= 25 %"
               << std::fixed << std::setprecision(2)
               << std::setw(12) << goal_v70
               << std::setw(14) << v70
               << (v70 <= goal_v70 ? "OK" : "VIOL") << "\n";

            os << std::left << std::setw(11) << org.name
               << std::setw(16) << "Dmean (info)"
               << std::setw(12) << "-"
               << std::setw(14) << dmean
               << "--" << "\n";
        }
    }
    os << "----------------------------------------------------------------\n";

    // ── Plan quality indices (CI, HI, coverage) ─────────────────────────────
    if (ptv_idx >= 0) {
        const auto& sp = sorted[ptv_idx];
        double d2  = percentileSorted(sp, 0.98);
        double d98 = percentileSorted(sp, 0.02);

        double v_ptv_at_Rx = volumeAtOrAbove(sp, D_Rx) * sp.size();

        // V_Rx = total voxels (any organ) receiving >= D_Rx.
        // Approximated using only organs we know — sufficient for relative CI.
        double v_total_at_Rx = 0.0;
        for (const auto& s : sorted)
            v_total_at_Rx += volumeAtOrAbove(s, D_Rx) * s.size();

        double CI = (v_ptv_at_Rx > 0.0) ? (v_total_at_Rx / v_ptv_at_Rx) : 0.0;
        double HI = (D_Rx > 0.0) ? ((d2 - d98) / D_Rx) : 0.0;
        double V95pct = 100.0 * volumeAtOrAbove(sp, 0.95 * D_Rx);

        os << "\n----------------------------------------------------------------\n";
        os << "  Plan quality indices\n";
        os << "----------------------------------------------------------------\n";
        os << "Conformity Index    CI (V_Rx / V_PTV)    = "
           << std::fixed << std::setprecision(3) << CI << "\n";
        os << "Homogeneity Index   HI (D2 - D98)/D_Rx   = "
           << std::setprecision(3) << HI << "\n";
        os << "PTV coverage        V95% of Rx           = "
           << std::setprecision(2) << V95pct << " %\n";
        os << "----------------------------------------------------------------\n";
    }

    // ── DVH CSV (optional) ──────────────────────────────────────────────────
    if (!dvh_csv_path.empty()) {
        std::ofstream csv(dvh_csv_path);
        if (csv.is_open()) {
            csv << "organ,dose_gy,volume_pct\n";
            const int N = 200;  // dose grid resolution
            for (int o = 0; o < (int)inst.organs.size(); ++o) {
                const auto& s = sorted[o];
                if (s.empty()) continue;
                double dmax = s.back();
                if (dmax <= 0.0) dmax = 1.0;
                for (int k = 0; k <= N; ++k) {
                    double dose = (dmax * k) / N;
                    double vol  = 100.0 * volumeAtOrAbove(s, dose);
                    csv << inst.organs[o].name << ","
                        << std::fixed << std::setprecision(3) << dose << ","
                        << std::setprecision(3) << vol << "\n";
                }
            }
            os << "DVH curves written to: " << dvh_csv_path << "\n";
        } else {
            os << "[reportPlan] Could not open DVH CSV: " << dvh_csv_path << "\n";
        }
    }
}

} // namespace imrt
} // namespace emili
