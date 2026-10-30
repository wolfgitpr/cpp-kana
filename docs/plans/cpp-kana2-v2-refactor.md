# cpp-kana v2（cpp-kana2）彻底重构方案 V2

> 状态：**已确认方向，进入实施**（用户 2026-10-31 就 D1–D4 作出裁定）。
> 快照：`cpp-kana` @ `3d1f785`（`v2` 分支已建并指向该提交）；参考仓 `cpp-pinyin` v2 @ `5d9925b`。
> V1 → V2 的变化：按用户裁定，**「多语言」= C ABI / Python / C# 三壳**（V1 曾把「可切换罗马字方案」也读进来，现从范围中移除：不建 scheme 注册表、不建插件面，只保留策略开关）；**不追求 v1 行为兼容**（V1 的 D3=A 作废），改为「按日语罗马字规范重写 + 一份 v1 差异说明」。
> 断言标注：**【事实】**=本次读码/实测所得；**【推断】**=未实测、落地时须用工具证伪。

---

## 1. 已裁定的决策（来源：2026-10-31 用户答复）

| 编号 | 裁定 | 对方案的影响 |
|---|---|---|
| D1 | **「多语言」= C ABI + Python + C#**（用户原话：「指 c abi，Python c#」） | 三壳是唯一的多语言面；核心只保一套 Hepburn 写法 + 策略开关，不建方案切换/插件面 |
| D2 | **新包 `cpp-kana2` 2.0.0** | 头 `<cpp-kana2/...>`、命名空间 `cpp_kana::v2`、C 符号 `cppk2_*`、CMake 包 `cpp-kana2`，与 v1 可同机共存 |
| D3 | **完全不追求与 v1 兼容，按日语罗马字规范重写** | 无「等价性门禁」；改为 `docs/V1_DIFFERENCES.md` 逐条说明差异与理由（升级方据此评估） |
| D4 | **允许本机自动构建 + ctest/pytest/dotnet**，Linux 复验按需上 firefly 板 | P1 起每阶段以实测门禁转绿收口 |

D1 的直接后果（写死进设计，避免越界）：

- **不实现**：罗马字方案（Hepburn/訓令式/日本式/かな入力）切换、`Registry`/`RomajiScheme` 插件面、自定义方案注册。V1 方案里的 `include/cpp-kana2/schemes/`、`ext/RomajiScheme.h`、`ext/Registry.h`、`data/schemes/<id>/` 全部删除。
- **保留**（这是实现「规范」所必需的最小旋钮，不是新层次）：音节切分后的渲染策略 —— `SokuonPolicy`（促音双写）、`HatsuonPolicy`（ん 的 `n'`）、`LongVowelPolicy`（长音写法）、`CasePolicy`（大小写）、`SeparatorPolicy`（分隔符）、`UnknownPolicy`（未识别字符的处置）。这些是 `Options` 结构体里的枚举，不是可扩展点。
- **只实现一套罗马字：Hepburn（ヘボン式）**，并按其现代通行写法（`し=shi`、`ち=chi`、`つ=tsu`、`ふ=fu`、`じ=ji`、`を=o`、`ん=n`、促音双写、`ん` 在元音/y 前写 `n'`）。将来的方案切换属**加性变更**（新增枚举值 + 新函数），不进本次范围。

---

## 2. 现状诊断（v1 缺陷清单）——重构的理由

规模：`src`+`include`+`tests` 共 12 个文件 **940 行**；两张硬编码表各 176 条（`src/Kana.cpp:15-369`）。**无 CI、无断言测试、无 C ABI、无绑定、无 pkg-config、无版本头、无文档目录、无 SONAME**。

### 2.1 正确性

