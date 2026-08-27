#!/usr/bin/env python3
"""Inspect HUD object geometry in the same controller-local frame as the game.

This is a calibration aid, not part of the runtime.  It applies COLLADA scene
transforms plus VrRotOffset/VrScale, then reports cross sections along the
controller Z axis (the configured handle axis for the current HUD objects).
"""

from __future__ import annotations

import argparse
import math
import re
import xml.etree.ElementTree as ET
from pathlib import Path

import numpy as np


NUMBER = r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?"


def parse_hud(path: Path) -> dict[str, object]:
    text = path.read_text(encoding="utf-8", errors="replace")

    def scalar(name: str, default: float) -> float:
        match = re.search(rf"\b{name}\s*=\s*\"?({NUMBER})f?\"?", text)
        return float(match.group(1)) if match else default

    def vector(name: str) -> np.ndarray:
        match = re.search(rf"\b{name}\s*=\s*\"([^\"]+)\"", text)
        if not match:
            return np.zeros(3)
        values = [float(value.rstrip("f")) for value in match.group(1).split()]
        if len(values) != 3:
            raise ValueError(f"{path}: {name} must have three values")
        return np.asarray(values, dtype=float)

    model_match = re.search(r'\bModelFile\s*=\s*"([^"]+)"', text)
    name_match = re.search(r'\bName\s*=\s*"([^"]+)"', text)
    if not model_match or not name_match:
        raise ValueError(f"{path}: missing Name or ModelFile")
    return {
        "name": name_match.group(1),
        "model": model_match.group(1),
        "translation": vector("VrTransOffset"),
        "rotation": vector("VrRotOffset"),
        "scale": scalar("VrScale", 1.0),
        "grip_point": vector("VrGripPoint"),
        "grip_radius": scalar("VrGripRadius", 0.0),
        "grip_twist": scalar("VrGripTwist", 0.0),
    }


def translation(values: np.ndarray) -> np.ndarray:
    matrix = np.eye(4)
    matrix[:3, 3] = values
    return matrix


def scaling(values: np.ndarray) -> np.ndarray:
    matrix = np.eye(4)
    matrix[0, 0], matrix[1, 1], matrix[2, 2] = values
    return matrix


def rotation(axis: np.ndarray, angle: float) -> np.ndarray:
    axis = axis / np.linalg.norm(axis)
    x, y, z = axis
    c, s = math.cos(angle), math.sin(angle)
    one_minus_c = 1.0 - c
    matrix = np.eye(4)
    matrix[:3, :3] = np.asarray(
        [
            [c + x * x * one_minus_c, x * y * one_minus_c - z * s, x * z * one_minus_c + y * s],
            [y * x * one_minus_c + z * s, c + y * y * one_minus_c, y * z * one_minus_c - x * s],
            [z * x * one_minus_c - y * s, z * y * one_minus_c + x * s, c + z * z * one_minus_c],
        ]
    )
    return matrix


def euler_xyz(values: np.ndarray) -> np.ndarray:
    # HPL cMath::MatrixRotate(XYZ) pre-multiplies X, Y, then Z.
    rx = rotation(np.asarray([1.0, 0.0, 0.0]), values[0])
    ry = rotation(np.asarray([0.0, 1.0, 0.0]), values[1])
    rz = rotation(np.asarray([0.0, 0.0, 1.0]), values[2])
    return rz @ ry @ rx


def local_name(element: ET.Element) -> str:
    return element.tag.rsplit("}", 1)[-1]


def element_transform(node: ET.Element) -> np.ndarray:
    matrix = np.eye(4)
    for child in node:
        tag = local_name(child)
        values = np.fromstring(child.text or "", sep=" ")
        if tag == "translate" and len(values) == 3:
            matrix = matrix @ translation(values)
        elif tag == "rotate" and len(values) == 4:
            matrix = matrix @ rotation(values[:3], math.radians(values[3]))
        elif tag == "scale" and len(values) == 3:
            matrix = matrix @ scaling(values)
        elif tag == "matrix" and len(values) == 16:
            matrix = matrix @ values.reshape((4, 4)).T
    return matrix


