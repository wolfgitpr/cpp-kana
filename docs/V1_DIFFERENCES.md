# v1 → v2：行为差异说明

> 本文是**升级方唯一需要读的差异清单**。v2 按用户裁定（2026-10-31，见 `docs/plans/cpp-kana2-v2-refactor.md` §1 D3）
> **不追求与 v1 行为兼容**：v1 的输出不是契约，本文逐条说明「v1 实测行为 → v2 行为 → 为什么改」。
> 所有 v1 读数由 `tools/v1_snapshot_driver.cpp` 在 `main`（`3d1f785`）上实测得到，输入语料是
> `tests/data/corpus/cases.tsv`（入库、LF、`tests/data/** -text`）。复现命令见本文末尾。

命名与 API 的差异不在本文范围（那是 `CHANGELOG.md` 与 `docs/CONTRACT.md` 的事）。

## 1. 促音 っ

| 输入 | v1（实测） | v2 | 原因 |
|---|---|---|---|
| `きって` | `ki cl te` | `kitte` | v1 用内部占位串 `cl` 表示 っ，输出时**没有把它替换掉**，占位符原样进了结果。 |
| `がっこう` | `ga cl ko u` | `gakkou` | 同上 |
| `まっちゃ` | `ma cl cha` | `matcha` | Hepburn 规定 っち → `tchi`；v1 连占位符都没处理 |
| `ちょっと` | `cho cl to` | `chotto` | 同上 |

**`doubleWrittenSokuon` 参数在 v1 里是死分支。** 传 `true` 与 `false` 输出完全相同（实测四例全同）。
原因：循环里查表用的是 `res[i].romaji`（也就是 `cl` 本身），`cl → っ`，而 `っ` 又在排除集合
`あいうえおアイウエオっんを` 里，于是条件恒假；即便成立，紧随的 `erase` 也会把刚改过的元素删掉。
v2 **不保留这个参数**，促音按 `SokuonPolicy` 处理，默认 `Geminate`（双写下一辅音）。

## 2. 长音符 ー

| 输入 | v1（实测） | v2 `Repeat`（默认） | v2 `Drop` |
|---|---|---|---|
| `コーヒー` | `ko hi` | `koohii` | `kohi` |
| `ケーキ` | `ke ki` | `keeki` | `keki` |
| `スーパー` | `su pa` | `suupaa` | `supa` |
| `ティー` | `ti` | `tii` | `ti` |
| `パーティー` | `pa ti` | `paatii` | `pati` |

v1 在渲染前 `if (kana != u"゜" && kana != u"ー")` 直接把 ー 丢掉，因此**长音信息完全消失**。
v2 把 ー 当作独立 mora（`MoraKind::LongVowel`），由 `LongVowelPolicy` 决定写法：
`Repeat`（重复前一元音，`koohii`）、`Drop`（不标记，`kohi`）、`KatakanaDash`（`ko-hi-`）。

> 默认取 `Repeat`：这是「信息不丢」的最小承诺，且与パスポート/道路標識之外的通行罗马字一致。
> 这些默认值已定稿：权威说明见 `docs/DECISIONS.md` 的 D10 与 D18，取值以 `docs/API.md` 的选项表为准。

## 3. 组合浊点 / 半浊点

| 输入 | v1（实测） | v2 |
|---|---|---|
| `か゛く`（か + U+309B） | `ka ゛ ku` | `gaku` |
| `か` + U+3099（组合符） | 同上路径 | `gaku` |

v1 不合成浊点：U+309B 落在假名区，被 `isKana` 判真、但是 1 字符 token 且不在表里，于是走 `Error::Default`
原样保留。更糟的是 `convertKana` 把 U+309B 按 `+0x60` 偏移成 U+30FB（`・`），实测 `か゛く` → `カ・ク`。
v2 在 normalize 阶段把「基础假名 + 浊点/半浊点」合成为一个 mora（U+3099/U+309A 组合符与 U+309B/U+309C 预组形式都接受）。

## 4. 半角片假名

| 输入 | v1（实测） | v2 |
|---|---|---|
| `ｶﾀｶﾅ` | **崩溃**（0xC0000005 访问违例） | `katakana` |

崩溃原因：半角假名不属于 `isLetter/isHanzi/isDigit/isKana` 任何一类，`splitString` 返回空向量；随后
`for (int i = 0; i < res.size() - 1; …)` 中 `size()` 为 0，`0 - 1` 在 `size_t` 上回绕成 `SIZE_MAX`，
循环体访问 `res[0]` → 越界。**任何「一个 token 都切不出来」的输入都会触发**（半角假名、纯标点、空串）。
v2：normalize 阶段把 U+FF61–FF9F 折成全角（含 ﾞﾟ 组合），并且所有循环以 `size()` 的安全写法或迭代器表达，
空输入是**合法的空结果**（`Outcome` 成功、0 个 mora）。

## 5. 历史假名与未收录假名

| 输入 | v1（实测） | v2 |
|---|---|---|
| `ゐゑ` | `ゐ ゑ`（原样） | 按规范折叠：`i`/`e`，并在 `Mora.kind` 标 `Archaic` |
| `ゔ` | `vu` | `vu`（v1 正确，保留） |

## 6. 罗马字 → 假名

