#ifndef IMRT_INSTANCE_H
#define IMRT_INSTANCE_H

#include <vector>
#include <string>
#include <map>
#include <unordered_map>

namespace emili {
namespace imrt {

/*---------------------------------------------------------------------------*
 * Sparse dose entry: one non-zero cell in the dose deposition matrix.
 * dose_rate = Gy·s / (monitor unit) — dose deposited in boxet b
 *             by dimlet d operating at unit intensity.
 *---------------------------------------------------------------------------*/
struct DoseEntry {
    int boxet_id;   // 0-based local boxet index (within this organ)
    int dimlet_id;  // 0-based GLOBAL dimlet index (across all angles)
    double dose_rate;
};

/*---------------------------------------------------------------------------*
 * CT grid geometry (populated when loading CORT-format instances).
 *---------------------------------------------------------------------------*/
struct GridInfo {
    int    nx, ny, nz;   // voxel counts per dimension
    int    n_voxels;     // total = nx * ny * nz
    double x0, y0, z0;  // position of first voxel (cm)
    double dx, dy, dz;  // voxel size (cm)
    double isocenter[3];

    GridInfo() : nx(0), ny(0), nz(0), n_voxels(0),
                 x0(0), y0(0), z0(0), dx(0), dy(0), dz(0)
    { isocenter[0] = isocenter[1] = isocenter[2] = 0.0; }
};

/*---------------------------------------------------------------------------*
 * One organ (PTV or OAR) with its DVH parameters and sparse dose data.
 *---------------------------------------------------------------------------*/
struct OrganData {
    std::string name;     // e.g. "PROSTATE", "BLADDER", "RECTUM"
    bool        is_ptv;   // true = Planning Target Volume (tumor)
    int         n_boxets; // number of voxels belonging to this organ

    // Clinical dose constraints
    double Dmin;          // min dose to PTV (Gy)  [ignored for OAR]
    double Dmax;          // max tolerated dose for OAR (Gy) [ignored for PTV]
    double vol_frac;      // DVH: fraction of volume that may exceed dose_limit
    double dose_limit;    // DVH: dose limit at vol_frac (Gy)

    // EUD (Equivalent Uniform Dose) exponent
    //   PTV:  a_eud < 0  (negative → generalised mean → minimum dose)
    //   OAR:  a_eud > 1  (large positive → generalised mean → maximum hot-spot)
    double a_eud;

    // Sparse entries for this organ (all angles combined)
    std::vector<DoseEntry> entries;

    // CORT format: global 0-based voxel indices for this organ
    // (empty in the old per-(organ,angle) text format)
    std::vector<int>             voxel_indices;  // VOIList: global voxel IDs
    std::unordered_map<int,int>  voxel_to_local; // global voxel ID → local boxet index
};

/*---------------------------------------------------------------------------*
 * ImrtInstance
 *
 * Aggregates the dose deposition data for a full IMRT treatment plan.
 *
 * Instance file layout on disk (one directory):
 *
 *   instance_config.txt        ← global parameters and organ list
 *   PROSTATE_81.txt            ← sparse dose matrix: PROSTATE, angle 81°
 *   BLADDER_81.txt             ← BLADDER, angle 81°
 *   RECTUM_81.txt              ← RECTUM, angle 81°
 *   PROSTATE_162.txt
 *   ...
 *
 * Sparse file format (space-separated, comments with #):
 *   boxet_id  dimlet_id  dose_rate
 *   ...
 *   (dimlet_id is LOCAL to the angle; ImrtInstance re-indexes globally)
 *
 * instance_config.txt format:
 *   angles     <n>  <ang1> <ang2> ...
 *   dimlets    <d_per_angle>              # same for every angle
 *   ptv        <ORGAN_NAME>  <Dmin>
 *   oar        <ORGAN_NAME>  <Dmax> <vol_frac> <dose_limit>
 *   w_under    <weight>                   # PTV underdose penalty weight
 *   w_over     <weight>                   # OAR overdose penalty weight
 *   max_intensity <value>                 # upper bound on each dimlet
 *---------------------------------------------------------------------------*/
class ImrtInstance {
public:
    // ---- geometry ----
    int n_angles;
    int n_dimlets_per_angle;
    int n_dimlets;          // total = n_angles * n_dimlets_per_angle
    int n_boxets_total;     // sum over all organs

