# ChessBotX

Analyseur d'échecs visuel avec overlay, basé sur la capture d'écran, la reconnaissance de pièces par template matching et le moteur Stockfish.

## Dépendances

```bash
sudo apt install cmake g++ qt6-base-dev libopencv-dev nlohmann-json3-dev \
                 libspdlog-dev libxtst-dev catch2 stockfish
```

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Lancement

```bash
cd build
DISPLAY=:0 QT_QPA_PLATFORM=xcb ./chessbotx
```

## Configuration

Fichier : `~/.config/chessbotx/chessbotx/config.json`

| Clé | Description | Défaut |
|-----|-------------|--------|
| `enginePath` | Chemin absolu vers Stockfish | `/usr/games/stockfish` |
| `engineThreads` | Threads CPU pour Stockfish | `4` |
| `engineHashMb` | Mémoire hash de Stockfish (MB) | `256` |
| `engineSkillLevel` | Niveau 0–20 | `20` |
| `moveTimeMs` | Temps d'analyse par coup (ms) | `1000` |
| `multiPV` | Nombre de coups candidats affichés | `3` |
| `playAsWhite` | `true` = joue les blancs | `false` |
| `boardRect` | Coordonnées du plateau à l'écran | via Calibrate Board |

## Calibration

1. Ouvrir lichess dans le navigateur
2. Clic droit sur l'icône tray → **Calibrate Board**
3. Cliquer sur le coin haut-gauche du plateau, puis le coin bas-droit
4. Les coordonnées sont sauvegardées automatiquement

## Utilisation

1. Lancer le bot
2. Ouvrir une partie sur lichess
3. Clic droit sur l'icône tray → **Start Analysis**
4. Les flèches apparaissent en overlay sur le plateau
