# LibcppXls 项目架构文档

> 本文档为本工程的项目架构基准文件，记录项目定位、术语约定与目录结构。
> **编码规范全量以 [`../cpp26.md`](../cpp26.md) 为唯一规范源，本文档不重复其内容。**
> 创建日期：2026-10-24。

## 一、项目概述

- **项目名称**：LibcppXls
- **项目性质**：新项目
- **项目定位**：基于 **C++26** 编写的 **Excel** 文件读写库
- **构建工具链**：llvm-mingw **Clang 23** + **CMake**（详见第五节）
- **编码规范**：统一参照 [`../cpp26.md`](../cpp26.md)（位于本项目上一级目录，即 `Z:\source_code\cpp26.md`），本项目不设附加或豁免条款

## 二、术语与拼写勘误

项目立项描述原文中存在以下拼写/用词问题，本工程统一按修正后的写法执行：

| 原文写法 | 修正写法 | 说明 |
|----------|----------|------|
| `src/third_part` | `src/third_party` | 英文「第三方」为 **third party**，目录名沿用业界惯例 `third_party`；`third_part` 属拼写截断错误。**此为准目录名，代码与脚本中不得出现 `third_part`** |
| 三方开源库 | 第三方开源库 | 中文规范用词为「第三方」，「三方」为口语省略 |
| src 文件下 | src 目录下 | src 是目录（文件夹），应称「目录」或「文件夹」 |
| c++26 / excel | C++26 / Excel | 专有名词大小写规范（C++ 标准、微软产品名） |

另：中文语句内标点统一使用全角逗号「，」，避免半角「,」与全角混用。

## 三、目录结构规范

- 源码统一放在 `src/` 目录下
- `src/` 的下级子目录**按照模块拆分**，一个模块一个子目录
- 第三方开源库统一放在 `src/third_party/` 目录下（注意拼写：`third_party`，见第二节勘误）
- 编译生成目录为 `Build/`，测试目录为 `Test/`，脚本及工具目录为 `Script/`

### 总体目录布局

```text
LibcppXls/
├── ARCHITECTURE.md        # 本架构文档
├── CMakeLists.txt         # 构建脚本（根）
├── src/                   # 全部源码（按模块拆分子目录）
│   ├── core/              # 公共基础：错误类型、单元格/坐标模型、字符串与编码转换
│   ├── reader/            # 读取管线：解包、XML 解析、工作簿/工作表/样式重建
│   ├── writer/            # 写入管线：内容序列化、打包压缩
│   ├── style/             # 样式与数字格式
│   ├── formula/           # 公式解析与求值（远期，可选）
│   └── third_party/       # 第三方开源库（如 zip/XML 解析库，按需引入）
├── Build/                 # 编译生成目录（CMake 构建输出，不入库）
├── Test/                  # 测试目录（单元测试）
└── Script/                # 脚本及工具目录
```

> 上述 `src/` 模块拆分为初始建议，随开发推进可在本文档同步修订；「`src/` 下按模块建子目录、第三方库进 `src/third_party/`、编译输出进 `Build/`、测试进 `Test/`、脚本工具进 `Script/`」为硬性约定。

## 四、编码规范

以 [`../cpp26.md`](../cpp26.md) 为唯一规范源，涵盖命名、格式、自由函数编码、RAII / 智能指针资源管理、强制转换 `_cast` 风格、错误处理（`std::expected` 等）、依赖选型（std → Boost → 自研）、标准库回退策略、注释禁令等全部细则。

本项目无附加、无豁免。标准库回退策略产生的每项回退决策记录于第七节 ADR。

## 五、构建工具链

- **编译链（定版）**：llvm-mingw **Clang 23**（UCRT 运行时），链接器 ld.lld；构建系统 **CMake**
- 满足 `../cpp26.md` 对编译器「Clang 21+」的最低要求；本机 `gcc.exe` 为 llvm-mingw 的 clang 壳，无真 GCC 可用，GCC 路线仅留 CI 待办
- 标准库以 `import std;` 引入，依赖 std.cppm 预编译 PCM（硬化编排下 std.pcm 须同享 `-fstack-protector-strong`）
- 编译/链接旗级（含 `-O2`、函数级裁剪、消毒器与硬化编排）以 `../cpp26.md` 为准，本文档不重复

## 六、测试标准