def load_collada_vertices(path: Path) -> np.ndarray:
    root = ET.parse(path).getroot()
    arrays: dict[str, np.ndarray] = {}
    for element in root.iter():
        if local_name(element) != "float_array":
            continue
        identifier = element.attrib.get("id", "")
        if "Position" in identifier or "position" in identifier:
            arrays[identifier] = np.fromstring(element.text or "", sep=" ").reshape((-1, 3))

    geometry_positions: dict[str, np.ndarray] = {}
    for geometry in (element for element in root.iter() if local_name(element) == "geometry"):
        for source in (element for element in geometry.iter() if local_name(element) == "source"):
            float_array = next((element for element in source if local_name(element) == "float_array"), None)
            if float_array is None:
                continue
            identifier = float_array.attrib.get("id", "")
            if identifier in arrays:
                geometry_positions[geometry.attrib["id"]] = arrays[identifier]
                break

    visual_scene = next(element for element in root.iter() if local_name(element) == "visual_scene")
    transformed: list[np.ndarray] = []

    def walk(node: ET.Element, parent: np.ndarray) -> None:
        world = parent @ element_transform(node)
        for child in node:
            tag = local_name(child)
            if tag == "instance_geometry":
                identifier = child.attrib["url"].lstrip("#")
                # Billboards are large camera-facing quads, not physical parts
                # of the held object, and would dominate the grip profile.
                if identifier.lower().startswith("_bb_"):
                    continue
                positions = geometry_positions.get(identifier)
                if positions is not None:
                    homogeneous = np.column_stack((positions, np.ones(len(positions))))
                    transformed.append((world @ homogeneous.T).T[:, :3])
            elif tag == "node":
                walk(child, world)

    for node in (element for element in visual_scene if local_name(element) == "node"):
        walk(node, np.eye(4))
    if not transformed:
        raise ValueError(f"{path}: no scene geometry found")
    return np.vstack(transformed)


def circle_from_points(points: np.ndarray) -> tuple[np.ndarray, float]:
    # Least-squares circle in XY: x^2+y^2 = 2*cx*x + 2*cy*y + c.
    xy = points[:, :2]
    lhs = np.column_stack((2.0 * xy[:, 0], 2.0 * xy[:, 1], np.ones(len(xy))))
    rhs = np.sum(xy * xy, axis=1)
    cx, cy, constant = np.linalg.lstsq(lhs, rhs, rcond=None)[0]
    radius = math.sqrt(max(0.0, constant + cx * cx + cy * cy))
    return np.asarray([cx, cy]), radius


def report_hand(path: Path, hud_path: Path, left: bool) -> None:
    root = ET.parse(path).getroot()
    hand_root = next(
        element
        for element in root.iter()
        if local_name(element) == "node" and element.attrib.get("id") == "Hand_Root"
    )
    hold_angles = {
        "Middle": (55.0, 70.0, 40.0),
        "Ring": (55.0, 70.0, 40.0),
        "Little": (55.0, 70.0, 40.0),
        "Index": (52.0, 66.0, 38.0),
        "Thumb": (28.0, 34.0, 20.0),
    }
    finger_axis = np.asarray([0.0, 0.0, 1.0 if left else -1.0])
    thumb_axis = np.asarray([0.0, 0.94, 0.342 if left else -0.342])
    def build_joints(pose_weight: float) -> dict[str, np.ndarray]:
        joints: dict[str, np.ndarray] = {}

        def walk(node: ET.Element, parent: np.ndarray) -> None:
            identifier = node.attrib.get("id", "")
            local = element_transform(node)
            match = re.match(r"(Middle|Ring|Little|Index|Thumb)([123])$", identifier)
            if match:
                axis = thumb_axis if match.group(1) == "Thumb" else finger_axis
                angle = math.radians(
                    hold_angles[match.group(1)][int(match.group(2)) - 1] * pose_weight
                )
                local = local @ rotation(axis, angle)
            world = parent @ local
            if identifier:
                joints[identifier] = world[:3, 3]
            for child in node:
                if local_name(child) == "node":
                    walk(child, world)

        walk(hand_root, np.eye(4))
        return joints

    joints = build_joints(1.0)
    hud = parse_hud(hud_path)
    hand_transform = translation(hud["translation"]) @ euler_xyz(hud["rotation"]) @ scaling(
        np.full(3, hud["scale"])
    )

    print(f"\n{'Left' if left else 'Right'} hand hold geometry")
    for finger in ("Index", "Middle", "Ring", "Little", "Thumb"):
        local_points = np.asarray([joints[f"{finger}{index}"] for index in (1, 2, 3)])
        local_scaled = local_points * float(hud["scale"])
        local_center, local_radius = circle_from_points(local_scaled)
        homogeneous = np.column_stack((local_points, np.ones(3)))
        controller_points = (hand_transform @ homogeneous.T).T[:, :3]
        center, radius = circle_from_points(controller_points)
        print(
            f"  {finger:6s}: joints="
            + " ".join(f"({p[0]: .4f},{p[1]: .4f},{p[2]: .4f})" for p in controller_points)
            + f"  local circle=({local_center[0]: .4f},{local_center[1]: .4f},z~{local_scaled[:, 2].mean(): .4f}) r={local_radius:.4f}"
            + f"  controller XY=({center[0]: .4f},{center[1]: .4f}) r={radius:.4f}"
        )

    print("  average long-finger grip centre by pose weight (hand-local metres):")
    for pose_weight in (0.72, 0.80, 0.90, 1.00):
        weighted_joints = build_joints(pose_weight)
        centres = []
        radii = []
        for finger in ("Index", "Middle", "Ring", "Little"):
            points = (
                np.asarray([weighted_joints[f"{finger}{index}"] for index in (1, 2, 3)])
                * float(hud["scale"])
            )
            centre, finger_radius = circle_from_points(points)
            centres.append(centre)
            radii.append(finger_radius)
        mean = np.mean(centres, axis=0)
        print(
            f"    weight {pose_weight:.2f}: centre=({mean[0]:.4f},{mean[1]:.4f}) "
            f"joint-arc radius={np.mean(radii):.4f}"
        )


