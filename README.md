Kotte - Bomberman World (Final Assignment)

A custom Bomberman-style game built on the Kotte C++ game engine and raylib for the Scalable Game Worlds in C++ course at Uppsala University.

Repository and Submission Links

Repository URL: https://github.com/Zinc12313/kotte-final-assignment

Final Commit Link: https://github.com/Zinc12313/kotte-final-assignment/commit/3cb2dde875eaeae9e92227c7cfcf251aa1102849

How to Play and Controls

Movement: WASD or Arrow Keys

Place Bomb: Spacebar

Restart Game: R key

Quit Game: Q or Escape

Feature Checklist

Mandatory Features

Player Damage and Death: Fully implemented health/life system with death states.

Round States: Dynamic Playing, Won, and Lost game states with achievable objectives.

Safe Restart: Clean restart functionality via the R key.

Solid Bombs with Owner Escape: Bombs act as solid obstacles while allowing the player who placed them to safely move away.

Collectible Blast-Range Power-Up: Power-ups spawn upon destroying crates and permanently increase bomb blast radius when collected.

Chosen Extensions

Multiple Bombs per Owner: Players can place up to 3 active bombs simultaneously.

Chain Reactions: Explosions trigger adjacent bombs, causing cascading detonations.

Temporary Invulnerability (I-Frames): Players receive brief invulnerability frames upon taking damage, complete with visual feedback.

Second Enemy Behavior (enemy_chaser): Custom chaser enemy behavior logic implemented alongside standard movement.

Requirements

Visual Studio Community 2026 (compiled at Warning Level 4 /W4 without errors or warnings)

raylib 6.0