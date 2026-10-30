# Documentation

Start with the [README](../README.md). This directory holds the material that does not belong in it.

| Document | What it is for |
|---|---|
| [QUICKSTART.md](QUICKSTART.md) | Build, link and call from C++, C, Python and C#. |
| [API.md](API.md) | The reference: error model, options with their real defaults, every entry point, the C ABI, and how the three surfaces map onto each other. |
| [CONTRACT.md](CONTRACT.md) | What callers may rely on: the frozen surface, status codes, structure layouts, ownership, determinism, threads, and what is explicitly *not* promised. |
| [STABILITY.md](STABILITY.md) | The version policy: what 2.x keeps frozen, what a 3.0 may change, how to pin a build. |
| [DECISIONS.md](DECISIONS.md) | The decision ledger, each entry with its reason and the gate or test that enforces it. |
| [BOARD_REPLAY.md](BOARD_REPLAY.md) | What was measured, where, with which toolchain — including the Linux board replay and what only it could find. |
| [V1_DIFFERENCES.md](V1_DIFFERENCES.md) | Measured v1 behaviour next to what v2 does instead, with reproduction commands. Written in Chinese, like the plan. |
| [plans/cpp-kana2-v2-refactor.md](plans/cpp-kana2-v2-refactor.md) | The plan the rewrite was carried out under: defect list, goals, acceptance criteria, phases. It is a historical document; where it disagrees with the tree, the tree wins (see its appendix). |

The conformance fixture has its own README: [conformance/README.md](../conformance/README.md).