| # | 缺陷 | 证据 | 断言 |
|---|---|---|---|
| C1 | `romajiToKana("ji")`→**ぢ**、`"zu"`→**づ**、`"wa"`→**うぁ**、`"o"`→お（を 不可达）。因为 `unordered_map` 初始化列表遇重复键**保留首次**，表里有 4 个重复键 | `src/Kana.cpp:238-239`、`:249-250`、`:198/:362`、`:203/:363`；实测 map2 176 条 / 去重 172 条 | **【事实】** |
| C2 | 往返不对称：`kanaToRomaji("じ")="ji"` → `romajiToKana` → ぢ | 同上 | **【事实】** |
| C3 | `doubleWrittenSokuon=true` 把 っ **整个删掉**，不是「双写」：首位插入后随即把同一元素 `erase` 掉 | `src/Kana.cpp:475-484` | **【事实】**（代码路径明确；落地用单测钉死为回归项） |
| C4 | 长音符 `ー`、半浊点 `゜` 被**静默丢弃** | `src/Kana.cpp:466` | **【事实】** |
| C5 | 组合浊点不被支持（`゛` U+309B 落在假名区、被 `isKana` 判真却不在任何表里） | `src/Utils.cpp:17,53-56`、`src/Kana.cpp:467-471` | **【事实】** |
| C6 | 半角片假名（U+FF61–FF9F）、`ゝゞヽヾ`、`ゐゑヷヸヹヺ` 均不支持；`ん` 无 `n'` 规则 | 全文无相关分支 | **【事实】** |
| C7 | 罗马字→假名是**逐 token 查表**：无最长匹配、无促音、无拨音、无长音；`splitString` 把连续 ASCII 字母当成一个 token，故 `"kitte"` 必然查不到 | `src/Utils.cpp:44-49`、`src/Kana.cpp:503-518` | **【事实】** |
| C8 | `convertKana` 用 `for (const char32_t c : u16string)` 遍历（元素实为 `char16_t`），代理对被当作两个 BMP 码位各自偏移 → 非法码位 | `src/Kana.cpp:371-393` | **【事实】** |
| C9 | 片假名区间只到 `+0x5E`，`ヷヸヹヺ`/`ヽヾ` 不转换 | `src/Kana.cpp:376,384` | **【事实】** |
| C10 | 逐元素 `std::regex` 全匹配（每次 UTF-8↔UTF-16 往返） | `src/Kana.cpp:404-408` | **【推断】**（性能，未实测） |
| C11 | 非法 UTF-8 直接 `throw std::invalid_argument`，无错误码通道；一旦跨 C ABI 就是 UB | `src/U16Str.cpp:45,49,83,89,95` | **【事实】** |

### 2.2 跨平台

| # | 缺陷 | 证据 | 断言 |
|---|---|---|---|
| X1 | 大小写敏感文件系统上测试**编译不过**：`<cpp-kana/kana.h>` vs 实际 `Kana.h` | `tests/main.cpp:3` | **【事实】** |
| X2 | MinGW 走 `__attribute__((visibility))` 分支（不定义 `_MSC_VER`），PE 上不导出任何符号 → GNU ld 在「无任何 dllexport」时自动导出全部全局符号：现存 v1 MinGW DLL 的 PE 导出表实测 **269 个**符号，其中 **226 个**是 `std::__detail`/regex 内部符号 | `include/cpp-kana/KanaGlobal.h:4-10`；`objdump -p cmake-build-release-mingw/bin/libcpp-kana.dll` 导出表计数 | **【事实】**（注意：`nm` 读的是 COFF 符号表、`.dll.a` 读的是导入库成员，两者都不等于导出表，门禁必须用 `objdump -p`/`dumpbin /exports`） |
| X3 | 无 `VERSION/SOVERSION`；`COMPATIBILITY AnyNewerVersion` 让 1.x 请求被 2.x 静默满足 | `CMakeLists.txt:56,104-108` | **【事实】** |
| X4 | `file(GLOB_RECURSE)` 决定源集合；`include_directories(include)` 全局污染 | `CMakeLists.txt:49-50` | **【事实】** |
| X5 | 静态库未设 `POSITION_INDEPENDENT_CODE` | `CMakeLists.txt:52-57` | **【推断】** |
| X6 | 无 `.pc`、无编译期版本宏；`VERSION 1.0.0.0` 四段 | `CMakeLists.txt:5` | **【事实】** |
| X7 | 测试从未注册进 CTest（无 `enable_testing()`）；`tests/main.cpp` 只 printf、恒 exit 0 | `tests/CMakeLists.txt`、`tests/main.cpp:5-21` | **【事实】** |
| X8 | 头保护宏与文件错位（`KanaRes.h` 用 `ROMAJIRES_H`、`RomajiRes.h` 用 `KANARES_H`）；README 写 `enum class` 实为裸 enum；README 的结构体出处写反 | `include/cpp-kana/KanaRes.h:1`、`RomajiRes.h:1`、`Kana.h:1,10`、`README.md:37-38,54` | **【事实】** |
| X9 | README 承诺 `convertKana` 的 `kanaType` 有默认值，头文件没有 | `README.md:98` vs `include/cpp-kana/Kana.h:30` | **【事实】** |
| X10 | 无任何 CI；Linux/macOS 从未验证 | 目录清单 | **【事实】** |