- **0 告警 0 错误**：全工程在全部旗级编排（基线 / 消毒器 / 硬化）下编译零 warning、零 error；任何测试失败即不通过
- 三编排构建：`cmake -B Build`（基线）、`cmake -B BuildAsan -DCPPXLS_ASAN=ON`、`cmake -B BuildHard -DCPPXLS_HARDENED=ON`；各自 `ctest` 全集通过
- **三方库交叉验证**：与 Python 生态（openpyxl / pandas 等）及其他三方 Excel 读写库对同一批用例文件做读写交叉比对
- **1:1 完全通过**：读取结果（单元格值、类型、样式、公式等）与三方库逐项一致；写出的文件经三方库回读，内容 1:1 一致；任何一项不符即判不通过
- 上述为合并/发布硬性门槛

### 遗留缺口

- BIFF5 码页未生效：`u2_codepage` 已解析但 8 位字符串恒按 Latin-1 解码，cp936/cp1251 等码页文件中文乱码；std 无码页转换能力，Boost.Locale 的 `from_utf/to_utf` 需编译 boost_locale+ICU（`./liboost/` 未构建），候选路径（构建 Boost.Locale / ICU 类库落 `src/third_party/` / 常用码表自研）待评估后补 ADR
- LABEL / FORMULA 等记录体超 8224 字节时跨 CONTINUE 的拼接仅 SST 路径实现，工作表内长 LABEL 截断（现实文件罕见，xlrd 亦不拼接）
- OLE 已知差异登记：小于 4096 字节却存于主 FAT 链的流（writer 违反 [MS-CFB]），本库按规范走 ministream 路径并报 `kParse`（minifat 无链即 `broken fat chain`）；xlrd 对此类违规文件宽容回退主链。规范正确性与 xlrd 兼容性二选一，取前者，不追改

## 七、架构决策记录

