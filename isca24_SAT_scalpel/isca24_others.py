from lassynth import LatticeSurgerySynthesizer


KISSAT_DIR = "/usr/local/bin/"  # change this to yours

SPECS = {
    "maj": {
        "max_i": 4,
        "max_j": 4,
        "max_k": 5,
        "ports": [
            {"location": [2, 4, 1], "direction": "-J", "z_basis_direction": "I", "function": "C_in"},
            {"location": [0, 1, 0], "direction": "+I", "z_basis_direction": "K", "function": "a^prime"},
            {"location": [1, 4, 4], "direction": "-J", "z_basis_direction": "I", "function": "CCZ"},
            {"location": [4, 1, 0], "direction": "-I", "z_basis_direction": "K", "function": "a"},
            {"location": [0, 1, 1], "direction": "+I", "z_basis_direction": "K", "function": "t^prime"},
            {"location": [1, 4, 3], "direction": "-J", "z_basis_direction": "I", "function": "CCZ"},
            {"location": [4, 1, 1], "direction": "-I", "z_basis_direction": "K", "function": "t"},
            {"location": [1, 4, 2], "direction": "-J", "z_basis_direction": "I", "function": "CCZ"},
            {"location": [2, 0, 1], "direction": "+J", "z_basis_direction": "I", "function": "C_out"}
        ],
        "stabilizers": [
            "X...XXX.X",
            "Z.Z....XZ",
            ".XX.XXX.X",
            ".ZZ......",
            "...XXX...",
            "...Z.Z.XZ",
            "....ZZ...",
            "......ZXZ",
            ".......ZX"
        ],
        "optional": {
            "forbidden_cubes": [
                [0,2,0], [0,3,0],
                [0,2,1], [0,3,1],
                [0,1,2], [0,2,2], [0,3,2],
                [0,1,3], [0,2,3], [0,3,3],
                [0,1,4], [0,2,4], [0,3,4],
                [0,0,0], [1,0,0], [2,0,0], [3,0,0],
                [0,0,1], [1,0,1], [3,0,1],
                [0,0,2], [1,0,2], [2,0,2], [3,0,2],
                [0,0,3], [1,0,3], [2,0,3], [3,0,3],
                [0,0,4], [1,0,4], [2,0,4], [3,0,4]
            ]
        }
    },
    "factory_99": {
        "max_i":3,
        "max_j":3,
        "max_k":12,
        "ports":[
            {"location":[1,0,0],"direction":"+K","z_basis_direction":"I"},
            {"location":[1,2,0],"direction":"+K","z_basis_direction":"I"},
            {"location":[2,0,0],"direction":"+K","z_basis_direction":"I"},
            {"location":[2,2,0],"direction":"+K","z_basis_direction":"I"},
            {"location":[0,2,1],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,2],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,3],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,4],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,5],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,6],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,7],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,8],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,9],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,10],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,11],"direction":"-J","z_basis_direction":"I"},
            {"location":[2,1,12],"direction":"-K","z_basis_direction":"J"}
        ],
        "stabilizers":[
            "X...XXX.X..X.XX.",
            ".X..XX.XX.X.X.X.",
            "..X.X.XXXX..XX..",
            "...X.XXXXXXX....",
            "ZZZ.Z...........",
            "ZZ.Z.Z..........",
            "Z.ZZ..Z.........",
            ".ZZZ...Z........",
            "ZZZZ....Z......Z",
            "..ZZ.....Z.....Z",
            ".Z.Z......Z....Z",
            "Z..Z.......Z...Z",
            ".ZZ.........Z..Z",
            "Z.Z..........Z.Z",
            "ZZ............ZZ",
            "........XXXXXXXX"
        ],
        "optional":{
            "forbidden_cubes":[
                [1,1,0], [2,1,0], [0,2,0], [0,1,0]
            ]
        }
    },
    "factory_121": {
        "max_i":5,
        "max_j":3,
        "max_k":12,
        "ports":[
            {"location":[2,0,0],"direction":"+K","z_basis_direction":"I"},
            {"location":[2,2,0],"direction":"+K","z_basis_direction":"I"},
            {"location":[3,0,0],"direction":"+K","z_basis_direction":"I"},
            {"location":[3,2,0],"direction":"+K","z_basis_direction":"I"},
            {"location":[0,2,1],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,2],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,3],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,4],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,5],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,6],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,7],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,8],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,9],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,10],"direction":"-J","z_basis_direction":"I"},
            {"location":[0,2,11],"direction":"-J","z_basis_direction":"I"},
            {"location":[4,1,12],"direction":"-K","z_basis_direction":"J"}
        ],
        "stabilizers":[
            "X...XXX.X..X.XX.",
            ".X..XX.XX.X.X.X.",
            "..X.X.XXXX..XX..",
            "...X.XXXXXXX....",
            "ZZZ.Z...........",
            "ZZ.Z.Z..........",
            "Z.ZZ..Z.........",
            ".ZZZ...Z........",
            "ZZZZ....Z......Z",
            "..ZZ.....Z.....Z",
            ".Z.Z......Z....Z",
            "Z..Z.......Z...Z",
            ".ZZ.........Z..Z",
            "Z.Z..........Z.Z",
            "ZZ............ZZ",
            "........XXXXXXXX"
        ],
        "optional":{
            "forbidden_cubes":[
                [2,1,0],[3,1,0],[0,0,0],[0,1,0],[0,2,0],[1,0,0],[1,1,0],[1,2,0],[4,1,0],
                [4,0,0],[4,0,1],[4,0,2],[4,0,3],[4,0,4],[4,0,5],[4,0,6],
                [4,0,7],[4,0,8],[4,0,9],[4,0,10],[4,0,11],
                [4,2,0],[4,2,1],[4,2,2],[4,2,3],[4,2,4],[4,2,5],[4,2,6],[4,2,7],[4,2,8],
                [4,2,9],[4,2,10],[4,2,11],
                [1,0,0],[1,0,1],[1,0,2],[1,0,3],[1,0,4],[1,0,5],[1,0,6],
                [1,0,7],[1,0,8],[1,0,9],[1,0,10],[1,0,11],
                [0,0,0],[0,0,1],[0,0,2],[0,0,3],[0,0,4],[0,0,5],[0,0,6],
                [0,0,7],[0,0,8],[0,0,9],[0,0,10],[0,0,11]
            ]
        }
    },
    "factory_162": {
        "max_i": 9,
        "max_j": 4,
        "max_k": 4,
        "ports": [
            {"location": [0, 0, 2], "direction": "+K", "z_basis_direction": "I", "function": "T"},
            {"location": [1, 0, 2], "direction": "+K", "z_basis_direction": "I", "function": "T"},
            {"location": [2, 0, 2], "direction": "+K", "z_basis_direction": "I", "function": "T"},
            {"location": [3, 0, 2], "direction": "+K", "z_basis_direction": "I", "function": "T"},
            {"location": [5, 0, 2], "direction": "+K", "z_basis_direction": "I", "function": "T"},
            {"location": [6, 0, 2], "direction": "+K", "z_basis_direction": "I", "function": "T"},
            {"location": [7, 0, 2], "direction": "+K", "z_basis_direction": "I", "function": "T"},
            {"location": [8, 0, 2], "direction": "+K", "z_basis_direction": "I", "function": "T"},
            {"location": [0, 3, 2], "direction": "+K", "z_basis_direction": "I", "function": "T"},
            {"location": [1, 3, 2], "direction": "+K", "z_basis_direction": "I", "function": "T"},
            {"location": [2, 3, 2], "direction": "+K", "z_basis_direction": "I", "function": "T"},
            {"location": [3, 3, 2], "direction": "+K", "z_basis_direction": "I", "function": "T"},
            {"location": [5, 3, 2], "direction": "+K", "z_basis_direction": "I", "function": "T"},
            {"location": [6, 3, 2], "direction": "+K", "z_basis_direction": "I", "function": "T"},
            {"location": [7, 3, 2], "direction": "+K", "z_basis_direction": "I", "function": "T"},
            {"location": [8, 3, 4], "direction": "-K", "z_basis_direction": "I", "function": "output"}
        ],
        "stabilizers": [
            "X...XXX.X..X.XX.",
            ".X..XX.XX.X.X.X.",
            "..X.X.XXXX..XX..",
            "...X.XXXXXXX....",
            "ZZZ.Z...........",
            "ZZ.Z.Z..........",
            "Z.ZZ..Z.........",
            ".ZZZ...Z........",
            "ZZZZ....Z......Z",
            "..ZZ.....Z.....Z",
            ".Z.Z......Z....Z",
            "Z..Z.......Z...Z",
            ".ZZ.........Z..Z",
            "Z.Z..........Z.Z",
            "ZZ............ZZ",
            "........XXXXXXXX"
        ],
        "optional": {
            "forbidden_cubes": [[8, 3, 0], [8, 3, 1]],
            "top_fixups": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14]
        }
    }
}