### 2.3 生态

- 无 C ABI → Python/C# 只能靠 C++ 名字改写绑定，跨编译器不可移植。**【事实】**
- vcpkg 端口（未跟踪，现已更新为 `cpp-kana2/`：name/version 2.0.0、`REF v2`、`CPP_KANA2_*` 选项、usage 修正）原先钉在旧提交且 `usage` 写的是 `cpp-pinyin`。**【事实】**

**结论**：v1 是「一张表 + 一段切分」的原型，其中 4 处会**给出错误假名**（C1/C2）、1 处**功能与文档相反**（C3）、多处静默丢字（C4/C5），并有编译不过（X1）与导出失控（X2）。逐条修补要动大半张表与全部解析逻辑，且仍缺 C ABI/门禁/文档面 —— **整体换代**。

---

## 3. 目标与验收线

- **G1** C++17、零第三方依赖（含测试：自建 ~90 行断言夹具 `tests/harness.h`，不内置 Catch2）。
- **G2** 四面可编程：C++ / C ABI / Python / C#，由**同一份 C ABI 清单**把守一致性。
- **G3** 按日语罗马字规范正确：促音、拨音、长音、组合浊点/半浊点、半角片假名、大小写、非 BMP。
- **G4** 跨平台：Windows(MSVC/MinGW) / Linux(GCC/Clang) / macOS(Clang)；大小写敏感 FS、UTF-8 路径、SONAME、导出面、确定性构建全部进清单。
- **G5** 可机检：10 门门禁 + CTest 断言 + 一份 conformance 夹具（含 sha256），`run_all_gates.py` 一条命令给单一退出码。

| 编号 | 判据 | 验证手段 |
|---|---|---|
| AC-1 | 干净克隆 `configure → build → ctest` 全绿；`BUILD_TESTS=OFF` 时 configure **明确打印**「无测试」原因 | 本机 MinGW + 板上 Linux |
| AC-2 | 公开面冻结：公开头文件数 / C ABI 符号数 两项各只登记一处，门禁逐项比对清单 | `tests/abi/expected_*.txt` |
| AC-3 | 分层机器可检：层表未覆盖的文件即失败；公共头不得 include `../src/` | `check_layers.py` |
| AC-4 | 数据可机检：mora 表覆盖库存、romaji 值唯一、每条 `kana→romaji→kana` 回到自身、数据文件 LF、无重复键 | `check_data_tables.py` |
| AC-5 | 规范行为：conformance 夹具 `cases.json` 的 sha256 与条数被钉住，三壳共读同一份 | `cases.sha256` + 三壳测试 |
| AC-6 | 三壳一致：Python/C# 的 P/Invoke·ctypes 入口集合 == C ABI 清单（多一个少一个都红） | `check_bindings.py` |
| AC-7 | 版本单一源：`project(... VERSION)` 与 `core/Version.h`、`_version.py`、C# `version.props`、`CHANGELOG` 标题一致 | `check_version_tokens.py` |
| AC-8 | 文档零断链、零未登记决策号 | `check_links.py`、`check_doc_refs.py` |
| AC-9 | 导出面：共享库只导出清单内符号（MinGW 自动全导出即红） | `ExportGate`（`nm`/`dumpbin`） |
| AC-10 | 安装面可用：`find_package(cpp-kana2)` + `.pc` + 跨大版本请求被拒 | `check_install_consumer.cmake` |

---

## 4. 包身份与版本化骨架（K1–K8）

