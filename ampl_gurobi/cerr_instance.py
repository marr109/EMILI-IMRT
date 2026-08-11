"""Native reader for raw CERR-exported dose data (instances/CERR_Prostate).

No intermediate conversion to EMILI's instance_config.txt/VOILIST format --
this reads the CERR export layout directly and loads only the requested
angles, since a full FMO solve only ever needs a fixed active-angle subset
(same convention as imrt/imrt_fmo.cpp::solve(active_angles)).

Layout read:
  <ORGAN>.txt              global voxel ids for the organ; line order = local boxet index
  <ORGAN>_<angle_idx>.txt  rows: global_voxel_id  local_beamlet_id(1-based)  dose_rate
  beamletIndex.txt         rows: angle_idx  global_beamlet_start  global_beamlet_end (1-based, inclusive)
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

import numpy as np
import pandas as pd


@dataclass
class OrganData:
    name: str
    voxel_ids: np.ndarray
    voxel_to_local: Dict[int, int]
    dimlet_id: np.ndarray = field(default_factory=lambda: np.empty(0, dtype=np.int64))
    boxet_id: np.ndarray = field(default_factory=lambda: np.empty(0, dtype=np.int64))
    dose_rate: np.ndarray = field(default_factory=lambda: np.empty(0, dtype=np.float64))

    @property
    def n_boxets(self) -> int:
        return len(self.voxel_ids)


@dataclass
class CerrInstance:
    root: Path
    n_angles_total: int
    n_dimlets_total: int
    beamlet_range: List[Tuple[int, int]]  # angle_idx -> (global_start, global_end), 1-based inclusive
    organs: Dict[str, OrganData]
    active_angles: List[int]

    def active_dimlet_ids(self) -> np.ndarray:
        ids = []
        for angle_idx in self.active_angles:
            start, end = self.beamlet_range[angle_idx]
            ids.append(np.arange(start - 1, end))  # 0-based
        return np.concatenate(ids)


def _read_beamlet_index(path: Path) -> List[Tuple[int, int]]:
    df = pd.read_csv(path, sep=r"\s+", header=None, names=["angle_idx", "start", "end"])
    df = df.sort_values("angle_idx")
    return list(zip(df["start"].to_numpy(), df["end"].to_numpy()))


def _read_voxel_list(path: Path) -> np.ndarray:
    return pd.read_csv(path, header=None, names=["voxel_id"])["voxel_id"].to_numpy()


def load_cerr_instance(
    root: str | Path,
    organ_names: Sequence[str],
    active_angles: Sequence[int],
) -> CerrInstance:
    root = Path(root)
    beamlet_range = _read_beamlet_index(root / "beamletIndex.txt")
    n_dimlets_total = beamlet_range[-1][1]

    organs: Dict[str, OrganData] = {}
    for name in organ_names:
        voxel_ids = _read_voxel_list(root / f"{name}.txt")
        voxel_to_local = {int(v): i for i, v in enumerate(voxel_ids)}
        organs[name] = OrganData(name=name, voxel_ids=voxel_ids, voxel_to_local=voxel_to_local)

    for angle_idx in active_angles:
        g_start, _g_end = beamlet_range[angle_idx]
        for name, organ in organs.items():
            fpath = root / f"{name}_{angle_idx}.txt"
            if not fpath.exists():
                continue
            df = pd.read_csv(
                fpath, sep=r"\s+", header=None,
                names=["voxel_id", "local_beamlet", "dose_rate"],
            )
            local_boxet = df["voxel_id"].map(organ.voxel_to_local)
            mask = local_boxet.notna()
            if not mask.any():
                continue
            global_dimlet = (g_start - 1) + (df.loc[mask, "local_beamlet"].to_numpy() - 1)
            organ.dimlet_id = np.concatenate([organ.dimlet_id, global_dimlet.astype(np.int64)])
            organ.boxet_id = np.concatenate(
                [organ.boxet_id, local_boxet[mask].to_numpy(dtype=np.int64)]
            )
            organ.dose_rate = np.concatenate(
                [organ.dose_rate, df.loc[mask, "dose_rate"].to_numpy(dtype=np.float64)]
            )

    return CerrInstance(
        root=root,
        n_angles_total=len(beamlet_range),
        n_dimlets_total=n_dimlets_total,
        beamlet_range=beamlet_range,
        organs=organs,
        active_angles=list(active_angles),
    )