| 编号 | 决策 | 理由 |
|------|------|------|
| ADR-001 | 第三方库目录定名 `src/third_party` | 拼写勘误结论（第二节），业界通行写法 |
| ADR-002 | Excel 容器解包 / XML 解析依赖的第三方库未定 | 按 `../cpp26.md` 选型顺序评估后，在 `src/third_party/` 落位 |
| ADR-003 | 自研范围暂定为 Excel 领域模型（工作簿/工作表/单元格/样式） | std / Boost 均无 Excel 格式对应能力，属规范允许的自研范畴 |
| ADR-004 | 编译链定版 llvm-mingw Clang 23 + CMake（ld.lld / UCRT） | 与 `../cpp26.md` 工具链实测结论同源，旗级编排与 std.cppm 经验可直接复用 |
| ADR-005 | 编译/链接启用函数级裁剪与 `-O2` 优化 | 链接期裁除未引用函数/数据，最小化交付体积；规范已收录对应旗级 |
| ADR-006 | 自由函数编码，不使用 `class`；资源以 RAII / 智能指针管理 | 细则以 `../cpp26.md` 为准 |
| ADR-007 | 强制类型转换一律 `_cast` 风格 | 细则以 `../cpp26.md` 为准 |
| ADR-008 | 采行标准库回退策略 | 三级回退细则以 `../cpp26.md` 为准；具体回退项按下一条目续记 |
| ADR-009 | 测试标准：0 告警 0 错误 + 三方库交叉验证 1:1 通过 | 以 Python 等成熟生态为对拍基准，读写结果逐项 1:1 一致方可合并；细节见第六节 |
| ADR-010 | 标准库引入采用 `import std;`，PCM 由 CMake 预编译注入（`-fmodule-file=std=`） | 落实 `../cpp26.md` 「只用 `import std;`」；实测要点：std.cppm 预编译旗级必须与消费方完全一致，否则 PCM 拒载并回落文本包含引发 pragma 失衡报错 |
| ADR-011 | `.xls` 读取首期落地 `src/reader`（OLE2/CFB 容器 + BIFF5/8 全局与网格解析） | 自由函数 + 聚合 struct 形态；SST 跨 CONTINUE 状态机、RK/MULRK、FORMULA 结果、合并单元格已实现；样式细节（XF 完整字段、summary）留后续批次 |
| ADR-012 | UTF-16LE / Latin-1 到 UTF-8 的转换自研，集中于 `src/core/text.hpp` | std 无对应能力（`std::wstring_convert` 于 C++26 移除、`codecvt` 弃用）；Boost 有纯头组件 `boost::locale::conv::utf_to_utf`（`encoding_utf.hpp`），但其错误路径抛 `conversion_error`（与本项目禁异常规范冲突），且非法码元的 `skip` 语义为静默丢弃而非 U+FFFD 替换（与 xlrd/WHATWG 行为不一致），故不适用；自研约 60 行零依赖、代理对配对 + U+FFFD 兜底统一实现，供 OLE 目录名与 BIFF 字符串共用 |
| ADR-013 | 小端字节读取函数集中于 `src/core/bytes.hpp` | 消除 ole/biff 两处重复；字节循环写法对宿主端序安全 |
| ADR-014 | 工作表网格稀疏化：每行 `std::vector<Cell>` 按列号升序存储 | 稠密 65536×256 网格按 `sizeof(Cell)` 约 64 字节计约 1 GB，实际文件即触发；稀疏存储内存正比于实际单元格数 |
| ADR-015 | 单元测试 `Test/unit.cpp`（合成 BIFF/CFB 用例）入 CTest；交叉对拍脚本（xlrd、合成件）接入 `add_test`；CMake 增设 `CPPXLS_ASAN` / `CPPXLS_HARDENED` 编排预设 | 落实第六节三编排零告警门槛与 ADR-009 对拍标准；回归防线覆盖越界、恶意头、编码、稀疏网格等已修缺陷 |
| ADR-016 | FORMULA 特殊结果判定维持单判据（结果高 16 位 == 0xFFFF），与对拍基准 xlrd 一致 | 完整判据应为结果字节 6/12 双处 0xFFFF；单判据会把位模式 `0xFFFF…` 的合法 NaN 判为特殊结果，但 xlrd 2.0.2 同此行为，1:1 对拍优先 |
| ADR-017 | SST 跨 CONTINUE 读取修复：`src_get_u8` 换段后复查边界，1 字节 CONTINUE 体不再越界读相邻记录头（回归用例 `sst_continue_oob`） | 越界字节落在同一堆分配内部，ASan 与对拍均不可见，实证发生确定性数据污染（SST 读出记录头字节 `<`）；根因是 `src_advance` 置 `st_pos=1` 后调用方未复查段长 |
| ADR-018 | BIFF5 STRING（FORMULA 字符串结果）按无 flags 字节布局解码（回归用例 `biff5_formula_string`）；新增 DATEMODE(0x0022) 解析落地 `b_1904`（回归用例 `datemode`） | BIFF5 STRING 为 `cch(2)+字符`、无 BIFF8 的 grbit；原实现误把首字符当 flags 导致整串丢失。`b_1904` 原为死字段，1904 纪元文件（xlrd datemode=1）误报 false |
| ADR-019 | FAT 表加载后裁剪至文件实际扇区数 | 原表按 `fat_count×128` 项分配且仅校验扇区号小于表长：越界扇区在 `sector_span` 返回空后被 `concat_sectors` 静默跳过（目录流被截不报错）；裁剪后此类扇区显式报 `kParse`，FAT 内存从文件大小×4 字节量级收敛到实际扇区数×4 |
| ADR-020 | FORMULA 待续字符串改以（行,列）坐标存储并于使用时重查；文件读取统一 `core::read_file_bytes`；OLE 文件路径单拷贝（`vec_data` 移动入 `OleFile`）；`expected` 返回显式 `std::move` | 原 `Cell*` 跨记录保存，同行 `vec_cells` 再分配（resize/insert/emplace_back）即悬垂，畸形记录序可触发释放后写；重复的读文件助手与整文件双份拷贝一并消除 |
| ADR-021 | `bytes`/`text` 实现移入 `.cpp`（头文件仅声明，落实规范 §六）；硬化编排链接补 `-pie` 与 `--dynamicbase,--nxcompat`（规范 §一·五 MinGW 等价展开）；编译器门禁补 GCC 16+ 并拒 MSVC；交叉脚本显式 `encoding="utf-8"`、xlrd 缺失按退出码 77 SKIP | 规范「头文件仅放声明」「编译器 GCC 16+/Clang 21+」收口；`-fPIE` 编译不配 PIE 链接则旗级落空；`text=True` 默认按本地代码页解码 UTF-8 输出在非 ASCII 内容下必错 |
| ADR-022 | 豁免登记：`Test/synth.hpp` 头内实现与 `ByteBuilder` 成员函数（测试脚手架）、库内函数局部短名变量（`wb`/`ws`/`rec` 等）按现行保留 | 测试脚手架改自由函数族收益低、徒增构建接线；短名均限于单函数作用域，可读性优先。命名前缀规则对跨作用域/成员变量仍全量执行 |
| ADR-023 | SST 跨 CONTINUE 语义按「阶段」区分：仅字符数据（rgb）跨段时 CONTINUE 首字节为 fHighByte 标志；cch/cRun/cbExtRst 头部字段与 rgRun/ExtRst 跳越跨段为裸续接（`StrSource::b_chars` 相位控制）；同时字符计数由「循环迭代数」更正为「UTF-16 码元数」（`rgb = cch×2`，代理对占 2 码元） | 原实现 `src_advance` 无条件消费 flags 字节，run/ext 数据跨段时多吞 1 字节致 SST 后续全错位（实证复现 `richrun`/`phonetic` 两变体，xlrd 与 LibreOffice `XclImpStream` 双基准一致）；原迭代计数使代理对字符串（emoji）多吞 1 码元。回归用例 `sst_richrun_continue`/`sst_phonetic_continue`/`sst_astral` |
| ADR-024 | MERGEDCELLS 覆盖区改区间存储（`Worksheet::vec_merges`，`MergeRange` 四元组），锚点 span 字段拓宽 `u4_rowspan`/`u4_colspan`（uint16 存不下 65536），`Cell::b_hidden` 移除改区间查询；覆盖/列范围循环一律宽类型（uint32） | 原 uint16 循环在 `r2==65535`/`c2==255`（整列/整行合并，Excel 常规操作）回绕死循环（实证 6KB 文件 CPU 满转 600s+ 不止）；全表合并物化 covered 格约 1.2GB，区间存储与 xlrd 同构；span 65536 截断为 0 系死循环掩盖的连带缺陷。MULRK/MULBLANK 同型循环原靠 `kst_off` 下溢巨值「碰巧」触发 break，一并宽类型化。回归用例 `merge_fullcol`/`merge_fullrow` |
| ADR-025 | 单 BLANK(0x0201) 记录与 MULBLANK 一致物化为 `kBlank` 格（含 XF） | 原预扫描计入 extent 但主循环不存格，与 MULBLANK 行为自相矛盾；统一后下游可取空白格样式。已知差异登记：extent 含空白格比 xlrd（无 formatting_info 时忽略 BLANK）宽，仅影响表尾空行/列计数，单元格值对拍不受影响。回归用例 `blank_cell` |
| ADR-026 | 全局子流首记录必须为 BOF（否则 `kParse`）；`parse_worksheet` 拒绝非 type-0 boundsheet（chart/macro/VB），`xls_csv_dump` 跳过之 | 原垃圾流静默产出 0 sheet 的 Workbook（xlrd 报 `Expected BOF record`）；chart 等子流被当 worksheet 解析有误读风险。xlrd 在 book 级直接忽略非 worksheet 条目（`_all_sheets_map = -1`），1:1 镜像。回归用例 `globals_nobof`/`sheet_type_gate` |
| ADR-027 | 新增 `open_ole_owned(vector&&)` 移动所有权入口（`open_workbook_file` 峰值内存 2×→1×）；`read_file_bytes`/`open_ole_file`/`open_workbook_file` 签名改 `std::filesystem::path`；流拼接按目标字节数早停预留 | 原整文件双份拷贝无必要；窄字符路径在 Windows 打不开非 ASCII 文件名，`ifstream(fs::path)` 原生走宽字符 API（std 已覆盖，无引入第三方必要） |
| ADR-028 | `cross_check_xlrd.py` 接受 `--dump=<exe>` 参数并由 CTest 以 `$<TARGET_FILE:xls_csv_dump>` 注入；`error.hpp` 移除自包含 include；`to_string(ErrorCode)` 收口 `std::unreachable()`；硬化编排链接旗移除 `-pie`（勘误 ADR-021） | 原脚本硬编码 `Build/xls_csv_dump.exe`，BuildAsan/BuildHard 编排下对拍实为测基线二进制（消毒器编排交叉验证空转）；自包含 include 为笔误；穷举 switch 后返回属规范 §四「不可达路径」；llvm-mingw（PE 目标）不支持 ELF 式 `-pie`，链接驱动报 `argument unused` 告警，PE 的等价物 `--dynamicbase,--nxcompat` 已配且保留，`-fPIE` 编译旗不变 |
| ADR-029 | 单元测试维持手写用例调度（`xls_unit <case>`），不引入 Catch2/GTest | `../cpp26.md` §七为「推荐」非强制；手写调度零依赖、CTest 直集成、与既有 26 用例形态一致；若后续用例规模显著增长再评估 |

## 八、文档维护约定

- 目录结构、模块边界、第三方依赖、工具链、测试标准发生变化时，须同步更新本文档对应章节与第七节 ADR
- 规范细则的增改一律落在 `../cpp26.md`，本文档只维护项目自身的架构事实与决策