def find_model(name: str, roots: list[Path]) -> Path:
    for root in roots:
        candidate = root / name
        if candidate.exists():
            return candidate
    raise FileNotFoundError(name)


def report(hud_path: Path, model_roots: list[Path], bins: int) -> None:
    hud = parse_hud(hud_path)
    model_path = find_model(str(hud["model"]), model_roots)
    positions = load_collada_vertices(model_path)
    transform = euler_xyz(hud["rotation"]) @ scaling(np.full(3, hud["scale"]))
    homogeneous = np.column_stack((positions, np.ones(len(positions))))
    positions = (transform @ homogeneous.T).T[:, :3]

    minimum = positions.min(axis=0)
    maximum = positions.max(axis=0)
    grip_point = np.asarray(hud["grip_point"])
    if float(hud["grip_radius"]) > 0.0 and (
        np.any(grip_point < minimum - 0.001) or np.any(grip_point > maximum + 0.001)
    ):
        raise ValueError(
            f"{hud_path}: grip point {grip_point} lies outside transformed model bounds "
            f"{minimum}..{maximum}"
        )
    print(f"\n{hud['name']}  ({model_path})")
    print(
        "  local bounds after HUD rotation/scale: "
        f"x [{minimum[0]: .4f}, {maximum[0]: .4f}]  "
        f"y [{minimum[1]: .4f}, {maximum[1]: .4f}]  "
        f"z [{minimum[2]: .4f}, {maximum[2]: .4f}]"
    )
    print(f"  current translation: {np.asarray(hud['translation'])}")
    print(
        f"  grip point/radius/twist: {grip_point} / {hud['grip_radius']:.4f} m / "
        f"{math.degrees(float(hud['grip_twist'])):.1f} deg (inside geometry)"
    )

    edges = np.linspace(minimum[2], maximum[2], bins + 1)
    for index in range(bins):
        lower, upper = edges[index], edges[index + 1]
        mask = (positions[:, 2] >= lower) & (positions[:, 2] <= upper)
        section = positions[mask]
        if len(section) < 4:
            continue
        low = np.quantile(section[:, :2], 0.05, axis=0)
        high = np.quantile(section[:, :2], 0.95, axis=0)
        width = high - low
        center = (high + low) * 0.5
        print(
            f"    z {lower: .4f}..{upper: .4f}: n={len(section):4d} "
            f"center=({center[0]: .4f},{center[1]: .4f}) "
            f"width=({width[0]: .4f},{width[1]: .4f})"
        )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("hud", nargs="*", help="HUD filenames; defaults to all equipped objects")
    parser.add_argument("--bins", type=int, default=16)
    parser.add_argument(
        "--game-models",
        type=Path,
        default=Path(r"C:\Program Files (x86)\Steam\steamapps\common\Penumbra Overture\redist\models\hud_objects"),
    )
    args = parser.parse_args()

    repository = Path(__file__).resolve().parents[1]
    hud_root = repository / "data" / "models" / "hud_objects"
    names = args.hud or [
        "hud_object_flashlight.hud",
        "hud_object_glowstick.hud",
        "hud_object_flare.hud",
        "hud_object_dynamite.hud",
        "hud_object_hammer.hud",
        "hud_object_pickaxe.hud",
        "hud_object_broom.hud",
    ]
    roots = [hud_root, args.game_models]
    report_hand(
        hud_root / "hud_object_hand_rig.dae",
        hud_root / "hud_object_hand_rig.hud",
        left=False,
    )
    report_hand(
        hud_root / "hud_object_hand_left_rig.dae",
        hud_root / "hud_object_hand_left_rig.hud",
        left=True,
    )
    for name in names:
        report(hud_root / name, roots, args.bins)


if __name__ == "__main__":
    main()