| 编号 | 规则 | 落地物 |
|---|---|---|
| K1 | 公开名唯一写法 `cpp_kana::v2::`；不提供不限定的根名；版本命名空间不 inline | `core/Global.h` |
| K2 | 每代一套安装面：`<cpp-kana2/...>`、`cpp-kana2::cpp-kana2`、`cpp-kana2.pc`，与 v1 无同名文件 | CMake 导出 |
| K3 | 每代一套宏前缀 `CPP_KANA2_*`（预处理层没有命名空间） | `core/Global.h` |
| K4 | C 面凡能产生名字的东西都带代令牌（符号/typedef/枚举/宏前缀 `cppk2_`）；符号唯一权威=清单 | `ext/c_api.h` + `tests/abi/expected_c_api_symbols.txt` |
| K5 | `SOVERSION`=包主版本，只表达「代」而非 ABI 承诺；PE 无 soname 记 n/a | `CMakeLists.txt` |
| K6 | 托管面也带代令牌：Python 分发名 `cpp-kana2`、导入名 `cpp_kana2`；C# `AssemblyName`/`RootNamespace`/`PackageId` = `CppKana2` | 打包清单 |
| K7 | 公开别名/回调类型在版本命名空间内**定义**，不用 `using` 指向标准库类型；头文件禁止 `using namespace cpp_kana::v2;` | `core/Types.h` + 门禁 |
| K8 | 数据自报家门：`data/format.txt`（数据格式代），读取前校验 | `check_data_tables.py` |

---

## 5. 分层与公共面

```
include/cpp-kana2/
  core/  Global.h  Version.h  Types.h  Mora.h  U16Str.h  Engine.h     # 6 个头
  ext/   c_api.h                                                      # 1 个头
src/
  infra/  Utf8.cpp/h  Unicode.cpp/h  CodeText.cpp/h
  data/   Tables.cpp/h  MoraInventory.cpp/h
  mora/   Normalize.cpp/h  Segment.cpp/h  KanaToRomaji.cpp/h  RomajiToKana.cpp/h
  core/   Types.cpp  Options.cpp  Mora.cpp  U16Str.cpp  Engine.cpp
  engine/ EngineImpl.cpp/h
  c_api/  c_api_lifecycle.cpp  c_api_query.cpp  c_api_internal.h
data/   moras.tsv  format.txt
tests/  cpp/  abi/  install/  data/  c_api_smoke.c
```

层依赖（`check_layers.py` 的层表，唯一权威）：`infra → data → mora → core → engine → c_api`，只许向下；`engine` 是唯一组合层；公共头之间只允许 `core/*.h` 互引。**未覆盖文件即失败**（参考仓踩过「整层不在扫描面仍报 OK」）。

公共头 7 个：`Global.h`（导出宏、`__MINGW32__` 的 `__declspec` 分支）、`Version.h`（纯宏）、`Types.h`（`Code`/`Error`/`Outcome`/`Script`/策略枚举/`Options`/`codeText`）、`Mora.h`（`Mora`/`MoraKind`/`Conversion`）、`U16Str.h`（UTF-8↔码位，返回 `Outcome`）、`Engine.h`（`Engine` 类）、`ext/c_api.h`。

---

## 6. 错误模型

- 唯一通道 `Outcome<T>`；`Error{Code code; std::string detail;}`；`Code` **冻结 0–7 且只许追加**：
  `Ok=0, InvalidArgument=1, InvalidUtf8=2, UnsupportedCharacter=3, NotReady=4, DataCorrupt=5, NotImplemented=6, Internal=7`。
- `codeText()` 是文案唯一源；C 面 `cppk2_code_text()` 镜像同一张表（一个实现两处暴露）。
- 失败不抛异常；取错边才抛（`value()` on failure → `ErrorException`；`error()` on success → `std::bad_variant_access`）。
- **空输入=合法空结果**；非法 UTF-8 → `InvalidUtf8`（v1 是 throw，C11）；`UnknownPolicy` 决定未识别字符是「原样保留 / 丢弃 / 整体报错」。
- 逐元素结果：`Mora.code` 逐条给出原因 —— 「用标记让用户可见，而不是堆判定链追求不可靠精度」。
- C ABI：C++ 异常一律钳在边界内，`Code::Internal` 唯一生产点是 `catch (...)`。