    std::vector<int>         angles;          // angle values in degrees
    std::vector<int>         couch_angles;    // couch angle per beam (default 0)
    std::vector<OrganData>   organs;          // organs in order

    // ---- solver / objective parameters ----
    double w_under;         // PTV underdose penalty weight
    double w_over;          // OAR overdose penalty weight
    double w_ptv_over;      // PTV overdose penalty weight (0 = disabled)
    double max_intensity;   // upper bound on each dimlet intensity

    // ---- CORT format fields ----
    bool     cort_format;   // true = CORT-style directory layout
    GridInfo grid;          // CT grid geometry (CORT only)

    ImrtInstance()
        : n_angles(0), n_dimlets_per_angle(0), n_dimlets(0),
          n_boxets_total(0), w_under(1.0), w_over(0.1), w_ptv_over(0.0),
          max_intensity(10.0), cort_format(false)
    {}

    /**
     * Loads the instance from a directory containing instance_config.txt
     * and the organ-angle sparse files.
     *
     * Two layouts are supported (auto-detected from instance_config.txt):
     *
     *   Old format  — one text file per (organ, angle):
     *     ORGANNAME_<angle>.txt  with lines: boxet_id  local_dimlet_id  dose_rate
     *
     *   CORT format — one file per angle plus VOILists (produced by cort_to_emili.py):
     *     instance_config.txt  contains "voilist <organ> <file>" entries
     *     ORGANNAME_VOILIST.txt  with 0-based global voxel indices
     *     Gantry<g>_Couch<c>_D.txt  with 0-based: global_voxel_id  local_beamlet_id  dose_rate
     *
     * Returns true on success.
     */
    bool loadFromDirectory(const char* dir_path);

    /**
     * Returns the index of organ `name` in the organs vector, or -1.
     */
    int findOrgan(const std::string& name) const;

    /**
     * Global dimlet index from (angle_idx, local_dimlet_idx).
     */
    int globalDimletIndex(int angle_idx, int local_dimlet_idx) const
    {
        return angle_idx * n_dimlets_per_angle + local_dimlet_idx;
    }

    /**
     * Computes, for each organ, the vector of received doses given
     * the global intensity vector x (length = n_dimlets).
     * doses[organ_idx][boxet_local_idx] = computed dose.
     */
    std::vector<std::vector<double>>
    computeOrganDoses(const std::vector<double>& x) const;

private:
    std::map<std::string,std::string> voilist_files_; // organ name → VOIList path

    bool loadConfig(const std::string& config_path);

    // Old format: one text file per (organ, angle)
    bool loadOrganAngleFile(const std::string& file_path,
                            int organ_idx,
                            int angle_idx);

    // CORT format helpers
    bool loadVoiListFile(int organ_idx, const std::string& file_path);
    bool loadAngleDijFileCort(const std::string& file_path, int angle_idx);
};

/*---------------------------------------------------------------------------*
 * reportPlan — clinical-style summary of a fluence map.
 *
 * Given an intensity vector x (length inst.n_dimlets) and the instance,
 * prints to `os` a literature-style report:
 *
 *   - selected angles, FMO objective value
 *   - per-organ Dmin, Dmean, Dmax, D95, D5, D2  (Gy)
 *   - DVH constraint table (V70 etc.) flagged OK / VIOL
 *   - Conformity Index (CI), Homogeneity Index (HI) per ICRU-83
 *
 * If dvh_csv_path is non-empty, also writes one row per organ × dose
 * percentile (1..99) usable for plotting the DVH curves.
 *---------------------------------------------------------------------------*/
void reportPlan(const ImrtInstance& inst,
                const std::vector<double>& x,
                double                     fmo_objective,
                const std::vector<int>&    angles_deg,
                std::ostream&              os,
                const std::string&         dvh_csv_path = "");

} // namespace imrt
} // namespace emili

#endif // IMRT_INSTANCE_H