v1 是**逐 token 查表**，而 `splitString` 把连续 ASCII 字母当作一个 token，因此任何多音节输入都查不到：

| 输入 | v1（实测） | v2 |
|---|---|---|
| `konnichiha` | `konnichiha`（原样返回） | `こんにちは` |
| `kitte` | `kitte` | `きって` |
| `matcha` | `matcha` | `まっちゃ` |
| `gakkou` | `gakkou` | `がっこう` |
| `shin'you` | `shin you` | `しんよう` |
| `toukyou` | `toukyou` | `とうきょう` |
| `nn` | `nn` | `ん` |
| `hu`、`sya` | `hu`、`sya`（原样） | `ふ`、`しゃ`（同义输入别名） |

v1 只有单音节能命中（`ka` → `か`）。v2 用最长匹配（长度从长到短试表） + 促音双写 + 拨音规则 + 长音规则。

### 6.1 重复键导致的错误假名

v1 的 `romajiToKanaMap` 有 176 条、去重后 **172** 条：`wa`、`o`、`ji`、`zu` 各出现两次。C++ 的
initializer-list 构造对重复键**保留首次出现**，于是：

| 输入 | v1（实测） | v2 |
|---|---|---|
| `ji` | `ぢ` | `じ` |
| `zu` | `づ` | `ず` |
| `wo` | `うぉ` | `を` |
| `ti` | `てぃ` | `てぃ`（不变：`ti` 是 Hepburn 对 てぃ 的写法） |
| `tu` | `とぅ` | `とぅ`（不变，同上） |

`ji`/`zu`/`wo` 三行是**修正**：v1 因为重复键被「外来语条目」抢走，答出了错误的假名。v2 的规则是
**canonical 优先**：表里每条假名有唯一一个 canonical 罗马字（Hepburn 写法），它一律优先；别名只补充
「没有任何假名 canonical 占用」的拼法（`si`/`sya`/`zi`/`hu`/`dji`/`dzu`…）。

由此得到的输入口径：

- `ji` → じ、`zu` → ず、`wo` → を（canonical）；
- ぢ / づ 用 `dji` / `dzu` 输入（`di`/`du` 已被 でぃ/どぅ 的 canonical 占用）；
- ち / つ 用 `chi` / `tsu`（`ti`/`tu` 已被 てぃ/とぅ 占用）；
- し 可用 `shi` 或 `si`，ふ 可用 `fu` 或 `hu`，じゃ 可用 `ja` / `zya` / `jya`。

这条规则的机器保证：`tools/gen_moras.py` 在生成期**拒绝**「别名等于另一条假名的 canonical」（否则反查
将取决于表顺序），并拒绝重复 kana / 重复 canonical；`tests/cpp/data_table.cpp` 再对整张表做
「逐行渲染 == 自身 romaji」「canonical 往返回到自身」「每条别名到达正确假名」三重核对。

v2 的表由 `tools/gen_moras.py` 生成（`data/moras.tsv` + `src/data/TableData.inc`），
`tools/check_data_tables.py`（P7）复核「生成物与 TSV 一致」。

## 7. 汉字与其他非假名字符

| 输入 | v1（实测） | v2 |
|---|---|---|
| `こん好にちは` | `ko n 好 ni chi ha` | 同（`UnknownPolicy::Keep` 原样保留，且该 mora 的 `Mora.code` 标 `UnsupportedCharacter`） |

这一点 v1 的行为是对的，v2 保留，但把它从「隐式约定」变成**可查询的逐元素结果**。

## 8. 大小写与分隔

v1 只有 `toStdStr(delimiter = " ")` 一种拼法，输出小写、空格分隔。v2 把这些变成 `Options` 的字段
（`CasePolicy`、`SeparatorPolicy`）。**但默认外观变了**：v1 的默认是「小写 + 每个 mora 用空格分隔」
（`がっこう` → `ga cl ko u`），v2 的默认是紧凑输出（`gakkou`），空格分隔改由 `SeparatorPolicy::Space`
显式选择。改默认值的理由是：罗马字最常见的用途是粘进文件名或 URL，逐 mora 加空格是实现细节漏到了
输出上，不是使用者的选择。

## 9. 复现方式

```powershell
# 1. 取得 v1 源（任意一条：临时 worktree 或 main 分支的 checkout）
git worktree add .tmp/v1-src main

# 2. 构建快照驱动（MinGW，仓库根目录执行）
g++ -std=c++17 -O1 -I.tmp/v1-src/include tools/v1_snapshot_driver.cpp `
    .tmp/v1-src/src/Kana.cpp .tmp/v1-src/src/KanaRes.cpp .tmp/v1-src/src/RomajiRes.cpp `
    .tmp/v1-src/src/U16Str.cpp .tmp/v1-src/src/Utils.cpp -o .tmp/v1_snapshot_driver.exe

# 3. 跑语料（逐例起进程：其中一例会崩溃，见 §4）
.\.tmp\v1_snapshot_driver.exe tests/data/corpus/cases.tsv
```

`tools/v1_snapshot_driver.cpp` 是受跟踪的工具（不在构建里，只用于复现本文的读数）；它链接的是 v1 的 `.tmp/v1-src`，输出即本文引用的读数，逐条可复核。