---

## 7. 核心算法（重写；这是本仓真正的业务）

```
UTF-8 ──decode──> 码位 ──normalize──> 规范假名 ──segment──> mora 序列 ──render──> romaji + 逐 mora 结果
                                                              └─(反向) parse romaji ─> mora 序列 ─> 假名 ─> encode
```

1. **decode**：严格 UTF-8（拒绝超长、代理区、截断）→ `Outcome<std::u32string>`。**【修 C11】**
2. **normalize**：半角片假名 → 全角假名（含 ﾞﾟ）；`か+゛/か+U+3099` → `が`；片假名 → 平假名（覆盖 `ヷヸヹヺ`）；`ゝゞヽヾ` 按前一 mora 展开；`ゔ`；变体选择符剥离。**【修 C5/C6/C9】**
3. **segment**：mora 切分（基础假名 + 小假名 ゃゅょぁぃぅぇぉ；`っ`/`ー`/`ん` 单列并带 `MoraKind`）；ASCII 字母连续串、汉字、数字、其余字符各成片段。**【修 C7 的切分前提】**
4. **render**：Hepburn 基数 + 策略叠加：
   - `SokuonPolicy::Geminate`（っ 双写下一辅音，`っち`→`tchi`）/ `Drop`；
   - `HatsuonPolicy::Apostrophe`（ん 在元音与 y 前写 `n'`，`しんよう`→`shin'you`）/ `Plain`；
   - `LongVowelPolicy::Repeat`（默认，重复前一元音：`コーヒー`→`koohii`）/ `Drop`（不标记：`kohi`）/ `KatakanaDash`（`ko-hi-`）；
   - `CasePolicy`（lower/Upper/Capitalize）、`SeparatorPolicy`（`""`/`" "`）。
5. **parse**：前缀 trie 最长匹配（基数表 + 同义输入别名：`sha/sya`、`chi/ti`、`tsu/tu`、`fu/hu`、`ji/zi`、`sha` 等）；促音双写辅音 → `っ`；拨音 `nn`/`n'`/`n`+非元音 → `ん`；长音 `aa/ii/uu/ee/oo`、`ou`、`ā…`（NFC）按策略还原；未识别走 `UnknownPolicy`。**【修 C1/C7】**
6. **encode**：码位 → UTF-8。

**数据布局（单一真相源）**

```
data/moras.tsv   # kana <TAB> romaji <TAB> 额外输入别名(逗号分隔) <TAB> kind
data/format.txt  # 数据格式代（整数）
```

表覆盖：五十音、浊音/半浊音、拗音、外来语组合（ファ/ヴァ/ティ/トゥ/ツァ/ウィ/クァ/シェ/ジェ/チェ/イェ…）、`ゔ`、`ゐ/ゑ`（按规范列为历史假名，默认折叠到 `i/e` 并标注 kind）、`っ/ー/ん` 特殊项。表由 `tools/gen_moras.py` 生成一次后入库，门禁校验「生成 vs 入库」一致（生成物必须有明确产出点）。

---

## 8. 跨平台清单

| 项 | 做法 | 检查点 |
|---|---|---|
| 大小写敏感 FS | include 一律真实大小写 | `check_include_case.py`（替代 X1 教训） |
| 导出面 | GCC/Clang `-fvisibility=hidden -fvisibility-inlines-hidden`；MSVC/MinGW `__declspec`；MinGW 加 `--exclude-all-symbols` | `ExportGate`**【修 X2】** |
| 符号版本 | `VERSION/SOVERSION` | `check_soname.cmake`（PE 记 n/a）**【修 X3】** |
| 安装面 | `SameMajorVersion`、相对 DESTINATION、`cpp-kana2.pc`、`Config.cmake` 用 `set_and_check` | `check_install_consumer.cmake`（1.x 请求必须被拒） |
| 源清单 | 显式源清单，禁 GLOB | `check_source_list.py`**【修 X4】** |
| 静态可嵌入 | 静态构建 `POSITION_INDEPENDENT_CODE ON` | `pic_probe.cpp`**【修 X5】** |
| 编译期版本 | `core/Version.h` 纯宏 + 门禁对齐 `project()` | `check_version_tokens.py`**【修 X6】** |
| 编码 | MSVC `/utf-8 /Zc:__cplusplus`；源码 UTF-8 无 BOM；数据 LF | `check_text_encoding.py` |
| 路径 | 库不读环境变量；数据目录只作构造参数 | 单测含非 ASCII 路径 |
| 确定性构建 | GNU `-ffile-prefix-map` + `ar qcD`/`ranlib -D`；MinGW `--no-insert-timestamp`；MSVC `/Brepro`+`/pathmap` | 两次构建 sha256 比对 |
| 平台矩阵 | MSVC / MinGW / GCC / Clang 四腿 × Windows/Linux/macOS | `.github/workflows/ci.yml` |
| 测试注册 | 根 `enable_testing()`；`BUILD_TESTS=OFF` 时 configure 打印原因 | `check_gate_inventory.py`**【修 X7】** |