def majority_gate():
    solver = LatticeSurgerySynthesizer(
        solver="kissat",
        kissat_dir=KISSAT_DIR)
    result = solver.solve(specification=SPECS["maj"])
    result = result.after_default_optimizations()
    result.save_lasre("majority_gate.lasre.json")
    result.to_3d_model_gltf("majority_gate.gltf", 
                            stabilizer=0, rm_dir=":+I", attach_axes=True)

def factory_162():
    solver = LatticeSurgerySynthesizer(
        solver="kissat",
        kissat_dir=KISSAT_DIR)
    result162 = solver.solve(specification=SPECS["factory_162"])
    result162 = result162.after_default_optimizations()
    result162.save_lasre("factory162.lasre.json")
    result162 = result162.after_t_factory_default_optimizations()
    result162.save_lasre("factory162_withfixup.lasre.json")
    result162.to_3d_model_gltf("factory162.gltf", attach_axes=True)


def factory_nodelay():
    solver = LatticeSurgerySynthesizer(
        solver="kissat",
        kissat_dir=KISSAT_DIR)
    result121 = solver.solve(specification=SPECS["factory_121"])
    result121 = result121.after_default_optimizations()
    result121.save_lasre("factory121.lasre.json")
    result121.to_3d_model_gltf("factory121.gltf", attach_axes=True)

    result99 = solver.solve(specification=SPECS["factory_99"])
    result99 = result99.after_default_optimizations()
    result99.save_lasre("factory99.lasre.json")
    result99.to_3d_model_gltf("factory99.gltf", attach_axes=True)


if __name__ == "__main__":
    majority_gate()
    factory_nodelay()
    factory_162()