---

## 9. 三壳

- **C ABI（约 20 个符号）**：`cppk2_version`、`cppk2_code_text`、`cppk2_engine_open`、`cppk2_engine_destroy`、`cppk2_kana_to_romaji`、`cppk2_romaji_to_kana`、`cppk2_convert_script`、`cppk2_analyze`、`cppk2_options_init`（×2）、`cppk2_conversion_free`、`cppk2_mora_free`、`cppk2_string_free`、`cppk2_scheme_name`（脚本名）、`cppk2_last_error`…。
  - UTF-8 进 UTF-8 出、NUL 结尾、无长度参数；库分配必须库释放；`int32_t` 不用 `bool`；无异常过边界。
  - 输出结构体（`cppk2_mora`、`cppk2_conversion`、`cppk2_error`）**2.x 内不得加成员**；入参 `*_opts` 走 `struct_size` + `reserved_0` 先认领 padding。
- **Python**：ctypes 手写 FFI（逐入口 argtypes/restype，结构体镜像，`take_detail` 单点释放），`build_native.py` 写 `_version.py` 与 `_native/name.txt`（不硬编码库名），`pip wheel --no-build-isolation`，pytest 覆盖契约对齐/失败码/UTF-8/策略/内存/并发读。
- **C#**：P/Invoke + `DllImportResolver` 按 RID 找库，`EngineHandle : SafeHandle`，`Errors.ThrowIfFailed` 带 fallback，`Utf8String`（不用 `LPStr`），TFM `net7.0;net10.0`（本机 SDK 齐备），测试用自建 `minirunner` 避免 NuGet 网络依赖。
- 一致性门禁 `check_bindings.py`：C ABI 清单 ↔ 三壳入口集合双向比对（C# 的 `internal static extern` 集合、Python 的 `lib.cppk2_*` 集合）。

---

## 10. 门禁（10 门 + CTest 断言组）

| # | 门禁 | 守住的事实 | 需构建树 |
|---|---|---|---|
| 1 | `check_source_list.py` | 源/头清单 == 树，且无 GLOB | 否 |
| 2 | `check_layers.py` | 层依赖单向；未覆盖文件即红 | 否 |
| 3 | `check_include_case.py` | include 大小写与文件名一致 | 否 |
| 4 | `check_public_surface.py` | 公开头清单 + 公开名清单 + C ABI 符号清单三向一致 | 否 |
| 5 | `check_c_api_signatures.py` | C 签名基线可 `--emit` 重生成，漂移即红 | 否 |
| 6 | `check_version_tokens.py` | 版本五处投影一致（AC-7） | 否 |
| 7 | `check_data_tables.py` | 库存覆盖/唯一/往返/format.txt/LF（AC-4） | 否 |
| 8 | `check_bindings.py` | 三壳入口集合 == C ABI 清单（AC-6） | 否 |
| 9 | `check_links.py` + `check_doc_refs.py` | 文档链接可解析、决策号已登记 | 否 |
| 10 | `check_gate_inventory.py` | 每门门禁都注册进 ctest 与 `run_all_gates.py`（元门禁） | 是 |
| — | `run_all_gates.py` | 一条命令、单一退出码；缺构建树时**报 SKIPPED 并打印 configure 行**；self-test 先于验树 | 视情况 |

CTest 断言组：`tables` / `segment` / `render` / `parse` / `unicode` / `errors` / `c_api_smoke`(C) / `ExportGate` / `SonameGate` / `HeaderSelfCheck` / `InstallConsumer` / `Conformance`。
每条门禁自带 `--self-test`；新增守卫必须有负对照（删守卫必红）。

---

## 11. 文档集

`README.md`（新）、`CHANGELOG.md`、`CONTRIBUTING.md`、`SECURITY.md`、
`docs/{README,QUICKSTART,API_en,API_zh,CONTRACT,DECISIONS,REQUIREMENTS,STABILITY,V1_DIFFERENCES}.md`。

单一真相源：版本号→`CMakeLists.txt`；注册用例数→`STABILITY.md §0`；冻结面计数→`REQUIREMENTS.md §5.3`；性能→`PERFORMANCE.md`（首版只写测量协议与判据，不写未测数字）。

---

## 12. 阶段计划（每阶段一个提交，可独立回滚）

| 阶段 | 内容 | 完成判据 |
|---|---|---|
| **P0** | v2 分支、`.gitignore/.clang-format/.gitattributes`、v1 行为快照（在临时 worktree 构建 v1，对语料记录输出，作为 `V1_DIFFERENCES.md` 的原始数据） | 快照文件入库且可复现 |
| **P1** | 骨架：CMake（选项/告警/sanitizer/可见性/确定性/安装/pc/SONAME）+ `Global.h`/`Version.h`/`Types.h` + 空转 `Engine` + CTest 注册 | 本机 configure+build 通过 |
| **P2** | infra + data：UTF-8 严格编解码、Unicode 归一、mora 表与生成器 | `check_data_tables` 自测绿 |
| **P3** | 算法：segment/render/parse + `Mora` 结构化结果 + 错误模型 | 单测全绿；`V1_DIFFERENCES` 首版 |
| **P4** | C ABI + `c_api_smoke.c` + 导出/结构体冻结清单 | `ExportGate` 绿 |
| **P5** | Python 壳 + pytest | `pip wheel` + pytest 全绿 |
| **P6** | C# 壳 + minirunner | `dotnet build/test/pack` 绿 |
| **P7** | 10 门门禁 + ctest 集成 + `run_all_gates.py` | 全绿 + 负对照 demo |
| **P8** | 文档集 + CI 四腿 + 板上 Linux 复验 | 板上 ctest 绿 |
| **P9** | 收口：CHANGELOG、只读对抗复核轮（找反例）、Top3 回修 | 复核轮结论落盘 |

---

## 13. 风险与不做清单

**风险**：① 表数据正确性需人工抽样（外来语长音、`っち`、`ん'`、半角组合），靠「往返 + 三壳一致 + 夹具 sha256」三重机检兜底；② C# 离线构建（无 NuGet 源）→ 自建 minirunner；③ 板上复验依赖 firefly 通道可用性。

**不做**：罗马字方案切换/插件注册（D1）、dlopen 动态插件、输入纠错容错开关、pypinyin 式兼容层、把发布动作自动化、vcpkg 端口填假 SHA512、迁移 v1 的 `doubleWrittenSokuon`（C3）语义。

---

## 14. 与参考仓的对应关系（复核「仿照」是否到位）

| cpp-pinyin v2 | cpp-kana2 v2 |
|---|---|
| `cpp_pinyin::v2` / `cppp2_` / `<cpp-pinyin2/...>` | `cpp_kana::v2` / `cppk2_` / `<cpp-kana2/...>` |
| dialects + 词典数据 | **无对应物**（D1：不做方案切换） |
| `Syllable`/`Segment` 结构化结果 | `Mora`/`Conversion` |
| `Outcome<T>` + `codeText` 单一文案源 | 同 |
| 六份 ABI 清单 | 三份冻结清单（头/公开名/C 符号）+ C 签名基线 |
| 27 门禁 | 10 门 |
| 三壳（C++/C ABI/Python/C#） | 同（本次的核心诉求） |

---

## 15. 决策台账

| 编号 | 结论 | 来源 |
|---|---|---|
| D1 | 多语言 = C ABI + Python + C#；不建方案切换与插件面 | 用户 2026-10-31 |
| D2 | 新包 `cpp-kana2` 2.0.0 | 用户 2026-10-31 |
| D3 | 不追求 v1 兼容；按规范重写 + 差异说明 | 用户 2026-10-31 |
| D4 | 允许本机自动构建/测试；Linux 按需上板 | 用户 2026-10-31 |
| D5 | 门禁首批 10 门（非参考仓 27 门） | 本方案，可回退 |
| D6 | 内部一律 `char32_t` 码位，UTF-8 只在边界 | C8/X1 教训 |
| D7 | `Outcome<T>` + 冻结 `Code` 0–7，异常只用于取错边 | 参考仓 CONTRACT §2 |
| D8 | 无虚函数插件面（本次无插件，故不涉及） | 参考仓切片教训 |
| D9 | 数据生成脚本与生成物都入库，门禁校验一致 | 参考仓 D44 |
| D10 | 默认 `LongVowelPolicy::Repeat`（长音重复前一元音，`コーヒー`→`koohii`）、`SokuonPolicy::Geminate`、`HatsuonPolicy::Apostrophe` | 信息不丢且纯 ASCII；v1 把长音直接丢弃（`ko hi`） |
| D11 | 测试用自建 `tests/harness.h`（~90 行），不内置 Catch2：库只有数千行，而 Catch2 单头是 25k 行第三方代码 + 独立许可 + format 门禁豁免。若将来需要 tag/夹具再引入，并在同一提交带 THIRD_PARTY_NOTICES | 本机无网络包管理；实测 harness 能报红（见 P1 提交的失败用例） |
| D12 | `BUILD_SHARED_LIBS` 默认 ON；静态构建定义 `CPP_KANA2_STATIC` | 参考仓 CMake |
| D13 | C# TFM `net7.0;net10.0` | 本机 SDK 7.0.302/10.0.402 实测 |
| D14 | Python 3.9+；wheel 平台 tag 由 `build_native.py` 决定 | 参考仓做法 |
| D15 | 库不读环境变量；数据目录只作构造参数 | 参考仓 DECISIONS |
| D16 | `doubleWrittenSokuon` 的 v1 语义（删字）不作为任何选项保留 | C3 |

---

## 附录 A：落地后与本文的差异（P7 收尾时核对，以树为准）

本文是计划，不是现状。下面是写完之后按实测结果纠正的地方，避免后来者按旧描述去找不存在的东西。

| 本文原说法 | 实际（P7 结束时的树） | 依据 |
|---|---|---|
| §5 列了 `U16Str.h`、`Engine.h` 等公共头 | 公共头 6 个：`core/{Convert,Global,Mora,Types,Utf8,Version}.h` + `ext/c_api.h` | `tests/abi/expected_public_headers.txt` |
| §5 的 6 层含 `engine` | 5 层：`infra < data < mora < core < c_api`，没有 engine 对象（无状态自由函数） | `tools/check_layers.py` 的 `LAYERS` |
| §10 说 10 道门禁 | 10 道 Python 门禁 + 3 个 ctest 侧门禁（`ExportGate`/`SonameGate`/`InstallConsumer`） | `tools/run_all_gates.py` + `tests/CMakeLists.txt` |
| 默认输出沿用 v1 的「小写 + 空格分隔」 | 默认紧凑（`がっこう` → `gakkou`），空格需显式选 `SeparatorPolicy::Space` | `Types.h` 的 `RomajiOptions`；D18 |
| 夹具字段 `ok` 表示「调用成功」 | `status` 表示调用状态，`ok` 表示「每个 mora 都转换成功」 | `tools/conformance_emit.cpp`；D19 |
| G1 提到「不引入测试框架」 | 自写 `tests/harness.h`，20 个用例 | `tests/cpp/smoke.cpp`；D11 |

尚未在本机跑过的门禁：`Gate_SonameGate` 只在 ELF 注册（PE 无 SONAME），Windows 开发机上不会执行，
需要 firefly 板上的 Linux 复验来兑现——这一点写在 `tests/CMakeLists.txt` 的注释和 P7 提交信息里。
